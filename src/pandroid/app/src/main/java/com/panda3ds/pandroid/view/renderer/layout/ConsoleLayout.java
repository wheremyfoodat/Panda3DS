package com.panda3ds.pandroid.view.renderer.layout;

import android.graphics.Rect;

import com.panda3ds.pandroid.math.Vector2;

public interface ConsoleLayout {
    void update(int screenWidth, int screenHeight);

    void setBottomDisplaySourceSize(int width, int height);
    void setTopDisplaySourceSize(int width, int height);

    Rect getBottomDisplayBounds();
    Rect getTopDisplayBounds();
}
