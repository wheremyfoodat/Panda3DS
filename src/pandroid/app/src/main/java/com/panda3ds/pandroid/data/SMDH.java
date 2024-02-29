package com.panda3ds.pandroid.data;

import android.graphics.Bitmap;
import android.graphics.Color;

import com.panda3ds.pandroid.data.game.GameRegion;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class SMDH {
    public static final int LANGUAGE_JAPANESE = 0;
    public static final int LANGUAGE_ENGLISH = 1;
    public static final int LANGUAGE_CHINESE = 6;
    public static final int LANGUAGE_KOREAN = 7;

    public static final int REGION_JAPAN_MASK = 0x1;
    public static final int REGION_NORTH_AMERICAN_MASK = 0x2;
    public static final int REGION_EUROPE_MASK = 0x4;
    public static final int REGION_AUSTRALIA_MASK = 0x8;
    public static final int REGION_CHINA_MASK = 0x10;
    public static final int REGION_KOREAN_MASK = 0x20;
    public static final int REGION_TAIWAN_MASK = 0x40;

    private static final int ICON_SIZE = 48;
    private static final int META_OFFSET = 0x8;
    private static final int META_REGION_OFFSET = 0x2018;
    private static final int IMAGE_OFFSET = 0x24C0;

    private int metaLanguage = LANGUAGE_ENGLISH;
    private final ByteBuffer smdh;
    private final String[] title = new String[12];
    private final String[] publisher = new String[12];
    private final int[] icon;

    private final GameRegion region;

    public SMDH(byte[] source) {
        smdh = ByteBuffer.allocate(source.length);
        smdh.position(0);
        smdh.put(source);
        smdh.position(0);

        region = parseRegion();
        icon = parseIcon();
        parseMeta();
    }

    private GameRegion parseRegion() {
        GameRegion region;
        smdh.position(META_REGION_OFFSET);

        int regionMasks = smdh.get() & 0xFF;

        final boolean japan = (regionMasks & REGION_JAPAN_MASK) != 0;
        final boolean northAmerica = (regionMasks & REGION_NORTH_AMERICAN_MASK) != 0;
        final boolean europe = (regionMasks & REGION_EUROPE_MASK) != 0;
        final boolean australia = (regionMasks & REGION_AUSTRALIA_MASK) != 0;
        final boolean china = (regionMasks & REGION_CHINA_MASK) != 0;
        final boolean korea = (regionMasks & REGION_KOREAN_MASK) != 0;
        final boolean taiwan = (regionMasks & REGION_TAIWAN_MASK) != 0;

        // Depending on the regions allowed in the region mask, pick one of the regions to use
        // We prioritize English-speaking regions both here and in the emulator core, since users are most likely to speak English at least
        if (northAmerica) {
            region = GameRegion.NorthAmerican;
        } else if (europe) {
            region = GameRegion.Europe;
        } else if (australia) {
            region = GameRegion.Australia;
        } else if (japan) {
            region = GameRegion.Japan;
            metaLanguage = LANGUAGE_JAPANESE;
        } else if (korea) {
            region = GameRegion.Korean;
            metaLanguage = LANGUAGE_KOREAN;
        } else if (china) {
            region = GameRegion.China;
            metaLanguage = LANGUAGE_CHINESE;
        } else if (taiwan) {
            region = GameRegion.Taiwan;
            metaLanguage = LANGUAGE_CHINESE;
        } else {
            region = GameRegion.None;
        }

        return region;
    }

    private void parseMeta() {
        byte[] data;
        for (int i = 0; i < 12; i++) {
            smdh.position(META_OFFSET + (512 * i) + 0x80);
            data = new byte[0x100];
            smdh.get(data);
            title[i] = convertString(data);

            smdh.position(META_OFFSET + (512 * i) + 0x180);
            data = new byte[0x80];
            smdh.get(data);
            publisher[i] = convertString(data);
        }
    }

    // The icons are stored in RGB562 but android need RGB888
    private int[] parseIcon() {
        int[] icon = new int[ICON_SIZE * ICON_SIZE];
        smdh.position(0);

        for (int x = 0; x < ICON_SIZE; x++) {
            for (int y = 0; y < ICON_SIZE; y++) {
                int indexY = y & ~7;
                int indexX = x & ~7;

                int interleave = mortonInterleave(x, y);
                int offset = (interleave + (indexX * 8)) * 2;

                offset = offset + indexY * ICON_SIZE * 2;

                smdh.position(offset + IMAGE_OFFSET);

                int lowByte = smdh.get() & 0xFF;
                int highByte = smdh.get() & 0xFF;
                int texel = (highByte << 8) | lowByte;

                // Convert texel from RGB565 to RGB888
                int r = (texel >> 11) & 0x1F;
                int g = (texel >> 5) & 0x3F;
                int b = texel & 0x1F;

                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);

                icon[x + ICON_SIZE * y] = Color.rgb(r, g, b);
            }
        }
    
        return icon;
    }


    public GameRegion getRegion() {
        return region;
    }

    public Bitmap getBitmapIcon() {
        Bitmap bitmap = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.RGB_565);
        bitmap.setPixels(icon, 0, ICON_SIZE, 0, 0, ICON_SIZE, ICON_SIZE);
        return bitmap;
    }

    public int[] getIcon() {
        return icon;
    }

    public String getTitle() {
        return title[metaLanguage];
    }

    public String getPublisher() {
        return publisher[metaLanguage];
    }

    // Strings in SMDH files are stored as UTF-16LE
    private static String convertString(byte[] buffer) {
        try {
            return new String(buffer, 0, buffer.length, StandardCharsets.UTF_16LE)
                    .replaceAll("\0", "");
        } catch (Exception e) {
            return "";
        }
    }

    // Reference: https://github.com/wheremyfoodat/Panda3DS/blob/master/src/core/renderer_gl/textures.cpp#L88
    private static int mortonInterleave(int u, int v) {
        int[] xlut = {0, 1, 4, 5, 16, 17, 20, 21};
        int[] ylut = {0, 2, 8, 10, 32, 34, 40, 42};

        return xlut[u % 8] + ylut[v % 8];
    }
}
