package com.panda3ds.pandroid.app.base;

import android.annotation.SuppressLint;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import com.panda3ds.pandroid.lang.Function;

public abstract class BasePreferenceFragment extends PreferenceFragmentCompat {
    @SuppressLint("RestrictedApi")
    protected void setItemClick(String key, Function<Preference> listener){
        findPreference(key).setOnPreferenceClickListener(preference -> {
            listener.run(preference);
            getPreferenceScreen().performClick();
            return false;
        });
    }
}
