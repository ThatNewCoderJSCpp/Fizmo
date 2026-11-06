#include "backend.hpp"
#include "format.hpp"
#include "resampler.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>

namespace fizmo {
namespace audio {
namespace detail {

namespace {

constexpr AudioObjectPropertyElement k_element_main = 0;
constexpr AudioObjectPropertySelector k_tap_property_format = 'tfmt';
constexpr UInt32 k_event_default = 1u;
constexpr UInt32 k_event_dead = 2u;
constexpr UInt32 k_event_format = 4u;
const AudioObjectPropertySelector k_watched_properties[] = { kAudioDevicePropertyDeviceIsAlive, kAudioDevicePropertyNominalSampleRate };

std::string fourcc_text(OSStatus status) {
    const UInt32 v = static_cast<UInt32>(status);
    char c[4] = { static_cast<char>((v >> 24) & 0xFF), static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 8) & 0xFF), static_cast<char>(v & 0xFF) };
    bool printable = true;
    for (char ch : c) {
        if (ch < 32 || ch > 126) printable = false;
    }
    char buf[32];
    if (printable) std::snprintf(buf, sizeof(buf), "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
    else std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(status));
    return buf;
}

std::string status_text(OSStatus status) {
    std::string name;
    switch (status) {
        case kAudioHardwareNotRunningError: name = "the Core Audio server isn't running"; break;
        case kAudioHardwareUnspecifiedError: name = "unspecified Core Audio error"; break;
        case kAudioHardwareUnknownPropertyError: name = "the device doesn't have that property"; break;
        case kAudioHardwareBadPropertySizeError: name = "bad property size"; break;
        case kAudioHardwareIllegalOperationError: name = "the device can't do that"; break;
        case kAudioHardwareBadObjectError: name = "the audio object doesn't exist"; break;
        case kAudioHardwareBadDeviceError: name = "the device was removed or doesn't exist"; break;
        case kAudioHardwareBadStreamError: name = "the device stream doesn't exist"; break;
        case kAudioHardwareUnsupportedOperationError: name = "the operation isn't supported (recording system audio may be unsupported here)"; break;
        case kAudioDeviceUnsupportedFormatError: name = "the device doesn't support that format"; break;
        case kAudioDevicePermissionsError: name = "the device is in exclusive use by another program, or access was denied"; break;
        case kAudioUnitErr_FormatNotSupported: name = "the format isn't supported"; break;
        case kAudioUnitErr_InvalidProperty: name = "invalid audio unit property"; break;
        case kAudioUnitErr_InvalidPropertyValue: name = "invalid audio unit property value"; break;
        case kAudioUnitErr_CannotDoInCurrentContext: name = "the audio unit can't do that right now"; break;
        case kAudioUnitErr_FailedInitialization: name = "the audio unit failed to initialize"; break;
        case kAudioUnitErr_NoConnection: name = "the audio unit has no input connected"; break;
        case kAudioUnitErr_TooManyFramesToProcess: name = "too many frames asked for at once"; break;
        case kAudio_ParamError: name = "invalid argument"; break;
        case kAudio_MemFullError: name = "out of memory"; break;
        default: break;
    }
    const std::string code = fourcc_text(status);
    return name.empty() ? std::string("error ") + code : name + " (" + code + ")";
}

std::string cf_string(CFStringRef s) {
    if (!s) return std::string();
    const char* direct = CFStringGetCStringPtr(s, kCFStringEncodingUTF8);
    if (direct) return std::string(direct);
    const CFIndex length = CFStringGetLength(s);
    const CFIndex max = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    if (max <= 1) return std::string();
    std::string out(static_cast<std::size_t>(max), '\0');
    if (!CFStringGetCString(s, &out[0], max, kCFStringEncodingUTF8)) return std::string();
    out.resize(std::strlen(out.c_str()));
    return out;
}

class CfString {
public:
    explicit CfString(const std::string& s) : m_ref(CFStringCreateWithCString(kCFAllocatorDefault, s.c_str(), kCFStringEncodingUTF8)) {}
    explicit CfString(CFStringRef owned) : m_ref(owned) {}
    ~CfString() {
        if (m_ref) CFRelease(m_ref);
    }
    CfString(const CfString&) = delete;
    CfString& operator=(const CfString&) = delete;
    CFStringRef get() const noexcept { return m_ref; }

private:
    CFStringRef m_ref = nullptr;
};

AudioObjectPropertyAddress property(AudioObjectPropertySelector selector, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal) {
    AudioObjectPropertyAddress a;
    a.mSelector = selector;
    a.mScope = scope;
    a.mElement = k_element_main;
    return a;
}

AudioObjectPropertyScope scope_of(bool input) {
    return input ? static_cast<AudioObjectPropertyScope>(kAudioObjectPropertyScopeInput) : static_cast<AudioObjectPropertyScope>(kAudioObjectPropertyScopeOutput);
}

std::vector<AudioObjectID> all_devices() {
    const AudioObjectPropertyAddress a = property(kAudioHardwarePropertyDevices);
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size) != noErr || size == 0) return {};
    std::vector<AudioObjectID> ids(size / sizeof(AudioObjectID));
    if (ids.empty()) return ids;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data()) != noErr) return {};
    ids.resize(size / sizeof(AudioObjectID));
    return ids;
}

AudioObjectID system_default(bool input) {
    const AudioObjectPropertyAddress a = property(input ? static_cast<AudioObjectPropertySelector>(kAudioHardwarePropertyDefaultInputDevice) : static_cast<AudioObjectPropertySelector>(kAudioHardwarePropertyDefaultOutputDevice));
    AudioObjectID id = kAudioObjectUnknown;
    UInt32 size = sizeof(id);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, &id) != noErr) return kAudioObjectUnknown;
    return id;
}

