package com.panda3ds.pandroid.app.preferences;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.SwitchPreference;
import com.google.android.material.color.DynamicColors;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.view.preferences.SingleSelectionPreferences;

public class AppearancePreferences extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.appearance_preference, rootKey);

        setActivityTitle(R.string.appearance);

        SingleSelectionPreferences themePreference = findPreference("theme");
        themePreference.setSelectedItem(GlobalConfig.get(GlobalConfig.KEY_APP_THEME));
        themePreference.setOnPreferenceChangeListener((preference, value) -> {
            GlobalConfig.set(GlobalConfig.KEY_APP_THEME, (int) value);
            return false;
        });
        setItemClick("dynamic_colors", pref -> GlobalConfig.set(GlobalConfig.KEY_DYNAMIC_COLORS, ((SwitchPreference) pref).isChecked()));

        refresh();
        }
    
        @Override
        public void onResume() {
        super.onResume();
        refresh();
        }

        private void refresh() {
        ((SwitchPreference) findPreference("dynamic_colors")).setChecked(GlobalConfig.get(GlobalConfig.KEY_DYNAMIC_COLORS));
        if (!DynamicColors.isDynamicColorAvailable(this)) {
        ((SwitchPreference) findPreference("dynamic_colors")).setEnabled(false);
        }
            
    }
       
 }
