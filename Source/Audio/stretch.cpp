#include "stretch.hpp"
#include "pitch.hpp"
#include "resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const double k_two_pi = 6.28318530717958647692;
const double k_cutoff = 0.95;

double princarg(double x) noexcept {
    return x - k_two_pi * std::floor(x / k_two_pi + 0.5);
}

double bessel_i0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k) {
        term *= q / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

std::size_t nearest_power_of_two(double v) noexcept {
    std::size_t n = 256;
    while (n < 65536 && std::fabs(std::log2(static_cast<double>(n * 2) / v)) < std::fabs(std::log2(static_cast<double>(n) / v))) n *= 2;
    return n;
}

} // namespace

const char* stretch_mode_name(StretchMode mode) noexcept {
    switch (mode) {
        case StretchMode::automatic: return "automatic";
        case StretchMode::phase_vocoder: return "phase_vocoder";
        case StretchMode::wsola: return "wsola";
    }
    return "";
}

TimeStretcher::TimeStretcher() = default;

TimeStretcher::TimeStretcher(std::size_t channels, std::uint32_t sample_rate, StretchMode mode, StretchQuality quality, std::size_t max_block) {
    configure(channels, sample_rate, mode, quality, max_block);
}

void TimeStretcher::configure(std::size_t channels, std::uint32_t sample_rate, StretchMode mode, StretchQuality quality, std::size_t max_block) {
    m_channels = channels ? channels : 1;
    m_rate = sample_rate ? sample_rate : 48000;
    m_mode = mode == StretchMode::automatic ? StretchMode::phase_vocoder : mode;
    m_quality = quality;
    const std::size_t c = m_channels;
    const double scale = static_cast<double>(m_rate) / 48000.0;
    std::size_t span = 0;
    if (m_mode == StretchMode::phase_vocoder) {
        const double base = quality == StretchQuality::fast ? 1024.0 : (quality == StretchQuality::normal ? 2048.0 : 4096.0);
        m_window = nearest_power_of_two(base * scale);
        m_hop = quality == StretchQuality::high ? m_window / 8 : m_window / 4;
        m_diff = m_window / 8;
        m_bins = m_window / 2 + 1;
        m_tolerance = 0;
        m_win = make_window(WindowType::hann, m_window);
        double norm = 0.0;
        for (std::size_t i = 0; i < m_window; i += m_hop) norm += static_cast<double>(m_win[i]) * m_win[i];
        m_norm = static_cast<float>(norm > 0.0 ? norm : 1.0);
        m_fft.resize(m_window);
        m_frame.assign(m_window, 0.0f);
        m_frame_mid.assign(m_window, 0.0f);
        m_spec.assign(c * m_bins, std::complex<float>());
        m_mid.assign(m_bins, std::complex<float>());
        m_mid_prev.assign(m_bins, std::complex<float>());
        m_mag.assign(m_bins, 0.0f);
        m_phase.assign(m_bins, 0.0f);
        m_psi.assign(m_bins, 0.0f);
        m_rot_re.assign(m_bins, 1.0f);
        m_rot_im.assign(m_bins, 0.0f);
        m_peaks.assign(m_bins, 0);
        m_time.assign(m_window, 0.0f);
        m_ref.clear();
        m_cand.clear();
        span = m_window + m_diff + 2;
    } else {
        double frame_s = 0.03;
        double tol_s = 0.012;
        m_decimate = static_cast<std::size_t>(std::max(1.0, std::floor(4.0 * scale + 0.5)));
        if (quality == StretchQuality::normal) {
            frame_s = 0.025;
            tol_s = 0.010;
        } else if (quality == StretchQuality::fast) {
            frame_s = 0.02;
            tol_s = 0.008;
            m_decimate *= 2;
        }
        m_hop = std::max<std::size_t>(32, static_cast<std::size_t>(frame_s * 0.5 * static_cast<double>(m_rate) + 0.5));
        m_window = 2 * m_hop;
        m_tolerance = std::max<std::size_t>(8, static_cast<std::size_t>(tol_s * static_cast<double>(m_rate) + 0.5));
        m_diff = 0;
        m_bins = 0;
        m_win = make_window(WindowType::hann, m_window);
        m_norm = 1.0f;
        m_frame.clear();
        m_frame_mid.clear();
        m_spec.clear();
        m_mid.clear();
        m_mid_prev.clear();
        m_mag.clear();
        m_phase.clear();
        m_psi.clear();
        m_rot_re.clear();
        m_rot_im.clear();
        m_peaks.clear();
        m_time.assign(m_hop + 2 * m_tolerance + 2 * m_decimate + 4, 0.0f);
        m_ref.assign(m_hop / m_decimate + 2, 0.0f);
        m_cand.assign((2 * m_tolerance + m_hop) / m_decimate + 4, 0.0f);
        span = m_window + 2 * m_tolerance + m_hop + 2;
    }
    m_in_cap = 2 * span + 8192;
    m_in.assign(m_in_cap * c, 0.0f);
    m_ola.assign(m_window * c, 0.0f);
    m_zero_crossings = quality == StretchQuality::fast ? 8 : (quality == StretchQuality::normal ? 12 : 16);
    m_table_res = 256;
    const std::size_t table_size = m_zero_crossings * m_table_res + 2;
    m_table.assign(table_size, 0.0f);
    const double beta = 8.0;
    const double i0b = bessel_i0(beta);
    for (std::size_t i = 0; i < table_size; ++i) {
        const double x = static_cast<double>(i) / static_cast<double>(m_table_res);
        const double r = x / static_cast<double>(m_zero_crossings);
        if (r >= 1.0) {
            m_table[i] = 0.0f;
            continue;
        }
        const double w = bessel_i0(beta * std::sqrt(1.0 - r * r)) / i0b;
        const double a = k_pi * x;
        const double s = x < 1e-12 ? 1.0 : std::sin(a) / a;
        m_table[i] = static_cast<float>(s * w);
    }
    m_reach_max = kernel_reach(std::pow(2.0, max_semitones / 12.0));
    m_st_cap = 2 * m_reach_max + 2 * m_hop + 256;
    m_stretched.assign(m_st_cap * c, 0.0f);
    m_out_cap = std::max<std::size_t>(256, max_block);
    m_out.assign(m_out_cap * c, 0.0f);
    reset();
}

