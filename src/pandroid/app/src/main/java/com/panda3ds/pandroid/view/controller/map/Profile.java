package com.panda3ds.pandroid.view.controller.map;

import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.app.PandroidApplication;

import java.util.UUID;

public class Profile {
    private final String id;
    private final Layout landscapeLayout;
    private final Layout portraitLayout;
    private String name;

    public Profile() {
        this(UUID.randomUUID().toString(), PandroidApplication.getAppContext().getString(R.string.unknown), new Layout(), new Layout());
    }

    public Profile(String id, String name, Layout landscape, Layout portrait) {
        this.id = id;
        this.name = name;
        this.landscapeLayout = landscape;
        this.portraitLayout = portrait;
    }

    public void applyToView(NodeID id, View view, int viewportWidth, int viewportHeight) {
        float pt = view.getResources().getDimension(R.dimen.SizePt);

        int width = view.getLayoutParams().width;
        int height = view.getLayoutParams().height;

        Layout layout = getLayoutBySize(viewportWidth, viewportHeight);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(width, height);
        Location location = layout.getLocation(id);

        int x = Math.round(location.getX() * pt);
        int y = Math.round(location.getY() * pt);

        params.gravity = location.getGravity() | Gravity.BOTTOM;
        params.bottomMargin = Math.max(Math.min(y - (height / 2), viewportHeight-height), 0);

        int gravity = location.getGravity() & Gravity.HORIZONTAL_GRAVITY_MASK;
        if (gravity == Gravity.RIGHT) {
            params.rightMargin = Math.max(x - (width / 2), 0);
        } else {
            params.leftMargin = Math.max(x - (width / 2), 0);
        }

        view.setVisibility(location.isVisible() ? View.VISIBLE : View.GONE);
        view.setLayoutParams(params);
    }

    public void setLocation(NodeID id, int x, int y, int viewportWidth, int viewportHeight) {
        float pt = PandroidApplication.getAppContext().getResources().getDimension(R.dimen.SizePt);

        Layout layout = getLayoutBySize(viewportWidth, viewportHeight);
        Location location = layout.getLocation(id);

        y = viewportHeight - y;

        if (x < viewportWidth / 2) {
            location.setGravity(Gravity.LEFT);
            location.setPosition(x / pt, y / pt);
        } else {
            x = (viewportWidth/2) - (x - (viewportWidth / 2));
            location.setGravity(Gravity.RIGHT);
            location.setPosition(x / pt, y / pt);
        }
    }


    public void setName(String name) {
        this.name = name;
    }

    public void setVisible(NodeID id, boolean visible) {
        landscapeLayout.getLocation(id).setVisible(visible);
        portraitLayout.getLocation(id).setVisible(visible);
    }
    private Layout getLayoutBySize(int width, int height) {
        return width > height ? landscapeLayout : portraitLayout;
    }

    public String getName() {
        return name;
    }

    public String getId() {
        return id;
    }

    @NonNull
    @Override
    public Profile clone() {
        return new Profile(id, name, landscapeLayout.clone(), portraitLayout.clone());
    }

    public boolean isVisible(NodeID id) {
        return landscapeLayout.getLocation(id).isVisible();
    }
}
