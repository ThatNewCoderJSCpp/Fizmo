#ifndef FIZMO_AUDIO_TIMELINE_HPP
#define FIZMO_AUDIO_TIMELINE_HPP

#include "buffer.hpp"
#include "device.hpp"
#include "effect.hpp"
#include "format.hpp"
#include "resampler.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fizmo {
namespace audio {

enum class FadeShape {
    linear,
    equal_power,
    exponential,
    logarithmic,
    s_curve
};

float fade_gain(FadeShape shape, double t) noexcept;

struct Clip {
    std::shared_ptr<const AudioBuffer> source;
    double position = 0.0;
    double source_start = 0.0;
    double length = 0.0;
    float gain_db = 0.0f;
    double fade_in = 0.0;
    double fade_out = 0.0;
    FadeShape fade_in_shape = FadeShape::linear;
    FadeShape fade_out_shape = FadeShape::linear;
    bool muted = false;
    std::string name;

    double end() const noexcept { return position + length; }
    bool contains(double time) const noexcept { return time >= position && time < end(); }
    double source_duration() const noexcept;
    bool split(double time, Clip& right);
    void trim_start(double time);
    void trim_end(double time);
    void move_to(double time) noexcept { position = time; }
    Clip duplicate(double at) const;
    float gain_at(double time) const noexcept;
};

Clip make_clip(std::shared_ptr<const AudioBuffer> source, double position = 0.0, std::string name = std::string());
Clip make_clip(const AudioBuffer& source, double position = 0.0, std::string name = std::string());

enum class Curve {
    linear,
    hold,
    exponential,
    s_curve,
    bezier
};

struct AutomationPoint {
    double time = 0.0;
    float value = 0.0f;
    Curve curve = Curve::linear;
    float tension = 0.0f;
};

enum class AutomationTarget {
    volume,
    pan,
    mute,
    effect_parameter
};

class AutomationLane {
public:
    AutomationLane() = default;
    explicit AutomationLane(AutomationTarget target, std::size_t effect_index = 0, std::string parameter = std::string());
    AutomationLane(const AutomationLane& other);
    AutomationLane& operator=(const AutomationLane& other);

    AutomationTarget target = AutomationTarget::volume;
    std::size_t effect_index = 0;
    std::string parameter;
    float default_value = 0.0f;
    bool enabled = true;

    std::size_t add(double time, float value, Curve curve = Curve::linear, float tension = 0.0f);
    std::size_t add(const AutomationPoint& point);
    bool remove(std::size_t index);
    std::size_t move(std::size_t index, double time, float value);
    bool set_curve(std::size_t index, Curve curve, float tension = 0.0f);
    void remove_range(double start, double end);
    void clear();
    std::size_t size() const noexcept { return m_points.size(); }
    bool empty() const noexcept { return m_points.empty(); }
    const std::vector<AutomationPoint>& points() const noexcept { return m_points; }
    const AutomationPoint& point(std::size_t index) const { return m_points[index]; }

    float value_at(double time) const noexcept;
    void render(double start_time, double sample_rate, float* out, std::size_t frames) const noexcept;
    void render_frames(std::int64_t first_frame, double sample_rate, float* out, std::size_t frames) const noexcept;
    bool active() const noexcept { return enabled && !m_points.empty(); }

private:
    std::size_t segment(double time) const noexcept;
    float evaluate(std::size_t index, double time) const noexcept;

    std::vector<AutomationPoint> m_points;
    mutable std::atomic<std::size_t> m_hint{ 0 };
};

float automation_shape(Curve curve, float tension, double t) noexcept;

class Track {
public:
    explicit Track(std::string name = std::string(), std::size_t channels = 0);

    std::string name;
    std::vector<Clip> clips;
    float volume_db = 0.0f;
    float pan = 0.0f;
    bool mute = false;
    bool solo = false;
    std::size_t channels = 0;
    std::shared_ptr<EffectChain> effects;
    std::vector<AutomationLane> lanes;

