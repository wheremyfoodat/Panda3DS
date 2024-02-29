package com.panda3ds.pandroid.app.preferences;

import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatRadioButton;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.base.BaseSheetDialog;
import com.panda3ds.pandroid.utils.CompatUtils;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.view.recycler.AutoFitGridLayout;
import com.panda3ds.pandroid.view.recycler.SimpleListAdapter;

import java.util.ArrayList;
import java.util.Arrays;

public class ThemeSelectorDialog extends BaseSheetDialog {

    private final SimpleListAdapter<Theme> adapter = new SimpleListAdapter<>(R.layout.hold_theme_preview_base, this::bindItemView);
    private final int currentTheme = GlobalConfig.get(GlobalConfig.KEY_APP_THEME);
    private static final ArrayList<Theme> themes = new ArrayList<>(Arrays.asList(
            new Theme(R.style.Theme_Pandroid, R.string.theme_device, GlobalConfig.THEME_ANDROID),
            new Theme(R.style.Theme_Pandroid_Light, R.string.light, GlobalConfig.THEME_LIGHT),
            new Theme(R.style.Theme_Pandroid_Dark, R.string.dark, GlobalConfig.THEME_DARK),
            new Theme(R.style.Theme_Pandroid_Black, R.string.black, GlobalConfig.THEME_BLACK)
    ));


    public ThemeSelectorDialog(@NonNull Context context) {
        super(context);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.dialog_select_theme);
        adapter.clear();
        themes.sort((o1, o2) -> o1.value == currentTheme ? -1 : 0);
        adapter.addAll(themes);

        RecyclerView recycler = findViewById(R.id.recycler);
        recycler.setAdapter(adapter);
        recycler.setLayoutManager(new AutoFitGridLayout(getContext(), 150));
    }

    private void bindItemView(int i, Theme theme, View view) {
        ViewGroup container = view.findViewById(R.id.preview);
        container.removeAllViews();
        container.addView(LayoutInflater.from(new ContextThemeWrapper(getContext(), theme.style)).inflate(R.layout.hold_theme_preview, null, false));
        ((TextView) view.findViewById(R.id.title)).setText(theme.name);
        ((AppCompatRadioButton) view.findViewById(R.id.checkbox)).setChecked(GlobalConfig.get(GlobalConfig.KEY_APP_THEME) == theme.value);
        view.setOnClickListener(v -> {
            dismiss();
            if (theme.value != GlobalConfig.get(GlobalConfig.KEY_APP_THEME)) {
                GlobalConfig.set(GlobalConfig.KEY_APP_THEME, theme.value);
                CompatUtils.findActivity(getContext()).recreate();
            }
        });
    }

    private static final class Theme {
        private final int style;
        private final int name;
        private final int value;

        private Theme(int style, int name, int value) {
            this.style = style;
            this.name = name;
            this.value = value;
        }
    }
}
