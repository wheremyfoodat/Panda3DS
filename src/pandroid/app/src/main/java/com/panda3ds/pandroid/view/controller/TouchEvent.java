package com.panda3ds.pandroid.view.controller;

public class TouchEvent {
	private final TouchType action;
	private final float x, y;

	public float getX() { return x; }

	public float getY() { return y; }

	public TouchType getAction() { return action; }

	public TouchEvent(float x, float y, TouchType action) {
		this.x = x;
		this.y = y;
		this.action = action;
	}
}
