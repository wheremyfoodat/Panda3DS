package com.panda3ds.pandroid.view.controller.nodes;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import com.panda3ds.pandroid.view.controller.ControllerNode;

public abstract class BasicControllerNode extends AppCompatTextView implements ControllerNode {
    public BasicControllerNode(@NonNull Context context) {
        super(context);
    }

    public BasicControllerNode(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public BasicControllerNode(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }
}
