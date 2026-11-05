package com.mirror.video;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Bundle;
import android.view.Surface;

import com.mirror.stream.VideoFrameWriter;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Low-latency H.264 surface-to-codec hardware compression worker.
 * Manages MediaCodec configuration, output buffer draining loop, and asynchronous sync-frame requests.
 */
public class ScreenEncoder {

    private MediaCodec codec;
    private Surface inputSurface;
    private final AtomicBoolean isRunning = new AtomicBoolean(false);

    private final VideoFrameWriter frameWriter;
    private final int width;
    private final int height;
    private final int bitRate;
    private final int maxFps;

    public ScreenEncoder(VideoFrameWriter frameWriter, int width, int height, int bitRate, int maxFps) {
        this.frameWriter = Objects.requireNonNull(frameWriter, "frameWriter cannot be null");
        this.width = width;
        this.height = height;
        this.bitRate = bitRate;
        this.maxFps = maxFps;
    }

    public Surface initialize() throws IOException {
        codec = MediaCodec.createEncoderByType("video/avc");

        MediaFormat format = MediaFormat.createVideoFormat("video/avc", width, height);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        format.setInteger(MediaFormat.KEY_FRAME_RATE, maxFps);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 10);
        format.setInteger(MediaFormat.KEY_PROFILE, MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline);
        format.setInteger(MediaFormat.KEY_LEVEL, MediaCodecInfo.CodecProfileLevel.AVCLevel31);

        // Ultra-low latency and repeating frame settings
        format.setInteger("latency", 0);
        format.setInteger("priority", 0);
        format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, 100_000L);

        codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        inputSurface = codec.createInputSurface();
        codec.start();

        return inputSurface;
    }

    public void drainLoop() throws IOException {
        isRunning.set(true);
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        while (isRunning.get()) {
            int index = codec.dequeueOutputBuffer(bufferInfo, 10_000);
            if (index >= 0) {
                ByteBuffer out = codec.getOutputBuffer(index);
                if (out != null) {
                    boolean isConfig = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
                    if (isConfig || bufferInfo.size > 0) {
                        frameWriter.writeFrame(out, bufferInfo.offset, bufferInfo.size, bufferInfo.presentationTimeUs);
                    }
                }
                codec.releaseOutputBuffer(index, false);
            } else if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                // Parameter set (SPS/PPS) or output format updated
            }
        }
    }

    public void requestKeyframe() {
        if (codec != null) {
            try {
                Bundle bundle = new Bundle();
                bundle.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
                codec.setParameters(bundle);
            } catch (Exception ignored) {
            }
        }
    }

    public synchronized void stop() {
        isRunning.set(false);
        if (codec != null) {
            try {
                codec.stop();
            } catch (Exception ignored) {
            }
            try {
                codec.release();
            } catch (Exception ignored) {
            }
            codec = null;
        }
        if (inputSurface != null) {
            try {
                inputSurface.release();
            } catch (Exception ignored) {
            }
            inputSurface = null;
        }
    }

    public boolean isRunning() {
        return isRunning.get();
    }

    public Surface getInputSurface() {
        return inputSurface;
    }
}
