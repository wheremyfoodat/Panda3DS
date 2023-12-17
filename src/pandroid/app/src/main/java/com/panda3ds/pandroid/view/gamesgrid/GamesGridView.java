package com.panda3ds.pandroid.view.gamesgrid;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.data.game.GameMetadata;

import java.util.List;

public class GamesGridView extends RecyclerView {
    private int iconSize = 170;
    private final GameAdapter adapter;

    public GamesGridView(@NonNull Context context) {
        this(context, null);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutManager(new AutoFitLayout());
        setAdapter(adapter = new GameAdapter());
    }

    public void setGameList(List<GameMetadata> games) {
        adapter.replace(games);
    }

    public void setIconSize(int iconSize) {
        this.iconSize = iconSize;
        requestLayout();
        measure(MeasureSpec.EXACTLY, MeasureSpec.EXACTLY);
    }

    private final class AutoFitLayout extends GridLayoutManager {
        public AutoFitLayout() {
            super(GamesGridView.this.getContext(), 1);
        }

        @Override
        public void onMeasure(@NonNull Recycler recycler, @NonNull State state, int widthSpec, int heightSpec) {
            super.onMeasure(recycler, state, widthSpec, heightSpec);
            int width = getMeasuredWidth();
            int iconSize = (int) (GamesGridView.this.iconSize * getResources().getDisplayMetrics().density);
            int iconCount = Math.max(1, width / iconSize);
            if (getSpanCount() != iconCount)
                setSpanCount(iconCount);
        }
    }
}