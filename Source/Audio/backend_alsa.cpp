#include "backend.hpp"
#include "format.hpp"

#if defined(__linux__) && !defined(__ANDROID__)

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <utility>

#if defined(__has_include)
#if __has_include(<alsa/asoundlib.h>)
#define FIZMO_AUDIO_HAVE_ALSA_HEADERS 1
#include <alsa/asoundlib.h>
#endif
#endif

namespace fizmo {
namespace audio {
namespace detail {

namespace {

namespace alsa {

struct pcm;

constexpr int stream_playback = 0;
constexpr int stream_capture = 1;
constexpr int format_s16_le = 2;
constexpr int format_float_le = 14;
constexpr int access_rw_interleaved = 3;

} // namespace alsa

#if defined(FIZMO_AUDIO_HAVE_ALSA_HEADERS)
static_assert(SND_PCM_STREAM_PLAYBACK == alsa::stream_playback && SND_PCM_STREAM_CAPTURE == alsa::stream_capture, "ALSA stream values");
static_assert(SND_PCM_FORMAT_S16_LE == alsa::format_s16_le && SND_PCM_FORMAT_FLOAT_LE == alsa::format_float_le, "ALSA format values");
static_assert(SND_PCM_ACCESS_RW_INTERLEAVED == alsa::access_rw_interleaved, "ALSA access value");
static_assert(sizeof(snd_pcm_uframes_t) == sizeof(unsigned long) && sizeof(snd_pcm_sframes_t) == sizeof(long), "ALSA frame count types");
#endif

void quiet_alsa_errors(const char*, int, const char*, int, const char*, ...) {}

struct AlsaApi {
    DynamicLibrary lib;
    int (*pcm_open)(alsa::pcm**, const char*, int, int) = nullptr;
    int (*pcm_set_params)(alsa::pcm*, int, int, unsigned, unsigned, int, unsigned) = nullptr;
    int (*pcm_get_params)(alsa::pcm*, unsigned long*, unsigned long*) = nullptr;
    long (*pcm_writei)(alsa::pcm*, const void*, unsigned long) = nullptr;
    long (*pcm_readi)(alsa::pcm*, void*, unsigned long) = nullptr;
    int (*pcm_recover)(alsa::pcm*, int, int) = nullptr;
    int (*pcm_close)(alsa::pcm*) = nullptr;
    int (*pcm_drop)(alsa::pcm*) = nullptr;
    int (*pcm_prepare)(alsa::pcm*) = nullptr;
    const char* (*strerror)(int) = nullptr;
    int (*name_hint)(int, const char*, void***) = nullptr;
    char* (*name_get_hint)(const void*, const char*) = nullptr;
    int (*name_free_hint)(void**) = nullptr;
    int (*set_error_handler)(void (*)(const char*, int, const char*, int, const char*, ...)) = nullptr;

    bool load(std::string& why) {
        if (!lib.open({ "libasound.so.2", "libasound.so" })) {
            why = "libasound (ALSA) isn't installed";
            return false;
        }
        bool ok = true;
        ok &= lib.load(pcm_open, "snd_pcm_open");
        ok &= lib.load(pcm_set_params, "snd_pcm_set_params");
        ok &= lib.load(pcm_get_params, "snd_pcm_get_params");
        ok &= lib.load(pcm_writei, "snd_pcm_writei");
        ok &= lib.load(pcm_readi, "snd_pcm_readi");
        ok &= lib.load(pcm_recover, "snd_pcm_recover");
        ok &= lib.load(pcm_close, "snd_pcm_close");
        ok &= lib.load(pcm_drop, "snd_pcm_drop");
        ok &= lib.load(pcm_prepare, "snd_pcm_prepare");
        ok &= lib.load(strerror, "snd_strerror");
        ok &= lib.load(name_hint, "snd_device_name_hint");
        ok &= lib.load(name_get_hint, "snd_device_name_get_hint");
        ok &= lib.load(name_free_hint, "snd_device_name_free_hint");
        if (!ok) why = "the installed libasound is missing functions fizmo needs";
        if (ok && lib.load(set_error_handler, "snd_lib_error_set_handler")) set_error_handler(quiet_alsa_errors);
        return ok;
    }

