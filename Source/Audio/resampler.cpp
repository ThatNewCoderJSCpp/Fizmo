#include "resampler.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace audio {

namespace {

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

struct QualitySettings {
    std::size_t half;
    std::size_t phases;
    double beta;
};

QualitySettings settings_for(ResampleQuality q) noexcept {
    switch (q) {
        case ResampleQuality::fast: return { 8, 64, 5.0 };
        case ResampleQuality::medium: return { 16, 128, 7.0 };
        case ResampleQuality::high: return { 48, 256, 9.0 };
        case ResampleQuality::best: return { 96, 1024, 12.0 };
    }
    return { 48, 256, 9.0 };
}

double cutoff_for(const QualitySettings& s) noexcept {
    const double attenuation = s.beta / 0.1102 + 8.7;
    const double transition = 2.0 * (attenuation - 7.95) / (14.36 * 2.0 * static_cast<double>(s.half));
    const double fc = 1.0 - transition / 2.0;
    return fc < 0.5 ? 0.5 : (fc > 0.98 ? 0.98 : fc);
}

bool whole_rate(double r) noexcept {
    return r >= 1.0 && r <= 2147483647.0 && std::fabs(r - std::floor(r + 0.5)) < 1e-9;
}

} // namespace

Resampler::Resampler() {
    configure(1, 48000.0, 48000.0, ResampleQuality::high);
}

Resampler::Resampler(std::size_t channels, double input_rate, double output_rate, ResampleQuality quality) {
    configure(channels, input_rate, output_rate, quality);
}

void Resampler::configure(std::size_t channels, double input_rate, double output_rate, ResampleQuality quality) {
    m_channels = channels == 0 ? 1 : channels;
    m_in_rate = input_rate > 0.0 ? input_rate : 48000.0;
    m_out_rate = output_rate > 0.0 ? output_rate : m_in_rate;
    m_quality = quality;
    build_table();
    reset();
}

void Resampler::build_table() {
    const QualitySettings s = settings_for(m_quality);
    m_passthrough = m_in_rate == m_out_rate;
    const double down = std::min(1.0, m_out_rate / m_in_rate);
    m_cutoff = cutoff_for(s) * down;
    m_half = static_cast<std::size_t>(std::ceil(static_cast<double>(s.half) / down));
    m_phases = s.phases;
    const std::size_t taps = 2 * m_half;
    m_table.assign((m_phases + 1) * taps, 0.0f);
    m_coeffs.assign(taps, 0.0f);
    const double i0b = bessel_i0(s.beta);
    const double pi = 3.14159265358979323846;
    for (std::size_t p = 0; p <= m_phases; ++p) {
        const double frac = static_cast<double>(p) / static_cast<double>(m_phases);
        double sum = 0.0;
        std::vector<double> row(taps);
        for (std::size_t i = 0; i < taps; ++i) {
            const double d = static_cast<double>(i) - static_cast<double>(m_half) + 1.0 - frac;
            const double x = d / static_cast<double>(m_half);
            double w = 0.0;
            if (x > -1.0 && x < 1.0) w = bessel_i0(s.beta * std::sqrt(1.0 - x * x)) / i0b;
            const double a = pi * m_cutoff * d;
            const double sinc = std::fabs(a) < 1e-12 ? 1.0 : std::sin(a) / a;
            row[i] = m_cutoff * sinc * w;
            sum += row[i];
        }
        for (std::size_t i = 0; i < taps; ++i) m_table[p * taps + i] = static_cast<float>(sum != 0.0 ? row[i] / sum : row[i]);
    }
}

void Resampler::compute_step() {
    const std::uint64_t old_den = m_den;
    if (whole_rate(m_in_rate) && whole_rate(m_out_rate)) {
        const std::uint64_t in = static_cast<std::uint64_t>(std::floor(m_in_rate + 0.5));
        const std::uint64_t out = static_cast<std::uint64_t>(std::floor(m_out_rate + 0.5));
        m_den = out;
        m_step_int = in / out;
        m_step_num = in % out;
    } else {
        const double step = m_in_rate / m_out_rate;
        const double scale = 4294967296.0;
        m_den = 4294967296ull;
        m_step_int = static_cast<std::uint64_t>(std::floor(step));
        m_step_num = static_cast<std::uint64_t>(std::floor((step - std::floor(step)) * scale + 0.5));
        if (m_step_num >= m_den) {
            ++m_step_int;
            m_step_num -= m_den;
        }
    }
    if (old_den != m_den && old_den != 0) {
        const long double f = static_cast<long double>(m_pos_num) / static_cast<long double>(old_den);
        m_pos_num = static_cast<std::uint64_t>(f * static_cast<long double>(m_den));
        if (m_pos_num >= m_den) m_pos_num = m_den - 1;
    }
}

