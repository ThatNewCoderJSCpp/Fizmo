#ifndef FIZMO_AUDIO_DEVICE_HPP
#define FIZMO_AUDIO_DEVICE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

enum class DeviceKind {
    output,
    input,
    loopback
};

const char* device_kind_name(DeviceKind kind) noexcept;

struct DeviceInfo {
    std::string id;
    std::string name;
    DeviceKind kind = DeviceKind::output;
    bool is_default = false;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::string backend;
};

std::vector<std::string> available_backends();
std::string backend_name();
bool set_backend(const std::string& name, std::string* error = nullptr);

std::vector<DeviceInfo> devices();
std::vector<DeviceInfo> devices(DeviceKind kind);
std::optional<DeviceInfo> default_device(DeviceKind kind);

bool set_android_java_vm(void* java_vm, void* context = nullptr, std::string* error = nullptr);
bool set_android_media_projection(void* media_projection, std::string* error = nullptr);
bool has_android_media_projection();

enum class AndroidPermissionState {
    granted,
    denied,
    pending
};

const char* android_permission_state_name(AndroidPermissionState state) noexcept;
bool android_permission_granted(const char* permission);
bool request_android_permission(const char* permission, std::string* error = nullptr);
AndroidPermissionState android_permission_state(const char* permission);

struct StreamConfig {
    DeviceKind kind = DeviceKind::output;
    std::string device;
    std::uint32_t sample_rate = 0;
    std::uint32_t channels = 0;
    std::uint32_t latency_ms = 20;
    std::string name = "fizmo";
    bool pin_device = false;
};

using PlaybackCallback = std::function<void(float* output, std::size_t frames)>;
using CaptureCallback = std::function<void(const float* input, std::size_t frames)>;

namespace detail {
class BackendStream;
struct StreamShared;
} // namespace detail

class Stream {
public:
    Stream();
    ~Stream();
    Stream(Stream&& other) noexcept;
    Stream& operator=(Stream&& other) noexcept;
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    bool open_playback(const StreamConfig& config, PlaybackCallback callback, std::string* error = nullptr);
    bool open_capture(const StreamConfig& config, CaptureCallback callback, std::string* error = nullptr);
    bool start(std::string* error = nullptr);
    void stop();
    void close();

    bool is_open() const noexcept;
    bool running() const noexcept;
    bool failed() const noexcept;
    std::string error() const;

    DeviceKind kind() const noexcept;
    std::uint32_t sample_rate() const noexcept;
    std::uint32_t channels() const noexcept;
    std::uint32_t latency_frames() const noexcept;
    std::string device_name() const;
    std::uint64_t frames_processed() const noexcept;
    double seconds_processed() const noexcept;

private:
    bool open(const StreamConfig& config, PlaybackCallback playback, CaptureCallback capture, std::string* error);

    std::shared_ptr<detail::StreamShared> m_shared;
    std::unique_ptr<detail::BackendStream> m_impl;
    bool m_running = false;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_DEVICE_HPP