    std::size_t add_clip(Clip clip);
    bool remove_clip(std::size_t index);
    bool split_clip(std::size_t index, double time);
    std::vector<std::size_t> clips_at(double time) const;
    AutomationLane& lane(AutomationTarget target, std::size_t effect_index = 0, const std::string& parameter = std::string());
    const AutomationLane* find_lane(AutomationTarget target, std::size_t effect_index = 0, const std::string& parameter = std::string()) const;
    bool remove_lane(AutomationTarget target, std::size_t effect_index = 0, const std::string& parameter = std::string());
    double length() const noexcept;
};

struct Marker {
    double time = 0.0;
    std::string name;
};

class ResampleCache {
public:
    std::shared_ptr<const AudioBuffer> get(const std::shared_ptr<const AudioBuffer>& source, std::uint32_t rate, ResampleQuality quality = ResampleQuality::high);
    void clear();
    std::size_t size() const;

private:
    struct Entry {
        std::weak_ptr<const AudioBuffer> source;
        const AudioBuffer* key = nullptr;
        std::uint32_t rate = 0;
        ResampleQuality quality = ResampleQuality::high;
        std::shared_ptr<const AudioBuffer> result;
    };

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

class Timeline {
public:
    explicit Timeline(std::uint32_t sample_rate = 48000, std::size_t channels = 2);

    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t channels() const noexcept { return m_channels; }
    void set_format(std::uint32_t sample_rate, std::size_t channels);

    std::vector<Track> tracks;
    std::vector<Marker> markers;
    double tempo = 120.0;
    int beats_per_bar = 4;
    int beat_unit = 4;
    double loop_start = 0.0;
    double loop_end = 0.0;
    bool loop_enabled = false;
    float master_volume_db = 0.0f;
    std::shared_ptr<EffectChain> master_effects;
    std::vector<AutomationLane> master_lanes;

    Track& add_track(std::string name = std::string(), std::size_t channels = 0);
    bool remove_track(std::size_t index);
    std::size_t add_marker(double time, std::string name = std::string());
    bool remove_marker(std::size_t index);
    AutomationLane& master_lane(AutomationTarget target, std::size_t effect_index = 0, const std::string& parameter = std::string());

    double beats_to_seconds(double beats) const noexcept;
    double seconds_to_beats(double seconds) const noexcept;
    double bars_to_seconds(double bars) const noexcept;
    double seconds_to_bars(double seconds) const noexcept;
    double beat_length() const noexcept;
    double bar_length() const noexcept;
    double snap(double seconds, double division_beats = 1.0) const noexcept;

    double length() const noexcept;
    bool any_solo() const noexcept;

    const std::shared_ptr<ResampleCache>& cache() const noexcept { return m_cache; }
    void set_cache(std::shared_ptr<ResampleCache> cache);

private:
    std::uint32_t m_rate;
    std::size_t m_channels;
    std::shared_ptr<ResampleCache> m_cache;
};

struct RenderOptions {
    std::uint32_t sample_rate = 0;
    std::size_t channels = 0;
    bool include_tail = true;
    double max_tail = 30.0;
    ResampleQuality quality = ResampleQuality::high;
    std::function<bool(double)> progress;
};

AudioBuffer render(const Timeline& timeline, double start, double end, const RenderOptions& options = RenderOptions());
AudioBuffer render(const Timeline& timeline, const RenderOptions& options = RenderOptions());
bool render_stream(const Timeline& timeline, double start, double end, const RenderOptions& options, const std::function<bool(const float*, std::size_t)>& sink);
bool export_wav(const Timeline& timeline, const std::string& path, SampleFormat format = SampleFormat::s16, std::string* error = nullptr, const RenderOptions& options = RenderOptions(), bool dither = true);
bool export_audio(const Timeline& timeline, const std::string& path, const std::function<bool(const AudioBuffer&, const std::string&)>& writer, const RenderOptions& options = RenderOptions());

namespace detail {
struct TimelineSnapshot;
class TimelineEngine;
} // namespace detail

class TimelinePlayer {
public:
    enum class State {
        stopped,
        playing,
        paused
    };

    explicit TimelinePlayer(std::uint32_t sample_rate = 48000, std::size_t channels = 2);
    ~TimelinePlayer();
    TimelinePlayer(const TimelinePlayer&) = delete;
    TimelinePlayer& operator=(const TimelinePlayer&) = delete;

    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t channels() const noexcept { return m_channels; }

