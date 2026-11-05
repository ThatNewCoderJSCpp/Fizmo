#ifndef FIZMO_AUDIO_LOUDNESS_HPP
#define FIZMO_AUDIO_LOUDNESS_HPP

#include "buffer.hpp"
#include "fft.hpp"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace audio {

class TruePeakDetector {
public:
    static constexpr std::size_t oversampling = 4;
    static constexpr std::size_t taps_per_phase = 16;

    TruePeakDetector() = default;
    explicit TruePeakDetector(std::size_t channels);

    void prepare(std::size_t channels);
    void reset() noexcept;
    std::size_t channels() const noexcept { return m_channels; }
    std::size_t latency_frames() const noexcept { return taps_per_phase / 2; }

    float push_frame(const float* frame) noexcept;
    float push_frame(const float* frame, float* per_channel) noexcept;
    float process(const float* interleaved, std::size_t frames) noexcept;
    float flush() noexcept;

private:
    std::size_t m_channels = 0;
    std::vector<float> m_history;
    std::size_t m_pos = 0;
    std::vector<float> m_zero;
};

struct LoudnessResult {
    float integrated = 0.0f;
    float range = 0.0f;
    float true_peak_db = 0.0f;
    float sample_peak_db = 0.0f;
    float max_momentary = 0.0f;
    float max_short_term = 0.0f;
    float momentary = 0.0f;
    float short_term = 0.0f;
};

class LoudnessMeter {
public:
    LoudnessMeter();
    LoudnessMeter(std::uint32_t sample_rate, std::size_t channels);
    LoudnessMeter(const LoudnessMeter&) = delete;
    LoudnessMeter& operator=(const LoudnessMeter&) = delete;

    void prepare(std::uint32_t sample_rate, std::size_t channels);
    void reset() noexcept;
    void push(const float* interleaved, std::size_t frames) noexcept;
    bool push(const AudioBuffer& buffer) noexcept;
    void finish() noexcept;

    void set_channel_weight(std::size_t channel, float weight) noexcept;
    float channel_weight(std::size_t channel) const noexcept;
    void set_true_peak_enabled(bool on) noexcept { m_true_peak_on = on; }

    std::uint32_t sample_rate() const noexcept { return m_rate; }
    std::size_t channels() const noexcept { return m_channels; }

    float momentary() const noexcept { return m_momentary.load(std::memory_order_relaxed); }
    float short_term() const noexcept { return m_short_term.load(std::memory_order_relaxed); }
    float integrated() const noexcept { return m_integrated.load(std::memory_order_relaxed); }
    float loudness_range() const noexcept { return m_range.load(std::memory_order_relaxed); }
    float true_peak_db() const noexcept;
    float sample_peak_db() const noexcept;
    float max_momentary() const noexcept { return m_max_momentary.load(std::memory_order_relaxed); }
    float max_short_term() const noexcept { return m_max_short_term.load(std::memory_order_relaxed); }
    LoudnessResult result() const noexcept;

    static constexpr std::size_t histogram_bins = 10000;
    static constexpr double histogram_min = -70.0;
    static constexpr double histogram_step = 0.01;

private:
    struct Hist {
        std::vector<std::uint64_t> count;
        std::vector<double> energy;
        std::uint64_t total = 0;
        double total_energy = 0.0;
        void clear() noexcept;
        void add(double loudness, double energy) noexcept;
    };

    void end_subblock() noexcept;
    void compute_integrated() noexcept;
    void compute_range() noexcept;

    std::uint32_t m_rate = 0;
    std::size_t m_channels = 0;
    std::vector<double> m_weights;
    std::vector<double> m_state;
    double m_pb[3] = { 1.0, 0.0, 0.0 };
    double m_pa[3] = { 1.0, 0.0, 0.0 };
    double m_rb[3] = { 1.0, 0.0, 0.0 };
    double m_ra[3] = { 1.0, 0.0, 0.0 };
    std::uint64_t m_frames = 0;
    std::uint64_t m_next_boundary = 0;
    std::uint64_t m_subblocks = 0;
    double m_acc = 0.0;
    std::uint64_t m_acc_frames = 0;
    double m_ring_energy[30] = {};
    std::uint64_t m_ring_frames[30] = {};
    Hist m_gate;
    Hist m_lra;
    TruePeakDetector m_tp;
    bool m_true_peak_on = true;
    float m_tp_max = 0.0f;
    float m_sp_max = 0.0f;
    std::atomic<float> m_momentary;
    std::atomic<float> m_short_term;
    std::atomic<float> m_integrated;
    std::atomic<float> m_range;
    std::atomic<float> m_true_peak;
    std::atomic<float> m_sample_peak;
    std::atomic<float> m_max_momentary;
    std::atomic<float> m_max_short_term;
};