void TimeStretcher::set_tempo(double tempo) noexcept {
    if (!(tempo == tempo)) return;
    m_tempo.store(std::min(max_tempo, std::max(min_tempo, tempo)), std::memory_order_relaxed);
}

void TimeStretcher::set_pitch_semitones(double semitones) noexcept {
    if (!(semitones == semitones)) return;
    const double s = std::min(max_semitones, std::max(min_semitones, semitones));
    m_pitch.store(s == 0.0 ? 1.0 : std::pow(2.0, s / 12.0), std::memory_order_relaxed);
}

void TimeStretcher::set_pitch_ratio(double ratio) noexcept {
    if (!(ratio > 0.0)) return;
    const double lo = std::pow(2.0, min_semitones / 12.0);
    const double hi = std::pow(2.0, max_semitones / 12.0);
    m_pitch.store(std::min(hi, std::max(lo, ratio)), std::memory_order_relaxed);
}

double TimeStretcher::pitch_semitones() const noexcept {
    return 12.0 * std::log2(pitch_ratio());
}

void TimeStretcher::reset() noexcept {
    m_in_base = 0;
    m_in_count = 0;
    m_in_total = 0;
    m_eof = false;
    m_a = 0.0;
    m_s = 0;
    m_prev_best = 0;
    m_first = true;
    m_last_reset = -(1ll << 40);
    m_s_end = 0.0;
    m_end_known = false;
    m_synth_done = false;
    std::fill(m_ola.begin(), m_ola.end(), 0.0f);
    std::fill(m_psi.begin(), m_psi.end(), 0.0f);
    m_st_base = 0;
    m_st_count = 0;
    m_rpos = 0.0;
    m_output_done = false;
    m_out_read = 0;
    m_out_count = 0;
    m_pulled = 0;
}

std::size_t TimeStretcher::kernel_reach(double ratio) const noexcept {
    const double fc = (ratio > 1.0 ? 1.0 / ratio : 1.0) * k_cutoff;
    return static_cast<std::size_t>(std::ceil(static_cast<double>(m_zero_crossings) / fc)) + 1;
}

