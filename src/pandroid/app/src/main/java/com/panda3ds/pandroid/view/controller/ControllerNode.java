package com.panda3ds.pandroid.view.controller;


import android.view.View;

import androidx.annotation.NonNull;

import com.panda3ds.pandroid.math.Vector2;

public interface ControllerNode {

    @NonNull
    default Vector2 getPosition(){
        View me = (View) this;

        int[] position = new int[2];
        me.getLocationInWindow(position);
        return new Vector2(position[0], position[1]);
    }

    @NonNull Vector2 getSize();

    void onTouch(TouchEvent event);
}
