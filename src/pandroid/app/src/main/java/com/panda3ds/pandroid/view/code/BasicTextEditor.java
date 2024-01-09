package com.panda3ds.pandroid.view.code;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.inputmethod.EditorInfo;
import android.widget.Scroller;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatEditText;

import com.panda3ds.pandroid.view.SimpleTextWatcher;

public class BasicTextEditor extends AppCompatEditText {
    private GestureDetector gestureDetector;

    public BasicTextEditor(@NonNull Context context) {
        super(context);
        initialize();
    }

    public BasicTextEditor(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        initialize();
    }

    public BasicTextEditor(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initialize();
    }

    protected void initialize() {
        setTypeface(Typeface.MONOSPACE);
        gestureDetector = new GestureDetector(getContext(), new ScrollGesture());

        setTypeface(Typeface.createFromAsset(getContext().getAssets(), "fonts/comic_mono.ttf"));
        setGravity(Gravity.START | Gravity.TOP);
        setTextSize(TypedValue.COMPLEX_UNIT_SP,16);
        setLineSpacing(0, 1.3f);
        setScroller(new Scroller(getContext()));


        setInputType(InputType.TYPE_CLASS_TEXT |
                InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS |
                InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE |
                InputType.TYPE_TEXT_FLAG_MULTI_LINE |
                InputType.TYPE_TEXT_FLAG_AUTO_CORRECT);

        setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        setBackgroundColor(Color.BLACK);
        setTextColor(Color.WHITE);

        setFocusableInTouchMode(true);
        setHorizontallyScrolling(true);
        setHorizontalScrollBarEnabled(true);

        addTextChangedListener((SimpleTextWatcher) value -> BasicTextEditor.this.onTextChanged());
    }

    // DISABLE ANDROID DEFAULT SCROLL
    @Override
    public void scrollBy(int x, int y) {
    }

    @Override
    public void scrollTo(int x, int y) {
    }

    public void setScroll(int x, int y) {
        super.scrollTo(x, y);
    }

    protected void onTextChanged() {
    }

    private boolean onSuperTouchListener(MotionEvent event) {
        return super.onTouchEvent(event);
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return gestureDetector.onTouchEvent(event);
    }

    private class ScrollGesture implements GestureDetector.OnGestureListener {
        private final Rect visibleRect = new Rect();

        @Override
        public boolean onDown(@NonNull MotionEvent e) {
            return true;
        }

        @Override
        public void onShowPress(@NonNull MotionEvent e) {
            onSuperTouchListener(e);
        }

        @Override
        public boolean onSingleTapUp(@NonNull MotionEvent e) {
            return onSuperTouchListener(e);
        }

        @Override
        public boolean onScroll(@NonNull MotionEvent e1, @NonNull MotionEvent e2, float distanceX, float distanceY) {
            int scrollX = (int) Math.max(0, getScrollX() + distanceX);
            int scrollY = (int) Math.max(0, getScrollY() + distanceY);
            int maxHeight = Math.round(getLineCount() * getLineHeight());
            getGlobalVisibleRect(visibleRect);
            maxHeight = Math.max(0, maxHeight - visibleRect.height());

            int maxWidth = (int) getPaint().measureText(getText(), 0, length());
            maxWidth += getPaddingLeft() + getPaddingRight();

            scrollX = Math.max(Math.min(maxWidth - visibleRect.width(), scrollX), 0);

            setScroll(scrollX, Math.min(maxHeight, scrollY));
            return true;
        }

        @Override
        public void onLongPress(@NonNull MotionEvent e) {
            onSuperTouchListener(e);
        }

        @Override
        public boolean onFling(@NonNull MotionEvent e1, @NonNull MotionEvent e2, float velocityX, float velocityY) {
            return false;
        }
    }

    public void insert(CharSequence text) {
        if (getSelectionStart() == getSelectionEnd()) {
            getText().insert(getSelectionStart(), text);
        } else {
            getText().replace(getSelectionStart(), getSelectionEnd(), text);
        }
    }
}
