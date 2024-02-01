package com.panda3ds.pandroid.view.code;

import android.content.Context;

import com.panda3ds.pandroid.app.PandroidApplication;

public class EditorColors {
    public static final byte COLOR_TEXT = 0x0;
    public static final byte COLOR_KEYWORDS = 0x1;
    public static final byte COLOR_NUMBERS = 0x2;
    public static final byte COLOR_STRING = 0x3;
    public static final byte COLOR_METADATA = 0x4;
    public static final byte COLOR_COMMENT = 0x5;
    public static final byte COLOR_SYMBOLS = 0x6;
    public static final byte COLOR_FIELDS = 0x7;
    public static final byte COLOR_BACKGROUND = 0x1D;
    public static final byte COLOR_BACKGROUND_SECONDARY = 0x2D;
    public static final byte COLOR_SELECTION = 0x3D;
    public static final byte COLOR_CARET = 0x4D;
    public static final byte COLOR_CURRENT_LINE = 0x5D;

    public static void obtainColorScheme(int[] colors, Context context) {
        if (PandroidApplication.isDarkMode()) {
            applyDarkTheme(colors);
        } else {
            applyLightTheme(colors);
        }
    }

    private static void applyLightTheme(int[] colors) {
        colors[EditorColors.COLOR_TEXT] = 0xFF000000;
        colors[EditorColors.COLOR_KEYWORDS] = 0xFF3AE666;
        colors[EditorColors.COLOR_NUMBERS] = 0xFF3A9EE6;
        colors[EditorColors.COLOR_METADATA] = 0xFF806AE6;
        colors[EditorColors.COLOR_SYMBOLS] = 0xFF202020;
        colors[EditorColors.COLOR_STRING] = 0xFF2EB541;
        colors[EditorColors.COLOR_FIELDS] = 0xFF9876AA;
        colors[EditorColors.COLOR_COMMENT] = 0xFF808080;

        colors[EditorColors.COLOR_BACKGROUND] = 0xFFFFFFFF;
        colors[EditorColors.COLOR_BACKGROUND_SECONDARY] = 0xFFF0F0F0;
        colors[EditorColors.COLOR_SELECTION] = 0x701F9EDE;
        colors[EditorColors.COLOR_CARET] = 0xFF000000;
        colors[EditorColors.COLOR_CURRENT_LINE] = 0x05000050;
    }

    private static void applyDarkTheme(int[] colors) {
        colors[EditorColors.COLOR_TEXT] = 0xFFFFFFFF;
        colors[EditorColors.COLOR_KEYWORDS] = 0xFFE37F3E;
        colors[EditorColors.COLOR_NUMBERS] = 0xFF3A9EE6;
        colors[EditorColors.COLOR_METADATA] = 0xFFC5CA1D;
        colors[EditorColors.COLOR_SYMBOLS] = 0xFFC0C0C0;
        colors[EditorColors.COLOR_STRING] = 0xFF2EB541;
        colors[EditorColors.COLOR_FIELDS] = 0xFF9876AA;
        colors[EditorColors.COLOR_COMMENT] = 0xFFBBBBBB;

        colors[EditorColors.COLOR_BACKGROUND] = 0xFF2B2B2B;
        colors[EditorColors.COLOR_BACKGROUND_SECONDARY] = 0xFF313335;
        colors[EditorColors.COLOR_SELECTION] = 0x701F9EDE;
        colors[EditorColors.COLOR_CARET] = 0x60FFFFFF;
        colors[EditorColors.COLOR_CURRENT_LINE] = 0x10FFFFFF;
    }
}