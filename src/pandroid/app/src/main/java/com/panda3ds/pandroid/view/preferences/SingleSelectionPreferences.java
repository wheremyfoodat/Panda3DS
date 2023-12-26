package com.panda3ds.pandroid.view.preferences;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.AttributeSet;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.utils.Constants;

public class SingleSelectionPreferences extends PreferenceCategory implements Preference.OnPreferenceClickListener {
    private final Drawable transparent = new ColorDrawable(Color.TRANSPARENT);
    private final Drawable doneDrawable = ContextCompat.getDrawable(getContext(), R.drawable.ic_done);

    public SingleSelectionPreferences(@NonNull Context context) {
        super(context);
    }

    public SingleSelectionPreferences(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public SingleSelectionPreferences(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public SingleSelectionPreferences(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    {
        try {
            TypedArray color = getContext().obtainStyledAttributes(new int[]{
                    android.R.attr.textColorSecondary
            });
            doneDrawable.setTint(color.getColor(0, Color.RED));
            color.recycle();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                color.close();
            }
        } catch (Exception e) {
            Log.e(Constants.LOG_TAG, "Error on obtain text color secondary: ", e);
        }
    }

    @Override
    public void onAttached() {
        super.onAttached();

        for (int i = 0; i < getPreferenceCount();i++) {
            getPreference(i).setOnPreferenceClickListener(this);
        }
    }

    public void setSelectedItem(int index) {
        onPreferenceClick(getPreference(index));
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        int index = 0;

        for (int i = 0; i < getPreferenceCount(); i++) {
            Preference item = getPreference(i);
            if (item == preference) {
                index = i;
                item.setIcon(R.drawable.ic_done);
            } else {
                item.setIcon(transparent);
            }
        }

        callChangeListener(index);
        return false;
    }
}
