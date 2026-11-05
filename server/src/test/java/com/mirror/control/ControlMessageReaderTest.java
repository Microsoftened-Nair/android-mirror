package com.mirror.control;

import org.junit.jupiter.api.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.jupiter.api.Assertions.*;

public class ControlMessageReaderTest {

    static class MockListener implements ControlEventListener {
        final AtomicInteger touchAction = new AtomicInteger();
        final AtomicLong touchPointerId = new AtomicLong();
        final AtomicInteger touchX = new AtomicInteger();
        final AtomicInteger touchY = new AtomicInteger();
        final AtomicInteger touchCanvasW = new AtomicInteger();
        final AtomicInteger touchCanvasH = new AtomicInteger();
        final AtomicInteger touchPressure = new AtomicInteger();
        final AtomicInteger touchButtons = new AtomicInteger();

        final AtomicInteger keyAction = new AtomicInteger();
        final AtomicInteger keyCode = new AtomicInteger();
        final AtomicInteger keyMetaState = new AtomicInteger();

        final AtomicInteger scrollX = new AtomicInteger();
        final AtomicInteger scrollY = new AtomicInteger();
        final AtomicInteger scrollH = new AtomicInteger();
        final AtomicInteger scrollV = new AtomicInteger();

        final AtomicReference<String> clipboardText = new AtomicReference<>();
        final AtomicBoolean clipboardPasteNow = new AtomicBoolean();

        final AtomicBoolean screenPowerOn = new AtomicBoolean();
        final AtomicBoolean keyframeRequested = new AtomicBoolean();
        final AtomicInteger heartbeatPingId = new AtomicInteger();

        @Override
        public void onTouchEvent(int action, long pointerId, int x, int y, int canvasWidth, int canvasHeight, int pressure, int buttons) {
            touchAction.set(action);
            touchPointerId.set(pointerId);
            touchX.set(x);
            touchY.set(y);
            touchCanvasW.set(canvasWidth);
            touchCanvasH.set(canvasHeight);
            touchPressure.set(pressure);
            touchButtons.set(buttons);
        }

        @Override
        public void onKeyEvent(int action, int keyCode, int metaState) {
            keyAction.set(action);
            this.keyCode.set(keyCode);
            keyMetaState.set(metaState);
        }

        @Override
        public void onScrollEvent(int x, int y, int hScroll, int vScroll) {
            scrollX.set(x);
            scrollY.set(y);
            scrollH.set(hScroll);
            scrollV.set(vScroll);
        }

        @Override
        public void onSetClipboard(String text, boolean pasteNow) {
            clipboardText.set(text);
            clipboardPasteNow.set(pasteNow);
        }

        @Override
        public void onSetScreenPower(boolean on) {
            screenPowerOn.set(on);
        }

        @Override
        public void onRequestKeyframe() {
            keyframeRequested.set(true);
        }

        @Override
        public void onHeartbeatPing(int pingId) {
            heartbeatPingId.set(pingId);
        }
    }

    @Test
    public void testTouchDeserializationAndClamping() throws IOException {
        MockListener listener = new MockListener();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        ByteBuffer bb = ByteBuffer.allocate(28).order(ByteOrder.BIG_ENDIAN);

        bb.put((byte) 0x00); // Type Touch
        bb.put((byte) 0x02); // Action MOVE
        bb.putShort((short) 0); // Reserved
        bb.putLong(123456789L); // Pointer ID
        bb.putInt(1500); // pos_x exceeds canvas_width (1080)
        bb.putInt(3000); // pos_y exceeds canvas_height (2400)
        bb.putShort((short) 1080); // canvas_width
        bb.putShort((short) 2400); // canvas_height
        bb.putShort((short) 0x7FFF); // pressure
        bb.putShort((short) 0x0001); // buttons

        baos.write(bb.array());

        ControlMessageReader reader = new ControlMessageReader(new ByteArrayInputStream(baos.toByteArray()), listener);
        boolean read = reader.readNextMessage();
        assertTrue(read);

        assertEquals(2, listener.touchAction.get());
        assertEquals(123456789L, listener.touchPointerId.get());
        // Verify clamping!
        assertEquals(1080, listener.touchX.get(), "X coordinate must be clamped to canvas_width");
        assertEquals(2400, listener.touchY.get(), "Y coordinate must be clamped to canvas_height");
        assertEquals(1080, listener.touchCanvasW.get());
        assertEquals(2400, listener.touchCanvasH.get());
        assertEquals(0x7FFF, listener.touchPressure.get());
        assertEquals(1, listener.touchButtons.get());
    }

