package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;
import com.panda3ds.pandroid.view.PandaLayoutController;
import com.panda3ds.pandroid.view.controller.ControllerLayout;

public class GameActivity extends BaseActivity {
    private PandaGlSurfaceView pandaSurface;
    private PandaLayoutController controllerLayout;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent intent = getIntent();
        if(!intent.hasExtra(Constants.EXTRA_PATH)){

            setContentView(new FrameLayout(this));
            Toast.makeText(this, "INVALID ROM PATH", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        pandaSurface = new PandaGlSurfaceView(this, intent.getStringExtra(Constants.EXTRA_PATH));;

        setContentView(R.layout.game_activity);

        ((FrameLayout)findViewById(R.id.panda_gl_frame))
                .addView(pandaSurface, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        controllerLayout = findViewById(R.id.controller_layout);
        controllerLayout.initialize();

        ((CheckBox)findViewById(R.id.hide_screen_controller))
                .setOnCheckedChangeListener((buttonView, isChecked) -> {
                    controllerLayout.setVisibility(isChecked ? View.VISIBLE : View.INVISIBLE);
                });
    }
}
