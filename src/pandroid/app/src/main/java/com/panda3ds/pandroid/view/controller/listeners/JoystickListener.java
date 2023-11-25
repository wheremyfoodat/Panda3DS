package com.panda3ds.pandroid.view.controller.listeners;

import com.panda3ds.pandroid.view.controller.nodes.Joystick;

public interface JoystickListener {
    void onJoystickAxisChange(Joystick joystick, float axisX, float axisY);
}
