package com.panda3ds.pandroid.app.base;

import android.app.Dialog;
import android.os.Bundle;
import android.view.Gravity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

import com.panda3ds.pandroid.R;

public class BottomDialogFragment extends DialogFragment {
    @Override
    public int getTheme() {
        return R.style.AlertDialog;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        Dialog dialog = super.onCreateDialog(savedInstanceState);
        dialog.getWindow().setGravity(Gravity.CENTER | Gravity.BOTTOM);
        dialog.getWindow().getAttributes().y = Math.round(getContext().getResources().getDisplayMetrics().density * 15);
        return dialog;
    }
}
