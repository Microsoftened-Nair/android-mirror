package com.mirror.power;

import android.os.Binder;
import android.os.IBinder;

import com.mirror.compat.DisplayManagerBridge;
import com.mirror.compat.ServiceManager;

import java.lang.reflect.Method;

/**
 * Manages physical screen power state (SurfaceControl.setDisplayPowerMode)
 * and CPU wake-lock via IPowerManager.
 */
public class DevicePowerController {

    public static final int POWER_MODE_OFF = 0;
    public static final int POWER_MODE_NORMAL = 2;
    public static final int PARTIAL_WAKE_LOCK = 1;

    private final IBinder wakeLockToken = new Binder();
    private boolean wakeLockHeld = false;
    private Object powerManager;
    private Method acquireWakeLockMethod;
    private Method releaseWakeLockMethod;

    public DevicePowerController() {
        try {
            IBinder binder = ServiceManager.getService("power");
            if (binder != null) {
                Class<?> stubClass = Class.forName("android.os.IPowerManager$Stub");
                Method asInterface = stubClass.getMethod("asInterface", IBinder.class);
                powerManager = asInterface.invoke(null, binder);

                for (Method m : powerManager.getClass().getMethods()) {
                    if ("acquireWakeLock".equals(m.getName())) {
                        acquireWakeLockMethod = m;
                        break;
                    }
                }
                for (Method m : powerManager.getClass().getMethods()) {
                    if ("releaseWakeLock".equals(m.getName())) {
                        releaseWakeLockMethod = m;
                        break;
                    }
                }
            }
        } catch (Throwable t) {
            System.err.println("DevicePowerController failed to initialize IPowerManager: " + t.getMessage());
        }
    }

    public void setScreenPower(boolean on) {
        int mode = on ? POWER_MODE_NORMAL : POWER_MODE_OFF;
        boolean success = false;
        try {
            IBinder token = DisplayManagerBridge.getInternalDisplayToken();
            if (token != null) {
                DisplayManagerBridge.setDisplayPowerMode(token, mode);
                success = true;
            }
        } catch (Exception e) {
            // SurfaceControl method may fail on Android 15+
        }

        if (!success) {
            // Fallback for Android 15+ (DisplayManagerGlobal.requestDisplayPower)
            DisplayManagerBridge.requestDisplayPower(0, on);
        }
    }

    public synchronized void acquireWakeLock() {
        if (wakeLockHeld || powerManager == null || acquireWakeLockMethod == null) {
            return;
        }

        try {
            Class<?>[] pts = acquireWakeLockMethod.getParameterTypes();
            Object[] args = new Object[pts.length];
            args[0] = wakeLockToken;
            args[1] = PARTIAL_WAKE_LOCK;
            args[2] = "mirror-wakelock";
            args[3] = "com.android.shell";
            for (int i = 4; i < pts.length; i++) {
                if (pts[i] == int.class) {
                    args[i] = 0;
                } else if (pts[i] == boolean.class) {
                    args[i] = false;
                } else if (pts[i] == long.class) {
                    args[i] = 0L;
                } else if (pts[i] == double.class) {
                    args[i] = 0.0;
                } else if (pts[i] == float.class) {
                    args[i] = 0.0f;
                }
            }

            acquireWakeLockMethod.invoke(powerManager, args);
            wakeLockHeld = true;
        } catch (Exception e) {
            System.err.println("DevicePowerController failed to acquire wake lock: " + e.getMessage());
        }
    }

    public synchronized void releaseWakeLock() {
        if (!wakeLockHeld || powerManager == null || releaseWakeLockMethod == null) {
            return;
        }

        try {
            Class<?>[] pts = releaseWakeLockMethod.getParameterTypes();
            Object[] args = new Object[pts.length];
            args[0] = wakeLockToken;
            for (int i = 1; i < pts.length; i++) {
                if (pts[i] == int.class) {
                    args[i] = 0;
                } else if (pts[i] == boolean.class) {
                    args[i] = false;
                } else if (pts[i] == long.class) {
                    args[i] = 0L;
                }
            }

            releaseWakeLockMethod.invoke(powerManager, args);
            wakeLockHeld = false;
        } catch (Exception e) {
            System.err.println("DevicePowerController failed to release wake lock: " + e.getMessage());
        }
    }

    public void restoreDefaults() {
        setScreenPower(true);
        releaseWakeLock();
    }

    public boolean isWakeLockHeld() {
        return wakeLockHeld;
    }
}
