package com.panda3ds.pandroid.app.base;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.AppCompatTextView;

import com.panda3ds.pandroid.R;

public class LoadingAlertDialog extends BottomAlertDialog {
    public LoadingAlertDialog(@NonNull Context context, @StringRes int title) {
        super(context);
        View view = LayoutInflater.from(context).inflate(R.layout.dialog_loading,null, false);
        setView(view);
        setCancelable(false);
        ((AppCompatTextView)view.findViewById(R.id.title))
                .setText(title);
    }
}
