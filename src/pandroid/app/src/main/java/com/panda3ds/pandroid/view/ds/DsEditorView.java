package com.panda3ds.pandroid.view.ds;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.appcompat.widget.AppCompatSpinner;
import androidx.appcompat.widget.AppCompatTextView;

import com.google.android.material.checkbox.MaterialCheckBox;
import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.math.Vector2;
import com.panda3ds.pandroid.utils.CompatUtils;
import com.panda3ds.pandroid.utils.Constants;

@SuppressLint("ViewConstructor")
public class DsEditorView extends FrameLayout {
    private final float SIZE_DP;

    private final Paint selectionPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final DsLayout layout;
    private int width = 1, height = 1;
    private final LinearLayout gravityAnchor;
    private final LinearLayout aspectRatioFixLayout;
    private final LinearLayout modeSelectorLayout;
    private final AppCompatSpinner modeSelector;
    private final PointView spacePoint;
    private final PointView topDisplay;
    private final PointView bottomDisplay;
    private final PointView topDisplayResizer;
    private final PointView bottomDisplayResizer;

    @SuppressLint("ClickableViewAccessibility")
    public DsEditorView(Context context, int index) {
        super(context);
        layout = (DsLayout) DsLayoutManager.createLayout(index);
        SIZE_DP = CompatUtils.applyDimensions(TypedValue.COMPLEX_UNIT_DIP, 1);
        int colorBottomSelection = CompatUtils.resolveColor(context, androidx.appcompat.R.attr.colorPrimary);
        int colorTopSelection = CompatUtils.resolveColor(context, com.google.android.material.R.attr.colorAccent);

        selectionPaint.setColor(colorTopSelection);
        selectionPaint.setStrokeWidth(SIZE_DP * 2);
        selectionPaint.setPathEffect(new DashPathEffect(new float[] { SIZE_DP * 10, SIZE_DP * 10 }, 0.0f));
        selectionPaint.setStyle(Paint.Style.STROKE);

        layout.setTopDisplaySourceSize(Constants.N3DS_WIDTH, Constants.N3DS_HALF_HEIGHT);
        layout.setBottomDisplaySourceSize(Constants.N3DS_WIDTH - 40 - 40, Constants.N3DS_HALF_HEIGHT);
        setBackgroundColor(Color.argb(2, 0, 0, 0));

        LayoutInflater inflater = LayoutInflater.from(context);

        gravityAnchor = (LinearLayout) inflater.inflate(R.layout.ds_editor_gravity_anchor, this, false);
        gravityAnchor.findViewById(R.id.up).setOnClickListener(v -> {
            layout.getCurrentModel().gravity = Gravity.TOP;
            refreshLayout();
        });

        gravityAnchor.findViewById(R.id.center).setOnClickListener(v -> {
            layout.getCurrentModel().gravity = Gravity.CENTER;
            refreshLayout();
        });
        
        gravityAnchor.findViewById(R.id.down).setOnClickListener(v -> {
            layout.getCurrentModel().gravity = Gravity.BOTTOM;
            refreshLayout();
        });
        
        gravityAnchor.findViewById(R.id.revert).setOnClickListener(v -> {
            layout.getCurrentModel().reverse = !layout.getCurrentModel().reverse;
            refreshLayout();
        });

        {
            modeSelectorLayout = (LinearLayout) inflater.inflate(R.layout.ds_editor_spinner, this, false);
            ArrayAdapter<String> spinnerAdapter = new ArrayAdapter<>(getContext(), R.layout.ds_editor_spinner_label);
            spinnerAdapter.addAll("Single", "Relative", "Absolute");
            modeSelector = modeSelectorLayout.findViewById(R.id.spinner);
            modeSelector.setAdapter(spinnerAdapter);
            modeSelector.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    layout.getCurrentModel().mode = Mode.values()[position];
                    refreshLayout();
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {}
            });
        }

        aspectRatioFixLayout = (LinearLayout) inflater.inflate(R.layout.ds_editor_lock_aspect, this, false);
        ((MaterialCheckBox) aspectRatioFixLayout.findViewById(R.id.checkbox)).setOnCheckedChangeListener((buttonView, checked) -> {
            layout.getCurrentModel().lockAspect = checked;
            refreshPoints();
        });

        spacePoint = new PointView();
        spacePoint.setColor(CompatUtils.resolveColor(context, com.google.android.material.R.attr.colorOnPrimary), colorTopSelection);
        spacePoint.setOnTouchListener((view, motion) -> {
            layout.getCurrentModel().space = (motion.getX() + spacePoint.x()) / (float) width;
            refreshPoints();
            return true;
        });

        spacePoint.setLayoutGravity(Gravity.START | Gravity.CENTER);

        setOnClickListener(v -> {
            if (layout.getCurrentModel().mode == Mode.SINGLE) {
                layout.getCurrentModel().onlyTop = !layout.getCurrentModel().onlyTop;
                refreshPoints();
            }
        });

        topDisplay = new PointView();
        topDisplay.setText(R.string.top_display);
        topDisplay.setOnTouchListener(new DisplayTouchEvent(true));
        topDisplay.setTextColor(colorTopSelection);
        topDisplay.setBackground(new SelectionDrawable(colorTopSelection));

        bottomDisplay = new PointView();
        bottomDisplay.setText(R.string.bottom_display);
        bottomDisplay.setOnTouchListener(new DisplayTouchEvent(false));
        bottomDisplay.setTextColor(colorBottomSelection);
        bottomDisplay.setBackground(new SelectionDrawable(colorBottomSelection));

        topDisplayResizer = new PointView();
        topDisplayResizer.setColor(0, colorTopSelection);
        topDisplayResizer.setOnTouchListener(new DisplayResizeTouchEvent(true));

        bottomDisplayResizer = new PointView();
        bottomDisplayResizer.setColor(0, colorBottomSelection);
        bottomDisplayResizer.setOnTouchListener(new DisplayResizeTouchEvent(false));
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        if (this.width != getWidth() || this.height != getHeight()) {
            this.width = getWidth();
            this.height = getHeight();
            refreshLayout();
        }
    }

    private void refreshPoints() {
        Model data = layout.getCurrentModel();
        data.preferredTop.fixOverlay(width, height, (int) (SIZE_DP * 5));
        data.preferredBottom.fixOverlay(width, height, (int) (SIZE_DP * 30));
        layout.update(width, height);
        Rect bottomDisplay = layout.getBottomDisplayBounds();
        Rect topDisplay = layout.getTopDisplayBounds();

        switch (data.mode) {
            case RELATIVE: {
                if (width > height) {
                    Rect primaryDisplay = data.reverse ? bottomDisplay : topDisplay;
                    data.space = primaryDisplay.width() / (float) width;
                    spacePoint.setCenterPosition(primaryDisplay.width(), (int) (SIZE_DP * 15));
                    spacePoint.setText(String.valueOf((int) (data.space * 100)));
                }
                
                break;
            }

            case SINGLE:
            case ABSOLUTE: break;
        }

        this.topDisplay.setSize(topDisplay.width(), topDisplay.height());
        this.topDisplay.setPosition(topDisplay.left, topDisplay.top);

        this.bottomDisplay.setSize(bottomDisplay.width(), bottomDisplay.height());
        this.bottomDisplay.setPosition(bottomDisplay.left, bottomDisplay.top);

        if (data.lockAspect) {
            topDisplayResizer.setCenterPosition(topDisplay.right, topDisplay.top + (topDisplay.height() / 2));
            bottomDisplayResizer.setCenterPosition(bottomDisplay.right, bottomDisplay.top + (bottomDisplay.height() / 2));
        } else {
            topDisplayResizer.setCenterPosition(topDisplay.right, topDisplay.bottom);
            bottomDisplayResizer.setCenterPosition(bottomDisplay.right, bottomDisplay.bottom);
        }

        invalidate();
    }

    private void refreshLayout() {
        removeAllViews();
        layout.update(width, height);
        boolean landscape = width > height;
        addView(topDisplay);
        addView(bottomDisplay);

        gravityAnchor.setOrientation(LinearLayout.HORIZONTAL);
        addView(modeSelectorLayout, new FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.BOTTOM | Gravity.CENTER));
        switch (layout.getCurrentModel().mode) {
            case RELATIVE: {
                addView(gravityAnchor, new FrameLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.CENTER | Gravity.TOP));
                if (landscape) {
                    addView(spacePoint);
                }

                break;
            }
            
            case ABSOLUTE: {
                addView(aspectRatioFixLayout, new LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.CENTER | Gravity.TOP));
                addView(topDisplayResizer);
                addView(bottomDisplayResizer);

                break;
            }

            case SINGLE: {
                addView(aspectRatioFixLayout, new LayoutParams(WRAP_CONTENT, WRAP_CONTENT, Gravity.CENTER | Gravity.TOP));
                break;
            }
        }
        ((MaterialCheckBox) aspectRatioFixLayout.findViewById(R.id.checkbox)).setChecked(layout.getCurrentModel().lockAspect);

        modeSelector.setSelection(layout.getCurrentModel().mode.ordinal());
        gravityAnchor.findViewById(R.id.revert).setRotation(landscape ? 0 : 90);
        refreshPoints();
    }

    private class PointView extends AppCompatTextView {
        public PointView() {
            super(DsEditorView.this.getContext());
            setLayoutParams(new FrameLayout.LayoutParams((int) (SIZE_DP * 30), (int) (SIZE_DP * 30)));
            setBackgroundResource(R.drawable.medium_card_background);
            setGravity(Gravity.CENTER);
            this.setFocusable(true);
            this.setClickable(true);
        }

        public int x() {
            return ((LayoutParams) getLayoutParams()).leftMargin;
        }

        public int y() {
            return ((LayoutParams) getLayoutParams()).topMargin;
        }

        public int width() {
            return ((LayoutParams) getLayoutParams()).width;
        }

        public void setColor(int text, int background) {
            setTextColor(text);
            setBackgroundTintList(ColorStateList.valueOf(background));
        }

        public void setSize(int width, int height) {
            LayoutParams params = (LayoutParams) getLayoutParams();
            params.width = Math.max(0, width);
            params.height = Math.max(0, height);
            setLayoutParams(params);
        }

        public void setPosition(int x, int y) {
            LayoutParams params = (LayoutParams) getLayoutParams();
            params.leftMargin = x;
            params.topMargin = y;
            setLayoutParams(params);
        }

        public void setCenterPosition(int x, int y) {
            int middle = this.width() / 2;
            setPosition(Math.max(-middle, Math.min(x - middle, width - middle)), Math.max(-middle, Math.min(y - middle, height - middle)));
        }

        public void setLayoutGravity(int gravity) {
            FrameLayout.LayoutParams params = (LayoutParams) getLayoutParams();
            params.gravity = gravity;
            setLayoutParams(params);
        }
    }

    private class DisplayTouchEvent implements OnTouchListener {
        private final boolean topScreen;
        private Vector2 downEvent = null;

        private DisplayTouchEvent(boolean topScreen) {
            this.topScreen = topScreen;
        }

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            Bounds preferred = topScreen ? layout.getCurrentModel().preferredTop : layout.getCurrentModel().preferredBottom;
            if (layout.getCurrentModel().mode == Mode.ABSOLUTE && event.getAction() != MotionEvent.ACTION_UP) {
                if (downEvent == null) {
                    downEvent = new Vector2(event.getRawX(), event.getRawY());
                    return true;
                }

                preferred.move((int) (event.getRawX() - downEvent.x), (int) (event.getRawY() - downEvent.y));
                downEvent.set(event.getRawX(), event.getRawY());
                refreshPoints();
                return true;
            } else if (layout.getCurrentModel().mode == Mode.SINGLE && event.getAction() == MotionEvent.ACTION_UP) {
                callOnClick();
            }
            downEvent = null;
            return false;
        }
    }

    private class DisplayResizeTouchEvent implements OnTouchListener {
        private final boolean topScreen;

        private DisplayResizeTouchEvent(boolean topScreen) {
            this.topScreen = topScreen;
        }

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            Bounds preferred = topScreen ? layout.getCurrentModel().preferredTop : layout.getCurrentModel().preferredBottom;
            if (event.getAction() != MotionEvent.ACTION_UP) {
                preferred.right = (int) (width - (((PointView) v).x() + event.getX()));
                preferred.bottom = (int) (height - (((PointView) v).y() + event.getY()));
                refreshPoints();
                return true;
            }
            
            return false;
        }
    }

    private class SelectionDrawable extends ColorDrawable {
        private final Paint solidPaint = new Paint();

        public SelectionDrawable(int color) {
            super(color);
        }

        @Override
        public void draw(Canvas canvas) {
            int color = this.getColor();
            selectionPaint.setColor(color);
            solidPaint.setColor(Color.argb(65, Color.red(color), Color.green(color), Color.blue(color)));
            canvas.drawRect(this.getBounds(), solidPaint);
            canvas.drawRect(this.getBounds(), selectionPaint);
        }
    }
}