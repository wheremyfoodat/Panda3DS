package com.panda3ds.pandroid.view.recycler;

import android.content.Context;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public final class AutoFitGridLayout extends GridLayoutManager {
    private final int iconSize;
    private final Context context;

    public AutoFitGridLayout(Context context, int iconSize) {
        super(context, 1);
        this.iconSize = iconSize;
        this.context = context;
    }

    @Override
    public void onMeasure(@NonNull RecyclerView.Recycler recycler, @NonNull RecyclerView.State state, int widthSpec, int heightSpec) {
        super.onMeasure(recycler, state, widthSpec, heightSpec);
        int width = View.MeasureSpec.getSize(widthSpec);
        int iconSize = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, this.iconSize, context.getResources().getDisplayMetrics());
        int iconCount = Math.max(1, width / iconSize);
        if (getSpanCount() != iconCount)
            setSpanCount(iconCount);
    }
}