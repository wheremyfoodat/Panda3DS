package com.panda3ds.pandroid.view.ds;

import android.graphics.Rect;
import android.view.Gravity;

import com.panda3ds.pandroid.math.Shape;
import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.view.renderer.layout.ConsoleLayout;

class DsLayout implements ConsoleLayout {
    private final Rect topDisplay = new Rect();
    private final Rect bottomDisplay = new Rect();

    private final Vector2 screenSize = new Vector2(0,0);
    private final Vector2 sourceTop = new Vector2(0,0);
    private final Vector2 sourceBottom = new Vector2(0,0);
    private final Model[] modes = new Model[2];

    public DsLayout(Model landscape, Model portrait){
        modes[0] = landscape;
        modes[1] = portrait;
    }

    public DsLayout(){
        this(new Model(), new Model());
    }

    @Override
    public void update(int screenWidth, int screenHeight) {
        screenSize.set(screenWidth, screenHeight);
        update();
    }

    @Override
    public void setBottomDisplaySourceSize(int width, int height) {
        sourceBottom.set(width, height);
        update();
    }

    @Override
    public void setTopDisplaySourceSize(int width, int height) {
        sourceTop.set(width, height);
        update();
    }

    @Override
    public Rect getBottomDisplayBounds() {
        return bottomDisplay;
    }

    @Override
    public Rect getTopDisplayBounds() {
        return topDisplay;
    }

    public void update(){
        Model data = getCurrentModel();
        Mode mode = data.mode;
        switch (mode){
            case RELATIVE:
                relative(data);
                break;
            case SINGLE:
                single(data);
                break;
            case ABSOLUTE:
                absolute(data);
                break;
        }
    }

    private void absolute(Model data) {
        Shape top = data.preferredTop;
        Shape bottom = data.preferredBottom;

        top.normalize();
        bottom.normalize();

        topDisplay.set(top.x, top.y, top.x +top.width, top.y + top.height);
        bottomDisplay.set(bottom.x, bottom.y, bottom.x +bottom.width, bottom.y + bottom.height);
    }

    /**
     * SINGLE LAYOUT:
     * SHOW ONLY SCREEN IN FIT MODE
     */
    private void single(Model data) {
        Vector2 source = data.singleTop ? sourceTop : sourceBottom;
        Rect dest = data.singleTop ? topDisplay : bottomDisplay;

        if (data.lockAspect) {
            int x = 0, y = 0;
            int width = (int) ((screenSize.y / source.y) * source.x);
            int height;

            if (width > screenSize.x) {
                height = (int) ((screenSize.x / source.x) * source.y);
                width = (int) screenSize.x;
                y = (int) ((screenSize.y - height) / 2);
            } else {
                height = (int) screenSize.y;
                x = (int) ((screenSize.x - width) / 2);
            }
            dest.set(x,y,x+width,y+height);
        } else {
            dest.set(0,0, (int) screenSize.x, (int) screenSize.y);
        }
        (data.singleTop ? bottomDisplay : topDisplay).set(0,0,0,0);
    }


    /***
     * RELATIVE LAYOUT:
     * ORGANIZE SCREEN IN POSITION BASED IN GRAVITY
     * AND SPACE, THE SPACE DETERMINE LANDSCAPE TOP SCREEN SIZE
     */
    private void relative(Model data) {
        int screenWidth = (int) screenSize.x;
        int screenHeight = (int) screenSize.y;

        Vector2 topSourceSize = this.sourceTop;
        Vector2 bottomSourceSize = this.sourceBottom;

        Rect topDisplay = this.topDisplay;
        Rect bottomDisplay = this.bottomDisplay;

        if (data.reverse){
            topSourceSize = this.sourceBottom;
            bottomSourceSize = this.sourceTop;

            topDisplay = this.bottomDisplay;
            bottomDisplay = this.topDisplay;
        }

        if (screenWidth > screenHeight) {
            int topDisplayWidth = (int) ((screenHeight / topSourceSize.y) * topSourceSize.x);
            int topDisplayHeight = screenHeight;

            if (topDisplayWidth > (screenWidth * data.space)) {
                topDisplayWidth = (int) (screenWidth * data.space);
                topDisplayHeight = (int) ((topDisplayWidth / topSourceSize.x) * topSourceSize.y);
            }

            int bottomDisplayHeight = (int) (((screenWidth - topDisplayWidth) / bottomSourceSize.x) * bottomSourceSize.y);

            topDisplay.set(0, 0, topDisplayWidth, topDisplayHeight);
            bottomDisplay.set(topDisplayWidth, 0, topDisplayWidth + (screenWidth - topDisplayWidth), bottomDisplayHeight);

            switch (data.gravity){
                case Gravity.CENTER:{
                    bottomDisplay.offset(0, (screenHeight-bottomDisplay.height())/2);
                    topDisplay.offset(0, (screenHeight-topDisplay.height())/2);
                }break;
                case Gravity.BOTTOM:{
                    bottomDisplay.offset(0, (screenHeight-bottomDisplay.height()));
                    topDisplay.offset(0, (screenHeight-topDisplay.height()));
                }break;
            }

        } else {
            int topScreenHeight = (int) ((screenWidth / topSourceSize.x) * topSourceSize.y);
            topDisplay.set(0, 0, screenWidth, topScreenHeight);

            int bottomDisplayHeight = (int) ((screenWidth / bottomSourceSize.x) * bottomSourceSize.y);
            int bottomDisplayWidth = screenWidth;
            int bottomDisplayX = 0;

            if (topScreenHeight + bottomDisplayHeight > screenHeight) {
                bottomDisplayHeight = (screenHeight - topScreenHeight);
                bottomDisplayWidth = (int) ((bottomDisplayHeight / bottomSourceSize.y) * bottomSourceSize.x);
                bottomDisplayX = (screenWidth - bottomDisplayX) / 2;
            }

            topDisplay.set(0, 0, screenWidth, topScreenHeight);
            bottomDisplay.set(bottomDisplayX, topScreenHeight, bottomDisplayX + bottomDisplayWidth, topScreenHeight + bottomDisplayHeight);
        }
    }

    public Model getCurrentModel() {
        return screenSize.x > screenSize.y ? modes[0] : modes[1];
    }

}