std::string device_string(AudioObjectID id, AudioObjectPropertySelector selector) {
    const AudioObjectPropertyAddress a = property(selector);
    CFStringRef s = nullptr;
    UInt32 size = sizeof(s);
    if (AudioObjectGetPropertyData(id, &a, 0, nullptr, &size, &s) != noErr || !s) return std::string();
    std::string out = cf_string(s);
    CFRelease(s);
    return out;
}

std::uint32_t channel_count(AudioObjectID id, bool input) {
    const AudioObjectPropertyAddress a = property(kAudioDevicePropertyStreamConfiguration, scope_of(input));
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(id, &a, 0, nullptr, &size) != noErr || size < sizeof(AudioBufferList)) return 0;
    std::vector<std::uint64_t> storage((size + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t) + 1);
    AudioBufferList* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(id, &a, 0, nullptr, &size, list) != noErr) return 0;
    std::uint32_t channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i) channels += list->mBuffers[i].mNumberChannels;
    return channels;
}

double nominal_rate(AudioObjectID id) {
    const AudioObjectPropertyAddress a = property(kAudioDevicePropertyNominalSampleRate);
    Float64 rate = 0.0;
    UInt32 size = sizeof(rate);
    if (AudioObjectGetPropertyData(id, &a, 0, nullptr, &size, &rate) != noErr) return 0.0;
    return rate;
}

UInt32 device_u32(AudioObjectID id, AudioObjectPropertySelector selector, AudioObjectPropertyScope scope, UInt32 fallback) {
    const AudioObjectPropertyAddress a = property(selector, scope);
    UInt32 value = 0;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(id, &a, 0, nullptr, &size, &value) != noErr) return fallback;
    return value;
}

bool device_alive(AudioObjectID id) {
    return device_u32(id, kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, 1) != 0;
}

AudioObjectID find_device(const std::string& uid, bool input) {
    for (AudioObjectID id : all_devices()) {
        if (channel_count(id, input) == 0) continue;
        if (device_string(id, kAudioDevicePropertyDeviceUID) == uid) return id;
    }
    return kAudioObjectUnknown;
}

std::string display_name(AudioObjectID id) {
    std::string name = device_string(id, kAudioObjectPropertyName);
    if (name.empty()) name = device_string(id, kAudioDevicePropertyDeviceUID);
    return name;
}

bool bundle_lacks(const char* key) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle || !CFBundleGetIdentifier(bundle)) return false;
    const CfString k{ std::string(key) };
    if (!k.get()) return false;
    return CFBundleGetValueForInfoDictionaryKey(bundle, k.get()) == nullptr;
}

void set_buffer_frames(AudioObjectID device, UInt32 frames) {
    AudioObjectPropertyAddress range_address = property(kAudioDevicePropertyBufferFrameSizeRange);
    AudioValueRange range;
    UInt32 size = sizeof(range);
    if (AudioObjectGetPropertyData(device, &range_address, 0, nullptr, &size, &range) == noErr) {
        frames = static_cast<UInt32>(std::max<Float64>(range.mMinimum, std::min<Float64>(range.mMaximum, static_cast<Float64>(frames))));
    }
    const AudioObjectPropertyAddress a = property(kAudioDevicePropertyBufferFrameSize);
    AudioObjectSetPropertyData(device, &a, 0, nullptr, sizeof(frames), &frames);
}

AudioStreamBasicDescription float_format(double rate, std::uint32_t channels) {
    AudioStreamBasicDescription f;
    std::memset(&f, 0, sizeof(f));
    f.mSampleRate = rate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    f.mBytesPerPacket = channels * 4;
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = channels * 4;
    f.mChannelsPerFrame = channels;
    f.mBitsPerChannel = 32;
    return f;
}

struct ObjcApi {
    void* (*get_class)(const char*) = nullptr;
    void* (*selector)(const char*) = nullptr;
    void* send = nullptr;
    void* (*pool_push)() = nullptr;
    void (*pool_pop)(void*) = nullptr;
    OSStatus (*create_tap)(void*, AudioObjectID*) = nullptr;
    OSStatus (*destroy_tap)(AudioObjectID) = nullptr;
    void* tap_class = nullptr;
    bool ok = false;

#if defined(__arm64__) || defined(__aarch64__)
    using objc_bool = bool;
#else
    using objc_bool = signed char;
#endif

    ObjcApi() {
        get_class = reinterpret_cast<void* (*)(const char*)>(dlsym(RTLD_DEFAULT, "objc_getClass"));
        selector = reinterpret_cast<void* (*)(const char*)>(dlsym(RTLD_DEFAULT, "sel_registerName"));
        send = dlsym(RTLD_DEFAULT, "objc_msgSend");
        pool_push = reinterpret_cast<void* (*)()>(dlsym(RTLD_DEFAULT, "objc_autoreleasePoolPush"));
        pool_pop = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_DEFAULT, "objc_autoreleasePoolPop"));
        create_tap = reinterpret_cast<OSStatus (*)(void*, AudioObjectID*)>(dlsym(RTLD_DEFAULT, "AudioHardwareCreateProcessTap"));
        destroy_tap = reinterpret_cast<OSStatus (*)(AudioObjectID)>(dlsym(RTLD_DEFAULT, "AudioHardwareDestroyProcessTap"));
        if (!get_class || !selector || !send || !pool_push || !pool_pop || !create_tap || !destroy_tap) return;
        tap_class = get_class("CATapDescription");
        if (!tap_class) return;
        ok = instances_respond(tap_class, "initStereoGlobalTapButExcludeProcesses:") && instances_respond(tap_class, "UUID");
    }

    void* msg(void* target, const char* name) const {
        return reinterpret_cast<void* (*)(void*, void*)>(send)(target, selector(name));
    }

    void* msg(void* target, const char* name, const void* arg) const {
        return reinterpret_cast<void* (*)(void*, void*, const void*)>(send)(target, selector(name), arg);
    }

    void msg_bool(void* target, const char* name, bool value) const {
        reinterpret_cast<void (*)(void*, void*, objc_bool)>(send)(target, selector(name), static_cast<objc_bool>(value ? 1 : 0));
    }

    bool responds(void* target, const char* name) const {
        return reinterpret_cast<objc_bool (*)(void*, void*, void*)>(send)(target, selector("respondsToSelector:"), selector(name)) != 0;
    }

    bool instances_respond(void* cls, const char* name) const {
        return reinterpret_cast<objc_bool (*)(void*, void*, void*)>(send)(cls, selector("instancesRespondToSelector:"), selector(name)) != 0;
    }
};

