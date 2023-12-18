package com.panda3ds.pandroid.app.preferences;

import android.content.Context;
import android.os.Bundle;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.SeekBarPreference;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.input.KeyName;

public class InputMapPreferences extends BasePreferenceFragment implements ActivityResultCallback<String> {

    private ActivityResultLauncher<String> requestKey;
    private String currentKey;

    private SeekBarPreference deadZonePreference;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.input_map_preferences, rootKey);

        ((BaseActivity) requireActivity()).getSupportActionBar().setTitle(R.string.controller_mapping);

        for (KeyName key : KeyName.values()) {
            if (key == KeyName.NULL) continue;
            setItemClick(key.name(), this::onItemPressed);
        }

        deadZonePreference = getPreferenceScreen().findPreference("dead_zone");

        deadZonePreference.setOnPreferenceChangeListener((preference, value) -> {
            InputMap.setDeadZone(((int)value/100.0f));
            refreshList();
            return false;
        });

        refreshList();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onAttach(@NonNull Context context) {
        super.onAttach(context);
        requestKey = registerForActivityResult(new InputMapActivity.Contract(), this);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        if (requestKey != null) {
            requestKey.unregister();
            requestKey = null;
        }
    }

    private void onItemPressed(Preference pref) {
        currentKey = pref.getKey();
        requestKey.launch(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        refreshList();
    }

    private void refreshList() {
        deadZonePreference.setValue((int)(InputMap.getDeadZone()*100));
        deadZonePreference.setSummary(deadZonePreference.getValue()+"%");
        for (KeyName key : KeyName.values()) {
            if (key == KeyName.NULL) continue;
            findPreference(key.name()).setSummary(InputMap.relative(key));
        }
    }

    @Override
    public void onActivityResult(String result) {
        if (result != null) {
            InputMap.set(KeyName.valueOf(currentKey), result);
            refreshList();
        }
    }
}
