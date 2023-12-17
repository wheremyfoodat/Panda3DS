package com.panda3ds.pandroid.app.preferences;

import android.content.Context;
import android.os.Bundle;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.input.KeyName;

public class InputMapPreferences extends BasePreferenceFragment implements ActivityResultCallback<String> {

    private ActivityResultLauncher<String> requestKey;
    private String currentKey;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.input_map_preferences, rootKey);
        for (KeyName key : KeyName.values()) {
            if (key == KeyName.NULL) return;
            setItemClick(key.name(), this::onItemPressed);
        }
        refreshList();
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
