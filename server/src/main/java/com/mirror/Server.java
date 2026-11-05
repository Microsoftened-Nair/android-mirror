package com.mirror;

import android.net.LocalServerSocket;
import android.net.LocalSocket;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Main entry point for the Android server daemon running under app_process.
 * Manages CLI argument parsing, abstract AF_UNIX server socket binding,
 * dual-channel acceptance, and JVM shutdown hook registration.
 */
public class Server {

    public static class Options {
        private String socketName = "mirror";
        private int maxSize = 0;
        private int bitRate = 8_000_000;
        private boolean tunnelForward = false;

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

    public static void main(String[] args) {
        Options options = parseArgs(args);
        registerShutdownHook();

        try {
            ConnectionChannels channels = connect(options);
            // Channels successfully established and ready for streaming/control
        } catch (IOException e) {
            System.err.println("Server connection failed: " + e.getMessage());
            System.exit(1);
        }
    }
}
