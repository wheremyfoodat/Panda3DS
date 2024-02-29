package com.panda3ds.pandroid.utils;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.util.TypedValue;

import androidx.annotation.AttrRes;

import com.panda3ds.pandroid.app.PandroidApplication;

public class CompatUtils {
    public static Activity findActivity(Context context) {
        if (context instanceof Activity) {
            return (Activity) context;
        } else if ((context instanceof ContextWrapper)) {
            return findActivity(((ContextWrapper) context).getBaseContext());
        }

        return ((Activity) context);
    }

    public static int resolveColor(Context context, @AttrRes int id) {
        try {
            TypedArray values = context.obtainStyledAttributes(new int[]{id});
            int color = values.getColor(0, Color.RED);
            values.recycle();
            return color;
        } catch (Exception e) {
            return Color.rgb(255,0,255);
        }
    }

    public static float applyDimensions(int unit, int size) {
        return TypedValue.applyDimension(unit, size, PandroidApplication.getAppContext().getResources().getDisplayMetrics());
    }
}
