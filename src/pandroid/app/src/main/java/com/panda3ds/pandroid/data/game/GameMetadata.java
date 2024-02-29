package com.panda3ds.pandroid.data.game;

import android.graphics.Bitmap;
import android.util.Log;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.data.SMDH;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.GameUtils;

import java.util.Objects;
import java.util.UUID;

public class GameMetadata {
    private final String id;
    private final String romPath;
    private String title;
    private String publisher;
    private GameRegion[] regions;
    private transient Bitmap icon;

    private GameMetadata(String id, String romPath, String title, String publisher, Bitmap icon, GameRegion[] regions) {
        this.id = id;
        this.title = title;
        this.publisher = publisher;
        this.romPath = romPath;
        this.regions = regions;
        if (icon != null) {
            GameUtils.setGameIcon(id, icon);
        }
    }

    public GameMetadata(String romPath,String title, String publisher, GameRegion[] regions) {
        this(UUID.randomUUID().toString(), romPath, title, publisher, null, regions);
    }

    public GameMetadata(String romPath,String title, String publisher) {
        this(romPath,title, publisher, new GameRegion[]{GameRegion.None});
    }

    public String getRomPath() {
        return romPath;
    }

    public String getRealPath() {
        return GameUtils.resolvePath(romPath);
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
        if (icon == null || icon.isRecycled()) {
            icon = GameUtils.loadGameIcon(id);
        }
        return icon;
    }

    public GameRegion[] getRegions() {
        return regions;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof GameMetadata) {
            return Objects.equals(((GameMetadata) obj).id, id);
        }
        return false;
    }

    public void applySMDH(SMDH smdh) {
        Bitmap icon = smdh.getBitmapIcon();
        this.title = smdh.getTitle();
        this.publisher = smdh.getPublisher();
        this.icon = icon;
        if (icon != null) {
            GameUtils.setGameIcon(id, icon);
        }
        
        this.regions = new GameRegion[]{smdh.getRegion()};
        GameUtils.writeChanges();
    }
}
