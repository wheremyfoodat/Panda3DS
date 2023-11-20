package com.panda3ds.pandroid;

public class AlberDriver {

    AlberDriver() {
        super();
    }

    public static native void Initialize();
    public static native void RunFrame(int fbo);
    public static native void Finalize();

    static {
        System.loadLibrary("Alber");
    }
}