void Resampler::set_rates(double input_rate, double output_rate) {
    if (input_rate <= 0.0 || output_rate <= 0.0) return;
    const double old_down = std::min(1.0, m_out_rate / m_in_rate);
    const bool was_passthrough = m_passthrough;
    m_in_rate = input_rate;
    m_out_rate = output_rate;
    compute_step();
    const double new_down = std::min(1.0, m_out_rate / m_in_rate);
    const bool now_passthrough = m_in_rate == m_out_rate;
    if (was_passthrough != now_passthrough || std::fabs(new_down - old_down) > 0.01 * old_down) {
        const std::size_t old_half = m_half;
        build_table();
        if (was_passthrough) {
            reset();
        } else if (m_half > old_half) {
            const std::size_t extra = m_half - old_half;
            m_history.insert(m_history.begin(), extra * m_channels, 0.0f);
            m_pos_int += extra;
        }
    }
    m_passthrough = now_passthrough && was_passthrough;
}

void Resampler::reset() {
    m_history.assign((m_half - 1) * m_channels, 0.0f);
    m_pos_int = m_half - 1;
    m_pos_num = 0;
    compute_step();
    m_input_total = 0;
    m_output_total = 0;
}

std::size_t Resampler::run(std::vector<float>& output, bool draining) {
    const std::size_t taps = 2 * m_half;
    const std::size_t c = m_channels;
    const std::size_t have = m_history.size() / c;
    const double inv_den = 1.0 / static_cast<double>(m_den);
    std::size_t limit = static_cast<std::size_t>(-1);
    if (draining) {
        const double expected = std::ceil(static_cast<double>(m_input_total) * (m_out_rate / m_in_rate) - 1e-9);
        const std::size_t want = expected > 0.0 ? static_cast<std::size_t>(expected) : 0;
        limit = want > m_output_total ? want - m_output_total : 0;
    }
    std::size_t produced = 0;
    while (produced < limit) {
        const std::size_t n = static_cast<std::size_t>(m_pos_int);
        if (n + m_half >= have) break;
        const double frac = static_cast<double>(m_pos_num) * inv_den;
        const double pf = frac * static_cast<double>(m_phases);
        std::size_t ip = static_cast<std::size_t>(pf);
        if (ip >= m_phases) ip = m_phases - 1;
        const float w = static_cast<float>(pf - static_cast<double>(ip));
        const float* r0 = &m_table[ip * taps];
        const float* r1 = &m_table[(ip + 1) * taps];
        for (std::size_t i = 0; i < taps; ++i) m_coeffs[i] = r0[i] + (r1[i] - r0[i]) * w;
        const std::size_t first = n + 1 - m_half;
        const std::size_t base = output.size();
        output.resize(base + c, 0.0f);
        float* out = &output[base];
        const float* src = &m_history[first * c];
        if (c == 1) {
            float acc = 0.0f;
            for (std::size_t i = 0; i < taps; ++i) acc += src[i] * m_coeffs[i];
            out[0] = acc;
        } else if (c == 2) {
            float a0 = 0.0f;
            float a1 = 0.0f;
            for (std::size_t i = 0; i < taps; ++i) {
                a0 += src[2 * i] * m_coeffs[i];
                a1 += src[2 * i + 1] * m_coeffs[i];
            }
            out[0] = a0;
            out[1] = a1;
        } else {
            for (std::size_t i = 0; i < taps; ++i) {
                const float k = m_coeffs[i];
                const float* f = src + i * c;
                for (std::size_t ch = 0; ch < c; ++ch) out[ch] += f[ch] * k;
            }
        }
        ++produced;
        m_pos_int += m_step_int;
        m_pos_num += m_step_num;
        if (m_pos_num >= m_den) {
            m_pos_num -= m_den;
            ++m_pos_int;
        }
    }
    m_output_total += produced;
    if (m_pos_int >= m_half) {
        std::size_t drop = static_cast<std::size_t>(m_pos_int) + 1 - m_half;
        drop = std::min(drop, have);
        m_history.erase(m_history.begin(), m_history.begin() + static_cast<std::ptrdiff_t>(drop * c));
        m_pos_int -= drop;
    }
    return produced;
}

std::size_t Resampler::process(const float* input, std::size_t input_frames, std::vector<float>& output) {
    if (input_frames == 0) return 0;
    if (m_passthrough) {
        output.insert(output.end(), input, input + input_frames * m_channels);
        m_input_total += input_frames;
        m_output_total += input_frames;
        return input_frames;
    }
    m_history.insert(m_history.end(), input, input + input_frames * m_channels);
    m_input_total += input_frames;
    return run(output, false);
}

std::size_t Resampler::flush(std::vector<float>& output) {
    if (m_passthrough) {
        reset();
        return 0;
    }
    const std::size_t pad = 2 * m_half + static_cast<std::size_t>(std::ceil(m_in_rate / m_out_rate)) + 2;
    m_history.insert(m_history.end(), pad * m_channels, 0.0f);
    const std::size_t produced = run(output, true);
    reset();
    return produced;
}

} // namespace audio
} // namespace fizmo
