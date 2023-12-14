package com.panda3ds.pandroid.data.node;

/**
 * JAVA THINGS:
 * Java allow cast primary (int, double, float and long)
 * but crash on cast object number why!!
 **/
class Caster {
    public static int intValue(Object value){
        if (value instanceof Number){
            return ((Number)value).intValue();
        } else {
            return Integer.parseInt((String) value);
        }
    }

    public static float floatValue(Object value){
        if (value instanceof Number){
            return ((Number)value).floatValue();
        } else {
            return Float.parseFloat((String) value);
        }
    }

    public static long longValue(Object value){
        if (value instanceof Number){
            return ((Number)value).longValue();
        } else {
            return Long.parseLong((String) value);
        }
    }

    public static double doubleValue(Object value){
        if (value instanceof Number){
            return ((Number)value).doubleValue();
        } else {
            return Double.parseDouble((String) value);
        }
    }
}
