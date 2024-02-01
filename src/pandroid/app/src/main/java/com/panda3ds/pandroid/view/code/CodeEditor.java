package com.panda3ds.pandroid.view.code;

import android.content.Context;
import android.util.AttributeSet;

import com.panda3ds.pandroid.view.code.syntax.CodeSyntax;

public class CodeEditor extends BaseEditor {
    private CodeSyntax syntax;
    private Runnable contentChangeListener;

    public CodeEditor(Context context) {
        super(context);
    }

    public CodeEditor(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public CodeEditor(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void setSyntax(CodeSyntax syntax) {
        this.syntax = syntax;
        invalidateAll();
    }

    public void setOnContentChangedListener(Runnable contentChangeListener) {
        this.contentChangeListener = contentChangeListener;
    }

    @Override
    protected void onTextChanged() {
        super.onTextChanged();
        if (contentChangeListener != null) {
            contentChangeListener.run();
        }
    }

    @Override
    protected void onRefreshColorScheme(byte[] buffer, int index, int length) {
        super.onRefreshColorScheme(buffer, index, length);

        if (syntax != null) {
            final CharSequence text = getText().subSequence(index, index + length);
            syntax.apply(syntaxBuffer, text);
            System.gc();
        }
    }
}
