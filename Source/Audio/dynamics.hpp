#ifndef FIZMO_AUDIO_DYNAMICS_HPP
#define FIZMO_AUDIO_DYNAMICS_HPP

#include "effect.hpp"
#include "filters.hpp"
#include "loudness.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace audio {

enum class Detection {
    peak,
    rms
};

class Compressor : public Effect {
public:
    static constexpr float max_lookahead_ms = 20.0f;

    Compressor(float threshold_db = -18.0f, float ratio = 4.0f, float attack_ms = 10.0f, float release_ms = 100.0f);
    const char* name() const noexcept override { return "compressor"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override { return m_lookahead; }

    float gain_reduction_db() const noexcept { return m_meter.load(std::memory_order_relaxed); }
    float static_gain_db(float input_db) const noexcept;
    float makeup_db() const noexcept;
    void set_detection(Detection d) noexcept { detection.set(d == Detection::rms ? 1.0f : 0.0f); }

    Parameter threshold;
    Parameter ratio;
    Parameter knee;
    Parameter attack;
    Parameter release;
    Parameter makeup;
    Parameter auto_makeup;
    Parameter detection;
    Parameter link;
    Parameter lookahead;
    Parameter sidechain_hpf;

private:
    std::vector<double> m_ms;
    std::vector<double> m_env;
    std::vector<double> m_det;
    std::vector<float> m_delay;
    std::size_t m_delay_frames = 1;
    std::size_t m_write = 0;
    std::size_t m_lookahead = 0;
    Biquad m_sidechain;
    float m_hpf_frequency = -1.0f;
    std::atomic<float> m_meter{ 0.0f };
};

class Limiter : public Effect {
public:
    static constexpr float max_lookahead_ms = 20.0f;

    Limiter(float ceiling_db = -1.0f, float release_ms = 50.0f, float lookahead_ms = 5.0f, bool true_peak = false);
    const char* name() const noexcept override { return "limiter"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t latency_frames() const noexcept override { return m_latency; }

    float gain_reduction_db() const noexcept { return m_meter.load(std::memory_order_relaxed); }

    Parameter ceiling;
    Parameter release;
    Parameter lookahead;
    Parameter true_peak;

private:
    void configure() noexcept;

    TruePeakDetector m_tp;
    bool m_use_tp = false;
    std::size_t m_window = 1;
    std::size_t m_detector_delay = 0;
    std::size_t m_latency = 0;
    std::size_t m_max_window = 1;
    std::vector<float> m_delay;
    std::size_t m_delay_frames = 1;
    std::size_t m_write = 0;
    std::vector<float> m_queue_value;
    std::vector<std::uint64_t> m_queue_index;
    std::size_t m_queue_head = 0;
    std::size_t m_queue_size = 0;
    std::vector<double> m_box;
    std::size_t m_box_pos = 0;
    double m_box_sum = 0.0;
    double m_env = 1.0;
    float m_prev_required = 1.0f;
    std::uint64_t m_counter = 0;
    std::atomic<float> m_meter{ 0.0f };
};

class NoiseGate : public Effect {
public:
    NoiseGate(float threshold_db = -40.0f, float hysteresis_db = 6.0f, float attack_ms = 1.0f, float hold_ms = 50.0f, float release_ms = 100.0f, float range_db = -80.0f);
    const char* name() const noexcept override { return "noise_gate"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    bool is_open() const noexcept { return m_open_flag.load(std::memory_order_relaxed); }
    float gain_reduction_db() const noexcept { return m_meter.load(std::memory_order_relaxed); }

    Parameter threshold;
    Parameter hysteresis;
    Parameter attack;
    Parameter hold;
    Parameter release;
    Parameter range;

private:
    double m_env = 0.0;
    double m_gain = 0.0;
    bool m_open = false;
    std::size_t m_hold_left = 0;
    std::atomic<bool> m_open_flag{ false };
    std::atomic<float> m_meter{ 0.0f };
};

class Expander : public Effect {
public:
    Expander(float threshold_db = -40.0f, float ratio = 2.0f, float attack_ms = 1.0f, float release_ms = 100.0f, float range_db = -60.0f);
    const char* name() const noexcept override { return "expander"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    float gain_reduction_db() const noexcept { return m_meter.load(std::memory_order_relaxed); }
    float static_gain_db(float input_db) const noexcept;

    Parameter threshold;
    Parameter ratio;
    Parameter attack;
    Parameter release;
    Parameter range;

private:
    double m_env = 0.0;
    double m_gr = 0.0;
    std::atomic<float> m_meter{ 0.0f };
};

class DeEsser : public Effect {
public:
    DeEsser(float frequency_hz = 6000.0f, float threshold_db = -30.0f, float amount_db = 6.0f);
    const char* name() const noexcept override { return "de_esser"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;

    float gain_reduction_db() const noexcept { return m_meter.load(std::memory_order_relaxed); }

    Parameter frequency;
    Parameter threshold;
    Parameter amount;

private:
    void design() noexcept;

    Biquad m_low1;
    Biquad m_low2;
    Biquad m_high1;
    Biquad m_high2;
    std::vector<double> m_high;
    std::vector<double> m_low;
    float m_frequency = -1.0f;
    double m_env = 0.0;
    double m_gr = 0.0;
    std::atomic<float> m_meter{ 0.0f };
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_DYNAMICS_HPP
