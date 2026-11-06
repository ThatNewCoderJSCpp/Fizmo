#include "backend.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fizmo {
namespace audio {

namespace detail {

void StreamShared::fail(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(error_mutex);
        if (error.empty()) error = message;
    }
    failed.store(true);
}

std::string StreamShared::last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return error;
}

void StreamShared::render(float* output, std::size_t frame_count) noexcept {
    const std::size_t samples = frame_count * channels;
    if (playback && !failed.load(std::memory_order_relaxed)) {
        try {
            playback(output, frame_count);
        } catch (const std::exception& e) {
            std::memset(output, 0, samples * sizeof(float));
            fail(std::string("the playback callback threw: ") + e.what());
        } catch (...) {
            std::memset(output, 0, samples * sizeof(float));
            fail("the playback callback threw");
        }
    } else {
        std::memset(output, 0, samples * sizeof(float));
    }
    frames.fetch_add(frame_count, std::memory_order_relaxed);
}

void StreamShared::deliver(const float* input, std::size_t frame_count) noexcept {
    if (capture && !failed.load(std::memory_order_relaxed)) {
        try {
            capture(input, frame_count);
        } catch (const std::exception& e) {
            fail(std::string("the capture callback threw: ") + e.what());
        } catch (...) {
            fail("the capture callback threw");
        }
    }
    frames.fetch_add(frame_count, std::memory_order_relaxed);
}

DynamicLibrary::~DynamicLibrary() {
    close();
}

bool DynamicLibrary::open(std::initializer_list<const char*> names) {
    close();
    for (const char* n : names) {
#if defined(_WIN32)
        m_handle = reinterpret_cast<void*>(LoadLibraryA(n));
#else
        m_handle = dlopen(n, RTLD_NOW | RTLD_LOCAL);
#endif
        if (m_handle) return true;
    }
    return false;
}

void DynamicLibrary::close() {
    if (!m_handle) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
    dlclose(m_handle);
#endif
    m_handle = nullptr;
}

void* DynamicLibrary::symbol(const char* name) const {
    if (!m_handle) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
#else
    return dlsym(m_handle, name);
#endif
}

void sleep_for_ms(double milliseconds) {
    if (milliseconds <= 0.0) return;
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(milliseconds * 1000.0)));
}

namespace {

class NullStream : public BackendStream {
public:
    NullStream(std::shared_ptr<StreamShared> shared, std::uint32_t latency_ms) : m_shared(std::move(shared)) {
        m_period = std::max<std::uint32_t>(1, m_shared->sample_rate * std::max<std::uint32_t>(latency_ms, 2) / 2000);
    }
    ~NullStream() override { stop(); }

    bool start(std::string&) override {
        if (m_thread.joinable()) return true;
        m_run.store(true);
        m_thread = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        m_run.store(false);
        if (m_thread.joinable()) m_thread.join();
    }

    std::uint32_t latency_frames() const override { return m_period * 2; }
    std::string device_name() const override { return "null"; }

private:
    void loop() {
        std::vector<float> buf(static_cast<std::size_t>(m_period) * m_shared->channels, 0.0f);
        const auto period = std::chrono::duration<double>(static_cast<double>(m_period) / static_cast<double>(m_shared->sample_rate));
        auto next = std::chrono::steady_clock::now();
        while (m_run.load()) {
            if (m_shared->kind == DeviceKind::output) {
                m_shared->render(buf.data(), m_period);
            } else {
                std::fill(buf.begin(), buf.end(), 0.0f);
                m_shared->deliver(buf.data(), m_period);
            }
            next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
            std::this_thread::sleep_until(next);
        }
    }

    std::shared_ptr<StreamShared> m_shared;
    std::uint32_t m_period = 480;
    std::atomic<bool> m_run{ false };
    std::thread m_thread;
};

class NullBackend : public Backend {
public:
    const char* name() const override { return "null"; }