float TimeStretcher::kernel(double x) const noexcept {
    const double t = std::fabs(x) * static_cast<double>(m_table_res);
    const std::size_t i = static_cast<std::size_t>(t);
    if (i + 1 >= m_table.size()) return 0.0f;
    const float f = static_cast<float>(t - static_cast<double>(i));
    return m_table[i] + (m_table[i + 1] - m_table[i]) * f;
}

float TimeStretcher::in_at(std::size_t channel, long long index) const noexcept {
    if (index < m_in_base || index >= m_in_total) return 0.0f;
    return m_in[static_cast<std::size_t>(index - m_in_base) * m_channels + channel];
}

float TimeStretcher::mid_in_at(long long index) const noexcept {
    if (index < m_in_base || index >= m_in_total) return 0.0f;
    const float* p = &m_in[static_cast<std::size_t>(index - m_in_base) * m_channels];
    float s = 0.0f;
    for (std::size_t c = 0; c < m_channels; ++c) s += p[c];
    return s;
}

long long TimeStretcher::need_start() const noexcept {
    const long long center = std::llround(m_a);
    const long long half = static_cast<long long>(m_window / 2);
    if (m_mode == StretchMode::phase_vocoder) return center - half - static_cast<long long>(m_diff);
    const long long tol = static_cast<long long>(m_tolerance);
    long long s = center - tol - half;
    if (!m_first) s = std::min(s, m_prev_best + static_cast<long long>(m_hop) - half);
    return s;
}

long long TimeStretcher::need_end() const noexcept {
    const long long center = std::llround(m_a);
    const long long half = static_cast<long long>(m_window / 2);
    if (m_mode == StretchMode::phase_vocoder) return center + half;
    return center + static_cast<long long>(m_tolerance) + half;
}

void TimeStretcher::compact_input() noexcept {
    const long long keep = need_start();
    if (keep <= m_in_base || m_in_count == 0) {
        if (m_in_count == 0 && keep > m_in_base) m_in_base = std::min(keep, m_in_total);
        return;
    }
    const std::size_t drop = static_cast<std::size_t>(std::min<long long>(keep - m_in_base, static_cast<long long>(m_in_count)));
    const std::size_t c = m_channels;
    if (drop < m_in_count) std::memmove(m_in.data(), m_in.data() + drop * c, (m_in_count - drop) * c * sizeof(float));
    m_in_count -= drop;
    m_in_base += static_cast<long long>(drop);
}

void TimeStretcher::compact_mid() noexcept {
    const long long keep = static_cast<long long>(std::floor(m_rpos)) - static_cast<long long>(m_reach_max) - 1;
    if (keep <= m_st_base || m_st_count == 0) return;
    const std::size_t drop = static_cast<std::size_t>(std::min<long long>(keep - m_st_base, static_cast<long long>(m_st_count)));
    const std::size_t c = m_channels;
    if (drop < m_st_count) std::memmove(m_stretched.data(), m_stretched.data() + drop * c, (m_st_count - drop) * c * sizeof(float));
    m_st_count -= drop;
    m_st_base += static_cast<long long>(drop);
}

std::size_t TimeStretcher::input_space() const noexcept {
    if (!m_channels || m_eof) return 0;
    return m_in_cap - m_in_count;
}

std::size_t TimeStretcher::input_required() const noexcept {
    if (!m_channels || m_eof || m_synth_done) return 0;
    if (m_out_count >= m_out_cap) return 0;
    const long long need = need_end() - m_in_total;
    return need > 0 ? static_cast<std::size_t>(need) : 0;
}

bool TimeStretcher::finished() const noexcept {
    return m_eof && m_output_done && m_out_count == 0;
}

std::size_t TimeStretcher::latency_frames() const noexcept {
    if (!m_channels) return 0;
    const std::size_t reach = kernel_reach(1.0);
    if (m_mode == StretchMode::phase_vocoder) return m_window + reach;
    return m_window + m_tolerance + reach;
}

