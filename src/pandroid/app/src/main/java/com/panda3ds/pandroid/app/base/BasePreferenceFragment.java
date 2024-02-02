package com.panda3ds.pandroid.app.base;

import android.annotation.SuppressLint;

import androidx.annotation.StringRes;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreference;

import com.panda3ds.pandroid.lang.Function;


public abstract class BasePreferenceFragment extends PreferenceFragmentCompat {
	@SuppressLint("RestrictedApi")
	protected void setItemClick(String key, Function<Preference> listener) {
		findPreference(key).setOnPreferenceClickListener(preference -> {
			listener.run(preference);
			getPreferenceScreen().performClick();
			return false;
		});
	}

	protected void setActivityTitle(@StringRes int titleId) {
		ActionBar header = ((AppCompatActivity) requireActivity()).getSupportActionBar();
		if (header != null) {
			header.setTitle(titleId);
		}
	}
}
