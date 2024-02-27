package com.panda3ds.pandroid.view.gamesgrid;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.RecyclerView;
import android.widget.TextView;
import com.google.android.material.imageview.ShapeableImageView;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.bottomsheet.BottomSheetDialog;
    
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
        ((GameIconView) itemView.findViewById(R.id.icon))
                .setImageBitmap(game.getIcon());
        ((AppCompatTextView) itemView.findViewById(R.id.description))
                .setText(game.getPublisher());

        itemView.setOnLongClickListener((v) -> {
            showBottomSheet(game);
            return true; // Return true to consume the long click event
        });

        itemView.setOnClickListener((v) -> {
            GameUtils.launch(v.getContext(), game);
        });
    }
    private void showBottomSheet(GameMetadata game) {
        BottomSheetDialog bottomSheetDialog = new BottomSheetDialog(itemView.getContext());
        View bottomSheetView = View.inflate(itemView.getContext(), R.layout.game_dialog, null);
        bottomSheetDialog.setContentView(bottomSheetView);

        TextView gameTitleTextView = bottomSheetView.findViewById(R.id.game_title);
        gameTitleTextView.setText(game.getTitle());

        ShapeableImageView gameIconImageView = bottomSheetView.findViewById(R.id.game_icon);
        gameIconImageView.setImageBitmap(game.getIcon());

        TextView gamePublisherTextView = bottomSheetView.findViewById(R.id.game_author);
        gamePublisherTextView.setText(game.getPublisher());

        MaterialButton gamePlayButton = bottomSheetView.findViewById(R.id.game_play);
        gamePlayButton.setOnClickListener(new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            GameUtils.launch(v.getContext(), game);
        }
    });

        bottomSheetDialog.show();
    }
}
