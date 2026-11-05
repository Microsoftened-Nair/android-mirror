package com.mirror.compat;

import android.os.IBinder;
import android.view.InputEvent;

import java.lang.reflect.Method;

/**
 * Reflection bridge for android.hardware.input.IInputManager / InputManager to inject
 * MotionEvent and KeyEvent asynchronously with zero per-event overhead.
 */
public class InputManagerBridge {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;

    private final Object inputManager;
    private final Method injectInputEventMethod;

    public InputManagerBridge(Object inputManager, Method injectInputEventMethod) {
        this.inputManager = inputManager;
        this.injectInputEventMethod = injectInputEventMethod;
    }

    public InputManagerBridge() throws ReflectiveOperationException {
        Object im = null;
        Method injectMethod = null;

        // Try InputManager.getInstance()
        try {
            Class<?> inputManagerClass = Class.forName("android.hardware.input.InputManager");
            Method getInstanceMethod = inputManagerClass.getMethod("getInstance");
            im = getInstanceMethod.invoke(null);
            if (im != null) {
                injectMethod = findInjectMethod(inputManagerClass);
            }
        } catch (Throwable ignored) {
        }

        // Fallback to IInputManager$Stub.asInterface(ServiceManager.getService("input"))
        if (im == null || injectMethod == null) {
            IBinder binder = ServiceManager.getService("input");
            if (binder == null) {
                throw new ReflectiveOperationException("ServiceManager returned null for 'input' service");
            }
            Class<?> stubClass = Class.forName("android.hardware.input.IInputManager$Stub");
            Method asInterface = stubClass.getMethod("asInterface", IBinder.class);
            im = asInterface.invoke(null, binder);
            if (im == null) {
                throw new ReflectiveOperationException("Failed to obtain IInputManager interface from binder");
            }
            injectMethod = findInjectMethod(im.getClass());
        }

        this.inputManager = im;
        this.injectInputEventMethod = injectMethod;
    }

    private static Method findInjectMethod(Class<?> clazz) throws NoSuchMethodException {
        for (Method m : clazz.getMethods()) {
            if ("injectInputEvent".equals(m.getName())) {
                Class<?>[] pts = m.getParameterTypes();
                if (pts.length >= 2 && InputEvent.class.isAssignableFrom(pts[0]) && pts[1] == int.class) {
                    return m;
                }
            }
        }
        return clazz.getMethod("injectInputEvent", InputEvent.class, int.class);
    }

    public void injectInputEvent(InputEvent event, int mode) throws ReflectiveOperationException {
        if (injectInputEventMethod.getParameterCount() == 2) {
            injectInputEventMethod.invoke(inputManager, event, mode);
        } else if (injectInputEventMethod.getParameterCount() == 3) {
            injectInputEventMethod.invoke(inputManager, event, mode, 0);
        } else {
            injectInputEventMethod.invoke(inputManager, event);
        }
    }

    public void injectInputEvent(InputEvent event) throws ReflectiveOperationException {
        injectInputEvent(event, INJECT_INPUT_EVENT_MODE_ASYNC);
    }
}
