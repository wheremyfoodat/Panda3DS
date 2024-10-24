package com.panda3ds.pandroid.app.preferences;

import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.preference.SwitchPreferenceCompat;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PreferenceActivity;
import com.panda3ds.pandroid.app.base.BasePreferenceFragment;
import com.panda3ds.pandroid.app.preferences.screen_editor.ScreenLayoutsPreference;
import com.panda3ds.pandroid.data.config.GlobalConfig;
import com.panda3ds.pandroid.utils.FileUtils;

import java.io.File;

public class GeneralPreferences extends BasePreferenceFragment implements ActivityResultCallback<Uri> {
    private final ActivityResultContracts.OpenDocument openFolderContract = new ActivityResultContracts.OpenDocument();
    private ActivityResultLauncher<String[]> pickFileRequest;
    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setPreferencesFromResource(R.xml.general_preference, rootKey);
        setItemClick("appearance.theme", (pref) -> new ThemeSelectorDialog(requireActivity()).show());
        setItemClick("appearance.ds", (pref) -> PreferenceActivity.launch(requireActivity(), ScreenLayoutsPreference.class));
        setItemClick("games.folders", (pref) -> PreferenceActivity.launch(requireActivity(), GamesFoldersPreferences.class));
        setItemClick("behavior.pictureInPicture", (pref)-> GlobalConfig.set(GlobalConfig.KEY_PICTURE_IN_PICTURE, ((SwitchPreferenceCompat)pref).isChecked()));
        setActivityTitle(R.string.general);
        refresh();

        setItemClick("games.aes_key", pref -> pickFileRequest.launch(new String[]{ "text/plain" }));
        setItemClick("games.seed_db", pref -> pickFileRequest.launch(new String[]{ "application/octet-stream" }));

        pickFileRequest = registerForActivityResult(openFolderContract, this);
    }

    @Override
    public void onResume() {
        super.onResume();
        refresh();
    }

    private void refresh() {
        setSwitchValue("behavior.pictureInPicture", GlobalConfig.get(GlobalConfig.KEY_PICTURE_IN_PICTURE));
        setSummaryValue("games.aes_key", String.format(getString(FileUtils.exists(FileUtils.getPrivatePath()+"/sysdata/aes_keys.txt") ? R.string.file_available : R.string.file_not_available), "aes_key.txt"));
        setSummaryValue("games.seed_db", String.format(getString(FileUtils.exists(FileUtils.getPrivatePath()+"/sysdata/seeddb.bin") ? R.string.file_available : R.string.file_not_available), "seeddb.bin"));
    }

	@Override
	public void onDestroy() {
		super.onDestroy();
		if (pickFileRequest != null) {
			pickFileRequest.unregister();
			pickFileRequest = null;
		}
	}

	@Override
	public void onActivityResult(Uri result) {
		if (result != null) {
			String path = result.toString();
			Log.w("File", path + " -> " + FileUtils.getName(path));
			switch (String.valueOf(FileUtils.getName(path))) {
				case "aes_keys.txt":
				case "seeddb.bin": {
					String name = FileUtils.getName(path);
					if (FileUtils.getLength(path) < 1024 * 256) {
						String sysdataFolder = FileUtils.getPrivatePath() + "/sysdata";
						if (!FileUtils.exists(sysdataFolder)) {
							FileUtils.createDir(FileUtils.getPrivatePath(), "sysdata");
						}
						if (FileUtils.exists(sysdataFolder + "/" + name)) {
							FileUtils.delete(sysdataFolder + "/" + name);
						}
						FileUtils.copyFile(path, FileUtils.getPrivatePath() + "/sysdata/", name);
						Toast.makeText(getActivity(), String.format(getString(R.string.file_imported), name), Toast.LENGTH_LONG).show();
					} else {
						Toast.makeText(getActivity(), R.string.invalid_file, Toast.LENGTH_LONG).show();
					}
				} break;
				default: Toast.makeText(getActivity(), R.string.invalid_file, Toast.LENGTH_LONG).show(); break;
			}
			refresh();
		}
	}
}
