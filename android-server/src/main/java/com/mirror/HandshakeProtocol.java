package com.mirror;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Handshake protocol serialization and deserialization for Android server daemon.
 * Enforces strict Big-Endian network byte order and zero dynamic allocations.
 */
public final class HandshakeProtocol {

    public static final byte[] MAGIC = {0x4D, 0x49, 0x52, 0x52}; // "MIRR"
    public static final int DEVICE_NAME_FIELD_LENGTH = 64;
    public static final int DEVICE_ID_PACKET_SIZE = 68;
    public static final int VIDEO_GEOMETRY_PACKET_SIZE = 12;
    public static final int HANDSHAKE_PACKET_SIZE = DEVICE_ID_PACKET_SIZE + VIDEO_GEOMETRY_PACKET_SIZE; // 80 bytes
    public static final int CLIENT_ACK_PACKET_SIZE = 8;

    public static final int CODEC_H264 = 1;
    public static final int EXPECTED_CLIENT_VERSION = 1;
    public static final int FLAG_SCREEN_OFF = 1 << 0;
    public static final int FLAG_STAY_AWAKE = 1 << 1;

    private static final byte[] HANDSHAKE_RAW_BUFFER = new byte[HANDSHAKE_PACKET_SIZE];
    private static final ByteBuffer HANDSHAKE_BYTE_BUFFER = ByteBuffer.wrap(HANDSHAKE_RAW_BUFFER).order(ByteOrder.BIG_ENDIAN);
    private static final Object HANDSHAKE_LOCK = new Object();

    private static final byte[] ACK_RAW_BUFFER = new byte[CLIENT_ACK_PACKET_SIZE];
    private static final ByteBuffer ACK_BYTE_BUFFER = ByteBuffer.wrap(ACK_RAW_BUFFER).order(ByteOrder.BIG_ENDIAN);
    private static final Object ACK_LOCK = new Object();

    private HandshakeProtocol() {
        // Non-instantiable utility class
    }

    /**
     * Sends the 80-byte handshake packet (68 bytes DeviceId + 12 bytes VideoGeometry)
     * over the video socket OutputStream.
     */
    public static void sendHandshake(OutputStream videoOut, String deviceName, int width, int height) throws IOException {
        synchronized (HANDSHAKE_LOCK) {
            HANDSHAKE_BYTE_BUFFER.clear();

            // 1. Magic (4 bytes): "MIRR"
            HANDSHAKE_BYTE_BUFFER.put(MAGIC);

            // 2. Device Name (64 bytes): UTF-8 string padded with 0x00
            int maxPos = HANDSHAKE_BYTE_BUFFER.position() + DEVICE_NAME_FIELD_LENGTH;
            if (deviceName != null) {
                for (int i = 0; i < deviceName.length(); i++) {
                    char c = deviceName.charAt(i);
                    if (c < 0x80) {
                        if (HANDSHAKE_BYTE_BUFFER.position() < maxPos) {
                            HANDSHAKE_BYTE_BUFFER.put((byte) c);
                        } else {
                            break;
                        }
                    } else if (c < 0x800) {
                        if (HANDSHAKE_BYTE_BUFFER.position() + 2 <= maxPos) {
                            HANDSHAKE_BYTE_BUFFER.put((byte) (0xc0 | (c >> 6)));
                            HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | (c & 0x3f)));
                        } else {
                            break;
                        }
                    } else if (Character.isSurrogate(c)) {
                        if (Character.isHighSurrogate(c) && i + 1 < deviceName.length() && Character.isLowSurrogate(deviceName.charAt(i + 1))) {
                            int codePoint = Character.toCodePoint(c, deviceName.charAt(i + 1));
                            i++;
                            if (HANDSHAKE_BYTE_BUFFER.position() + 4 <= maxPos) {
                                HANDSHAKE_BYTE_BUFFER.put((byte) (0xf0 | (codePoint >> 18)));
                                HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | ((codePoint >> 12) & 0x3f)));
                                HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | ((codePoint >> 6) & 0x3f)));
                                HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | (codePoint & 0x3f)));
                            } else {
                                break;
                            }
                        }
                    } else {
                        if (HANDSHAKE_BYTE_BUFFER.position() + 3 <= maxPos) {
                            HANDSHAKE_BYTE_BUFFER.put((byte) (0xe0 | (c >> 12)));
                            HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | ((c >> 6) & 0x3f)));
                            HANDSHAKE_BYTE_BUFFER.put((byte) (0x80 | (c & 0x3f)));
                        } else {
                            break;
                        }
                    }
                }
            }

            while (HANDSHAKE_BYTE_BUFFER.position() < maxPos) {
                HANDSHAKE_BYTE_BUFFER.put((byte) 0);
            }

            // 3. Codec ID (4 bytes, uint32): 0x00000001 (H.264)
            HANDSHAKE_BYTE_BUFFER.putInt(CODEC_H264);

            // 4. Width (4 bytes, uint32): aligned canvas width
            HANDSHAKE_BYTE_BUFFER.putInt(width);

            // 5. Height (4 bytes, uint32): aligned canvas height
            HANDSHAKE_BYTE_BUFFER.putInt(height);

            videoOut.write(HANDSHAKE_RAW_BUFFER, 0, HANDSHAKE_PACKET_SIZE);
            videoOut.flush();
        }
    }

    /**
     * Reads the 8-byte ClientAck packet from the control socket InputStream.
     * Validates that version == 1, and returns the flags bitmask.
     */
    public static int readClientAck(InputStream controlIn) throws IOException {
        synchronized (ACK_LOCK) {
            int totalRead = 0;
            while (totalRead < CLIENT_ACK_PACKET_SIZE) {
                int read = controlIn.read(ACK_RAW_BUFFER, totalRead, CLIENT_ACK_PACKET_SIZE - totalRead);
                if (read < 0) {
                    throw new EOFException("End of stream reached before reading full client ACK packet ("
                            + totalRead + "/" + CLIENT_ACK_PACKET_SIZE + " bytes read)");
                }
                totalRead += read;
            }

            ACK_BYTE_BUFFER.clear();
            int version = ACK_BYTE_BUFFER.getInt();
            if (version != EXPECTED_CLIENT_VERSION) {
                throw new IOException("Unsupported client handshake version: " + version
                        + " (expected: " + EXPECTED_CLIENT_VERSION + ")");
            }

            return ACK_BYTE_BUFFER.getInt();
        }
    }
}
