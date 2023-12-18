package com.panda3ds.pandroid.app;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.config.GlobalConfig;

public class BaseActivity extends AppCompatActivity {
    private int currentTheme = GlobalConfig.get(GlobalConfig.KEY_APP_THEME);
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        applyTheme();
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (GlobalConfig.get(GlobalConfig.KEY_APP_THEME) != currentTheme){
            recreate();
        }
    }

    private void applyTheme(){
        switch (GlobalConfig.get(GlobalConfig.KEY_APP_THEME)){
            case GlobalConfig.VALUE_THEME_ANDROID:
                setTheme(R.style.Theme_Pandroid);
                break;
            case GlobalConfig.VALUE_THEME_LIGHT:
                setTheme(R.style.Theme_Pandroid_Light);
                break;
            case GlobalConfig.VALUE_THEME_DARK:
                setTheme(R.style.Theme_Pandroid_Dark);
                break;
            case GlobalConfig.VALUE_THEME_BLACK:
                setTheme(R.style.Theme_Pandroid_Black);
                break;
        }
        currentTheme = GlobalConfig.get(GlobalConfig.KEY_APP_THEME);
    }
}
