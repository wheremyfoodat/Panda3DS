package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Build;
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
import android.app.PictureInPictureParams;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.util.Rational;
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
			findViewById(R.id.overlay_controller).setVisibility(checked ? View.VISIBLE : View.GONE);
			findViewById(R.id.overlay_controller).invalidate();
			findViewById(R.id.overlay_controller).requestLayout();
			GlobalConfig.set(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE, checked);
		});
		((CheckBox) findViewById(R.id.hide_screen_controller)).setChecked(GlobalConfig.get(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE));

		getSupportFragmentManager().beginTransaction().replace(R.id.drawer_fragment, drawerFragment).commitNow();

		if (GlobalConfig.get(GlobalConfig.KEY_SHOW_PERFORMANCE_OVERLAY)) {
			PerformanceView view = new PerformanceView(this);
			((FrameLayout) findViewById(R.id.panda_gl_frame)).addView(view, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
		getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		InputHandler.reset();
		InputHandler.setMotionDeadZone(InputMap.getDeadZone());
		InputHandler.setEventListener(inputListener);
		if (GlobalConfig.get(GlobalConfig.KEY_PICTURE_IN_PICTURE)) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
		if (!drawerFragment.isOpened()) {
                    setPictureInPictureParams(new PictureInPictureParams.Builder().setAutoEnterEnabled(true).build());
		   }
		 }
	      }
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
			getTheme().applyStyle(R.style.GameActivityNavigationBar, true);
		}
	}

	@Override
        public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode, Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        if (isInPictureInPictureMode) {
           findViewById(R.id.overlay_controller).setVisibility(View.GONE);
        } else {
	  if (GlobalConfig.get(GlobalConfig.KEY_SCREEN_GAMEPAD_VISIBLE)) {
            findViewById(R.id.overlay_controller).setVisibility(View.VISIBLE);
	  }
        }
    }

	@Override
        public void onUserLeaveHint() {
        super.onUserLeaveHint();
	DisplayMetrics displayMetrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);

        int widthPixels = displayMetrics.widthPixels;
        int heightPixels = displayMetrics.heightPixels;

        // Calculate aspect ratio
        float aspectRatio = (float) widthPixels / (float) heightPixels;

        if (GlobalConfig.get(GlobalConfig.KEY_PICTURE_IN_PICTURE)) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
	if (!drawerFragment.isOpened()) {
            Rational aspectRatioRational = new Rational(widthPixels, heightPixels);
            PictureInPictureParams.Builder pipBuilder = new PictureInPictureParams.Builder();
            pipBuilder.setAspectRatio(aspectRatioRational);
            enterPictureInPictureMode(pipBuilder.build());
	  }
       }
     }
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
	public void swapScreens() {
		currentDsLayout = currentDsLayout + 1 < DsLayoutManager.getLayoutCount() ? currentDsLayout + 1 : 0;
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
	}

	@Override
	protected void onUserLeaveHint() {
		super.onUserLeaveHint();
	}

	@Override
	protected void onDestroy() {
		if (AlberDriver.HasRomLoaded()) {
			AlberDriver.Finalize();
		}

		super.onDestroy();
	}
}
