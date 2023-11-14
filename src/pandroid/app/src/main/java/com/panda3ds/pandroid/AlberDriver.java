package com.panda3ds.pandroid;

public class AlberDriver {

    public static native void Initialize();

    static {
        System.loadLibrary("Alber");
    }
}