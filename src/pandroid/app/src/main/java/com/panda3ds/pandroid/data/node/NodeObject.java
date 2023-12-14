package com.panda3ds.pandroid.data.node;

import androidx.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Iterator;

public class NodeObject extends NodeBase {

    public static final String EMPTY_SOURCE = "{}";
    private final HashMap<String, Object> map = new HashMap<>();

    NodeObject(JSONObject obj) {
        init(obj);
    }

    public NodeObject() {
        this(EMPTY_SOURCE);
    }

    public NodeObject(String source) {
        try {
            init(new JSONObject(source));
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }

    private void init(JSONObject base) {
        try {
            Iterator<String> keys = base.keys();
            while (keys.hasNext()) {
                String key = keys.next();
                Object item = base.get(key);
                if (item instanceof JSONObject) {
                    item = new NodeObject((JSONObject) item);
                    ((NodeObject) item).setParent(this);
                } else if (item instanceof JSONArray) {
                    item = new NodeArray((JSONArray) item);
                    ((NodeArray) item).setParent(this);
                }
                map.put(key, item);
            }
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        }
    }

    private Object get(String key, Object def) {
        if (!has(key))
            return def;

        return map.get(key);
    }

    private void put(String key, Object value) {
        map.put(key, value);
        changed();
    }

    public String getString(String key, String def) {
        return (String) get(key, def);
    }

    public int getInteger(String key, int def) {
        return Caster.intValue(get(key, def));
    }

    public long getLong(String key, long def) {
        return Caster.longValue(get(key, def));
    }

    public boolean getBoolean(String key, boolean def) {
        return (boolean) get(key, def);
    }

    public double getDouble(String key, double def) {
        return Caster.doubleValue(get(key, def));
    }

    public NodeArray getArray(String key, NodeArray def) {
        return (NodeArray) get(key, def);
    }

    public NodeObject getObject(String key, NodeObject def) {
        return (NodeObject) get(key, def);
    }

    public void put(String key, String val) {
        put(key, (Object) val);
    }

    public void put(String key, int val) {
        put(key, (Object) val);
    }

    public void put(String key, double val) {
        put(key, (Object) val);
    }

    public void put(String key, boolean val) {
        put(key, (Object) val);
    }

    public void put(String key, NodeBase val) {
        put(key, (Object) val);
    }

    public boolean has(String key) {
        return map.containsKey(key);
    }

    public void remove(String key) {
        map.remove(key);
        changed();
    }


    public void clear() {
        map.clear();
        changed();
    }

    public String[] getKeys() {
        return map.keySet().toArray(new String[0]);
    }


    public int getSize() {
        return map.size();
    }

    @NonNull
    @Override
    public String toString() {
        try {
            return ((JSONObject) buildValue()).toString(4);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected Object buildValue() {
        try {
            JSONObject dest = new JSONObject();
            for (String key : getKeys()) {
                Object obj = map.get(key);
                if (obj instanceof NodeBase) {
                    obj = ((NodeBase) obj).buildValue();
                }
                dest.put(key, obj);
            }

            return dest;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
