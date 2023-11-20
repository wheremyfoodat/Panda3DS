package com.panda3ds.pandroid;

import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.DisplayMetrics;

import com.panda3ds.pandroid.PandaGlRenderer;

public class PandaGlSurfaceView extends GLSurfaceView {
    final PandaGlRenderer renderer;

    public PandaGlSurfaceView(Context context) {
        super(context);
        setEGLContextClientVersion(3);
        renderer = new PandaGlRenderer();
        setRenderer(renderer);
    }
}