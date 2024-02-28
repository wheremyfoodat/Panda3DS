package com.panda3ds.pandroid.app.game;

import android.graphics.Color;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.drawerlayout.widget.DrawerLayout;
import androidx.fragment.app.Fragment;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;

import com.google.android.material.navigation.NavigationView;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.view.gamesgrid.GameIconView;

public class DrawerFragment extends Fragment implements DrawerLayout.DrawerListener, NavigationView.OnNavigationItemSelectedListener {
    private DrawerLayout drawerContainer;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        drawerContainer = requireActivity().findViewById(R.id.drawer_container);
        drawerContainer.removeDrawerListener(this);
        drawerContainer.addDrawerListener(this);
        drawerContainer.setScrimColor(Color.argb(160, 0,0,0));
        drawerContainer.setVisibility(View.GONE);

        return inflater.inflate(R.layout.fragment_game_drawer, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        drawerContainer.setVisibility(View.GONE);

        GameMetadata game = GameUtils.getCurrentGame();

        ((GameIconView)view.findViewById(R.id.game_icon)).setImageBitmap(game.getIcon());
        ((AppCompatTextView)view.findViewById(R.id.game_title)).setText(game.getTitle());
        ((AppCompatTextView)view.findViewById(R.id.game_publisher)).setText(game.getPublisher());

        ((NavigationView)view.findViewById(R.id.menu)).setNavigationItemSelectedListener(this);
    }

    @Override
    public void onDetach() {
        if (drawerContainer != null) {
            drawerContainer.removeDrawerListener(this);
        }

        super.onDetach();
    }

    private void refreshLayout() {
        drawerContainer.measure(View.MeasureSpec.EXACTLY, View.MeasureSpec.EXACTLY);
        drawerContainer.requestLayout();
        drawerContainer.invalidate();
        drawerContainer.forceLayout();
    }

    public void open() {
        if (!drawerContainer.isOpen()) {
            drawerContainer.setVisibility(View.VISIBLE);
            drawerContainer.open();
            drawerContainer.postDelayed(this::refreshLayout, 20);
        }
    }

    public void close() {
        if (drawerContainer.isOpen()) {        
            drawerContainer.close();
        }
    }

    @Override
    public void onDrawerSlide(@NonNull View drawerView, float slideOffset) {}

    @Override
    public void onDrawerOpened(@NonNull View drawerView) {
        AlberDriver.Pause();
    }

    @Override
    public void onDrawerClosed(@NonNull View drawerView) {
        drawerContainer.setVisibility(View.GONE);
        AlberDriver.Resume();
    }

    @Override
    public void onDrawerStateChanged(int newState) {}

    @Override
    public boolean onNavigationItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.resume) {
            close();
        } else if (id == R.id.exit) {
            requireActivity().finish();
        } else if (id == R.id.lua_script) {
            new LuaDialogFragment().show(getParentFragmentManager(), null);
        } else if (id == R.id.change_orientation) {
            boolean isLandscape = getResources().getDisplayMetrics().widthPixels > getResources().getDisplayMetrics().heightPixels;
            requireActivity().setRequestedOrientation(isLandscape ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        }

        return false;
    }

    public boolean isOpened() {
        return drawerContainer.isOpen();
    }
}