    void update(const Timeline& timeline);
    void set_quality(ResampleQuality quality) noexcept { m_quality = quality; }

    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void seek_frame(std::int64_t frame);
    void set_loop(double start, double end, bool enabled = true);
    void set_looping(bool enabled);
    bool looping() const noexcept { return m_loop_on.load(); }
    double loop_start() const noexcept;
    double loop_end() const noexcept;

    State state() const noexcept { return static_cast<State>(m_state.load()); }
    bool is_playing() const noexcept { return state() == State::playing; }
    double position() const noexcept;
    std::int64_t position_frame() const noexcept;
    float output_peak() const noexcept { return m_peak.load(); }
    std::uint64_t snapshots_applied() const noexcept { return m_swaps.load(); }

    void render(float* output, std::size_t frames) noexcept;

    bool start(const StreamConfig& config = StreamConfig(), std::string* error = nullptr);
    void stop_output();
    bool output_running() const noexcept { return m_stream.running(); }
    const Stream& stream() const noexcept { return m_stream; }

private:
    void collect();

    std::uint32_t m_rate;
    std::size_t m_channels;
    ResampleQuality m_quality = ResampleQuality::high;
    std::shared_ptr<ResampleCache> m_cache;

    std::mutex m_mutex;
    std::shared_ptr<const detail::TimelineSnapshot> m_pending;
    std::shared_ptr<const detail::TimelineSnapshot> m_live;
    std::vector<std::shared_ptr<const detail::TimelineSnapshot>> m_retired;
    std::atomic<bool> m_dirty{ false };

    std::unique_ptr<detail::TimelineEngine> m_engine;
    std::atomic<int> m_state{ 0 };
    std::atomic<std::int64_t> m_seek{ -1 };
    std::atomic<std::int64_t> m_position{ 0 };
    std::atomic<std::int64_t> m_origin{ 0 };
    std::atomic<std::int64_t> m_loop_a{ 0 };
    std::atomic<std::int64_t> m_loop_b{ 0 };
    std::atomic<bool> m_loop_on{ false };
    std::atomic<std::uint64_t> m_loop_version{ 0 };
    std::atomic<float> m_peak{ 0.0f };
    std::atomic<std::uint64_t> m_swaps{ 0 };
    bool m_restart = true;
    std::uint64_t m_seen_loop = 0;

    Stream m_stream;
};

class TrackRecorder {
public:
    TrackRecorder();
    ~TrackRecorder();
    TrackRecorder(const TrackRecorder&) = delete;
    TrackRecorder& operator=(const TrackRecorder&) = delete;

    bool start(const StreamConfig& config, const TimelinePlayer* player = nullptr, double position = 0.0, std::string* error = nullptr);
    bool begin(std::uint32_t sample_rate, std::size_t channels, double position, const TimelinePlayer* player = nullptr);
    void push(const float* interleaved, std::size_t frames) noexcept;
    Clip finish(const std::string& name = "take");
    void cancel();

    void set_latency_offset(double seconds) noexcept { m_offset.store(seconds); }
    double latency_offset() const noexcept { return m_offset.load(); }
    void set_compensate_stream_latency(bool on) noexcept { m_compensate.store(on); }

    bool recording() const noexcept { return m_recording.load(); }
    double recorded_seconds() const noexcept;
    std::uint64_t dropped_frames() const noexcept { return m_dropped.load(); }
    double start_position() const noexcept;
    float input_peak() const noexcept { return m_peak.load(); }

private:
    void drain();
    void writer_loop();

    std::uint32_t m_rate = 0;
    std::size_t m_channels = 0;
    std::vector<float> m_ring;
    std::atomic<std::size_t> m_head{ 0 };
    std::atomic<std::size_t> m_tail{ 0 };
    std::vector<float> m_data;
    std::mutex m_data_mutex;
    std::atomic<bool> m_recording{ false };
    std::atomic<bool> m_run{ false };
    std::atomic<bool> m_first{ true };
    std::atomic<double> m_position{ 0.0 };
    std::atomic<double> m_offset{ 0.0 };
    std::atomic<bool> m_compensate{ true };
    std::atomic<std::uint64_t> m_dropped{ 0 };
    std::atomic<std::uint64_t> m_frames{ 0 };
    std::atomic<float> m_peak{ 0.0f };
    const TimelinePlayer* m_player = nullptr;
    double m_fixed_position = 0.0;
    std::thread m_writer;
    Stream m_stream;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_TIMELINE_HPP
