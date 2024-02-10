package com.panda3ds.pandroid;

import android.util.Log;

public class AlberDriver {
	AlberDriver() { super(); }

	public static native void Setup();
	public static native void Initialize();
	public static native void RunFrame(int fbo);
	public static native boolean HasRomLoaded();
	public static native boolean LoadRom(String path);
	public static native void Finalize();

	public static native void KeyDown(int code);
	public static native void KeyUp(int code);
	public static native void SetCirclepadAxis(int x, int y);
	public static native void TouchScreenUp();
	public static native void TouchScreenDown(int x, int y);
	public static native void Pause();
	public static native void Resume();
	public static native void LoadLuaScript(String script);
	public static native byte[] GetSmdh();

	public static native void setShaderJitEnabled(boolean enable);

	static { System.loadLibrary("Alber"); }
}
