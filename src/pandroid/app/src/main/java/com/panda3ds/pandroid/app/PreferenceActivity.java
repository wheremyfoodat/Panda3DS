package com.panda3ds.pandroid.app;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.MenuItem;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;

public class PreferenceActivity extends BaseActivity {
	@Override
	protected void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		Intent intent = getIntent();

		setContentView(R.layout.activity_preference);
		setSupportActionBar(findViewById(R.id.toolbar));

		getSupportActionBar().setDisplayHomeAsUpEnabled(true);

		if (!intent.hasExtra(Constants.ACTIVITY_PARAMETER_FRAGMENT)) {
			finish();
			return;
		}

		try {
			Class<?> clazz = getClassLoader().loadClass(intent.getStringExtra(Constants.ACTIVITY_PARAMETER_FRAGMENT));
			Fragment fragment = (Fragment) clazz.newInstance();
			fragment.setArguments(intent.getExtras());
			getSupportFragmentManager().beginTransaction().replace(R.id.fragment_container, fragment).commitNow();
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	public static void launch(Context context, Class<? extends Fragment> clazz) {
		launch(context, clazz, new Intent());
	}

	public static void launch(Context context, Class<? extends Fragment> clazz, Intent extras) {
		context.startActivity(new Intent(context, PreferenceActivity.class)
								  .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
								  .putExtras(extras)
								  .putExtra(Constants.ACTIVITY_PARAMETER_FRAGMENT, clazz.getName()));
	}

	@Override
	public boolean onOptionsItemSelected(@NonNull MenuItem item) {
		if (item.getItemId() == android.R.id.home) {
			finish();
		}

		return super.onOptionsItemSelected(item);
	}
}
