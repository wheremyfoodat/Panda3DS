package com.panda3ds.pandroid.view;

import android.content.Context;
import android.graphics.Canvas;
import android.opengl.GLSurfaceView;
import android.util.Log;
import androidx.annotation.NonNull;
import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.controller.nodes.TouchScreenNodeImpl;
import com.panda3ds.pandroid.view.renderer.ConsoleRenderer;

public class PandaGlSurfaceView extends GLSurfaceView implements TouchScreenNodeImpl {
	final PandaGlRenderer renderer;
	private int size_width;
	private int size_height;

	public PandaGlSurfaceView(Context context, String romPath) {
		super(context);
		setEGLContextClientVersion(3);
		setDebugFlags(DEBUG_LOG_GL_CALLS);
		renderer = new PandaGlRenderer(romPath);
		setRenderer(renderer);
	}

	public ConsoleRenderer getRenderer() { return renderer; }

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
		size_width = getMeasuredWidth();
		size_height = getMeasuredHeight();
	}

	@NonNull
	@Override
	public Vector2 getSize() {
		return new Vector2(size_width, size_height);
	}

	@Override
	public void onTouch(TouchEvent event) {
		onTouchScreenPress(renderer, event);
	}
}