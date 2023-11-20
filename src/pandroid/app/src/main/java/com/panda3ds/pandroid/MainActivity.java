package com.panda3ds.pandroid;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Build;
import android.os.Bundle;
import android.view.WindowInsets;
import android.view.View;

public class MainActivity extends AppCompatActivity {

    PandaGlSurfaceView glView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        glView = new PandaGlSurfaceView(this);
        setContentView(glView);
    }
}