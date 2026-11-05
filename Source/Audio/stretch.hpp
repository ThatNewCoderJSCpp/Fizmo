#ifndef FIZMO_AUDIO_STRETCH_HPP
#define FIZMO_AUDIO_STRETCH_HPP

#include "buffer.hpp"
#include "fft.hpp"

#include <atomic>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace audio {

enum class StretchMode {
    automatic,
    phase_vocoder,
    wsola
};

enum class StretchQuality {
    fast,
    normal,
    high
};

const char* stretch_mode_name(StretchMode mode) noexcept;

class TimeStretcher {
public:
    static constexpr double min_tempo = 0.25;
    static constexpr double max_tempo = 4.0;
    static constexpr double min_semitones = -24.0;
    static constexpr double max_semitones = 24.0;

    TimeStretcher();
    TimeStretcher(std::size_t channels, std::uint32_t sample_rate, StretchMode mode = StretchMode::phase_vocoder, StretchQuality quality = StretchQuality::high, std::size_t max_block = 4096);
    TimeStretcher(const TimeStretcher&) = delete;
    TimeStretcher& operator=(const TimeStretcher&) = delete;

    void configure(std::size_t channels, std::uint32_t sample_rate, StretchMode mode = StretchMode::phase_vocoder, StretchQuality quality = StretchQuality::high, std::size_t max_block = 4096);
    bool configured() const noexcept { return m_channels != 0; }

    void set_tempo(double tempo) noexcept;
    void set_pitch_semitones(double semitones) noexcept;
    void set_pitch_ratio(double ratio) noexcept;
    double tempo() const noexcept { return m_tempo.load(std::memory_order_relaxed); }
    double pitch_ratio() const noexcept { return m_pitch.load(std::memory_order_relaxed); }
    double pitch_semitones() const noexcept;

    std::size_t channels() const noexcept { return m_channels; }
    std::uint32_t sample_rate() const noexcept { return m_rate; }
    StretchMode mode() const noexcept { return m_mode; }
    StretchQuality quality() const noexcept { return m_quality; }
    std::size_t window_frames() const noexcept { return m_window; }
    std::size_t hop_frames() const noexcept { return m_hop; }

    void reset() noexcept;
    std::size_t push(const float* interleaved, std::size_t frames) noexcept;
    std::size_t pull(float* interleaved, std::size_t frames) noexcept;
    void finish() noexcept;

    std::size_t available() const noexcept { return m_out_count; }
    std::size_t input_required() const noexcept;
    std::size_t input_space() const noexcept;
    bool input_finished() const noexcept { return m_eof; }
    bool finished() const noexcept;
    std::size_t latency_frames() const noexcept;
    std::uint64_t frames_pushed() const noexcept { return static_cast<std::uint64_t>(m_in_total); }
    std::uint64_t frames_pulled() const noexcept { return m_pulled; }

private:
    bool synth_frame() noexcept;
    void synth_pv(long long center) noexcept;
    long long wsola_search(long long center) noexcept;
    void synth_wsola(long long best) noexcept;
    void commit_hop() noexcept;
    void run() noexcept;
    void produce(double ratio) noexcept;
    float in_at(std::size_t channel, long long index) const noexcept;
    float mid_in_at(long long index) const noexcept;
    long long need_start() const noexcept;
    long long need_end() const noexcept;
    void compact_input() noexcept;
    void compact_mid() noexcept;
    std::size_t kernel_reach(double ratio) const noexcept;
    float kernel(double x) const noexcept;

    std::size_t m_channels = 0;
    std::uint32_t m_rate = 0;
    StretchMode m_mode = StretchMode::phase_vocoder;
    StretchQuality m_quality = StretchQuality::high;
    std::atomic<double> m_tempo{ 1.0 };
    std::atomic<double> m_pitch{ 1.0 };

    std::size_t m_window = 0;
    std::size_t m_hop = 0;
    std::size_t m_bins = 0;
    std::size_t m_diff = 0;
    std::size_t m_tolerance = 0;
    std::size_t m_decimate = 4;
    float m_norm = 1.0f;
    std::vector<float> m_win;
    Fft m_fft;

    std::vector<float> m_in;
    std::size_t m_in_cap = 0;
    long long m_in_base = 0;
    std::size_t m_in_count = 0;
    long long m_in_total = 0;
    bool m_eof = false;

    double m_a = 0.0;
    long long m_s = 0;
    long long m_prev_best = 0;
    bool m_first = true;
    long long m_last_reset = 0;
    double m_s_end = 0.0;
    bool m_end_known = false;
    bool m_synth_done = false;

    std::vector<float> m_frame;
    std::vector<float> m_frame_mid;
    std::vector<std::complex<float>> m_spec;
    std::vector<std::complex<float>> m_mid;
    std::vector<std::complex<float>> m_mid_prev;
    std::vector<float> m_mag;
    std::vector<float> m_phase;
    std::vector<float> m_psi;
    std::vector<float> m_rot_re;
    std::vector<float> m_rot_im;
    std::vector<std::size_t> m_peaks;
    std::vector<float> m_time;
    std::vector<float> m_ref;
    std::vector<float> m_cand;

    std::vector<float> m_ola;

    std::vector<float> m_stretched;
    std::size_t m_st_cap = 0;
    long long m_st_base = 0;
    std::size_t m_st_count = 0;

    double m_rpos = 0.0;
    bool m_output_done = false;
    std::size_t m_zero_crossings = 16;
    std::size_t m_table_res = 256;
    std::vector<float> m_table;
    std::size_t m_reach_max = 0;

    std::vector<float> m_out;
    std::size_t m_out_cap = 0;
    std::size_t m_out_read = 0;
    std::size_t m_out_count = 0;
    std::uint64_t m_pulled = 0;
};

StretchMode detect_stretch_mode(const AudioBuffer& buffer);
AudioBuffer time_stretch(const AudioBuffer& buffer, double tempo_factor, StretchQuality quality = StretchQuality::high, StretchMode mode = StretchMode::automatic);
AudioBuffer change_pitch(const AudioBuffer& buffer, double semitones, StretchQuality quality = StretchQuality::high, StretchMode mode = StretchMode::automatic);
AudioBuffer change_speed(const AudioBuffer& buffer, double factor, ResampleQuality quality = ResampleQuality::high);
AudioBuffer change_tempo_and_pitch(const AudioBuffer& buffer, double tempo_factor, double semitones, StretchQuality quality = StretchQuality::high, StretchMode mode = StretchMode::automatic);

} // namespace audio
} // namespace fizmo

#endif // FIZMO_AUDIO_STRETCH_HPP
