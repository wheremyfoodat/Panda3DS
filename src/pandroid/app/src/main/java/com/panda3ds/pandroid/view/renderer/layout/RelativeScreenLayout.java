package com.panda3ds.pandroid.view.renderer.layout;

import android.graphics.Rect;
import android.view.Gravity;

import com.panda3ds.pandroid.math.Vector2;

public class RelativeScreenLayout implements ConsoleLayout {
	private final Rect topDisplay = new Rect();
	private final Rect bottomDisplay = new Rect();

	private final Vector2 screenSize = new Vector2(1.0f, 1.0f);
	private final Vector2 topSourceSize = new Vector2(1.0f, 1.0f);
	private final Vector2 bottomSourceSize = new Vector2(1.0f, 1.0f);

	private boolean landscapeReverse = false;
	private boolean portraitReverse = false;
	private float landscapeSpace = 0.6f;
	private int landscapeGravity = Gravity.CENTER;
	private int portraitGravity = Gravity.TOP;

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

	public void setPortraitGravity(int portraitGravity) {
		this.portraitGravity = portraitGravity;
	}

	public void setLandscapeGravity(int landscapeGravity) {
		this.landscapeGravity = landscapeGravity;
	}

	public void setLandscapeSpace(float landscapeSpace) {
		this.landscapeSpace = landscapeSpace;
	}

	public void setLandscapeReverse(boolean landscapeReverse) {
		this.landscapeReverse = landscapeReverse;
	}

	public void setPortraitReverse(boolean portraitReverse) {
		this.portraitReverse = portraitReverse;
	}

	private void updateBounds() {
		int screenWidth = (int) screenSize.x;
		int screenHeight = (int) screenSize.y;

		Vector2 topSourceSize = this.topSourceSize;
		Vector2 bottomSourceSize = this.bottomSourceSize;

		Rect topDisplay = this.topDisplay;
		Rect bottomDisplay = this.bottomDisplay;

		if ((landscapeReverse  && screenWidth > screenHeight) || (portraitReverse && screenWidth < screenHeight)){
			topSourceSize = this.bottomSourceSize;
			bottomSourceSize = this.topSourceSize;

			topDisplay = this.bottomDisplay;
			bottomDisplay = this.topDisplay;
		}

		if (screenWidth > screenHeight) {
			int topDisplayWidth = (int) ((screenHeight / topSourceSize.y) * topSourceSize.x);
			int topDisplayHeight = screenHeight;

			if (topDisplayWidth > (screenWidth * landscapeSpace)) {
				topDisplayWidth = (int) (screenWidth * landscapeSpace);
				topDisplayHeight = (int) ((topDisplayWidth / topSourceSize.x) * topSourceSize.y);
			}

			int bottomDisplayHeight = (int) (((screenWidth - topDisplayWidth) / bottomSourceSize.x) * bottomSourceSize.y);

			topDisplay.set(0, 0, topDisplayWidth, topDisplayHeight);
			bottomDisplay.set(topDisplayWidth, 0, topDisplayWidth + (screenWidth - topDisplayWidth), bottomDisplayHeight);
			adjustHorizontalGravity();
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
			adjustVerticalGravity();
		}
	}

	private void adjustHorizontalGravity(){
		int topOffset = 0;
		int bottomOffset = 0;
		switch (landscapeGravity){
			case Gravity.CENTER:{
				topOffset = (int) (screenSize.y - topDisplay.height())/2;
				bottomOffset = (int) (screenSize.y - bottomDisplay.height())/2;
			}break;
			case Gravity.BOTTOM:{
				topOffset = (int) (screenSize.y - topDisplay.height());
				bottomOffset = (int) (screenSize.y - bottomDisplay.height());
			}break;
		}
		topDisplay.offset(0, topOffset);
		bottomDisplay.offset(0, bottomOffset);
	}

	private void adjustVerticalGravity(){
		int height = (topDisplay.height() + bottomDisplay.height());
		int space = 0;
		switch (portraitGravity){
			case Gravity.CENTER:{
				space =  (int) (screenSize.y - height)/2;
			}break;
			case Gravity.BOTTOM:{
				space =  (int) (screenSize.y - height);
			}break;
		}
		topDisplay.offset(0, space);
		bottomDisplay.offset(0,space);
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
