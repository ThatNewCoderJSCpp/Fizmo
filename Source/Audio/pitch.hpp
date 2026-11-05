#ifndef FIZMO_AUDIO_PITCH_HPP
#define FIZMO_AUDIO_PITCH_HPP

#include "effect.hpp"
#include "fft.hpp"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace audio {

struct PitchEstimate {
    float frequency = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

class PitchDetector {
public:
    PitchDetector() = default;
    explicit PitchDetector(float sample_rate, float min_hz = 60.0f, float max_hz = 1200.0f, float threshold = 0.15f);

    void configure(float sample_rate, float min_hz = 60.0f, float max_hz = 1200.0f, float threshold = 0.15f);
    void set_range(float min_hz, float max_hz) noexcept;
    void set_threshold(float threshold) noexcept;
    void set_hop(std::size_t frames) noexcept;
    void reset() noexcept;

    bool push_sample(float sample) noexcept;
    PitchEstimate push(const float* mono, std::size_t frames) noexcept;
    PitchEstimate analyze(const float* mono, std::size_t frames) noexcept;
    PitchEstimate latest() const noexcept { return m_latest; }

    float sample_rate() const noexcept { return m_rate; }
    float min_hz() const noexcept { return m_min_hz; }
    float max_hz() const noexcept { return m_max_hz; }
    float threshold() const noexcept { return m_threshold; }
    std::size_t window_frames() const noexcept { return m_span; }
    std::size_t hop_frames() const noexcept { return m_hop; }

private:
    PitchEstimate run(const float* window) noexcept;

    float m_rate = 0.0f;
    float m_config_min = 60.0f;
    float m_config_max = 1200.0f;
    float m_min_hz = 60.0f;
    float m_max_hz = 1200.0f;
    float m_threshold = 0.15f;
    std::size_t m_window = 0;
    std::size_t m_max_lag = 0;
    std::size_t m_span = 0;
    std::size_t m_hop = 1;
    std::size_t m_count = 0;
    std::size_t m_write = 0;
    Fft m_fft;
    std::vector<float> m_ring;
    std::vector<float> m_a;
    std::vector<float> m_b;
    std::vector<float> m_corr;
    std::vector<float> m_scratch;
    std::vector<double> m_prefix;
    std::vector<float> m_cmnd;
    std::vector<std::complex<float>> m_spec_a;
    std::vector<std::complex<float>> m_spec_b;
    PitchEstimate m_latest;
};

float detect_pitch(const float* mono, std::size_t frames, float sample_rate);
float detect_pitch(const float* mono, std::size_t frames, float sample_rate, float min_hz, float max_hz, float* confidence = nullptr);

class PitchShifter : public Effect {
public:
    explicit PitchShifter(float semitones = 0.0f, float mode = 0.0f);
    ~PitchShifter() override;

    const char* name() const noexcept override { return "pitch_shifter"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    float detected_hz() const noexcept;

    Parameter semitones;
    Parameter cents;
    Parameter mode;
    Parameter preserve_formants;
    Parameter formant;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

class FormantShifter : public Effect {
public:
    explicit FormantShifter(float semitones = 0.0f);
    ~FormantShifter() override;

    const char* name() const noexcept override { return "formant_shifter"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    Parameter shift;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

enum class MusicalScale {
    chromatic,
    major,
    minor,
    harmonic_minor,
    pentatonic_major,
    pentatonic_minor,
    blues,
    custom
};

unsigned scale_mask(MusicalScale scale) noexcept;

class AutoTune : public Effect {
public:
    AutoTune();
    ~AutoTune() override;

    const char* name() const noexcept override { return "autotune"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override;

    float detected_hz() const noexcept { return m_detected_hz.load(std::memory_order_relaxed); }
    float detected_note() const noexcept { return m_detected_note.load(std::memory_order_relaxed); }
    float target_note() const noexcept { return m_target_note.load(std::memory_order_relaxed); }
    float correction_cents() const noexcept { return m_correction.load(std::memory_order_relaxed); }
    bool voiced() const noexcept { return m_voiced.load(std::memory_order_relaxed); }

    Parameter key;
    Parameter scale;
    Parameter mask;
    Parameter retune_ms;
    Parameter humanize;
    Parameter preserve_formants;
    Parameter min_hz;
    Parameter max_hz;
    Parameter reference_hz;

private:
    struct State;
    friend struct State;
    std::unique_ptr<State> m_state;
    std::atomic<float> m_detected_hz{ 0.0f };
    std::atomic<float> m_detected_note{ 0.0f };
    std::atomic<float> m_target_note{ 0.0f };
    std::atomic<float> m_correction{ 0.0f };
    std::atomic<bool> m_voiced{ false };
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_PITCH_HPP
