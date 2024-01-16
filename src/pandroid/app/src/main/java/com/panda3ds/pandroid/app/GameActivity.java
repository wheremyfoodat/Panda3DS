package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.Toast;
import androidx.annotation.Nullable;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.game.AlberInputListener;
import com.panda3ds.pandroid.app.game.DrawerFragment;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.input.InputHandler;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;
import com.panda3ds.pandroid.view.PandaLayoutController;

public class GameActivity extends BaseActivity {
	private final DrawerFragment drawerFragment = new DrawerFragment();
	private final AlberInputListener inputListener = new AlberInputListener(this::onBackPressed);

	@Override
	protected void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		Intent intent = getIntent();
		if (!intent.hasExtra(Constants.ACTIVITY_PARAMETER_PATH)) {
			setContentView(new FrameLayout(this));
			Toast.makeText(this, "Invalid rom path!", Toast.LENGTH_LONG).show();
			finish();
			return;
		}

		PandaGlSurfaceView pandaSurface = new PandaGlSurfaceView(this, intent.getStringExtra(Constants.ACTIVITY_PARAMETER_PATH));
		setContentView(R.layout.game_activity);

		((FrameLayout) findViewById(R.id.panda_gl_frame))
			.addView(pandaSurface, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

		PandaLayoutController controllerLayout = findViewById(R.id.controller_layout);
		controllerLayout.initialize();

		((CheckBox) findViewById(R.id.hide_screen_controller)).setOnCheckedChangeListener((buttonView, checked) -> {
			findViewById(R.id.overlay_controller).setVisibility(checked ? View.VISIBLE : View.GONE);
			findViewById(R.id.overlay_controller).invalidate();
			findViewById(R.id.overlay_controller).requestLayout();
			GlobalConfig.set(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE, checked);
		});
		((CheckBox) findViewById(R.id.hide_screen_controller)).setChecked(GlobalConfig.get(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE));

		getSupportFragmentManager().beginTransaction().replace(R.id.drawer_fragment, drawerFragment).commitNow();
	}

	@Override
	protected void onResume() {
		super.onResume();
		getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		InputHandler.reset();
		InputHandler.setMotionDeadZone(InputMap.getDeadZone());
		InputHandler.setEventListener(inputListener);
	}

	@Override
	protected void onPause() {
		super.onPause();

		InputHandler.reset();
		drawerFragment.open();
	}

	@Override
	public boolean dispatchKeyEvent(KeyEvent event) {
		if ((!drawerFragment.isOpened()) && InputHandler.processKeyEvent(event)) {
			return true;
		}

		return super.dispatchKeyEvent(event);
	}

	@Override
	public void onBackPressed() {
		if (drawerFragment.isOpened()) {
			drawerFragment.close();
		} else {
			drawerFragment.open();
		}
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent ev) {
		if ((!drawerFragment.isOpened()) && InputHandler.processMotionEvent(ev)) {
			return true;
		}

		return super.dispatchGenericMotionEvent(ev);
	}

	@Override
	protected void onDestroy() {
		if (AlberDriver.HasRomLoaded()) {
			AlberDriver.Finalize();
		}

		super.onDestroy();
	}
}
