package com.panda3ds.pandroid.app.base;

import android.content.Context;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.CompatUtils;

import org.jetbrains.annotations.NotNull;

public class BaseSheetDialog extends BottomSheetDialog {
    private final LinearLayout contentView;
    public BaseSheetDialog(@NonNull Context context) {
        super(CompatUtils.findActivity(context));

        int width = CompatUtils.findActivity(context).getWindow().getDecorView().getMeasuredWidth();
        int height = CompatUtils.findActivity(context).getWindow().getDecorView().getMeasuredHeight();
        float heightScale = 0.87f; // What percentage of the screen's height to use up

        getBehavior().setPeekHeight((int) (height * heightScale));
        getBehavior().setMaxHeight((int) (height * heightScale));
        getBehavior().setMaxWidth(width);
        
        super.setContentView(R.layout.dialog_bottom_sheet);
        contentView = super.findViewById(R.id.content);
    }

    @Override
    public void setContentView(View view) {
        contentView.removeAllViews();
        contentView.addView(view);
    }

    @Override
    public void setContentView(int layoutResId) {
        setContentView(LayoutInflater.from(getContext()).inflate(layoutResId, null, false));
    }

    @NotNull
    @Override
    public <T extends View> T findViewById(int id) {
        return contentView.findViewById(id);
    }
}
