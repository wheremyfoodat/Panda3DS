package com.panda3ds.pandroid.view.controller.mapping;

import android.view.Gravity;

import androidx.annotation.NonNull;

public class Location {
    private float x = 0.0f;
    private float y = 0.0f;
    private int gravity = Gravity.LEFT;
    private boolean visible = false;

    public Location() {}

    public Location(float x, float y, int gravity, boolean visible) {
        this.x = x;
        this.y = y;
        this.gravity = gravity;
        this.visible = visible;
    }

    public int getGravity() {
        return gravity;
    }

    public float getX() {
        return x;
    }

    public float getY() {
        return y;
    }

    public void setVisible(boolean visible) {
        this.visible = visible;
    }

    public boolean isVisible() {
        return visible;
    }

    public void setGravity(int gravity) {
        this.gravity = gravity;
    }

    public void setPosition(float x, float y) {
        this.x = x;
        this.y = y;
    }

    @NonNull
    @Override
    public Location clone() {
        return new Location(x, y, gravity, visible);
    }

}