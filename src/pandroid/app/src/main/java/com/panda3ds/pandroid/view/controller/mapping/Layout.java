package com.panda3ds.pandroid.view.controller.mapping;


import androidx.annotation.NonNull;

import com.google.gson.Gson;

import org.jetbrains.annotations.NotNull;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class Layout {
    private static final Gson gson = new Gson();
    private final Map<String, Location> mapLocations = new HashMap<>();

    public void setLocation(ControllerItem item, Location location) {
        mapLocations.put(item.name(), location);
    }

    public Location getLocation(ControllerItem key) {
        Object raw = mapLocations.get(key.name());
        if (raw == null) return null;

        if (raw instanceof Location) {
            return (Location) raw;
        }

        return gson.fromJson(gson.toJson(raw), Location.class);
    }

    @NonNull
    @Override
    public Layout clone() {
        Layout cloned = new Layout();

        for (String key : mapLocations.keySet()) {
            ControllerItem item = ControllerItem.valueOf(key);
            cloned.setLocation(item, getLocation(item).clone());
        }

        return cloned;
    }
}
