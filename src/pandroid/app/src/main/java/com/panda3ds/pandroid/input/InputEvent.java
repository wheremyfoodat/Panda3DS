package com.panda3ds.pandroid.input;

public class InputEvent {
    private final String name;
    private final float value;

    public InputEvent(String name, float value) {
        this.name = name;
        this.value = Math.max(0.0f, Math.min(1.0f, value));
    }

    public boolean isDown() {
        return value > 0.0f;
    }

    public String getName() {
        return name;
    }

    public float getValue() {
        return value;
    }
}
