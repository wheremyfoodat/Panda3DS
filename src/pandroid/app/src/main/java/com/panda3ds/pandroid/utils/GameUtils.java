package com.panda3ds.pandroid.utils;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.util.Log;

import com.panda3ds.pandroid.app.GameActivity;
import com.panda3ds.pandroid.data.GsonConfigParser;
import com.panda3ds.pandroid.data.game.GameMetadata;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

public class GameUtils {
    private static final Bitmap DEFAULT_ICON = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888);
    public static GsonConfigParser parser = new GsonConfigParser(Constants.PREF_GAME_UTILS);

    private static DataModel data;

    private static GameMetadata currentGame;

    public static void initialize() {
        data = parser.load(DataModel.class);
    }

    public static GameMetadata findByRomPath(String romPath) {
        for (GameMetadata game : data.games) {
            if (Objects.equals(romPath, game.getRealPath())) {
                return game;
            }
        }
        return null;
    }

    public static void launch(Context context, GameMetadata game) {
        currentGame = game;
        String path = game.getRealPath();
        if (path.contains("://")) {
            String[] parts = Uri.decode(game.getRomPath()).split("/");
            String name = parts[parts.length - 1];
            path = "game://internal/" + name;
        }
        
        context.startActivity(new Intent(context, GameActivity.class).putExtra(Constants.ACTIVITY_PARAMETER_PATH, path));
    }

    public static GameMetadata getCurrentGame() {
        return currentGame;
    }

    public static void removeGame(GameMetadata game) {
        data.games.remove(game);
        writeChanges();
    }

    public static void addGame(GameMetadata game) {
        data.games.add(0, game);
        writeChanges();
    }

    public static String resolvePath(String path) {
        String lower = path.toLowerCase();
        if (!lower.contains("://")) {
            return path;
        }

        Uri uri = Uri.parse(path);
        switch (uri.getScheme().toLowerCase()) {
            case "elf": {
                return FileUtils.getResourcePath(Constants.RESOURCE_FOLDER_ELF)+"/"+uri.getAuthority();
            }
        }
    
        return path;
    }

    public static ArrayList<GameMetadata> getGames() {
        return new ArrayList<>(data.games);
    }

    private static void writeChanges() {
        parser.save(data);
    }

    public static void setGameIcon(String id, Bitmap icon) {
        try {
            String appPath = FileUtils.getPrivatePath();
            FileUtils.createDir(appPath, "cache_icons");
            FileUtils.createFile(appPath + "/cache_icons/", id + ".png");

            OutputStream output = FileUtils.getOutputStream(appPath + "/cache_icons/" + id + ".png");
            icon.compress(Bitmap.CompressFormat.PNG, 100, output);
            output.close();
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on save game icon: ", e);
        }
    }

    public static Bitmap loadGameIcon(String id) {
        try {
            String path = FileUtils.getPrivatePath() + "/cache_icons/" + id + ".png";
            if (FileUtils.exists(path)) {
                InputStream stream = FileUtils.getInputStream(path);
                Bitmap image = BitmapFactory.decodeStream(stream);
                stream.close();
                return image;
            }
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on load game icon: ", e);
        }
        return DEFAULT_ICON;
    }

    private static class DataModel {
        public final List<GameMetadata> games = new ArrayList<>();
    }
}
