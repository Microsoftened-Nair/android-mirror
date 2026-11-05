package com.mirror.compat;

import android.os.IBinder;

import java.lang.reflect.Method;

/**
 * Wrapper around android.os.ServiceManager to query hidden system service binders and interface stubs.
 */
public final class ServiceManager {

    private static Class<?> serviceManagerClass;
    private static Method getServiceMethod;

    static {
        try {
            serviceManagerClass = Class.forName("android.os.ServiceManager");
            getServiceMethod = serviceManagerClass.getMethod("getService", String.class);
        } catch (Throwable ignored) {
        }
    }

    private ServiceManager() {
    }

    public static IBinder getService(String serviceName) throws ReflectiveOperationException {
        if (getServiceMethod == null) {
            serviceManagerClass = Class.forName("android.os.ServiceManager");
            getServiceMethod = serviceManagerClass.getMethod("getService", String.class);
        }
        try {
            return (IBinder) getServiceMethod.invoke(null, serviceName);
        } catch (Exception e) {
            throw new ReflectiveOperationException("Failed to get service: " + serviceName, e);
        }
    }
}
