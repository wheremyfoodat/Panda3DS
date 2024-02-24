package com.panda3ds.pandroid.input;

import com.panda3ds.pandroid.utils.Constants;

public enum KeyName {
    A(Constants.INPUT_KEY_A),
    B(Constants.INPUT_KEY_B),
    X(Constants.INPUT_KEY_X),
    Y(Constants.INPUT_KEY_Y),
    UP(Constants.INPUT_KEY_UP),
    DOWN(Constants.INPUT_KEY_DOWN),
    LEFT(Constants.INPUT_KEY_LEFT),
    RIGHT(Constants.INPUT_KEY_RIGHT),
    AXIS_LEFT,
    AXIS_RIGHT,
    AXIS_UP,
    AXIS_DOWN,
    START(Constants.INPUT_KEY_START),
    SELECT(Constants.INPUT_KEY_SELECT),
    L(Constants.INPUT_KEY_L),
    R(Constants.INPUT_KEY_R),
    NULL,
    CHANGE_DS_LAYOUT;

    private final int keyId;

    KeyName() {
        this(-1);
    }

    KeyName(int keyId) {
        this.keyId = keyId;
    }

    public int getKeyId() {
        return keyId;
    }

}