package com.panda3ds.pandroid.view.ds;

import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.view.renderer.layout.ConsoleLayout;

import java.util.ArrayList;

public class DsLayoutManager {
    private static final DataModel data;

    static {
        data = GlobalConfig.getExtra(GlobalConfig.KEY_DS_LAYOUTS, DataModel.class);
        if (data.models.size() == 0){
            setupBasicModels();
        }
    }

    private static void setupBasicModels() {
        Model model1 = new Model();

        Model model2 = new Model();
        model2.mode = Mode.SINGLE;
        model2.singleTop = false;

        Model model3 = new Model();
        model3.mode = Mode.SINGLE;
        model3.singleTop = true;

        data.models.add(new Model[]{model1, model1.clone()});
        data.models.add(new Model[]{model2, model2.clone()});
        data.models.add(new Model[]{model3, model3.clone()});

        save();
    }

    public static synchronized void save(){
        GlobalConfig.putExtra(GlobalConfig.KEY_DS_LAYOUTS, data);
    }

    public static int getLayoutCount(){
        return data.models.size();
    }

    public static ConsoleLayout createLayout(int index){
        index = Math.min(getLayoutCount()-1, index);
        return new DsLayout(data.models.get(index)[0],data.models.get(index)[1]);
    }

    private static class DataModel {
        private final ArrayList<Model[]> models = new ArrayList<>();
    }
}
