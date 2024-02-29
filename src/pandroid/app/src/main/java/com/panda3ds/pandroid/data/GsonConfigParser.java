package com.panda3ds.pandroid.data;

import com.google.gson.ExclusionStrategy;
import com.google.gson.FieldAttributes;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.panda3ds.pandroid.lang.Task;
import com.panda3ds.pandroid.utils.FileUtils;

public class GsonConfigParser {
    private final Gson gson = new GsonBuilder().setPrettyPrinting().create();
    private final String name;

    public GsonConfigParser(String name) {
        this.name = name;
    }

    private String getPath() {
        return FileUtils.getConfigPath()+ "/" + name + ".json";
    }

    public void save(Object data) {
        synchronized (this) {
            new Task(() -> {
                String json = gson.toJson(data, data.getClass());
                FileUtils.writeTextFile(FileUtils.getConfigPath(), name + ".json", json);
            }).runSync();
        }
    }

    public <T> T load(Class<T> myClass) {
        String[] content = new String[] {"{}"};
        new Task(()->{
            if (FileUtils.exists(getPath())) {
                String src = FileUtils.readTextFile(getPath());
                if(src != null && src.length() > 2){
                    content[0] = src;
                }
            }
        }).runSync();

        return gson.fromJson(content[0], myClass);
    }
}
