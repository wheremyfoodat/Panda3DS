package com.panda3ds.pandroid.view.controller.mapping;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.panda3ds.pandroid.R;
import com.panda3ds.pandroid.lang.Function;
import com.panda3ds.pandroid.math.Vector2;

public class ControllerMapper extends FrameLayout {
    public static int COLOR_DARK = Color.rgb(20, 20, 20);
    public static int COLOR_LIGHT = Color.rgb(60, 60, 60);
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Profile profile;
    private View selectedView;
    private final Paint selectionPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int width = -1;
    private int height = -1;
    private Function<ControllerItem> changeListener;

    public ControllerMapper(@NonNull Context context) {
        this(context, null);
    }

    public ControllerMapper(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ControllerMapper(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public ControllerMapper(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        setBackground(new ColorDrawable(Color.YELLOW));
        float dp = getResources().getDisplayMetrics().density;

        selectionPaint.setColor(Color.RED);
        selectionPaint.setStrokeWidth(dp * 2);
        selectionPaint.setStyle(Paint.Style.STROKE);
        selectionPaint.setPathEffect(new DashPathEffect(new float[]{dp * 10, dp * 10}, 0.0f));
    }

    public void initialize(Function<ControllerItem> changeListener, Profile profile) {
        this.profile = profile;
        this.changeListener = changeListener;

        measure(MeasureSpec.EXACTLY, MeasureSpec.EXACTLY);

        new MoveElementListener(ControllerItem.L, findViewById(R.id.button_l));
        new MoveElementListener(ControllerItem.R, findViewById(R.id.button_r));
        new MoveElementListener(ControllerItem.START, findViewById(R.id.button_start));
        new MoveElementListener(ControllerItem.SELECT, findViewById(R.id.button_select));
        new MoveElementListener(ControllerItem.DPAD, findViewById(R.id.dpad));
        new MoveElementListener(ControllerItem.GAMEPAD, findViewById(R.id.gamepad));
        new MoveElementListener(ControllerItem.JOYSTICK, findViewById(R.id.left_analog));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        drawBackground(canvas);
        if (selectedView != null) {
            paint.setColor(Color.argb(30, 255, 0, 0));
            drawSelected(canvas, paint);
        }
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        if (selectedView != null) {
            drawSelected(canvas, selectionPaint);
        }
    }

    public void drawSelected(Canvas canvas, Paint paint) {
        int[] absolutePosition = new int[2];
        int[] selectedViewPosition = new int[2];

        selectedView.getLocationOnScreen(selectedViewPosition);
        getLocationOnScreen(absolutePosition);

        int width = selectedView.getLayoutParams().width;
        int height = selectedView.getLayoutParams().height;

        int x = selectedViewPosition[0] - absolutePosition[0];
        int y = selectedViewPosition[1] - absolutePosition[1];

        canvas.drawRect(x, y, x + width, y + height, paint);
    }


    private void drawBackground(Canvas canvas) {
        paint.setStyle(Paint.Style.FILL);

        int shapeSize = Math.round(getResources().getDimension(R.dimen.SizePt) * 7.2f);
        boolean dark = true;
        boolean start = true;

        for (int x = 0; x < width + shapeSize; x += shapeSize) {
            for (int y = 0; y < height + shapeSize; y += shapeSize) {
                paint.setColor(dark ? COLOR_DARK : COLOR_LIGHT);
                canvas.drawRect(x, y, x + shapeSize, y + shapeSize, paint);
                dark = !dark;
            }
            start = !start;
            dark = start;
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int measuredWidth = getMeasuredWidth();
        int measuredHeight = getMeasuredHeight();
        
        if (measuredWidth != width || measuredHeight != height) {
            width = measuredWidth;
            height = measuredHeight;
            refreshLayout();
        }
    }

    public void refreshLayout() {
        if (profile != null) {
            profile.applyToView(ControllerItem.L, findViewById(R.id.button_l), width, height);
            profile.applyToView(ControllerItem.R, findViewById(R.id.button_r), width, height);
            profile.applyToView(ControllerItem.START, findViewById(R.id.button_start), width, height);
            profile.applyToView(ControllerItem.SELECT, findViewById(R.id.button_select), width, height);
            profile.applyToView(ControllerItem.DPAD, findViewById(R.id.dpad), width, height);
            profile.applyToView(ControllerItem.GAMEPAD, findViewById(R.id.gamepad), width, height);
            profile.applyToView(ControllerItem.JOYSTICK, findViewById(R.id.left_analog), width, height);
        }
    }

    public int getCurrentWidth() {
        return width;
    }

    public int getCurrentHeight() {
        return height;
    }

    public class MoveElementListener implements OnTouchListener {
        private final ControllerItem id;
        private final View view;
        private final Vector2 downPosition = new Vector2(0.0f, 0.0f);
        private boolean down = false;

        public MoveElementListener(ControllerItem id, View view) {
            this.view = view;
            this.id = id;
            this.view.setOnTouchListener(this);
        }

        @SuppressLint("ClickableViewAccessibility")
        @Override
        public boolean onTouch(View v, MotionEvent event) {
            if (!down) {
                down = true;
                downPosition.set(event.getX() - (view.getLayoutParams().width / 2.0f), event.getY() - (view.getLayoutParams().height / 2.0f));
            }

            int[] viewPosition = new int[2];
            getLocationOnScreen(viewPosition);

            int x = Math.max(0, Math.min(Math.round(event.getRawX() - viewPosition[0] - downPosition.x), width));
            int y = Math.max(0, Math.min(Math.round(event.getRawY() - viewPosition[1] - downPosition.y), height));

            profile.setLocation(id, x, y, width, height);
            profile.applyToView(id, view, width, height);

            if (changeListener != null) {
                changeListener.run(id);
            }

            selectedView = view;

            if (event.getAction() == MotionEvent.ACTION_UP) {
                selectedView = null;
                down = false;
                invalidate();
                return false;
            } else {
                return true;
            }
        }
    }
}
