package com.panda3ds.pandroid.view.ds;

import android.util.Log;
import android.view.Gravity;

import androidx.annotation.NonNull;

import com.panda3ds.pandroid.utils.Constants;

class Model implements Cloneable {
    public Mode mode = Mode.RELATIVE;
    public final Bounds preferredTop = new Bounds();
    public final Bounds preferredBottom = new Bounds();
    public boolean reverse = false;
    public boolean singleTop = true;
    public float space = 0.6f;
    public int gravity = Gravity.CENTER;
    public boolean lockAspect = true;

    @NonNull
    @Override
    public Model clone() {
        try {
            return (Model) super.clone();
        } catch (Exception e){
            Log.e(Constants.LOG_TAG, "Error on clone DsModel!", e);
            return new Model();
        }
    }
}
