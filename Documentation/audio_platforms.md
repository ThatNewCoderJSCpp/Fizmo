# Audio: platform notes

`#include <audio.hpp>` gives `fizmo::audio`. Files, codecs, effects and the mixer work the same everywhere; sound devices go through one backend per platform, and a few platforms need setup from the app before recording works.

```cpp
using namespace fizmo::audio;

StreamConfig cfg;
cfg.kind = DeviceKind::loopback;
Stream rec;
std::string err;
if (!rec.open_capture(cfg, [](const float* in, std::size_t frames) { }, &err) || !rec.start(&err)) std::cerr << err << '\n';
```

## Backends

| Platform | Backend | Playback | Microphone | System audio (`DeviceKind::loopback`) |
|---|---|---|---|---|
| Windows | `wasapi` | yes | yes | yes, WASAPI loopback of any output device |
| Linux | `pulse` (PulseAudio or PipeWire), then `alsa` | yes | yes | with `pulse`: the `.monitor` source of each output |
| macOS | `coreaudio` | yes | yes, needs `NSMicrophoneUsageDescription` | macOS 14.2 or newer, needs `NSAudioCaptureUsageDescription` |
| Android | `aaudio` (Android 8.0, API 26, or newer) | yes | yes, needs `RECORD_AUDIO` | Android 10 (API 29) or newer, needs a `MediaProjection` |
| any | `null` | silent, on a clock | silence | silence |

`available_backends()` lists what this build has, `backend_name()` says which one is in use, and `set_backend(name, &error)` switches. The environment variable `FIZMO_AUDIO_BACKEND` (such as `null` for tests and servers with no sound card) picks one before the first device call. `devices(kind)` and `default_device(kind)` list what the backend sees; loopback devices are only listed where system audio can be recorded.

## macOS

Core Audio asks the user before an app records, and shows the text from the app bundle's `Info.plist`. Without the key, fizmo refuses to open the stream and the error says which key is missing (macOS would otherwise end the app).

| `Info.plist` key | Needed for |
|---|---|
| `NSMicrophoneUsageDescription` | `DeviceKind::input` (microphones and other inputs) |
| `NSAudioCaptureUsageDescription` | `DeviceKind::loopback` (system audio) |

```xml
<key>NSMicrophoneUsageDescription</key>
<string>Records your voice for the voice changer.</string>
<key>NSAudioCaptureUsageDescription</key>
<string>Records what other apps play so it can be mixed into the stream.</string>
```

System audio uses Core Audio process taps, which macOS 14.2 added. On older versions opening a loopback stream fails with "recording system audio needs macOS 14.2 or newer". A loopback stream follows the default output device: `device` empty or `"system"`. Programs run from a terminal rather than an app bundle have no `Info.plist`; macOS then asks on behalf of the terminal.

## Android

Playback needs no permission. Recording (a microphone or system audio) needs `<uses-permission android:name="android.permission.RECORD_AUDIO"/>` in the manifest and the user's consent at run time. When it isn't granted yet, opening an input or loopback stream asks for it (`Activity.requestPermissions`, so it works from a plain `NativeActivity`) and fails with an error saying the permission prompt was shown; open the stream again once `android_permission_state("android.permission.RECORD_AUDIO")` turns `granted`. If the manifest doesn't list the permission, the error says so instead. Recording system audio uses `AudioPlaybackCaptureConfiguration`, so it needs Android 10 (API 29) and a `MediaProjection` from the user:

1. Ask with `MediaProjectionManager.createScreenCaptureIntent()`.
2. Start a foreground service of type `mediaProjection` and get the `MediaProjection` from the result.
3. Give fizmo the Java VM and the projection:

```cpp
extern "C" JNIEXPORT void JNICALL Java_com_example_App_onProjection(JNIEnv* env, jobject self, jobject projection) {
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    std::string err;
    fizmo::audio::set_android_java_vm(vm, self, &err);
    fizmo::audio::set_android_media_projection(projection, &err);
}
```

| Function | Does |
|---|---|
| `set_android_java_vm(java_vm, context = nullptr, &error)` | Tells fizmo the app's `JavaVM*` and, optionally, a `Context` (any activity or service; fizmo keeps its application context, and a weak reference to an activity for asking for permissions). Apps built on fizmo's own Android app layer don't need it, and fizmo also looks for a created VM by itself |
| `set_android_media_projection(media_projection, &error)` | Hands over the `MediaProjection` object (a `jobject`); fizmo keeps a global reference. Needs the Java VM first |
| `has_android_media_projection()` | Whether one has been given |
| `android_permission_granted(permission)` | Whether Android says the app holds a permission, such as `"android.permission.RECORD_AUDIO"`; `false` when fizmo can't reach the Java VM. Always `true` off Android |
| `request_android_permission(permission, &error)` | Shows Android's prompt for a permission (API 23+), from any thread. `true` when it's already granted or the prompt was shown (or is still showing); `false` with a reason when it can't ask: no Java VM or Activity, or the manifest doesn't list the permission. Always `true` off Android |
| `android_permission_state(permission)` | `granted`, `pending` while a prompt fizmo showed is waiting for the user, or `denied` (never asked, or the user said no). Fizmo tells the prompt is open from the activity losing window focus, so poll it a few times a second rather than once; a prompt that doesn't appear within 3 seconds (the user chose "don't ask again") counts as `denied`. `android_permission_state_name()` gives the name. Always `granted` off Android |

Only the default mix can be recorded (`device` empty or `"default"`), and Android only records apps whose sound is media, game or unknown usage and that allow capture. Without a projection, or below API 29, opening a loopback stream fails with an error that says what is missing.
