package com.panda3ds.pandroid.app.main;

import android.os.Bundle;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.preferences.InputMapPreferences;

public class SettingsFragment extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.start_preferences, rootKey);
        setItemClick("inputMap", (item) -> PreferenceActivity.launch(requireContext(), InputMapPreferences.class));
    }
}