    @Test
    public void testKeyAndScrollDeserialization() throws IOException {
        MockListener listener = new MockListener();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();

        // 1. Key packet (12 bytes)
        ByteBuffer keyBb = ByteBuffer.allocate(12).order(ByteOrder.BIG_ENDIAN);
        keyBb.put((byte) 0x01); // InjectKey
        keyBb.put((byte) 0x00); // Action DOWN
        keyBb.putShort((short) 0);
        keyBb.putInt(66); // KeyCode ENTER
        keyBb.putInt(1);  // MetaState SHIFT
        baos.write(keyBb.array());

        // 2. Scroll packet (20 bytes)
        ByteBuffer scrollBb = ByteBuffer.allocate(20).order(ByteOrder.BIG_ENDIAN);
        scrollBb.put((byte) 0x02); // InjectScroll
        scrollBb.put((byte) 0);
        scrollBb.put((byte) 0);
        scrollBb.put((byte) 0);
        scrollBb.putInt(500); // pos_x
        scrollBb.putInt(600); // pos_y
        scrollBb.putInt(-10); // hScroll
        scrollBb.putInt(25);  // vScroll
        baos.write(scrollBb.array());

        ControlMessageReader reader = new ControlMessageReader(new ByteArrayInputStream(baos.toByteArray()), listener);
        assertTrue(reader.readNextMessage());
        assertEquals(0, listener.keyAction.get());
        assertEquals(66, listener.keyCode.get());
        assertEquals(1, listener.keyMetaState.get());

        assertTrue(reader.readNextMessage());
        assertEquals(500, listener.scrollX.get());
        assertEquals(600, listener.scrollY.get());
        assertEquals(-10, listener.scrollH.get());
        assertEquals(25, listener.scrollV.get());
    }

    @Test
    public void testClipboardDeserializationAndLimitEnforcement() throws IOException {
        MockListener listener = new MockListener();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();

        // Valid clipboard
        String testText = "Mirror Clipboard Content!";
        byte[] textBytes = testText.getBytes(StandardCharsets.UTF_8);

        ByteBuffer clipBb = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
        clipBb.put((byte) 0x03); // SetClipboard
        clipBb.put((byte) 0x01); // PasteNow = true
        clipBb.putShort((short) 0);
        clipBb.putInt(textBytes.length);

        baos.write(clipBb.array());
        baos.write(textBytes);

        ControlMessageReader reader = new ControlMessageReader(new ByteArrayInputStream(baos.toByteArray()), listener);
        assertTrue(reader.readNextMessage());
        assertEquals(testText, listener.clipboardText.get());
        assertTrue(listener.clipboardPasteNow.get());

        // Oversized clipboard (> 256 KB)
        ByteBuffer overBb = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
        overBb.put((byte) 0x03);
        overBb.put((byte) 0x00);
        overBb.putShort((short) 0);
        overBb.putInt(262145); // Exceeds 262144

        ControlMessageReader malformedReader = new ControlMessageReader(new ByteArrayInputStream(overBb.array()), listener);
        IOException ex = assertThrows(IOException.class, malformedReader::readNextMessage);
        assertTrue(ex.getMessage().contains("Clipboard length exceeds maximum permitted size"));
    }

    @Test
    public void testPowerKeyframeHeartbeatAndInvalidType() throws IOException {
        MockListener listener = new MockListener();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();

        // Screen Power ON (0x04, 0x02)
        baos.write(new byte[]{0x04, 0x02});

        // Keyframe Request (0x05)
        baos.write(new byte[]{0x05});

        // Heartbeat (0x06, pingId=9999)
        ByteBuffer hbBb = ByteBuffer.allocate(5).order(ByteOrder.BIG_ENDIAN);
        hbBb.put((byte) 0x06);
        hbBb.putInt(9999);
        baos.write(hbBb.array());

        ControlMessageReader reader = new ControlMessageReader(new ByteArrayInputStream(baos.toByteArray()), listener);

        assertTrue(reader.readNextMessage());
        assertTrue(listener.screenPowerOn.get());

        assertTrue(reader.readNextMessage());
        assertTrue(listener.keyframeRequested.get());

        assertTrue(reader.readNextMessage());
        assertEquals(9999, listener.heartbeatPingId.get());

        // Rejection of unknown type (e.g. 0x99)
        ControlMessageReader invalidReader = new ControlMessageReader(new ByteArrayInputStream(new byte[]{(byte) 0x99}), listener);
        IOException ex = assertThrows(IOException.class, invalidReader::readNextMessage);
        assertTrue(ex.getMessage().contains("Unrecognized control message type"));
    }
}
