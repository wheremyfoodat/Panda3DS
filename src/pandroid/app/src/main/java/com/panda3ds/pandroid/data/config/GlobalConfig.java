package com.panda3ds.pandroid.data.config;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.google.gson.internal.LinkedTreeMap;
import com.panda3ds.pandroid.data.GsonConfigParser;
import com.panda3ds.pandroid.utils.Constants;

import java.io.Serializable;
import java.util.Map;

public class GlobalConfig {

    private static final GsonConfigParser parser = new GsonConfigParser(Constants.PREF_GLOBAL_CONFIG);
    private static final Gson gson = new Gson();

    public static final int THEME_ANDROID = 0;
    public static final int THEME_LIGHT = 1;
    public static final int THEME_DARK = 2;
    public static final int THEME_BLACK = 3;

    public static DataModel data;

    public static final Key<Boolean> KEY_SHADER_JIT = new Key<>("emu.shader_jit", false);
    public static final Key<Boolean> KEY_SHOW_PERFORMANCE_OVERLAY = new Key<>("dev.performanceOverlay", false);
    public static final Key<Boolean> KEY_LOGGER_SERVICE = new Key<>("dev.loggerService", false);
    public static final Key<Integer> KEY_APP_THEME = new Key<>("app.theme", THEME_ANDROID);
    public static final Key<Boolean> KEY_SCREEN_GAMEPAD_VISIBLE = new Key<>("app.screen_gamepad.visible", true);
    public static final Key<String> KEY_DS_LAYOUTS = new Key<>("app.ds.layouts", "");

    public static void initialize() {
        data = parser.load(DataModel.class);
    }

    public static <T extends Serializable> T get(Key<T> key) {
        Serializable value;

        if (!data.configs.containsKey(key.name)) {
            return key.defaultValue;
        }

        if (key.defaultValue instanceof String) {
            value = (String) data.configs.get(key.name);
        } else if (key.defaultValue instanceof Integer) {
            value = ((Number) data.get(key.name)).intValue();
        } else if (key.defaultValue instanceof Boolean) {
            value = (boolean) data.get(key.name);
        } else if (key.defaultValue instanceof Long) {
            value = ((Number) data.get(key.name)).longValue();
        } else {
            value = ((Number) data.get(key.name)).floatValue();
        }
        return (T) value;
    }

    public static synchronized <T extends Serializable> void set(Key<T> key, T value) {
        data.configs.put(key.name, value);
        writeChanges();
    }

    public static <T extends Object> T getExtra(Key<String> key, Class<T> dataClass){
        if (data.extras.has(key.name)){
            return gson.fromJson(data.extras.getAsJsonObject(key.name), dataClass);
        }
        return gson.fromJson("{}", dataClass);
    }

    public static synchronized void putExtra(Key<String> key, Object value){
        if (data.extras.has(key.name)){
            data.extras.remove(key.name);
        }
        data.extras.add(key.name, gson.toJsonTree(value));
        writeChanges();
    }

    private static void writeChanges() {
        parser.save(data);
    }

    private static class Key<T extends Serializable> {
        private final String name;
        private final T defaultValue;

        private Key(String name, T defaultValue) {
            this.name = name;
            this.defaultValue = defaultValue;
        }
    }

    private static class DataModel {
        private final Map<String, Object> configs = new LinkedTreeMap<>();
        private final JsonObject extras = new JsonObject();

        public Object get(String key) {
            return configs.get(key);
        }
    }
}
