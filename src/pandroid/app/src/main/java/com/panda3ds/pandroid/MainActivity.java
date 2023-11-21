package com.panda3ds.pandroid;

import androidx.appcompat.app.AppCompatActivity;
import static android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowInsets;
import android.view.View;
import android.os.Environment;
import android.widget.Toast;
import android.widget.FrameLayout;
import com.panda3ds.pandroid.PathUtils;

import com.google.android.material.floatingactionbutton.FloatingActionButton;

public class MainActivity extends AppCompatActivity {

    PandaGlSurfaceView glView;

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
        if (!Environment.isExternalStorageManager()) {
            Intent intent = new Intent(ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
            startActivity(intent);
        }
        
        glView = new PandaGlSurfaceView(this);
        setContentView(glView);
        FloatingActionButton fab = new FloatingActionButton(this);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                openFile();
            }
        });
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(200, 200);
        addContentView(fab, params);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == PICK_3DS_ROM) {
            if (resultCode == RESULT_OK) {
                String path = PathUtils.getPath(getApplicationContext(), data.getData());
                Toast.makeText(getApplicationContext(), "pandroid opening " + path, Toast.LENGTH_LONG).show();
                glView.queueEvent(new Runnable() {
                    @Override
                    public void run() {
                        AlberDriver.LoadRom(path);
                    }
                });
            }
        }
    }
}