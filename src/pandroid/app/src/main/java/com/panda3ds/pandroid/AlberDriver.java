package com.panda3ds.pandroid;

import android.content.Context;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;

import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.utils.GameUtils;

import java.io.File;
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

	public static int openDocument(String path, String mode){
		try {
			mode = mode.substring(0,1);
			Context context = PandroidApplication.getAppContext();
			Uri uri = FileUtils.obtainUri(path);
			ParcelFileDescriptor parcel;
			if (Objects.equals(uri.getScheme(), "game")) {
				uri = FileUtils.obtainUri(GameUtils.getCurrentGame().getRomPath());
				parcel = context.getContentResolver().openFileDescriptor(uri, "r");
			} else {
				parcel = context.getContentResolver().openFileDescriptor(uri, mode);
			}
			int fd = parcel.detachFd();
			parcel.close();

			return fd;
		} catch (Exception e){
			throw new RuntimeException(e);
		}
	}
	static { System.loadLibrary("Alber"); }
}
