package com.panda3ds.pandroid.app;

import static android.Manifest.permission.READ_EXTERNAL_STORAGE;
import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;
import static android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.view.MenuItem;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.navigation.NavigationBarView;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.main.GamesFragment;
import com.panda3ds.pandroid.app.main.SearchFragment;
import com.panda3ds.pandroid.app.main.SettingsFragment;

public class MainActivity extends BaseActivity implements NavigationBarView.OnItemSelectedListener {
	private static final int PICK_ROM = 2;
	private static final int PERMISSION_REQUEST_CODE = 3;

	private final GamesFragment gamesFragment = new GamesFragment();
	private final SearchFragment searchFragment = new SearchFragment();
	private final SettingsFragment settingsFragment = new SettingsFragment();

	private void openFile() {
		Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
		intent.addCategory(Intent.CATEGORY_OPENABLE);
		intent.setType("*/*");
		startActivityForResult(intent, PICK_ROM);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (!Environment.isExternalStorageManager()) {
				Intent intent = new Intent(ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
				startActivity(intent);
			}
		} else {
			ActivityCompat.requestPermissions(this, new String[]{READ_EXTERNAL_STORAGE}, PERMISSION_REQUEST_CODE);
			ActivityCompat.requestPermissions(this, new String[]{WRITE_EXTERNAL_STORAGE}, PERMISSION_REQUEST_CODE);
		}

		setContentView(R.layout.activity_main);

		NavigationBarView bar = findViewById(R.id.navigation);
		bar.setOnItemSelectedListener(this);
		bar.postDelayed(() -> bar.setSelectedItemId(bar.getSelectedItemId()), 5);
	}

	@Override
	public boolean onNavigationItemSelected(@NonNull MenuItem item) {
		int id = item.getItemId();
		FragmentManager manager = getSupportFragmentManager();
		Fragment fragment;
		if (id == R.id.games) {
			fragment = gamesFragment;
		} else if (id == R.id.search) {
			fragment = searchFragment;
		} else if (id == R.id.settings) {
			fragment = settingsFragment;
		} else {
			return false;
		}

		manager.beginTransaction()
				.replace(R.id.fragment_container, fragment)
				.commitNow();
		return true;
	}
}