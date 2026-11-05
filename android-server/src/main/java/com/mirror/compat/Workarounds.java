package com.mirror.compat;

import java.lang.reflect.Method;

/**
 * Android runtime compatibility workarounds.
 * Grants hidden API exemptions across Android 9+ (API 28+) so reflection
 * on internal framework classes is unrestricted by OEM policies.
 */
public final class Workarounds {

    private Workarounds() {
    }

    public static void prepare() {
        // Exempt all private APIs from hidden API enforcement (Android 9+)
        try {
            Class<?> vmRuntimeClass = Class.forName("dalvik.system.VMRuntime");
            Method getRuntimeMethod = vmRuntimeClass.getDeclaredMethod("getRuntime");
            Object vmRuntime = getRuntimeMethod.invoke(null);
            Method setExemptionsMethod = vmRuntimeClass.getDeclaredMethod("setHiddenApiExemptions", String[].class);
            setExemptionsMethod.invoke(vmRuntime, (Object) new String[]{"L"});
        } catch (Throwable ignored) {
            // Android <= 8.1 has no hidden API blacklist
        }
    }
}
