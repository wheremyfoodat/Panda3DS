package com.panda3ds.pandroid.data.config;

import com.panda3ds.pandroid.data.GsonConfigParser;
import com.panda3ds.pandroid.utils.Constants;

import java.io.Serializable;
import java.util.HashMap;

public class GlobalConfig {

    private static final GsonConfigParser parser = new GsonConfigParser(Constants.PREF_GLOBAL_CONFIG);

    public static final int VALUE_THEME_ANDROID = 0;
    public static final int VALUE_THEME_LIGHT = 1;
    public static final int VALUE_THEME_DARK = 2;
    public static final int VALUE_THEME_BLACK = 3;

    public static DataModel data;

    public static final Key<Integer> KEY_APP_THEME = new Key<>("app.theme", VALUE_THEME_ANDROID);

    public static void initialize() {
        data = parser.load(DataModel.class);
    }

    public static <T extends Serializable> T get(Key<T> key) {
        Serializable value;

        if (!data.configs.containsKey(key.name)){
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

    private static void writeChanges(){
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
        private final HashMap<String, Object> configs = new HashMap<>();

        public Object get(String key){
            return configs.get(key);
        }
    }
}
