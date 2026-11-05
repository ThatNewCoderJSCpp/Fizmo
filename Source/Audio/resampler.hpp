#ifndef FIZMO_AUDIO_RESAMPLER_HPP
#define FIZMO_AUDIO_RESAMPLER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace audio {

enum class ResampleQuality {
    fast,
    medium,
    high,
    best
};

class Resampler {
public:
    Resampler();
    Resampler(std::size_t channels, double input_rate, double output_rate, ResampleQuality quality = ResampleQuality::high);

    void configure(std::size_t channels, double input_rate, double output_rate, ResampleQuality quality = ResampleQuality::high);
    void set_rates(double input_rate, double output_rate);
    void reset();

    std::size_t process(const float* input, std::size_t input_frames, std::vector<float>& output);
    std::size_t flush(std::vector<float>& output);

    std::size_t channels() const noexcept { return m_channels; }
    double input_rate() const noexcept { return m_in_rate; }
    double output_rate() const noexcept { return m_out_rate; }
    double ratio() const noexcept { return m_out_rate / m_in_rate; }
    std::size_t latency_frames() const noexcept { return m_half; }
    std::size_t taps() const noexcept { return 2 * m_half; }

private:
    void build_table();
    void compute_step();
    std::size_t run(std::vector<float>& output, bool draining);

    std::size_t m_channels = 1;
    double m_in_rate = 48000.0;
    double m_out_rate = 48000.0;
    ResampleQuality m_quality = ResampleQuality::high;
    std::size_t m_half = 32;
    std::size_t m_phases = 256;
    double m_cutoff = 1.0;
    std::vector<float> m_table;
    std::vector<float> m_history;
    std::vector<float> m_coeffs;
    std::uint64_t m_pos_int = 0;
    std::uint64_t m_pos_num = 0;
    std::uint64_t m_den = 1;
    std::uint64_t m_step_int = 1;
    std::uint64_t m_step_num = 0;
    std::size_t m_input_total = 0;
    std::size_t m_output_total = 0;
    bool m_passthrough = true;
};

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_RESAMPLER_HPP
