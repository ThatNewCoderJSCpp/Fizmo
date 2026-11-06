#include "backend.hpp"
#include "format.hpp"
#include "resampler.hpp"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propidl.h>

#include <algorithm>
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

const CLSID k_clsid_enumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
const IID k_iid_enumerator = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
const IID k_iid_audio_client = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
const IID k_iid_render_client = { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
const IID k_iid_capture_client = { 0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
const IID k_iid_notification = { 0x7991EEC9, 0x7E89, 0x4D85, { 0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0 } };
const IID k_iid_unknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const GUID k_subtype_float = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
const GUID k_subtype_pcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
const PROPERTYKEY k_friendly_name = { { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } }, 14 };

constexpr DWORD k_flag_loopback = 0x00020000;
constexpr DWORD k_flag_event = 0x00040000;
constexpr DWORD k_flag_autoconvert = 0x80000000;
constexpr DWORD k_flag_src_quality = 0x08000000;
constexpr DWORD k_buffer_silent = 0x2;
constexpr HRESULT k_device_invalidated = static_cast<HRESULT>(0x88890004L);
constexpr HRESULT k_not_initialized = static_cast<HRESULT>(0x88890001L);

template <typename T>
class Com {
public:
    Com() = default;
    ~Com() { reset(); }
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    T* get() const noexcept { return m_p; }
    T* operator->() const noexcept { return m_p; }
    T** put() noexcept {
        reset();
        return &m_p;
    }
    void** put_void() noexcept { return reinterpret_cast<void**>(put()); }
    void reset() noexcept {
        if (m_p) {
            m_p->Release();
            m_p = nullptr;
        }
    }
    explicit operator bool() const noexcept { return m_p != nullptr; }

private:
    T* m_p = nullptr;
};

std::string narrow(const wchar_t* w) {
    if (!w || !*w) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string hresult_text(HRESULT hr) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%08lX", static_cast<unsigned long>(hr));
    std::string name;
    switch (static_cast<unsigned long>(hr)) {
        case 0x88890004UL: name = "the device was removed or disabled"; break;
        case 0x8889000AUL: name = "the device is in use by another program in exclusive mode"; break;
        case 0x88890008UL: name = "the device doesn't support that format"; break;
        case 0x88890003UL: name = "the device can't be used this way (recording system audio may be unsupported here)"; break;
        case 0x88890002UL: name = "the stream is already initialized"; break;
        case 0x80070005UL: name = "access denied (check the microphone privacy settings)"; break;
        case 0x80070490UL: name = "the device wasn't found"; break;
        case 0x80070057UL: name = "invalid argument"; break;
        default: break;
    }
    return name.empty() ? std::string("error ") + hex : name + " (" + hex + ")";
}

class ComScope {
public:
    ComScope() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_ok = SUCCEEDED(hr);
        m_usable = m_ok || hr == RPC_E_CHANGED_MODE;
    }
    ~ComScope() {
        if (m_ok) CoUninitialize();
    }
    bool usable() const noexcept { return m_usable; }

private:
    bool m_ok = false;
    bool m_usable = false;
};

std::string friendly_name(IMMDevice* device) {
    Com<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, store.put()))) return std::string();
    PROPVARIANT v;
    PropVariantInit(&v);
    std::string name;
    if (SUCCEEDED(store->GetValue(k_friendly_name, &v)) && v.vt == VT_LPWSTR) name = narrow(v.pwszVal);
    PropVariantClear(&v);
    return name;
}

std::string device_id(IMMDevice* device) {
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || !id) return std::string();
    std::string s = narrow(id);
    CoTaskMemFree(id);
    return s;
}

bool mix_format(IMMDevice* device, std::uint32_t& rate, std::uint32_t& channels) {
    Com<IAudioClient> client;
    if (FAILED(device->Activate(k_iid_audio_client, CLSCTX_ALL, nullptr, client.put_void()))) return false;
    WAVEFORMATEX* fmt = nullptr;
    if (FAILED(client->GetMixFormat(&fmt)) || !fmt) return false;
    rate = fmt->nSamplesPerSec;
    channels = fmt->nChannels;
    CoTaskMemFree(fmt);
    return true;
}

