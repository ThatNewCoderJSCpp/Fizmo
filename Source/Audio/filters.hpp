#ifndef FIZMO_AUDIO_FILTERS_HPP
#define FIZMO_AUDIO_FILTERS_HPP

#include "effect.hpp"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace audio {

enum class FilterType {
    lowpass,
    highpass,
    bandpass,
    notch,
    peak,
    lowshelf,
    highshelf,
    allpass
};

const char* filter_type_name(FilterType type) noexcept;

struct BiquadCoefficients {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;

    static BiquadCoefficients identity() noexcept { return BiquadCoefficients(); }
    bool is_identity() const noexcept;
    std::complex<double> response(double frequency, double sample_rate) const noexcept;
    double magnitude(double frequency, double sample_rate) const noexcept;
    double magnitude_db(double frequency, double sample_rate) const noexcept;
    double pole_radius() const noexcept;
};

BiquadCoefficients design_biquad(FilterType type, double sample_rate, double frequency, double q, double gain_db);
std::vector<double> butterworth_q(std::size_t stages);

class Biquad {
public:
    Biquad() = default;
    explicit Biquad(std::size_t channels);

    void prepare(std::size_t channels);
    std::size_t channels() const noexcept { return m_z1.size(); }

    void set_coefficients(const BiquadCoefficients& c) noexcept;
    void set_target(const BiquadCoefficients& c, std::size_t ramp_frames) noexcept;
    const BiquadCoefficients& coefficients() const noexcept { return m_c; }
    const BiquadCoefficients& target() const noexcept { return m_target; }
    bool ramping() const noexcept { return m_remaining > 0; }

    void reset() noexcept;
    void process(float* interleaved, std::size_t frames) noexcept;
    void process_frame(float* frame) noexcept;
    double process_sample(std::size_t channel, double x) noexcept;
    void advance() noexcept;

private:
    BiquadCoefficients m_c;
    BiquadCoefficients m_target;
    BiquadCoefficients m_step;
    std::size_t m_remaining = 0;
    std::vector<double> m_z1;
    std::vector<double> m_z2;
};

class SmoothedBiquad {
public:
    static constexpr std::size_t max_stages = 4;
    static constexpr std::size_t block_frames = 32;

    void prepare(double sample_rate, std::size_t channels);
    void set(FilterType type, double frequency, double q, double gain_db, std::size_t stages = 1, bool active = true) noexcept;
    void set_smoothing(double parameter_seconds, double coefficient_seconds) noexcept;
    void snap() noexcept;
    void reset() noexcept;
    void process(float* interleaved, std::size_t frames) noexcept;

    bool bypassed() const noexcept;
    std::size_t stage_count() const noexcept { return max_stages; }
    const BiquadCoefficients& stage(std::size_t index) const noexcept { return m_stages[index].coefficients(); }
    std::size_t tail_frames() const noexcept;

    static void design(FilterType type, double sample_rate, double frequency, double q, double gain_db, std::size_t stages, bool active, BiquadCoefficients* out) noexcept;

private:
    void update() noexcept;

    std::array<Biquad, max_stages> m_stages;
    std::array<BiquadCoefficients, max_stages> m_coef;
    double m_rate = 48000.0;
    std::size_t m_channels = 0;
    double m_param_seconds = 0.02;
    double m_coef_seconds = 0.004;
    double m_param_alpha = 1.0;
    double m_coef_alpha = 1.0;
    FilterType m_type = FilterType::lowpass;
    double m_target_freq = 1000.0;
    double m_target_q = 0.7071067811865476;
    double m_target_gain = 0.0;
    std::size_t m_target_stages = 1;
    bool m_active = true;
    double m_log_freq = 6.907755278982137;
    double m_log_q = -0.34657359027997264;
    double m_gain = 0.0;
    bool m_settled = false;
    std::size_t m_countdown = 0;
};

class Filter : public Effect {
public:
    explicit Filter(FilterType type = FilterType::lowpass, float frequency = 1000.0f, float q = 0.70710678f, float gain_db = 0.0f, float slope_db = 12.0f);
    const char* name() const noexcept override { return "filter"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;

    void set_type(FilterType t) noexcept { type.set(static_cast<float>(t)); }
    FilterType filter_type() const noexcept;
    std::size_t stages() const noexcept;
    std::vector<BiquadCoefficients> target_coefficients() const;

    Parameter type;
    Parameter frequency;
    Parameter q;
    Parameter gain;
    Parameter slope;

private:
    void push_parameters() noexcept;
    SmoothedBiquad m_filter;
};

class ParametricEq : public Effect {
public:
    static constexpr std::size_t max_bands = 16;

    struct Band {
        explicit Band(std::size_t index);
        Parameter enabled;
        Parameter type;
        Parameter frequency;
        Parameter q;
        Parameter gain;
        FilterType filter_type() const noexcept;
        bool is_enabled() const noexcept { return enabled.get() >= 0.5f; }
    };

    explicit ParametricEq(std::size_t bands = 8);
    const char* name() const noexcept override { return "parametric_eq"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;
    std::string save_state() const override;
    bool load_state(const std::string& state) override;

    std::size_t band_count() const noexcept { return m_bands.size(); }
    Band& band(std::size_t index) noexcept { return *m_bands[index]; }
    const Band& band(std::size_t index) const noexcept { return *m_bands[index]; }
    void set_band(std::size_t index, FilterType type, float frequency, float q, float gain_db, bool enabled = true) noexcept;

    Parameter output;

private:
    void push_parameters() noexcept;
    std::vector<std::unique_ptr<Band>> m_bands;
    std::vector<SmoothedBiquad> m_filters;
    float m_output = 1.0f;
    float m_smooth = 0.001f;
};

std::vector<float> frequency_response(const ParametricEq& eq, const std::vector<float>& freqs);

class GraphicEq : public Effect {
public:
    static constexpr std::size_t band_count = 10;
    static const float centers[band_count];

    GraphicEq();
    const char* name() const noexcept override { return "graphic_eq"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;

    Parameter& band(std::size_t index) noexcept { return *m_gains[index]; }
    std::vector<float> response_db(const std::vector<float>& freqs) const;

    Parameter output;

private:
    void push_parameters() noexcept;
    std::vector<std::unique_ptr<Parameter>> m_gains;
    std::vector<SmoothedBiquad> m_filters;
    float m_output = 1.0f;
    float m_smooth = 0.001f;
};

class DcBlocker : public Effect {
public:
    explicit DcBlocker(float frequency_hz = 10.0f);
    const char* name() const noexcept override { return "dc_blocker"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override;
    void process(float* interleaved, std::size_t frames) noexcept override;
    void reset() noexcept override;
    std::size_t tail_frames() const noexcept override;

    Parameter frequency;

private:
    std::vector<double> m_x1;
    std::vector<double> m_y1;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_FILTERS_HPP
