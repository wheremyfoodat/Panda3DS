package com.panda3ds.pandroid.app;

import android.app.ActivityManager;
import android.app.PictureInPictureParams;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Rational;
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
import com.panda3ds.pandroid.app.game.EmulatorCallback;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.input.InputHandler;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;
import com.panda3ds.pandroid.view.PandaLayoutController;
import com.panda3ds.pandroid.view.ds.DsLayoutManager;
import com.panda3ds.pandroid.view.renderer.ConsoleRenderer;
import com.panda3ds.pandroid.view.utils.PerformanceView;

public class GameActivity extends BaseActivity implements EmulatorCallback {
	private final DrawerFragment drawerFragment = new DrawerFragment();
	private final AlberInputListener inputListener = new AlberInputListener(this);
	private ConsoleRenderer renderer;
	private int currentDsLayout;

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

		renderer = pandaSurface.getRenderer();

		((FrameLayout) findViewById(R.id.panda_gl_frame))
				.addView(pandaSurface, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

		PandaLayoutController controllerLayout = findViewById(R.id.controller_layout);
		controllerLayout.initialize();

		((CheckBox) findViewById(R.id.hide_screen_controller)).setOnCheckedChangeListener((buttonView, checked) -> {
			changeOverlayVisibility(checked);
			GlobalConfig.set(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE, checked);
		});
		((CheckBox) findViewById(R.id.hide_screen_controller)).setChecked(GlobalConfig.get(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE));

		getSupportFragmentManager().beginTransaction().replace(R.id.drawer_fragment, drawerFragment).commitNow();

		if (GlobalConfig.get(GlobalConfig.KEY_SHOW_PERFORMANCE_OVERLAY)) {
			PerformanceView view = new PerformanceView(this);
			((FrameLayout) findViewById(R.id.panda_gl_frame)).addView(view, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		}
		swapScreens(GlobalConfig.get(GlobalConfig.KEY_CURRENT_DS_LAYOUT));
	}

	private void changeOverlayVisibility(boolean visible) {
		findViewById(R.id.overlay_controller).setVisibility(visible ? View.VISIBLE : View.GONE);
		findViewById(R.id.overlay_controller).invalidate();
		findViewById(R.id.overlay_controller).requestLayout();
	}

	@Override
	protected void onResume() {
		super.onResume();
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		InputHandler.reset();
		InputHandler.setMotionDeadZone(InputMap.getDeadZone());
		InputHandler.setEventListener(inputListener);
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
			getTheme().applyStyle(R.style.GameActivityNavigationBar, true);
		}
	}

	private void enablePIP() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			PictureInPictureParams.Builder builder = new PictureInPictureParams.Builder();
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
				builder.setAutoEnterEnabled(true);
				builder.setSeamlessResizeEnabled(true);
			}

			builder.setAspectRatio(new Rational(10, 14));
			enterPictureInPictureMode(builder.build());
		}
	}

	@Override
	protected void onPause() {
		super.onPause();

		InputHandler.reset();
		if (GlobalConfig.get(GlobalConfig.KEY_PICTURE_IN_PICTURE)) {
			if (Build.VERSION.SDK_INT > Build.VERSION_CODES.O) {
				enablePIP();
			}
		} else {
			drawerFragment.open();
		}
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
	public void swapScreens(int index) {
		currentDsLayout = index;
		GlobalConfig.set(GlobalConfig.KEY_CURRENT_DS_LAYOUT, index);
		renderer.setLayout(DsLayoutManager.createLayout(currentDsLayout));
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent ev) {
		if ((!drawerFragment.isOpened()) && InputHandler.processMotionEvent(ev)) {
			return true;
		}

		return super.dispatchGenericMotionEvent(ev);
	}

	@Override
	public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode) {
		super.onPictureInPictureModeChanged(isInPictureInPictureMode);

		changeOverlayVisibility(!isInPictureInPictureMode && GlobalConfig.get(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE));
		findViewById(R.id.hide_screen_controller).setVisibility(isInPictureInPictureMode ? View.INVISIBLE : View.VISIBLE);
		
        if (isInPictureInPictureMode) {
			getWindow().getDecorView().postDelayed(drawerFragment::close, 250);
		} else if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
			ActivityManager manager = ((ActivityManager) getSystemService(ACTIVITY_SERVICE));
			manager.getAppTasks().forEach(ActivityManager.AppTask::moveToFront);
		}
	}

	@Override
	protected void onDestroy() {
		if (AlberDriver.HasRomLoaded()) {
			AlberDriver.Finalize();
		}

		super.onDestroy();
	}
}
