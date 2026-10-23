package fizmo.android;

import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Vibrator;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class FizmoActivity extends NativeActivity {
    public static final int KEYBOARD_TEXT = 0;
    public static final int KEYBOARD_MULTILINE = 1;
    public static final int KEYBOARD_NUMBER = 2;
    public static final int KEYBOARD_DECIMAL = 3;
    public static final int KEYBOARD_EMAIL = 4;
    public static final int KEYBOARD_URL = 5;
    public static final int KEYBOARD_PASSWORD = 6;
    public static final int KEYBOARD_PHONE = 7;

    private TextSink sink;
    private int keyboardKind = KEYBOARD_TEXT;
    private boolean keyboardWanted = false;
    private boolean ready = false;

    native void nativeText(String text);
    native void nativeErase(int before, int after);
    native void nativeKey(int keyCode, int metaState, boolean down);
    native void nativeInsets(int left, int top, int right, int bottom, int ime);
    native void nativeDeviceRemoved(int deviceId);

    private InputManager.InputDeviceListener deviceListener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sink = new TextSink(this);
        addContentView(sink, new ViewGroup.LayoutParams(1, 1));
        sink.setFocusable(true);
        sink.setFocusableInTouchMode(true);
        sink.requestFocus();
        ready = true;
        if (Build.VERSION.SDK_INT >= 20) installInsetsListener();
        installDeviceListener();
    }

    @Override
    protected void onDestroy() {
        ready = false;
        try {
            InputManager im = (InputManager) getSystemService(Context.INPUT_SERVICE);
            if (im != null && deviceListener != null) im.unregisterInputDeviceListener(deviceListener);
        } catch (Throwable ignored) {
        }
        super.onDestroy();
    }

    private void installDeviceListener() {
        try {
            InputManager im = (InputManager) getSystemService(Context.INPUT_SERVICE);
            if (im == null) return;
            deviceListener = new InputManager.InputDeviceListener() {
                @Override
                public void onInputDeviceAdded(int deviceId) {
                }

                @Override
                public void onInputDeviceRemoved(int deviceId) {
                    if (!ready) return;
                    try {
                        nativeDeviceRemoved(deviceId);
                    } catch (Throwable ignored) {
                    }
                }

                @Override
                public void onInputDeviceChanged(int deviceId) {
                }
            };
            im.registerInputDeviceListener(deviceListener, null);
        } catch (Throwable ignored) {
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && keyboardWanted) showKeyboardNow();
    }

    private void installInsetsListener() {
        final View decor = getWindow().getDecorView();
        decor.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                reportInsets(insets);
                return v.onApplyWindowInsets(insets);
            }
        });
        decor.requestApplyInsets();
    }

    private void reportInsets(WindowInsets insets) {
        int left = 0, top = 0, right = 0, bottom = 0, ime = 0;
        boolean done = false;
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Class<?> type = Class.forName("android.view.WindowInsets$Type");
                int bars = (Integer) type.getMethod("systemBars").invoke(null);
                int cutout = (Integer) type.getMethod("displayCutout").invoke(null);
                int imeMask = (Integer) type.getMethod("ime").invoke(null);
                Method getInsets = WindowInsets.class.getMethod("getInsets", int.class);
                Object safe = getInsets.invoke(insets, bars | cutout);
                Object keyboard = getInsets.invoke(insets, imeMask);
                Class<?> box = safe.getClass();
                Field l = box.getField("left"), t = box.getField("top"), r = box.getField("right"), b = box.getField("bottom");
                left = l.getInt(safe);
                top = t.getInt(safe);
                right = r.getInt(safe);
                bottom = b.getInt(safe);
                ime = b.getInt(keyboard);
                done = true;
            } catch (Throwable ignored) {
            }
        }
        if (!done) {
            left = insets.getSystemWindowInsetLeft();
            top = insets.getSystemWindowInsetTop();
            right = insets.getSystemWindowInsetRight();
            bottom = insets.getSystemWindowInsetBottom();
            if (Build.VERSION.SDK_INT >= 28) {
                try {
                    Object cut = WindowInsets.class.getMethod("getDisplayCutout").invoke(insets);
                    if (cut != null) {
                        Class<?> c = cut.getClass();
                        left = Math.max(left, (Integer) c.getMethod("getSafeInsetLeft").invoke(cut));
                        top = Math.max(top, (Integer) c.getMethod("getSafeInsetTop").invoke(cut));
                        right = Math.max(right, (Integer) c.getMethod("getSafeInsetRight").invoke(cut));
                        bottom = Math.max(bottom, (Integer) c.getMethod("getSafeInsetBottom").invoke(cut));
                    }
                } catch (Throwable ignored) {
                }
            }
        }
        if (ready) {
            try {
                nativeInsets(left, top, right, bottom, ime);
            } catch (Throwable ignored) {
            }
        }
    }

    public void fizmoShowKeyboard(final int kind) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                keyboardWanted = true;
                if (kind != keyboardKind) {
                    keyboardKind = kind;
                    sink.resetComposing();
                    InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                    if (imm != null) imm.restartInput(sink);
                }
                showKeyboardNow();
            }
        });
    }

    private void showKeyboardNow() {
        sink.requestFocus();
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.showSoftInput(sink, 0);
    }

    public void fizmoHideKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                keyboardWanted = false;
                sink.resetComposing();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.hideSoftInputFromWindow(sink.getWindowToken(), 0);
            }
        });
    }

    public boolean fizmoSetClipboard(String text) {
        try {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm == null) return false;
            cm.setPrimaryClip(ClipData.newPlainText("text", text));
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    public String fizmoClipboard() {
        try {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm == null || !cm.hasPrimaryClip()) return "";
            ClipData clip = cm.getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) return "";
            CharSequence text = clip.getItemAt(0).coerceToText(this);
            return text == null ? "" : text.toString();
        } catch (Throwable e) {
            return "";
        }
    }

    public boolean fizmoVibrate(long ms) {
        try {
            Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            if (v == null || !v.hasVibrator()) return false;
            v.vibrate(ms);
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    public boolean fizmoOpenUrl(final String url) {
        try {
            final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        startActivity(intent);
                    } catch (Throwable ignored) {
                    }
                }
            });
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    public boolean fizmoShareText(final String text, final String title) {
        try {
            final Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("text/plain");
            send.putExtra(Intent.EXTRA_TEXT, text);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        startActivity(Intent.createChooser(send, title));
                    } catch (Throwable ignored) {
                    }
                }
            });
            return true;
        } catch (Throwable e) {
            return false;
        }
    }

    public void fizmoSetOrientation(final int orientation) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setRequestedOrientation(orientation);
            }
        });
    }

    public void fizmoSetImmersive(final boolean immersive) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                View decor = getWindow().getDecorView();
                int flags = 0;
                if (immersive) {
                    flags = View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
                          | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
                }
                decor.setSystemUiVisibility(flags);
            }
        });
    }

    public void fizmoKeepScreenOn(final boolean on) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (on) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public void fizmoMoveToBack() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                moveTaskToBack(true);
            }
        });
    }

    public String fizmoCacheDir() {
        try {
            return getCacheDir().getAbsolutePath();
        } catch (Throwable e) {
            return "";
        }
    }

    public String fizmoNativeLibraryDir() {
        try {
            return getApplicationInfo().nativeLibraryDir;
        } catch (Throwable e) {
            return "";
        }
    }

    public String fizmoLocale() {
        try {
            java.util.Locale l = java.util.Locale.getDefault();
            return l.toString();
        } catch (Throwable e) {
            return "";
        }
    }

    public int fizmoUnicodeChar(int keyCode, int metaState) {
        try {
            return new KeyEvent(KeyEvent.ACTION_DOWN, keyCode).getUnicodeChar(metaState);
        } catch (Throwable e) {
            return 0;
        }
    }

    int keyboardKind() {
        return keyboardKind;
    }

    void sendText(String text) {
        if (!ready || text == null || text.isEmpty()) return;
        try {
            nativeText(text);
        } catch (Throwable ignored) {
        }
    }

    void sendErase(int before, int after) {
        if (!ready || (before <= 0 && after <= 0)) return;
        try {
            nativeErase(Math.max(0, before), Math.max(0, after));
        } catch (Throwable ignored) {
        }
    }

    void sendKey(int keyCode, int metaState, boolean down) {
        if (!ready) return;
        try {
            nativeKey(keyCode, metaState, down);
        } catch (Throwable ignored) {
        }
    }

    static final class TextSink extends View {
        private final FizmoActivity owner;
        private SinkConnection connection;

        TextSink(FizmoActivity owner) {
            super(owner);
            this.owner = owner;
        }

        void resetComposing() {
            if (connection != null) connection.composing = "";
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo info) {
            switch (owner.keyboardKind()) {
                case KEYBOARD_MULTILINE:
                    info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
                    info.imeOptions = EditorInfo.IME_ACTION_NONE;
                    break;
                case KEYBOARD_NUMBER:
                    info.inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
                case KEYBOARD_DECIMAL:
                    info.inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED | InputType.TYPE_NUMBER_FLAG_DECIMAL;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
                case KEYBOARD_EMAIL:
                    info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
                case KEYBOARD_URL:
                    info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                    info.imeOptions = EditorInfo.IME_ACTION_GO;
                    break;
                case KEYBOARD_PASSWORD:
                    info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
                case KEYBOARD_PHONE:
                    info.inputType = InputType.TYPE_CLASS_PHONE;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
                default:
                    info.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES;
                    info.imeOptions = EditorInfo.IME_ACTION_DONE;
                    break;
            }
            info.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN;
            connection = new SinkConnection(this, owner);
            return connection;
        }
    }

    static final class SinkConnection extends BaseInputConnection {
        private final FizmoActivity owner;
        String composing = "";

        SinkConnection(View view, FizmoActivity owner) {
            super(view, false);
            this.owner = owner;
        }

        private void replaceComposing(String next) {
            if (next == null) next = "";
            int same = 0;
            int max = Math.min(composing.length(), next.length());
            while (same < max && composing.charAt(same) == next.charAt(same)) same++;
            if (same > 0 && Character.isHighSurrogate(composing.charAt(same - 1))) same--;
            int erase = composing.codePointCount(same, composing.length());
            if (erase > 0) owner.sendErase(erase, 0);
            if (same < next.length()) owner.sendText(next.substring(same));
            composing = next;
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            replaceComposing(text == null ? "" : text.toString());
            composing = "";
            return true;
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            replaceComposing(text == null ? "" : text.toString());
            return true;
        }

        @Override
        public boolean finishComposingText() {
            composing = "";
            return true;
        }

        @Override
        public boolean setComposingRegion(int start, int end) {
            return false;
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            composing = "";
            owner.sendErase(beforeLength, afterLength);
            return true;
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            int action = event.getAction();
            if (action == KeyEvent.ACTION_DOWN || action == KeyEvent.ACTION_UP) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_DEL && !composing.isEmpty()) {
                    if (action == KeyEvent.ACTION_DOWN) {
                        String next = composing.substring(0, composing.offsetByCodePoints(composing.length(), -1));
                        replaceComposing(next);
                    }
                    return true;
                }
                if (action == KeyEvent.ACTION_DOWN) composing = "";
                owner.sendKey(event.getKeyCode(), event.getMetaState(), action == KeyEvent.ACTION_DOWN);
            }
            return true;
        }

        @Override
        public boolean performEditorAction(int actionCode) {
            composing = "";
            owner.sendKey(KeyEvent.KEYCODE_ENTER, 0, true);
            owner.sendKey(KeyEvent.KEYCODE_ENTER, 0, false);
            return true;
        }

        @Override
        public CharSequence getTextBeforeCursor(int n, int flags) {
            return composing.length() <= n ? composing : composing.substring(composing.length() - n);
        }

        @Override
        public CharSequence getTextAfterCursor(int n, int flags) {
            return "";
        }
    }
}
