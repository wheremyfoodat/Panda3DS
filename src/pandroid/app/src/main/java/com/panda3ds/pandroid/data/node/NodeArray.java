package com.panda3ds.pandroid.data.node;

import androidx.annotation.NonNull;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

public class NodeArray extends NodeBase {
    public static final String EMPTY_SOURCE = "[]";

    private final ArrayList<Object> list = new ArrayList<>();

    NodeArray(JSONArray array) {
        init(array);
    }

    public NodeArray() {
        this(EMPTY_SOURCE);
    }

    public NodeArray(String source) {
        try {
            init(new JSONArray(source));
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }

    private void init(JSONArray array) {
        try {
            for (int i = 0; i < array.length(); i++) {
                Object item = array.get(i);
                if (item instanceof JSONArray) {
                    item = new NodeArray((JSONArray) item);
                    ((NodeArray) item).setParent(this);
                } else if (item instanceof JSONObject) {
                    item = new NodeObject((JSONObject) item);
                    ((NodeObject) item).setParent(this);
                }
                list.add(item);
            }
        } catch (Exception e) {
            throw new IllegalArgumentException(e);
        }
    }

    private void add(Object obj) {
        list.add(obj);
        changed();
    }

    public String getString(int index) {
        return (String) list.get(index);
    }

    public int getInteger(int index) {
        return Caster.intValue(list.get(index));
    }

    public long getLong(int index) {
        return Caster.longValue(list.get(index));
    }

    public boolean getBoolean(int index) {
        return (boolean) list.get(index);
    }

    public double getDouble(int index) {
        return Caster.doubleValue(list.get(index));
    }

    public NodeArray getArray(int index) {
        return (NodeArray) list.get(index);
    }

    public NodeObject getObject(int index) {
        return (NodeObject) list.get(index);
    }

    public void add(String val) {
        list.add(val);
    }

    public void add(int val) {
        add((Object) val);
    }

    public void add(long val) {
        add((Object) val);
    }

    public void add(double val) {
        add((Object) val);
    }

    public void add(boolean val) {
        add((Object) val);
    }

    public void add(NodeBase val) {
        add((Object) val);
    }

    public int getSize() {
        return list.size();
    }

    @NonNull
    @Override
    public String toString() {
        try {
            return ((JSONArray) buildValue()).toString(4);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected Object buildValue() {
        JSONArray array = new JSONArray();
        for (Object obj : list) {
            if (obj instanceof NodeBase) {
                array.put(((NodeBase) obj).buildValue());
            } else {
                array.put(obj);
            }
        }
        return array;
    }
}
