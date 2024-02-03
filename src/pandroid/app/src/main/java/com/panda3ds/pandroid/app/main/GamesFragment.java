package com.panda3ds.pandroid.app.main;

import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.FileUtils;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.view.gamesgrid.GamesGridView;


public class GamesFragment extends Fragment implements ActivityResultCallback<Uri> {
	private final ActivityResultContracts.OpenDocument openRomContract = new ActivityResultContracts.OpenDocument();
	private ActivityResultLauncher<String[]> pickFileRequest;
	private GamesGridView gameListView;

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		return inflater.inflate(R.layout.fragment_games, container, false);
	}

	@Override
	public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
		super.onViewCreated(view, savedInstanceState);
		gameListView = view.findViewById(R.id.games);

		view.findViewById(R.id.add_rom).setOnClickListener((v) -> pickFileRequest.launch(new String[] {"*/*"}));
	}

	@Override
	public void onResume() {
		super.onResume();
		gameListView.setGameList(GameUtils.getGames());
	}

	@Override
	public void onActivityResult(Uri result) {
		if (result != null) {
			String uri = result.toString();
			if (GameUtils.findByRomPath(uri) == null) {
				if (!FileUtils.exists(uri)) {
					Toast.makeText(getContext(), "Invalid file path", Toast.LENGTH_LONG).show();
					return;
				}
				FileUtils.makeUriPermanent(uri, FileUtils.MODE_READ);
				GameMetadata game = new GameMetadata(uri, FileUtils.getName(uri).split("\\.")[0], "Unknown");
				GameUtils.addGame(game);
				GameUtils.launch(requireActivity(), game);
			}
		}
	}

	@Override
	public void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		pickFileRequest = registerForActivityResult(openRomContract, this);
	}

	@Override
	public void onDestroy() {
		if (pickFileRequest != null) {
			pickFileRequest.unregister();
			pickFileRequest = null;
		}

		super.onDestroy();
	}
}
