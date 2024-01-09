package com.panda3ds.pandroid.view.code;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.text.Editable;
import android.text.Layout;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Arrays;

public class BaseEditor extends BasicTextEditor {
    private static final String HELLO_WORLD = "Hello World";
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    private final Rect rect = new Rect();
    private int currentLine;
    private float spaceWidth;
    private int lineHeight;
    private int textOffset;
    private int beginLine;
    private int beginIndex;
    private int endLine;
    private int endIndex;
    private int visibleHeight;
    private int contentWidth;
    private Layout textLayout;

    private final char[] textBuffer = new char[1];
    protected final int[] colors = new int[256];

    //512KB OF BUFFER
    protected final byte[] syntaxBuffer = new byte[1024 * 512];
    private boolean requireUpdate = true;

    public BaseEditor(@NonNull Context context) {
        super(context);
    }

    public BaseEditor(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public BaseEditor(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    {
        EditorColors.obtainColorScheme(colors, getContext());
    }

    @SuppressLint("MissingSuperCall")
    @Override
    public void draw(Canvas canvas) {
        //super.draw(canvas);
        canvas.drawColor(colors[EditorColors.COLOR_BACKGROUND]);
        textLayout = getLayout();
        if (textLayout == null) {
            postDelayed(this::invalidate, 25);
            return;
        }

        try {
            prepareDraw();
            if (requireUpdate) {
                onVisibleContentChanged(beginIndex, endIndex - beginIndex);
            }

            if (getSelectionStart() == getSelectionEnd()) {
                drawCaret(canvas);
                drawCurrentLine(canvas);
            } else {
                drawSelection(canvas);
            }
            drawText(canvas);
            drawLineCount(canvas);
        } catch (Throwable e) {
            drawError(canvas, e);
        }
    }

    private void drawError(Canvas canvas, Throwable e) {
        canvas.drawColor(Color.RED);
        paint.setTextSize(getTextSize());
        paint.setColor(Color.WHITE);
        canvas.drawText("Editor draw error:", getPaddingLeft(), getLineHeight(), paint);
        canvas.drawText(String.valueOf(e), getPaddingLeft(), getLineHeight() * 2, paint);
        int index = 2;
        for (StackTraceElement trace : e.getStackTrace()) {
            index++;
            if (index > 5) break;
            canvas.drawText(trace.getClassName() + ":" + trace.getMethodName() + ":" + trace.getLineNumber(), getPaddingLeft(), getLineHeight() * index, paint);
        }
    }

    private void prepareDraw() {
        paint.setTypeface(getTypeface());
        paint.setTextSize(getTextSize());

        Paint.FontMetrics fontMetrics = paint.getFontMetrics();
        spaceWidth = paint.measureText(" ");
        lineHeight = getLineHeight();

        //Align text to center of line
        {
            int ascent = (int) Math.abs(fontMetrics.ascent);
            paint.getTextBounds(HELLO_WORLD, 0, HELLO_WORLD.length(), rect);
            ;
            textOffset = Math.max(((lineHeight - rect.height()) / 2), 0) + ascent;
        }

        int lineCount = textLayout.getLineCount();
        currentLine = textLayout.getLineForOffset(getSelectionStart());

        int oldBeginLine = beginLine;
        int oldEndLine = endLine;

        beginLine = Math.max(0, Math.min((getScrollY() / lineHeight) - 1, lineCount));
        beginIndex = textLayout.getLineStart(beginLine);

        if (oldEndLine != endLine || beginLine != oldBeginLine) {
            requireUpdate = true;
        }

        getGlobalVisibleRect(rect);
        visibleHeight = rect.height();

        endLine = Math.round(((float) visibleHeight / lineHeight) + 2) + beginLine;
        endIndex = getLayout().getLineStart(Math.min(lineCount, endLine));

        int padding = (int) (paint.measureText(String.valueOf(lineCount)) + (spaceWidth * 4));
        if (getPaddingLeft() != padding) {
            setPadding(padding, 0, 0, 0);
        }

        contentWidth = getWidth() + getScrollX();
    }

    private void drawLineCount(Canvas canvas) {
        int colorEnable = colors[EditorColors.COLOR_TEXT];
        int colorDisable = applyAlphaToColor(colors[EditorColors.COLOR_TEXT], 100);

        paint.setColor(colors[EditorColors.COLOR_BACKGROUND_SECONDARY]);
        int scrollY = getScrollY();
        float x = getScrollX();

        canvas.translate(x, 0);
        canvas.drawRect(0, scrollY, getPaddingLeft() - spaceWidth, visibleHeight + scrollY, paint);
        paint.setColor(applyAlphaToColor(colorEnable, 50));
        canvas.drawRect(0, currentLine * lineHeight, getPaddingLeft() - spaceWidth, (currentLine * lineHeight) + lineHeight, paint);

        for (int i = beginLine; i < Math.min(getLineCount(), endLine); i++) {
            String text = String.valueOf(i + 1);
            if (i == currentLine) {
                paint.setColor(colorEnable);
            } else {
                paint.setColor(colorDisable);
            }
            float width = paint.measureText(text);
            canvas.drawText(text, getPaddingLeft() - width - (spaceWidth * 2.5f), (i * lineHeight) + textOffset, paint);
        }

        paint.setColor(applyAlphaToColor(colorEnable, 10));
        canvas.drawRect(getPaddingLeft() - spaceWidth - (spaceWidth / 4), scrollY, getPaddingLeft() - spaceWidth, visibleHeight + scrollY, paint);

        canvas.translate(-x, 0);
    }

    private void drawCurrentLine(Canvas canvas) {
        float y = currentLine * lineHeight;
        paint.setColor(applyAlphaToColor(colors[EditorColors.COLOR_TEXT], 50));
        canvas.drawRect(0, y, contentWidth, y + lineHeight, paint);
    }

    private void drawText(Canvas canvas) {
        Editable edit = getText();
        float x = 0;
        float y = textOffset;
        int line = 0;

        canvas.translate(getPaddingLeft(), beginLine * lineHeight);

        paint.setColor(colors[EditorColors.COLOR_TEXT]);
        for (int i = beginIndex; i < endIndex; i++) {
            textBuffer[0] = edit.charAt(i);
            switch (textBuffer[0]) {
                case '\n':
                    x = 0;
                    line++;
                    y = (line * lineHeight) + textOffset;
                    break;
                case ' ':
                    x += spaceWidth;
                    break;
                default:
                    paint.setColor(colors[syntaxBuffer[i - beginIndex]]);
                    canvas.drawText(textBuffer, 0, 1, x, y, paint);
                    x += paint.measureText(textBuffer, 0, 1);
                    break;
            }
        }

        canvas.translate(-getPaddingLeft(), -(beginLine * lineHeight));
    }

    private void drawCaret(Canvas canvas) {
        int start = textLayout.getLineStart(currentLine);
        int end = textLayout.getLineEnd(currentLine);
        int position = getSelectionStart();
        float x = getPaddingLeft();
        float y = (currentLine * lineHeight);
        Editable text = getText();
        for (int i = start; i < end; i++) {
            if (i == position) break;
            textBuffer[0] = text.charAt(i);
            x += paint.measureText(textBuffer, 0, 1);
        }

        paint.setColor(colors[EditorColors.COLOR_CARET]);
        float caretWidth = spaceWidth / 2;
        canvas.drawRect(x - (caretWidth / 2), y, x + (caretWidth / 2), y + lineHeight, paint);
    }

    private void drawSelection(Canvas canvas) {
        int start = getSelectionStart();
        int end = getSelectionEnd();
        int endLine = textLayout.getLineForOffset(end);
        canvas.translate(getPaddingLeft(), 0);

        paint.setColor(colors[EditorColors.COLOR_SELECTION]);

        Editable text = getText();

        for (int line = currentLine; line <= endLine; line++) {

            if (line < beginLine) continue;
            if (line > this.endLine) break;

            if (line == endLine || line == currentLine) {
                int lineStart = textLayout.getLineStart(line);
                float x = 0;

                if (lineStart <= start) {
                    x = paint.measureText(text, lineStart, start);
                    lineStart = start;
                }
                float width;
                if (line < endLine) {
                    width = contentWidth;
                } else {
                    width = paint.measureText(text, lineStart, end);
                }

                canvas.drawRect(x, lineHeight * line, x + width, (lineHeight * line) + lineHeight, paint);
            } else {
                canvas.drawRect(0, lineHeight * line, contentWidth, (lineHeight * line) + lineHeight, paint);
            }
        }
        canvas.translate(-getPaddingLeft(), 0);
    }

    public int applyAlphaToColor(int color, int alpha) {
        return Color.argb(alpha, Color.red(color), Color.green(color), Color.blue(color));
    }

    protected void onVisibleContentChanged(int index, int length) {
        requireUpdate = false;

        Arrays.fill(syntaxBuffer, (byte) 0);
        if (length > 0) {
            onRefreshColorScheme(syntaxBuffer, index, length);
        }
    }

    protected void onRefreshColorScheme(byte[] buffer, int index, int length) {
    }

    protected void invalidateAll(){
        requireUpdate = true;
        invalidate();
    }

    @Override
    protected void onTextChanged() {
        super.onTextChanged();
        requireUpdate = true;
    }
}