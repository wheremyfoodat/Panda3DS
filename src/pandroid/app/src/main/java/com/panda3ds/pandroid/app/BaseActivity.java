package com.panda3ds.pandroid.app;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.color.DynamicColors;
import com.panda3ds.pandroid.data.config.GlobalConfig;


public class BaseActivity extends AppCompatActivity {
	private int currentTheme = PandroidApplication.getThemeId();

	@Override
	protected void onCreate(@Nullable Bundle savedInstanceState) {
		applyTheme();
		super.onCreate(savedInstanceState);
	}

	@Override
	protected void onResume() {
		super.onResume();

		if (PandroidApplication.getThemeId() != currentTheme) {
			recreate();
		}
	}

	private void applyTheme() {
		currentTheme = PandroidApplication.getThemeId();
		setTheme(currentTheme);
		if (GlobalConfig.get(GlobalConfig.KEY_APP_THEME) == GlobalConfig.THEME_ANDROID){
			DynamicColors.applyToActivityIfAvailable(this);
		}
	}
}
