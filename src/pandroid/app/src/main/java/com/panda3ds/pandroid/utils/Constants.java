package com.panda3ds.pandroid.utils;

public class Constants {
	public static final int INPUT_KEY_A = 1 << 0;
	public static final int INPUT_KEY_B = 1 << 1;
	public static final int INPUT_KEY_SELECT = 1 << 2;
	public static final int INPUT_KEY_START = 1 << 3;
	public static final int INPUT_KEY_RIGHT = 1 << 4;
	public static final int INPUT_KEY_LEFT = 1 << 5;
	public static final int INPUT_KEY_UP = 1 << 6;
	public static final int INPUT_KEY_DOWN = 1 << 7;
	public static final int INPUT_KEY_R = 1 << 8;
	public static final int INPUT_KEY_L = 1 << 9;
	public static final int INPUT_KEY_X = 1 << 10;
	public static final int INPUT_KEY_Y = 1 << 11;

	public static final int N3DS_WIDTH = 400;
	public static final int N3DS_FULL_HEIGHT = 480;
	public static final int N3DS_HALF_HEIGHT = N3DS_FULL_HEIGHT / 2;

	public static final String ACTIVITY_PARAMETER_PATH = "path";
	public static final String ACTIVITY_PARAMETER_FRAGMENT = "fragment";
	public static final String LOG_TAG = "pandroid";

	public static final String PREF_GLOBAL_CONFIG = "app.GlobalConfig";
    public static final String PREF_GAME_UTILS = "app.GameUtils";
    public static final String PREF_INPUT_MAP = "app.InputMap";
    public static final String PREF_SCREEN_CONTROLLER_PROFILES = "app.input.ScreenControllerManager";
    public static final String RESOURCE_FOLDER_ELF = "ELF"; // Folder for caching ELF files
    public static final String RESOURCE_FOLDER_LUA_SCRIPTS = "Lua Scripts";
}
