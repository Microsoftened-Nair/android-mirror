package com.mirror.compat;

import android.graphics.Point;
import android.graphics.Rect;
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
            // 1. Try legacy signature (API <= 29): (String name, int width, int height, int displayId, Surface surface, int flags, ...)
            for (Method m : dmg.getClass().getDeclaredMethods()) {
                if ("createVirtualDisplay".equals(m.getName())) {
                    Class<?>[] pts = m.getParameterTypes();
                    if (pts.length >= 6 && pts[0] == String.class && pts[1] == int.class && pts[2] == int.class) {
                        m.setAccessible(true);
                        createVirtualDisplayMethod = m;
                        break;
                    }
                }
            }

            // 2. Try VirtualDisplayConfig signature taking String packageName (API 30-33)
            if (createVirtualDisplayMethod == null) {
                for (Method m : dmg.getClass().getDeclaredMethods()) {
                    if ("createVirtualDisplay".equals(m.getName())) {
                        Class<?>[] pts = m.getParameterTypes();
                        if (pts.length == 5 && pts[0] == String.class) {
                            m.setAccessible(true);
                            createVirtualDisplayMethod = m;
                            break;
                        }
                    }
                }
            }

            // 3. Try modern VirtualDisplayConfig signature (API 34+)
            if (createVirtualDisplayMethod == null) {
                for (Method m : dmg.getClass().getDeclaredMethods()) {
                    if ("createVirtualDisplay".equals(m.getName())) {
                        Class<?>[] pts = m.getParameterTypes();
                        if (pts.length == 3) {
                            m.setAccessible(true);
                            createVirtualDisplayMethod = m;
                            break;
                        }
                    }
                }
            }

            // 4. Any other createVirtualDisplay fallback
            if (createVirtualDisplayMethod == null) {
                for (Method m : dmg.getClass().getDeclaredMethods()) {
                    if ("createVirtualDisplay".equals(m.getName())) {
                        m.setAccessible(true);
                        createVirtualDisplayMethod = m;
                        break;
                    }
                }
            }
        }

        if (createVirtualDisplayMethod == null) {
            throw new NoSuchMethodException("No matching createVirtualDisplay method found on DisplayManagerGlobal");
        }

        createVirtualDisplayMethod.setAccessible(true);
        Class<?>[] paramTypes = createVirtualDisplayMethod.getParameterTypes();

        if (paramTypes.length >= 6 && paramTypes[0] == String.class && paramTypes[1] == int.class) {
            Object[] args = new Object[paramTypes.length];
            args[0] = name;
            args[1] = width;
            args[2] = height;
            args[3] = displayId;
            args[4] = surface;
            args[5] = flags;
            return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, args);
        } else {
            // Build VirtualDisplayConfig reflectively
            Class<?> configBuilderClass = Class.forName("android.hardware.display.VirtualDisplayConfig$Builder");
            Object builder = configBuilderClass.getConstructor(String.class, int.class, int.class, int.class)
                    .newInstance(name, width, height, 1);
            builder.getClass().getMethod("setSurface", Surface.class).invoke(builder, surface);
            builder.getClass().getMethod("setFlags", int.class).invoke(builder, flags);
            Object config = builder.getClass().getMethod("build").invoke(builder);

            if (paramTypes.length == 5) {
                Object arg0 = (paramTypes[0] == String.class) ? "com.android.shell" : null;
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, arg0, null, config, null, null);
            } else if (paramTypes.length == 3) {
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, config, null, null);
            } else if (paramTypes.length == 1) {
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, config);
            } else {
                Object[] args = new Object[paramTypes.length];
                for (int i = 0; i < paramTypes.length; i++) {
                    if (paramTypes[i].getName().contains("VirtualDisplayConfig")) {
                        args[i] = config;
                    } else if (paramTypes[i] == String.class) {
                        args[i] = "com.android.shell";
                    } else {
                        args[i] = null;
                    }
                }
                return (VirtualDisplay) createVirtualDisplayMethod.invoke(dmg, args);
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

    public static Point getDisplayRealSize(int displayId) throws ReflectiveOperationException {
        Throwable lastException = null;

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
        } catch (Throwable t) {
            lastException = t;
        }

        // 2. Try IWindowManager.getInitialDisplaySize(displayId, outPoint)
        try {
            IBinder binder = ServiceManager.getService("window");
            if (binder != null) {
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
            }
        } catch (Throwable t) {
            if (lastException != null) {
                t.addSuppressed(lastException);
            }
            lastException = t;
        }

        throw new ReflectiveOperationException("Failed to determine display size via DisplayManagerGlobal or IWindowManager", lastException);
    }

    public static IBinder createDisplay(String name, boolean secure) throws ReflectiveOperationException {
        Class<?> sc = Class.forName("android.view.SurfaceControl");
        Method method = sc.getMethod("createDisplay", String.class, boolean.class);
        return (IBinder) method.invoke(null, name, secure);
    }

    public static void setDisplaySurface(IBinder displayToken, Surface surface, Rect deviceRect, Rect displayRect, int layerStack)
            throws ReflectiveOperationException {
        Class<?> sc = Class.forName("android.view.SurfaceControl");

        // 1. Try modern Transaction API (API 29+/31+)
        try {
            Class<?> txClass = Class.forName("android.view.SurfaceControl$Transaction");
            Object tx = txClass.getConstructor().newInstance();
            Method setSurf = txClass.getMethod("setDisplaySurface", IBinder.class, Surface.class);
            Method setProj = txClass.getMethod("setDisplayProjection", IBinder.class, int.class, Rect.class, Rect.class);
            Method setLayer = txClass.getMethod("setDisplayLayerStack", IBinder.class, int.class);
            Method apply = txClass.getMethod("apply");

            setSurf.invoke(tx, displayToken, surface);
            setProj.invoke(tx, displayToken, 0, deviceRect, displayRect);
            setLayer.invoke(tx, displayToken, layerStack);
            apply.invoke(tx);
            return;
        } catch (Throwable ignored) {
            // Fallback to legacy static methods on SurfaceControl (API <= 30)
        }

        // 2. Legacy static methods
        Method openTx = sc.getMethod("openTransaction");
        Method closeTx = sc.getMethod("closeTransaction");
        Method setSurface = sc.getMethod("setDisplaySurface", IBinder.class, Surface.class);
        Method setProjection = sc.getMethod("setDisplayProjection", IBinder.class, int.class, Rect.class, Rect.class);
        Method setLayerStack = sc.getMethod("setDisplayLayerStack", IBinder.class, int.class);

        openTx.invoke(null);
        try {
            setSurface.invoke(null, displayToken, surface);
            setProjection.invoke(null, displayToken, 0, deviceRect, displayRect);
            setLayerStack.invoke(null, displayToken, layerStack);
        } finally {
            closeTx.invoke(null);
        }
    }

    public static void destroyDisplay(IBinder displayToken) {
        if (displayToken == null) {
            return;
        }
        try {
            Class<?> sc = Class.forName("android.view.SurfaceControl");
            Method destroy = sc.getMethod("destroyDisplay", IBinder.class);
            destroy.invoke(null, displayToken);
        } catch (Exception ignored) {
        }
    }
}
