package com.panda3ds.pandroid.view;

import android.content.Context;
import android.opengl.GLSurfaceView;

public class PandaGlSurfaceView extends GLSurfaceView {
    final PandaGlRenderer renderer;

    public PandaGlSurfaceView(Context context) {
        super(context);
        setEGLContextClientVersion(3);
        renderer = new PandaGlRenderer();
        setRenderer(renderer);
    }

    public PandaGlRenderer getRenderer() {
        return renderer;
    }
}