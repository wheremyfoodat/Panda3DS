package com.panda3ds.pandroid.app.game;

import com.panda3ds.pandroid.data.config.GlobalConfig;

public interface EmulatorCallback {
    void onBackPressed();
    void swapScreens(int index);

    default void swapScreens() {
        swapScreens(GlobalConfig.get(GlobalConfig.KEY_CURRENT_DS_LAYOUT) + 1);
    }
}