std::size_t TimeStretcher::push(const float* interleaved, std::size_t frames) noexcept {
    if (!m_channels || m_eof || !interleaved) return 0;
    compact_input();
    const std::size_t c = m_channels;
    std::size_t used = 0;
    if (m_in_count == 0) {
        const long long ns = need_start();
        if (m_in_total < ns) {
            const std::size_t skip = static_cast<std::size_t>(std::min<long long>(ns - m_in_total, static_cast<long long>(frames)));
            m_in_total += static_cast<long long>(skip);
            m_in_base = m_in_total;
            used += skip;
        }
    }
    const std::size_t n = std::min(frames - used, m_in_cap - m_in_count);
    if (n) {
        std::memcpy(m_in.data() + m_in_count * c, interleaved + used * c, n * c * sizeof(float));
        m_in_count += n;
        m_in_total += static_cast<long long>(n);
        used += n;
    }
    run();
    return used;
}

void TimeStretcher::finish() noexcept {
    if (!m_channels) return;
    m_eof = true;
    run();
}

std::size_t TimeStretcher::pull(float* interleaved, std::size_t frames) noexcept {
    if (!m_channels || !interleaved) return 0;
    const std::size_t c = m_channels;
    std::size_t done = 0;
    while (done < frames) {
        if (m_out_count == 0) {
            run();
            if (m_out_count == 0) break;
        }
        const std::size_t contiguous = std::min(m_out_count, m_out_cap - m_out_read);
        const std::size_t n = std::min(frames - done, contiguous);
        std::memcpy(interleaved + done * c, m_out.data() + m_out_read * c, n * c * sizeof(float));
        m_out_read = (m_out_read + n) % m_out_cap;
        m_out_count -= n;
        done += n;
    }
    run();
    m_pulled += done;
    return done;
}

void TimeStretcher::run() noexcept {
    if (!m_channels) return;
    const double ratio = m_pitch.load(std::memory_order_relaxed);
    while (m_out_count < m_out_cap && !m_output_done) {
        produce(ratio);
        if (m_out_count >= m_out_cap || m_output_done) break;
        if (!synth_frame()) {
            if (m_synth_done) produce(ratio);
            break;
        }
    }
}

void TimeStretcher::produce(double ratio) noexcept {
    const std::size_t c = m_channels;
    const double fc = (ratio > 1.0 ? 1.0 / ratio : 1.0) * k_cutoff;
    const long long reach = static_cast<long long>(kernel_reach(ratio));
    while (m_out_count < m_out_cap) {
        if (m_end_known && m_rpos >= m_s_end) {
            m_output_done = true;
            break;
        }
        const double fl = std::floor(m_rpos);
        const long long i0 = static_cast<long long>(fl);
        const double frac = m_rpos - fl;
        const bool exact = ratio == 1.0 && frac == 0.0;
        const long long hi = exact ? i0 : i0 + reach;
        const long long st_end = m_st_base + static_cast<long long>(m_st_count);
        if (hi >= st_end && !m_synth_done) break;
        float* o = m_out.data() + ((m_out_read + m_out_count) % m_out_cap) * c;
        if (exact) {
            if (i0 >= m_st_base && i0 < st_end) {
                std::memcpy(o, m_stretched.data() + static_cast<std::size_t>(i0 - m_st_base) * c, c * sizeof(float));
            } else {
                std::fill(o, o + c, 0.0f);
            }
        } else {
            std::fill(o, o + c, 0.0f);
            double wsum = 0.0;
            for (long long j = i0 - reach + 1; j <= i0 + reach; ++j) {
                const float w = kernel((static_cast<double>(j) - m_rpos) * fc);
                if (w == 0.0f) continue;
                wsum += w;
                if (j < m_st_base || j >= st_end) continue;
                const float* x = m_stretched.data() + static_cast<std::size_t>(j - m_st_base) * c;
                for (std::size_t ch = 0; ch < c; ++ch) o[ch] += w * x[ch];
            }
            if (wsum > 1e-9) {
                const float g = static_cast<float>(1.0 / wsum);
                for (std::size_t ch = 0; ch < c; ++ch) o[ch] *= g;
            }
        }
        ++m_out_count;
        m_rpos += ratio;
    }
}