    std::vector<DeviceInfo> devices() override {
        std::vector<DeviceInfo> out;
        for (DeviceKind k : { DeviceKind::output, DeviceKind::input, DeviceKind::loopback }) {
            DeviceInfo d;
            d.id = "null";
            d.name = "No device";
            d.kind = k;
            d.is_default = true;
            d.sample_rate = 48000;
            d.channels = k == DeviceKind::input ? 1 : 2;
            d.backend = "null";
            out.push_back(d);
        }
        return out;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string&) override {
        shared->sample_rate = request.sample_rate ? request.sample_rate : 48000;
        shared->channels = request.channels ? request.channels : (request.kind == DeviceKind::input ? 1 : 2);
        return std::make_unique<NullStream>(std::move(shared), request.latency_ms);
    }
};

struct Registry {
    std::mutex mutex;
    std::unique_ptr<Backend> backend;
    bool tried = false;
    std::string why;
};

Registry& registry() {
    static Registry r;
    return r;
}

std::vector<std::string> platform_order() {
#if defined(_WIN32)
    return { "wasapi" };
#elif defined(__ANDROID__)
    return { "aaudio" };
#elif defined(__APPLE__)
    return { "coreaudio" };
#elif defined(__linux__)
    return { "pulse", "alsa" };
#else
    return {};
#endif
}

std::unique_ptr<Backend> create_backend(const std::string& name, std::string& why) {
    if (name == "null") return make_null_backend();
    if (name == "pulse" || name == "pulseaudio" || name == "pipewire") return make_pulse_backend(why);
    if (name == "alsa") return make_alsa_backend(why);
    if (name == "wasapi") return make_wasapi_backend(why);
    if (name == "coreaudio" || name == "core-audio" || name == "core audio") return make_coreaudio_backend(why);
    if (name == "aaudio") return make_aaudio_backend(why);
    why = "there's no audio backend called '" + name + "'";
    return nullptr;
}

Backend* current(std::string* why) {
    Registry& r = registry();
    if (!r.tried) {
        r.tried = true;
        const char* forced = std::getenv("FIZMO_AUDIO_BACKEND");
        std::vector<std::string> order;
        if (forced && *forced) order.push_back(forced);
        else order = platform_order();
        std::string reasons;
        for (const std::string& n : order) {
            std::string w;
            r.backend = create_backend(n, w);
            if (r.backend) break;
            if (!reasons.empty()) reasons += "; ";
            reasons += n + ": " + w;
        }
        if (!r.backend) r.why = order.empty() ? std::string("audio devices aren't supported on this platform yet") : "no audio backend works (" + reasons + ")";
    }
    if (!r.backend && why) *why = r.why;
    return r.backend.get();
}

} // namespace

std::unique_ptr<Backend> make_null_backend() {
    return std::make_unique<NullBackend>();
}

} // namespace detail

const char* device_kind_name(DeviceKind kind) noexcept {
    switch (kind) {
        case DeviceKind::output: return "output";
        case DeviceKind::input: return "input";
        case DeviceKind::loopback: return "loopback";
    }
    return "unknown";
}

const char* android_permission_state_name(AndroidPermissionState state) noexcept {
    switch (state) {
        case AndroidPermissionState::granted: return "granted";
        case AndroidPermissionState::denied: return "denied";
        case AndroidPermissionState::pending: return "pending";
    }
    return "unknown";
}

std::vector<std::string> available_backends() {
    std::vector<std::string> out;
    for (const std::string& n : detail::platform_order()) {
        std::string why;
        if (detail::create_backend(n, why)) out.push_back(n);
    }
    out.push_back("null");
    return out;
}

std::string backend_name() {
    detail::Registry& r = detail::registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    detail::Backend* b = detail::current(nullptr);
    return b ? b->name() : std::string();
}

bool set_backend(const std::string& name, std::string* error) {
    detail::Registry& r = detail::registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (name.empty() || name == "auto") {
        r.backend.reset();
        r.tried = false;
        r.why.clear();
        if (!detail::current(error)) return false;
        return true;
    }
    std::string why;
    std::unique_ptr<detail::Backend> b = detail::create_backend(name, why);
    if (!b) {
        if (error) *error = why;
        return false;
    }
    r.backend = std::move(b);
    r.tried = true;
    r.why.clear();
    return true;
}

std::vector<DeviceInfo> devices() {
    detail::Registry& r = detail::registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    detail::Backend* b = detail::current(nullptr);
    return b ? b->devices() : std::vector<DeviceInfo>();
}

std::vector<DeviceInfo> devices(DeviceKind kind) {
    std::vector<DeviceInfo> all = devices();
    std::vector<DeviceInfo> out;
    for (DeviceInfo& d : all) {
        if (d.kind == kind) out.push_back(std::move(d));
    }
    return out;
}

std::optional<DeviceInfo> default_device(DeviceKind kind) {
    std::vector<DeviceInfo> list = devices(kind);
    for (DeviceInfo& d : list) {
        if (d.is_default) return std::move(d);
    }
    if (!list.empty()) return std::move(list.front());
    return std::nullopt;
}

