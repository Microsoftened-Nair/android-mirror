package com.mirror.stream;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Objects;

/**
 * Device-side video frame transmitter writing 12-byte headers (microsecond PTS and
 * payload length in Big-Endian order) and raw Annex B MediaCodec buffers to the client socket.
 * Guarantees zero heap allocation inside the writeFrame loop.
 */
public class VideoFrameWriter {

    private static final int HEADER_SIZE = 12;
    private static final int CHUNK_BUFFER_SIZE = 65536; // 64 KB

    private final OutputStream out;
    private final byte[] headerBytes = new byte[HEADER_SIZE];
    private final ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes).order(ByteOrder.BIG_ENDIAN);
    private final byte[] transferBuffer = new byte[CHUNK_BUFFER_SIZE];

    public VideoFrameWriter(OutputStream outputStream) {
        this.out = Objects.requireNonNull(outputStream, "outputStream cannot be null");
    }

    /**
     * Serializes the 12-byte header and writes the frame payload directly into the output stream.
     *
     * @param frameBuffer the buffer containing encoded video frame data (array-backed or direct)
     * @param offset      starting position of valid frame data in frameBuffer
     * @param length      number of bytes to write
     * @param ptsUs       presentation timestamp in microseconds
     * @throws IOException if a network error occurs or the stream is closed
     */
    public synchronized void writeFrame(ByteBuffer frameBuffer, int offset, int length, long ptsUs) throws IOException {
        Objects.requireNonNull(frameBuffer, "frameBuffer cannot be null");
        if (offset < 0 || length < 0 || offset + length > frameBuffer.capacity()) {
            throw new IndexOutOfBoundsException("Invalid offset=" + offset + " or length=" + length
                    + " for buffer capacity=" + frameBuffer.capacity());
        }

        // 1. Populate 12-byte header (8-byte PTS + 4-byte payload length)
        headerBuffer.clear();
        headerBuffer.putLong(ptsUs);
        headerBuffer.putInt(length);

        // 2. Transmit header in Big-Endian network byte order
        out.write(headerBytes, 0, HEADER_SIZE);

        if (length == 0) {
            out.flush();
            return;
        }

        // 3. Transmit frame payload with zero heap allocations
        if (frameBuffer.hasArray()) {
            out.write(frameBuffer.array(), frameBuffer.arrayOffset() + offset, length);
        } else {
            int savedPosition = frameBuffer.position();
            int savedLimit = frameBuffer.limit();
            try {
                frameBuffer.position(offset);
                frameBuffer.limit(offset + length);
                while (frameBuffer.hasRemaining()) {
                    int chunk = Math.min(frameBuffer.remaining(), transferBuffer.length);
                    frameBuffer.get(transferBuffer, 0, chunk);
                    out.write(transferBuffer, 0, chunk);
                }
            } finally {
                frameBuffer.position(savedPosition);
                frameBuffer.limit(savedLimit);
            }
        }

        // 4. Flush to prevent framing latency
        out.flush();
    }
}
