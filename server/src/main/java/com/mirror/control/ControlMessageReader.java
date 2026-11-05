package com.mirror.control;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Objects;

/**
 * Stream parser for the control channel reading binary command packets, clamping input bounds,
 * enforcing maximum payload constraints, and invoking target handlers.
 */
public class ControlMessageReader {

    private static final int MAX_CLIPBOARD_LEN = 262144; // 256 KB

    private final InputStream in;
    private final ControlEventListener listener;

    private final byte[] buffer28 = new byte[28];
    private final ByteBuffer view28 = ByteBuffer.wrap(buffer28).order(ByteOrder.BIG_ENDIAN);

    public ControlMessageReader(InputStream in, ControlEventListener listener) {
        this.in = Objects.requireNonNull(in, "InputStream cannot be null");
        this.listener = Objects.requireNonNull(listener, "ControlEventListener cannot be null");
    }

    /**
     * Reads a single control message from the input stream and dispatches it to the listener.
     *
     * @return true if a message was parsed, false if clean end-of-stream was reached.
     * @throws IOException on unrecognized type, payload limit violation, or read error.
     */
    public boolean readNextMessage() throws IOException {
        int type = in.read();
        if (type < 0) {
            return false;
        }
        buffer28[0] = (byte) type;

        switch (type) {
            case 0x00: { // InjectTouch (28 bytes total)
                readExact(buffer28, 1, 27);
                int action = buffer28[1] & 0xFF;
                long pointerId = view28.getLong(4);
                int posX = view28.getInt(12);
                int posY = view28.getInt(16);
                int canvasW = view28.getShort(20) & 0xFFFF;
                int canvasH = view28.getShort(22) & 0xFFFF;
                int pressure = view28.getShort(24) & 0xFFFF;
                int buttons = view28.getShort(26) & 0xFFFF;

                // Clamping to display canvas boundaries
                posX = Math.min(posX, canvasW);
                posY = Math.min(posY, canvasH);

                listener.onTouchEvent(action, pointerId, posX, posY, canvasW, canvasH, pressure, buttons);
                return true;
            }

            case 0x01: { // InjectKey (12 bytes total)
                readExact(buffer28, 1, 11);
                int action = buffer28[1] & 0xFF;
                int keyCode = view28.getInt(4);
                int metaState = view28.getInt(8);

                listener.onKeyEvent(action, keyCode, metaState);
                return true;
            }

            case 0x02: { // InjectScroll (20 bytes total)
                readExact(buffer28, 1, 19);
                int posX = view28.getInt(4);
                int posY = view28.getInt(8);
                int hScroll = view28.getInt(12);
                int vScroll = view28.getInt(16);

                listener.onScrollEvent(posX, posY, hScroll, vScroll);
                return true;
            }

            case 0x03: { // SetClipboard (8-byte header + UTF-8 payload)
                readExact(buffer28, 1, 7);
                boolean pasteNow = (buffer28[1] != 0);
                int length = view28.getInt(4);

                if (length < 0 || length > MAX_CLIPBOARD_LEN) {
                    throw new IOException("Clipboard length exceeds maximum permitted size: " + length);
                }

                String text;
                if (length == 0) {
                    text = "";
                } else {
                    byte[] clipBytes = new byte[length];
                    readExact(clipBytes, 0, length);
                    text = new String(clipBytes, StandardCharsets.UTF_8);
                }

                listener.onSetClipboard(text, pasteNow);
                return true;
            }

            case 0x04: { // SetScreenPower (2 bytes total)
                readExact(buffer28, 1, 1);
                int powerMode = buffer28[1] & 0xFF;
                boolean on = (powerMode == 0x02); // 0x02 = NORMAL, 0x00 = OFF

                listener.onSetScreenPower(on);
                return true;
            }

            case 0x05: { // RequestKeyframe (1 byte total)
                listener.onRequestKeyframe();
                return true;
            }

            case 0x06: { // HeartbeatPing (5 bytes total)
                readExact(buffer28, 1, 4);
                int pingId = view28.getInt(1);

                listener.onHeartbeatPing(pingId);
                return true;
            }

            default:
                throw new IOException("Unrecognized control message type: " + type + ". Aborting connection.");
        }
    }

    /**
     * Loops continuously reading and dispatching control messages until stream termination or error.
     */
    public void readLoop() throws IOException {
        while (readNextMessage()) {
            // Steady-state continuous event read loop
        }
    }

    private void readExact(byte[] buf, int offset, int length) throws IOException {
        int total = 0;
        while (total < length) {
            int read = in.read(buf, offset + total, length - total);
            if (read < 0) {
                throw new EOFException("Premature end of stream while reading control packet (read "
                        + total + "/" + length + " bytes)");
            }
            total += read;
        }
    }
}
