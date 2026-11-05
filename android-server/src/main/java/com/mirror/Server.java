package com.mirror;

import android.graphics.Point;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Build;
import android.view.Surface;

import com.mirror.compat.DisplayManagerBridge;
import com.mirror.compat.InputManagerBridge;
import com.mirror.control.ControlMessageReader;
import com.mirror.input.InputDispatcher;
import com.mirror.power.DevicePowerController;
import com.mirror.stream.VideoFrameWriter;
import com.mirror.video.DisplayCalculator;
import com.mirror.video.ScreenEncoder;
import com.mirror.video.VirtualDisplayController;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Main entry point for the Android server daemon running under app_process.
 * Manages CLI argument parsing, abstract AF_UNIX server socket binding,
 * dual-channel acceptance, streaming/control lifecycle, and JVM shutdown hook registration.
 */
public class Server {

    public static class Options {
        private String socketName = "mirror";
        private int maxSize = 0;
        private int bitRate = 8_000_000;
        private boolean tunnelForward = false;
        private boolean stayAwake = false;

        public String getSocketName() {
            return socketName;
        }

        public void setSocketName(String socketName) {
            this.socketName = socketName;
        }

        public int getMaxSize() {
            return maxSize;
        }

        public void setMaxSize(int maxSize) {
            this.maxSize = maxSize;
        }

        public int getBitRate() {
            return bitRate;
        }

        public void setBitRate(int bitRate) {
            this.bitRate = bitRate;
        }

        public boolean isTunnelForward() {
            return tunnelForward;
        }

        public void setTunnelForward(boolean tunnelForward) {
            this.tunnelForward = tunnelForward;
        }

        public boolean isStayAwake() {
            return stayAwake;
        }

        public void setStayAwake(boolean stayAwake) {
            this.stayAwake = stayAwake;
        }
    }

    private static final AtomicReference<LocalServerSocket> serverSocketRef = new AtomicReference<>();
    private static final AtomicReference<LocalSocket> videoSocketRef = new AtomicReference<>();
    private static final AtomicReference<LocalSocket> controlSocketRef = new AtomicReference<>();

