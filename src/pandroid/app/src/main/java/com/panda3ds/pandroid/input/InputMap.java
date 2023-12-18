package com.panda3ds.pandroid.input;

import android.content.Context;
import android.content.SharedPreferences;

import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.Constants;

public class InputMap {

    private static SharedPreferences data;
    private static final String KEY_DEAD_ZONE = "deadZone";

    public static void initialize() {
        data = PandroidApplication.getAppContext().getSharedPreferences(Constants.PREF_INPUT_MAP, Context.MODE_PRIVATE);
    }

    public static float getDeadZone() {
        return data.getFloat(KEY_DEAD_ZONE, 0.2f);
    }

    public static void set(KeyName key, String name) {
        data.edit().putString(key.name(), name).apply();
    }

    public static String relative(KeyName key) {
        return data.getString(key.name(), "-");
    }

    public static KeyName relative(String name) {
        for (KeyName key : KeyName.values()) {
            if (relative(key).equalsIgnoreCase(name))
                return key;
        }
        return KeyName.NULL;
    }

    public static void setDeadZone(float value) {
        data.edit().putFloat(KEY_DEAD_ZONE, Math.max(0.0f, Math.min(1.0f, value))).apply();
    }

}
