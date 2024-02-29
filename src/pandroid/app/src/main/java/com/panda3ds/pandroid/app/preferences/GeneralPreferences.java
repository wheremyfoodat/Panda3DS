package com.panda3ds.pandroid.app.preferences;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.SwitchPreferenceCompat;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.preferences.screen_editor.ScreenLayoutsPreference;
import com.panda3ds.pandroid.data.config.GlobalConfig;

public class GeneralPreferences extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.general_preference, rootKey);
        setItemClick("appearance.theme", (pref) -> new ThemeSelectorDialog(requireActivity()).show());
        setItemClick("appearance.ds", (pref) -> PreferenceActivity.launch(requireActivity(), ScreenLayoutsPreference.class));
        setItemClick("games.folders", (pref) -> PreferenceActivity.launch(requireActivity(), GamesFoldersPreferences.class));
        setItemClick("behavior.pictureInPicture", (pref)-> GlobalConfig.set(GlobalConfig.KEY_PICTURE_IN_PICTURE, ((SwitchPreferenceCompat)pref).isChecked()));
        setActivityTitle(R.string.general);
        refresh();
    }

    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh(){
        setSwitchValue("behavior.pictureInPicture", GlobalConfig.get(GlobalConfig.KEY_PICTURE_IN_PICTURE));
    }
}
