package com.panda3ds.pandroid.view.renderer.layout;

import android.graphics.Rect;
import android.view.Gravity;

import com.panda3ds.pandroid.math.Vector2;

public class SingleScreenLayout implements ConsoleLayout {
	private final Rect topDisplay = new Rect();
	private final Rect bottomDisplay = new Rect();
	private final Vector2 screenSize = new Vector2(1.0f, 1.0f);
	private final Vector2 topSourceSize = new Vector2(1.0f, 1.0f);
	private final Vector2 bottomSourceSize = new Vector2(1.0f, 1.0f);
	private boolean top = true;

	@Override
	public void update(int screenWidth, int screenHeight) {
		screenSize.set(screenWidth, screenHeight);
		updateBounds();
	}

	@Override
	public void setBottomDisplaySourceSize(int width, int height) {
		bottomSourceSize.set(width, height);
		updateBounds();
	}
	@Override
	public void setTopDisplaySourceSize(int width, int height) {
		topSourceSize.set(width, height);
		updateBounds();
	}

	private void updateBounds() {
		int screenWidth = (int) screenSize.x;
		int screenHeight = (int) screenSize.y;
		Vector2 source = top ? topSourceSize : bottomSourceSize;
		Rect dest = top ? topDisplay : bottomDisplay;

		int width = Math.round((screenHeight / source.y) * source.x);
		int height = screenHeight;
		int y = 0;
		int x = (screenWidth - width) / 2;
		if (width > screenWidth){
			width = screenWidth;
			height = Math.round((screenWidth / source.x) * source.y);
			x = 0;
			y = (screenHeight - height)/2;
		}
		dest.set(x, y, x + width, y+height);

		(top ? bottomDisplay : topDisplay).set(0,0,0,0);
	}

	@Override
	public Rect getBottomDisplayBounds() {
		return bottomDisplay;
	}

	@Override
	public Rect getTopDisplayBounds() {
		return topDisplay;
	}
}
