package com.mirror.video;

/**
 * Pure mathematical utility calculating scaled, mod-16 aligned width and height
 * based on native device dimensions and max-size constraints.
 */
public final class DisplayCalculator {

    public static final class Size {
        public final int width;
        public final int height;

        public Size(int width, int height) {
            this.width = width;
            this.height = height;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            Size size = (Size) o;
            return width == size.width && height == size.height;
        }

        @Override
        public int hashCode() {
            return 31 * width + height;
        }

        @Override
        public String toString() {
            return width + "x" + height;
        }
    }

    private DisplayCalculator() {
    }

    /**
     * Computes scaled and mod-16 aligned display dimensions.
     *
     * @param nativeWidth  raw canvas width
     * @param nativeHeight raw canvas height
     * @param maxSize      maximum bounding box limit (0 for unlimited)
     * @return mod-16 aligned Size, minimum 16x16
     */
    public static Size computeDisplaySize(int nativeWidth, int nativeHeight, int maxSize) {
        int wClamped;
        int hClamped;

        int major = Math.max(nativeWidth, nativeHeight);
        if (maxSize <= 0 || major <= maxSize) {
            wClamped = (nativeWidth / 16) * 16;
            hClamped = (nativeHeight / 16) * 16;
        } else {
            double scale = (double) maxSize / (double) major;
            wClamped = ((int) (nativeWidth * scale) / 16) * 16;
            hClamped = ((int) (nativeHeight * scale) / 16) * 16;
        }

        wClamped = Math.max(16, wClamped);
        hClamped = Math.max(16, hClamped);

        return new Size(wClamped, hClamped);
    }
}
