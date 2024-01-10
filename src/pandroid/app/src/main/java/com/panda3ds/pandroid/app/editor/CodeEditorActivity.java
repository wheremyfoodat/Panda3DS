package com.panda3ds.pandroid.app.editor;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.base.BottomAlertDialog;
import com.panda3ds.pandroid.lang.Task;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.view.code.CodeEditor;
import com.panda3ds.pandroid.view.code.syntax.CodeSyntax;

import java.io.Serializable;

public class CodeEditorActivity extends BaseActivity {
    private static final String TAB_CONTENT = "    ";
    private String path;
    private String fileName;
    private CodeEditor editor;
    private AppCompatTextView title;
    private View saveButton;
    private boolean changed = false;


    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_code_editor);
        Arguments args = (Arguments) getIntent().getSerializableExtra("args");

        editor = findViewById(R.id.editor);
        getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(this::onGlobalLayoutChanged);

        path = args.path;
        fileName = args.fileName;
        title = findViewById(R.id.title);
        title.setText(fileName);

        saveButton = findViewById(R.id.save);

        saveButton.setVisibility(View.GONE);
        saveButton.setOnClickListener(v -> save());

        new Task(() -> {
            String content = FileUtils.readTextFile(path + "/" + fileName);
            editor.post(() -> {
                editor.setText(content);
                editor.setSyntax(CodeSyntax.obtainByFileName(fileName));
                editor.setOnContentChangedListener(this::onDocumentContentChanged);
            });
        }).start();

        switch (args.type) {
            case LUA_SCRIPT_EDITOR:
                setupLuaPatchEditor();
                break;
            case READ_ONLY_EDITOR:
                setupReadOnlyEditor();
                break;
        }

        onGlobalLayoutChanged();

        findViewById(R.id.key_hide).setOnClickListener(v -> {
            ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(v.getWindowToken(), 0);
        });
        findViewById(R.id.key_tab).setOnClickListener(v -> {
            editor.insert(TAB_CONTENT);
        });
    }

    // Detect virtual keyboard is visible
    private void onGlobalLayoutChanged() {
        View view = getWindow().getDecorView();
        Rect rect = new Rect();
        view.getWindowVisibleDisplayFrame(rect);
        int currentHeight = rect.height();
        int height = view.getHeight();

        if (currentHeight < height * 0.8) {
            findViewById(R.id.keybar).setVisibility(View.VISIBLE);
        } else {
            findViewById(R.id.keybar).setVisibility(View.GONE);
        }

        System.out.println(height + "/" + currentHeight);
    }

    private void setupReadOnlyEditor() {
        editor.setEnabled(false);
        editor.setFocusable(false);
    }

    private void setupLuaPatchEditor() {
        findViewById(R.id.lua_toolbar).setVisibility(View.VISIBLE);
        findViewById(R.id.lua_play).setOnClickListener(v -> {
            if (changed) {
                save();
            }
            setResult(Activity.RESULT_OK, new Intent(Result.ACTION_PLAY.name()));
            finish();
        });
    }

    @SuppressLint("SetTextI18n")
    private void onDocumentContentChanged() {
        title.setText("*" + fileName);
        changed = true;
        saveButton.setVisibility(View.VISIBLE);
    }

    public void save() {
        title.setText(fileName);
        saveButton.setVisibility(View.GONE);
        changed = false;
        new Task(() -> FileUtils.writeTextFile(path, fileName, String.valueOf(editor.getText()))).runSync();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        System.out.println();
        if (event.getKeyCode() == KeyEvent.KEYCODE_TAB) {
            if (event.getAction() == KeyEvent.ACTION_UP) {
                editor.insert(TAB_CONTENT);
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        if (changed) {
            new BottomAlertDialog(this)
                    .setNeutralButton(android.R.string.cancel, (dialog, which) -> dialog.dismiss())
                    .setPositiveButton(R.string.save_and_exit, (dialog, which) -> {
                        save();
                        finish();
                    })
                    .setNegativeButton(R.string.exit_without_saving, (dialog, which) -> finish())
                    .setTitle(String.format(getString(R.string.exit_without_saving_title_ff), fileName)).show();
        } else {
            super.onBackPressed();
        }
    }

    public static final class Arguments implements Serializable {
        private final String path;
        private final String fileName;
        private final EditorType type;

        public Arguments(String path, String fileName, EditorType type) {
            this.path = path;
            this.fileName = fileName;
            this.type = type;
        }
    }

    public enum Result {
        ACTION_PLAY,
        NULL
    }

    public enum EditorType {
        LUA_SCRIPT_EDITOR,
        READ_ONLY_EDITOR,
        TEXT_EDITOR
    }

    public static final class Contract extends ActivityResultContract<Arguments, Result> {
        @NonNull
        @Override
        public Intent createIntent(@NonNull Context context, Arguments args) {
            return new Intent(context, CodeEditorActivity.class).putExtra("args", args);
        }

        @Override
        public Result parseResult(int i, @Nullable Intent intent) {
            return i == RESULT_OK && intent != null ? Result.valueOf(intent.getAction()) : Result.NULL;
        }
    }
}