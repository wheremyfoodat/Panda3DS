package com.panda3ds.pandroid.input;

import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import com.panda3ds.pandroid.lang.Function;

import java.util.HashMap;

public class InputHandler {
    private static Function<InputEvent> eventListener;
    private static float motionDeadZone = 0.0f;

    private static final int[] gamepadSources = {
            InputDevice.SOURCE_GAMEPAD,
            InputDevice.SOURCE_JOYSTICK
    };

    private static final int[] validSources = {
            InputDevice.SOURCE_GAMEPAD,
            InputDevice.SOURCE_JOYSTICK,
            InputDevice.SOURCE_DPAD,
            InputDevice.SOURCE_KEYBOARD
    };

    private static final HashMap<String, Float> motionDownEvents = new HashMap<>();

    private static boolean containsSource(int[] sources, int sourceMasked) {
        for (int source : sources) {
            if ((sourceMasked & source) == source)
                return true;
        }
        return false;
    }

    private static boolean isGamepadSource(int sourceMask) {
        return containsSource(gamepadSources, sourceMask);
    }

    private static boolean isSourceValid(int sourceMasked) {
        return containsSource(validSources, sourceMasked);
    }

    public static void setEventListener(Function<InputEvent> eventListener) {
        InputHandler.eventListener = eventListener;
    }

    private static void handleEvent(InputEvent event) {
        if (eventListener != null) {
            eventListener.run(event);
        }
    }

    public static void setMotionDeadZone(float motionDeadZone) {
        InputHandler.motionDeadZone = motionDeadZone;
    }

    public static boolean processMotionEvent(MotionEvent event) {
        if (!isSourceValid(event.getSource()))
            return false;

        if (isGamepadSource(event.getSource())) {
            for (InputDevice.MotionRange range : event.getDevice().getMotionRanges()) {
                float axisValue = event.getAxisValue(range.getAxis());
                float value = Math.abs(axisValue);
                String name = (MotionEvent.axisToString(range.getAxis()) + (axisValue >= 0 ? "+" : "-")).toUpperCase();
                String reverseName = (MotionEvent.axisToString(range.getAxis()) + (axisValue >= 0 ? "-" : "+")).toUpperCase();

                if (motionDownEvents.containsKey(reverseName)) {
                    motionDownEvents.remove(reverseName);
                    handleEvent(new InputEvent(reverseName.toUpperCase(), 0.0f));
                }

                if (value > motionDeadZone) {
                    motionDownEvents.put(name, value);
                    handleEvent(new InputEvent(name.toUpperCase(), (value - motionDeadZone) / (1.0f - motionDeadZone)));
                } else if (motionDownEvents.containsKey(name)) {
                    motionDownEvents.remove(name);
                    handleEvent(new InputEvent(name.toUpperCase(), 0.0f));
                }

            }
        }

        return true;
    }

    public static boolean processKeyEvent(KeyEvent event) {
        if (!isSourceValid(event.getSource()))
            return false;

        if (isGamepadSource(event.getSource())) {
            // Dpad return motion event + key event, this remove the key event
            switch (event.getKeyCode()) {
                case KeyEvent.KEYCODE_DPAD_UP:
                case KeyEvent.KEYCODE_DPAD_UP_LEFT:
                case KeyEvent.KEYCODE_DPAD_UP_RIGHT:
                case KeyEvent.KEYCODE_DPAD_DOWN:
                case KeyEvent.KEYCODE_DPAD_DOWN_LEFT:
                case KeyEvent.KEYCODE_DPAD_DOWN_RIGHT:
                case KeyEvent.KEYCODE_DPAD_RIGHT:
                case KeyEvent.KEYCODE_DPAD_LEFT:
                    return true;
            }
        }
        handleEvent(new InputEvent(KeyEvent.keyCodeToString(event.getKeyCode()), event.getAction() == KeyEvent.ACTION_UP ? 0.0f : 1.0f));
        return true;
    }

    public static void reset() {
        eventListener = null;
        motionDeadZone = 0.0f;
        motionDownEvents.clear();
    }
}
