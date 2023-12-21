package com.panda3ds.pandroid.data;

import android.graphics.Bitmap;

import com.panda3ds.pandroid.data.game.GameRegion;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class SMDH {
    private static final int ICON_SIZE = 48;
    private static final int META_OFFSET = 0x8 ;
    private static final int META_REGION_OFFSET = 0x2018;
    private static final int IMAGE_OFFSET = 0x24C0;

    private int metaIndex = 1;
    private final ByteBuffer smdh;
    private final String[] title = new String[12];
    private final String[] publisher = new String[12];
    private final int[] icon;

    private final GameRegion region;

    public SMDH(byte[] source){
        smdh = ByteBuffer.allocate(source.length);
        smdh.position(0);
        smdh.put(source);
        smdh.position(0);

        region = parseRegion();
        icon = parseIcon();
        parseMeta();
    }

    private GameRegion parseRegion(){
        GameRegion region;
        smdh.position(META_REGION_OFFSET);

        int regionMasks = smdh.get() & 0xFF;

        final boolean japan = (regionMasks & 0x1) != 0;
        final boolean northAmerica = (regionMasks & 0x2) != 0;
        final boolean europe = (regionMasks & 0x4) != 0;
        final boolean australia = (regionMasks & 0x8) != 0;
        final boolean china = (regionMasks & 0x10) != 0;
        final boolean korea = (regionMasks & 0x20) != 0;

        final boolean taiwan = (regionMasks & 0x40) != 0;
        if (northAmerica) {
            region = GameRegion.NorthAmerican;
        } else if (europe) {
            region = GameRegion.Europe;
        } else if (australia) {
            region = GameRegion.Australia;
        } else if (japan) {
            region = GameRegion.Japan;
            metaIndex = 0;
        } else if (korea) {
            metaIndex = 7;
            region = GameRegion.Korean;
        } else if (china) {
            metaIndex = 6;
            region = GameRegion.China;
        } else if (taiwan) {
            metaIndex = 6;
            region = GameRegion.Taiwan;
        } else {
            region = GameRegion.None;
        }

        return region;
    }

    private void parseMeta(){

        for (int i = 0; i < 12; i++){
            smdh.position(META_OFFSET + (512*i) + 0x80);
            byte[] data = new byte[0x100];
            smdh.get(data);
            title[i] = convertString(data).replaceAll("\n", " ");
        }

        for (int i = 0; i < 12; i++){
            smdh.position(META_OFFSET + (512 * i) + 0x180);
            byte[] data = new byte[0x80];
            smdh.get(data);
            publisher[i] = convertString(data);
        }
    }

    private int[] parseIcon() {
        int[] icon = new int[ICON_SIZE*ICON_SIZE];
        smdh.position(0);

        for (int x = 0; x < ICON_SIZE; x++) {
            for (int y = 0; y < ICON_SIZE; y++) {
                int curseY = y & ~7;
                int curseX = x & ~7;

                int i = mortonInterleave(x, y);
                int offset = (i + (curseX * 8)) * 2;

                offset = offset + curseY * 48 * 2;

                smdh.position(offset + IMAGE_OFFSET);

                int bit1 = smdh.get() & 0xFF;
                int bit2 = smdh.get() & 0xFF;

                int pixel = bit1 + (bit2 << 8);

                int r = (((pixel & 0xF800) >> 11) << 3);
                int g = (((pixel & 0x7E0) >> 5) << 2);
                int b = (((pixel & 0x1F)) << 3);

                //Convert to ARGB8888
                icon[x + 48 * y] = 255 << 24 | (r & 255) << 16 | (g & 255) << 8 | (b & 255);
            }
        }
        return icon;
    }


    public GameRegion getRegion() {
        return region;
    }

    public Bitmap getBitmapIcon(){
        Bitmap bitmap = Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.RGB_565);
        bitmap.setPixels(icon,0,ICON_SIZE,0,0,ICON_SIZE,ICON_SIZE);
        return bitmap;
    }

    public int[] getIcon() {
        return icon;
    }

    public String getTitle(){
        return title[metaIndex];
    }

    public String getPublisher(){
        return publisher[metaIndex];
    }

    // SMDH stores string in UTF-16LE format
    private static String convertString(byte[] buffer){
        try {
            return new String(buffer,0, buffer.length, StandardCharsets.UTF_16LE)
                    .replaceAll("\0","");
        } catch (Exception e){
            return "";
        }
    }

    // u and v are the UVs of the relevant texel
    // Texture data is stored interleaved in Morton order, ie in a Z - order curve as shown here
    // https://en.wikipedia.org/wiki/Z-order_curve
    // Textures are split into 8x8 tiles.This function returns the in - tile offset depending on the u & v of the texel
    // The in - tile offset is the sum of 2 offsets, one depending on the value of u % 8 and the other on the value of y % 8
    // As documented in this picture https ://en.wikipedia.org/wiki/File:Moser%E2%80%93de_Bruijn_addition.svg
    private static int mortonInterleave(int u, int v) {
        int[] xlut = {0, 1, 4, 5, 16, 17, 20, 21};
        int[] ylut = {0, 2, 8, 10, 32, 34, 40, 42};
        return xlut[u % 8] + ylut[v % 8];
    }
}
