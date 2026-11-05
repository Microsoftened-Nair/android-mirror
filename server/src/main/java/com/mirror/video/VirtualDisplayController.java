package com.mirror.video;

import android.hardware.display.VirtualDisplay;
import android.view.Display;
import android.view.Surface;

import com.mirror.compat.DisplayManagerBridge;

/**
 * Encapsulates VirtualDisplay creation, Surface binding, and deterministic teardown.
 */
public class VirtualDisplayController {

    public static final int VIRTUAL_DISPLAY_FLAG_PUBLIC = 1;
    public static final int VIRTUAL_DISPLAY_FLAG_SECURE = 4;
    public static final int VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR = 16;

    private final int width;
    private final int height;

    private VirtualDisplay virtualDisplay;
    private Surface surface;

    public VirtualDisplayController(int width, int height) {
        this.width = width;
        this.height = height;
    }

    public synchronized void setup(Surface inputSurface) throws ReflectiveOperationException {
        release();

        int flags = VIRTUAL_DISPLAY_FLAG_PUBLIC | VIRTUAL_DISPLAY_FLAG_SECURE | VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR;
        int displayId = Display.DEFAULT_DISPLAY;

        this.surface = inputSurface;
        this.virtualDisplay = DisplayManagerBridge.createVirtualDisplay(
                "MirrorDisplay",
                width,
                height,
                displayId,
                inputSurface,
                flags
        );
    }

    public synchronized void release() {
        if (virtualDisplay != null) {
            try {
                virtualDisplay.release();
            } catch (Exception ignored) {
            }
            virtualDisplay = null;
        }
        surface = null;
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