const ObjcApi& objc() {
    static const ObjcApi api;
    return api;
}

bool taps_available() {
    return objc().ok;
}

std::mutex& listener_mutex() {
    static std::mutex m;
    return m;
}

class CoreAudioStream;

std::vector<CoreAudioStream*>& live_streams() {
    static std::vector<CoreAudioStream*> v;
    return v;
}

class CoreAudioStream : public BackendStream {
public:
    CoreAudioStream(const StreamRequest& request, std::shared_ptr<StreamShared> shared)
        : m_request(request), m_shared(std::move(shared)) {
        m_input = request.kind == DeviceKind::input;
        m_loopback = request.kind == DeviceKind::loopback;
        m_follow_default = request.device.empty() || (m_loopback && request.device == "system");
        m_pinned = request.pin_device && !m_follow_default;
        m_force_convert = std::getenv("FIZMO_AUDIO_COREAUDIO_CONVERT") != nullptr;
    }

    ~CoreAudioStream() override {
        {
            std::lock_guard<std::mutex> lock(listener_mutex());
            auto& v = live_streams();
            v.erase(std::remove(v.begin(), v.end(), this), v.end());
        }
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_quit = true;
        }
        m_event_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
        std::lock_guard<std::mutex> lock(m_control);
        if (m_running) end();
        m_running = false;
        teardown();
        if (m_default_listening) {
            const AudioObjectPropertyAddress a = default_address();
            AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &a, on_property, this);
        }
    }

    bool open(std::string& error) {
        {
            std::lock_guard<std::mutex> lock(m_control);
            if (!setup(error)) {
                teardown();
                return false;
            }
        }
        {
            std::lock_guard<std::mutex> lock(listener_mutex());
            live_streams().push_back(this);
        }
        if (m_follow_default) {
            const AudioObjectPropertyAddress a = default_address();
            m_default_listening = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &a, on_property, this) == noErr;
        }
        m_worker = std::thread([this] { worker(); });
        return true;
    }

    bool start(std::string& error) override {
        std::lock_guard<std::mutex> lock(m_control);
        if (m_running) return true;
        if (!ready() && !setup(error)) {
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
        return m_device_name;
    }

    void notify(UInt32 events) {
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_pending |= events;
        }
        m_event_cv.notify_all();
    }

