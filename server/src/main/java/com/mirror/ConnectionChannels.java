package com.mirror;

import android.net.LocalSocket;

import java.io.Closeable;
import java.io.IOException;
import java.util.Objects;

/**
 * Thread-safe immutable holder encapsulating the paired {@link LocalSocket} descriptors
 * for video streaming and bidirectional control.
 */
public final class ConnectionChannels implements Closeable {
    private final LocalSocket videoSocket;
    private final LocalSocket controlSocket;

    public ConnectionChannels(LocalSocket videoSocket, LocalSocket controlSocket) {
        this.videoSocket = Objects.requireNonNull(videoSocket, "videoSocket cannot be null");
        this.controlSocket = Objects.requireNonNull(controlSocket, "controlSocket cannot be null");
    }

    public LocalSocket getVideoSocket() {
        return videoSocket;
    }

    public LocalSocket getControlSocket() {
        return controlSocket;
    }

    @Override
    public void close() throws IOException {
        IOException exception = null;
        try {
            videoSocket.close();
        } catch (IOException e) {
            exception = e;
        }

        try {
            controlSocket.close();
        } catch (IOException e) {
            if (exception == null) {
                exception = e;
            }
        }

        if (exception != null) {
            throw exception;
        }
    }
}
