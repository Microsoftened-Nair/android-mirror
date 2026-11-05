package com.mirror;

import org.junit.jupiter.api.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

import static org.junit.jupiter.api.Assertions.*;

public class HandshakeProtocolTest {

    @Test
    public void testSendHandshake() throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        String deviceName = "Pixel 7 Pro";
        int width = 1440;
        int height = 3120;

        HandshakeProtocol.sendHandshake(out, deviceName, width, height);
        byte[] bytes = out.toByteArray();

        assertEquals(80, bytes.length);

        // Magic
        assertEquals((byte) 0x4D, bytes[0]);
        assertEquals((byte) 0x49, bytes[1]);
        assertEquals((byte) 0x52, bytes[2]);
        assertEquals((byte) 0x52, bytes[3]);

        // Device name
        String extractedName = new String(bytes, 4, deviceName.length(), StandardCharsets.UTF_8);
        assertEquals(deviceName, extractedName);
        for (int i = 4 + deviceName.length(); i < 68; i++) {
            assertEquals((byte) 0, bytes[i], "Padding byte should be 0");
        }

        // Geometry block
        ByteBuffer bb = ByteBuffer.wrap(bytes, 68, 12).order(ByteOrder.BIG_ENDIAN);
        assertEquals(HandshakeProtocol.CODEC_H264, bb.getInt());
        assertEquals(width, bb.getInt());
        assertEquals(height, bb.getInt());
    }

    @Test
    public void testReadClientAckSuccess() throws IOException {
        ByteBuffer bb = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
        bb.putInt(1); // Version 1
        bb.putInt(HandshakeProtocol.FLAG_SCREEN_OFF | HandshakeProtocol.FLAG_STAY_AWAKE); // Flags: 3

        ByteArrayInputStream in = new ByteArrayInputStream(bb.array());
        int flags = HandshakeProtocol.readClientAck(in);

        assertEquals(3, flags);
    }

    @Test
    public void testReadClientAckInvalidVersion() {
        ByteBuffer bb = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
        bb.putInt(2); // Unsupported version 2
        bb.putInt(0);

        ByteArrayInputStream in = new ByteArrayInputStream(bb.array());
        IOException thrown = assertThrows(IOException.class, () -> HandshakeProtocol.readClientAck(in));
        assertTrue(thrown.getMessage().contains("Unsupported client handshake version"));
    }

    @Test
    public void testReadClientAckPrematureEOF() {
        byte[] shortPacket = new byte[]{0, 0, 0, 1}; // only 4 bytes
        ByteArrayInputStream in = new ByteArrayInputStream(shortPacket);
        assertThrows(EOFException.class, () -> HandshakeProtocol.readClientAck(in));
    }
}