private:
    AudioObjectPropertyAddress default_address() const {
        return property(m_input ? static_cast<AudioObjectPropertySelector>(kAudioHardwarePropertyDefaultInputDevice) : static_cast<AudioObjectPropertySelector>(kAudioHardwarePropertyDefaultOutputDevice));
    }

    static OSStatus on_property(AudioObjectID object, UInt32 count, const AudioObjectPropertyAddress* addresses, void* client) {
        UInt32 events = 0;
        for (UInt32 i = 0; i < count; ++i) {
            switch (addresses[i].mSelector) {
                case kAudioHardwarePropertyDefaultOutputDevice:
                case kAudioHardwarePropertyDefaultInputDevice:
                    events |= k_event_default;
                    break;
                case kAudioDevicePropertyDeviceIsAlive:
                    if (!device_alive(object)) events |= k_event_dead;
                    break;
                case kAudioDevicePropertyNominalSampleRate:
                case kAudioDevicePropertyStreamConfiguration:
                    events |= k_event_format;
                    break;
                default:
                    break;
            }
        }
        if (events == 0) return noErr;
        std::lock_guard<std::mutex> lock(listener_mutex());
        auto& v = live_streams();
        CoreAudioStream* self = static_cast<CoreAudioStream*>(client);
        if (std::find(v.begin(), v.end(), self) != v.end()) self->notify(events);
        return noErr;
    }

    bool ready() const noexcept {
        return m_loopback ? m_aggregate != kAudioObjectUnknown : m_unit != nullptr;
    }

    void watch_device(AudioObjectID device) {
        m_watched = device;
        for (AudioObjectPropertySelector sel : k_watched_properties) {
            const AudioObjectPropertyAddress a = property(sel);
            AudioObjectAddPropertyListener(device, &a, on_property, this);
        }
    }

    void unwatch_device() {
        if (m_watched == kAudioObjectUnknown) return;
        for (AudioObjectPropertySelector sel : k_watched_properties) {
            const AudioObjectPropertyAddress a = property(sel);
            AudioObjectRemovePropertyListener(m_watched, &a, on_property, this);
        }
        m_watched = kAudioObjectUnknown;
    }

    bool resolve_device(AudioObjectID& device, std::string& error) {
        if (m_follow_default) {
            device = system_default(m_input);
            if (device == kAudioObjectUnknown) {
                error = std::string("there's no default ") + (m_input ? "recording" : "playback") + " device";
                return false;
            }
            return true;
        }
        device = find_device(m_request.device, m_input);
        if (device == kAudioObjectUnknown) {
            error = "can't find device '" + m_request.device + "'";
            return false;
        }
        return true;
    }

    std::string pinned_lost_message() const {
        std::string shown;
        {
            std::lock_guard<std::mutex> lock(m_name_mutex);
            shown = m_device_name;
        }
        return "the device '" + (shown.empty() ? m_request.device : shown) + "' was removed or disabled";
    }

    bool device_present() const {
        const AudioObjectID id = find_device(m_request.device, m_input && !m_loopback);
        return id != kAudioObjectUnknown && device_alive(id);
    }

    void set_name(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_name_mutex);
        m_device_name = name;
    }

    bool setup(std::string& error) {
        teardown();
        return m_loopback ? setup_tap(error) : setup_unit(error);
    }

    void teardown() {
        unwatch_device();
        teardown_unit();
        teardown_tap();
    }

    bool begin(std::string& error) {
        m_render_error.store(0);
        if (m_loopback) {
            const OSStatus st = AudioDeviceStart(m_aggregate, m_proc);
            if (st != noErr) {
                error = "can't start recording system audio: " + status_text(st);
                return false;
            }
            return true;
        }
        const OSStatus st = AudioOutputUnitStart(m_unit);
        if (st != noErr) {
            error = "can't start the audio device: " + status_text(st);
            return false;
        }
        return true;
    }

    void end() {
        if (m_loopback) {
            if (m_aggregate != kAudioObjectUnknown && m_proc) AudioDeviceStop(m_aggregate, m_proc);
        } else if (m_unit) {
            AudioOutputUnitStop(m_unit);
        }
    }

    void prepare_conversion(std::size_t max_frames) {
        const std::size_t uc = m_shared->channels;
        const std::size_t dc = m_device_channels;
        const std::size_t widest = std::max(uc, dc);
        const double ratio = static_cast<double>(std::max<std::uint32_t>(m_shared->sample_rate, m_device_rate)) / static_cast<double>(std::max<std::uint32_t>(1, std::min<std::uint32_t>(m_shared->sample_rate, m_device_rate)));
        const std::size_t room = static_cast<std::size_t>(static_cast<double>(max_frames + 512) * ratio) + 1024;
        m_raw.assign(std::max<std::size_t>(max_frames, 1) * widest, 0.0f);
        m_user.assign(k_chunk * widest, 0.0f);
        m_fifo.clear();
        m_fifo.reserve(room * widest * 2);
    }

    bool setup_unit(std::string& error) {
        AudioObjectID device = kAudioObjectUnknown;
        if (!resolve_device(device, error)) return false;
        const std::uint32_t device_channels = channel_count(device, m_input);
        if (device_channels == 0) {
            error = "'" + display_name(device) + "' has no " + (m_input ? "input" : "output") + " channels";
            return false;
        }
        if (m_input && bundle_lacks("NSMicrophoneUsageDescription")) {
            error = "recording from the microphone needs NSMicrophoneUsageDescription in the app's Info.plist (macOS shows that text when it asks the user for permission)";
            return false;
        }
        double device_rate = nominal_rate(device);
        if (device_rate <= 0.0) device_rate = 48000.0;
        AudioComponentDescription desc;
        std::memset(&desc, 0, sizeof(desc));
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent component = AudioComponentFindNext(nullptr, &desc);
        if (!component) {
            error = "can't find Core Audio's HAL output unit";
            return false;
        }
        OSStatus st = AudioComponentInstanceNew(component, &m_unit);
        if (st != noErr || !m_unit) {
            m_unit = nullptr;
            error = "can't create a Core Audio output unit: " + status_text(st);
            return false;
        }
        if (m_input) {
            const UInt32 on = 1;
            const UInt32 off = 0;
            st = AudioUnitSetProperty(m_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &on, sizeof(on));
            if (st == noErr) st = AudioUnitSetProperty(m_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &off, sizeof(off));
            if (st != noErr) {
                error = "can't switch the audio unit to recording: " + status_text(st);
                return false;
            }
        }
        st = AudioUnitSetProperty(m_unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, sizeof(device));
        if (st != noErr) {
            error = "can't open '" + display_name(device) + "': " + status_text(st);
            return false;
        }
        const std::uint32_t rate = m_shared->sample_rate ? m_shared->sample_rate : (m_request.sample_rate ? m_request.sample_rate : static_cast<std::uint32_t>(device_rate + 0.5));
        const std::uint32_t channels = m_shared->channels ? m_shared->channels : (m_request.channels ? m_request.channels : device_channels);
        set_buffer_frames(device, std::max<UInt32>(32, static_cast<UInt32>(device_rate * m_request.latency_ms / 2000.0)));
        const AudioUnitScope client_scope = m_input ? static_cast<AudioUnitScope>(kAudioUnitScope_Output) : static_cast<AudioUnitScope>(kAudioUnitScope_Input);
        const AudioUnitElement client_element = m_input ? 1 : 0;
        bool direct = !m_force_convert && (!m_input || static_cast<double>(rate) == device_rate);
        if (direct) {
            const AudioStreamBasicDescription want = float_format(rate, channels);
            direct = AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat, client_scope, client_element, &want, sizeof(want)) == noErr;
        }
        if (direct) {
            m_convert = false;
            m_device_rate = rate;
            m_device_channels = channels;
        } else {
            const AudioStreamBasicDescription native = float_format(device_rate, device_channels);
            st = AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat, client_scope, client_element, &native, sizeof(native));
            if (st != noErr) {
                error = "the audio device won't take " + std::to_string(device_channels) + " float channels at " + std::to_string(static_cast<std::uint32_t>(device_rate + 0.5)) + " Hz: " + status_text(st);
                return false;
            }
            m_convert = true;
            m_device_rate = static_cast<std::uint32_t>(device_rate + 0.5);
            m_device_channels = device_channels;
        }
        m_shared->sample_rate = rate;
        m_shared->channels = channels;
        AURenderCallbackStruct cb;
        cb.inputProcRefCon = this;
        if (m_input) {
            cb.inputProc = on_input;
            st = AudioUnitSetProperty(m_unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &cb, sizeof(cb));
        } else {
            cb.inputProc = on_render;
            st = AudioUnitSetProperty(m_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb));
        }
        if (st != noErr) {
            error = "can't set up the audio callback: " + status_text(st);
            return false;
        }
        st = AudioUnitInitialize(m_unit);
        if (st != noErr) {
            error = "can't start the audio device: " + status_text(st);
            return false;
        }
        m_initialized = true;
        UInt32 max_frames = 4096;
        UInt32 size = sizeof(max_frames);
        if (AudioUnitGetProperty(m_unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &max_frames, &size) != noErr || max_frames == 0) max_frames = 4096;
        m_max_frames = max_frames;
        const std::size_t widest = std::max<std::size_t>(m_device_channels, m_shared->channels);
        if (m_input) m_capture.assign(static_cast<std::size_t>(max_frames) * widest, 0.0f);
        if (m_convert) {
            if (m_input) m_resampler.configure(m_shared->channels, m_device_rate, rate, ResampleQuality::medium);
            else m_resampler.configure(m_device_channels, rate, m_device_rate, ResampleQuality::medium);
            prepare_conversion(max_frames);
        }
        const AudioObjectPropertyScope scope = scope_of(m_input);
        const UInt32 buffer = device_u32(device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, max_frames);
        const UInt32 hw = device_u32(device, kAudioDevicePropertyLatency, scope, 0) + device_u32(device, kAudioDevicePropertySafetyOffset, scope, 0);
        m_latency.store(static_cast<std::uint32_t>((static_cast<std::uint64_t>(buffer) * 2 + hw) * rate / std::max<std::uint32_t>(1, static_cast<std::uint32_t>(device_rate + 0.5))));
        m_device = device;
        set_name(display_name(device));
        watch_device(device);
        return true;
    }

    void teardown_unit() {
        if (!m_unit) return;
        AudioOutputUnitStop(m_unit);
        if (m_initialized) AudioUnitUninitialize(m_unit);
        AudioComponentInstanceDispose(m_unit);
        m_unit = nullptr;
        m_initialized = false;
        m_device = kAudioObjectUnknown;
    }

    static OSStatus on_render(void* ref, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames, AudioBufferList* io) {
        static_cast<CoreAudioStream*>(ref)->fill_output(io, frames);
        return noErr;
    }

    void fill_output(AudioBufferList* io, UInt32 frames) noexcept {
        if (!io || io->mNumberBuffers == 0) return;
        for (UInt32 i = 1; i < io->mNumberBuffers; ++i) {
            if (io->mBuffers[i].mData) std::memset(io->mBuffers[i].mData, 0, io->mBuffers[i].mDataByteSize);
        }
        float* out = static_cast<float*>(io->mBuffers[0].mData);
        if (!out) return;
        const std::size_t capacity = io->mBuffers[0].mDataByteSize / (sizeof(float) * std::max<std::uint32_t>(1, m_device_channels));
        const std::size_t n = std::min<std::size_t>(frames, capacity);
        if (!m_convert) {
            m_shared->render(out, n);
            return;
        }
        const std::size_t dc = m_device_channels;
        const std::size_t uc = m_shared->channels;
        while (m_fifo.size() < n * dc) {
            m_shared->render(m_user.data(), k_chunk);
            remix(m_user.data(), uc, m_user.data(), dc, k_chunk);
            m_resampler.process(m_user.data(), k_chunk, m_fifo);
        }
        std::memcpy(out, m_fifo.data(), n * dc * sizeof(float));
        m_fifo.erase(m_fifo.begin(), m_fifo.begin() + static_cast<std::ptrdiff_t>(n * dc));
    }

    static OSStatus on_input(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time, UInt32 bus, UInt32 frames, AudioBufferList*) {
        static_cast<CoreAudioStream*>(ref)->pull_input(flags, time, bus, frames);
        return noErr;
    }

    void pull_input(AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time, UInt32 bus, UInt32 frames) noexcept {
        if (!m_unit || frames == 0) return;
        const std::size_t dc = m_device_channels;
        const std::size_t uc = m_shared->channels;
        const UInt32 n = std::min<UInt32>(frames, m_max_frames);
        AudioBufferList list;
        list.mNumberBuffers = 1;
        list.mBuffers[0].mNumberChannels = static_cast<UInt32>(dc);
        list.mBuffers[0].mDataByteSize = static_cast<UInt32>(n * dc * sizeof(float));
        list.mBuffers[0].mData = m_capture.data();
        const OSStatus st = AudioUnitRender(m_unit, flags, time, bus, n, &list);
        if (st != noErr) {
            if (st != kAudioUnitErr_CannotDoInCurrentContext && st != kAudioUnitErr_NoConnection) {
                OSStatus none = 0;
                if (m_render_error.compare_exchange_strong(none, st)) m_event_cv.notify_all();
            }
            return;
        }
        if (!m_convert) {
            m_shared->deliver(m_capture.data(), n);
            return;
        }
        remix(m_capture.data(), dc, m_capture.data(), uc, n);
        m_fifo.clear();
        const std::size_t got = m_resampler.process(m_capture.data(), n, m_fifo);
        if (got) m_shared->deliver(m_fifo.data(), got);
    }

    bool setup_tap(std::string& error) {
        const ObjcApi& api = objc();
        if (!api.ok) {
            error = "recording system audio needs macOS 14.2 or newer (Core Audio process taps aren't available here)";
            return false;
        }
        if (bundle_lacks("NSAudioCaptureUsageDescription")) {
            error = "recording system audio needs NSAudioCaptureUsageDescription in the app's Info.plist (macOS shows that text when it asks the user for permission)";
            return false;
        }
        AudioObjectID clock = kAudioObjectUnknown;
        if (m_follow_default) {
            clock = system_default(false);
        } else {
            clock = find_device(m_request.device, false);
            if (clock == kAudioObjectUnknown) {
                error = "can't find device '" + m_request.device + "'";
                return false;
            }
        }
        const std::string clock_uid = clock != kAudioObjectUnknown ? device_string(clock, kAudioDevicePropertyDeviceUID) : std::string();
        std::string tap_uid;
        {
            void* pool = api.pool_push();
            CFArrayRef none = CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks);
            void* desc = api.msg(api.msg(api.tap_class, "alloc"), "initStereoGlobalTapButExcludeProcesses:", none);
            if (none) CFRelease(none);
            if (desc) {
                if (api.responds(desc, "setPrivateTap:")) api.msg_bool(desc, "setPrivateTap:", true);
                else if (api.responds(desc, "setPrivate:")) api.msg_bool(desc, "setPrivate:", true);
                void* uuid = api.msg(desc, "UUID");
                void* text = uuid ? api.msg(uuid, "UUIDString") : nullptr;
                tap_uid = cf_string(static_cast<CFStringRef>(text));
                AudioObjectID tap = kAudioObjectUnknown;
                const OSStatus st = api.create_tap(desc, &tap);
                api.msg(desc, "release");
                if (st != noErr || tap == kAudioObjectUnknown) {
                    api.pool_pop(pool);
                    error = "can't create a system-audio tap: " + status_text(st);
                    return false;
                }
                m_tap = tap;
            }
            api.pool_pop(pool);
        }
        if (m_tap == kAudioObjectUnknown || tap_uid.empty()) {
            error = "can't describe a system-audio tap";
            return false;
        }
        AudioStreamBasicDescription tap_format;
        std::memset(&tap_format, 0, sizeof(tap_format));
        {
            const AudioObjectPropertyAddress a = property(k_tap_property_format);
            UInt32 size = sizeof(tap_format);
            const OSStatus st = AudioObjectGetPropertyData(m_tap, &a, 0, nullptr, &size, &tap_format);
            if (st != noErr) {
                error = "can't read the system-audio tap's format: " + status_text(st);
                return false;
            }
        }
        if (tap_format.mFormatID != kAudioFormatLinearPCM || !(tap_format.mFormatFlags & kAudioFormatFlagIsFloat) || tap_format.mBitsPerChannel != 32 || tap_format.mChannelsPerFrame == 0) {
            error = "the system-audio tap uses a sample format fizmo doesn't know";
            return false;
        }
        m_tap_channels = tap_format.mChannelsPerFrame;
        static std::atomic<unsigned> counter{ 0 };
        const std::string aggregate_uid = "fizmo.tap." + std::to_string(static_cast<long>(getpid())) + "." + std::to_string(counter.fetch_add(1));
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFMutableArrayRef taps = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFMutableArrayRef subs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFMutableDictionaryRef tap_entry = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!dict || !taps || !subs || !tap_entry) {
            if (dict) CFRelease(dict);
            if (taps) CFRelease(taps);
            if (subs) CFRelease(subs);
            if (tap_entry) CFRelease(tap_entry);
            error = "out of memory describing the system-audio device";
            return false;
        }
        CfString k_uid(std::string("uid"));
        CfString k_name(std::string("name"));
        CfString k_private(std::string("private"));
        CfString k_stacked(std::string("stacked"));
        CfString k_autostart(std::string("tapautostart"));
        CfString k_main(std::string("master"));
        CfString k_subdevices(std::string("subdevices"));
        CfString k_taps(std::string("taps"));
        CfString k_drift(std::string("drift"));
        CfString v_uid(aggregate_uid);
        CfString v_name(std::string("fizmo system audio"));
        CfString v_tap(tap_uid);
        CFDictionarySetValue(dict, k_uid.get(), v_uid.get());
        CFDictionarySetValue(dict, k_name.get(), v_name.get());
        CFDictionarySetValue(dict, k_private.get(), kCFBooleanTrue);
        CFDictionarySetValue(dict, k_stacked.get(), kCFBooleanFalse);
        CFDictionarySetValue(dict, k_autostart.get(), kCFBooleanTrue);
        CFDictionarySetValue(tap_entry, k_uid.get(), v_tap.get());
        CFDictionarySetValue(tap_entry, k_drift.get(), kCFBooleanTrue);
        CFArrayAppendValue(taps, tap_entry);
        CFDictionarySetValue(dict, k_taps.get(), taps);
        if (!clock_uid.empty()) {
            CfString v_clock(clock_uid);
            CFMutableDictionaryRef sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if (sub) {
                CFDictionarySetValue(sub, k_uid.get(), v_clock.get());
                CFArrayAppendValue(subs, sub);
                CFRelease(sub);
            }
            CFDictionarySetValue(dict, k_main.get(), v_clock.get());
        }
        CFDictionarySetValue(dict, k_subdevices.get(), subs);
        AudioObjectID aggregate = kAudioObjectUnknown;
        OSStatus st = AudioHardwareCreateAggregateDevice(dict, &aggregate);
        CFRelease(tap_entry);
        CFRelease(subs);
        CFRelease(taps);
        CFRelease(dict);
        if (st != noErr || aggregate == kAudioObjectUnknown) {
            error = "can't create the system-audio recording device: " + status_text(st);
            return false;
        }
        m_aggregate = aggregate;
        st = AudioDeviceCreateIOProcID(m_aggregate, on_io, this, &m_proc);
        if (st != noErr || !m_proc) {
            m_proc = nullptr;
            error = "can't set up the system-audio callback: " + status_text(st);
            return false;
        }
        double source_rate = tap_format.mSampleRate > 0.0 ? tap_format.mSampleRate : nominal_rate(m_aggregate);
        if (source_rate <= 0.0) source_rate = 48000.0;
        m_device_rate = static_cast<std::uint32_t>(source_rate + 0.5);
        m_device_channels = m_tap_channels;
        const std::uint32_t rate = m_shared->sample_rate ? m_shared->sample_rate : (m_request.sample_rate ? m_request.sample_rate : m_device_rate);
        const std::uint32_t channels = m_shared->channels ? m_shared->channels : (m_request.channels ? m_request.channels : m_tap_channels);
        m_shared->sample_rate = rate;
        m_shared->channels = channels;
        set_buffer_frames(m_aggregate, std::max<UInt32>(32, static_cast<UInt32>(source_rate * m_request.latency_ms / 2000.0)));
        const UInt32 buffer = device_u32(m_aggregate, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, 512);
        m_max_frames = std::max<UInt32>(buffer * 2, 1024);
        m_convert = true;
        m_resampler.configure(channels, m_device_rate, rate, ResampleQuality::medium);
        prepare_conversion(m_max_frames);
        m_latency.store(static_cast<std::uint32_t>(static_cast<std::uint64_t>(buffer) * 2 * rate / std::max<std::uint32_t>(1, m_device_rate)));
        set_name(clock != kAudioObjectUnknown ? display_name(clock) + " (system audio)" : std::string("System audio"));
        if (clock != kAudioObjectUnknown) watch_device(clock);
        return true;
    }

    void teardown_tap() {
        if (m_aggregate != kAudioObjectUnknown) {
            if (m_proc) {
                AudioDeviceStop(m_aggregate, m_proc);
                AudioDeviceDestroyIOProcID(m_aggregate, m_proc);
            }
            AudioHardwareDestroyAggregateDevice(m_aggregate);
        }
        m_proc = nullptr;
        m_aggregate = kAudioObjectUnknown;
        if (m_tap != kAudioObjectUnknown && objc().destroy_tap) objc().destroy_tap(m_tap);
        m_tap = kAudioObjectUnknown;
    }

    static OSStatus on_io(AudioObjectID, const AudioTimeStamp*, const AudioBufferList* input, const AudioTimeStamp*, AudioBufferList* output, const AudioTimeStamp*, void* client) {
        if (output) {
            for (UInt32 i = 0; i < output->mNumberBuffers; ++i) {
                if (output->mBuffers[i].mData) std::memset(output->mBuffers[i].mData, 0, output->mBuffers[i].mDataByteSize);
            }
        }
        if (input) static_cast<CoreAudioStream*>(client)->take_tap(input);
        return noErr;
    }

    void take_tap(const AudioBufferList* input) noexcept {
        const std::size_t tc = m_tap_channels;
        UInt32 first = input->mNumberBuffers;
        std::size_t found = 0;
        while (first > 0 && found < tc) {
            --first;
            found += input->mBuffers[first].mNumberChannels;
        }
        if (found == 0) return;
        std::size_t frames = static_cast<std::size_t>(-1);
        for (UInt32 b = first; b < input->mNumberBuffers; ++b) {
            const AudioBuffer& buf = input->mBuffers[b];
            if (!buf.mData || buf.mNumberChannels == 0) return;
            frames = std::min<std::size_t>(frames, buf.mDataByteSize / (sizeof(float) * buf.mNumberChannels));
        }
        if (frames == 0 || frames == static_cast<std::size_t>(-1)) return;
        const std::size_t uc = m_shared->channels;
        const std::size_t widest = std::max(tc, uc);
        const std::size_t step = std::max<std::size_t>(1, m_raw.size() / widest);
        for (std::size_t done = 0; done < frames; done += step) {
            const std::size_t n = std::min(step, frames - done);
            std::size_t channel = 0;
            for (UInt32 b = first; b < input->mNumberBuffers && channel < tc; ++b) {
                const AudioBuffer& buf = input->mBuffers[b];
                const float* src = static_cast<const float*>(buf.mData) + done * buf.mNumberChannels;
                const std::size_t take = std::min<std::size_t>(buf.mNumberChannels, tc - channel);
                for (std::size_t f = 0; f < n; ++f) {
                    for (std::size_t c = 0; c < take; ++c) m_raw[f * tc + channel + c] = src[f * buf.mNumberChannels + c];
                }
                channel += take;
            }
            for (; channel < tc; ++channel) {
                for (std::size_t f = 0; f < n; ++f) m_raw[f * tc + channel] = 0.0f;
            }
            remix(m_raw.data(), tc, m_raw.data(), uc, n);
            m_fifo.clear();
            const std::size_t got = m_resampler.process(m_raw.data(), n, m_fifo);
            if (got) m_shared->deliver(m_fifo.data(), got);
        }
    }

    void worker() {
        std::unique_lock<std::mutex> lock(m_event_mutex);
        while (!m_quit) {
            m_event_cv.wait_for(lock, std::chrono::milliseconds(250), [this] { return m_quit || m_pending != 0 || m_render_error.load() != 0; });
            if (m_quit) break;
            const UInt32 events = m_pending;
            m_pending = 0;
            const OSStatus failure = m_render_error.exchange(0);
            if (events == 0 && failure == 0) continue;
            lock.unlock();
            handle(events, failure);
            lock.lock();
        }
    }

    void handle(UInt32 events, OSStatus failure) {
        std::lock_guard<std::mutex> lock(m_control);
        if (m_shared->failed.load()) return;
        bool rebuild = false;
        bool moved = false;
        if (events & k_event_default) {
            if (m_follow_default) {
                const AudioObjectID now = system_default(m_input && !m_loopback);
                const AudioObjectID current = m_loopback ? m_watched : m_device;
                if (now != current) {
                    rebuild = true;
                    moved = true;
                }
            }
        }
        if (events & k_event_dead) {
            if (!m_follow_default) {
                if (m_running) end();
                m_shared->fail(m_pinned ? pinned_lost_message() : std::string("the audio device was removed or disabled"));
                return;
            }
            rebuild = true;
            moved = true;
        }
        if (events & k_event_format) rebuild = true;
        if (failure != 0 && !rebuild) {
            if (m_running) end();
            m_shared->fail("the audio device stopped: " + status_text(failure));
            return;
        }
        if (!rebuild) return;
        if (m_running) end();
        teardown();
        std::string why;
        if (m_pinned && !device_present()) {
            m_shared->fail(pinned_lost_message());
            return;
        }
        if (!setup(why) || (m_running && !begin(why))) {
            m_shared->fail(std::string(moved ? "lost the audio device and couldn't switch to the new default: " : "the audio device changed and couldn't be reopened: ") + why);
        }
    }

    static constexpr std::size_t k_chunk = 256;

    StreamRequest m_request;
    std::shared_ptr<StreamShared> m_shared;
    bool m_input = false;
    bool m_loopback = false;
    bool m_follow_default = true;
    bool m_pinned = false;
    bool m_force_convert = false;
    bool m_running = false;
    bool m_default_listening = false;
    AudioUnit m_unit = nullptr;
    bool m_initialized = false;
    AudioObjectID m_device = kAudioObjectUnknown;
    AudioObjectID m_watched = kAudioObjectUnknown;
    AudioObjectID m_tap = kAudioObjectUnknown;
    AudioObjectID m_aggregate = kAudioObjectUnknown;
    AudioDeviceIOProcID m_proc = nullptr;
    std::uint32_t m_tap_channels = 2;
    UInt32 m_max_frames = 4096;
    bool m_convert = false;
    std::uint32_t m_device_rate = 0;
    std::uint32_t m_device_channels = 0;
    Resampler m_resampler;
    std::vector<float> m_capture;
    std::vector<float> m_raw;
    std::vector<float> m_user;
    std::vector<float> m_fifo;
    std::atomic<std::uint32_t> m_latency{ 0 };
    std::atomic<OSStatus> m_render_error{ 0 };
    std::mutex m_control;
    mutable std::mutex m_name_mutex;
    std::string m_device_name;
    std::mutex m_event_mutex;
    std::condition_variable m_event_cv;
    UInt32 m_pending = 0;
    bool m_quit = false;
    std::thread m_worker;
};