bool TimeStretcher::synth_frame() noexcept {
    if (m_synth_done) return false;
    const long long half = static_cast<long long>(m_window / 2);
    if (m_end_known) {
        const double final_end = static_cast<double>(m_s - half);
        if (final_end >= m_s_end + static_cast<double>(m_reach_max) + 2.0) {
            m_synth_done = true;
            return false;
        }
    }
    compact_mid();
    if (m_st_count + m_hop > m_st_cap) return false;
    if (!m_eof && need_end() > m_in_total) return false;
    compact_input();
    const double hop = static_cast<double>(m_hop);
    if (m_eof && !m_end_known && m_a >= static_cast<double>(m_in_total)) {
        const double tempo = m_tempo.load(std::memory_order_relaxed);
        const double ratio = m_pitch.load(std::memory_order_relaxed);
        const double ha = hop * tempo / ratio;
        m_s_end = std::max(0.0, static_cast<double>(m_s) - (m_a - static_cast<double>(m_in_total)) * hop / ha);
        if (m_s == 0) m_s_end = 0.0;
        m_end_known = true;
    }
    const long long center = std::llround(m_a);
    if (m_mode == StretchMode::phase_vocoder) {
        synth_pv(center);
    } else {
        const long long best = m_first ? center : wsola_search(center);
        synth_wsola(best);
        m_prev_best = best;
    }
    m_first = false;
    commit_hop();
    const double tempo = m_tempo.load(std::memory_order_relaxed);
    const double ratio = m_pitch.load(std::memory_order_relaxed);
    m_a += hop * tempo / ratio;
    return true;
}

void TimeStretcher::commit_hop() noexcept {
    const std::size_t c = m_channels;
    const long long half = static_cast<long long>(m_window / 2);
    const long long start = m_s - half;
    for (std::size_t i = 0; i < m_hop; ++i) {
        const long long idx = start + static_cast<long long>(i);
        if (idx < 0) continue;
        if (idx != m_st_base + static_cast<long long>(m_st_count)) continue;
        if (m_st_count >= m_st_cap) break;
        std::memcpy(m_stretched.data() + m_st_count * c, m_ola.data() + i * c, c * sizeof(float));
        ++m_st_count;
    }
    const std::size_t keep = (m_window - m_hop) * c;
    std::memmove(m_ola.data(), m_ola.data() + m_hop * c, keep * sizeof(float));
    std::fill(m_ola.begin() + static_cast<std::ptrdiff_t>(keep), m_ola.end(), 0.0f);
    m_s += static_cast<long long>(m_hop);
}

