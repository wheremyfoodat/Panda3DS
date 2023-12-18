package com.panda3ds.pandroid.data.game;

import android.graphics.Bitmap;

import java.util.UUID;

public class GameMetadata {

    private final String id;
    private final String romPath;
    private final String title;
    private transient final Bitmap icon = Bitmap.createBitmap(48,48, Bitmap.Config.RGB_565);
    private final String publisher;
    private final GameRegion[] regions = new GameRegion[]{GameRegion.None};

    public GameMetadata(String title, String romPath, String publisher) {
        this.id = UUID.randomUUID().toString();
        this.title = title;
        this.publisher = publisher;
        this.romPath = romPath;
    }

    public String getRomPath() {
        return romPath;
    }

    public String getId() {
        return id;
    }

    public String getTitle() {
        return title;
    }

    public String getPublisher() {
        return publisher;
    }

    public Bitmap getIcon() {
        return icon;
    }

    public GameRegion[] getRegions() {
        return regions;
    }
}
