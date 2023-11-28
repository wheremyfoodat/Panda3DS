package com.panda3ds.pandroid.view.controller;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;
import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.utils.Constants;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;

public class ControllerLayout extends RelativeLayout {
	private final HashMap<Integer, ControllerNode> activeTouchEvents = new HashMap<>();
	private final ArrayList<ControllerNode> controllerNodes = new ArrayList<>();

	public ControllerLayout(Context context) { this(context, null); }

	public ControllerLayout(Context context, AttributeSet attrs) { this(context, attrs, 0); }

	public ControllerLayout(Context context, AttributeSet attrs, int defStyleAttr) { this(context, attrs, defStyleAttr, 0); }

	public ControllerLayout(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	public void refreshChildren() {
		ArrayList<ControllerNode> nodes = new ArrayList<>();
		populateNodesArray(this, nodes);

		// Need Reverse: First view is in back and last view is in front for respect android View hierarchy
		Collections.reverse(nodes);

		controllerNodes.clear();
		controllerNodes.addAll(nodes);
	}

	private void populateNodesArray(ViewGroup group, ArrayList<ControllerNode> list) {
		for (int i = 0; i < group.getChildCount(); i++) {
			View view = group.getChildAt(i);
			if (view instanceof ControllerNode) {
				list.add((ControllerNode) view);
			} else if (view instanceof ViewGroup) {
				populateNodesArray((ViewGroup) view, list);
			}
		}
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		int index = event.getActionIndex();

		switch (event.getActionMasked()) {
			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_CANCEL:
			case MotionEvent.ACTION_POINTER_UP: {
				int id = event.getPointerId(index);
				processTouch(true, event.getX(index), event.getY(index), id);
			} break;
			case MotionEvent.ACTION_DOWN:
			case MotionEvent.ACTION_POINTER_DOWN: {
				int id = event.getPointerId(index);
				processTouch(false, event.getX(index), event.getY(index), id);
			} break;
			case MotionEvent.ACTION_MOVE:
				for (int id = 0; id < event.getPointerCount(); id++) {
					processTouch(false, event.getX(id), event.getY(id), id);
				}
				break;
		}
		return true;
	}

	private void processTouch(boolean up, float x, float y, int index) {
		int[] globalPosition = new int[2];
		getLocationInWindow(globalPosition);

		int action = TouchEvent.ACTION_MOVE;

		if ((!activeTouchEvents.containsKey(index))) {
			if (up) return;
			ControllerNode node = null;
			for (ControllerNode item : controllerNodes) {
				Vector2 pos = item.getPosition();
				Vector2 size = item.getSize();

				float cx = (pos.x - globalPosition[0]);
				float cy = (pos.y - globalPosition[1]);
				if (item.isVisible() && x > cx && x < cx + size.x && y > cy && y < cy + size.y) {
					node = item;
					break;
				}
			}
			if (node != null) {
				activeTouchEvents.put(index, node);
				action = TouchEvent.ACTION_DOWN;
			} else {
				return;
			}
		}

		if (up) action = TouchEvent.ACTION_UP;

		ControllerNode node = activeTouchEvents.get(index);
		Vector2 pos = node.getPosition();
		pos.x -= globalPosition[0];
		pos.y -= globalPosition[1];

		x -= pos.x;
		y -= pos.y;

		node.onTouch(new TouchEvent(x, y, action));

		if (up) {
			activeTouchEvents.remove(index);
		}
	}

	@Override
	public void onViewAdded(View child) {
		super.onViewAdded(child);
		refreshChildren();
	}

	@Override
	public void onViewRemoved(View child) {
		super.onViewRemoved(child);
		refreshChildren();
	}

	/*@TODO: Need replace that methods for prevent Android send events directly to children*/

	@Override
	public ArrayList<View> getTouchables() {
		return new ArrayList<>();
	}

	@Override
	public boolean onInterceptTouchEvent(MotionEvent ev) {
		return true;
	}
}
