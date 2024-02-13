package com.panda3ds.pandroid.utils;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Debug;
import android.os.Process;

import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.data.config.GlobalConfig;

public class PerformanceMonitor {
    private static int fps = 1;
    private static String backend = "";
    private static int frames = 0;
    private static long lastUpdate = 0;
    private static long totalMemory = 1;
    private static long availableMemory = 0;

    public static void initialize(String backendName) {
        fps = 1;
        backend = backendName;
    }

    public static void runFrame() {
        if (GlobalConfig.get(GlobalConfig.KEY_SHOW_PERFORMANCE_OVERLAY)) {
            frames++;
            if (System.currentTimeMillis() - lastUpdate > 1000) {
                lastUpdate = System.currentTimeMillis();
                fps = frames;
                frames = 0;
                try {
                    Context ctx = PandroidApplication.getAppContext();
                    ActivityManager manager = (ActivityManager) ctx.getSystemService(Context.ACTIVITY_SERVICE);
                    ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
                    manager.getMemoryInfo(info);
                    totalMemory = info.totalMem;
                    availableMemory = info.availMem;
                } catch (Exception e) {}
            }
        }
    }

    public static long getUsedMemory() {
        return Math.max(1, totalMemory - availableMemory);
    }

    public static long getTotalMemory() {
        return totalMemory;
    }

    public static long getAvailableMemory() {
        return availableMemory;
    }

    public static int getFps() {
        return fps;
    }

    public static String getBackend() {
        return backend;
    }

    public static void destroy() {}
}
