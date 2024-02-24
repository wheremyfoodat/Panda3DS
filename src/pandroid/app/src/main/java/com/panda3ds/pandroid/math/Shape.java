package com.panda3ds.pandroid.math;

public class Shape {
    public int x = 0, y = 0, width = 1, height = 1;

    public Shape() {
    }

    public Shape(int x, int y, int width, int height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    }

    public void move(int x, int y) {
        this.x += x;
        this.y += y;
    }

    public void normalize() {
        this.x = Math.max(x, 0);
        this.y = Math.max(y, 0);
        this.width = Math.max(width, 1);
        this.height = Math.max(height, 1);
    }

    public void maxSize(int w, int h) {
        this.width = Math.max(w, this.width);
        this.height = Math.max(h, this.height);
    }
}
