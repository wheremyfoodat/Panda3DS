package com.panda3ds.pandroid.view;

import android.content.Context;
import android.util.AttributeSet;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.view.controller.ControllerLayout;
import com.panda3ds.pandroid.view.controller.map.ControllerProfileManager;
import com.panda3ds.pandroid.view.controller.map.NodeID;
import com.panda3ds.pandroid.view.controller.map.Profile;
import com.panda3ds.pandroid.view.controller.nodes.Button;
import com.panda3ds.pandroid.view.controller.nodes.Joystick;

public class PandaLayoutController extends ControllerLayout {

	private int width = -1;
	private int height = -1;

	public PandaLayoutController(Context context) { super(context); }
	public PandaLayoutController(Context context, AttributeSet attrs) { super(context, attrs); }
	public PandaLayoutController(Context context, AttributeSet attrs, int defStyleAttr) { super(context, attrs, defStyleAttr); }

	public PandaLayoutController(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	public void initialize() {
		int[] keyButtonList = {R.id.button_a,     Constants.INPUT_KEY_A,     R.id.button_b,      Constants.INPUT_KEY_B,
							   R.id.button_y,     Constants.INPUT_KEY_Y,     R.id.button_x,      Constants.INPUT_KEY_X,

							   R.id.button_left,  Constants.INPUT_KEY_LEFT,  R.id.button_right,  Constants.INPUT_KEY_RIGHT,
							   R.id.button_up,    Constants.INPUT_KEY_UP,    R.id.button_down,   Constants.INPUT_KEY_DOWN,

							   R.id.button_start, Constants.INPUT_KEY_START, R.id.button_select, Constants.INPUT_KEY_SELECT,

							   R.id.button_l,     Constants.INPUT_KEY_L,     R.id.button_r,      Constants.INPUT_KEY_R};

		for (int i = 0; i < keyButtonList.length; i += 2) {
			final int keyCode = keyButtonList[i + 1];
			((Button) findViewById(keyButtonList[i])).setStateListener((btn, pressed) -> {
				if (pressed)
					AlberDriver.KeyDown(keyCode);
				else
					AlberDriver.KeyUp(keyCode);
			});
		}

		((Joystick) findViewById(R.id.left_analog)).setJoystickListener((joystick, axisX, axisY) -> {
			AlberDriver.SetCirclepadAxis((int) (axisX * 0x9C), (int) (axisY * 0x9C) * -1);
		});

		refreshChildren();
		measure(MeasureSpec.EXACTLY, MeasureSpec.EXACTLY);
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
		int measuredWidth = getMeasuredWidth();
		int measuredHeight = getMeasuredHeight();
		if (measuredWidth != width || measuredHeight != height){
			width = measuredWidth;
			height = measuredHeight;
			applyProfileMap();
		}
	}

	private void applyProfileMap() {
		Profile profile = ControllerProfileManager.getDefaultProfile();
		profile.applyToView(NodeID.L,findViewById(R.id.button_l), width, height);
		profile.applyToView(NodeID.R, findViewById(R.id.button_r), width, height);
		profile.applyToView(NodeID.START, findViewById(R.id.button_start), width, height);
		profile.applyToView(NodeID.SELECT, findViewById(R.id.button_select), width, height);
		profile.applyToView(NodeID.JOYSTICK, findViewById(R.id.left_analog), width, height);
		profile.applyToView(NodeID.GAMEPAD, findViewById(R.id.gamepad), width, height);
		profile.applyToView(NodeID.DPAD, findViewById(R.id.dpad), width, height);
	}
}
