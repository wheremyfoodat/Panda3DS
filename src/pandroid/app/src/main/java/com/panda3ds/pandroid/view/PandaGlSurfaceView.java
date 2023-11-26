package com.panda3ds.pandroid.view;

import android.content.Context;
import android.opengl.GLSurfaceView;

public class PandaGlSurfaceView extends GLSurfaceView {
    final PandaGlRenderer renderer;

    public PandaGlSurfaceView(Context context, String romPath) {
        super(context);
        setEGLContextClientVersion(3);
        setDebugFlags(DEBUG_LOG_GL_CALLS);
        renderer = new PandaGlRenderer(romPath);
        setRenderer(renderer);
    }

    public PandaGlRenderer getRenderer() {
        return renderer;
    }
}