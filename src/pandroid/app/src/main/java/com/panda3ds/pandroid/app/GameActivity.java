package com.panda3ds.pandroid.app;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.C;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.view.PandaGlSurfaceView;

public class GameActivity extends BaseActivity {
    private PandaGlSurfaceView pandaSurface;


    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        pandaSurface = new PandaGlSurfaceView(this);;
        setContentView(pandaSurface);

        Intent intent = getIntent();
        if(!intent.hasExtra(C.EXTRA_PATH)){
            Toast.makeText(this, "INVALID ROM PATH", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        pandaSurface.getRenderer().postDrawEvent(()->{
            String path = intent.getStringExtra(C.EXTRA_PATH);
            Log.i(C.LOG_TAG,"Try load ROM: "+path);
            AlberDriver.LoadRom(path);
        });
    }
}
