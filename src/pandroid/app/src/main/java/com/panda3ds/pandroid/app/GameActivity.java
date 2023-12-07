package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;
import com.panda3ds.pandroid.view.PandaLayoutController;

public class GameActivity extends BaseActivity {

	@Override
	protected void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		Intent intent = getIntent();
		if (!intent.hasExtra(Constants.EXTRA_PATH)) {
			setContentView(new FrameLayout(this));
			Toast.makeText(this, "Invalid rom path!", Toast.LENGTH_LONG).show();
			finish();
			return;
		}

		PandaGlSurfaceView pandaSurface = new PandaGlSurfaceView(this, intent.getStringExtra(Constants.EXTRA_PATH));

		setContentView(R.layout.game_activity);

		((FrameLayout) findViewById(R.id.panda_gl_frame))
			.addView(pandaSurface, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

		PandaLayoutController controllerLayout = findViewById(R.id.controller_layout);

		controllerLayout.initialize();

		((CheckBox) findViewById(R.id.hide_screen_controller)).setOnCheckedChangeListener((buttonView, isChecked) -> findViewById(R.id.overlay_controller).setVisibility(isChecked ? View.VISIBLE : View.INVISIBLE));
	}

	@Override
	protected void onResume() {
		super.onResume();
		getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
	}
}
