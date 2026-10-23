package fizmo.android;

import android.Manifest;
import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;
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

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class FizmoActivity extends NativeActivity {
    public static final int KEYBOARD_TEXT = 0;
    public static final int KEYBOARD_MULTILINE = 1;
    public static final int KEYBOARD_NUMBER = 2;
    public static final int KEYBOARD_DECIMAL = 3;
    public static final int KEYBOARD_EMAIL = 4;
    public static final int KEYBOARD_URL = 5;
    public static final int KEYBOARD_PASSWORD = 6;
    public static final int KEYBOARD_PHONE = 7;

    public static final int PICK_OPEN = 0;
    public static final int PICK_SAVE = 1;
    public static final int PICK_FOLDER = 2;

    private static final int REQUEST_PICK = 0x46a0;
    private static final int REQUEST_ALL_FILES = 0x46a1;
    private static final int REQUEST_STORAGE = 0x46a2;
    private static final String ALL_FILES_PERMISSION = "android.permission.MANAGE_EXTERNAL_STORAGE";
    private static final String ALL_FILES_SETTINGS = "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION";
    private static final String ALL_FILES_SETTINGS_ANY = "android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION";
    private static final String EXTRA_INITIAL_URI = "android.provider.extra.INITIAL_URI";
    private static final String EXTERNAL_STORAGE_AUTHORITY = "com.android.externalstorage.documents";
    private static final String DOWNLOADS_AUTHORITY = "com.android.providers.downloads.documents";

    private static final class PickRequest {
        int id;
        int kind;
        boolean multiple;
        String title;
        String name;
        String[] patterns;
        String start;
    }

    private PickRequest pick;
    private boolean askedForAllFiles = false;

    private TextSink sink;
    private int keyboardKind = KEYBOARD_TEXT;
    private boolean keyboardWanted = false;
    private boolean ready = false;

    native void nativeText(String text);
    native void nativeErase(int before, int after);
    native void nativeKey(int keyCode, int metaState, boolean down);
    native void nativeInsets(int left, int top, int right, int bottom, int ime);
    native void nativeDeviceRemoved(int deviceId);
    native void nativePicked(int id, String[] paths, String error, boolean cancelled);

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
        if (pick != null) {
            finishPick(pick, null, "", true);
            pick = null;
        }
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

    public boolean fizmoHasAllFilesAccess() {
        return hasAllFilesAccess();
    }

    public boolean fizmoDeclaresAllFilesAccess() {
        return declaresAllFilesAccess();
    }

    public boolean fizmoPick(final int id, final int kind, final boolean multiple, final String title, final String name, final String[] patterns, final String start) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (pick != null) finishPick(pick, null, "", true);
                PickRequest r = new PickRequest();
                r.id = id;
                r.kind = kind;
                r.multiple = multiple;
                r.title = title == null ? "" : title;
                r.name = name == null ? "" : name;
                r.patterns = patterns == null ? new String[0] : patterns;
                r.start = start == null ? "" : start;
                pick = r;
                continuePick();
            }
        });
        return true;
    }

    private void continuePick() {
        PickRequest r = pick;
        if (r == null) return;
        if (!askedForAllFiles && declaresAllFilesAccess() && !hasAllFilesAccess()) {
            askedForAllFiles = true;
            if (askForAllFilesAccess()) return;
        }
        launchPicker(r);
    }

    private boolean declaresAllFilesAccess() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), PackageManager.GET_PERMISSIONS);
            if (info.requestedPermissions == null) return false;
            for (String p : info.requestedPermissions) {
                if (ALL_FILES_PERMISSION.equals(p)) return true;
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    private boolean hasAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Method m = Environment.class.getMethod("isExternalStorageManager");
                return (Boolean) m.invoke(null);
            } catch (Throwable e) {
                return false;
            }
        }
        if (Build.VERSION.SDK_INT >= 23) {
            return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private boolean askForAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent intent = new Intent(ALL_FILES_SETTINGS, Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, REQUEST_ALL_FILES);
                return true;
            } catch (Throwable e) {
                try {
                    startActivityForResult(new Intent(ALL_FILES_SETTINGS_ANY), REQUEST_ALL_FILES);
                    return true;
                } catch (Throwable ignored) {
                    return false;
                }
            }
        }
        if (Build.VERSION.SDK_INT >= 23) {
            requestPermissions(new String[] { Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQUEST_STORAGE);
            return true;
        }
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_STORAGE) continuePick();
    }

    private String mimeFor(String pattern) {
        String p = pattern.trim();
        if (p.isEmpty() || p.equals("*") || p.equals("*.*")) return null;
        if (p.contains("/")) return p;
        int dot = p.lastIndexOf('.');
        String ext = dot >= 0 ? p.substring(dot + 1) : p;
        if (ext.isEmpty() || ext.contains("*")) return null;
        String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext.toLowerCase());
        return mime;
    }

    private String[] mimeTypes(String[] patterns) {
        List<String> out = new ArrayList<String>();
        for (String p : patterns) {
            String m = mimeFor(p);
            if (m == null) return new String[0];
            if (!out.contains(m)) out.add(m);
        }
        return out.toArray(new String[0]);
    }

    private Uri initialUri(String start) {
        if (start == null || start.isEmpty()) return null;
        try {
            String root = Environment.getExternalStorageDirectory().getAbsolutePath();
            String path = start;
            File f = new File(path);
            if (f.isFile()) path = f.getParent();
            if (path == null || !path.startsWith(root)) return null;
            String rel = path.substring(root.length());
            while (rel.startsWith("/")) rel = rel.substring(1);
            return DocumentsContract.buildDocumentUri(EXTERNAL_STORAGE_AUTHORITY, "primary:" + rel);
        } catch (Throwable e) {
            return null;
        }
    }

    private void launchPicker(PickRequest r) {
        Intent intent;
        String[] mimes = mimeTypes(r.patterns);
        if (r.kind == PICK_FOLDER) {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        } else if (r.kind == PICK_SAVE) {
            intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mimes.length > 0 ? mimes[0] : "application/octet-stream");
            if (!r.name.isEmpty()) intent.putExtra(Intent.EXTRA_TITLE, r.name);
        } else {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            if (mimes.length == 1) {
                intent.setType(mimes[0]);
            } else {
                intent.setType("*/*");
                if (mimes.length > 1) intent.putExtra(Intent.EXTRA_MIME_TYPES, mimes);
            }
            if (r.multiple) intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        Uri initial = initialUri(r.start);
        if (initial != null && Build.VERSION.SDK_INT >= 26) intent.putExtra(EXTRA_INITIAL_URI, initial);
        try {
            startActivityForResult(intent, REQUEST_PICK);
        } catch (Throwable e) {
            finishPick(r, null, "this phone has no app for choosing files", false);
            pick = null;
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_ALL_FILES) {
            continuePick();
            return;
        }
        if (requestCode != REQUEST_PICK) return;
        final PickRequest r = pick;
        pick = null;
        if (r == null) return;
        if (resultCode != RESULT_OK || data == null) {
            finishPick(r, null, "", true);
            return;
        }
        final List<Uri> uris = new ArrayList<Uri>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u != null) uris.add(u);
            }
        }
        if (uris.isEmpty() && data.getData() != null) uris.add(data.getData());
        if (uris.isEmpty()) {
            finishPick(r, null, "", true);
            return;
        }
        final int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        for (Uri u : uris) {
            try {
                getContentResolver().takePersistableUriPermission(u, flags);
            } catch (Throwable ignored) {
            }
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                resolvePicked(r, uris);
            }
        }, "fizmo-pick").start();
    }

    private void resolvePicked(PickRequest r, List<Uri> uris) {
        List<String> paths = new ArrayList<String>();
        String error = "";
        for (Uri u : uris) {
            String path = pathOf(u, r.kind == PICK_FOLDER);
            if (r.kind == PICK_FOLDER) {
                if (path == null) {
                    error = "that folder isn't on this phone's storage, so it can't be opened as a folder";
                } else if (!new File(path).canRead()) {
                    error = declaresAllFilesAccess()
                        ? "allow \"All files access\" for this app in Settings to open that folder"
                        : "this app can't read folders outside its own storage";
                    path = null;
                }
            } else if (r.kind == PICK_SAVE) {
                if (path == null) {
                    error = "can't save there; choose a place on this phone's storage";
                } else if (!new File(path).canWrite()) {
                    error = declaresAllFilesAccess()
                        ? "allow \"All files access\" for this app in Settings to save there"
                        : "this app can't save outside its own storage";
                    path = null;
                }
            } else {
                if (path == null || !new File(path).canRead()) {
                    String copy = copyToCache(u);
                    if (copy != null) path = copy;
                    else {
                        path = null;
                        error = "can't read that file";
                    }
                }
            }
            if (path != null) paths.add(path);
        }
        finishPick(r, paths.toArray(new String[0]), paths.isEmpty() ? error : "", false);
    }

    private void finishPick(PickRequest r, String[] paths, String error, boolean cancelled) {
        if (!ready) return;
        try {
            nativePicked(r.id, paths == null ? new String[0] : paths, error == null ? "" : error, cancelled);
        } catch (Throwable ignored) {
        }
    }

    private String externalStoragePath(String docId) {
        int colon = docId.indexOf(':');
        String type = colon >= 0 ? docId.substring(0, colon) : docId;
        String rel = colon >= 0 ? docId.substring(colon + 1) : "";
        String base;
        if ("primary".equalsIgnoreCase(type)) {
            base = Environment.getExternalStorageDirectory().getAbsolutePath();
        } else if ("home".equalsIgnoreCase(type)) {
            base = new File(Environment.getExternalStorageDirectory(), "Documents").getAbsolutePath();
        } else if ("raw".equalsIgnoreCase(type)) {
            return rel;
        } else {
            base = "/storage/" + type;
        }
        return rel.isEmpty() ? base : base + "/" + rel;
    }

    private String dataColumn(Uri uri) {
        Cursor c = null;
        try {
            c = getContentResolver().query(uri, new String[] { "_data" }, null, null, null);
            if (c != null && c.moveToFirst()) {
                int i = c.getColumnIndex("_data");
                if (i >= 0) {
                    String v = c.getString(i);
                    if (v != null && !v.isEmpty()) return v;
                }
            }
        } catch (Throwable ignored) {
        } finally {
            if (c != null) c.close();
        }
        return null;
    }

    private String pathOf(Uri uri, boolean tree) {
        try {
            if (ContentResolver.SCHEME_FILE.equals(uri.getScheme())) return uri.getPath();
            String authority = uri.getAuthority();
            String docId;
            if (tree) docId = DocumentsContract.getTreeDocumentId(uri);
            else if (DocumentsContract.isDocumentUri(this, uri)) docId = DocumentsContract.getDocumentId(uri);
            else docId = null;
            if (docId != null && EXTERNAL_STORAGE_AUTHORITY.equals(authority)) return externalStoragePath(docId);
            if (docId != null && DOWNLOADS_AUTHORITY.equals(authority)) {
                if (docId.startsWith("raw:")) return docId.substring(4);
                if (tree) return Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getAbsolutePath();
            }
            if (tree) return null;
            return dataColumn(uri);
        } catch (Throwable e) {
            return null;
        }
    }

    private String displayName(Uri uri) {
        Cursor c = null;
        try {
            c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null);
            if (c != null && c.moveToFirst()) {
                String v = c.getString(0);
                if (v != null && !v.isEmpty()) return v.replace('/', '_');
            }
        } catch (Throwable ignored) {
        } finally {
            if (c != null) c.close();
        }
        String last = uri.getLastPathSegment();
        if (last == null || last.isEmpty()) return "file";
        int slash = last.lastIndexOf('/');
        return slash >= 0 ? last.substring(slash + 1) : last.replace(':', '_');
    }

    private String copyToCache(Uri uri) {
        InputStream in = null;
        OutputStream out = null;
        try {
            File dir = new File(getCacheDir(), "picked/" + System.nanoTime());
            if (!dir.mkdirs() && !dir.isDirectory()) return null;
            File dest = new File(dir, displayName(uri));
            in = getContentResolver().openInputStream(uri);
            if (in == null) return null;
            out = new FileOutputStream(dest);
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return dest.getAbsolutePath();
        } catch (Throwable e) {
            return null;
        } finally {
            try { if (in != null) in.close(); } catch (Throwable ignored) {}
            try { if (out != null) out.close(); } catch (Throwable ignored) {}
        }
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
