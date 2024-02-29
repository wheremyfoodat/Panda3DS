package com.panda3ds.pandroid.app.base;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.app.game.GameLauncher;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.CompatUtils;
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
        findViewById(R.id.shortcut).setOnClickListener(v -> {
            dismiss();
            makeShortcut();
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

    private void makeShortcut() {
        Context context = CompatUtils.findActivity(getContext());
        ShortcutInfoCompat.Builder shortcut = new ShortcutInfoCompat.Builder(context, game.getId());
        if (game.getIcon() != null){
            shortcut.setIcon(IconCompat.createWithAdaptiveBitmap(game.getIcon()));
        } else {
            shortcut.setIcon(IconCompat.createWithResource(getContext(), R.mipmap.ic_launcher));
        }
        shortcut.setActivity(new ComponentName(context, GameLauncher.class));
        shortcut.setLongLabel(game.getTitle());
        shortcut.setShortLabel(game.getTitle());
        Intent intent = new Intent(PandroidApplication.getAppContext(), GameLauncher.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(new Uri.Builder().scheme("pandroid-game").authority(game.getId()).build());
        shortcut.setIntent(intent);
        ShortcutManagerCompat.requestPinShortcut(context,shortcut.build(),null);
    }
}
