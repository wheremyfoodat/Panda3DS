package com.panda3ds.pandroid.view;

import android.text.Editable;
import android.text.TextWatcher;

public interface SimpleTextWatcher extends TextWatcher {
    void onChange(String value);

    @Override
    default void onTextChanged(CharSequence s, int start, int before, int count) {}

    @Override
    default void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    default void afterTextChanged(Editable s){
        onChange(s.toString());
    }
}
