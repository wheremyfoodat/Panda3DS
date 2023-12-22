package com.panda3ds.pandroid.input;

import com.panda3ds.pandroid.data.GsonConfigParser;
import com.panda3ds.pandroid.utils.Constants;

public class InputMap {
    public static final GsonConfigParser parser = new GsonConfigParser(Constants.PREF_INPUT_MAP);
    private static DataModel data;

    public static void initialize() {
        data = parser.load(DataModel.class);
    }

    public static float getDeadZone() {
        return data.deadZone;
    }

    public static void set(KeyName key, String name) {
        data.keys[key.ordinal()] = name;
        writeConfig();
    }

    public static String relative(KeyName key) {
        return data.keys[key.ordinal()] == null ? "-" : data.keys[key.ordinal()];
    }

    public static KeyName relative(String name) {
        for (KeyName key : KeyName.values()) {
            if (relative(key).equalsIgnoreCase(name))
                return key;
        }
        return KeyName.NULL;
    }

    public static void setDeadZone(float value) {
        data.deadZone = Math.max(0.0f, Math.min(1.0f, value));
        writeConfig();
    }

    private static void writeConfig() {
        parser.save(data);
    }

    private static class DataModel {
        public float deadZone = 0.2f;
        public final String[] keys = new String[32];
    }
}
