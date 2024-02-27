package com.panda3ds.pandroid.app.base;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.view.gamesgrid.GameIconView;

public class GameAboutDialog extends BottomSheetDialog {
    public GameAboutDialog(@NonNull Context context, GameMetadata game) {
        super(context);
        View content = LayoutInflater.from(context).inflate(R.layout.dialog_game_about, null, false);
        setContentView(content);

        ((GameIconView)content.findViewById(R.id.game_icon)).setImageBitmap(game.getIcon());
        ((TextView)content.findViewById(R.id.game_title)).setText(game.getTitle());
        ((TextView)content.findViewById(R.id.game_publisher)).setText(game.getPublisher());
        ((TextView)content.findViewById(R.id.region)).setText(game.getRegions()[0].name());
        ((TextView)content.findViewById(R.id.directory)).setText(FileUtils.obtainUri(game.getRealPath()).getPath());
        
        content.findViewById(R.id.play).setOnClickListener(v -> {
            dismiss();
            GameUtils.launch(getContext(), game);
        });

        if (game.getRomPath().startsWith("folder:")){
            content.findViewById(R.id.remove).setVisibility(View.GONE);
        } else {
            content.findViewById(R.id.remove).setOnClickListener(v-> {
                dismiss();
                GameUtils.removeGame(game);
            });
        }
    }
}
