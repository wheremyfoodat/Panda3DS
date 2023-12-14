package com.panda3ds.pandroid.app;

import android.app.Application;
import android.content.Context;

import com.panda3ds.pandroid.data.config.GlobalConfig;

public class PandaApplication extends Application {
    private static Context appContext;

    @Override
    public void onCreate() {
        super.onCreate();
        appContext = this;
        GlobalConfig.initialize();
    }

    public static Context getAppContext() {
        return appContext;
    }
}
