package com.panda3ds.pandroid.view.gamesgrid;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.GameUtils;

class ItemHolder extends RecyclerView.ViewHolder {
    public ItemHolder(@NonNull View itemView) {
        super(itemView);
    }

    public void apply(GameMetadata game) {
        ((AppCompatTextView) itemView.findViewById(R.id.title))
                .setText(game.getTitle());
        ((AppCompatTextView) itemView.findViewById(R.id.description))
                .setText(game.getPublisher());

        itemView.setOnClickListener((v) -> {
            GameUtils.launch(v.getContext(), game);
        });
    }
}