LoudnessResult measure_loudness(const AudioBuffer& buffer);
void normalize_loudness(AudioBuffer& buffer, float target_lufs = -14.0f, float max_true_peak_db = -1.0f);
float peak_db(const AudioBuffer& buffer);
float true_peak_db(const AudioBuffer& buffer);
void normalize_peak(AudioBuffer& buffer, float target_db);

class SpectrumAnalyzer {
public:
    static constexpr std::size_t min_fft_size = 512;
    static constexpr std::size_t max_fft_size = 16384;

    explicit SpectrumAnalyzer(std::size_t fft_size = 4096, WindowType window = WindowType::hann);
    SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
    SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;

    void prepare(std::uint32_t sample_rate, std::size_t channels);
    void push(const float* interleaved, std::size_t frames) noexcept;
    void clear() noexcept;

    void set_fft_size(std::size_t size);
    void set_window(WindowType window);
    void set_smoothing(float amount) noexcept;
    void set_peak_hold(float hold_seconds, float decay_db_per_second) noexcept;

    bool update();

    std::size_t fft_size() const noexcept { return m_size; }
    std::size_t bins() const noexcept { return m_size / 2 + 1; }
    std::uint32_t sample_rate() const noexcept { return m_rate; }
    float bin_frequency(std::size_t bin) const noexcept;
    const std::vector<float>& magnitudes_db() const noexcept { return m_db; }
    const std::vector<float>& peaks_db() const noexcept { return m_peak_db; }
    std::vector<float> bands(std::size_t count, float min_hz = 20.0f, float max_hz = 20000.0f, bool peaks = false) const;
    void bands(float* out, std::size_t count, float min_hz = 20.0f, float max_hz = 20000.0f, bool peaks = false) const;
    std::vector<float> band_frequencies(std::size_t count, float min_hz = 20.0f, float max_hz = 20000.0f) const;

private:
    void rebuild();

    std::uint32_t m_rate = 48000;
    std::size_t m_channels = 1;
    std::size_t m_size = 4096;
    WindowType m_window_type = WindowType::hann;
    std::unique_ptr<std::atomic<float>[]> m_ring;
    std::size_t m_ring_size = 0;
    std::atomic<std::uint64_t> m_written;
    std::uint64_t m_last_written = 0;
    Fft m_fft;
    std::vector<float> m_window;
    float m_window_scale = 1.0f;
    std::vector<float> m_frame;
    std::vector<std::complex<float>> m_spectrum;
    std::vector<float> m_power;
    std::vector<float> m_db;
    std::vector<float> m_peak_db;
    std::vector<float> m_peak_age;
    float m_smoothing = 0.5f;
    float m_hold = 1.0f;
    float m_decay = 20.0f;
    bool m_fresh = true;
};

class LevelMeter {
public:
    LevelMeter() = default;
    LevelMeter(const LevelMeter&) = delete;
    LevelMeter& operator=(const LevelMeter&) = delete;

    void prepare(std::uint32_t sample_rate, std::size_t channels);
    void reset() noexcept;
    void push(const float* interleaved, std::size_t frames) noexcept;

    void set_rms_window(float seconds) noexcept { m_rms_seconds = seconds; }
    void set_peak_decay(float db_per_second) noexcept { m_decay_db = db_per_second; }
    void set_hold_time(float seconds) noexcept { m_hold_seconds = seconds; }

    std::size_t channels() const noexcept { return m_channels; }
    float peak(std::size_t channel) const noexcept;
    float rms(std::size_t channel) const noexcept;
    float hold(std::size_t channel) const noexcept;
    float peak_db(std::size_t channel) const noexcept;
    float rms_db(std::size_t channel) const noexcept;
    float hold_db(std::size_t channel) const noexcept;
    bool clipped(std::size_t channel) const noexcept;
    void clear_clip() noexcept;

private:
    struct State {
        double mean_square = 0.0;
        float peak = 0.0f;
        float hold = 0.0f;
        double hold_age = 0.0;
    };

    std::uint32_t m_rate = 48000;
    std::size_t m_channels = 0;
    float m_rms_seconds = 0.3f;
    float m_decay_db = 20.0f;
    float m_hold_seconds = 1.5f;
    std::vector<State> m_state;
    std::unique_ptr<std::atomic<float>[]> m_peak;
    std::unique_ptr<std::atomic<float>[]> m_rms;
    std::unique_ptr<std::atomic<float>[]> m_hold;
    std::unique_ptr<std::atomic<bool>[]> m_clip;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_LOUDNESS_HPP
