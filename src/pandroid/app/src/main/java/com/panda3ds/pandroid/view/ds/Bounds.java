package com.panda3ds.pandroid.view.ds;

import android.graphics.Rect;

class Bounds {
    public int left = 0;
    public int right = 0;
    public int top = 0;
    public int bottom = 0;

    public void normalize() {
        left = Math.abs(left);
        right = Math.abs(right);
        top = Math.abs(top);
        bottom = Math.abs(bottom);
    }

    public void applyWithAspect(Rect rect, int width, double aspectRatio) {
        normalize();
        rect.set(left, top, width-right, (int) Math.round((width-right-left)*aspectRatio)+top);
    }

    public void apply(Rect rect, int width, int height) {
        normalize();
        rect.set(left, top, width-right, height-bottom);
    }

    public void move(int x, int y) {
        left += x;
        right -= x;

        top += y;
        bottom -= y;
        normalize();
    }

    public void fixOverlay(int width, int height, int size) {
        if (left > (width - right) - size) {
            right = (width - left) - size;
        }
        
        if (top > (height - bottom) - size) {
            bottom = (height - top) - size;
        }

        normalize();
    }
}