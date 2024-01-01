package com.panda3ds.pandroid.app.base;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.AppCompatEditText;
import androidx.appcompat.widget.LinearLayoutCompat;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.lang.Function;

public class BottomAlertDialog extends AlertDialog.Builder {
    private final LinearLayoutCompat layoutCompat;

    public BottomAlertDialog(@NonNull Context context) {
        super(context, R.style.AlertDialog);
        layoutCompat = new LinearLayoutCompat(context);
        layoutCompat.setOrientation(LinearLayoutCompat.VERTICAL);

        int padding = getContext().getResources().getDimensionPixelSize(androidx.appcompat.R.dimen.abc_dialog_padding_material);
        layoutCompat.setPadding(padding, 0, padding, 0);

        setView(layoutCompat);
    }

    @NonNull
    @Override
    public AlertDialog create() {
        AlertDialog dialog = super.create();
        dialog.getWindow().setGravity(Gravity.BOTTOM | Gravity.CENTER);
        dialog.getWindow().getAttributes().y = Math.round(getContext().getResources().getDisplayMetrics().density * 15);
        return dialog;
    }

    public BottomAlertDialog setTextInput(String hint, Function<String> listener) {
        AppCompatEditText edit = new AppCompatEditText(getContext());
        edit.setHint(hint);
        int margin = layoutCompat.getPaddingLeft() / 2;
        LinearLayoutCompat.LayoutParams params = new LinearLayoutCompat.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.setMargins(0, margin, 0, margin);
        layoutCompat.addView(edit, params);
        setPositiveButton(android.R.string.ok, (dialog, which) -> listener.run(String.valueOf(edit.getText())));
        setNegativeButton(android.R.string.cancel, (dialog, which) -> dialog.dismiss());
        return this;
    }

    @Override
    public AlertDialog show() {
        AlertDialog dialog = create();
        dialog.show();
        return dialog;
    }
}
