package com.mirror.video;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

public class DisplayCalculatorTest {

    @Test
    public void testComputeDisplaySizeUnlimited() {
        // 1080 is not divisible by 16: 1080 / 16 = 67 -> 67 * 16 = 1072
        // 1920 is divisible by 16: 1920 / 16 = 120 -> 120 * 16 = 1920
        DisplayCalculator.Size size = DisplayCalculator.computeDisplaySize(1080, 1920, 0);
        assertEquals(1072, size.width);
        assertEquals(1920, size.height);
        assertEquals(0, size.width % 16);
        assertEquals(0, size.height % 16);
    }

    @Test
    public void testComputeDisplaySizeScaled() {
        // native 1080x2400, maxSize 1200
        // scale = 1200 / 2400 = 0.5
        // W = (int)(1080 * 0.5) = 540 -> 540 / 16 * 16 = 528
        // H = (int)(2400 * 0.5) = 1200 -> 1200 / 16 * 16 = 1200
        DisplayCalculator.Size size = DisplayCalculator.computeDisplaySize(1080, 2400, 1200);
        assertEquals(528, size.width);
        assertEquals(1200, size.height);
        assertEquals(0, size.width % 16);
        assertEquals(0, size.height % 16);
    }

    @Test
    public void testComputeDisplaySizeMinimumBound() {
        // Very small native dimensions must be clamped to at least 16x16
        DisplayCalculator.Size size = DisplayCalculator.computeDisplaySize(10, 12, 0);
        assertEquals(16, size.width);
        assertEquals(16, size.height);
    }

    @Test
    public void testComputeDisplaySizeLandscape() {
        // 2560x1440, maxSize 1280
        // scale = 1280 / 2560 = 0.5
        // W = 1280 / 16 * 16 = 1280
        // H = 720 / 16 * 16 = 720
        DisplayCalculator.Size size = DisplayCalculator.computeDisplaySize(2560, 1440, 1280);
        assertEquals(1280, size.width);
        assertEquals(720, size.height);
        assertEquals(0, size.width % 16);
        assertEquals(0, size.height % 16);
    }
}
