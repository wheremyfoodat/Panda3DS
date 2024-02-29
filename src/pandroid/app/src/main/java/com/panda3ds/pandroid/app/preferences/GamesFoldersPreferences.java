package com.panda3ds.pandroid.app.preferences;

import android.annotation.SuppressLint;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.base.BaseSheetDialog;
import com.panda3ds.pandroid.data.game.GamesFolder;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.utils.GameUtils;

public class GamesFoldersPreferences extends BasePreferenceFragment implements ActivityResultCallback<Uri> {
    private final ActivityResultContracts.OpenDocumentTree openFolderContract = new ActivityResultContracts.OpenDocumentTree();
    private ActivityResultLauncher<Uri> pickFolderRequest;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.empty_preferences, rootKey);
        setActivityTitle(R.string.pref_games_folders);
        refreshList();
        pickFolderRequest = registerForActivityResult(openFolderContract, this);
    }

    @SuppressLint("RestrictedApi")
    private void refreshList() {
        GamesFolder[] folders = GameUtils.getFolders();
        PreferenceScreen screen = getPreferenceScreen();
        screen.removeAll();
        for (GamesFolder folder : folders) {
            Preference preference = new Preference(screen.getContext());
            preference.setOnPreferenceClickListener((item) -> {
                showFolderInfo(folder);
                screen.performClick();
                return false;
            });
            preference.setTitle(FileUtils.getName(folder.getPath()));
            preference.setSummary(String.format(getString(R.string.games_count_f), folder.getGames().size()));
            preference.setIcon(R.drawable.ic_folder);
            screen.addPreference(preference);
        }

        Preference add = new Preference(screen.getContext());
        add.setTitle(R.string.import_folder);
        add.setIcon(R.drawable.ic_add);
        add.setOnPreferenceClickListener(preference -> {
            pickFolderRequest.launch(null);
            return false;
        });
        screen.addPreference(add);
    }

    private void showFolderInfo(GamesFolder folder) {
        BaseSheetDialog dialog = new BaseSheetDialog(requireActivity());
        View layout = LayoutInflater.from(requireActivity()).inflate(R.layout.dialog_games_folder, null, false);
        dialog.setContentView(layout);

        ((TextView) layout.findViewById(R.id.name)).setText(FileUtils.getName(folder.getPath()));
        ((TextView) layout.findViewById(R.id.directory)).setText(FileUtils.obtainUri(folder.getPath()).getPath());
        ((TextView) layout.findViewById(R.id.games)).setText(String.valueOf(folder.getGames().size()));

        layout.findViewById(R.id.ok).setOnClickListener(v -> dialog.dismiss());
        layout.findViewById(R.id.remove).setOnClickListener(v -> {
            dialog.dismiss();
            GameUtils.removeFolder(folder);
            refreshList();
        });

        dialog.show();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (pickFolderRequest != null) {
            pickFolderRequest.unregister();
            pickFolderRequest = null;
        }
    }

    @Override
    public void onActivityResult(Uri result) {
        if (result != null) {
            FileUtils.makeUriPermanent(result.toString(), "r");
            GameUtils.registerFolder(result.toString());
            refreshList();
        }
    }
}