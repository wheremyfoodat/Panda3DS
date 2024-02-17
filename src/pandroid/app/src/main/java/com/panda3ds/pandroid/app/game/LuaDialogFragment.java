package com.panda3ds.pandroid.app.game;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.base.BottomAlertDialog;
import com.panda3ds.pandroid.app.base.BottomDialogFragment;
import com.panda3ds.pandroid.app.editor.CodeEditorActivity;
import com.panda3ds.pandroid.lang.Task;
import com.panda3ds.pandroid.utils.Constants;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.view.recycler.AutoFitGridLayout;
import com.panda3ds.pandroid.view.recycler.SimpleListAdapter;

import java.util.ArrayList;
import java.util.UUID;

public class LuaDialogFragment extends BottomDialogFragment {
    private final SimpleListAdapter<LuaFile> adapter = new SimpleListAdapter<>(R.layout.holder_lua_script, this::onCreateListItem);
    private ActivityResultLauncher<CodeEditorActivity.Arguments> codeEditorLauncher;
    private LuaFile currentEditorFile;

    private ActivityResultLauncher<String[]> openDocumentLauncher;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.dialog_lua_scripts, container, false);
    }


    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        openDocumentLauncher = registerForActivityResult(new ActivityResultContracts.OpenDocument(), result -> {
            if (result != null) {
                String fileName = FileUtils.getName(result.toString());

                if (fileName.toLowerCase().endsWith(".lua")) {
                    new Task(() -> {
                        String content = FileUtils.readTextFile(result.toString());
                        createFile(FileUtils.getName(result.toString()), content);
                    }).start();
                } else {
                    Toast.makeText(getContext(), R.string.file_not_supported, Toast.LENGTH_SHORT).show();
                }
            }
        });

        codeEditorLauncher = registerForActivityResult(new CodeEditorActivity.Contract(), result -> {
            if (result != null) {
                switch (result) {
                    case ACTION_PLAY:
                        loadScript(currentEditorFile);
                        break;
                }
            }
            
            orderByModified();
        });
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        view.findViewById(R.id.open_file).setOnClickListener(v -> {
            openDocumentLauncher.launch(new String[]{"*/*"});
        });
        view.findViewById(R.id.create).setOnClickListener(v -> {
            new BottomAlertDialog(requireContext())
                    .setTextInput(getString(R.string.name), arg -> {
                        String name = arg.trim();
                        if (name.length() > 1) {
                            new Task(() -> {
                                LuaFile file = createFile(name, "");
                                currentEditorFile = file;
                                codeEditorLauncher.launch(new CodeEditorActivity.Arguments(file.path, file.name, CodeEditorActivity.EditorType.LUA_SCRIPT_EDITOR));
                            }).start();
                        }
                    }).setTitle(R.string.create_new)
                    .show();
        });

        ((RecyclerView) view.findViewById(R.id.recycler)).setAdapter(adapter);
        ((RecyclerView) view.findViewById(R.id.recycler)).setLayoutManager(new AutoFitGridLayout(getContext(), 140));
        ArrayList<LuaFile> files = new ArrayList<>();
        String path = FileUtils.getResourcePath(Constants.RESOURCE_FOLDER_LUA_SCRIPTS);
        for (String file : FileUtils.listFiles(path)) {
            files.add(new LuaFile(file));
        }

        adapter.addAll(files);
        orderByModified();
    }

    private LuaFile createFile(String name, String content) {
        if (name.toLowerCase().endsWith(".lua")) {
            name = name.substring(0, name.length() - 4);
        }

        name = name.replaceAll("[^[a-zA-Z0-9-_ ]]", "-");

        String fileName = name + "." + UUID.randomUUID().toString().substring(0, 4) + ".lua";
        LuaFile file = new LuaFile(fileName);
        FileUtils.writeTextFile(file.path, fileName, content);
        getView().post(() -> {
            adapter.addAll(file);
            orderByModified();
        });

        return file;
    }

    private void orderByModified() {
        adapter.sort((o1, o2) -> Long.compare(o2.lastModified(), o1.lastModified()));
    }

    private void onCreateListItem(int position, LuaFile file, View view) {
        ((TextView) view.findViewById(R.id.title))
                .setText(file.name.split("\\.")[0]);

        view.setOnClickListener(v -> loadScript(file));
        view.findViewById(R.id.edit).setOnClickListener(v -> {
            currentEditorFile = file;
            codeEditorLauncher.launch(new CodeEditorActivity.Arguments(file.path, file.name, CodeEditorActivity.EditorType.LUA_SCRIPT_EDITOR));
        });
    }

    private void loadScript(LuaFile file) {
        dismiss();
        
        Toast.makeText(getContext(), String.format(getString(R.string.running_ff), file.name), Toast.LENGTH_SHORT).show();
        new Task(() -> {
            String script = FileUtils.readTextFile(file.absolutePath());
            file.update();
            AlberDriver.LoadLuaScript(script);
        }).start();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        openDocumentLauncher.unregister();
        codeEditorLauncher.unregister();
    }

    private static class LuaFile {
        private final String name;
        private final String path;

        private LuaFile(String path, String name) {
            this.name = name;
            this.path = path;
        }

        private LuaFile(String name) {
            this(FileUtils.getResourcePath(Constants.RESOURCE_FOLDER_LUA_SCRIPTS), name);
        }

        private String absolutePath() {
            return path + "/" + name;
        }

        private void update() {
            FileUtils.updateFile(absolutePath());
        }

        private long lastModified() {
            return FileUtils.getLastModified(absolutePath());
        }
    }
}
