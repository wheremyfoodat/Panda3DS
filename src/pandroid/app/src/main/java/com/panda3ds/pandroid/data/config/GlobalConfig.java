package com.panda3ds.pandroid.data.config;

import android.content.Context;
import android.content.SharedPreferences;

import com.panda3ds.pandroid.app.PandaApplication;
import com.panda3ds.pandroid.utils.Constants;

import java.io.Serializable;

public class GlobalConfig {
    private static SharedPreferences data;

    public static void initialize() {
        data = PandaApplication.getAppContext()
                .getSharedPreferences(Constants.PREF_GLOBAL_CONFIG, Context.MODE_PRIVATE);
    }

    private static <T extends Serializable> T get(Key<T> key) {
        Serializable value;
        if (key.defValue instanceof String) {
            value = data.getString(key.name, (String) key.defValue);
        } else if (key.defValue instanceof Integer) {
            value = data.getInt(key.name, (int) key.defValue);
        } else if (key.defValue instanceof Boolean) {
            value = data.getBoolean(key.name, (Boolean) key.defValue);
        } else if (key.defValue instanceof Long) {
            value = data.getLong(key.name, (Long) key.defValue);
        } else {
            value = data.getFloat(key.name, ((Number) key.defValue).floatValue());
        }
        return (T) value;
    }

    //Need synchronized why SharedPreferences don't support aysnc write
    private static synchronized <T extends Serializable> void set(Key<T> key, T value) {
        if (value instanceof String) {
            data.edit().putString(key.name, (String) value).apply();
        } else if (value instanceof Integer) {
            data.edit().putInt(key.name, (Integer) value).apply();
        } else if (value instanceof Boolean) {
            data.edit().putBoolean(key.name, (Boolean) value).apply();
        } else if (value instanceof Long) {
            data.edit().putLong(key.name, (Long) value).apply();
        } else if (value instanceof Float) {
            data.edit().putFloat(key.name, (Float) value).apply();
        } else {
            throw new IllegalArgumentException("Invalid global config value instance");
        }
    }

    private static class Key<T extends Serializable> {
        private final String name;
        private final T defValue;

        private Key(String name, T defValue) {
            this.name = name;
            this.defValue = defValue;
        }
    }
}