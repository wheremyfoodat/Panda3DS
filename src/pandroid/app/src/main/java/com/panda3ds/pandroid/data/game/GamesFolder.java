package com.panda3ds.pandroid.data.game;

import android.net.Uri;
import android.util.Log;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.utils.FileUtils;

import java.util.Collection;
import java.util.HashMap;
import java.util.UUID;

public class GamesFolder {
    private final String id = UUID.randomUUID().toString();
    private final String path;
    private final HashMap<String, GameMetadata> games = new HashMap<>();

    public GamesFolder(String path) {
        this.path = path;
    }

    public boolean isValid() {
        return FileUtils.exists(path);
    }

    public String getId() {
        return id;
    }

    public String getPath() {
        return path;
    }

    public Collection<GameMetadata> getGames() {
        return games.values();
    }

    public void refresh() {
        String[] gamesId = games.keySet().toArray(new String[0]);
        for (String file: gamesId) {
            if (!FileUtils.exists(path + "/" + file)) {
                games.remove(file);
            }
        }
        
        String unknown = PandroidApplication.getAppContext().getString(R.string.unknown);

        for (String file: FileUtils.listFiles(path)) {
            String path = FileUtils.getChild(this.path, file);
            if (FileUtils.isDirectory(path) || games.containsKey(file)) {
                continue;
            }

            String ext = FileUtils.extension(path);
            if (ext.equals("3ds") || ext.equals("3dsx") || ext.equals("cci") || ext.equals("cxi") || ext.equals("app") || ext.equals("ncch")) {
                String name = FileUtils.getName(path).trim().split("\\.")[0];
                games.put(file, new GameMetadata(new Uri.Builder().path(file).authority(id).scheme("folder").build().toString(), name, unknown));
            }
        }
    }
}