    std::string error_text(int code) const {
        const char* s = strerror ? strerror(code) : nullptr;
        return s ? std::string(s) : "ALSA error " + std::to_string(code);
    }

    std::string hint(const void* h, const char* id) const {
        char* v = name_get_hint(h, id);
        if (!v) return std::string();
        std::string out(v);
        std::free(v);
        return out;
    }
};

bool starts_with(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

bool keep_device(const std::string& name) {
    if (name == "default" || name == "pulse" || name == "pipewire" || name == "jack") return true;
    return starts_with(name, "sysdefault:") || starts_with(name, "plughw:") || starts_with(name, "hdmi:") || starts_with(name, "iec958:");
}

std::string clean_description(std::string d) {
    for (char& c : d) {
        if (c == '\n' || c == '\r') c = '|';
    }
    std::string out;
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (d[i] == '|') out += " - ";
        else out += d[i];
    }
    return out;
}

class AlsaStream : public BackendStream {
public:
    AlsaStream(std::shared_ptr<const AlsaApi> api, std::shared_ptr<StreamShared> shared, alsa::pcm* handle, bool float_samples, std::uint32_t period, std::uint32_t buffer, std::string device)
        : m_api(std::move(api)), m_shared(std::move(shared)), m_handle(handle), m_float(float_samples), m_period(period), m_buffer(buffer), m_device(std::move(device)) {}

    ~AlsaStream() override {
        stop();
        if (m_handle) m_api->pcm_close(m_handle);
    }

    bool start(std::string& error) override {
        if (m_thread.joinable()) return true;
        const int r = m_api->pcm_prepare(m_handle);
        if (r < 0) {
            error = "can't start the ALSA device: " + m_api->error_text(r);
            return false;
        }
        m_run.store(true);
        m_thread = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        m_run.store(false);
        if (m_thread.joinable()) m_thread.join();
        if (m_handle) m_api->pcm_drop(m_handle);
    }

    std::uint32_t latency_frames() const override { return m_buffer; }
    std::string device_name() const override { return m_device; }

private:
    void loop() {
        const std::size_t channels = m_shared->channels;
        std::vector<float> buf(static_cast<std::size_t>(m_period) * channels, 0.0f);
        std::vector<std::int16_t> shorts(m_float ? 0 : buf.size());
        while (m_run.load(std::memory_order_relaxed)) {
            if (m_shared->kind == DeviceKind::output) {
                m_shared->render(buf.data(), m_period);
                const void* data = buf.data();
                if (!m_float) {
                    from_float(buf.data(), shorts.data(), SampleFormat::s16, buf.size(), true);
                    data = shorts.data();
                }
                const std::size_t frame_bytes = channels * (m_float ? 4 : 2);
                std::size_t done = 0;
                while (done < m_period && m_run.load(std::memory_order_relaxed)) {
                    const long r = m_api->pcm_writei(m_handle, static_cast<const std::uint8_t*>(data) + done * frame_bytes, m_period - done);
                    if (r >= 0) {
                        done += static_cast<std::size_t>(r);
                        continue;
                    }
                    if (r == -EAGAIN) continue;
                    if (m_api->pcm_recover(m_handle, static_cast<int>(r), 1) < 0) {
                        m_shared->fail("the ALSA device stopped: " + m_api->error_text(static_cast<int>(r)));
                        return;
                    }
                }
            } else {
                void* data = m_float ? static_cast<void*>(buf.data()) : static_cast<void*>(shorts.data());
                const std::size_t frame_bytes = channels * (m_float ? 4 : 2);
                std::size_t done = 0;
                while (done < m_period && m_run.load(std::memory_order_relaxed)) {
                    const long r = m_api->pcm_readi(m_handle, static_cast<std::uint8_t*>(data) + done * frame_bytes, m_period - done);
                    if (r >= 0) {
                        done += static_cast<std::size_t>(r);
                        continue;
                    }
                    if (r == -EAGAIN) continue;
                    if (m_api->pcm_recover(m_handle, static_cast<int>(r), 1) < 0) {
                        m_shared->fail("the ALSA device stopped: " + m_api->error_text(static_cast<int>(r)));
                        return;
                    }
                }
                if (done == 0) continue;
                if (!m_float) to_float(shorts.data(), SampleFormat::s16, buf.data(), done * channels);
                m_shared->deliver(buf.data(), done);
            }
        }
    }

