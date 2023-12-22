package com.panda3ds.pandroid.data;

import com.google.gson.Gson;
import com.panda3ds.pandroid.lang.Task;
import com.panda3ds.pandroid.utils.FileUtils;

public class GsonConfigParser {
    private final Gson gson = new Gson();
    private final String name;

    public GsonConfigParser(String name){
        this.name = name;
    }

    private String getPath(){
        return FileUtils.getConfigPath()+"/"+name+".json";
    }

    public void save(Object data){
        synchronized (this) {
            new Task(() -> {
                String json = gson.toJson(data, data.getClass());
                FileUtils.writeTextFile(FileUtils.getConfigPath(), name + ".json", json);
            }).runSync();
        }
    }

    public <T> T load(Class<T> clazz){
        String[] content = new String[]{"{}"};
        new Task(()->{
            if (FileUtils.exists(getPath())){
                content[0] = FileUtils.readTextFile(getPath());
            }
        }).runSync();
        return gson.fromJson(content[0], clazz);
    }
}
