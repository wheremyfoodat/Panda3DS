package com.panda3ds.pandroid.app;

import android.os.Bundle;
import android.view.MenuItem;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.navigation.NavigationBarView;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.main.GamesFragment;
import com.panda3ds.pandroid.app.main.SearchFragment;
import com.panda3ds.pandroid.app.main.SettingsFragment;

public class MainActivity extends BaseActivity implements NavigationBarView.OnItemSelectedListener {
	private final GamesFragment gamesFragment = new GamesFragment();
	private final SearchFragment searchFragment = new SearchFragment();
	private final SettingsFragment settingsFragment = new SettingsFragment();

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
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

		manager.beginTransaction().replace(R.id.fragment_container, fragment).commitNow();
		return true;
	}
}