void TimeStretcher::synth_pv(long long center) noexcept {
    const std::size_t n = m_window;
    const std::size_t bins = m_bins;
    const std::size_t c = m_channels;
    const long long half = static_cast<long long>(n / 2);
    const long long first = center - half;
    for (std::size_t ch = 0; ch < c; ++ch) {
        for (std::size_t i = 0; i < n; ++i) m_frame[i] = in_at(ch, first + static_cast<long long>(i)) * m_win[i];
        m_fft.forward_real(m_frame.data(), m_spec.data() + ch * bins);
    }
    if (c == 1) {
        std::copy(m_spec.begin(), m_spec.begin() + static_cast<std::ptrdiff_t>(bins), m_mid.begin());
    } else {
        for (std::size_t i = 0; i < n; ++i) m_frame_mid[i] = mid_in_at(first + static_cast<long long>(i)) * m_win[i];
        m_fft.forward_real(m_frame_mid.data(), m_mid.data());
    }
    const long long prev_first = first - static_cast<long long>(m_diff);
    for (std::size_t i = 0; i < n; ++i) m_frame_mid[i] = mid_in_at(prev_first + static_cast<long long>(i)) * m_win[i];
    m_fft.forward_real(m_frame_mid.data(), m_mid_prev.data());

    const long long block = static_cast<long long>(n / 16);
    double e_center = 0.0;
    double e_before = 0.0;
    for (long long i = -block; i < block; ++i) {
        const double v = mid_in_at(center + i);
        e_center += v * v;
    }
    for (long long i = -3 * block; i < -block; ++i) {
        const double v = mid_in_at(center + i);
        e_before += v * v;
    }
    const bool onset = e_center > 10.0 * e_before + 1e-7 * static_cast<double>(block) && center - m_last_reset >= static_cast<long long>(n / 4);

    float max_mag = 0.0f;
    for (std::size_t b = 0; b < bins; ++b) {
        m_mag[b] = std::abs(m_mid[b]);
        m_phase[b] = std::arg(m_mid[b]);
        max_mag = std::max(max_mag, m_mag[b]);
    }
    const double hop = static_cast<double>(m_hop);
    const double bin_w = k_two_pi / static_cast<double>(n);
    if (m_first || onset) {
        for (std::size_t b = 0; b < bins; ++b) m_psi[b] = m_phase[b];
        m_last_reset = center;
    } else {
        const float floor_mag = max_mag * 1e-6f;
        std::size_t count = 0;
        for (std::size_t b = 1; b + 1 < bins; ++b) {
            const float m = m_mag[b];
            if (m <= floor_mag || m < m_mag[b - 1] || m <= m_mag[b + 1]) continue;
            if (b >= 2 && m < m_mag[b - 2]) continue;
            if (b + 2 < bins && m <= m_mag[b + 2]) continue;
            m_peaks[count++] = b;
        }
        if (count == 0) {
            for (std::size_t b = 0; b < bins; ++b) m_psi[b] = static_cast<float>(princarg(static_cast<double>(m_psi[b]) + hop * bin_w * static_cast<double>(b)));
        } else {
            const double d = static_cast<double>(m_diff);
            std::size_t lo = 0;
            for (std::size_t k = 0; k < count; ++k) {
                const std::size_t p = m_peaks[k];
                const std::size_t hi = k + 1 < count ? (p + m_peaks[k + 1]) / 2 + 1 : bins;
                const double expected = bin_w * static_cast<double>(p);
                const double dev = princarg(static_cast<double>(m_phase[p]) - static_cast<double>(std::arg(m_mid_prev[p])) - expected * d);
                const double omega = expected + dev / d;
                const double psi_p = princarg(static_cast<double>(m_psi[p]) + hop * omega);
                const double base = psi_p - static_cast<double>(m_phase[p]);
                for (std::size_t b = lo; b < hi; ++b) m_psi[b] = static_cast<float>(princarg(base + static_cast<double>(m_phase[b])));
                lo = hi;
            }
        }
    }
    for (std::size_t b = 0; b < bins; ++b) {
        const double r = static_cast<double>(m_psi[b]) - static_cast<double>(m_phase[b]);
        m_rot_re[b] = static_cast<float>(std::cos(r));
        m_rot_im[b] = static_cast<float>(std::sin(r));
    }
    const float inv_norm = 1.0f / m_norm;
    for (std::size_t ch = 0; ch < c; ++ch) {
        std::complex<float>* x = m_spec.data() + ch * bins;
        for (std::size_t b = 0; b < bins; ++b) x[b] *= std::complex<float>(m_rot_re[b], m_rot_im[b]);
        x[0] = std::complex<float>(x[0].real(), 0.0f);
        x[bins - 1] = std::complex<float>(x[bins - 1].real(), 0.0f);
        m_fft.inverse_real(x, m_time.data());
        float* o = m_ola.data() + ch;
        for (std::size_t i = 0; i < n; ++i) o[i * c] += m_time[i] * m_win[i] * inv_norm;
    }
}

