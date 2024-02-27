package com.panda3ds.pandroid.app.preferences;

import android.os.Bundle;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.preferences.ds.DsListPreferences;

public class GeneralPreferences extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.general_preference, rootKey);
        setItemClick("appearance.theme", (pref) -> new ThemeSelectorDialog(requireActivity()).show());
        setItemClick("appearance.ds", (pref) -> PreferenceActivity.launch(requireActivity(), DsListPreferences.class));
        setItemClick("games.folders", (pref) -> PreferenceActivity.launch(requireActivity(), GamesFoldersPreferences.class));
        setActivityTitle(R.string.general);
    }
}
