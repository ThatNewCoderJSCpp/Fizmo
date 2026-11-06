#include "backend.hpp"
#include "format.hpp"
#include "resampler.hpp"

#if defined(__ANDROID__)

#include "../Android/jni_android.hpp"

#include <jni.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <utility>

#if defined(__has_include)
#if __has_include(<aaudio/AAudio.h>)
#define FIZMO_AUDIO_HAVE_AAUDIO_HEADERS 1
#include <aaudio/AAudio.h>
#endif
#endif

extern "C" void* fizmo_android_app_java_vm() __attribute__((weak));
extern "C" void* fizmo_android_app_activity() __attribute__((weak));

namespace fizmo {
namespace audio {
namespace detail {

namespace {

namespace aa {

struct stream;
struct builder;

constexpr std::int32_t unspecified = 0;
constexpr std::int32_t ok = 0;
constexpr std::int32_t error_disconnected = -899;
constexpr std::int32_t direction_output = 0;
constexpr std::int32_t direction_input = 1;
constexpr std::int32_t format_unspecified = 0;
constexpr std::int32_t format_i16 = 1;
constexpr std::int32_t format_float = 2;
constexpr std::int32_t format_i24_packed = 3;
constexpr std::int32_t format_i32 = 4;
constexpr std::int32_t sharing_shared = 1;
constexpr std::int32_t performance_low_latency = 12;
constexpr std::int32_t usage_media = 1;
constexpr std::int32_t callback_continue = 0;
constexpr std::int32_t state_stopping = 9;
constexpr std::int32_t state_stopped = 10;
constexpr std::int32_t state_closing = 11;
constexpr std::int32_t state_closed = 12;
constexpr std::int32_t state_disconnected = 13;

using data_callback = std::int32_t (*)(stream*, void*, void*, std::int32_t);
using error_callback = void (*)(stream*, void*, std::int32_t);

} // namespace aa

#if defined(FIZMO_AUDIO_HAVE_AAUDIO_HEADERS)
static_assert(AAUDIO_UNSPECIFIED == aa::unspecified && AAUDIO_OK == aa::ok, "AAudio basic values");
static_assert(AAUDIO_ERROR_DISCONNECTED == aa::error_disconnected, "AAudio disconnect value");
static_assert(AAUDIO_DIRECTION_OUTPUT == aa::direction_output && AAUDIO_DIRECTION_INPUT == aa::direction_input, "AAudio direction values");
static_assert(AAUDIO_FORMAT_UNSPECIFIED == aa::format_unspecified && AAUDIO_FORMAT_PCM_I16 == aa::format_i16 && AAUDIO_FORMAT_PCM_FLOAT == aa::format_float, "AAudio format values");
static_assert(AAUDIO_SHARING_MODE_SHARED == aa::sharing_shared && AAUDIO_PERFORMANCE_MODE_LOW_LATENCY == aa::performance_low_latency, "AAudio mode values");
static_assert(AAUDIO_USAGE_MEDIA == aa::usage_media && AAUDIO_CALLBACK_RESULT_CONTINUE == aa::callback_continue, "AAudio usage and callback values");
static_assert(AAUDIO_STREAM_STATE_STOPPING == aa::state_stopping && AAUDIO_STREAM_STATE_STOPPED == aa::state_stopped, "AAudio state values");
static_assert(AAUDIO_STREAM_STATE_CLOSING == aa::state_closing && AAUDIO_STREAM_STATE_CLOSED == aa::state_closed && AAUDIO_STREAM_STATE_DISCONNECTED == aa::state_disconnected, "AAudio state values");
#endif

int api_level() {
    static const int level = [] {
        char value[PROP_VALUE_MAX];
        std::memset(value, 0, sizeof(value));
        if (__system_property_get("ro.build.version.sdk", value) <= 0) return 0;
        return std::atoi(value);
    }();
    return level;
}

struct AAudioApi {
    DynamicLibrary lib;

    std::int32_t (*create_builder)(aa::builder**) = nullptr;
    void (*set_device_id)(aa::builder*, std::int32_t) = nullptr;
    void (*set_direction)(aa::builder*, std::int32_t) = nullptr;
    void (*set_sharing_mode)(aa::builder*, std::int32_t) = nullptr;
    void (*set_sample_rate)(aa::builder*, std::int32_t) = nullptr;
    void (*set_channel_count)(aa::builder*, std::int32_t) = nullptr;
    void (*set_format)(aa::builder*, std::int32_t) = nullptr;
    void (*set_performance_mode)(aa::builder*, std::int32_t) = nullptr;
    void (*set_data_callback)(aa::builder*, aa::data_callback, void*) = nullptr;
    void (*set_error_callback)(aa::builder*, aa::error_callback, void*) = nullptr;
    void (*set_usage)(aa::builder*, std::int32_t) = nullptr;
    std::int32_t (*open_stream)(aa::builder*, aa::stream**) = nullptr;
    std::int32_t (*delete_builder)(aa::builder*) = nullptr;
    std::int32_t (*close)(aa::stream*) = nullptr;
    std::int32_t (*request_start)(aa::stream*) = nullptr;
    std::int32_t (*request_stop)(aa::stream*) = nullptr;
    std::int32_t (*get_state)(aa::stream*) = nullptr;
    std::int32_t (*wait_for_state_change)(aa::stream*, std::int32_t, std::int32_t*, std::int64_t) = nullptr;
    std::int32_t (*get_sample_rate)(aa::stream*) = nullptr;
    std::int32_t (*get_channel_count)(aa::stream*) = nullptr;
    std::int32_t (*get_format)(aa::stream*) = nullptr;
    std::int32_t (*get_frames_per_burst)(aa::stream*) = nullptr;
    std::int32_t (*get_buffer_size)(aa::stream*) = nullptr;
    std::int32_t (*set_buffer_size)(aa::stream*, std::int32_t) = nullptr;
    std::int32_t (*get_buffer_capacity)(aa::stream*) = nullptr;
    std::int32_t (*get_device_id)(aa::stream*) = nullptr;
    const char* (*result_text)(std::int32_t) = nullptr;

    bool load(std::string& why) {
        if (api_level() != 0 && api_level() < 26) {
            why = "AAudio needs Android 8.0 (API 26) or newer";
            return false;
        }
        if (!lib.open({ "libaaudio.so" })) {
            why = "libaaudio.so isn't available (AAudio needs Android 8.0 or newer)";
            return false;
        }
        bool good = true;
        good &= lib.load(create_builder, "AAudio_createStreamBuilder");
        good &= lib.load(set_device_id, "AAudioStreamBuilder_setDeviceId");
        good &= lib.load(set_direction, "AAudioStreamBuilder_setDirection");
        good &= lib.load(set_sharing_mode, "AAudioStreamBuilder_setSharingMode");
        good &= lib.load(set_sample_rate, "AAudioStreamBuilder_setSampleRate");
        good &= lib.load(set_channel_count, "AAudioStreamBuilder_setChannelCount");
        good &= lib.load(set_format, "AAudioStreamBuilder_setFormat");
        good &= lib.load(set_performance_mode, "AAudioStreamBuilder_setPerformanceMode");
        good &= lib.load(set_data_callback, "AAudioStreamBuilder_setDataCallback");
        good &= lib.load(set_error_callback, "AAudioStreamBuilder_setErrorCallback");
        good &= lib.load(open_stream, "AAudioStreamBuilder_openStream");
        good &= lib.load(delete_builder, "AAudioStreamBuilder_delete");
        good &= lib.load(close, "AAudioStream_close");
        good &= lib.load(request_start, "AAudioStream_requestStart");
        good &= lib.load(request_stop, "AAudioStream_requestStop");
        good &= lib.load(get_state, "AAudioStream_getState");
        good &= lib.load(wait_for_state_change, "AAudioStream_waitForStateChange");
        good &= lib.load(get_sample_rate, "AAudioStream_getSampleRate");
        good &= lib.load(get_channel_count, "AAudioStream_getChannelCount");
        good &= lib.load(get_format, "AAudioStream_getFormat");
        good &= lib.load(get_frames_per_burst, "AAudioStream_getFramesPerBurst");
        good &= lib.load(get_buffer_size, "AAudioStream_getBufferSizeInFrames");
        good &= lib.load(set_buffer_size, "AAudioStream_setBufferSizeInFrames");
        good &= lib.load(get_buffer_capacity, "AAudioStream_getBufferCapacityInFrames");
        good &= lib.load(get_device_id, "AAudioStream_getDeviceId");
        good &= lib.load(result_text, "AAudio_convertResultToText");
        lib.load(set_usage, "AAudioStreamBuilder_setUsage");
        if (!good) why = "the system's libaaudio.so is missing functions fizmo needs";
        return good;
    }