    std::shared_ptr<const AlsaApi> m_api;
    std::shared_ptr<StreamShared> m_shared;
    alsa::pcm* m_handle = nullptr;
    bool m_float = true;
    std::uint32_t m_period = 0;
    std::uint32_t m_buffer = 0;
    std::string m_device;
    std::atomic<bool> m_run{ false };
    std::thread m_thread;
};

class AlsaBackend : public Backend {
public:
    bool init(std::string& why) {
        m_api = std::make_shared<AlsaApi>();
        return m_api->load(why);
    }

    const char* name() const override { return "alsa"; }

    std::vector<DeviceInfo> devices() override {
        std::vector<DeviceInfo> out;
        void** hints = nullptr;
        if (m_api->name_hint(-1, "pcm", &hints) < 0 || !hints) return out;
        for (void** h = hints; *h; ++h) {
            const std::string name = m_api->hint(*h, "NAME");
            if (name.empty() || !keep_device(name)) continue;
            const std::string io = m_api->hint(*h, "IOID");
            std::string desc = clean_description(m_api->hint(*h, "DESC"));
            if (desc.empty()) desc = name;
            for (DeviceKind k : { DeviceKind::output, DeviceKind::input }) {
                if (k == DeviceKind::output && io == "Input") continue;
                if (k == DeviceKind::input && io == "Output") continue;
                DeviceInfo d;
                d.id = name;
                d.name = desc;
                d.kind = k;
                d.is_default = name == "default";
                d.sample_rate = 48000;
                d.channels = 2;
                d.backend = "alsa";
                out.push_back(d);
            }
        }
        m_api->name_free_hint(hints);
        return out;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) override {
        if (request.kind == DeviceKind::loopback) {
            error = "ALSA can't record what other programs play; that needs PulseAudio or PipeWire";
            return nullptr;
        }
        const std::string device = request.device.empty() ? std::string("default") : request.device;
        const std::uint32_t rate = request.sample_rate ? request.sample_rate : 48000;
        const std::uint32_t channels = request.channels ? request.channels : 2;
        const int dir = request.kind == DeviceKind::output ? alsa::stream_playback : alsa::stream_capture;
        alsa::pcm* handle = nullptr;
        int r = m_api->pcm_open(&handle, device.c_str(), dir, 0);
        if (r < 0) {
            error = "can't open ALSA device '" + device + "': " + m_api->error_text(r);
            return nullptr;
        }
        const unsigned latency_us = request.latency_ms * 1000u;
        bool use_float = true;
        r = m_api->pcm_set_params(handle, alsa::format_float_le, alsa::access_rw_interleaved, channels, rate, 1, latency_us);
        if (r < 0) {
            use_float = false;
            r = m_api->pcm_set_params(handle, alsa::format_s16_le, alsa::access_rw_interleaved, channels, rate, 1, latency_us);
        }
        if (r < 0) {
            error = "ALSA device '" + device + "' won't take " + std::to_string(channels) + " channels at " + std::to_string(rate) + " Hz: " + m_api->error_text(r);
            m_api->pcm_close(handle);
            return nullptr;
        }
        unsigned long buffer = 0;
        unsigned long period = 0;
        if (m_api->pcm_get_params(handle, &buffer, &period) < 0 || period == 0) {
            buffer = rate * request.latency_ms / 1000;
            period = std::max<unsigned long>(32, buffer / 4);
        }
        shared->sample_rate = rate;
        shared->channels = channels;
        return std::make_unique<AlsaStream>(m_api, std::move(shared), handle, use_float, static_cast<std::uint32_t>(period), static_cast<std::uint32_t>(buffer), device);
    }

private:
    std::shared_ptr<AlsaApi> m_api;
};

} // namespace

std::unique_ptr<Backend> make_alsa_backend(std::string& why) {
    auto b = std::make_unique<AlsaBackend>();
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

std::unique_ptr<Backend> make_alsa_backend(std::string& why) {
    why = "ALSA is only used on Linux";
    return nullptr;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // defined(__linux__) && !defined(__ANDROID__)
