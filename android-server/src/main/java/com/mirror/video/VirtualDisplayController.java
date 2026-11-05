package com.mirror.video;

import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.IBinder;
import android.view.Display;
import android.view.Surface;

import com.mirror.compat.DisplayManagerBridge;

/**
 * Encapsulates display mirroring creation via SurfaceControl (or fallback to DisplayManager),
 * Surface binding, and deterministic teardown.
 */
public class VirtualDisplayController {

    public static final int VIRTUAL_DISPLAY_FLAG_PUBLIC = 1;
    public static final int VIRTUAL_DISPLAY_FLAG_SECURE = 4;
    public static final int VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR = 16;

    private final int width;
    private final int height;

    private IBinder displayToken;
    private VirtualDisplay virtualDisplay;
    private Surface surface;

    public VirtualDisplayController(int width, int height) {
        this.width = width;
        this.height = height;
    }

    public synchronized void setup(Surface inputSurface) throws ReflectiveOperationException {
        release();
        this.surface = inputSurface;

        // 1. Primary method: SurfaceControl (accessible under shell UID 2000 via ACCESS_SURFACE_FLINGER)
        try {
            Point realSize = DisplayManagerBridge.getDisplayRealSize(Display.DEFAULT_DISPLAY);
            Rect deviceRect = new Rect(0, 0, realSize.x, realSize.y);
            Rect displayRect = new Rect(0, 0, width, height);

            IBinder token = DisplayManagerBridge.createDisplay("mirror", false);
            if (token != null) {
                DisplayManagerBridge.setDisplaySurface(token, inputSurface, deviceRect, displayRect, 0);
                this.displayToken = token;
                System.out.println("Display mirroring initialized via SurfaceControl (" + width + "x" + height + ")");
                return;
            }
        } catch (Throwable t) {
            System.err.println("SurfaceControl display creation failed: " + t.getMessage() + ", falling back to DisplayManager...");
        }

        // 2. Fallback: DisplayManagerGlobal (requires CAPTURE_VIDEO_OUTPUT / system UID)
        int flags = VIRTUAL_DISPLAY_FLAG_PUBLIC | VIRTUAL_DISPLAY_FLAG_SECURE | VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR;
        int displayId = Display.DEFAULT_DISPLAY;
        this.virtualDisplay = DisplayManagerBridge.createVirtualDisplay(
                "MirrorDisplay",
                width,
                height,
                displayId,
                inputSurface,
                flags
        );
        System.out.println("Display mirroring initialized via DisplayManager (" + width + "x" + height + ")");
    }

    public synchronized void release() {
        if (displayToken != null) {
            try {
                DisplayManagerBridge.destroyDisplay(displayToken);
            } catch (Exception ignored) {
            }
            displayToken = null;
        }
        if (virtualDisplay != null) {
            try {
                virtualDisplay.release();
            } catch (Exception ignored) {
            }
            virtualDisplay = null;
        }
        surface = null;
    }

    public synchronized IBinder getDisplayToken() {
        return displayToken;
    }

    public synchronized VirtualDisplay getVirtualDisplay() {
        return virtualDisplay;
    }

    public synchronized Surface getSurface() {
        return surface;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }
}
