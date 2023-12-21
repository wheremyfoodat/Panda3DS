package com.panda3ds.pandroid;

import android.util.Log;

import com.panda3ds.pandroid.data.SMDH;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.GameUtils;

public class AlberDriver {
	AlberDriver() { super(); }

	public static native void Setup();
	public static native void Initialize();
	public static native void RunFrame(int fbo);
	public static native boolean HasRomLoaded();
	public static native void LoadRom(String path);
	public static native void Finalize();

	public static native void KeyDown(int code);
	public static native void KeyUp(int code);
	public static native void SetCirclepadAxis(int x, int y);
	public static native void TouchScreenUp();
	public static native void TouchScreenDown(int x, int y);

	public static void OnSmdhLoaded(byte[] buffer) {
		SMDH smdh = new SMDH(buffer);
		Log.i(Constants.LOG_TAG, "Loaded rom SDMH");
		Log.i(Constants.LOG_TAG, String.format("Are you playing '%s' published by '%s'", smdh.getTitle(), smdh.getPublisher()));
		GameMetadata game = GameUtils.getCurrentGame();
		GameUtils.removeGame(game);
		GameUtils.addGame(GameMetadata.applySMDH(game, smdh));
	}

	static { System.loadLibrary("Alber"); }
}