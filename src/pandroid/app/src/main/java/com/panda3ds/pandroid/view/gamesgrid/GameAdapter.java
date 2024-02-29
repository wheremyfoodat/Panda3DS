package com.panda3ds.pandroid.view.gamesgrid;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.lang.Function;

import java.util.ArrayList;
import java.util.List;

class GameAdapter extends RecyclerView.Adapter<ItemHolder> {
    private final ArrayList<GameMetadata> games = new ArrayList<>();
    private final Function<GameMetadata> clickListener;
    private final Function<GameMetadata> longClickListener;

    GameAdapter(Function<GameMetadata> clickListener, Function<GameMetadata> longClickListener) {
        this.clickListener = clickListener;
        this.longClickListener = longClickListener;
    }

    @NonNull
    @Override
    public ItemHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        return new ItemHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.holder_game, parent, false));
    }

    @Override
    public void onBindViewHolder(@NonNull ItemHolder holder, int position) {
        holder.itemView.setOnClickListener(v -> clickListener.run(games.get(position)));
        holder.itemView.setOnLongClickListener(v -> {
            longClickListener.run(games.get(position));
            return false;
        });
        holder.apply(games.get(position));
    }

    public void replace(List<GameMetadata> games) {
        int oldCount = getItemCount();
        this.games.clear();
        notifyItemRangeRemoved(0, oldCount);
        this.games.addAll(games);
        notifyItemRangeInserted(0, getItemCount());
    }

    @Override
    public int getItemCount() {
        return games.size();
    }

}
