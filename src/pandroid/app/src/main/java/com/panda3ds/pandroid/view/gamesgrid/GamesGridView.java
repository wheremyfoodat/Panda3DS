package com.panda3ds.pandroid.view.gamesgrid;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.view.recycler.AutoFitGridLayout;

import java.util.List;

public class GamesGridView extends RecyclerView {
    private final GameAdapter adapter;

    public GamesGridView(@NonNull Context context) {
        this(context, null);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutManager(new AutoFitGridLayout(getContext(), 170));
        setAdapter(adapter = new GameAdapter());
    }

    public void setGameList(List<GameMetadata> games) {
        adapter.replace(games);
    }
}