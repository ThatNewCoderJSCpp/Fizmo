#include "jni_android.hpp"

#if defined(OS_ANDROID)

#include <cstring>
#include <vector>

namespace fizmo {
namespace android {
namespace detail {

namespace {

struct ThreadDetacher {
    JavaVM* vm = nullptr;
    ~ThreadDetacher() { if (vm) vm->DetachCurrentThread(); }
};

thread_local ThreadDetacher t_detacher;

jmethodID method(JNIEnv* env, jclass cls, const char* name, const char* sig) noexcept {
    if (!env || !cls) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (clear_exception(env)) return nullptr;
    return id;
}

jmethodID static_method(JNIEnv* env, jclass cls, const char* name, const char* sig) noexcept {
    if (!env || !cls) return nullptr;
    jmethodID id = env->GetStaticMethodID(cls, name, sig);
    if (clear_exception(env)) return nullptr;
    return id;
}

jclass find(JNIEnv* env, const char* name) noexcept {
    if (!env) return nullptr;
    jclass cls = env->FindClass(name);
    if (clear_exception(env)) return nullptr;
    return cls;
}

jobject call_object(JNIEnv* env, jobject obj, const char* name, const char* sig) noexcept {
    if (!env || !obj) return nullptr;
    jclass cls = env->GetObjectClass(obj);
    jmethodID id = method(env, cls, name, sig);
    if (!id) return nullptr;
    jobject out = env->CallObjectMethod(obj, id);
    if (clear_exception(env)) return nullptr;
    return out;
}

std::string string_result(JNIEnv* env, jobject obj) {
    if (!obj) return std::string();
    return utf8_of(env, static_cast<jstring>(obj));
}

std::string file_path(JNIEnv* env, jobject file) {
    if (!file) return std::string();
    return string_result(env, call_object(env, file, "getAbsolutePath", "()Ljava/lang/String;"));
}

jobject system_service(JNIEnv* env, jobject activity, const char* name) {
    jclass cls = env->GetObjectClass(activity);
    jmethodID get = method(env, cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!get) return nullptr;
    jstring key = java_string(env, name);
    jobject service = env->CallObjectMethod(activity, get, key);
    if (clear_exception(env)) return nullptr;
    return service;
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

} // namespace

JNIEnv* env_for_thread(JavaVM* vm) noexcept {
    if (!vm) return nullptr;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return env;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    t_detacher.vm = vm;
    return env;
}

bool clear_exception(JNIEnv* env) noexcept {
    if (!env || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass find_class_in_hierarchy(JNIEnv* env, jobject object, const char* dotted_name) noexcept {
    if (!env || !object || !dotted_name) return nullptr;
    jclass class_class = find(env, "java/lang/Class");
    jmethodID get_name = method(env, class_class, "getName", "()Ljava/lang/String;");
    if (!get_name) return nullptr;
    jclass cls = env->GetObjectClass(object);
    while (cls) {
        jstring name = static_cast<jstring>(env->CallObjectMethod(cls, get_name));
        if (clear_exception(env)) name = nullptr;
        const bool match = name && utf8_of(env, name) == dotted_name;
        if (name) env->DeleteLocalRef(name);
        if (match) {
            env->DeleteLocalRef(class_class);
            return cls;
        }
        jclass parent = env->GetSuperclass(cls);
        env->DeleteLocalRef(cls);
        cls = parent;
    }
    env->DeleteLocalRef(class_class);
    return nullptr;
}

std::string utf8_of(JNIEnv* env, jstring s) {
    if (!env || !s) return std::string();
    const jsize n = env->GetStringLength(s);
    if (n <= 0) return std::string();
    std::vector<jchar> units(static_cast<std::size_t>(n));
    env->GetStringRegion(s, 0, n, units.data());
    if (clear_exception(env)) return std::string();
    std::string out;
    out.reserve(units.size());
    for (std::size_t i = 0; i < units.size(); ++i) {
        std::uint32_t cp = units[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (static_cast<std::uint32_t>(units[i + 1]) - 0xDC00);
            ++i;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        append_utf8(out, cp);
    }
    return out;
}

std::string utf8_of(std::uint32_t cp) {
    std::string out;
    append_utf8(out, cp);
    return out;
}

jstring java_string(JNIEnv* env, const std::string& utf8) {
    if (!env) return nullptr;
    std::vector<jchar> units;
    units.reserve(utf8.size());
    std::size_t i = 0;
    while (i < utf8.size()) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp = 0xFFFD;
        std::size_t len = 1;
        if (c < 0x80) { cp = c; }
        else if ((c >> 5) == 0x6 && i + 1 < utf8.size()) { cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu); len = 2; }
        else if ((c >> 4) == 0xE && i + 2 < utf8.size()) { cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu); len = 3; }
        else if ((c >> 3) == 0x1E && i + 3 < utf8.size()) { cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(utf8[i + 3]) & 0x3Fu); len = 4; }
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            units.push_back(static_cast<jchar>(0xD800 + (cp >> 10)));
            units.push_back(static_cast<jchar>(0xDC00 + (cp & 0x3FF)));
        } else {
            units.push_back(static_cast<jchar>(cp));
        }
    }
    jstring out = env->NewString(units.data(), static_cast<jsize>(units.size()));
    if (clear_exception(env)) return nullptr;
    return out;
}

JavaHooks load_java_hooks(JNIEnv* env, jobject activity) noexcept {
    JavaHooks h;
    h.activity = activity;
    if (!env || !activity) return h;
    LocalFrame frame(env, 16);
    jclass key_event = find(env, "android/view/KeyEvent");
    if (key_event) {
        h.key_event = static_cast<jclass>(env->NewGlobalRef(key_event));
        h.key_event_new = method(env, key_event, "<init>", "(II)V");
        h.key_event_char = method(env, key_event, "getUnicodeChar", "(I)I");
    }
    jclass cls = find_class_in_hierarchy(env, activity, "fizmo.android.FizmoActivity");
    if (!cls) return h;
    h.fizmo         = true;
    h.show_keyboard = method(env, cls, "fizmoShowKeyboard", "(I)V");
    h.hide_keyboard = method(env, cls, "fizmoHideKeyboard", "()V");
    h.set_clipboard = method(env, cls, "fizmoSetClipboard", "(Ljava/lang/String;)Z");
    h.get_clipboard = method(env, cls, "fizmoClipboard", "()Ljava/lang/String;");
    h.vibrate       = method(env, cls, "fizmoVibrate", "(J)Z");
    h.open_url      = method(env, cls, "fizmoOpenUrl", "(Ljava/lang/String;)Z");
    h.share_text    = method(env, cls, "fizmoShareText", "(Ljava/lang/String;Ljava/lang/String;)Z");
    h.orientation   = method(env, cls, "fizmoSetOrientation", "(I)V");
    h.immersive     = method(env, cls, "fizmoSetImmersive", "(Z)V");
    h.unicode_char  = method(env, cls, "fizmoUnicodeChar", "(II)I");
    h.pick          = method(env, cls, "fizmoPick", "(IIZLjava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Z");
    h.has_all_files = method(env, cls, "fizmoHasAllFilesAccess", "()Z");
    h.wants_all_files = method(env, cls, "fizmoDeclaresAllFilesAccess", "()Z");
    return h;
}

void release_java_hooks(JNIEnv* env, JavaHooks& h) noexcept {
    if (env && h.key_event) env->DeleteGlobalRef(h.key_event);
    h = JavaHooks{};
}

DeviceDetails device_details(JNIEnv* env, int device_id) {
    DeviceDetails d;
    if (!env) return d;
    LocalFrame frame(env, 16);
    jclass cls = find(env, "android/view/InputDevice");
    jmethodID get = static_method(env, cls, "getDevice", "(I)Landroid/view/InputDevice;");
    if (!get) return d;
    jobject dev = env->CallStaticObjectMethod(cls, get, static_cast<jint>(device_id));
    if (clear_exception(env) || !dev) return d;
    d.name = string_result(env, call_object(env, dev, "getName", "()Ljava/lang/String;"));
    if (jmethodID vendor = method(env, cls, "getVendorId", "()I")) {
        d.vendor = static_cast<std::uint16_t>(env->CallIntMethod(dev, vendor));
        clear_exception(env);
    }
    if (jmethodID product = method(env, cls, "getProductId", "()I")) {
        d.product = static_cast<std::uint16_t>(env->CallIntMethod(dev, product));
        clear_exception(env);
    }
    return d;
}

std::uint32_t unicode_char(JNIEnv* env, const JavaHooks& h, int code, int meta) noexcept {
    if (!env) return 0;
    if (h.fizmo && h.unicode_char && h.activity) {
        const jint c = env->CallIntMethod(h.activity, h.unicode_char, static_cast<jint>(code), static_cast<jint>(meta));
        if (!clear_exception(env)) return static_cast<std::uint32_t>(c);
    }
    if (!h.key_event || !h.key_event_new || !h.key_event_char) return 0;
    LocalFrame frame(env, 4);
    jobject ev = env->NewObject(h.key_event, h.key_event_new, static_cast<jint>(0), static_cast<jint>(code));
    if (clear_exception(env) || !ev) return 0;
    const jint c = env->CallIntMethod(ev, h.key_event_char, static_cast<jint>(meta));
    if (clear_exception(env)) return 0;
    return static_cast<std::uint32_t>(c);
}

std::string package_name(JNIEnv* env, jobject activity) {
    if (!env || !activity) return std::string();
    LocalFrame frame(env, 4);
    return string_result(env, call_object(env, activity, "getPackageName", "()Ljava/lang/String;"));
}

bool move_task_to_back(JNIEnv* env, const JavaHooks&, jobject activity) noexcept {
    if (!env || !activity) return false;
    LocalFrame frame(env, 4);
    jclass cls = env->GetObjectClass(activity);
    jmethodID id = method(env, cls, "moveTaskToBack", "(Z)Z");
    if (!id) return false;
    const jboolean ok = env->CallBooleanMethod(activity, id, JNI_TRUE);
    return !clear_exception(env) && ok == JNI_TRUE;
}

bool set_orientation(JNIEnv* env, const JavaHooks& h, jobject activity, int value) noexcept {
    if (!env || !activity) return false;
    if (h.fizmo && h.orientation) {
        env->CallVoidMethod(activity, h.orientation, static_cast<jint>(value));
        return !clear_exception(env);
    }
    LocalFrame frame(env, 4);
    jclass cls = env->GetObjectClass(activity);
    jmethodID id = method(env, cls, "setRequestedOrientation", "(I)V");
    if (!id) return false;
    env->CallVoidMethod(activity, id, static_cast<jint>(value));
    return !clear_exception(env);
}

bool set_immersive(JNIEnv* env, const JavaHooks& h, jobject activity, bool on) noexcept {
    if (!env || !activity || !h.fizmo || !h.immersive) return false;
    env->CallVoidMethod(activity, h.immersive, on ? JNI_TRUE : JNI_FALSE);
    return !clear_exception(env);
}

bool show_keyboard(JNIEnv* env, const JavaHooks& h, jobject activity, int kind) noexcept {
    if (!env || !activity || !h.fizmo || !h.show_keyboard) return false;
    env->CallVoidMethod(activity, h.show_keyboard, static_cast<jint>(kind));
    return !clear_exception(env);
}

bool hide_keyboard(JNIEnv* env, const JavaHooks& h, jobject activity) noexcept {
    if (!env || !activity || !h.fizmo || !h.hide_keyboard) return false;
    env->CallVoidMethod(activity, h.hide_keyboard);
    return !clear_exception(env);
}

std::string clipboard_text(JNIEnv* env, const JavaHooks& h, jobject activity) {
    if (!env || !activity) return std::string();
    LocalFrame frame(env, 16);
    if (h.fizmo && h.get_clipboard) {
        jobject s = env->CallObjectMethod(activity, h.get_clipboard);
        if (!clear_exception(env)) return string_result(env, s);
    }
    jobject cm = system_service(env, activity, "clipboard");
    if (!cm) return std::string();
    jobject clip = call_object(env, cm, "getPrimaryClip", "()Landroid/content/ClipData;");
    if (!clip) return std::string();
    jclass clip_cls = env->GetObjectClass(clip);
    jmethodID count = method(env, clip_cls, "getItemCount", "()I");
    jmethodID item_at = method(env, clip_cls, "getItemAt", "(I)Landroid/content/ClipData$Item;");
    if (!count || !item_at) return std::string();
    const jint n = env->CallIntMethod(clip, count);
    if (clear_exception(env) || n <= 0) return std::string();
    jobject item = env->CallObjectMethod(clip, item_at, static_cast<jint>(0));
    if (clear_exception(env) || !item) return std::string();
    jclass item_cls = env->GetObjectClass(item);
    jmethodID coerce = method(env, item_cls, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    if (!coerce) return std::string();
    jobject text = env->CallObjectMethod(item, coerce, activity);
    if (clear_exception(env) || !text) return std::string();
    return string_result(env, call_object(env, text, "toString", "()Ljava/lang/String;"));
}

bool set_clipboard_text(JNIEnv* env, const JavaHooks& h, jobject activity, const std::string& text) {
    if (!env || !activity) return false;
    LocalFrame frame(env, 16);
    jstring value = java_string(env, text);
    if (!value) return false;
    if (h.fizmo && h.set_clipboard) {
        const jboolean ok = env->CallBooleanMethod(activity, h.set_clipboard, value);
        if (!clear_exception(env)) return ok == JNI_TRUE;
    }
    jobject cm = system_service(env, activity, "clipboard");
    if (!cm) return false;
    jclass clip_data = find(env, "android/content/ClipData");
    jmethodID plain = static_method(env, clip_data, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (!plain) return false;
    jobject clip = env->CallStaticObjectMethod(clip_data, plain, java_string(env, "text"), value);
    if (clear_exception(env) || !clip) return false;
    jclass cm_cls = env->GetObjectClass(cm);
    jmethodID set = method(env, cm_cls, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (!set) return false;
    env->CallVoidMethod(cm, set, clip);
    return !clear_exception(env);
}

bool vibrate(JNIEnv* env, const JavaHooks& h, jobject activity, unsigned int ms) noexcept {
    if (!env || !activity || !h.fizmo || !h.vibrate) return false;
    const jboolean ok = env->CallBooleanMethod(activity, h.vibrate, static_cast<jlong>(ms));
    return !clear_exception(env) && ok == JNI_TRUE;
}

bool start_pick(JNIEnv* env, const JavaHooks& h, jobject activity, int id, int kind, bool multiple, const std::string& title, const std::string& name, const std::vector<std::string>& patterns, const std::string& start) noexcept {
    if (!env || !activity || !h.fizmo || !h.pick) return false;
    LocalFrame frame(env, static_cast<jint>(16 + patterns.size()));
    jclass string_cls = find(env, "java/lang/String");
    if (!string_cls) return false;
    jobjectArray list = env->NewObjectArray(static_cast<jsize>(patterns.size()), string_cls, nullptr);
    if (clear_exception(env) || !list) return false;
    for (std::size_t i = 0; i < patterns.size(); ++i) {
        jstring p = java_string(env, patterns[i]);
        env->SetObjectArrayElement(list, static_cast<jsize>(i), p);
        if (clear_exception(env)) return false;
    }
    const jboolean ok = env->CallBooleanMethod(activity, h.pick, static_cast<jint>(id), static_cast<jint>(kind), multiple ? JNI_TRUE : JNI_FALSE,
        java_string(env, title), java_string(env, name), list, java_string(env, start));
    return !clear_exception(env) && ok == JNI_TRUE;
}

bool has_all_files_access(JNIEnv* env, const JavaHooks& h, jobject activity) noexcept {
    if (!env || !activity || !h.fizmo || !h.has_all_files) return false;
    const jboolean ok = env->CallBooleanMethod(activity, h.has_all_files);
    return !clear_exception(env) && ok == JNI_TRUE;
}

bool declares_all_files_access(JNIEnv* env, const JavaHooks& h, jobject activity) noexcept {
    if (!env || !activity || !h.fizmo || !h.wants_all_files) return false;
    const jboolean ok = env->CallBooleanMethod(activity, h.wants_all_files);
    return !clear_exception(env) && ok == JNI_TRUE;
}

bool open_url(JNIEnv* env, const JavaHooks& h, jobject activity, const std::string& url) noexcept {
    if (!env || !activity) return false;
    LocalFrame frame(env, 16);
    jstring value = java_string(env, url);
    if (!value) return false;
    if (h.fizmo && h.open_url) {
        const jboolean ok = env->CallBooleanMethod(activity, h.open_url, value);
        if (!clear_exception(env)) return ok == JNI_TRUE;
    }
    jclass uri_cls = find(env, "android/net/Uri");
    jmethodID parse = static_method(env, uri_cls, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jclass intent_cls = find(env, "android/content/Intent");
    jmethodID ctor = method(env, intent_cls, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
    jmethodID flags = method(env, intent_cls, "addFlags", "(I)Landroid/content/Intent;");
    if (!parse || !ctor || !flags) return false;
    jobject uri = env->CallStaticObjectMethod(uri_cls, parse, value);
    if (clear_exception(env) || !uri) return false;
    jobject intent = env->NewObject(intent_cls, ctor, java_string(env, "android.intent.action.VIEW"), uri);
    if (clear_exception(env) || !intent) return false;
    env->CallObjectMethod(intent, flags, static_cast<jint>(0x10000000));
    clear_exception(env);
    jclass act_cls = env->GetObjectClass(activity);
    jmethodID start = method(env, act_cls, "startActivity", "(Landroid/content/Intent;)V");
    if (!start) return false;
    env->CallVoidMethod(activity, start, intent);
    return !clear_exception(env);
}

bool share_text(JNIEnv* env, const JavaHooks& h, jobject activity, const std::string& text, const std::string& title) noexcept {
    if (!env || !activity || !h.fizmo || !h.share_text) return false;
    LocalFrame frame(env, 8);
    const jboolean ok = env->CallBooleanMethod(activity, h.share_text, java_string(env, text), java_string(env, title));
    return !clear_exception(env) && ok == JNI_TRUE;
}

std::string cache_dir(JNIEnv* env, jobject activity) {
    if (!env || !activity) return std::string();
    LocalFrame frame(env, 8);
    return file_path(env, call_object(env, activity, "getCacheDir", "()Ljava/io/File;"));
}

std::string native_library_dir(JNIEnv* env, jobject activity) {
    if (!env || !activity) return std::string();
    LocalFrame frame(env, 8);
    jobject info = call_object(env, activity, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    if (!info) return std::string();
    jclass cls = env->GetObjectClass(info);
    jfieldID field = env->GetFieldID(cls, "nativeLibraryDir", "Ljava/lang/String;");
    if (clear_exception(env) || !field) return std::string();
    jobject dir = env->GetObjectField(info, field);
    if (clear_exception(env)) return std::string();
    return string_result(env, dir);
}

std::string locale(JNIEnv* env) {
    if (!env) return std::string();
    LocalFrame frame(env, 8);
    jclass cls = find(env, "java/util/Locale");
    jmethodID get = static_method(env, cls, "getDefault", "()Ljava/util/Locale;");
    if (!get) return std::string();
    jobject loc = env->CallStaticObjectMethod(cls, get);
    if (clear_exception(env) || !loc) return std::string();
    return string_result(env, call_object(env, loc, "toString", "()Ljava/lang/String;"));
}

float font_scale(JNIEnv* env, jobject activity) noexcept {
    if (!env || !activity) return 1.0f;
    LocalFrame frame(env, 8);
    jobject res = call_object(env, activity, "getResources", "()Landroid/content/res/Resources;");
    if (!res) return 1.0f;
    jobject config = call_object(env, res, "getConfiguration", "()Landroid/content/res/Configuration;");
    if (!config) return 1.0f;
    jclass cls = env->GetObjectClass(config);
    jfieldID field = env->GetFieldID(cls, "fontScale", "F");
    if (clear_exception(env) || !field) return 1.0f;
    const jfloat v = env->GetFloatField(config, field);
    if (clear_exception(env) || !(v > 0.0f)) return 1.0f;
    return v;
}

} // namespace detail
} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
