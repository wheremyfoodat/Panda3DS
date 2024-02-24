package com.panda3ds.pandroid.app.preferences;

import android.app.Activity;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatRadioButton;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.view.recycler.AutoFitGridLayout;
import com.panda3ds.pandroid.view.recycler.SimpleListAdapter;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.function.Predicate;

public class ThemeSelectorDialog extends BottomSheetDialog {

    private final SimpleListAdapter<Theme> adapter;
    private final int currentTheme = GlobalConfig.get(GlobalConfig.KEY_APP_THEME);
    private static final Theme[] themes = {
            new Theme(R.style.Theme_Pandroid, R.string.theme_device, GlobalConfig.THEME_ANDROID),
            new Theme(R.style.Theme_Pandroid_Light, R.string.light, GlobalConfig.THEME_LIGHT),
            new Theme(R.style.Theme_Pandroid_Dark, R.string.dark, GlobalConfig.THEME_DARK),
            new Theme(R.style.Theme_Pandroid_Black, R.string.black, GlobalConfig.THEME_BLACK)
    };


    public ThemeSelectorDialog(@NonNull Context context) {
        super(context);
        View content = LayoutInflater.from(context).inflate(R.layout.dialog_select_theme, null, false);
        setContentView(content);
        adapter = new SimpleListAdapter<>(R.layout.hold_theme_preview_summary, this::bindItemView);
        adapter.clear();
        ArrayList<Theme> themeList = new ArrayList<>(Arrays.asList(themes));
        themeList.sort((o1, o2) -> o1.id == currentTheme ? -1 : 0);
        adapter.addAll(themeList);

        ((RecyclerView) content.findViewById(R.id.recycler)).setAdapter(adapter);
        ((RecyclerView) content.findViewById(R.id.recycler)).setLayoutManager(new AutoFitGridLayout(getContext(), 150));
    }

    private void bindItemView(int i, Theme theme, View view) {
        ViewGroup container = view.findViewById(R.id.preview);
        container.removeAllViews();
        container.addView(LayoutInflater.from(new ContextThemeWrapper(getContext(), theme.style)).inflate(R.layout.hold_theme_preview, null, false));
        ((TextView) view.findViewById(R.id.title)).setText(theme.name);
        ((AppCompatRadioButton) view.findViewById(R.id.checkbox)).setChecked(GlobalConfig.get(GlobalConfig.KEY_APP_THEME) == theme.id);
        view.setOnClickListener(v -> {
            dismiss();
            if (theme.id != GlobalConfig.get(GlobalConfig.KEY_APP_THEME)) {
                GlobalConfig.set(GlobalConfig.KEY_APP_THEME, theme.id);
                ((Activity) v.getContext()).recreate();
            }
        });
    }

    private static final class Theme {
        private final int style;
        private final int name;
        private final int id;

        private Theme(int style, int name, int value) {
            this.style = style;
            this.name = name;
            this.id = value;
        }
    }
}
