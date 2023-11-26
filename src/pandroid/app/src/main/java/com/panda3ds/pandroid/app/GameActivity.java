package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Bundle;
import android.widget.FrameLayout;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;

public class GameActivity extends BaseActivity {
    private PandaGlSurfaceView pandaSurface;

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
        setContentView(pandaSurface);
    }
}