bool sample_format_of(const WAVEFORMATEX* f, SampleFormat& out) {
    WORD tag = f->wFormatTag;
    WORD container = f->wBitsPerSample;
    WORD valid = f->wBitsPerSample;
    if (tag == WAVE_FORMAT_EXTENSIBLE && f->cbSize >= 22) {
        const WAVEFORMATEXTENSIBLE* x = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(f);
        valid = x->Samples.wValidBitsPerSample;
        if (IsEqualGUID(x->SubFormat, k_subtype_float)) tag = 3;
        else if (IsEqualGUID(x->SubFormat, k_subtype_pcm)) tag = 1;
        else return false;
    }
    (void)valid;
    if (tag == 3) {
        if (container == 32) { out = SampleFormat::f32; return true; }
        if (container == 64) { out = SampleFormat::f64; return true; }
        return false;
    }
    if (tag == 1) {
        switch (container) {
            case 8: out = SampleFormat::u8; return true;
            case 16: out = SampleFormat::s16; return true;
            case 24: out = SampleFormat::s24; return true;
            case 32: out = SampleFormat::s32; return true;
            default: return false;
        }
    }
    return false;
}

WAVEFORMATEXTENSIBLE float_format(std::uint32_t rate, std::uint32_t channels) {
    WAVEFORMATEXTENSIBLE x;
    std::memset(&x, 0, sizeof(x));
    x.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    x.Format.nChannels = static_cast<WORD>(channels);
    x.Format.nSamplesPerSec = rate;
    x.Format.wBitsPerSample = 32;
    x.Format.nBlockAlign = static_cast<WORD>(channels * 4);
    x.Format.nAvgBytesPerSec = rate * channels * 4;
    x.Format.cbSize = 22;
    x.Samples.wValidBitsPerSample = 32;
    x.dwChannelMask = 0;
    switch (channels) {
        case 1: x.dwChannelMask = 0x4; break;
        case 2: x.dwChannelMask = 0x3; break;
        case 4: x.dwChannelMask = 0x33; break;
        case 6: x.dwChannelMask = 0x3F; break;
        case 8: x.dwChannelMask = 0x63F; break;
        default: break;
    }
    x.SubFormat = k_subtype_float;
    return x;
}

class NotificationClient : public IMMNotificationClient {
public:
    NotificationClient(EDataFlow flow, HANDLE wake) : m_flow(flow), m_wake(wake) {}
    NotificationClient(EDataFlow flow, HANDLE wake, std::wstring watched) : m_flow(flow), m_wake(wake), m_watched(std::move(watched)) {}

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_refs)); }
    ULONG STDMETHODCALLTYPE Release() override {
        const LONG n = InterlockedDecrement(&m_refs);
        if (n == 0) delete this;
        return static_cast<ULONG>(n);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_POINTER;
        if (IsEqualIID(riid, k_iid_unknown) || IsEqualIID(riid, k_iid_notification)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD state) override {
        if (state != DEVICE_STATE_ACTIVE && watches(id)) mark_lost();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override {
        if (watches(id)) mark_lost();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == m_flow && role == eConsole) {
            changed.store(true);
            SetEvent(m_wake);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    std::atomic<bool> changed{ false };
    std::atomic<bool> lost{ false };

private:
    virtual ~NotificationClient() = default;

    bool watches(LPCWSTR id) const {
        return id && !m_watched.empty() && lstrcmpiW(id, m_watched.c_str()) == 0;
    }

    void mark_lost() {
        lost.store(true);
        SetEvent(m_wake);
    }

    LONG m_refs = 1;
    EDataFlow m_flow;
    HANDLE m_wake;
    std::wstring m_watched;
};

struct Avrt {
    DynamicLibrary lib;
    HANDLE (WINAPI* set)(LPCWSTR, LPDWORD) = nullptr;
    BOOL (WINAPI* revert)(HANDLE) = nullptr;
    Avrt() {
        if (lib.open({ "avrt.dll" })) {
            lib.load(set, "AvSetMmThreadCharacteristicsW");
            lib.load(revert, "AvRevertMmThreadCharacteristics");
        }
    }
};

struct OpenResult {
    bool done = false;
    bool ok = false;
    std::string error;
};

class WasapiStream : public BackendStream {
public:
    WasapiStream(const StreamRequest& request, std::shared_ptr<StreamShared> shared)
        : m_request(request), m_shared(std::move(shared)) {
        m_audio_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        m_wake_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        m_flow = request.kind == DeviceKind::input ? eCapture : eRender;
        m_loopback = request.kind == DeviceKind::loopback;
        m_follow_default = request.device.empty();
        m_pinned = request.pin_device && !request.device.empty();
    }

    ~WasapiStream() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_command = Command::quit;
        }
        m_cv.notify_all();
        SetEvent(m_wake_event);
        if (m_thread.joinable()) m_thread.join();
        if (m_audio_event) CloseHandle(m_audio_event);
        if (m_wake_event) CloseHandle(m_wake_event);
    }

    bool launch(std::string& error) {
        m_thread = std::thread([this] { thread_main(); });
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_open.done; });
        if (!m_open.ok) {
            error = m_open.error;
            return false;
        }
        return true;
    }

    bool start(std::string& error) override {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_state == State::running) return true;
        m_command = Command::start;
        m_cv.notify_all();
        m_cv.wait(lock, [this] { return m_state != State::idle || m_command != Command::start; });
        if (m_state != State::running) {
            error = m_start_error.empty() ? std::string("can't start the audio device") : m_start_error;
            return false;
        }
        return true;
    }

    void stop() override {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_state != State::running) return;
        m_command = Command::stop;
        SetEvent(m_wake_event);
        m_cv.notify_all();
        m_cv.wait(lock, [this] { return m_state != State::running; });
    }

    std::uint32_t latency_frames() const override { return m_latency.load(); }
    std::string device_name() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_device_name;
    }

