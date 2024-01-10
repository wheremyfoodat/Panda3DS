package com.panda3ds.pandroid.app;

import android.app.Application;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.utils.GameUtils;


public class PandroidApplication extends Application {
	private static Context appContext;

	@Override
	public void onCreate() {
		super.onCreate();
		appContext = this;

		GlobalConfig.initialize();
		GameUtils.initialize();
		InputMap.initialize();
		AlberDriver.Setup();
	}

	public static int getThemeId() {
		switch (GlobalConfig.get(GlobalConfig.KEY_APP_THEME)) {
			case GlobalConfig.THEME_LIGHT:
				return R.style.Theme_Pandroid_Light;
			case GlobalConfig.THEME_DARK:
				return R.style.Theme_Pandroid_Dark;
			case GlobalConfig.THEME_BLACK:
				return R.style.Theme_Pandroid_Black;
		}
		return R.style.Theme_Pandroid;
	}

	public static boolean isDarkMode() {
		switch (GlobalConfig.get(GlobalConfig.KEY_APP_THEME)) {
			case GlobalConfig.THEME_DARK:
			case GlobalConfig.THEME_BLACK:
				return true;
			case GlobalConfig.THEME_LIGHT:
				return false;
		}
		Resources res = Resources.getSystem();
		int nightFlags = res.getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
		return nightFlags == Configuration.UI_MODE_NIGHT_YES;
	}

	public static Context getAppContext() { return appContext; }
}