    std::string error_text(std::int32_t code) const {
        const char* s = result_text ? result_text(code) : nullptr;
        std::string out = s ? std::string(s) : "AAudio error " + std::to_string(code);
        if (code == aa::error_disconnected) out += " (the device was removed or the audio route changed)";
        return out;
    }
};

struct Detacher {
    JavaVM* vm = nullptr;
    ~Detacher() {
        if (vm) vm->DetachCurrentThread();
    }
};

thread_local Detacher t_detacher;

JNIEnv* env_for(JavaVM* vm) noexcept {
    if (!vm) return nullptr;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return env;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    t_detacher.vm = vm;
    return env;
}

std::string java_text(JNIEnv* env, jobject value) {
    if (!env || !value) return std::string();
    jclass object_class = env->FindClass("java/lang/Object");
    if (env->ExceptionCheck() || !object_class) {
        env->ExceptionClear();
        return std::string();
    }
    jmethodID to_string = env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    if (env->ExceptionCheck() || !to_string) {
        env->ExceptionClear();
        return std::string();
    }
    jstring s = static_cast<jstring>(env->CallObjectMethod(value, to_string));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return std::string();
    }
    if (!s) return std::string();
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out = chars ? std::string(chars) : std::string();
    if (chars) env->ReleaseStringUTFChars(s, chars);
    env->DeleteLocalRef(s);
    return out;
}

bool take_exception(JNIEnv* env, std::string* text = nullptr) {
    if (!env || !env->ExceptionCheck()) return false;
    jthrowable t = env->ExceptionOccurred();
    env->ExceptionClear();
    if (text) *text = java_text(env, t);
    if (t) env->DeleteLocalRef(t);
    return true;
}

jmethodID method(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    if (!cls) return nullptr;
    jmethodID id = env->GetMethodID(cls, name, sig);
    if (take_exception(env)) return nullptr;
    return id;
}

jmethodID static_method(JNIEnv* env, jclass cls, const char* name, const char* sig) {
    if (!cls) return nullptr;
    jmethodID id = env->GetStaticMethodID(cls, name, sig);
    if (take_exception(env)) return nullptr;
    return id;
}

jclass find_class(JNIEnv* env, const char* name) {
    jclass cls = env->FindClass(name);
    if (take_exception(env)) return nullptr;
    return cls;
}

struct JavaState {
    std::mutex mutex;
    JavaVM* vm = nullptr;
    jobject context = nullptr;
    jobject projection = nullptr;
    bool searched = false;
    jobject activity = nullptr;
    std::map<std::string, std::chrono::steady_clock::time_point> asked;
    std::map<std::string, bool> lost_focus;
};

JavaState& java() {
    static JavaState s;
    return s;
}

JavaVM* find_vm_locked(JavaState& s) {
    if (s.vm) return s.vm;
    if (fizmo_android_app_java_vm) {
        s.vm = static_cast<JavaVM*>(fizmo_android_app_java_vm());
        if (s.vm) return s.vm;
    }
    if (s.searched) return nullptr;
    s.searched = true;
    DynamicLibrary lib;
    if (lib.open({ "libnativehelper.so", "libart.so" })) {
        jint (*created)(JavaVM**, jsize, jsize*) = nullptr;
        if (lib.load(created, "JNI_GetCreatedJavaVMs")) {
            JavaVM* vm = nullptr;
            jsize count = 0;
            if (created(&vm, 1, &count) == JNI_OK && count > 0) s.vm = vm;
        }
    }
    return s.vm;
}

jobject application_of(JNIEnv* env, jobject context) {
    if (!context) return nullptr;
    jclass cls = env->GetObjectClass(context);
    jmethodID get = method(env, cls, "getApplicationContext", "()Landroid/content/Context;");
    jobject app = get ? env->CallObjectMethod(context, get) : nullptr;
    if (take_exception(env)) app = nullptr;
    if (cls) env->DeleteLocalRef(cls);
    jobject global = env->NewGlobalRef(app ? app : context);
    if (app) env->DeleteLocalRef(app);
    return global;
}

jobject context_locked(JavaState& s, JNIEnv* env) {
    if (s.context) return s.context;
    if (fizmo_android_app_activity) {
        jobject activity = static_cast<jobject>(fizmo_android_app_activity());
        if (activity) s.context = application_of(env, activity);
        if (s.context) return s.context;
    }
    jclass thread_class = find_class(env, "android/app/ActivityThread");
    jmethodID current = static_method(env, thread_class, "currentApplication", "()Landroid/app/Application;");
    if (current) {
        jobject app = env->CallStaticObjectMethod(thread_class, current);
        if (take_exception(env)) app = nullptr;
        if (app) {
            s.context = env->NewGlobalRef(app);
            env->DeleteLocalRef(app);
        }
    }
    if (thread_class) env->DeleteLocalRef(thread_class);
    return s.context;
}

constexpr jint k_permission_granted = 0;
constexpr jint k_get_permissions = 4096;
constexpr jint k_permission_request_code = 0x4D49;
constexpr const char* k_record_audio = "android.permission.RECORD_AUDIO";

jobject activity_locked(JavaState& s, JNIEnv* env) {
    if (fizmo_android_app_activity) {
        jobject activity = static_cast<jobject>(fizmo_android_app_activity());
        if (activity) return env->NewLocalRef(activity);
    }
    if (s.activity) {
        jobject activity = env->NewLocalRef(s.activity);
        if (activity) return activity;
    }
    return nullptr;
}

int permission_check_locked(JavaState& s, JNIEnv* env, const char* permission) {
    jobject context = context_locked(s, env);
    if (!context) return -1;
    android::detail::LocalFrame frame(env, 8);
    if (!frame) return -1;
    jclass cls = env->GetObjectClass(context);
    jmethodID check = method(env, cls, "checkSelfPermission", "(Ljava/lang/String;)I");
    if (!check) check = method(env, cls, "checkCallingOrSelfPermission", "(Ljava/lang/String;)I");
    if (!check) return -1;
    jstring name = env->NewStringUTF(permission);
    if (take_exception(env) || !name) return -1;
    const jint result = env->CallIntMethod(context, check, name);
    if (take_exception(env)) return -1;
    return result == k_permission_granted ? 1 : 0;
}

int permission_declared_locked(JavaState& s, JNIEnv* env, const char* permission) {
    jobject context = context_locked(s, env);
    if (!context) return -1;
    android::detail::LocalFrame frame(env, 16);
    if (!frame) return -1;
    jclass cls = env->GetObjectClass(context);
    jmethodID get_manager = method(env, cls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jmethodID get_package = method(env, cls, "getPackageName", "()Ljava/lang/String;");
    if (!get_manager || !get_package) return -1;
    jobject manager = env->CallObjectMethod(context, get_manager);
    if (take_exception(env) || !manager) return -1;
    jobject package = env->CallObjectMethod(context, get_package);
    if (take_exception(env) || !package) return -1;
    jclass manager_class = env->GetObjectClass(manager);
    jmethodID get_info = method(env, manager_class, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    if (!get_info) return -1;
    jobject info = env->CallObjectMethod(manager, get_info, package, k_get_permissions);
    if (take_exception(env) || !info) return -1;
    jclass info_class = env->GetObjectClass(info);
    jfieldID field = env->GetFieldID(info_class, "requestedPermissions", "[Ljava/lang/String;");
    if (take_exception(env) || !field) return -1;
    jobjectArray list = static_cast<jobjectArray>(env->GetObjectField(info, field));
    if (take_exception(env)) return -1;
    if (!list) return 0;
    const jsize n = env->GetArrayLength(list);
    for (jsize i = 0; i < n; ++i) {
        jobject item = env->GetObjectArrayElement(list, i);
        if (take_exception(env)) return -1;
        const bool same = java_text(env, item) == permission;
        if (item) env->DeleteLocalRef(item);
        if (same) return 1;
    }
    return 0;
}

int activity_focused_locked(JavaState& s, JNIEnv* env) {
    jobject activity = activity_locked(s, env);
    if (!activity) return -1;
    jclass cls = env->GetObjectClass(activity);
    jmethodID has_focus = method(env, cls, "hasWindowFocus", "()Z");
    int out = -1;
    if (has_focus) {
        const jboolean focused = env->CallBooleanMethod(activity, has_focus);
        if (!take_exception(env)) out = focused ? 1 : 0;
    }
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return out;
}

AndroidPermissionState permission_state_locked(JavaState& s, JNIEnv* env, const std::string& permission) {
    const int have = permission_check_locked(s, env, permission.c_str());
    const auto asked = s.asked.find(permission);
    if (have == 1 || asked == s.asked.end()) {
        s.asked.erase(permission);
        s.lost_focus.erase(permission);
        return have == 1 ? AndroidPermissionState::granted : AndroidPermissionState::denied;
    }
    const int focused = activity_focused_locked(s, env);
    bool& lost = s.lost_focus[permission];
    if (focused == 0) lost = true;
    const bool waiting_for_prompt = std::chrono::steady_clock::now() - asked->second < std::chrono::seconds(3);
    if (focused == 0 || (!lost && waiting_for_prompt)) return AndroidPermissionState::pending;
    s.asked.erase(permission);
    s.lost_focus.erase(permission);
    return AndroidPermissionState::denied;
}

bool request_permission_locked(JavaState& s, JNIEnv* env, const std::string& permission, std::string& error) {
    const int have = permission_check_locked(s, env, permission.c_str());
    if (have == 1) return true;
    if (s.asked.count(permission) && permission_state_locked(s, env, permission) == AndroidPermissionState::pending) return true;
    if (permission_declared_locked(s, env, permission.c_str()) == 0) {
        error = "the app's AndroidManifest.xml doesn't ask for " + permission + " (add <uses-permission android:name=\"" + permission + "\"/>), so Android can't grant it";
        return false;
    }
    if (api_level() != 0 && api_level() < 23) {
        error = permission + " isn't granted, and Android " + std::to_string(api_level()) + " only grants permissions when the app is installed";
        return false;
    }
    jobject activity = activity_locked(s, env);
    if (!activity) {
        error = "asking for " + permission + " needs the app's Activity; use fizmo's Android app layer or pass the activity to fizmo::audio::set_android_java_vm()";
        return false;
    }
    android::detail::LocalFrame frame(env, 8);
    if (!frame) {
        env->DeleteLocalRef(activity);
        error = "out of Java memory";
        return false;
    }
    jclass cls = env->GetObjectClass(activity);
    jmethodID request = method(env, cls, "requestPermissions", "([Ljava/lang/String;I)V");
    jclass string_class = find_class(env, "java/lang/String");
    if (!request || !string_class) {
        env->DeleteLocalRef(activity);
        error = "this Android version can't ask for permissions while the app runs (that needs Android 6.0, API 23)";
        return false;
    }
    jstring name = env->NewStringUTF(permission.c_str());
    jobjectArray list = name ? env->NewObjectArray(1, string_class, name) : nullptr;
    if (take_exception(env) || !list) {
        env->DeleteLocalRef(activity);
        error = "out of Java memory";
        return false;
    }
    env->CallVoidMethod(activity, request, list, k_permission_request_code);
    env->DeleteLocalRef(activity);
    std::string why;
    if (take_exception(env, &why)) {
        error = "Android wouldn't ask for " + permission + (why.empty() ? std::string() : ": " + why);
        return false;
    }
    s.asked[permission] = std::chrono::steady_clock::now();
    s.lost_focus[permission] = false;
    return true;
}

bool recording_allowed(std::string& error) {
    JavaState& s = java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JNIEnv* env = env_for(find_vm_locked(s));
    if (!env) return true;
    if (permission_check_locked(s, env, k_record_audio) != 0) return true;
    std::string why;
    if (!request_permission_locked(s, env, k_record_audio, why)) {
        error = "recording needs the RECORD_AUDIO permission, and fizmo couldn't ask the user for it: " + why;
        return false;
    }
    error = "recording needs the RECORD_AUDIO permission: the permission prompt was shown, so open the microphone again once the user allows it (fizmo::audio::android_permission_state() says when)";
    return false;
}

const char* device_type_name(int type) {
    switch (type) {
        case 1: return "earpiece";
        case 2: return "speaker";
        case 3: return "wired headset";
        case 4: return "wired headphones";
        case 5: return "analog line";
        case 6: return "digital line";
        case 7: return "Bluetooth headset";
        case 8: return "Bluetooth";
        case 9: return "HDMI";
        case 10: return "HDMI ARC";
        case 11: return "USB device";
        case 12: return "USB accessory";
        case 13: return "dock";
        case 14: return "FM";
        case 15: return "built-in microphone";
        case 16: return "FM tuner";
        case 17: return "TV tuner";
        case 19: return "aux line";
        case 20: return "network";
        case 21: return "bus";
        case 22: return "USB headset";
        case 23: return "hearing aid";
        case 26: return "Bluetooth LE headset";
        case 27: return "Bluetooth LE speaker";
        case 29: return "HDMI eARC";
        case 30: return "Bluetooth LE broadcast";
        default: return "audio device";
    }
}

std::uint32_t largest(JNIEnv* env, jintArray values) {
    if (!values) return 0;
    const jsize n = env->GetArrayLength(values);
    std::uint32_t best = 0;
    if (n > 0) {
        std::vector<jint> v(static_cast<std::size_t>(n));
        env->GetIntArrayRegion(values, 0, n, v.data());
        if (take_exception(env)) return 0;
        for (jint x : v) best = std::max<std::uint32_t>(best, x > 0 ? static_cast<std::uint32_t>(x) : 0u);
    }
    return best;
}

struct JavaDevices {
    bool ok = false;
    std::uint32_t output_rate = 0;
    std::vector<DeviceInfo> list;
};

JavaDevices java_devices() {
    JavaDevices out;
    JavaState& s = java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JavaVM* vm = find_vm_locked(s);
    JNIEnv* env = env_for(vm);
    if (!env) return out;
    jobject context = context_locked(s, env);
    if (!context) return out;
    android::detail::LocalFrame frame(env, 32);
    if (!frame) return out;
    jclass context_class = find_class(env, "android/content/Context");
    jmethodID get_service = method(env, context_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!get_service) return out;
    jstring audio_key = env->NewStringUTF("audio");
    jobject manager = env->CallObjectMethod(context, get_service, audio_key);
    if (take_exception(env) || !manager) return out;
    jclass manager_class = find_class(env, "android/media/AudioManager");
    jmethodID get_devices = method(env, manager_class, "getDevices", "(I)[Landroid/media/AudioDeviceInfo;");
    jmethodID get_property = method(env, manager_class, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
    jclass info_class = find_class(env, "android/media/AudioDeviceInfo");
    jmethodID get_id = method(env, info_class, "getId", "()I");
    jmethodID get_name = method(env, info_class, "getProductName", "()Ljava/lang/CharSequence;");
    jmethodID get_type = method(env, info_class, "getType", "()I");
    jmethodID get_rates = method(env, info_class, "getSampleRates", "()[I");
    jmethodID get_counts = method(env, info_class, "getChannelCounts", "()[I");
    if (!get_devices || !get_id || !get_name || !get_type || !get_rates || !get_counts) return out;
    if (get_property) {
        jstring key = env->NewStringUTF("android.media.property.OUTPUT_SAMPLE_RATE");
        jobject value = env->CallObjectMethod(manager, get_property, key);
        if (!take_exception(env) && value) out.output_rate = static_cast<std::uint32_t>(std::max(0, std::atoi(java_text(env, value).c_str())));
    }
    for (int pass = 0; pass < 2; ++pass) {
        const bool outputs = pass == 0;
        jobjectArray array = static_cast<jobjectArray>(env->CallObjectMethod(manager, get_devices, static_cast<jint>(outputs ? 2 : 1)));
        if (take_exception(env) || !array) continue;
        const jsize n = env->GetArrayLength(array);
        for (jsize i = 0; i < n; ++i) {
            android::detail::LocalFrame item(env, 16);
            if (!item) break;
            jobject dev = env->GetObjectArrayElement(array, i);
            if (take_exception(env) || !dev) continue;
            const jint type = env->CallIntMethod(dev, get_type);
            if (take_exception(env)) continue;
            if (type == 18 || type == 25 || type == 24) continue;
            const jint id = env->CallIntMethod(dev, get_id);
            if (take_exception(env)) continue;
            jobject product = env->CallObjectMethod(dev, get_name);
            if (take_exception(env)) product = nullptr;
            jintArray rates = static_cast<jintArray>(env->CallObjectMethod(dev, get_rates));
            if (take_exception(env)) rates = nullptr;
            jintArray counts = static_cast<jintArray>(env->CallObjectMethod(dev, get_counts));
            if (take_exception(env)) counts = nullptr;
            DeviceInfo d;
            d.id = std::to_string(static_cast<int>(id));
            const std::string product_name = java_text(env, product);
            const std::string kind_name = device_type_name(static_cast<int>(type));
            d.name = product_name.empty() ? kind_name : product_name + " (" + kind_name + ")";
            d.kind = outputs ? DeviceKind::output : DeviceKind::input;
            d.sample_rate = largest(env, rates);
            d.channels = largest(env, counts);
            if (d.channels == 0) d.channels = outputs ? 2 : 1;
            d.backend = "aaudio";
            out.list.push_back(d);
        }
    }
    out.ok = true;
    return out;
}

std::mutex& live_mutex() {
    static std::mutex m;
    return m;
}

std::vector<void*>& live_streams() {
    static std::vector<void*> v;
    return v;
}

void add_live(void* p) {
    std::lock_guard<std::mutex> lock(live_mutex());
    live_streams().push_back(p);
}

void remove_live(void* p) {
    std::lock_guard<std::mutex> lock(live_mutex());
    auto& v = live_streams();
    v.erase(std::remove(v.begin(), v.end(), p), v.end());
}

bool sample_format_of(std::int32_t format, SampleFormat& out) {
    switch (format) {
        case aa::format_float: out = SampleFormat::f32; return true;
        case aa::format_i16: out = SampleFormat::s16; return true;
        case aa::format_i24_packed: out = SampleFormat::s24; return true;
        case aa::format_i32: out = SampleFormat::s32; return true;
        default: return false;
    }
}

class AAudioStream : public BackendStream {
public:
    AAudioStream(std::shared_ptr<const AAudioApi> api, const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::int32_t device_id, std::string shown)
        : m_api(std::move(api)), m_request(request), m_shared(std::move(shared)), m_device_id(device_id), m_shown(std::move(shown)) {
        m_input = request.kind == DeviceKind::input;
        m_force_convert = std::getenv("FIZMO_AUDIO_AAUDIO_CONVERT") != nullptr;
    }

    ~AAudioStream() override {
        remove_live(this);
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_quit = true;
        }
        m_event_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
        std::lock_guard<std::mutex> lock(m_control);
        teardown();
    }

    bool open(std::string& error) {
        {
            std::lock_guard<std::mutex> lock(m_control);
            if (!setup(error)) {
                teardown();
                return false;
            }
        }
        add_live(this);
        m_worker = std::thread([this] { worker(); });
        return true;
    }

    bool start(std::string& error) override {
        std::lock_guard<std::mutex> lock(m_control);
        if (m_running) return true;
        if (!m_stream && !setup(error)) {
            teardown();
            return false;
        }
        if (!begin(error)) return false;
        m_running = true;
        return true;
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(m_control);
        if (!m_running) return;
        end();
        m_running = false;
    }

    std::uint32_t latency_frames() const override { return m_latency.load(); }

    std::string device_name() const override {
        std::lock_guard<std::mutex> lock(m_name_mutex);
        return m_shown;
    }

private:
    bool follows_default() const noexcept { return m_device_id == aa::unspecified; }

    aa::stream* open_stream(std::int32_t rate, std::int32_t channels, std::int32_t format, std::int32_t& result) {
        aa::builder* b = nullptr;
        result = m_api->create_builder(&b);
        if (result != aa::ok || !b) return nullptr;
        m_api->set_direction(b, m_input ? aa::direction_input : aa::direction_output);
        m_api->set_device_id(b, m_device_id);
        m_api->set_sharing_mode(b, aa::sharing_shared);
        m_api->set_performance_mode(b, aa::performance_low_latency);
        m_api->set_format(b, format);
        m_api->set_sample_rate(b, rate);
        m_api->set_channel_count(b, channels);
        if (!m_input && m_api->set_usage && api_level() >= 28) m_api->set_usage(b, aa::usage_media);
        m_api->set_data_callback(b, on_data, this);
        m_api->set_error_callback(b, on_error, this);
        aa::stream* s = nullptr;
        result = m_api->open_stream(b, &s);
        m_api->delete_builder(b);
        if (result != aa::ok) return nullptr;
        return s;
    }

    bool setup(std::string& error) {
        teardown();
        const std::uint32_t want_rate = m_shared->sample_rate ? m_shared->sample_rate : m_request.sample_rate;
        const std::uint32_t want_channels = m_shared->channels ? m_shared->channels : m_request.channels;
        std::int32_t result = aa::ok;
        aa::stream* s = nullptr;
        if (!m_force_convert) s = open_stream(static_cast<std::int32_t>(want_rate), static_cast<std::int32_t>(want_channels), aa::format_float, result);
        if (!s) s = open_stream(aa::unspecified, aa::unspecified, aa::format_float, result);
        if (!s) s = open_stream(aa::unspecified, aa::unspecified, aa::format_unspecified, result);
        if (!s) {
            error = "can't open " + (follows_default() ? std::string("the default device") : "device '" + m_request.device + "'") + ": " + m_api->error_text(result);
            if (m_input) error += " (recording needs the RECORD_AUDIO permission)";
            return false;
        }
        m_stream = s;
        if (m_request.pin_device && !follows_default() && m_api->get_device_id(s) != m_device_id) {
            error = "device '" + m_request.device + "' isn't available (Android opened another device instead)";
            return false;
        }
        const std::int32_t format = m_api->get_format(s);
        if (!sample_format_of(format, m_device_format)) {
            error = "the audio device uses a sample format fizmo doesn't know";
            return false;
        }
        m_device_rate = static_cast<std::uint32_t>(std::max(1, m_api->get_sample_rate(s)));
        m_device_channels = static_cast<std::uint32_t>(std::max(1, m_api->get_channel_count(s)));
        const std::uint32_t rate = want_rate ? want_rate : m_device_rate;
        const std::uint32_t channels = want_channels ? want_channels : m_device_channels;
        if (channels > 64) {
            error = "the audio device opened with more channels than fizmo supports";
            return false;
        }
        m_convert = m_device_format != SampleFormat::f32 || rate != m_device_rate || channels != m_device_channels;
        m_shared->sample_rate = rate;
        m_shared->channels = channels;
        const std::int32_t burst = std::max(1, m_api->get_frames_per_burst(s));
        const std::int32_t capacity = std::max(burst, m_api->get_buffer_capacity(s));
        if (!m_input) {
            const std::int32_t target = std::max<std::int32_t>(burst * 2, static_cast<std::int32_t>(static_cast<std::uint64_t>(m_device_rate) * m_request.latency_ms / 1000));
            m_api->set_buffer_size(s, std::min(target, capacity));
        }
        const std::int32_t buffer = std::max(burst, m_api->get_buffer_size(s));
        m_latency.store(static_cast<std::uint32_t>(static_cast<std::uint64_t>(buffer) * rate / m_device_rate));
        m_max_frames = static_cast<std::size_t>(std::max<std::int32_t>(capacity, 1024));
        if (m_convert) {
            const std::size_t widest = std::max<std::size_t>(channels, m_device_channels);
            if (m_input) m_resampler.configure(channels, m_device_rate, rate, ResampleQuality::medium);
            else m_resampler.configure(m_device_channels, rate, m_device_rate, ResampleQuality::medium);
            const double ratio = static_cast<double>(std::max(rate, m_device_rate)) / static_cast<double>(std::max<std::uint32_t>(1, std::min(rate, m_device_rate)));
            m_raw.assign(m_max_frames * widest, 0.0f);
            m_user.assign(k_chunk * widest, 0.0f);
            m_fifo.clear();
            m_fifo.reserve(static_cast<std::size_t>(static_cast<double>(m_max_frames + 2 * k_chunk) * ratio + 1024.0) * widest * 2);
        }
        if (follows_default()) {
            std::lock_guard<std::mutex> lock(m_name_mutex);
            m_shown = "default";
        }
        return true;
    }

    void teardown() {
        if (!m_stream) return;
        m_api->request_stop(m_stream);
        m_api->close(m_stream);
        m_stream = nullptr;
    }

    bool begin(std::string& error) {
        const std::int32_t r = m_api->request_start(m_stream);
        if (r != aa::ok) {
            error = "can't start the audio device: " + m_api->error_text(r);
            return false;
        }
        return true;
    }

    void end() {
        if (!m_stream) return;
        if (m_api->request_stop(m_stream) != aa::ok) return;
        std::int32_t state = m_api->get_state(m_stream);
        for (int i = 0; i < 20; ++i) {
            if (state == aa::state_stopped || state == aa::state_closing || state == aa::state_closed || state == aa::state_disconnected) break;
            std::int32_t next = state;
            if (m_api->wait_for_state_change(m_stream, state, &next, 100000000LL) != aa::ok) break;
            state = next;
        }
    }

    static std::int32_t on_data(aa::stream*, void* user, void* data, std::int32_t frames) {
        AAudioStream* self = static_cast<AAudioStream*>(user);
        if (frames > 0 && data) {
            if (self->m_input) self->take_input(data, static_cast<std::size_t>(frames));
            else self->fill_output(data, static_cast<std::size_t>(frames));
        }
        return aa::callback_continue;
    }

    static void on_error(aa::stream*, void* user, std::int32_t error) {
        std::lock_guard<std::mutex> lock(live_mutex());
        auto& v = live_streams();
        if (std::find(v.begin(), v.end(), user) == v.end()) return;
        AAudioStream* self = static_cast<AAudioStream*>(user);
        {
            std::lock_guard<std::mutex> events(self->m_event_mutex);
            self->m_error = error;
            self->m_pending = true;
        }
        self->m_event_cv.notify_all();
    }

    void fill_output(void* data, std::size_t frames) noexcept {
        if (!m_convert) {
            m_shared->render(static_cast<float*>(data), frames);
            return;
        }
        const std::size_t dc = m_device_channels;
        const std::size_t uc = m_shared->channels;
        while (m_fifo.size() < frames * dc) {
            m_shared->render(m_user.data(), k_chunk);
            remix(m_user.data(), uc, m_user.data(), dc, k_chunk);
            m_resampler.process(m_user.data(), k_chunk, m_fifo);
        }
        if (m_device_format == SampleFormat::f32) std::memcpy(data, m_fifo.data(), frames * dc * sizeof(float));
        else from_float(m_fifo.data(), data, m_device_format, frames * dc, true);
        m_fifo.erase(m_fifo.begin(), m_fifo.begin() + static_cast<std::ptrdiff_t>(frames * dc));
    }

    void take_input(const void* data, std::size_t frames) noexcept {
        if (!m_convert) {
            m_shared->deliver(static_cast<const float*>(data), frames);
            return;
        }
        const std::size_t dc = m_device_channels;
        const std::size_t uc = m_shared->channels;
        const std::size_t frame_bytes = dc * bytes_per_sample(m_device_format);
        for (std::size_t done = 0; done < frames;) {
            const std::size_t n = std::min(m_max_frames, frames - done);
            to_float(static_cast<const std::uint8_t*>(data) + done * frame_bytes, m_device_format, m_raw.data(), n * dc);
            remix(m_raw.data(), dc, m_raw.data(), uc, n);
            m_fifo.clear();
            const std::size_t got = m_resampler.process(m_raw.data(), n, m_fifo);
            if (got) m_shared->deliver(m_fifo.data(), got);
            done += n;
        }
    }

    void worker() {
        std::unique_lock<std::mutex> lock(m_event_mutex);
        while (!m_quit) {
            m_event_cv.wait(lock, [this] { return m_quit || m_pending; });
            if (m_quit) break;
            const std::int32_t error = m_error;
            m_pending = false;
            lock.unlock();
            handle(error);
            lock.lock();
        }
    }

    void handle(std::int32_t error) {
        std::lock_guard<std::mutex> lock(m_control);
        if (m_shared->failed.load()) return;
        if (error != aa::error_disconnected) {
            teardown();
            m_shared->fail("the audio device stopped: " + m_api->error_text(error));
            return;
        }
        if (!follows_default()) {
            teardown();
            m_shared->fail(m_request.pin_device ? "the device '" + m_shown + "' was removed or disabled" : std::string("the audio device was removed or disabled"));
            return;
        }
        std::string why;
        if (!setup(why) || (m_running && !begin(why))) {
            teardown();
            m_shared->fail("lost the audio device and couldn't switch to the new default: " + why);
        }
    }

    static constexpr std::size_t k_chunk = 256;

    std::shared_ptr<const AAudioApi> m_api;
    StreamRequest m_request;
    std::shared_ptr<StreamShared> m_shared;
    std::int32_t m_device_id = aa::unspecified;
    bool m_input = false;
    bool m_force_convert = false;
    bool m_running = false;
    aa::stream* m_stream = nullptr;
    bool m_convert = false;
    SampleFormat m_device_format = SampleFormat::f32;
    std::uint32_t m_device_rate = 0;
    std::uint32_t m_device_channels = 0;
    std::size_t m_max_frames = 1024;
    Resampler m_resampler;
    std::vector<float> m_raw;
    std::vector<float> m_user;
    std::vector<float> m_fifo;
    std::atomic<std::uint32_t> m_latency{ 0 };
    std::mutex m_control;
    mutable std::mutex m_name_mutex;
    std::string m_shown;
    std::mutex m_event_mutex;
    std::condition_variable m_event_cv;
    std::int32_t m_error = 0;
    bool m_pending = false;
    bool m_quit = false;
    std::thread m_worker;
};

constexpr jint k_encoding_float = 4;
constexpr jint k_channel_in_mono = 16;
constexpr jint k_channel_in_stereo = 12;
constexpr jint k_state_initialized = 1;
constexpr jint k_recording = 3;
constexpr jint k_read_blocking = 0;
constexpr jint k_usage_unknown = 0;
constexpr jint k_usage_media = 1;
constexpr jint k_usage_game = 14;

class PlaybackCaptureStream : public BackendStream {
public:
    PlaybackCaptureStream(const StreamRequest& request, std::shared_ptr<StreamShared> shared)
        : m_request(request), m_shared(std::move(shared)) {}

    ~PlaybackCaptureStream() override {
        stop();
        JNIEnv* env = env_for(m_vm);
        if (!env) return;
        if (m_record) {
            if (m_release) env->CallVoidMethod(m_record, m_release);
            take_exception(env);
            env->DeleteGlobalRef(m_record);
        }
        if (m_array) env->DeleteGlobalRef(m_array);
    }

    bool open(std::string& error) {
        if (api_level() < 29) {
            error = "recording system audio needs Android 10 (API 29) or newer";
            return false;
        }
        jobject projection = nullptr;
        JNIEnv* env = nullptr;
        {
            JavaState& s = java();
            std::lock_guard<std::mutex> lock(s.mutex);
            m_vm = find_vm_locked(s);
            env = env_for(m_vm);
            if (!env) {
                error = "recording system audio on Android needs the app's Java VM; call fizmo::audio::set_android_java_vm() first";
                return false;
            }
            if (!s.projection) {
                error = "recording system audio on Android needs a MediaProjection: ask the user with MediaProjectionManager.createScreenCaptureIntent(), start a foreground service of type mediaProjection, then pass the MediaProjection to fizmo::audio::set_android_media_projection()";
                return false;
            }
            projection = env->NewLocalRef(s.projection);
        }
        android::detail::LocalFrame frame(env, 64);
        if (!frame || !projection) {
            error = "out of Java memory";
            return false;
        }
        const std::uint32_t want_rate = m_request.sample_rate ? m_request.sample_rate : 48000;
        const std::uint32_t want_channels = m_request.channels ? m_request.channels : 2;
        m_capture_channels = want_channels == 1 ? 1 : 2;
        std::string why;
        jobject record = build(env, projection, want_rate, why);
        if (!record && want_rate != 48000) {
            std::string again;
            record = build(env, projection, 48000, again);
        }
        if (!record) {
            error = "Android refused the system-audio recording: " + why;
            return false;
        }
        m_record = env->NewGlobalRef(record);
        jclass record_class = env->GetObjectClass(record);
        m_start = method(env, record_class, "startRecording", "()V");
        m_stop = method(env, record_class, "stop", "()V");
        m_release = method(env, record_class, "release", "()V");
        m_read = method(env, record_class, "read", "([FIII)I");
        m_recording_state = method(env, record_class, "getRecordingState", "()I");
        jmethodID get_rate = method(env, record_class, "getSampleRate", "()I");
        if (!m_start || !m_stop || !m_release || !m_read || !get_rate) {
            error = "this Android version's AudioRecord is missing methods fizmo needs";
            return false;
        }
        const jint actual = env->CallIntMethod(record, get_rate);
        if (take_exception(env) || actual <= 0) {
            error = "can't read the system-audio recording's sample rate";
            return false;
        }
        m_device_rate = static_cast<std::uint32_t>(actual);
        m_period = std::max<std::size_t>(64, static_cast<std::size_t>(m_device_rate) * std::max<std::uint32_t>(m_request.latency_ms, 4) / 2000);
        jfloatArray array = env->NewFloatArray(static_cast<jsize>(m_period * m_capture_channels));
        if (take_exception(env) || !array) {
            error = "out of Java memory";
            return false;
        }
        m_array = static_cast<jfloatArray>(env->NewGlobalRef(array));
        m_shared->sample_rate = want_rate;
        m_shared->channels = want_channels;
        const std::size_t widest = std::max<std::size_t>(want_channels, m_capture_channels);
        m_raw.assign(m_period * widest, 0.0f);
        m_resampler.configure(want_channels, m_device_rate, want_rate, ResampleQuality::medium);
        m_fifo.reserve(static_cast<std::size_t>(static_cast<double>(m_period) * (static_cast<double>(want_rate) / static_cast<double>(m_device_rate)) + 1024.0) * want_channels * 2);
        return true;
    }

    bool start(std::string& error) override {
        if (m_thread.joinable()) return true;
        JNIEnv* env = env_for(m_vm);
        if (!env || !m_record) {
            error = "the system-audio recording isn't open";
            return false;
        }
        env->CallVoidMethod(m_record, m_start);
        std::string why;
        if (take_exception(env, &why)) {
            error = "can't start recording system audio: " + why;
            return false;
        }
        if (m_recording_state) {
            const jint state = env->CallIntMethod(m_record, m_recording_state);
            if (!take_exception(env) && state != k_recording) {
                error = "can't start recording system audio (another app may be using the recorder)";
                return false;
            }
        }
        m_run.store(true);
        m_thread = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        if (!m_thread.joinable()) return;
        m_run.store(false);
        JNIEnv* env = env_for(m_vm);
        if (env && m_record) {
            env->CallVoidMethod(m_record, m_stop);
            take_exception(env);
        }
        m_thread.join();
    }

    std::uint32_t latency_frames() const override {
        return static_cast<std::uint32_t>(m_period * 2 * m_shared->sample_rate / std::max<std::uint32_t>(1, m_device_rate));
    }

    std::string device_name() const override { return "System audio"; }

private:
    jobject build(JNIEnv* env, jobject projection, std::uint32_t rate, std::string& why) {
        jclass config_builder_class = find_class(env, "android/media/AudioPlaybackCaptureConfiguration$Builder");
        jclass format_builder_class = find_class(env, "android/media/AudioFormat$Builder");
        jclass record_builder_class = find_class(env, "android/media/AudioRecord$Builder");
        jclass record_class = find_class(env, "android/media/AudioRecord");
        if (!config_builder_class || !format_builder_class || !record_builder_class || !record_class) {
            why = "AudioPlaybackCaptureConfiguration isn't available";
            return nullptr;
        }
        jmethodID config_new = method(env, config_builder_class, "<init>", "(Landroid/media/projection/MediaProjection;)V");
        jmethodID config_usage = method(env, config_builder_class, "addMatchingUsage", "(I)Landroid/media/AudioPlaybackCaptureConfiguration$Builder;");
        jmethodID config_build = method(env, config_builder_class, "build", "()Landroid/media/AudioPlaybackCaptureConfiguration;");
        jmethodID format_new = method(env, format_builder_class, "<init>", "()V");
        jmethodID format_encoding = method(env, format_builder_class, "setEncoding", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID format_rate = method(env, format_builder_class, "setSampleRate", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID format_mask = method(env, format_builder_class, "setChannelMask", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID format_build = method(env, format_builder_class, "build", "()Landroid/media/AudioFormat;");
        jmethodID record_new = method(env, record_builder_class, "<init>", "()V");
        jmethodID record_format = method(env, record_builder_class, "setAudioFormat", "(Landroid/media/AudioFormat;)Landroid/media/AudioRecord$Builder;");
        jmethodID record_size = method(env, record_builder_class, "setBufferSizeInBytes", "(I)Landroid/media/AudioRecord$Builder;");
        jmethodID record_capture = method(env, record_builder_class, "setAudioPlaybackCaptureConfig", "(Landroid/media/AudioPlaybackCaptureConfiguration;)Landroid/media/AudioRecord$Builder;");
        jmethodID record_build = method(env, record_builder_class, "build", "()Landroid/media/AudioRecord;");
        jmethodID min_size = static_method(env, record_class, "getMinBufferSize", "(III)I");
        jmethodID get_state = method(env, record_class, "getState", "()I");
        jmethodID release = method(env, record_class, "release", "()V");
        if (!config_new || !config_usage || !config_build || !format_new || !format_encoding || !format_rate || !format_mask || !format_build || !record_new || !record_format || !record_size || !record_capture || !record_build || !min_size || !get_state) {
            why = "this Android version is missing the playback-capture API";
            return nullptr;
        }
        jobject config_builder = env->NewObject(config_builder_class, config_new, projection);
        if (take_exception(env, &why) || !config_builder) return nullptr;
        for (jint usage : { k_usage_media, k_usage_game, k_usage_unknown }) {
            jobject same = env->CallObjectMethod(config_builder, config_usage, usage);
            if (take_exception(env, &why)) return nullptr;
            if (same) env->DeleteLocalRef(same);
        }
        jobject config = env->CallObjectMethod(config_builder, config_build);
        if (take_exception(env, &why) || !config) return nullptr;
        const jint mask = m_capture_channels == 1 ? k_channel_in_mono : k_channel_in_stereo;
        jobject format_builder = env->NewObject(format_builder_class, format_new);
        if (take_exception(env, &why) || !format_builder) return nullptr;
        jobject r1 = env->CallObjectMethod(format_builder, format_encoding, k_encoding_float);
        if (take_exception(env, &why)) return nullptr;
        jobject r2 = env->CallObjectMethod(format_builder, format_rate, static_cast<jint>(rate));
        if (take_exception(env, &why)) return nullptr;
        jobject r3 = env->CallObjectMethod(format_builder, format_mask, mask);
        if (take_exception(env, &why)) return nullptr;
        if (r1) env->DeleteLocalRef(r1);
        if (r2) env->DeleteLocalRef(r2);
        if (r3) env->DeleteLocalRef(r3);
        jobject format = env->CallObjectMethod(format_builder, format_build);
        if (take_exception(env, &why) || !format) return nullptr;
        jint bytes = env->CallStaticIntMethod(record_class, min_size, static_cast<jint>(rate), mask, k_encoding_float);
        if (take_exception(env, &why)) return nullptr;
        const jint wanted = static_cast<jint>(static_cast<std::uint64_t>(rate) * m_capture_channels * 4 * std::max<std::uint32_t>(m_request.latency_ms, 20) * 2 / 1000);
        bytes = std::max(bytes, wanted);
        jobject record_builder = env->NewObject(record_builder_class, record_new);
        if (take_exception(env, &why) || !record_builder) return nullptr;
        jobject b1 = env->CallObjectMethod(record_builder, record_format, format);
        if (take_exception(env, &why)) return nullptr;
        jobject b2 = env->CallObjectMethod(record_builder, record_size, bytes);
        if (take_exception(env, &why)) return nullptr;
        jobject b3 = env->CallObjectMethod(record_builder, record_capture, config);
        if (take_exception(env, &why)) return nullptr;
        if (b1) env->DeleteLocalRef(b1);
        if (b2) env->DeleteLocalRef(b2);
        if (b3) env->DeleteLocalRef(b3);
        jobject record = env->CallObjectMethod(record_builder, record_build);
        if (take_exception(env, &why)) {
            if (why.find("SecurityException") != std::string::npos) why += " (the app needs the RECORD_AUDIO permission)";
            return nullptr;
        }
        if (!record) {
            why = "AudioRecord.Builder.build() returned nothing";
            return nullptr;
        }
        const jint state = env->CallIntMethod(record, get_state);
        if (take_exception(env, &why) || state != k_state_initialized) {
            if (release) env->CallVoidMethod(record, release);
            take_exception(env);
            if (why.empty()) why = "the recorder didn't initialize";
            return nullptr;
        }
        return record;
    }

    void loop() {
        JNIEnv* env = env_for(m_vm);
        if (!env) {
            m_shared->fail("can't attach the system-audio thread to Java");
            return;
        }
        const std::size_t cc = m_capture_channels;
        const std::size_t uc = m_shared->channels;
        const jint length = static_cast<jint>(m_period * cc);
        std::size_t carry = 0;
        while (m_run.load(std::memory_order_relaxed)) {
            const jint n = env->CallIntMethod(m_record, m_read, m_array, static_cast<jint>(carry), length - static_cast<jint>(carry), k_read_blocking);
            std::string why;
            if (take_exception(env, &why)) {
                if (m_run.load()) m_shared->fail("the system-audio recording stopped: " + why);
                return;
            }
            if (n < 0) {
                if (m_run.load()) m_shared->fail("the system-audio recording stopped (AudioRecord error " + std::to_string(static_cast<int>(n)) + ")");
                return;
            }
            const std::size_t have = carry + static_cast<std::size_t>(n);
            const std::size_t frames = have / cc;
            if (frames == 0) {
                carry = have;
                continue;
            }
            env->GetFloatArrayRegion(m_array, 0, static_cast<jsize>(have), m_raw.data());
            if (take_exception(env)) return;
            remix(m_raw.data(), cc, m_raw.data(), uc, frames);
            m_fifo.clear();
            const std::size_t got = m_resampler.process(m_raw.data(), frames, m_fifo);
            if (got) m_shared->deliver(m_fifo.data(), got);
            carry = have - frames * cc;
            if (carry) {
                float tail[2];
                env->GetFloatArrayRegion(m_array, static_cast<jsize>(frames * cc), static_cast<jsize>(carry), tail);
                env->SetFloatArrayRegion(m_array, 0, static_cast<jsize>(carry), tail);
                if (take_exception(env)) return;
            }
        }
    }

    StreamRequest m_request;
    std::shared_ptr<StreamShared> m_shared;
    JavaVM* m_vm = nullptr;
    jobject m_record = nullptr;
    jfloatArray m_array = nullptr;
    jmethodID m_start = nullptr;
    jmethodID m_stop = nullptr;
    jmethodID m_release = nullptr;
    jmethodID m_read = nullptr;
    jmethodID m_recording_state = nullptr;
    std::size_t m_capture_channels = 2;
    std::uint32_t m_device_rate = 48000;
    std::size_t m_period = 480;
    Resampler m_resampler;
    std::vector<float> m_raw;
    std::vector<float> m_fifo;
    std::atomic<bool> m_run{ false };
    std::thread m_thread;
};

class AAudioBackend : public Backend {
public:
    bool init(std::string& why) {
        m_api = std::make_shared<AAudioApi>();
        return m_api->load(why);
    }

    const char* name() const override { return "aaudio"; }

    std::vector<DeviceInfo> devices() override {
        const JavaDevices found = java_devices();
        std::vector<DeviceInfo> out;
        DeviceInfo d;
        d.id = "default";
        d.backend = "aaudio";
        d.is_default = true;
        d.kind = DeviceKind::output;
        d.name = "Default output";
        d.sample_rate = found.output_rate ? found.output_rate : 48000;
        d.channels = 2;
        out.push_back(d);
        for (const DeviceInfo& x : found.list) {
            if (x.kind == DeviceKind::output) out.push_back(x);
        }
        d.kind = DeviceKind::input;
        d.name = "Default microphone";
        d.channels = 1;
        out.push_back(d);
        for (const DeviceInfo& x : found.list) {
            if (x.kind == DeviceKind::input) out.push_back(x);
        }
        if (api_level() >= 29) {
            d.kind = DeviceKind::loopback;
            d.name = "System audio";
            d.channels = 2;
            out.push_back(d);
        }
        return out;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) override {
        if (request.kind != DeviceKind::output && !recording_allowed(error)) return nullptr;
        if (request.kind == DeviceKind::loopback) {
            if (!request.device.empty() && request.device != "default") {
                error = "can't find device '" + request.device + "' (Android records system audio only from the default mix)";
                return nullptr;
            }
            auto stream = std::make_unique<PlaybackCaptureStream>(request, std::move(shared));
            if (!stream->open(error)) return nullptr;
            return stream;
        }
        std::int32_t id = aa::unspecified;
        std::string shown = "default";
        if (!request.device.empty() && request.device != "default") {
            char* end = nullptr;
            const long v = std::strtol(request.device.c_str(), &end, 10);
            if (!end || *end != '\0' || v <= 0 || v > 0x7FFFFFFFL) {
                error = "can't find device '" + request.device + "' (Android device ids are the numbers devices() lists)";
                return nullptr;
            }
            id = static_cast<std::int32_t>(v);
            shown = request.device;
            const JavaDevices found = java_devices();
            if (found.ok) {
                bool listed = false;
                const DeviceKind want = request.kind == DeviceKind::input ? DeviceKind::input : DeviceKind::output;
                for (const DeviceInfo& d : found.list) {
                    if (d.id == request.device && d.kind == want) {
                        listed = true;
                        shown = d.name;
                        break;
                    }
                }
                if (!listed) {
                    error = "can't find device '" + request.device + "'";
                    return nullptr;
                }
            }
        }
        auto stream = std::make_unique<AAudioStream>(m_api, request, std::move(shared), id, shown);
        if (!stream->open(error)) return nullptr;
        return stream;
    }

private:
    std::shared_ptr<AAudioApi> m_api;
};

} // namespace

std::unique_ptr<Backend> make_aaudio_backend(std::string& why) {
    auto b = std::make_unique<AAudioBackend>();
    if (!b->init(why)) return nullptr;
    return b;
}

} // namespace detail

bool set_android_java_vm(void* java_vm, void* context, std::string* error) {
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JavaVM* vm = static_cast<JavaVM*>(java_vm);
    if (!vm) vm = detail::find_vm_locked(s);
    if (!vm) {
        if (error) *error = "no Java VM was given";
        return false;
    }
    JNIEnv* env = detail::env_for(vm);
    if (!env) {
        if (error) *error = "can't attach this thread to the Java VM";
        return false;
    }
    if (s.vm && s.vm != vm) {
        s.context = nullptr;
        s.projection = nullptr;
        s.activity = nullptr;
        s.asked.clear();
        s.lost_focus.clear();
    }
    s.vm = vm;
    if (context) {
        jclass activity_class = detail::find_class(env, "android/app/Activity");
        if (activity_class && env->IsInstanceOf(static_cast<jobject>(context), activity_class)) {
            if (s.activity) env->DeleteWeakGlobalRef(s.activity);
            s.activity = env->NewWeakGlobalRef(static_cast<jobject>(context));
        }
        if (activity_class) env->DeleteLocalRef(activity_class);
        jobject app = detail::application_of(env, static_cast<jobject>(context));
        if (!app) {
            if (error) *error = "the context isn't a valid Java object";
            return false;
        }
        if (s.context) env->DeleteGlobalRef(s.context);
        s.context = app;
    }
    return true;
}

bool set_android_media_projection(void* media_projection, std::string* error) {
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JavaVM* vm = detail::find_vm_locked(s);
    JNIEnv* env = detail::env_for(vm);
    if (!env) {
        if (error) *error = "fizmo doesn't know the app's Java VM; call fizmo::audio::set_android_java_vm() first";
        return false;
    }
    jobject next = nullptr;
    if (media_projection) {
        next = env->NewGlobalRef(static_cast<jobject>(media_projection));
        if (!next) {
            if (error) *error = "the MediaProjection isn't a valid Java object";
            return false;
        }
    }
    if (s.projection) env->DeleteGlobalRef(s.projection);
    s.projection = next;
    return true;
}

bool has_android_media_projection() {
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.projection != nullptr;
}

bool android_permission_granted(const char* permission) {
    if (!permission || !*permission) return false;
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JNIEnv* env = detail::env_for(detail::find_vm_locked(s));
    if (!env) return false;
    return detail::permission_check_locked(s, env, permission) == 1;
}

bool request_android_permission(const char* permission, std::string* error) {
    if (!permission || !*permission) {
        if (error) *error = "no permission was named";
        return false;
    }
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JNIEnv* env = detail::env_for(detail::find_vm_locked(s));
    if (!env) {
        if (error) *error = "fizmo doesn't know the app's Java VM; call fizmo::audio::set_android_java_vm() first";
        return false;
    }
    std::string why;
    if (!detail::request_permission_locked(s, env, permission, why)) {
        if (error) *error = why;
        return false;
    }
    return true;
}

AndroidPermissionState android_permission_state(const char* permission) {
    if (!permission || !*permission) return AndroidPermissionState::denied;
    detail::JavaState& s = detail::java();
    std::lock_guard<std::mutex> lock(s.mutex);
    JNIEnv* env = detail::env_for(detail::find_vm_locked(s));
    if (!env) return AndroidPermissionState::denied;
    return detail::permission_state_locked(s, env, permission);
}

} // namespace audio
} // namespace fizmo

#else

namespace fizmo {
namespace audio {
namespace detail {

std::unique_ptr<Backend> make_aaudio_backend(std::string& why) {
    why = "AAudio is only on Android";
    return nullptr;
}

} // namespace detail

bool set_android_java_vm(void*, void*, std::string* error) {
    if (error) *error = "the Java VM is only used on Android";
    return false;
}

bool set_android_media_projection(void*, std::string* error) {
    if (error) *error = "MediaProjection only exists on Android";
    return false;
}

bool has_android_media_projection() {
    return false;
}

bool android_permission_granted(const char*) {
    return true;
}

bool request_android_permission(const char*, std::string*) {
    return true;
}

AndroidPermissionState android_permission_state(const char*) {
    return AndroidPermissionState::granted;
}

} // namespace audio
} // namespace fizmo

#endif // defined(__ANDROID__)