Stream::Stream() = default;

Stream::~Stream() {
    close();
}

Stream::Stream(Stream&& other) noexcept
    : m_shared(std::move(other.m_shared)), m_impl(std::move(other.m_impl)), m_running(other.m_running) {
    other.m_running = false;
}

Stream& Stream::operator=(Stream&& other) noexcept {
    if (this != &other) {
        close();
        m_shared = std::move(other.m_shared);
        m_impl = std::move(other.m_impl);
        m_running = other.m_running;
        other.m_running = false;
    }
    return *this;
}

bool Stream::open_playback(const StreamConfig& config, PlaybackCallback callback, std::string* error) {
    StreamConfig c = config;
    if (c.kind != DeviceKind::output) c.kind = DeviceKind::output;
    return open(c, std::move(callback), nullptr, error);
}

bool Stream::open_capture(const StreamConfig& config, CaptureCallback callback, std::string* error) {
    StreamConfig c = config;
    if (c.kind == DeviceKind::output) c.kind = DeviceKind::input;
    return open(c, nullptr, std::move(callback), error);
}

bool Stream::open(const StreamConfig& config, PlaybackCallback playback, CaptureCallback capture, std::string* error) {
    close();
    std::string why;
    detail::Backend* backend = nullptr;
    {
        detail::Registry& r = detail::registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        backend = detail::current(&why);
    }
    if (!backend) {
        if (error) *error = why;
        return false;
    }
    if (config.channels > 64) {
        if (error) *error = "streams can have at most 64 channels";
        return false;
    }
    auto shared = std::make_shared<detail::StreamShared>();
    shared->playback = std::move(playback);
    shared->capture = std::move(capture);
    shared->kind = config.kind;
    detail::StreamRequest req;
    req.kind = config.kind;
    req.device = config.device;
    req.sample_rate = config.sample_rate;
    req.channels = config.channels;
    req.latency_ms = std::max<std::uint32_t>(2, std::min<std::uint32_t>(config.latency_ms ? config.latency_ms : 20, 2000));
    req.name = config.name.empty() ? std::string("fizmo") : config.name;
    req.pin_device = config.pin_device;
    std::unique_ptr<detail::BackendStream> impl = backend->open(req, shared, why);
    if (!impl) {
        if (error) *error = why.empty() ? std::string("can't open the audio device") : why;
        return false;
    }
    m_shared = std::move(shared);
    m_impl = std::move(impl);
    return true;
}

bool Stream::start(std::string* error) {
    if (!m_impl) {
        if (error) *error = "the stream isn't open";
        return false;
    }
    if (m_running) return true;
    std::string why;
    if (!m_impl->start(why)) {
        if (error) *error = why;
        m_shared->fail(why);
        return false;
    }
    m_running = true;
    return true;
}

void Stream::stop() {
    if (m_impl && m_running) m_impl->stop();
    m_running = false;
}

void Stream::close() {
    stop();
    m_impl.reset();
    m_shared.reset();
}

bool Stream::is_open() const noexcept {
    return m_impl != nullptr;
}

bool Stream::running() const noexcept {
    return m_running && m_shared && !m_shared->failed.load();
}

bool Stream::failed() const noexcept {
    return m_shared && m_shared->failed.load();
}

std::string Stream::error() const {
    return m_shared ? m_shared->last_error() : std::string();
}

DeviceKind Stream::kind() const noexcept {
    return m_shared ? m_shared->kind : DeviceKind::output;
}

std::uint32_t Stream::sample_rate() const noexcept {
    return m_shared ? m_shared->sample_rate : 0;
}

std::uint32_t Stream::channels() const noexcept {
    return m_shared ? m_shared->channels : 0;
}

std::uint32_t Stream::latency_frames() const noexcept {
    return m_impl ? m_impl->latency_frames() : 0;
}

std::string Stream::device_name() const {
    return m_impl ? m_impl->device_name() : std::string();
}

std::uint64_t Stream::frames_processed() const noexcept {
    return m_shared ? m_shared->frames.load() : 0;
}

double Stream::seconds_processed() const noexcept {
    const std::uint32_t rate = sample_rate();
    return rate ? static_cast<double>(frames_processed()) / static_cast<double>(rate) : 0.0;
}

} // namespace audio
} // namespace fizmo
