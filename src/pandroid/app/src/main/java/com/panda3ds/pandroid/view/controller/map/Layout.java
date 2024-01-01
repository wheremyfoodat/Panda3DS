package com.panda3ds.pandroid.view.controller.map;


import androidx.annotation.NonNull;

import org.jetbrains.annotations.NotNull;

import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

public class Layout {
    private final Map<NodeID, Location> mapLocations = new HashMap<>();
    public void setLocation(NodeID id, Location location) {
        mapLocations.put(id, location);
    }

    @NotNull
    public Location getLocation(NodeID id) {
        if (!mapLocations.containsKey(id)) {
            setLocation(id, new Location());
        }
        return Objects.requireNonNull(mapLocations.get(id));
    }

    @NonNull
    @Override
    public Layout clone() {
        Layout cloned = new Layout();
        for (NodeID key :  mapLocations.keySet()){
            cloned.setLocation(key, getLocation(key).clone());
        }
        return cloned;
    }
}
