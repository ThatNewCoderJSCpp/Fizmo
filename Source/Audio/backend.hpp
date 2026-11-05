#ifndef FIZMO_AUDIO_BACKEND_HPP
#define FIZMO_AUDIO_BACKEND_HPP

#include "device.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {
namespace detail {

struct StreamShared {
    PlaybackCallback playback;
    CaptureCallback capture;
    DeviceKind kind = DeviceKind::output;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::atomic<std::uint64_t> frames{ 0 };
    std::atomic<bool> failed{ false };
    std::atomic<bool> stopping{ false };
    mutable std::mutex error_mutex;
    std::string error;

    void fail(const std::string& message);
    std::string last_error() const;
    void render(float* output, std::size_t frame_count) noexcept;
    void deliver(const float* input, std::size_t frame_count) noexcept;
};

struct StreamRequest {
    DeviceKind kind = DeviceKind::output;
    std::string device;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::uint32_t latency_ms = 20;
    std::string name;
};

class BackendStream {
public:
    virtual ~BackendStream() = default;
    virtual bool start(std::string& error) = 0;
    virtual void stop() = 0;
    virtual std::uint32_t latency_frames() const = 0;
    virtual std::string device_name() const = 0;
};

class Backend {
public:
    virtual ~Backend() = default;
    virtual const char* name() const = 0;
    virtual std::vector<DeviceInfo> devices() = 0;
    virtual std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) = 0;
};

std::unique_ptr<Backend> make_null_backend();
std::unique_ptr<Backend> make_pulse_backend(std::string& why);
std::unique_ptr<Backend> make_alsa_backend(std::string& why);
std::unique_ptr<Backend> make_wasapi_backend(std::string& why);
std::unique_ptr<Backend> make_coreaudio_backend(std::string& why);
std::unique_ptr<Backend> make_aaudio_backend(std::string& why);

class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    bool open(std::initializer_list<const char*> names);
    void close();
    bool is_open() const noexcept { return m_handle != nullptr; }
    void* symbol(const char* name) const;

    template <typename T>
    bool load(T& out, const char* name) const {
        void* p = symbol(name);
        out = reinterpret_cast<T>(p);
        return p != nullptr;
    }

private:
    void* m_handle = nullptr;
};

void sleep_for_ms(double milliseconds);

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_BACKEND_HPP