long long TimeStretcher::wsola_search(long long center) noexcept {
    const long long half = static_cast<long long>(m_window / 2);
    const long long len = static_cast<long long>(m_hop);
    const long long tol = static_cast<long long>(m_tolerance);
    const long long q = static_cast<long long>(m_decimate);
    const long long cont = m_prev_best + len;
    const long long ref_start = cont - half;
    const long long cand_start = center - tol - half;
    const std::size_t nref = static_cast<std::size_t>(len / q);
    double e_ref = 0.0;
    for (std::size_t j = 0; j < nref; ++j) {
        float s = 0.0f;
        for (long long k = 0; k < q; ++k) s += mid_in_at(ref_start + static_cast<long long>(j) * q + k);
        m_ref[j] = s;
        e_ref += static_cast<double>(s) * s;
    }
    if (e_ref < 1e-12 || nref == 0) return center;
    const std::size_t steps = static_cast<std::size_t>(2 * tol / q) + 1;
    const std::size_t ncand = steps + nref;
    for (std::size_t j = 0; j < ncand && j < m_cand.size(); ++j) {
        float s = 0.0f;
        for (long long k = 0; k < q; ++k) s += mid_in_at(cand_start + static_cast<long long>(j) * q + k);
        m_cand[j] = s;
    }
    double energy = 0.0;
    for (std::size_t j = 0; j < nref; ++j) energy += static_cast<double>(m_cand[j]) * m_cand[j];
    double best_score = -1e300;
    std::size_t best_step = steps / 2;
    for (std::size_t o = 0; o < steps && o + nref <= m_cand.size(); ++o) {
        double corr = 0.0;
        for (std::size_t j = 0; j < nref; ++j) corr += static_cast<double>(m_ref[j]) * m_cand[o + j];
        const double score = corr / std::sqrt(energy + 1e-9);
        const double bias = 1.0 - 0.1 * std::fabs(static_cast<double>(o) - static_cast<double>(steps / 2)) / static_cast<double>(steps);
        if (score * (score > 0.0 ? bias : 1.0) > best_score) {
            best_score = score * (score > 0.0 ? bias : 1.0);
            best_step = o;
        }
        if (o + nref < m_cand.size()) {
            energy += static_cast<double>(m_cand[o + nref]) * m_cand[o + nref] - static_cast<double>(m_cand[o]) * m_cand[o];
            if (energy < 0.0) energy = 0.0;
        }
    }
    const long long coarse = static_cast<long long>(best_step) * q - tol;
    const long long from = std::max(-tol, coarse - q);
    const long long to = std::min(tol, coarse + q);
    const std::size_t span = static_cast<std::size_t>(to - from + len);
    for (std::size_t i = 0; i < span && i < m_time.size(); ++i) m_time[i] = mid_in_at(center + from - half + static_cast<long long>(i));
    double best_fine = -1e300;
    long long best = coarse;
    for (long long d = from; d <= to; ++d) {
        double corr = 0.0;
        double e = 0.0;
        const std::size_t off = static_cast<std::size_t>(d - from);
        for (long long i = 0; i < len; ++i) {
            const double x = mid_in_at(ref_start + i);
            const double y = off + static_cast<std::size_t>(i) < m_time.size() ? m_time[off + static_cast<std::size_t>(i)] : 0.0;
            corr += x * y;
            e += y * y;
        }
        const double score = corr / std::sqrt(e + 1e-9);
        if (score > best_fine) {
            best_fine = score;
            best = d;
        }
    }
    return center + best;
}

void TimeStretcher::synth_wsola(long long best) noexcept {
    const std::size_t c = m_channels;
    const long long first = best - static_cast<long long>(m_window / 2);
    for (std::size_t i = 0; i < m_window; ++i) {
        const float w = m_win[i];
        float* o = m_ola.data() + i * c;
        for (std::size_t ch = 0; ch < c; ++ch) o[ch] += in_at(ch, first + static_cast<long long>(i)) * w;
    }
}

StretchMode detect_stretch_mode(const AudioBuffer& buffer) {
    if (buffer.empty() || buffer.channels() == 0 || buffer.sample_rate() == 0) return StretchMode::phase_vocoder;
    const std::size_t c = buffer.channels();
    if (c > 2) return StretchMode::phase_vocoder;
    if (c == 2) {
        double side = 0.0;
        double mid = 0.0;
        for (std::size_t i = 0; i < buffer.frames(); ++i) {
            const double l = buffer.at(i, 0);
            const double r = buffer.at(i, 1);
            side += (l - r) * (l - r);
            mid += (l + r) * (l + r);
        }
        if (side > 0.01 * mid) return StretchMode::phase_vocoder;
    }
    const std::size_t rate = buffer.sample_rate();
    const std::size_t total = std::min(buffer.frames(), rate * 20);
    std::vector<float> mono(total);
    for (std::size_t i = 0; i < total; ++i) {
        float s = 0.0f;
        for (std::size_t ch = 0; ch < c; ++ch) s += buffer.at(i, ch);
        mono[i] = s / static_cast<float>(c);
    }
    PitchDetector detector(static_cast<float>(rate), 70.0f, 500.0f);
    const std::size_t span = detector.window_frames();
    const std::size_t hop = std::max<std::size_t>(1, rate / 50);
    if (total < span) return StretchMode::phase_vocoder;
    double peak_rms = 0.0;
    std::vector<double> rms;
    for (std::size_t start = 0; start + span <= total; start += hop) {
        double e = 0.0;
        for (std::size_t i = 0; i < span; ++i) e += static_cast<double>(mono[start + i]) * mono[start + i];
        rms.push_back(std::sqrt(e / static_cast<double>(span)));
        peak_rms = std::max(peak_rms, rms.back());
    }
    if (peak_rms <= 0.0) return StretchMode::phase_vocoder;
    std::size_t active = 0;
    std::size_t voiced = 0;
    std::size_t index = 0;
    for (std::size_t start = 0; start + span <= total; start += hop, ++index) {
        if (rms[index] < 0.1 * peak_rms) continue;
        ++active;
        const PitchEstimate e = detector.analyze(mono.data() + start, span);
        if (e.voiced && e.confidence > 0.8f && e.frequency >= 70.0f && e.frequency <= 500.0f) ++voiced;
    }
    if (active == 0) return StretchMode::phase_vocoder;
    return static_cast<double>(voiced) >= 0.6 * static_cast<double>(active) ? StretchMode::wsola : StretchMode::phase_vocoder;
}

