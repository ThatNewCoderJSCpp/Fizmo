#ifndef FIZMO_AUDIO_MIXER_HPP
#define FIZMO_AUDIO_MIXER_HPP

#include "buffer.hpp"
#include "codec.hpp"
#include "device.hpp"
#include "stretch.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace audio {

using Sound = std::shared_ptr<const AudioBuffer>;

Sound make_sound(AudioBuffer buffer);

struct PlayOptions {
    float gain = 1.0f;
    float pan = 0.0f;
    float speed = 1.0f;
    bool loop = false;
    bool paused = false;
    double start = 0.0;
    double fade_in = 0.0;
    bool preserve_pitch = false;
    float pitch = 0.0f;
    bool stretchable = false;
    StretchMode stretch_mode = StretchMode::automatic;
    StretchQuality stretch_quality = StretchQuality::high;
    double stream_buffer_seconds = 2.0;
};

class Mixer {
public:
    using VoiceId = std::uint64_t;

    explicit Mixer(std::uint32_t sample_rate = 48000, std::size_t channels = 2);
    ~Mixer();
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;

    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t channels() const noexcept { return m_channels; }

    VoiceId play(Sound sound, const PlayOptions& options = PlayOptions());
    VoiceId play(const AudioBuffer& buffer, const PlayOptions& options = PlayOptions());
    VoiceId play_file(const std::string& path, const PlayOptions& options = PlayOptions(), std::string* error = nullptr);
    VoiceId play_stream(std::unique_ptr<AudioReader> reader, const PlayOptions& options = PlayOptions(), std::string* error = nullptr);
    void stop(VoiceId id, double fade_out = 0.0);
    void stop_all(double fade_out = 0.0);
    void set_gain(VoiceId id, float gain);
    void set_pan(VoiceId id, float pan);
    void set_speed(VoiceId id, float speed);
    void set_loop(VoiceId id, bool loop);
    void set_paused(VoiceId id, bool paused);
    bool seek(VoiceId id, double seconds);
    bool set_pitch(VoiceId id, float semitones);
    bool set_preserve_pitch(VoiceId id, bool preserve);

    bool is_playing(VoiceId id) const;
    bool is_paused(VoiceId id) const;
    double position(VoiceId id) const;
    double playback_position(VoiceId id) const;
    double duration(VoiceId id) const;
    std::size_t voice_count() const;
    bool is_streaming(VoiceId id) const;
    bool is_stretched(VoiceId id) const;
    float pitch(VoiceId id) const;
    std::uint64_t stream_underruns(VoiceId id) const;
    double buffered_seconds(VoiceId id) const;

    void set_master_gain(float gain) noexcept { m_master.store(gain); }
    float master_gain() const noexcept { return m_master.load(); }
    float output_peak() const noexcept { return m_peak.load(); }

    void render(float* output, std::size_t frames) noexcept;
    void set_output_latency(double seconds) noexcept { m_latency_override.store(seconds); }
    double output_latency() const noexcept;

    bool start(const StreamConfig& config = StreamConfig(), std::string* error = nullptr);
    void stop_output();
    bool output_running() const noexcept { return m_stream.running(); }
    const Stream& stream() const noexcept { return m_stream; }

private:
    struct Voice;
    struct Streamer;

    std::shared_ptr<Voice> find(VoiceId id) const;
    void collect() const;
    void render_voice(Voice& v, float* out, std::size_t frames) noexcept;
    void render_path_voice(Voice& v, float* out, std::size_t frames) noexcept;
    void publish_clock(Voice& v, double before, double after) noexcept;
    VoiceId add_voice(std::shared_ptr<Voice> v);

    std::uint32_t m_rate;
    std::size_t m_channels;
    std::atomic<float> m_master{ 1.0f };
    std::atomic<float> m_peak{ 0.0f };
    std::atomic<double> m_latency_override{ -1.0 };
    std::atomic<double> m_stream_latency{ 0.0 };
    std::int64_t m_render_ns = 0;
    double m_block_seconds = 0.0;
    std::atomic<VoiceId> m_next{ 1 };

    mutable std::mutex m_mutex;
    mutable std::unordered_map<VoiceId, std::shared_ptr<Voice>> m_voices;
    mutable std::vector<std::shared_ptr<Voice>> m_pending;
    mutable std::vector<std::shared_ptr<Voice>> m_finished;
    std::vector<std::shared_ptr<Voice>> m_active;
    std::vector<float> m_frame_in;
    std::vector<float> m_frame_out;

    Stream m_stream;
    std::unique_ptr<Streamer> m_streamer;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_MIXER_HPP
