package com.panda3ds.pandroid.view.controller.nodes;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.Gravity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.view.controller.ControllerNode;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.controller.listeners.ButtonStateListener;

public class Button extends BasicControllerNode {
    private boolean pressed = false;
    private int width,height;

    private ButtonStateListener stateListener;

    public Button(@NonNull Context context) {
        super(context);
        init();
    }

    public Button(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public Button(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        width = getWidth();
        height = getHeight();
    }

    private void init(){
        setTextAlignment(TEXT_ALIGNMENT_CENTER);
        setGravity(Gravity.CENTER);
    }

    public void setStateListener(ButtonStateListener stateListener) {
        this.stateListener = stateListener;
    }

    public boolean isPressed() {
        return pressed;
    }

    @NonNull
    @Override
    public Vector2 getSize() {
        return new Vector2(width,height);
    }

    @Override
    public void onTouch(TouchEvent event) {
        pressed = event.getAction() != TouchEvent.ACTION_UP;
        setAlpha(pressed ? 0.2F : 1.0F);
        if (stateListener != null){
            stateListener.onButtonPressedChange(this, pressed);
        }
    }
}
