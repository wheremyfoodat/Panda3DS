package com.panda3ds.pandroid.app.preferences.screen_editor;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.view.ds.DsLayoutManager;

public class ScreenLayoutsPreference extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.empty_preferences, rootKey);
        setActivityTitle(R.string.dual_screen_layouts);
        refresh();
    }

    public void refresh() {
        PreferenceScreen screen = getPreferenceScreen();
        screen.removeAll();

        for (int i = 0; i < DsLayoutManager.getLayoutCount(); i++) {
            Preference pref = new Preference(getPreferenceScreen().getContext());
            pref.setIconSpaceReserved(false);
            pref.setTitle("Layout "+ (i + 1));
            pref.setSummary(R.string.click_to_change);
            pref.setIcon(R.drawable.ic_edit);
            pref.setKey(String.valueOf(i));

            final int index = i;
            pref.setOnPreferenceClickListener(preference -> {
                PreferenceActivity.launch(requireContext(), ScreenEditorPreference.class, new Intent().putExtra("index", index));
                return false;
            });
            screen.addPreference(pref);
        }
    }
}
