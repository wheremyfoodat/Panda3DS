package com.panda3ds.pandroid.app.preferences;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.base.BottomAlertDialog;
import com.panda3ds.pandroid.view.controller.map.ControllerProfileManager;
import com.panda3ds.pandroid.view.controller.map.Profile;

import java.util.List;
import java.util.Objects;

public class InputPreferences extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.input_preference, rootKey);
        setItemClick("inputMap", (item) -> PreferenceActivity.launch(requireContext(), InputMapPreferences.class));
        setItemClick("add_screen_profile", (item) -> {
            new BottomAlertDialog(requireContext())
                    .setTextInput(getString(R.string.name), (name) -> {
                        name = formatName(name);
                        if (name.length() > 0) {
                            Profile profile = ControllerProfileManager.makeDefaultProfile();
                            profile.setName(name);
                            ControllerProfileManager.add(profile);
                            refreshScreenProfileList();
                        }
                    }).setTitle(R.string.create_profile).show();
        });

        setItemClick("defaultControllerProfile", (item)->{
            List<Profile> profiles = ControllerProfileManager.listAll();
            String defaultProfileId = ControllerProfileManager.getDefaultProfile().getId();
            int defaultProfileIndex = 0;
            CharSequence[] names = new CharSequence[profiles.size()];
            for (int i = 0; i < names.length; i++){
                names[i] = profiles.get(i).getName();
                if (Objects.equals(profiles.get(i).getId(), defaultProfileId)){
                    defaultProfileIndex = i;
                }
            }
            new BottomAlertDialog(item.getContext())
                    .setSingleChoiceItems(names, defaultProfileIndex, (dialog, which) -> {
                        dialog.dismiss();
                        ControllerProfileManager.setDefaultId(profiles.get(which).getId());
                        item.setSummary(profiles.get(which).getName());
                    }).setTitle(R.string.pref_default_controller_title).show();
        });

        ((BaseActivity)requireActivity()).getSupportActionBar().setTitle(R.string.input);
    }


    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }

    public String formatName(String name) {
        name = String.valueOf(name);
        while (name.startsWith(" ")) {
            name = name.substring(1);
        }
        while (name.endsWith(" ")) {
            name = name.substring(0, name.length() - 1);
        }
        name = name.replaceAll("\\s\\s", " ");
        return name;
    }

    private void refresh() {
        findPreference("defaultControllerProfile").setSummary(ControllerProfileManager.getDefaultProfile().getName());
        refreshScreenProfileList();
    }

    @SuppressLint("RestrictedApi")
    private void refreshScreenProfileList() {
        PreferenceCategory category = findPreference("screenGamepadProfiles");
        Preference add = category.getPreference(category.getPreferenceCount() - 1);
        category.removeAll();
        category.setOrderingAsAdded(true);

        for (Profile profile : ControllerProfileManager.listAll()) {
            Preference item = new Preference(category.getContext());
            item.setOnPreferenceClickListener(preference -> {
                category.performClick();
                PreferenceActivity.launch(requireActivity(), ControllerMapperPreferences.class, new Intent().putExtra("profile", profile.getId()));
                return false;
            });
            item.setOrder(category.getPreferenceCount());
            item.setIconSpaceReserved(false);
            item.setTitle(profile.getName());
            category.addPreference(item);
        }

        add.setOrder(category.getPreferenceCount());
        category.addPreference(add);
    }
}
