package com.panda3ds.pandroid;

import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.utils.GameUtils;

import java.util.Objects;

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


	public static String parseNativeMode(String mode){
		mode = mode.toLowerCase();
		switch (mode){
			case "r":
			case "rb":
				return "r";
			case "r+":
			case "r+b":
			case "rb+":
				return "rw";
			case "w+":
				return "rwt";
			case "w":
			case "wb":
				return "wt";
			case "wa":
				return "wa";
		}
		throw new IllegalArgumentException("Invalid file mode: "+mode);
	}

	public static int openDocument(String path, String mode){
		try {
			mode = parseNativeMode(mode);
			Context context = PandroidApplication.getAppContext();
			Uri uri = FileUtils.obtainUri(path);
			ParcelFileDescriptor parcel;
			if (Objects.equals(uri.getScheme(), "game")) {
				if (mode.contains("w")){
					throw new IllegalArgumentException("Cannot write to rom-fs");
				}
				uri = FileUtils.obtainUri(GameUtils.getCurrentGame().getRomPath());
			}
			parcel = context.getContentResolver().openFileDescriptor(uri, mode);
			int fd = parcel.detachFd();
			parcel.close();

			return fd;
		} catch (Exception e){
			throw new RuntimeException(e);
		}
	}
	static { System.loadLibrary("Alber"); }
}
