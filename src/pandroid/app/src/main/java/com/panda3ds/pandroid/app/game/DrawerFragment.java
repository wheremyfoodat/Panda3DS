package com.panda3ds.pandroid.app.game;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
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

import com.google.android.material.navigation.NavigationView;
import com.panda3ds.pandroid.AlberDriver;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.data.game.GameMetadata;
import com.panda3ds.pandroid.utils.GameUtils;
import com.panda3ds.pandroid.view.gamesgrid.GameIconView;

public class DrawerFragment extends Fragment implements DrawerLayout.DrawerListener, NavigationView.OnNavigationItemSelectedListener {
    private DrawerLayout drawerContainer;
    private View drawerLayout;
    private EmulatorCallback emulator;
    private GameMetadata game;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        emulator = ((EmulatorCallback) requireActivity());
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
        drawerLayout = view.findViewById(R.id.drawer_layout);

        ((NavigationView)view.findViewById(R.id.menu)).setNavigationItemSelectedListener(this);
        refresh();
    }

    private void refresh() {
        game = GameUtils.getCurrentGame();
        if (game.getIcon() != null && !game.getIcon().isRecycled()) {
            ((GameIconView) drawerLayout.findViewById(R.id.game_icon)).setImageBitmap(game.getIcon());
        } else {
            ((GameIconView) drawerLayout.findViewById(R.id.game_icon)).setImageDrawable(new ColorDrawable(Color.TRANSPARENT));
        }
        ((AppCompatTextView)drawerLayout.findViewById(R.id.game_title)).setText(game.getTitle());
        ((AppCompatTextView)drawerLayout.findViewById(R.id.game_publisher)).setText(game.getPublisher());
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
            refresh();
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
        } else if (id == R.id.ds_switch) {
            emulator.swapScreens();
            close();
        } else if (id == R.id.exit) {
            requireActivity().finishAndRemoveTask();
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