private:
    enum class Command { none, start, stop, quit };
    enum class State { idle, running, finished };

    bool setup(std::string& error) {
        teardown();
        if (!m_enumerator) {
            HRESULT hr = CoCreateInstance(k_clsid_enumerator, nullptr, CLSCTX_ALL, k_iid_enumerator, m_enumerator.put_void());
            if (FAILED(hr)) {
                error = "can't reach the Windows audio service: " + hresult_text(hr);
                return false;
            }
            if (m_follow_default || m_pinned) {
                m_notify = m_pinned ? new NotificationClient(m_flow, m_wake_event, widen(m_request.device)) : new NotificationClient(m_flow, m_wake_event);
                if (FAILED(m_enumerator->RegisterEndpointNotificationCallback(m_notify))) {
                    m_notify->Release();
                    m_notify = nullptr;
                }
            }
        }
        if (m_notify) m_notify->changed.store(false);
        HRESULT hr;
        if (m_follow_default) {
            hr = m_enumerator->GetDefaultAudioEndpoint(m_flow, eConsole, m_device.put());
        } else {
            hr = m_enumerator->GetDevice(widen(m_request.device).c_str(), m_device.put());
        }
        if (FAILED(hr)) {
            error = (m_follow_default ? std::string("there's no default ") + (m_flow == eCapture ? "recording" : "playback") + " device" : "can't find device '" + m_request.device + "'") + ": " + hresult_text(hr);
            return false;
        }
        hr = m_device->Activate(k_iid_audio_client, CLSCTX_ALL, nullptr, m_client.put_void());
        if (FAILED(hr)) {
            error = "can't open the audio device: " + hresult_text(hr);
            return false;
        }
        WAVEFORMATEX* mix = nullptr;
        hr = m_client->GetMixFormat(&mix);
        if (FAILED(hr) || !mix) {
            error = "can't read the device's format: " + hresult_text(hr);
            return false;
        }
        const std::uint32_t rate = m_shared->sample_rate ? m_shared->sample_rate : (m_request.sample_rate ? m_request.sample_rate : mix->nSamplesPerSec);
        const std::uint32_t channels = m_shared->channels ? m_shared->channels : (m_request.channels ? m_request.channels : mix->nChannels);
        const REFERENCE_TIME duration = static_cast<REFERENCE_TIME>(m_request.latency_ms) * 10000;
        const DWORD base_flags = (m_loopback ? k_flag_loopback : k_flag_event);
        WAVEFORMATEXTENSIBLE want = float_format(rate, channels);
        const bool force_convert = std::getenv("FIZMO_AUDIO_WASAPI_CONVERT") != nullptr;
        hr = force_convert ? E_FAIL : m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, base_flags | k_flag_autoconvert | k_flag_src_quality, duration, 0, reinterpret_cast<WAVEFORMATEX*>(&want), nullptr);
        m_convert = false;
        if (FAILED(hr)) {
            m_client.reset();
            hr = m_device->Activate(k_iid_audio_client, CLSCTX_ALL, nullptr, m_client.put_void());
            if (SUCCEEDED(hr)) hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, base_flags, duration, 0, mix, nullptr);
            if (FAILED(hr)) {
                CoTaskMemFree(mix);
                error = "can't start the audio device: " + hresult_text(hr);
                return false;
            }
            if (!sample_format_of(mix, m_device_format)) {
                CoTaskMemFree(mix);
                error = "the audio device uses a sample format fizmo doesn't know";
                return false;
            }
            m_convert = true;
            m_device_rate = mix->nSamplesPerSec;
            m_device_channels = mix->nChannels;
        } else {
            m_device_format = SampleFormat::f32;
            m_device_rate = rate;
            m_device_channels = channels;
        }
        CoTaskMemFree(mix);
        m_shared->sample_rate = rate;
        m_shared->channels = channels;
        if (!m_loopback) {
            hr = m_client->SetEventHandle(m_audio_event);
            if (FAILED(hr)) {
                error = "can't set up the audio event: " + hresult_text(hr);
                return false;
            }
        }
        UINT32 frames = 0;
        m_client->GetBufferSize(&frames);
        m_buffer_frames = frames;
        m_latency.store(static_cast<std::uint32_t>(static_cast<std::uint64_t>(frames) * rate / std::max<std::uint32_t>(1, m_device_rate)));
        if (m_flow == eRender && !m_loopback) hr = m_client->GetService(k_iid_render_client, m_render.put_void());
        else hr = m_client->GetService(k_iid_capture_client, m_capture.put_void());
        if (FAILED(hr)) {
            error = "can't get the audio buffer service: " + hresult_text(hr);
            return false;
        }
        if (m_convert) {
            const bool output = m_flow == eRender && !m_loopback;
            if (output) m_resampler.configure(m_device_channels, rate, m_device_rate, ResampleQuality::medium);
            else m_resampler.configure(channels, m_device_rate, rate, ResampleQuality::medium);
            m_fifo.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_device_name = friendly_name(m_device.get());
            if (m_loopback) m_device_name += " (system audio)";
        }
        return true;
    }

    void teardown() {
        if (m_client) m_client->Stop();
        m_render.reset();
        m_capture.reset();
        m_client.reset();
        m_device.reset();
    }

    void release_enumerator() {
        if (m_enumerator && m_notify) m_enumerator->UnregisterEndpointNotificationCallback(m_notify);
        if (m_notify) {
            m_notify->Release();
            m_notify = nullptr;
        }
        m_enumerator.reset();
    }

    void thread_main() {
        ComScope com;
        std::string error;
        bool ok = com.usable();
        if (!ok) error = "can't start COM for audio";
        if (ok) ok = setup(error);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_open.done = true;
            m_open.ok = ok;
            m_open.error = error;
        }
        m_cv.notify_all();
        if (!ok) {
            teardown();
            release_enumerator();
            return;
        }
        Avrt avrt;
        for (;;) {
            Command cmd;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_command == Command::start || m_command == Command::quit; });
                cmd = m_command;
            }
            if (cmd == Command::quit) break;
            std::string why;
            if (!m_client && !setup(why)) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_start_error = why;
                m_command = Command::none;
                m_cv.notify_all();
                continue;
            }
            DWORD task = 0;
            HANDLE mmcss = avrt.set ? avrt.set(L"Pro Audio", &task) : nullptr;
            const bool started = begin(why);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (started) {
                    m_state = State::running;
                    m_start_error.clear();
                } else {
                    m_start_error = why;
                }
                m_command = Command::none;
            }
            m_cv.notify_all();
            if (started) run();
            if (mmcss && avrt.revert) avrt.revert(mmcss);
            if (m_client) {
                m_client->Stop();
                m_client->Reset();
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = State::idle;
            if (m_command == Command::stop) m_command = Command::none;
            m_cv.notify_all();
        }
        teardown();
        release_enumerator();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = State::finished;
        m_cv.notify_all();
    }

    bool begin(std::string& why) {
        if (m_render) fill_output(m_buffer_frames);
        const HRESULT hr = m_client->Start();
        if (FAILED(hr)) {
            why = "can't start the audio device: " + hresult_text(hr);
            return false;
        }
        return true;
    }

    bool should_stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_command == Command::stop || m_command == Command::quit;
    }

    std::string pinned_lost_message() const {
        std::string shown;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            shown = m_device_name;
        }
        return "the device '" + (shown.empty() ? m_request.device : shown) + "' was removed or disabled";
    }

    bool reroute() {
        std::string why;
        teardown();
        if (!setup(why) || !begin(why)) {
            m_shared->fail("lost the audio device and couldn't switch to the new default: " + why);
            return false;
        }
        return true;
    }

    void run() {
        HANDLE handles[2] = { m_audio_event, m_wake_event };
        const DWORD poll_ms = std::max<DWORD>(1, static_cast<DWORD>(m_request.latency_ms / 4));
        std::uint32_t quiet_ms = 0;
        for (;;) {
            if (m_loopback) {
                const DWORD w = WaitForSingleObject(m_wake_event, poll_ms);
                (void)w;
            } else {
                const DWORD w = WaitForMultipleObjects(2, handles, FALSE, 2000);
                if (w == WAIT_FAILED) {
                    m_shared->fail("waiting for the audio device failed");
                    return;
                }
            }
            if (should_stop()) return;
            if (m_pinned && m_notify && m_notify->lost.load()) {
                m_shared->fail(pinned_lost_message());
                return;
            }
            if (m_follow_default && m_notify && m_notify->changed.exchange(false)) {
                if (!reroute()) return;
                continue;
            }
            HRESULT hr = S_OK;
            if (m_render) {
                UINT32 padding = 0;
                hr = m_client->GetCurrentPadding(&padding);
                if (SUCCEEDED(hr) && padding < m_buffer_frames) hr = fill_output(m_buffer_frames - padding);
            } else {
                bool got = false;
                hr = drain_input(got);
                if (m_loopback && SUCCEEDED(hr)) {
                    if (got) {
                        quiet_ms = 0;
                    } else {
                        quiet_ms += poll_ms;
                        if (quiet_ms >= static_cast<std::uint32_t>(m_request.latency_ms)) {
                            deliver_silence(static_cast<std::size_t>(m_shared->sample_rate) * quiet_ms / 1000);
                            quiet_ms = 0;
                        }
                    }
                }
            }
            if (hr == k_device_invalidated || hr == k_not_initialized) {
                if (m_follow_default) {
                    if (!reroute()) return;
                    continue;
                }
                m_shared->fail(m_pinned ? pinned_lost_message() : std::string("the audio device was removed or disabled"));
                return;
            }
            if (FAILED(hr)) {
                m_shared->fail("the audio device stopped: " + hresult_text(hr));
                return;
            }
        }
    }

    HRESULT fill_output(UINT32 frames) {
        if (frames == 0) return S_OK;
        BYTE* data = nullptr;
        HRESULT hr = m_render->GetBuffer(frames, &data);
        if (FAILED(hr)) return hr;
        if (!m_convert) {
            m_shared->render(reinterpret_cast<float*>(data), frames);
        } else {
            const std::size_t dc = m_device_channels;
            const std::size_t uc = m_shared->channels;
            const std::size_t chunk = 256;
            while (m_fifo.size() < static_cast<std::size_t>(frames) * dc) {
                m_user.resize(chunk * std::max(uc, dc));
                m_shared->render(m_user.data(), chunk);
                remix(m_user.data(), uc, m_user.data(), dc, chunk);
                m_resampler.process(m_user.data(), chunk, m_fifo);
            }
            from_float(m_fifo.data(), data, m_device_format, static_cast<std::size_t>(frames) * dc, true);
            m_fifo.erase(m_fifo.begin(), m_fifo.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(frames) * dc));
        }
        return m_render->ReleaseBuffer(frames, 0);
    }

    void deliver_silence(std::size_t frames) {
        m_user.assign(frames * m_shared->channels, 0.0f);
        m_shared->deliver(m_user.data(), frames);
    }

    HRESULT drain_input(bool& got) {
        for (;;) {
            UINT32 packet = 0;
            HRESULT hr = m_capture->GetNextPacketSize(&packet);
            if (FAILED(hr)) return hr;
            if (packet == 0) return S_OK;
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            hr = m_capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if (FAILED(hr)) return hr;
            if (frames > 0) {
                got = true;
                const std::size_t dc = m_device_channels;
                const std::size_t uc = m_shared->channels;
                if (!m_convert) {
                    if (flags & k_buffer_silent) {
                        m_user.assign(static_cast<std::size_t>(frames) * uc, 0.0f);
                        m_shared->deliver(m_user.data(), frames);
                    } else {
                        m_shared->deliver(reinterpret_cast<const float*>(data), frames);
                    }
                } else {
                    m_raw.resize(static_cast<std::size_t>(frames) * std::max(dc, uc));
                    if (flags & k_buffer_silent) std::fill(m_raw.begin(), m_raw.end(), 0.0f);
                    else to_float(data, m_device_format, m_raw.data(), static_cast<std::size_t>(frames) * dc);
                    remix(m_raw.data(), dc, m_raw.data(), uc, frames);
                    m_fifo.clear();
                    const std::size_t n = m_resampler.process(m_raw.data(), frames, m_fifo);
                    if (n) m_shared->deliver(m_fifo.data(), n);
                }
            }
            hr = m_capture->ReleaseBuffer(frames);
            if (FAILED(hr)) return hr;
        }
    }

    StreamRequest m_request;
    std::shared_ptr<StreamShared> m_shared;
    EDataFlow m_flow = eRender;
    bool m_loopback = false;
    bool m_follow_default = true;
    bool m_pinned = false;
    HANDLE m_audio_event = nullptr;
    HANDLE m_wake_event = nullptr;
    Com<IMMDeviceEnumerator> m_enumerator;
    Com<IMMDevice> m_device;
    Com<IAudioClient> m_client;
    Com<IAudioRenderClient> m_render;
    Com<IAudioCaptureClient> m_capture;
    NotificationClient* m_notify = nullptr;
    UINT32 m_buffer_frames = 0;
    std::atomic<std::uint32_t> m_latency{ 0 };
    bool m_convert = false;
    SampleFormat m_device_format = SampleFormat::f32;
    std::uint32_t m_device_rate = 0;
    std::uint32_t m_device_channels = 0;
    Resampler m_resampler;
    std::vector<float> m_fifo;
    std::vector<float> m_user;
    std::vector<float> m_raw;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    OpenResult m_open;
    Command m_command = Command::none;
    State m_state = State::idle;
    std::string m_start_error;
    std::string m_device_name;
    std::thread m_thread;
};

