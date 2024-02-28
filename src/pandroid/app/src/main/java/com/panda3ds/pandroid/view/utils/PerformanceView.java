package com.panda3ds.pandroid.view.utils;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.text.Html;
import android.util.AttributeSet;
import android.util.TypedValue;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.utils.PerformanceMonitor;

public class PerformanceView extends AppCompatTextView {
    private boolean running = false;

    public PerformanceView(@NonNull Context context) {
        this(context, null);
    }

    public PerformanceView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs,0);
    }

    public PerformanceView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        int padding = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 10, getResources().getDisplayMetrics());
        setPadding(padding,padding,padding,padding);
        setTextColor(Color.WHITE);
        setShadowLayer(padding,0,0,Color.BLACK);
    }

    public void refresh() {
        running = isShown();
        if (!running) {
            return;
        }

        String debug = "";

        // Calculate total memory in MB and the current memory usage
        int memoryTotalMb = (int) Math.round(PerformanceMonitor.getTotalMemory() / (1024.0 * 1024.0));
        int memoryUsageMb = (int) Math.round(PerformanceMonitor.getUsedMemory() / (1024.0 * 1024.0));

        debug += "<b>FPS: </b>" + PerformanceMonitor.getFps() + "<br>";
        debug += "<b>RAM: </b>" + Math.round(((float) memoryUsageMb / memoryTotalMb) * 100) + "%  (" + memoryUsageMb + "MB/" + memoryTotalMb + "MB)<br>";
        debug += "<b>BACKEND: </b>" + PerformanceMonitor.getBackend() + (GlobalConfig.get(GlobalConfig.KEY_SHADER_JIT) ? " + JIT" : "") + "<br>";
        setText(Html.fromHtml(debug, Html.FROM_HTML_MODE_COMPACT));
        postDelayed(this::refresh, 250);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        if (!running) {
            refresh();
        }
    }
}
