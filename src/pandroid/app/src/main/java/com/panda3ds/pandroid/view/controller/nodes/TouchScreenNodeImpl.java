package com.panda3ds.pandroid.view.controller.nodes;

import android.graphics.Rect;
import android.view.View;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.view.controller.ControllerNode;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.controller.TouchType;
import com.panda3ds.pandroid.view.renderer.ConsoleRenderer;

public interface TouchScreenNodeImpl extends ControllerNode {
	default void onTouchScreenPress(ConsoleRenderer renderer, TouchEvent event) {
		View view = (View) this;
		boolean hasDownEvent = view.getTag(R.id.TagEventHasDown) != null && (boolean) view.getTag(R.id.TagEventHasDown);

		Rect bounds = renderer.getLayout().getBottomDisplayBounds();

		if (event.getX() >= bounds.left && event.getY() >= bounds.top && event.getX() <= bounds.right && event.getY() <= bounds.bottom) {
			int x = (int) (event.getX() - bounds.left);
			int y = (int) (event.getY() - bounds.top);

			x = Math.round((x / (float) bounds.width()) * 320);
			y = Math.round((y / (float) bounds.height()) * 240);

			AlberDriver.TouchScreenDown(x, y);

			view.setTag(R.id.TagEventHasDown, true);
		}

		if (hasDownEvent && event.getAction() == TouchType.ACTION_UP) {
			AlberDriver.TouchScreenUp();
			view.setTag(R.id.TagEventHasDown, false);
		}
	}
}
