package com.mirror.stream;

import org.junit.jupiter.api.Test;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import static org.junit.jupiter.api.Assertions.*;

public class VideoFrameWriterTest {

    @Test
    public void testWriteFrameArrayBacked() throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        VideoFrameWriter writer = new VideoFrameWriter(out);

        byte[] payload = new byte[]{0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03};
        ByteBuffer buf = ByteBuffer.wrap(payload);

        long ptsUs = 1234567890123L;
        writer.writeFrame(buf, 0, payload.length, ptsUs);

        byte[] written = out.toByteArray();
        assertEquals(12 + payload.length, written.length);

        ByteBuffer header = ByteBuffer.wrap(written, 0, 12).order(ByteOrder.BIG_ENDIAN);
        assertEquals(ptsUs, header.getLong());
        assertEquals(payload.length, header.getInt());

        byte[] writtenPayload = new byte[payload.length];
        System.arraycopy(written, 12, writtenPayload, 0, payload.length);
        assertArrayEquals(payload, writtenPayload);
    }

    @Test
    public void testWriteFrameDirectBufferWithChunking() throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        VideoFrameWriter writer = new VideoFrameWriter(out);

        // 150 KB direct buffer (larger than 64 KB chunk buffer)
        int size = 150 * 1024;
        ByteBuffer directBuf = ByteBuffer.allocateDirect(size);
        for (int i = 0; i < size; i++) {
            directBuf.put((byte) (i & 0xFF));
        }

        long ptsUs = 9876543210L;
        int origPos = 10;
        int origLimit = size - 10;
        directBuf.position(origPos);
        directBuf.limit(origLimit);

        writer.writeFrame(directBuf, 0, size, ptsUs);

        // Verify buffer position & limit restored
        assertEquals(origPos, directBuf.position());
        assertEquals(origLimit, directBuf.limit());

        byte[] written = out.toByteArray();
        assertEquals(12 + size, written.length);

        ByteBuffer header = ByteBuffer.wrap(written, 0, 12).order(ByteOrder.BIG_ENDIAN);
        assertEquals(ptsUs, header.getLong());
        assertEquals(size, header.getInt());

        // Validate payload content
        for (int i = 0; i < size; i++) {
            assertEquals((byte) (i & 0xFF), written[12 + i]);
        }
    }

    @Test
    public void testWriteFramePropagatesIOException() {
        OutputStream failingStream = new OutputStream() {
            @Override
            public void write(int b) throws IOException {
                throw new IOException("Socket broken / SIGPIPE");
            }

            @Override
            public void write(byte[] b, int off, int len) throws IOException {
                throw new IOException("Socket broken / SIGPIPE");
            }
        };

        VideoFrameWriter writer = new VideoFrameWriter(failingStream);
        ByteBuffer buf = ByteBuffer.allocate(10);

        IOException ex = assertThrows(IOException.class, () -> writer.writeFrame(buf, 0, 10, 1000L));
        assertTrue(ex.getMessage().contains("Socket broken"));
    }
}
