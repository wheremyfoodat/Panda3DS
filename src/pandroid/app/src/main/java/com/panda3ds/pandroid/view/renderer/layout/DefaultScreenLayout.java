package com.panda3ds.pandroid.view.renderer.layout;

import android.graphics.Rect;
import android.util.Size;
import com.panda3ds.pandroid.math.Vector2;

public class DefaultScreenLayout implements ConsoleLayout {
	private final Rect topDisplay = new Rect();
	private final Rect bottomDisplay = new Rect();

	private final Vector2 screenSize = new Vector2(1.0F, 1.0F);
	private final Vector2 topSourceSize = new Vector2(1.0F, 1.0F);
	private final Vector2 bottomSourceSize = new Vector2(1.0F, 1.0F);

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

		if (screenWidth > screenHeight) {
			int topDisplayWidth = (int) ((screenHeight / topSourceSize.y) * topSourceSize.x);
			int topDisplayHeight = screenHeight;

			if (topDisplayWidth > (screenWidth * 0.7)) {
				topDisplayWidth = (int) (screenWidth * 0.7);
				topDisplayHeight = (int) ((topDisplayWidth / topSourceSize.x) * topSourceSize.y);
			}

			int bottomDisplayHeight = (int) (((screenWidth - topDisplayWidth) / bottomSourceSize.x) * bottomSourceSize.y);

			topDisplay.set(0, 0, topDisplayWidth, topDisplayHeight);
			bottomDisplay.set(topDisplayWidth, 0, topDisplayWidth + (screenWidth - topDisplayWidth), bottomDisplayHeight);
		} else {
			int topScreenHeight = (int) ((screenWidth / topSourceSize.x) * topSourceSize.y);
			topDisplay.set(0, 0, screenWidth, topScreenHeight);

			int bottomDisplayHeight = (int) ((screenWidth / bottomSourceSize.x) * bottomSourceSize.y);
			int bottomDisplayWidth = screenWidth;
			int bottomDisplayX = 0;

			if (topScreenHeight + bottomDisplayHeight > screenHeight) {
				bottomDisplayHeight = (screenHeight - topScreenHeight);
				bottomDisplayWidth = (int) ((bottomDisplayHeight / bottomSourceSize.y) * bottomSourceSize.x);
				bottomDisplayX = (screenWidth - bottomDisplayX) / 2;
			}

			topDisplay.set(0, 0, screenWidth, topScreenHeight);
			bottomDisplay.set(bottomDisplayX, topScreenHeight, bottomDisplayX + bottomDisplayWidth, topScreenHeight + bottomDisplayHeight);
		}
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
