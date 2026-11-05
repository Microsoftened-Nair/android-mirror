package com.mirror.input;

import android.os.IBinder;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.mirror.compat.InputManagerBridge;
import com.mirror.compat.ServiceManager;
import com.mirror.control.ControlEventListener;
import com.mirror.power.DevicePowerController;
import com.mirror.video.ScreenEncoder;

import java.lang.reflect.Method;
import java.util.Objects;

/**
 * Transforms incoming ControlEventListener callbacks into pooled Android InputEvents
 * and dispatches them via InputManagerBridge with zero per-event heap allocation.
 */
public class InputDispatcher implements ControlEventListener {

    private final InputManagerBridge inputManagerBridge;
    private final DevicePowerController powerController;
    private final ScreenEncoder screenEncoder;

    // Pre-allocated Pointer structures to eliminate garbage collection pressure
    private final MotionEvent.PointerProperties[] pointerProperties = new MotionEvent.PointerProperties[1];
    private final MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[1];

    private long lastDownTime = 0;

    public InputDispatcher(InputManagerBridge inputManagerBridge,
                           DevicePowerController powerController,
                           ScreenEncoder screenEncoder) {
        this.inputManagerBridge = Objects.requireNonNull(inputManagerBridge, "inputManagerBridge cannot be null");
        this.powerController = powerController;
        this.screenEncoder = screenEncoder;

        pointerProperties[0] = new MotionEvent.PointerProperties();
        pointerCoords[0] = new MotionEvent.PointerCoords();
    }

    public InputDispatcher(InputManagerBridge inputManagerBridge) {
        this(inputManagerBridge, null, null);
    }

    @Override
    public synchronized void onTouchEvent(int action, long pointerId, int x, int y,
                                         int canvasWidth, int canvasHeight, int pressure, int buttons) {
        long now = SystemClock.uptimeMillis();
        if (action == MotionEvent.ACTION_DOWN) {
            lastDownTime = now;
        }
        long downTime = (lastDownTime != 0) ? lastDownTime : now;

        pointerProperties[0].id = (int) pointerId;
        pointerProperties[0].toolType = MotionEvent.TOOL_TYPE_FINGER;

        pointerCoords[0].x = x;
        pointerCoords[0].y = y;
        pointerCoords[0].pressure = (pressure > 0) ? (pressure / 65535.0f) : (action == MotionEvent.ACTION_UP ? 0.0f : 1.0f);
        pointerCoords[0].size = 0.0f;

        MotionEvent event = MotionEvent.obtain(
                downTime,
                now,
                action,
                1,
                pointerProperties,
                pointerCoords,
                0,
                buttons,
                1.0f,
                1.0f,
                0,
                0,
                InputDevice.SOURCE_TOUCHSCREEN,
                0
        );

        try {
            inputManagerBridge.injectInputEvent(event, InputManagerBridge.INJECT_INPUT_EVENT_MODE_ASYNC);
        } catch (ReflectiveOperationException ignored) {
        } finally {
            event.recycle();
            if (action == MotionEvent.ACTION_UP) {
                lastDownTime = 0;
            }
        }
    }

    @Override
    public void onKeyEvent(int action, int keyCode, int metaState) {
        long now = SystemClock.uptimeMillis();
        KeyEvent event = new KeyEvent(
                now,
                now,
                action,
                keyCode,
                0,
                metaState,
                0,
                0,
                KeyEvent.FLAG_FROM_SYSTEM,
                InputDevice.SOURCE_KEYBOARD
        );

        try {
            inputManagerBridge.injectInputEvent(event, InputManagerBridge.INJECT_INPUT_EVENT_MODE_ASYNC);
        } catch (ReflectiveOperationException ignored) {
        }
    }

    @Override
    public synchronized void onScrollEvent(int x, int y, int hScroll, int vScroll) {
        long now = SystemClock.uptimeMillis();

        pointerProperties[0].id = 0;
        pointerProperties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        pointerCoords[0].x = x;
        pointerCoords[0].y = y;
        pointerCoords[0].pressure = 0.0f;
        pointerCoords[0].size = 0.0f;
        pointerCoords[0].setAxisValue(MotionEvent.AXIS_HSCROLL, (float) hScroll);
        pointerCoords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, (float) vScroll);

        MotionEvent event = MotionEvent.obtain(
                now,
                now,
                MotionEvent.ACTION_SCROLL,
                1,
                pointerProperties,
                pointerCoords,
                0,
                0,
                1.0f,
                1.0f,
                0,
                0,
                InputDevice.SOURCE_MOUSE,
                0
        );

        try {
            inputManagerBridge.injectInputEvent(event, InputManagerBridge.INJECT_INPUT_EVENT_MODE_ASYNC);
        } catch (ReflectiveOperationException ignored) {
        } finally {
            event.recycle();
            pointerCoords[0].setAxisValue(MotionEvent.AXIS_HSCROLL, 0.0f);
            pointerCoords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, 0.0f);
        }
    }

    @Override
    public void onSetClipboard(String text, boolean pasteNow) {
        setClipboardText(text);
        if (pasteNow) {
            onKeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PASTE, 0);
            onKeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_PASTE, 0);
        }
    }

    @Override
    public void onSetScreenPower(boolean on) {
        if (powerController != null) {
            powerController.setScreenPower(on);
        }
    }

    @Override
    public void onRequestKeyframe() {
        if (screenEncoder != null) {
            screenEncoder.requestKeyframe();
        }
    }

    @Override
    public void onHeartbeatPing(int pingId) {
        // No-op for now or can log heartbeat
    }

    private void setClipboardText(String text) {
        try {
            IBinder binder = ServiceManager.getService("clipboard");
            if (binder == null) {
                return;
            }
            Class<?> stubClass = Class.forName("android.content.IClipboard$Stub");
            Method asInterface = stubClass.getMethod("asInterface", IBinder.class);
            Object clipboard = asInterface.invoke(null, binder);
            if (clipboard == null) {
                return;
            }

            Class<?> clipDataClass = Class.forName("android.content.ClipData");
            Method newPlainText = clipDataClass.getMethod("newPlainText", CharSequence.class, CharSequence.class);
            Object clipData = newPlainText.invoke(null, "mirror", text);

            for (Method m : clipboard.getClass().getMethods()) {
                if ("setPrimaryClip".equals(m.getName())) {
                    Class<?>[] types = m.getParameterTypes();
                    if (types.length == 2 && types[0].isAssignableFrom(clipDataClass) && types[1] == String.class) {
                        m.invoke(clipboard, clipData, "com.mirror");
                        return;
                    } else if (types.length == 3 && types[0].isAssignableFrom(clipDataClass)
                            && types[1] == String.class && types[2] == int.class) {
                        m.invoke(clipboard, clipData, "com.mirror", 0);
                        return;
                    } else if (types.length == 4) {
                        m.invoke(clipboard, clipData, "com.mirror", null, 0);
                        return;
                    }
                }
            }
        } catch (Throwable ignored) {
        }
    }
}
