#ifndef FIZMO_VIDEO_PLAYER_HPP
#define FIZMO_VIDEO_PLAYER_HPP

#include "decoder.hpp"
#include "deinterlace.hpp"
#include "demux.hpp"
#include "frame.hpp"

#include "../Audio/device.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace audio {
class Mixer;
} // namespace audio
} // namespace fizmo

namespace fizmo {
namespace video {

struct MediaInfo {
    bool valid = false;
    std::string error;
    std::string container;
    double duration = 0.0;
    std::vector<TrackInfo> tracks;
    std::vector<std::pair<std::string, std::string>> tags;
    int video_track = -1;
    int audio_track = -1;
    bool video_supported = false;
    bool audio_supported = false;
    std::string video_codec;
    std::string audio_codec;
    std::string video_error;
    std::string audio_error;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t display_width = 0;
    std::uint32_t display_height = 0;
    double display_aspect = 0.0;
    int rotation = 0;
    double frame_rate = 0.0;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
};

MediaInfo probe_media(const std::string& path);
MediaInfo probe_media(const void* data, std::size_t size);

bool extract_frame(const std::string& path, double seconds, VideoFrame& out, std::string* error = nullptr);
bool extract_frame(const void* data, std::size_t size, double seconds, VideoFrame& out, std::string* error = nullptr);

enum class PlayerState {
    closed,
    paused,
    playing,
    ended,
    error
};

const char* player_state_name(PlayerState state) noexcept;

enum class PlayerEventType {
    loaded,
    first_frame,
    play,
    pause,
    playing,
    waiting,
    seeking,
    seeked,
    ended,
    looped,
    rate_changed,
    volume_changed,
    track_changed,
    warning,
    error
};

const char* player_event_name(PlayerEventType type) noexcept;

struct PlayerEvent {
    PlayerEventType type = PlayerEventType::loaded;
    double position = 0.0;
    std::string message;
};

struct PlayerStats {
    std::uint64_t frames_decoded = 0;
    std::uint64_t frames_presented = 0;
    std::uint64_t frames_dropped = 0;
    std::uint64_t frames_skipped = 0;
    std::uint64_t packets_skipped = 0;
    std::uint64_t decode_errors = 0;
    std::uint64_t audio_underruns = 0;
    std::size_t queued_packets = 0;
    std::size_t queued_frames = 0;
    double audio_buffered = 0.0;
    double average_decode_ms = 0.0;
    double average_convert_ms = 0.0;
    bool audio_clock = false;
    std::uint64_t frames_deinterlaced = 0;
    bool deinterlacing = false;
};

struct PlayerOptions {
    bool enable_video = true;
    bool enable_audio = true;
    bool rgba = false;
    bool autoplay = false;
    bool loop = false;
    bool preserve_pitch = true;
    float volume = 1.0f;
    bool muted = false;
    double rate = 1.0;
    std::size_t max_queued_frames = 0;
    std::size_t max_queued_packet_bytes = 16u << 20;
    double presentation_offset = 0.0;
    double audio_buffer_seconds = 1.0;
    std::uint32_t output_sample_rate = 48000;
    std::uint32_t output_channels = 2;
    audio::StreamConfig output;
    DeinterlaceSetting deinterlace = DeinterlaceSetting::automatic;
    DeinterlaceMode deinterlace_mode = DeinterlaceMode::yadif;
    bool deinterlace_field_rate = false;
};

class MediaPlayer {
public:
    using EventCallback = std::function<void(const PlayerEvent&)>;

    struct Impl;

    MediaPlayer();
    explicit MediaPlayer(const PlayerOptions& options);
    explicit MediaPlayer(audio::Mixer& mixer, const PlayerOptions& options = PlayerOptions());
    ~MediaPlayer();
    MediaPlayer(const MediaPlayer&) = delete;
    MediaPlayer& operator=(const MediaPlayer&) = delete;

    void set_options(const PlayerOptions& options);
    PlayerOptions options() const;
    void attach(audio::Mixer& mixer);
    void detach();
    audio::Mixer* mixer() const noexcept;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    bool open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const MediaInfo& info() const noexcept;
    const std::vector<TrackInfo>& tracks() const noexcept;
    double duration() const noexcept;
    bool has_video() const noexcept;
    bool has_audio() const noexcept;
    std::uint32_t width() const noexcept;
    std::uint32_t height() const noexcept;
    std::uint32_t display_width() const noexcept;
    std::uint32_t display_height() const noexcept;
    double display_aspect() const noexcept;
    int rotation() const noexcept;
    double frame_rate() const noexcept;
    int video_track() const noexcept;
    int audio_track() const noexcept;
    bool select_video_track(int index, std::string* error = nullptr);
    bool select_audio_track(int index, std::string* error = nullptr);
    std::string video_error() const;
    std::string audio_error() const;

    void play();
    void pause();
    void stop();
    bool seek(double seconds, bool accurate = true);
    double position() const;
    PlayerState state() const noexcept;
    bool is_playing() const noexcept;
    bool is_paused() const noexcept;
    bool is_seeking() const noexcept;
    bool is_buffering() const noexcept;
    bool ended() const noexcept;
    std::string error() const;

    void set_loop(bool loop);
    bool loop() const noexcept;
    bool set_rate(double rate);
    double rate() const noexcept;
    void set_preserve_pitch(bool preserve);
    bool preserve_pitch() const noexcept;
    void set_volume(float volume);
    float volume() const noexcept;
    void set_muted(bool muted);
    bool muted() const noexcept;
    void set_deinterlace(DeinterlaceSetting setting);
    void set_deinterlace_mode(DeinterlaceMode mode, bool field_rate = false);
    DeinterlaceSetting deinterlace() const noexcept;
    DeinterlaceMode deinterlace_mode() const noexcept;
    bool deinterlace_field_rate() const noexcept;
    bool is_deinterlacing() const noexcept;

    std::vector<PlayerEvent> poll_events();
    void set_event_callback(EventCallback callback);

    bool current_frame(const VideoFrame*& frame, std::uint64_t& serial);
    bool current_rgba(const std::uint8_t*& rgba, std::uint32_t& width, std::uint32_t& height, std::uint64_t& serial);
    bool frame_rgba(std::vector<std::uint8_t>& rgba, std::uint32_t& width, std::uint32_t& height, std::uint64_t& serial);
    std::uint64_t frame_serial() const noexcept;
    double frame_time() const noexcept;
    double next_frame_delay() const;
    PlayerStats stats() const;

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_PLAYER_HPP
