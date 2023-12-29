package com.panda3ds.pandroid.app.game;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.input.InputEvent;
import com.panda3ds.pandroid.input.InputMap;
import com.panda3ds.pandroid.input.KeyName;
import com.panda3ds.pandroid.lang.Function;
import com.panda3ds.pandroid.math.Vector2;

import java.util.Objects;


public class AlberInputListener implements Function<InputEvent> {
	private final Runnable backListener;
	public AlberInputListener(Runnable backListener) { this.backListener = backListener;  }

	private final Vector2 axis = new Vector2(0.0f, 0.0f);

	@Override
	public void run(InputEvent event) {
		KeyName key = InputMap.relative(event.getName());

		if (Objects.equals(event.getName(), "KEYCODE_BACK")) {
			backListener.run();
			return;
		}

		if (key == KeyName.NULL) {
			return;
		}

		boolean axisChanged = false;

		switch (key) {
			case AXIS_UP:
				axis.y = event.getValue();
				axisChanged = true;
				break;
			case AXIS_DOWN:
				axis.y = -event.getValue();
				axisChanged = true;
				break;
			case AXIS_LEFT:
				axis.x = -event.getValue();
				axisChanged = true;
				break;
			case AXIS_RIGHT:
				axis.x = event.getValue();
				axisChanged = true;
				break;
			default:
				if (event.isDown()) {
					AlberDriver.KeyDown(key.getKeyId());
				} else {
					AlberDriver.KeyUp(key.getKeyId());
				}
				break;
		}

		if (axisChanged) {
			AlberDriver.SetCirclepadAxis(Math.round(axis.x * 0x9C), Math.round(axis.y * 0x9C));
		}
	}
}
