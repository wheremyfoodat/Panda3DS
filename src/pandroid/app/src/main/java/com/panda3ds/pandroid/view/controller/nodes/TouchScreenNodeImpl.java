package com.panda3ds.pandroid.view.controller.nodes;

import android.graphics.Rect;
import android.util.Log;
import android.view.View;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.controller.ControllerNode;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.renderer.ConsoleRenderer;

public interface TouchScreenNodeImpl extends ControllerNode {
	default void onTouchScreenPress(ConsoleRenderer renderer, TouchEvent event) {
		View me = (View) this;
		boolean hasDownEvent = me.getTag(R.id.TagEventHasDown) != null && (boolean) me.getTag(R.id.TagEventHasDown);

		Rect bounds = renderer.getLayout().getBottomDisplayBounds();
		;

		if (event.getX() >= bounds.left && event.getY() >= bounds.top && event.getX() <= bounds.right && event.getY() <= bounds.bottom) {
			int x = (int) (event.getX() - bounds.left);
			int y = (int) (event.getY() - bounds.top);

			x = Math.round((x / (float) bounds.width()) * 320);
			y = Math.round((y / (float) bounds.height()) * 240);

			AlberDriver.TouchScreenDown(x, y);

			me.setTag(R.id.TagEventHasDown, true);
		}

		if (hasDownEvent && event.getAction() == TouchEvent.ACTION_UP) {
			AlberDriver.TouchScreenUp();
			me.setTag(R.id.TagEventHasDown, false);
		}
	}
}
