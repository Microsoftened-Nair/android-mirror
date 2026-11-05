package com.mirror.compat;

import android.graphics.Point;
import android.hardware.display.VirtualDisplay;
import android.os.IBinder;
import android.view.Surface;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Reflection bridge for DisplayManagerGlobal, physical display token retrieval,
 * and virtual display creation across multi-API levels.
 */
public final class DisplayManagerBridge {

    private static Object displayManagerGlobalInstance;
    private static Method getDisplayInfoMethod;
    private static Method createVirtualDisplayMethod;

    private DisplayManagerBridge() {
    }

    public static synchronized Object getDisplayManagerGlobal() throws ReflectiveOperationException {
        if (displayManagerGlobalInstance == null) {
            Class<?> dmgClass = Class.forName("android.hardware.display.DisplayManagerGlobal");
            Method getInstanceMethod = dmgClass.getMethod("getInstance");
            displayManagerGlobalInstance = getInstanceMethod.invoke(null);
        }
        return displayManagerGlobalInstance;
    }

    public static VirtualDisplay createVirtualDisplay(String name, int width, int height, int displayId,
                                                      Surface surface, int flags) throws ReflectiveOperationException {
        Object dmg = getDisplayManagerGlobal();
        if (createVirtualDisplayMethod == null) {
            for (Method m : dmg.getClass().getDeclaredMethods()) {
                if ("createVirtualDisplay".equals(m.getName())) {
                    Class<?>[] pts = m.getParameterTypes();
                    if (pts.length >= 6 && pts[0] == String.class && pts[1] == int.class && pts[2] == int.class) {
                        createVirtualDisplayMethod = m;
                        break;
                    }
                }
            }
            if (createVirtualDisplayMethod == null) {
                // Fallback for modern VirtualDisplayConfig signature if legacy string method is absent
                for (Method m : dmg.getClass().getDeclaredMethods()) {
                    if ("createVirtualDisplay".equals(m.getName())) {
                        createVirtualDisplayMethod = m;
                        break;
                    }
                }
            }
        }

        if (createVirtualDisplayMethod == null) {
            throw new NoSuchMethodException("No matching createVirtualDisplay method found on DisplayManagerGlobal");
        }

        Class<?>[] paramTypes = createVirtualDisplayMethod.getParameterTypes();
        Object[] args = new Object[paramTypes.length];

        if (paramTypes.length >= 6 && paramTypes[0] == String.class) {
            args[0] = name;
            args[1] = width;
            args[2] = height;
            args[3] = displayId;
            args[4] = surface;
            args[5] = flags;
            // Any remaining parameters (callback, handler, uniqueId) remain null
            return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, args);
        } else {
            // Build VirtualDisplayConfig reflectively
            Class<?> configBuilderClass = Class.forName("android.hardware.display.VirtualDisplayConfig$Builder");
            Object builder = configBuilderClass.getConstructor(String.class, int.class, int.class, int.class)
                    .newInstance(name, width, height, 1);
            builder.getClass().getMethod("setSurface", Surface.class).invoke(builder, surface);
            builder.getClass().getMethod("setFlags", int.class).invoke(builder, flags);
            Object config = builder.getClass().getMethod("build").invoke(builder);

            if (paramTypes.length == 3) {
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, config, null, null);
            } else {
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, config);
            }
        }
    }

    public static IBinder getInternalDisplayToken() throws ReflectiveOperationException {
        Class<?> surfaceControlClass = Class.forName("android.view.SurfaceControl");

        // API <= 30
        try {
            Method m = surfaceControlClass.getMethod("getInternalDisplayToken");
            return (IBinder) m.invoke(null);
        } catch (NoSuchMethodException ignored) {
        }

        // API >= 31 fallback via getPhysicalDisplayIds / getDisplayToken
        try {
            Method getPhysicalDisplayIdsMethod = null;
            try {
                getPhysicalDisplayIdsMethod = surfaceControlClass.getMethod("getPhysicalDisplayIds");
            } catch (NoSuchMethodException ignored) {
                getPhysicalDisplayIdsMethod = surfaceControlClass.getMethod("getDisplayIds");
            }

            long[] physicalDisplayIds = (long[]) getPhysicalDisplayIdsMethod.invoke(null);
            if (physicalDisplayIds != null && physicalDisplayIds.length > 0) {
                Method getDisplayTokenMethod = null;
                try {
                    getDisplayTokenMethod = surfaceControlClass.getMethod("getPhysicalDisplayToken", long.class);
                } catch (NoSuchMethodException ignored) {
                    getDisplayTokenMethod = surfaceControlClass.getMethod("getDisplayToken", long.class);
                }
                return (IBinder) getDisplayTokenMethod.invoke(null, physicalDisplayIds[0]);
            }
        } catch (Exception e) {
            throw new ReflectiveOperationException("Failed to obtain physical display token on modern Android release", e);
        }

        throw new ReflectiveOperationException("No internal or physical display token could be retrieved from SurfaceControl");
    }

    public static void setDisplayPowerMode(IBinder token, int mode) throws ReflectiveOperationException {
        Class<?> surfaceControlClass = Class.forName("android.view.SurfaceControl");
        Method method = surfaceControlClass.getMethod("setDisplayPowerMode", IBinder.class, int.class);
        method.invoke(null, token, mode);
    }

    public static Point getDisplayRealSize(int displayId) {
        // 1. Try DisplayManagerGlobal.getDisplayInfo(displayId)
        try {
            Object dmg = getDisplayManagerGlobal();
            if (getDisplayInfoMethod == null) {
                getDisplayInfoMethod = dmg.getClass().getMethod("getDisplayInfo", int.class);
            }
            Object displayInfo = getDisplayInfoMethod.invoke(dmg, displayId);
            if (displayInfo != null) {
                Field lw = displayInfo.getClass().getDeclaredField("logicalWidth");
                Field lh = displayInfo.getClass().getDeclaredField("logicalHeight");
                lw.setAccessible(true);
                lh.setAccessible(true);
                int w = lw.getInt(displayInfo);
                int h = lh.getInt(displayInfo);
                if (w > 0 && h > 0) {
                    return new Point(w, h);
                }
            }
        } catch (Throwable ignored) {
        }

        // 2. Try IWindowManager.getInitialDisplaySize(displayId, outPoint)
        try {
            IBinder binder = ServiceManager.getService("window");
            Class<?> stubClass = Class.forName("android.view.IWindowManager$Stub");
            Method asInterface = stubClass.getMethod("asInterface", IBinder.class);
            Object wm = asInterface.invoke(null, binder);

            Point outPoint = new Point();
            Method getInitialSize;
            try {
                getInitialSize = wm.getClass().getMethod("getInitialDisplaySize", int.class, Point.class);
            } catch (NoSuchMethodException ignored) {
                getInitialSize = wm.getClass().getMethod("getBaseDisplaySize", int.class, Point.class);
            }
            getInitialSize.invoke(wm, displayId, outPoint);
            if (outPoint.x > 0 && outPoint.y > 0) {
                return outPoint;
            }
        } catch (Throwable ignored) {
        }

        // Safe fallback default if neither service can be reached
        return new Point(1080, 2400);
    }
}
