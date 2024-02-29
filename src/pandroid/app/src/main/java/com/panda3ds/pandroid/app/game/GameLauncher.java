package com.panda3ds.pandroid.app.game;

import android.net.Uri;
import android.os.Bundle;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.core.content.pm.ShortcutManagerCompat;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.GameUtils;

import java.util.Arrays;

public class GameLauncher extends BaseActivity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(new TextView(this));
        Uri uri = getIntent().getData();
        if (uri != null && uri.getScheme().equals("pandroid-game")) {
            String gameId = uri.getAuthority();
            GameMetadata game = GameUtils.findGameById(gameId);

            if (game != null) {
                GameUtils.launch(this, game);
            } else {
                Toast.makeText(this, R.string.invalid_game, Toast.LENGTH_LONG).show();
                ShortcutManagerCompat.removeDynamicShortcuts(this, Arrays.asList(gameId));
                ShortcutManagerCompat.removeLongLivedShortcuts(this, Arrays.asList(gameId));
            }
        }
        
        finish();
    }
}
