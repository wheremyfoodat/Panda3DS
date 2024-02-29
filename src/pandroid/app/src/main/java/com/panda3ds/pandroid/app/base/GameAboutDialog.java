package com.panda3ds.pandroid.app.base;

import android.content.Context;
import android.os.Bundle;
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

public class GameAboutDialog extends BaseSheetDialog {
    private final GameMetadata game;
    public GameAboutDialog(@NonNull Context context, GameMetadata game) {
        super(context);
        this.game = game;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.dialog_game_about);

        ((GameIconView) findViewById(R.id.game_icon)).setImageBitmap(game.getIcon());
        ((TextView) findViewById(R.id.game_title)).setText(game.getTitle());
        ((TextView) findViewById(R.id.game_publisher)).setText(game.getPublisher());
        ((TextView) findViewById(R.id.region)).setText(game.getRegions()[0].localizedName());
        ((TextView) findViewById(R.id.directory)).setText(FileUtils.obtainUri(game.getRealPath()).getPath());

        findViewById(R.id.play).setOnClickListener(v -> {
            dismiss();
            GameUtils.launch(getContext(), game);
        });

        if (game.getRomPath().startsWith("folder:")) {
            findViewById(R.id.remove).setVisibility(View.GONE);
        } else {
            findViewById(R.id.remove).setOnClickListener(v -> {
                dismiss();
                if (game.getRomPath().startsWith("elf:")) {
                    FileUtils.delete(game.getRealPath());
                }
                GameUtils.removeGame(game);
            });
        }
    }
}
