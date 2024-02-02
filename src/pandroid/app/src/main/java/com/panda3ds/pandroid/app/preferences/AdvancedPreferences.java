package com.panda3ds.pandroid.app.preferences;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.SwitchPreference;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.services.LoggerService;
import com.panda3ds.pandroid.data.config.GlobalConfig;

public class AdvancedPreferences extends BasePreferenceFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.advanced_preferences, rootKey);
        setActivityTitle(R.string.developer_options);

        setItemClick("performanceMonitor", pref -> GlobalConfig.set(GlobalConfig.KEY_SHOW_PERFORMANCE_OVERLAY, ((SwitchPreference) pref).isChecked()));
        setItemClick("shaderJit", pref -> GlobalConfig.set(GlobalConfig.KEY_SHADER_JIT, ((SwitchPreference) pref).isChecked()));
        setItemClick("loggerService", pref -> {
            boolean checked = ((SwitchPreference) pref).isChecked();
            Context ctx = PandroidApplication.getAppContext();
            if (checked) {
                ctx.startService(new Intent(ctx, LoggerService.class));
            } else {
                ctx.stopService(new Intent(ctx, LoggerService.class));
            }
            GlobalConfig.set(GlobalConfig.KEY_LOGGER_SERVICE, checked);
        });

        refresh();
    }

    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh() {
        ((SwitchPreference) findPreference("performanceMonitor")).setChecked(GlobalConfig.get(GlobalConfig.KEY_SHOW_PERFORMANCE_OVERLAY));
        ((SwitchPreference) findPreference("loggerService")).setChecked(GlobalConfig.get(GlobalConfig.KEY_LOGGER_SERVICE));
        ((SwitchPreference) findPreference("shaderJit")).setChecked(GlobalConfig.get(GlobalConfig.KEY_SHADER_JIT));
    }
}
