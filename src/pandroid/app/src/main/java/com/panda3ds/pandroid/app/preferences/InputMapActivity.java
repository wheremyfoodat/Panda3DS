package com.panda3ds.pandroid.app.preferences;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.Toast;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.input.InputEvent;
import com.panda3ds.pandroid.input.InputHandler;

import java.util.Objects;

public class InputMapActivity extends BaseActivity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_input_map);
    }

    @Override
    protected void onResume() {
        super.onResume();

        InputHandler.reset();
        InputHandler.setMotionDeadZone(0.8f);
        InputHandler.setEventListener(this::onInputEvent);
    }

    @Override
    protected void onPause() {
        super.onPause();
        InputHandler.reset();
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent ev) {
        return InputHandler.processMotionEvent(ev);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return InputHandler.processKeyEvent(event, false);
    }

    private void onInputEvent(InputEvent event) {
        if (Objects.equals(event.getName(), "KEYCODE_BACK")) {
            onBackPressed();
            return;
        }
        setResult(RESULT_OK, new Intent(event.getName()));
        Toast.makeText(this, event.getName(), Toast.LENGTH_SHORT).show();
        finish();
    }


    public static final class Contract extends ActivityResultContract<String, String> {
        @NonNull
        @Override
        public Intent createIntent(@NonNull Context context, String s) {
            return new Intent(context, InputMapActivity.class);
        }

        @Override
        public String parseResult(int i, @Nullable Intent intent) {
            return i == RESULT_OK ? intent.getAction() : null;
        }
    }
}
