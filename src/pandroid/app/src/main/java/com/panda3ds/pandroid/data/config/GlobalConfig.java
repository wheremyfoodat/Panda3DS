package com.panda3ds.pandroid.data.config;

import android.content.Context;
import android.content.SharedPreferences;

import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.Constants;

import java.io.Serializable;

public class GlobalConfig {
    private static SharedPreferences data;

    public static void initialize() {
        data = PandroidApplication.getAppContext()
                .getSharedPreferences(Constants.PREF_GLOBAL_CONFIG, Context.MODE_PRIVATE);
    }

    public static <T extends Serializable> T get(Key<T> key) {
        Serializable value;
        if (key.defaultValue instanceof String) {
            value = data.getString(key.name, (String) key.defaultValue);
        } else if (key.defaultValue instanceof Integer) {
            value = data.getInt(key.name, (int) key.defaultValue);
        } else if (key.defaultValue instanceof Boolean) {
            value = data.getBoolean(key.name, (boolean) key.defaultValue);
        } else if (key.defaultValue instanceof Long) {
            value = data.getLong(key.name, (long) key.defaultValue);
        } else {
            value = data.getFloat(key.name, (float) key.defaultValue);
        }
        return (T) value;
    }

    public static synchronized <T extends Serializable> void set(Key<T> key, T value) {
        if (value instanceof String) {
            data.edit().putString(key.name, (String) value).apply();
        } else if (value instanceof Integer) {
            data.edit().putInt(key.name, (int) value).apply();
        } else if (value instanceof Boolean) {
            data.edit().putBoolean(key.name, (boolean) value).apply();
        } else if (value instanceof Long) {
            data.edit().putLong(key.name, (long) value).apply();
        } else if (value instanceof Float) {
            data.edit().putFloat(key.name, (float) value).apply();
        } else {
            throw new IllegalArgumentException("Invalid global config value instance");
        }
    }

    private static class Key<T extends Serializable> {
        private final String name;
        private final T defaultValue;

        private Key(String name, T defaultValue) {
            this.name = name;
            this.defaultValue = defaultValue;
        }
    }
}
