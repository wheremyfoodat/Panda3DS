package com.panda3ds.pandroid.view.controller;

public class TouchEvent {
    public static final int ACTION_DOWN = 0;
    public static final int ACTION_MOVE = 1;
    public static final int ACTION_UP   = 2;

    private final int action;
    private final float x,y;

    public float getX() {
        return x;
    }

    public float getY() {
        return y;
    }

    public int getAction() {
        return action;
    }

    public TouchEvent(float x, float y, int action){
        this.x = x;
        this.y = y;
        this.action = action;
    }
}
