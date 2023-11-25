package com.panda3ds.pandroid.view.controller.listeners;

import com.panda3ds.pandroid.view.controller.nodes.Button;

public interface ButtonStateListener {
    void onButtonPressedChange(Button button, boolean pressed);
}
