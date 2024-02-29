package com.panda3ds.pandroid.view.gamesgrid;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.lang.Function;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.view.recycler.AutoFitGridLayout;

import java.util.List;

public class GamesGridView extends RecyclerView {
    private final GameAdapter adapter;
    private Function<GameMetadata> longClickListener = null;

    public GamesGridView(@NonNull Context context) {
        this(context, null);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public GamesGridView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        setLayoutManager(new AutoFitGridLayout(getContext(), 170));
        setAdapter(adapter = new GameAdapter(this::onClickGame, this::onLongClickGame));
    }

    public void setItemLongClick(Function<GameMetadata> longClickListener) {
        this.longClickListener = longClickListener;
    }

    private void onClickGame(GameMetadata game) {
        GameUtils.launch(getContext(), game);
    }

    private void onLongClickGame(GameMetadata game) {
        if (longClickListener != null) {
            longClickListener.run(game);
        }
    }

    public void setGameList(List<GameMetadata> games) {
        adapter.replace(games);
    }
}