namespace {

AudioBuffer run_offline(const AudioBuffer& buffer, double tempo, double semitones, StretchQuality quality, StretchMode mode) {
    if (buffer.empty() || buffer.channels() == 0 || buffer.sample_rate() == 0) return buffer;
    if (mode == StretchMode::automatic) mode = detect_stretch_mode(buffer);
    const std::size_t c = buffer.channels();
    const std::size_t block = 8192;
    TimeStretcher st(c, buffer.sample_rate(), mode, quality, block);
    st.set_tempo(tempo);
    st.set_pitch_semitones(semitones);
    const std::size_t frames = buffer.frames();
    const std::size_t expected = static_cast<std::size_t>(std::floor(static_cast<double>(frames) / st.tempo() + 0.5));
    std::vector<float> out;
    out.reserve((expected + block) * c);
    std::vector<float> tmp(block * c);
    std::size_t pos = 0;
    std::size_t idle = 0;
    while (!st.finished() && idle < 4) {
        std::size_t pushed = 0;
        if (pos < frames) {
            pushed = st.push(buffer.frame(pos), std::min(block, frames - pos));
            pos += pushed;
            if (pos >= frames) st.finish();
        } else if (!st.input_finished()) {
            st.finish();
        }
        const std::size_t got = st.pull(tmp.data(), block);
        out.insert(out.end(), tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(got * c));
        if (got == 0 && pushed == 0) {
            ++idle;
        } else {
            idle = 0;
        }
        if (out.size() / c > expected + block) break;
    }
    out.resize(expected * c, 0.0f);
    return AudioBuffer(c, buffer.sample_rate(), std::move(out));
}

} // namespace

AudioBuffer time_stretch(const AudioBuffer& buffer, double tempo_factor, StretchQuality quality, StretchMode mode) {
    return run_offline(buffer, tempo_factor, 0.0, quality, mode);
}

AudioBuffer change_pitch(const AudioBuffer& buffer, double semitones, StretchQuality quality, StretchMode mode) {
    return run_offline(buffer, 1.0, semitones, quality, mode);
}

AudioBuffer change_tempo_and_pitch(const AudioBuffer& buffer, double tempo_factor, double semitones, StretchQuality quality, StretchMode mode) {
    return run_offline(buffer, tempo_factor, semitones, quality, mode);
}

AudioBuffer change_speed(const AudioBuffer& buffer, double factor, ResampleQuality quality) {
    if (buffer.empty() || buffer.channels() == 0 || buffer.sample_rate() == 0 || !(factor > 0.0)) return buffer;
    factor = std::min(100.0, std::max(0.01, factor));
    if (factor == 1.0) return buffer;
    const std::size_t c = buffer.channels();
    const double rate = static_cast<double>(buffer.sample_rate());
    Resampler r(c, rate * factor, rate, quality);
    std::vector<float> out;
    const std::size_t expected = static_cast<std::size_t>(std::floor(static_cast<double>(buffer.frames()) / factor + 0.5));
    out.reserve((expected + 64) * c);
    r.process(buffer.data(), buffer.frames(), out);
    r.flush(out);
    out.resize(expected * c, 0.0f);
    return AudioBuffer(c, buffer.sample_rate(), std::move(out));
}

} // namespace audio
} // namespace fizmo