void list_devices(std::vector<DeviceInfo>& out, std::string& error) {
    ComScope com;
    if (!com.usable()) {
        error = "can't start COM for audio";
        return;
    }
    Com<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(k_clsid_enumerator, nullptr, CLSCTX_ALL, k_iid_enumerator, enumerator.put_void());
    if (FAILED(hr)) {
        error = "can't reach the Windows audio service: " + hresult_text(hr);
        return;
    }
    for (EDataFlow flow : { eRender, eCapture }) {
        std::string default_id;
        {
            Com<IMMDevice> def;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, def.put()))) default_id = device_id(def.get());
        }
        Com<IMMDeviceCollection> collection;
        if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, collection.put()))) continue;
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            Com<IMMDevice> device;
            if (FAILED(collection->Item(i, device.put()))) continue;
            DeviceInfo d;
            d.id = device_id(device.get());
            d.name = friendly_name(device.get());
            if (d.name.empty()) d.name = d.id;
            d.is_default = !default_id.empty() && d.id == default_id;
            d.backend = "wasapi";
            mix_format(device.get(), d.sample_rate, d.channels);
            if (flow == eRender) {
                d.kind = DeviceKind::output;
                out.push_back(d);
                d.kind = DeviceKind::loopback;
                d.name += " (system audio)";
                out.push_back(d);
            } else {
                d.kind = DeviceKind::input;
                out.push_back(d);
            }
        }
    }
}

class WasapiBackend : public Backend {
public:
    bool init(std::string& why) {
        std::vector<DeviceInfo> list;
        std::thread t([&] { list_devices(list, why); });
        t.join();
        return why.empty();
    }

    const char* name() const override { return "wasapi"; }

    std::vector<DeviceInfo> devices() override {
        std::vector<DeviceInfo> list;
        std::string why;
        std::thread t([&] { list_devices(list, why); });
        t.join();
        return list;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) override {
        auto stream = std::make_unique<WasapiStream>(request, std::move(shared));
        if (!stream->launch(error)) return nullptr;
        return stream;
    }
};

} // namespace

std::unique_ptr<Backend> make_wasapi_backend(std::string& why) {
    auto b = std::make_unique<WasapiBackend>();
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

std::unique_ptr<Backend> make_wasapi_backend(std::string& why) {
    why = "WASAPI is only on Windows";
    return nullptr;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // defined(_WIN32)
