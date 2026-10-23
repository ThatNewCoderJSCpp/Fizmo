#ifndef FIZMO_ANDROID_JNI_HPP
#define FIZMO_ANDROID_JNI_HPP

#include "android_types.hpp"

#if defined(OS_ANDROID)

#include <jni.h>

#include <cstdint>
#include <string>

namespace fizmo {
namespace android {
namespace detail {

struct JavaHooks {
    bool      fizmo          = false;
    jobject   activity       = nullptr;
    jmethodID show_keyboard  = nullptr;
    jmethodID hide_keyboard  = nullptr;
    jmethodID set_clipboard  = nullptr;
    jmethodID get_clipboard  = nullptr;
    jmethodID vibrate        = nullptr;
    jmethodID open_url       = nullptr;
    jmethodID share_text     = nullptr;
    jmethodID orientation    = nullptr;
    jmethodID immersive      = nullptr;
    jmethodID unicode_char   = nullptr;
    jclass    key_event      = nullptr;
    jmethodID key_event_new  = nullptr;
    jmethodID key_event_char = nullptr;
};

struct DeviceDetails {
    std::string   name;
    std::uint16_t vendor  = 0;
    std::uint16_t product = 0;
};

class LocalFrame {
private:
    JNIEnv* m_env;
    bool    m_pushed;

public:
    LocalFrame(JNIEnv* env, jint capacity) noexcept : m_env(env), m_pushed(env && env->PushLocalFrame(capacity) == JNI_OK) {}
    ~LocalFrame() { if (m_pushed) m_env->PopLocalFrame(nullptr); }
    LocalFrame(const LocalFrame&) = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;
    explicit operator bool() const noexcept { return m_pushed; }
};

JNIEnv* env_for_thread(JavaVM* vm) noexcept;
bool clear_exception(JNIEnv* env) noexcept;
jclass find_class_in_hierarchy(JNIEnv* env, jobject object, const char* dotted_name) noexcept;

std::string utf8_of(JNIEnv* env, jstring s);
std::string utf8_of(std::uint32_t code_point);
jstring java_string(JNIEnv* env, const std::string& utf8);

JavaHooks load_java_hooks(JNIEnv* env, jobject activity) noexcept;
void release_java_hooks(JNIEnv* env, JavaHooks& hooks) noexcept;

DeviceDetails device_details(JNIEnv* env, int device_id);
std::uint32_t unicode_char(JNIEnv* env, const JavaHooks& hooks, int key_code, int meta_state) noexcept;
std::string package_name(JNIEnv* env, jobject activity);

bool move_task_to_back(JNIEnv* env, const JavaHooks& hooks, jobject activity) noexcept;
bool set_orientation(JNIEnv* env, const JavaHooks& hooks, jobject activity, int value) noexcept;
bool set_immersive(JNIEnv* env, const JavaHooks& hooks, jobject activity, bool on) noexcept;
bool show_keyboard(JNIEnv* env, const JavaHooks& hooks, jobject activity, int kind) noexcept;
bool hide_keyboard(JNIEnv* env, const JavaHooks& hooks, jobject activity) noexcept;
std::string clipboard_text(JNIEnv* env, const JavaHooks& hooks, jobject activity);
bool set_clipboard_text(JNIEnv* env, const JavaHooks& hooks, jobject activity, const std::string& text);
bool vibrate(JNIEnv* env, const JavaHooks& hooks, jobject activity, unsigned int ms) noexcept;
bool open_url(JNIEnv* env, const JavaHooks& hooks, jobject activity, const std::string& url) noexcept;
bool share_text(JNIEnv* env, const JavaHooks& hooks, jobject activity, const std::string& text, const std::string& title) noexcept;

std::string cache_dir(JNIEnv* env, jobject activity);
std::string native_library_dir(JNIEnv* env, jobject activity);
std::string locale(JNIEnv* env);
float font_scale(JNIEnv* env, jobject activity) noexcept;

} // namespace detail
} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
#endif // FIZMO_ANDROID_JNI_HPP
