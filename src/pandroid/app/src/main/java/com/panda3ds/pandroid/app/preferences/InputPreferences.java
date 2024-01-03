package com.panda3ds.pandroid.app.preferences;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.base.BottomAlertDialog;
import com.panda3ds.pandroid.view.controller.mapping.ControllerProfileManager;
import com.panda3ds.pandroid.view.controller.mapping.Profile;

import java.util.List;
import java.util.Objects;

public class InputPreferences extends BasePreferenceFragment {

    public static final String ID_DEFAULT_CONTROLLER_PROFILE = "defaultControllerProfile";
    public static final String ID_INPUT_MAP = "inputMap";
    public static final String ID_CREATE_PROFILE = "createProfile";
    private static final CharSequence ID_GAMEPAD_PROFILE_LIST = "gamepadProfileList";

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.input_preference, rootKey);
        setItemClick(ID_INPUT_MAP, (item) -> PreferenceActivity.launch(requireContext(), InputMapPreferences.class));
        setItemClick(ID_CREATE_PROFILE, (item) -> {
            new BottomAlertDialog(requireContext())
                    .setTextInput(getString(R.string.name), (name) -> {
                        name = formatName(name);
                        if (name.length() > 0) {
                            Profile profile = ControllerProfileManager.makeDefaultProfile();
                            profile.setName(name);
                            ControllerProfileManager.add(profile);
                            refreshScreenProfileList();
                        } else {
                            Toast.makeText(requireContext(), R.string.invalid_name, Toast.LENGTH_SHORT).show();
                        }
                    }).setTitle(R.string.create_profile).show();
        });

        setItemClick(ID_DEFAULT_CONTROLLER_PROFILE, (item) -> {
            List<Profile> profiles = ControllerProfileManager.listAll();
            String defaultProfileId = ControllerProfileManager.getDefaultProfile().getId();
            int defaultProfileIndex = 0;
            CharSequence[] names = new CharSequence[profiles.size()];
            for (int i = 0; i < names.length; i++) {
                names[i] = profiles.get(i).getName();
                if (Objects.equals(profiles.get(i).getId(), defaultProfileId)) {
                    defaultProfileIndex = i;
                }
            }
            new BottomAlertDialog(item.getContext())
                    .setSingleChoiceItems(names, defaultProfileIndex, (dialog, which) -> {
                        dialog.dismiss();
                        ControllerProfileManager.setDefaultProfileId(profiles.get(which).getId());
                        item.setSummary(profiles.get(which).getName());
                    }).setTitle(R.string.pref_default_controller_title).show();
        });

        ((BaseActivity) requireActivity()).getSupportActionBar().setTitle(R.string.input);
    }

    public String formatName(String name) {
        return name.trim().replaceAll("\\s\\s", " ");
    }

    private void refresh() {
        findPreference(ID_DEFAULT_CONTROLLER_PROFILE).setSummary(ControllerProfileManager.getDefaultProfile().getName());
        refreshScreenProfileList();
    }

    @SuppressLint("RestrictedApi")
    private void refreshScreenProfileList() {
        PreferenceCategory category = findPreference(ID_GAMEPAD_PROFILE_LIST);
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

    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }
}