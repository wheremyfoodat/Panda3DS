package com.panda3ds.pandroid.app.main;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.preferences.GeneralPreferences;
import com.panda3ds.pandroid.app.preferences.AdvancedPreferences;
import com.panda3ds.pandroid.app.preferences.InputPreferences;

public class SettingsFragment extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.start_preferences, rootKey);
        findPreference("application").setSummary(getVersionName());
        setItemClick("input", (item) -> PreferenceActivity.launch(requireContext(), InputPreferences.class));
        setItemClick("general", (item)-> PreferenceActivity.launch(requireContext(), GeneralPreferences.class));
        setItemClick("advanced", (item)-> PreferenceActivity.launch(requireContext(), AdvancedPreferences.class));
    }

    private String getVersionName() {
        try {
            Context context = PandroidApplication.getAppContext();
            return context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "Error: Unknown version";
        }
    }
}
