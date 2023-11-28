package com.panda3ds.pandroid.view.controller.nodes;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatTextView;
import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.view.controller.ControllerNode;
import com.panda3ds.pandroid.view.controller.TouchEvent;
import com.panda3ds.pandroid.view.controller.listeners.JoystickListener;

public class Joystick extends BasicControllerNode implements ControllerNode {
	private float stick_x = 0;
	private float stick_y = 0;

	private int size_width = 0;
	private int size_height = 0;

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
		size_width = getWidth();
		size_height = getHeight();

		int analogIconSize = size_width - getPaddingLeft();

		float middleIconSize = analogIconSize / 2.0F;
		float middle = size_width / 2.0F;

		float maxDistance = (middle - middleIconSize) * 0.9F;

		float tx = maxDistance * stick_x;
		float ty = maxDistance * stick_y;

		float radius = Vector2.distance(0.0F, 0.0F, Math.abs(tx), Math.abs(ty));
		radius = Math.min(maxDistance, radius);

		double deg = Math.atan2(ty, tx) * (180.0 / Math.PI);
		float rx = (float) (radius * Math.cos(Math.PI * 2 * deg / 360.0));
		float ry = (float) (radius * Math.sin(Math.PI * 2 * deg / 360.0));

		stick_x = Math.max(-1.0f, Math.min(1.0f, stick_x));
		stick_y = Math.max(-1.0f, Math.min(1.0f, stick_y));

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

	public Vector2 getAxis() { return new Vector2(Math.max(-1.0F, Math.min(1.0F, stick_x)), Math.max(-1.0F, Math.min(1.0F, stick_y))); }

	public void setJoystickListener(JoystickListener joystickListener) { this.joystickListener = joystickListener; }

	@NonNull
	@Override
	public Vector2 getSize() {
		return new Vector2(size_width, size_height);
	}

	@Override
	public void onTouch(TouchEvent event) {
		float middle = size_width / 2.0F;

		float x = event.getX();
		float y = event.getY();

		x = Math.max(0, Math.min(middle * 2, x));
		y = Math.max(0, Math.min(middle * 2, y));

		stick_x = ((x - middle) / middle);

		stick_y = ((y - middle) / middle);

		if (event.getAction() == TouchEvent.ACTION_UP) {
			stick_x = 0;
			stick_y = 0;
		}

		if (joystickListener != null) {
			joystickListener.onJoystickAxisChange(this, stick_x, stick_y);
		}

		invalidate();
	}
}
