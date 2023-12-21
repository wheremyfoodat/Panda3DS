package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.util.Log;

import com.google.gson.Gson;
import com.panda3ds.pandroid.app.GameActivity;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.data.game.GameMetadata;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Objects;

public class GameUtils {
    private static final Bitmap DEFAULT_ICON = Bitmap.createBitmap(48,48, Bitmap.Config.ARGB_8888);
    private static final String KEY_GAME_LIST = "gameList";
    private static final ArrayList<GameMetadata> games = new ArrayList<>();
    private static SharedPreferences data;
    private static final Gson gson = new Gson();

    private static GameMetadata currentGame;

    public static void initialize() {
        data = PandroidApplication.getAppContext().getSharedPreferences(Constants.PREF_GAME_UTILS, Context.MODE_PRIVATE);

        GameMetadata[] list = gson.fromJson(data.getString(KEY_GAME_LIST, "[]"), GameMetadata[].class);

        for (GameMetadata game: list)
            game.getIcon();

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
        currentGame = game;
        String path = PathUtils.getPath(Uri.parse(game.getRomPath()));
        context.startActivity(new Intent(context, GameActivity.class).putExtra(Constants.ACTIVITY_PARAMETER_PATH, path));
    }

    public static GameMetadata getCurrentGame() {
        return currentGame;
    }

    public static void removeGame(GameMetadata game) {
        games.remove(game);
        saveAll();
    }

    public static void addGame(GameMetadata game) {
        games.add(0,game);
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

    public static void setGameIcon(String id, Bitmap icon) {
        try {
            String appPath = FileUtils.getPrivatePath();
            FileUtils.createDir(appPath, "cache_icons");
            FileUtils.createFile(appPath+"/cache_icons/", id+".png");

            OutputStream output = FileUtils.getOutputStream(appPath+"/cache_icons/"+id+".png");
            icon.compress(Bitmap.CompressFormat.PNG, 100, output);
            output.close();
        } catch (Exception e){
            Log.e(Constants.LOG_TAG, "Error on save game icon: ", e);
        }
    }

    public static Bitmap loadGameIcon(String id) {
        try {
            String path = FileUtils.getPrivatePath()+"/cache_icons/"+id+".png";
            if (FileUtils.exists(path)) {
                InputStream stream = FileUtils.getInputStream(path);
                Bitmap image = BitmapFactory.decodeStream(stream);
                stream.close();
                return image;
            }
        } catch (Exception e){
            Log.e(Constants.LOG_TAG, "Error on load game icon: ", e);
        }
        return DEFAULT_ICON;
    }
}
