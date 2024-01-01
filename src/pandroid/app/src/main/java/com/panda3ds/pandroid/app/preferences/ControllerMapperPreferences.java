package com.panda3ds.pandroid.app.preferences;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.BaseActivity;
import com.panda3ds.pandroid.app.base.BottomAlertDialog;
import com.panda3ds.pandroid.view.controller.map.ControllerMapper;
import com.panda3ds.pandroid.view.controller.map.ControllerProfileManager;
import com.panda3ds.pandroid.view.controller.map.NodeID;
import com.panda3ds.pandroid.view.controller.map.Profile;

public class ControllerMapperPreferences extends Fragment {

    private Profile currentProfile;
    private ControllerMapper mapper;
    private View saveButton;

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.preference_controller_mapper, container, false);
    }


    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);


        currentProfile = ControllerProfileManager.get(getArguments().getString("profile")).clone();

        ((BaseActivity) requireActivity()).getSupportActionBar().hide();
        mapper = view.findViewById(R.id.mapper);
        mapper.initialize(this::onLocationChanged, currentProfile);

        view.findViewById(R.id.change_visibility).setOnClickListener(v -> {
            BottomAlertDialog builder = new BottomAlertDialog(v.getContext());
            builder.setTitle("Visibility");
            boolean[] visibleList = {
                    currentProfile.isVisible(NodeID.START),
                    currentProfile.isVisible(NodeID.SELECT),
                    currentProfile.isVisible(NodeID.L),
                    currentProfile.isVisible(NodeID.R),
                    currentProfile.isVisible(NodeID.DPAD),
                    currentProfile.isVisible(NodeID.JOYSTICK),
                    currentProfile.isVisible(NodeID.GAMEPAD),
            };
            builder.setMultiChoiceItems(new CharSequence[]{
                    "Start", "Select", "L", "R", "Dpad", getString(R.string.axis), "A/B/X/Y"
            }, visibleList, (dialog, index, visibility) -> {
                visibleList[index] = visibility;
            }).setPositiveButton(android.R.string.ok, (dialog, which) -> {

                saveButton.setVisibility(View.VISIBLE);

                currentProfile.setVisible(NodeID.START, visibleList[0]);
                currentProfile.setVisible(NodeID.SELECT, visibleList[1]);
                currentProfile.setVisible(NodeID.L, visibleList[2]);
                currentProfile.setVisible(NodeID.R, visibleList[3]);
                currentProfile.setVisible(NodeID.DPAD, visibleList[4]);
                currentProfile.setVisible(NodeID.JOYSTICK, visibleList[5]);
                currentProfile.setVisible(NodeID.GAMEPAD, visibleList[6]);

                mapper.refreshLayout();
            }).setNegativeButton(android.R.string.cancel, (dialog, which) -> dialog.dismiss());
            builder.show();
        });

        saveButton = view.findViewById(R.id.save);
        saveButton.setOnClickListener(v -> {
            ControllerProfileManager.add(currentProfile);
            Toast.makeText(v.getContext(), R.string.saved, Toast.LENGTH_SHORT).show();
            requireActivity().finish();
        });

        view.findViewById(R.id.delete).setOnClickListener(v -> {
            ControllerProfileManager.remove(currentProfile.getId());
            requireActivity().finish();
        });

        view.findViewById(R.id.rotate).setOnClickListener(v->{
            requireActivity().setRequestedOrientation(mapper.getCurrentWidth() > mapper.getCurrentHeight() ?  ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        });

        view.findViewById(R.id.delete).setVisibility(ControllerProfileManager.getProfileCount() > 1 ? View.VISIBLE : View.GONE);

        saveButton.setVisibility(View.GONE);
    }

    public void onLocationChanged(NodeID id) {
        saveButton.setVisibility(View.VISIBLE);
    }
}
