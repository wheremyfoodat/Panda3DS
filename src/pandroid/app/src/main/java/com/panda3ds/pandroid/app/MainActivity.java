package com.panda3ds.pandroid.app;

import static android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.PathUtils;

public class MainActivity extends BaseActivity {
    private static final int PICK_3DS_ROM = 2;

    private void openFile() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, PICK_3DS_ROM);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivity(intent);
            }
        }

        setContentView(R.layout.activity_main);

        findViewById(R.id.load_rom).setOnClickListener(v->{
            openFile();
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == PICK_3DS_ROM) {
            if (resultCode == RESULT_OK) {
                String path = PathUtils.getPath(getApplicationContext(), data.getData());
                Toast.makeText(getApplicationContext(), "pandroid opening " + path, Toast.LENGTH_LONG).show();
                startActivity(new Intent(this, GameActivity.class)
                        .putExtra(Constants.EXTRA_PATH, path));
            }
            super.onActivityResult(requestCode, resultCode, data);
        }
    }
}