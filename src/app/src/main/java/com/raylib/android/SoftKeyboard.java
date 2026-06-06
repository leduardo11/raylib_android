package com.raylib.android;

import android.view.inputmethod.InputMethodManager;
import android.app.NativeActivity;
import android.content.Context;
import android.view.KeyEvent;

public class SoftKeyboard {

    private final Context context;
    private final InputMethodManager imm;
    private KeyEvent lastKeyEvent = null;

    public SoftKeyboard(Context context) {
        imm = (InputMethodManager)context.getSystemService(Context.INPUT_METHOD_SERVICE);
        this.context = context;
    }

    public void showKeyboard() {
        imm.showSoftInput(((NativeActivity)context).getWindow().getDecorView(), InputMethodManager.SHOW_FORCED);
    }

    public void hideKeyboard() {
        imm.hideSoftInputFromWindow(((NativeActivity)context).getWindow().getDecorView().getWindowToken(), 0);
    }

    public int getLastKeyCode() {
        if (lastKeyEvent != null) return lastKeyEvent.getKeyCode();
        return 0;
    }

    public char getLastKeyLabel() {
        if (lastKeyEvent != null) return lastKeyEvent.getDisplayLabel();
        return '\0';
    }

    public int getLastKeyUnicode() {
        if (lastKeyEvent != null) return lastKeyEvent.getUnicodeChar();
        return 0;
    }

    public void clearLastKeyEvent() {
        lastKeyEvent = null;
    }

    public void onKeyUpEvent(KeyEvent event) {
        lastKeyEvent = event;
    }
}
