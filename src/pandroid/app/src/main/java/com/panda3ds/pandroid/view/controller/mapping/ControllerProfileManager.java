package com.panda3ds.pandroid.view.controller.mapping;

import android.annotation.SuppressLint;
import android.view.Gravity;

import com.panda3ds.pandroid.data.GsonConfigParser;
import com.panda3ds.pandroid.utils.Constants;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;

public class ControllerProfileManager {

    public static final GsonConfigParser parser;
    private static final DataModel data;

    static {
        parser = new GsonConfigParser(Constants.PREF_SCREEN_CONTROLLER_PROFILES);
        data = parser.load(DataModel.class);
        if (data.profiles.size() == 0) {
            add(makeDefaultProfile());
        }
    }

    public static void remove(String id) {
        data.profiles.remove(id);
        save();
    }

    public static void add(Profile profile) {
        data.profiles.put(profile.getId(), profile);
        save();
    }

    public static List<Profile> listAll() {
        return new ArrayList<>(data.profiles.values());
    }

    public static int getProfileCount() {
        return data.profiles.size();
    }

    public static Profile getDefaultProfile() {
        if (data.profiles.containsKey(data.profileId)) {
            return data.profiles.get(data.profileId);
        } else if (getProfileCount() > 0) {
            data.profileId = data.profiles.keySet().iterator().next();
            save();
            return getDefaultProfile();
        } else {
            add(makeDefaultProfile());
            return getDefaultProfile();
        }
    }

    private static void save() {
        if ((!data.profiles.containsKey(data.profileId)) && getProfileCount() > 0) {
            data.profileId = data.profiles.keySet().iterator().next();
        }
        parser.save(data);
    }

    public static Profile makeDefaultProfile() {
        return new Profile(UUID.randomUUID().toString(), "Default", createDefaultLayout(), createDefaultLayout());
    }

    @SuppressLint("RtlHardcoded")
    public static Layout createDefaultLayout() {
        Layout layout = new Layout();

        layout.setLocation(ControllerItem.L, new Location(39, 145, Gravity.LEFT, true));
        layout.setLocation(ControllerItem.R, new Location(39, 145, Gravity.RIGHT, true));

        layout.setLocation(ControllerItem.SELECT, new Location(32, 131, Gravity.LEFT, true));
        layout.setLocation(ControllerItem.START, new Location(32, 131, Gravity.RIGHT, true));

        layout.setLocation(ControllerItem.DPAD, new Location(42, 90, Gravity.LEFT, true));
        layout.setLocation(ControllerItem.JOYSTICK, new Location(74, 45, Gravity.LEFT, true));
        layout.setLocation(ControllerItem.GAMEPAD, new Location(42, 75, Gravity.RIGHT, true));

        return layout;
    }

    public static Profile get(String profile) {
        return data.profiles.getOrDefault(profile, null);
    }

    public static void setDefaultProfileId(String id) {
        if (data.profiles.containsKey(id) && !Objects.equals(id, data.profileId)) {
            data.profileId = id;
            save();
        }
    }

    public static class DataModel {
        public final Map<String, Profile> profiles = new HashMap<>();
        public String profileId;
    }
}
