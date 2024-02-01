package com.panda3ds.pandroid.view.controller.mapping;


import androidx.annotation.NonNull;

import org.jetbrains.annotations.NotNull;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class Layout {
    private final Map<ControllerItem, Location> mapLocations = new HashMap<>();

    public void setLocation(ControllerItem item, Location location) {
        mapLocations.put(item, location);
    }

    @NotNull
    public Location getLocation(ControllerItem item) {
        if (!mapLocations.containsKey(item)) {
            setLocation(item, new Location());
        }
        return Objects.requireNonNull(mapLocations.get(item));
    }

    @NonNull
    @Override
    public Layout clone() {
        Layout cloned = new Layout();
        for (ControllerItem key : mapLocations.keySet()) {
            cloned.setLocation(key, getLocation(key).clone());
        }
        return cloned;
    }
}