    public static Options parseArgs(String[] args) {
        Options options = new Options();
        if (args == null) {
            return options;
        }

        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (arg.startsWith("--socket-name=")) {
                options.setSocketName(arg.substring("--socket-name=".length()));
            } else if (arg.equals("--socket-name") && i + 1 < args.length) {
                options.setSocketName(args[++i]);
            } else if (arg.startsWith("--max-size=")) {
                options.setMaxSize(Integer.parseInt(arg.substring("--max-size=".length())));
            } else if (arg.equals("--max-size") && i + 1 < args.length) {
                options.setMaxSize(Integer.parseInt(args[++i]));
            } else if (arg.startsWith("--bit-rate=")) {
                options.setBitRate(Integer.parseInt(arg.substring("--bit-rate=".length())));
            } else if (arg.equals("--bit-rate") && i + 1 < args.length) {
                options.setBitRate(Integer.parseInt(args[++i]));
            } else if (arg.startsWith("--tunnel-forward=")) {
                options.setTunnelForward(Boolean.parseBoolean(arg.substring("--tunnel-forward=".length())));
            } else if (arg.equals("--tunnel-forward") && i + 1 < args.length) {
                options.setTunnelForward(Boolean.parseBoolean(args[++i]));
            } else if (arg.startsWith("--stay-awake=")) {
                options.setStayAwake(Boolean.parseBoolean(arg.substring("--stay-awake=".length())));
            } else if (arg.equals("--stay-awake") && i + 1 < args.length) {
                options.setStayAwake(Boolean.parseBoolean(args[++i]));
            }
        }
        return options;
    }

    public static ConnectionChannels connect(Options options) throws IOException {
        LocalServerSocket serverSocket = new LocalServerSocket(options.getSocketName());
        serverSocketRef.set(serverSocket);

        LocalSocket videoSocket = serverSocket.accept();
        videoSocketRef.set(videoSocket);

        LocalSocket controlSocket = serverSocket.accept();
        controlSocketRef.set(controlSocket);

        return new ConnectionChannels(videoSocket, controlSocket);
    }

    public static void registerShutdownHook() {
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            closeQuietly(videoSocketRef.getAndSet(null));
            closeQuietly(controlSocketRef.getAndSet(null));
            closeQuietly(serverSocketRef.getAndSet(null));
        }, "mirror-shutdown"));
    }

    public static void closeQuietly(AutoCloseable closeable) {
        if (closeable != null) {
            try {
                closeable.close();
            } catch (Exception ignored) {
                // Safely catch and suppress IOExceptions and other close exceptions
            }
        }
    }

    public static void start(Options options) throws Exception {
        ConnectionChannels channels = connect(options);

        Point realSize = DisplayManagerBridge.getDisplayRealSize(0);
        DisplayCalculator.Size targetSize = DisplayCalculator.computeDisplaySize(realSize.x, realSize.y, options.getMaxSize());

        String deviceName = Build.MODEL;
        if (deviceName == null || deviceName.isEmpty()) {
            deviceName = "Android Device";
        }

        // 1. Send 80-byte handshake on video socket
        HandshakeProtocol.sendHandshake(
                channels.getVideoSocket().getOutputStream(),
                deviceName,
                targetSize.width,
                targetSize.height
        );

        // 2. Read 8-byte ClientAck on control socket
        int clientFlags = HandshakeProtocol.readClientAck(channels.getControlSocket().getInputStream());

        // 3. Setup device power and wake lock
        DevicePowerController powerController = new DevicePowerController();
        if ((clientFlags & HandshakeProtocol.FLAG_STAY_AWAKE) != 0 || options.isStayAwake()) {
            powerController.acquireWakeLock();
        }
        if ((clientFlags & HandshakeProtocol.FLAG_SCREEN_OFF) != 0) {
            powerController.setScreenPower(false);
        }

        // 4. Configure Video Pipeline
        VideoFrameWriter frameWriter = new VideoFrameWriter(channels.getVideoSocket().getOutputStream());
        ScreenEncoder screenEncoder = new ScreenEncoder(frameWriter, targetSize.width, targetSize.height, options.getBitRate(), 60);
        Surface inputSurface = screenEncoder.initialize();

        VirtualDisplayController virtualDisplay = new VirtualDisplayController(targetSize.width, targetSize.height);
        virtualDisplay.setup(inputSurface);

        // 5. Configure Input Dispatcher and Control Channel Reader
        InputManagerBridge inputBridge = new InputManagerBridge();
        InputDispatcher inputDispatcher = new InputDispatcher(inputBridge, powerController, screenEncoder);
        ControlMessageReader controlReader = new ControlMessageReader(channels.getControlSocket().getInputStream(), inputDispatcher);

        // 6. Spawn video encoder drain loop on dedicated background thread
        Thread videoThread = new Thread(() -> {
            try {
                screenEncoder.drainLoop();
            } catch (Exception e) {
                System.err.println("ScreenEncoder drainLoop terminated: " + e.getMessage());
            }
        }, "mirror-video-encoder");
        videoThread.start();

        // 7. Drive control message read loop synchronously until EOF or client disconnect
        try {
            controlReader.readLoop();
        } finally {
            screenEncoder.stop();
            virtualDisplay.release();
            powerController.restoreDefaults();
            closeQuietly(channels);
            try {
                videoThread.join(2000);
            } catch (InterruptedException ignored) {
            }
        }
    }

    public static void main(String[] args) {
        Options options = parseArgs(args);
        registerShutdownHook();

        try {
            start(options);
        } catch (Exception e) {
            System.err.println("Server connection failed: " + e.getMessage());
            System.exit(1);
        }
    }
}
