package com.panda3ds.pandroid.app;

import android.app.Application;
import android.content.Context;

import com.panda3ds.pandroid.AlberDriver;
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

    public static Context getAppContext() {
        return appContext;
    }
}
