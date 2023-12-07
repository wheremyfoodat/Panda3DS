package com.panda3ds.pandroid.view.controller.nodes;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import androidx.annotation.NonNull;

import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.view.controller.ControllerNode;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.controller.listeners.JoystickListener;

public class Joystick extends BasicControllerNode implements ControllerNode {
	private float axisX = 0;
	private float axisY = 0;

	private int width = 0;
	private int height = 0;

	private JoystickListener joystickListener;

	public Joystick(Context context) { this(context, null); }

	public Joystick(Context context, AttributeSet attrs) { this(context, attrs, 0); }

	public Joystick(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);

		paint.setColor(Color.RED);
		invalidate();
	}

	private final Paint paint = new Paint();

	@Override
	protected void onDraw(Canvas canvas) {
		super.onDraw(canvas);
	}

	@Override
	public void onDrawForeground(Canvas canvas) {
		width = getWidth();
		height = getHeight();

		int analogIconSize = width - getPaddingLeft();

		float middleIconSize = analogIconSize / 2.0F;
		float middle = width / 2.0F;

		float maxDistance = (middle - middleIconSize) * 0.9F;

		float tx = maxDistance * axisX;
		float ty = maxDistance * axisY;

		float radius = Vector2.distance(0.0F, 0.0F, Math.abs(tx), Math.abs(ty));
		radius = Math.min(maxDistance, radius);

		double deg = Math.atan2(ty, tx) * (180.0 / Math.PI);
		float rx = (float) (radius * Math.cos(Math.PI * 2 * deg / 360.0));
		float ry = (float) (radius * Math.sin(Math.PI * 2 * deg / 360.0));

		axisX = Math.max(-1.0f, Math.min(1.0f, axisX));
		axisY = Math.max(-1.0f, Math.min(1.0f, axisY));

		float x = middle - middleIconSize + rx;
		float y = middle - middleIconSize + ry;

		Drawable foreground = getForeground();
		if (foreground != null) {
			foreground.setBounds((int) x, (int) y, (int) (x + analogIconSize), (int) (y + analogIconSize));
			foreground.draw(canvas);
		} else {
			canvas.drawOval(x, y, x + analogIconSize, y + analogIconSize, paint);
		}
	}

	public Vector2 getAxis() { return new Vector2(Math.max(-1.0F, Math.min(1.0F, axisX)), Math.max(-1.0F, Math.min(1.0F, axisY))); }

	public void setJoystickListener(JoystickListener joystickListener) { this.joystickListener = joystickListener; }

	@NonNull
	@Override
	public Vector2 getSize() {
		return new Vector2(width, height);
	}

	@Override
	public void onTouch(TouchEvent event) {
		float middle = width / 2.0F;

		float x = event.getX();
		float y = event.getY();

		x = Math.max(0, Math.min(middle * 2, x));
		y = Math.max(0, Math.min(middle * 2, y));

		axisX = ((x - middle) / middle);

		axisY = ((y - middle) / middle);

		if (event.getAction() == TouchEvent.ACTION_UP) {
			axisX = 0;
			axisY = 0;
		}

		if (joystickListener != null) {
			joystickListener.onJoystickAxisChange(this, axisX, axisY);
		}

		invalidate();
	}
}