class CoreAudioBackend : public Backend {
public:
    bool init(std::string& why) {
        CFRunLoopRef none = nullptr;
        const AudioObjectPropertyAddress loop = property(kAudioHardwarePropertyRunLoop);
        AudioObjectSetPropertyData(kAudioObjectSystemObject, &loop, 0, nullptr, sizeof(none), &none);
        const AudioObjectPropertyAddress a = property(kAudioHardwarePropertyDevices);
        UInt32 size = 0;
        const OSStatus st = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size);
        if (st != noErr) {
            why = "can't reach the Core Audio server: " + status_text(st);
            return false;
        }
        return true;
    }

    const char* name() const override { return "coreaudio"; }

    std::vector<DeviceInfo> devices() override {
        std::vector<DeviceInfo> out;
        const AudioObjectID default_out = system_default(false);
        const AudioObjectID default_in = system_default(true);
        std::vector<DeviceInfo> inputs;
        for (AudioObjectID id : all_devices()) {
            const std::string uid = device_string(id, kAudioDevicePropertyDeviceUID);
            if (uid.empty()) continue;
            const std::string name = display_name(id);
            const std::uint32_t rate = static_cast<std::uint32_t>(nominal_rate(id) + 0.5);
            const std::uint32_t out_channels = channel_count(id, false);
            const std::uint32_t in_channels = channel_count(id, true);
            DeviceInfo d;
            d.id = uid;
            d.name = name;
            d.sample_rate = rate;
            d.backend = "coreaudio";
            if (out_channels > 0) {
                d.kind = DeviceKind::output;
                d.channels = out_channels;
                d.is_default = id == default_out;
                out.push_back(d);
            }
            if (in_channels > 0) {
                d.kind = DeviceKind::input;
                d.channels = in_channels;
                d.is_default = id == default_in;
                inputs.push_back(d);
            }
        }
        out.insert(out.end(), inputs.begin(), inputs.end());
        if (taps_available()) {
            DeviceInfo d;
            d.id = "system";
            d.name = "System audio";
            d.kind = DeviceKind::loopback;
            d.is_default = true;
            d.sample_rate = default_out != kAudioObjectUnknown ? static_cast<std::uint32_t>(nominal_rate(default_out) + 0.5) : 48000;
            d.channels = 2;
            d.backend = "coreaudio";
            out.push_back(d);
        }
        return out;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) override {
        auto stream = std::make_unique<CoreAudioStream>(request, std::move(shared));
        if (!stream->open(error)) return nullptr;
        return stream;
    }
};

} // namespace

std::unique_ptr<Backend> make_coreaudio_backend(std::string& why) {
    auto b = std::make_unique<CoreAudioBackend>();
    if (!b->init(why)) return nullptr;
    return b;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#else

namespace fizmo {
namespace audio {
namespace detail {

std::unique_ptr<Backend> make_coreaudio_backend(std::string& why) {
    why = "Core Audio is only on macOS";
    return nullptr;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // defined(__APPLE__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX
