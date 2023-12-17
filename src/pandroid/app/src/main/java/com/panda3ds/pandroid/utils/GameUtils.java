package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;

import com.google.gson.Gson;
import com.panda3ds.pandroid.app.GameActivity;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.data.game.GameMetadata;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Objects;

public class GameUtils {
    private static final String KEY_GAME_LIST = "gameList";
    private static final ArrayList<GameMetadata> games = new ArrayList<>();
    private static SharedPreferences data;
    private static final Gson gson = new Gson();

    public static void initialize() {
        data = PandroidApplication.getAppContext().getSharedPreferences(Constants.PREF_GAME_UTILS, Context.MODE_PRIVATE);

        GameMetadata[] list = gson.fromJson(data.getString(KEY_GAME_LIST, "[]"), GameMetadata[].class);
        games.clear();
        games.addAll(Arrays.asList(list));
    }

    public static GameMetadata findByRomPath(String romPath) {
        for (GameMetadata game : games) {
            if (Objects.equals(romPath, game.getRomPath())) {
                return game;
            }
        }
        return null;
    }

    public static void launch(Context context, GameMetadata game) {
        String path = PathUtils.getPath(Uri.parse(game.getRomPath()));
        context.startActivity(new Intent(context, GameActivity.class).putExtra(Constants.ACTIVITY_PARAMETER_PATH, path));
    }

    public static void removeGame(GameMetadata game) {
        games.remove(game);
        saveAll();
    }

    public static void addGame(GameMetadata game) {
        games.add(game);
        saveAll();
    }

    public static ArrayList<GameMetadata> getGames() {
        return new ArrayList<>(games);
    }

    private static synchronized void saveAll() {
        data.edit()
                .putString(KEY_GAME_LIST, gson.toJson(games.toArray(new GameMetadata[0])))
                .apply();
    }
}
