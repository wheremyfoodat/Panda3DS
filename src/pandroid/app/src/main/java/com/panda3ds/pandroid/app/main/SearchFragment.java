package com.panda3ds.pandroid.app.main;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatEditText;
import androidx.fragment.app.Fragment;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.base.GameAboutDialog;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.utils.SearchAgent;
import com.panda3ds.pandroid.view.SimpleTextWatcher;
import com.panda3ds.pandroid.view.gamesgrid.GamesGridView;
import java.util.ArrayList;
import java.util.List;


public class SearchFragment extends Fragment {
	private final SearchAgent searchAgent = new SearchAgent();
	private GamesGridView gamesListView;

	@Nullable
	@Override
	public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
		return inflater.inflate(R.layout.fragment_search, container, false);
	}

	@Override
	public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
		super.onViewCreated(view, savedInstanceState);

		gamesListView = view.findViewById(R.id.games);
		gamesListView.setItemLongClick((game)->{
			GameAboutDialog dialog = new GameAboutDialog(requireActivity(), game);
			dialog.setOnDismissListener((x)-> search(((AppCompatEditText) view.findViewById(R.id.search_bar)).getText().toString()));
			dialog.show();
		});
		((AppCompatEditText) view.findViewById(R.id.search_bar)).addTextChangedListener((SimpleTextWatcher) this::search);
	}

	@Override
	public void onResume() {
		super.onResume();
		searchAgent.clearBuffer();
		for (GameMetadata game : GameUtils.getGames()) {
			searchAgent.addToBuffer(game.getId(), game.getTitle(), game.getPublisher());
		}

		search("");
	}

	private void search(String query) {
		List<String> resultIds = searchAgent.search(query);
		ArrayList<GameMetadata> games = new ArrayList<>(GameUtils.getGames());
		Object[] resultObj = games.stream().filter(gameMetadata -> resultIds.contains(gameMetadata.getId())).toArray();

		games.clear();
		for (Object res : resultObj) {
            games.add((GameMetadata) res);
        }

		gamesListView.setGameList(games);
	}
}
