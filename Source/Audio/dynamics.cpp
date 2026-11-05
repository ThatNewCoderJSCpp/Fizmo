#include "dynamics.hpp"
#include "format.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace audio {

namespace {

double time_coefficient(double ms, double rate) noexcept {
    if (ms <= 0.0 || rate <= 0.0) return 0.0;
    return std::exp(-1000.0 / (ms * rate));
}

double power_to_db(double p) noexcept {
    return 10.0 * std::log10(p + 1e-30);
}

double amplitude_to_db(double a) noexcept {
    return 20.0 * std::log10(a + 1e-30);
}

double gain_from_db(double db) noexcept {
    return std::pow(10.0, db / 20.0);
}

double rate_of(std::uint32_t r) noexcept {
    return r ? static_cast<double>(r) : 48000.0;
}

double compressor_curve(double level, double t, double r, double w) noexcept {
    const double over = level - t;
    const double slope = 1.0 / r - 1.0;
    if (w > 0.0 && 2.0 * std::fabs(over) <= w) {
        const double x = over + w * 0.5;
        return slope * x * x / (2.0 * w);
    }
    return over > 0.0 ? slope * over : 0.0;
}

} // namespace

Compressor::Compressor(float threshold_db, float ratio_value, float attack_ms, float release_ms)
    : threshold("threshold", threshold_db, -80.0f, 0.0f, "dB"),
      ratio("ratio", ratio_value, 1.0f, 100.0f, ":1"),
      knee("knee", 6.0f, 0.0f, 24.0f, "dB"),
      attack("attack", attack_ms, 0.01f, 500.0f, "ms"),
      release("release", release_ms, 1.0f, 5000.0f, "ms"),
      makeup("makeup", 0.0f, -24.0f, 36.0f, "dB"),
      auto_makeup("auto_makeup", 0.0f, 0.0f, 1.0f),
      detection("detection", 0.0f, 0.0f, 1.0f),
      link("link", 1.0f, 0.0f, 1.0f),
      lookahead("lookahead", 0.0f, 0.0f, max_lookahead_ms, "ms"),
      sidechain_hpf("sidechain_hpf", 0.0f, 0.0f, 1000.0f, "Hz") {
    add_parameter(threshold);
    add_parameter(ratio);
    add_parameter(knee);
    add_parameter(attack);
    add_parameter(release);
    add_parameter(makeup);
    add_parameter(auto_makeup);
    add_parameter(detection);
    add_parameter(link);
    add_parameter(lookahead);
    add_parameter(sidechain_hpf);
}

float Compressor::makeup_db() const noexcept {
    double m = makeup.get();
    if (auto_makeup.get() >= 0.5f) m -= 0.5 * compressor_curve(0.0, threshold.get(), ratio.get(), knee.get());
    return static_cast<float>(m);
}

float Compressor::static_gain_db(float input_db) const noexcept {
    return static_cast<float>(compressor_curve(input_db, threshold.get(), ratio.get(), knee.get()) + makeup_db());
}

void Compressor::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_ms.assign(channels, 0.0);
    m_env.assign(channels, 0.0);
    m_det.assign(channels, 0.0);
    m_delay_frames = static_cast<std::size_t>(std::ceil(max_lookahead_ms * 0.001 * rate_of(sample_rate))) + 1;
    m_delay.assign(m_delay_frames * channels, 0.0f);
    m_sidechain.prepare(channels);
    reset();
}

void Compressor::reset() noexcept {
    std::fill(m_ms.begin(), m_ms.end(), 0.0);
    std::fill(m_env.begin(), m_env.end(), 0.0);
    std::fill(m_delay.begin(), m_delay.end(), 0.0f);
    m_write = 0;
    const std::size_t la = static_cast<std::size_t>(std::lround(lookahead.get() * 0.001 * rate_of(sample_rate())));
    m_lookahead = std::min(la, m_delay_frames ? m_delay_frames - 1 : 0);
    m_sidechain.reset();
    m_hpf_frequency = sidechain_hpf.get();
    if (m_hpf_frequency >= 10.0f) m_sidechain.set_coefficients(design_biquad(FilterType::highpass, rate_of(sample_rate()), m_hpf_frequency, 0.70710678, 0.0));
    m_meter.store(0.0f, std::memory_order_relaxed);
}

void Compressor::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0 || m_env.size() < c) return;
    const double rate = rate_of(sample_rate());
    const double t = threshold.get();
    const double r = ratio.get();
    const double w = knee.get();
    const double aa = time_coefficient(attack.get(), rate);
    const double ar = time_coefficient(release.get(), rate);
    const double rms_coef = time_coefficient(10.0, rate);
    const double mk = makeup_db();
    const bool rms = detection.get() >= 0.5f;
    const double lk = link.get();
    const float hpf = sidechain_hpf.get();
    const bool use_hpf = hpf >= 10.0f;
    if (use_hpf && hpf != m_hpf_frequency) {
        const BiquadCoefficients coef = design_biquad(FilterType::highpass, rate, hpf, 0.70710678, 0.0);
        if (m_hpf_frequency < 10.0f) {
            m_sidechain.reset();
            m_sidechain.set_coefficients(coef);
        } else {
            m_sidechain.set_target(coef, 64);
        }
    }
    m_hpf_frequency = use_hpf ? hpf : 0.0f;
    double worst = 0.0;
    for (std::size_t f = 0; f < frames; ++f) {
        float* p = interleaved + f * c;
        double maxdet = 0.0;
        for (std::size_t ch = 0; ch < c; ++ch) {
            double s = p[ch];
            if (use_hpf) s = m_sidechain.process_sample(ch, s);
            double d = s * s;
            if (rms) {
                m_ms[ch] = rms_coef * m_ms[ch] + (1.0 - rms_coef) * d;
                d = m_ms[ch];
            }
            m_det[ch] = d;
            maxdet = std::max(maxdet, d);
        }
        if (use_hpf) m_sidechain.advance();
        float* slot = m_delay.data() + m_write * c;
        for (std::size_t ch = 0; ch < c; ++ch) slot[ch] = p[ch];
        const std::size_t read = (m_write + m_delay_frames - m_lookahead) % m_delay_frames;
        const float* delayed = m_delay.data() + read * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const double d = lk * maxdet + (1.0 - lk) * m_det[ch];
            const double gr = compressor_curve(power_to_db(d), t, r, w);
            double& env = m_env[ch];
            env = gr < env ? aa * env + (1.0 - aa) * gr : ar * env + (1.0 - ar) * gr;
            worst = std::min(worst, env);
            p[ch] = static_cast<float>(delayed[ch] * gain_from_db(env + mk));
        }
        m_write = (m_write + 1) % m_delay_frames;
    }
    for (double& v : m_ms) {
        if (v < 1e-30) v = 0.0;
    }
    m_meter.store(static_cast<float>(worst), std::memory_order_relaxed);
}

Limiter::Limiter(float ceiling_db, float release_ms, float lookahead_ms, bool tp)
    : ceiling("ceiling", ceiling_db, -60.0f, 0.0f, "dB"),
      release("release", release_ms, 1.0f, 2000.0f, "ms"),
      lookahead("lookahead", lookahead_ms, 0.05f, max_lookahead_ms, "ms"),
      true_peak("true_peak", tp ? 1.0f : 0.0f, 0.0f, 1.0f) {
    add_parameter(ceiling);
    add_parameter(release);
    add_parameter(lookahead);
    add_parameter(true_peak);
}

void Limiter::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_max_window = static_cast<std::size_t>(std::ceil(max_lookahead_ms * 0.001 * rate_of(sample_rate))) + 1;
    m_delay_frames = m_max_window + TruePeakDetector::taps_per_phase / 2 + 2;
    m_delay.assign(m_delay_frames * channels, 0.0f);
    m_queue_value.assign(m_max_window + 2, 1.0f);
    m_queue_index.assign(m_max_window + 2, 0);
    m_box.assign(m_max_window + 1, 1.0);
    m_tp.prepare(channels);
    reset();
}

void Limiter::configure() noexcept {
    const long w = std::lround(lookahead.get() * 0.001 * rate_of(sample_rate()));
    m_window = std::min(m_max_window, static_cast<std::size_t>(std::max<long>(1, w)));
    m_use_tp = true_peak.get() >= 0.5f;
    m_detector_delay = m_use_tp ? m_tp.latency_frames() : 0;
    m_latency = m_detector_delay + m_window - 1;
}

void Limiter::reset() noexcept {
    configure();
    std::fill(m_delay.begin(), m_delay.end(), 0.0f);
    m_write = 0;
    m_queue_head = 0;
    m_queue_size = 0;
    std::fill(m_box.begin(), m_box.end(), 1.0);
    m_box_pos = 0;
    m_box_sum = static_cast<double>(m_window);
    m_env = 1.0;
    m_prev_required = 1.0f;
    m_counter = 0;
    m_tp.reset();
    m_meter.store(0.0f, std::memory_order_relaxed);
}

void Limiter::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0 || m_delay.size() < m_delay_frames * c || m_box.size() < m_window) return;
    const double rate = rate_of(sample_rate());
    const float ceil_gain = db_to_gain(ceiling.get());
    const double ar = time_coefficient(release.get(), rate);
    const std::size_t cap = m_queue_value.size();
    const double inv_window = 1.0 / static_cast<double>(m_window);
    double lowest = 1.0;
    for (std::size_t f = 0; f < frames; ++f) {
        float* p = interleaved + f * c;
        float peak = 0.0f;
        if (m_use_tp) {
            peak = m_tp.push_frame(p);
        } else {
            for (std::size_t ch = 0; ch < c; ++ch) peak = std::max(peak, std::fabs(p[ch]));
        }
        float required = peak > ceil_gain ? ceil_gain / peak : 1.0f;
        if (m_use_tp) {
            const float r = required;
            required = std::min(required, m_prev_required);
            m_prev_required = r;
        }
        while (m_queue_size > 0) {
            const std::size_t back = (m_queue_head + m_queue_size - 1) % cap;
            if (m_queue_value[back] >= required) --m_queue_size; else break;
        }
        const std::size_t slot_q = (m_queue_head + m_queue_size) % cap;
        m_queue_value[slot_q] = required;
        m_queue_index[slot_q] = m_counter;
        ++m_queue_size;
        while (m_queue_index[m_queue_head] + m_window <= m_counter) {
            m_queue_head = (m_queue_head + 1) % cap;
            --m_queue_size;
        }
        const double held = m_queue_value[m_queue_head];
        m_env = held < m_env ? held : held + (m_env - held) * ar;
        m_box_sum += m_env - m_box[m_box_pos];
        m_box[m_box_pos] = m_env;
        m_box_pos = m_box_pos + 1 == m_window ? 0 : m_box_pos + 1;
        const double g = std::min(1.0, m_box_sum * inv_window);
        lowest = std::min(lowest, g);
        float* slot = m_delay.data() + m_write * c;
        for (std::size_t ch = 0; ch < c; ++ch) slot[ch] = p[ch];
        const std::size_t read = (m_write + m_delay_frames - m_latency) % m_delay_frames;
        const float* delayed = m_delay.data() + read * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float y = static_cast<float>(delayed[ch] * g);
            p[ch] = std::min(ceil_gain, std::max(-ceil_gain, y));
        }
        m_write = (m_write + 1) % m_delay_frames;
        ++m_counter;
    }
    if (m_counter > (static_cast<std::uint64_t>(1) << 62)) {
        for (std::size_t i = 0; i < cap; ++i) m_queue_index[i] -= std::min(m_queue_index[i], static_cast<std::uint64_t>(1) << 61);
        m_counter -= static_cast<std::uint64_t>(1) << 61;
    }
    m_meter.store(static_cast<float>(amplitude_to_db(lowest)), std::memory_order_relaxed);
}

NoiseGate::NoiseGate(float threshold_db, float hysteresis_db, float attack_ms, float hold_ms, float release_ms, float range_db)
    : threshold("threshold", threshold_db, -100.0f, 0.0f, "dB"),
      hysteresis("hysteresis", hysteresis_db, 0.0f, 30.0f, "dB"),
      attack("attack", attack_ms, 0.01f, 200.0f, "ms"),
      hold("hold", hold_ms, 0.0f, 5000.0f, "ms"),
      release("release", release_ms, 1.0f, 5000.0f, "ms"),
      range("range", range_db, -120.0f, 0.0f, "dB") {
    add_parameter(threshold);
    add_parameter(hysteresis);
    add_parameter(attack);
    add_parameter(hold);
    add_parameter(release);
    add_parameter(range);
}

void NoiseGate::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    reset();
}

void NoiseGate::reset() noexcept {
    m_env = 0.0;
    m_open = false;
    m_hold_left = 0;
    m_gain = range.get() <= -120.0f ? 0.0 : gain_from_db(range.get());
    m_open_flag.store(false, std::memory_order_relaxed);
    m_meter.store(static_cast<float>(range.get()), std::memory_order_relaxed);
}

void NoiseGate::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0) return;
    const double rate = rate_of(sample_rate());
    const double open_level = gain_from_db(threshold.get());
    const double close_level = gain_from_db(threshold.get() - hysteresis.get());
    const double closed_gain = range.get() <= -120.0f ? 0.0 : gain_from_db(range.get());
    const std::size_t hold_frames = static_cast<std::size_t>(std::lround(hold.get() * 0.001 * rate));
    const double det = time_coefficient(10.0, rate);
    const double ka = 1.0 - time_coefficient(attack.get(), rate);
    const double kr = 1.0 - time_coefficient(release.get(), rate);
    for (std::size_t f = 0; f < frames; ++f) {
        float* p = interleaved + f * c;
        double peak = 0.0;
        for (std::size_t ch = 0; ch < c; ++ch) peak = std::max(peak, static_cast<double>(std::fabs(p[ch])));
        m_env = std::max(peak, m_env * det);
        if (!m_open) {
            if (m_env >= open_level) {
                m_open = true;
                m_hold_left = hold_frames;
            }
        } else if (m_env >= close_level) {
            m_hold_left = hold_frames;
        } else if (m_hold_left > 0) {
            --m_hold_left;
        } else {
            m_open = false;
        }
        const double target = m_open ? 1.0 : closed_gain;
        m_gain += (target - m_gain) * (target > m_gain ? ka : kr);
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] = static_cast<float>(p[ch] * m_gain);
    }
    if (m_env < 1e-30) m_env = 0.0;
    m_open_flag.store(m_open, std::memory_order_relaxed);
    m_meter.store(static_cast<float>(amplitude_to_db(m_gain)), std::memory_order_relaxed);
}

Expander::Expander(float threshold_db, float ratio_value, float attack_ms, float release_ms, float range_db)
    : threshold("threshold", threshold_db, -100.0f, 0.0f, "dB"),
      ratio("ratio", ratio_value, 1.0f, 20.0f, ":1"),
      attack("attack", attack_ms, 0.01f, 500.0f, "ms"),
      release("release", release_ms, 1.0f, 5000.0f, "ms"),
      range("range", range_db, -120.0f, 0.0f, "dB") {
    add_parameter(threshold);
    add_parameter(ratio);
    add_parameter(attack);
    add_parameter(release);
    add_parameter(range);
}

float Expander::static_gain_db(float input_db) const noexcept {
    const double t = threshold.get();
    if (input_db >= t) return 0.0f;
    return static_cast<float>(std::max<double>(range.get(), (input_db - t) * (ratio.get() - 1.0)));
}

void Expander::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    reset();
}

void Expander::reset() noexcept {
    m_env = 0.0;
    m_gr = 0.0;
    m_meter.store(0.0f, std::memory_order_relaxed);
}

void Expander::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0) return;
    const double rate = rate_of(sample_rate());
    const double det = time_coefficient(10.0, rate);
    const double aa = time_coefficient(attack.get(), rate);
    const double ar = time_coefficient(release.get(), rate);
    const double t = threshold.get();
    const double r = ratio.get();
    const double floor_db = range.get();
    for (std::size_t f = 0; f < frames; ++f) {
        float* p = interleaved + f * c;
        double peak = 0.0;
        for (std::size_t ch = 0; ch < c; ++ch) peak = std::max(peak, static_cast<double>(std::fabs(p[ch])));
        m_env = std::max(peak, m_env * det);
        const double level = amplitude_to_db(m_env);
        const double target = level >= t ? 0.0 : std::max(floor_db, (level - t) * (r - 1.0));
        m_gr = target > m_gr ? aa * m_gr + (1.0 - aa) * target : ar * m_gr + (1.0 - ar) * target;
        const double g = gain_from_db(m_gr);
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] = static_cast<float>(p[ch] * g);
    }
    if (m_env < 1e-30) m_env = 0.0;
    m_meter.store(static_cast<float>(m_gr), std::memory_order_relaxed);
}

DeEsser::DeEsser(float frequency_hz, float threshold_db, float amount_db)
    : frequency("frequency", frequency_hz, 1000.0f, 16000.0f, "Hz"),
      threshold("threshold", threshold_db, -60.0f, 0.0f, "dB"),
      amount("amount", amount_db, 0.0f, 24.0f, "dB") {
    add_parameter(frequency);
    add_parameter(threshold);
    add_parameter(amount);
}

void DeEsser::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_low1.prepare(channels);
    m_low2.prepare(channels);
    m_high1.prepare(channels);
    m_high2.prepare(channels);
    m_high.assign(channels, 0.0);
    m_low.assign(channels, 0.0);
    reset();
}

void DeEsser::design() noexcept {
    const double rate = rate_of(sample_rate());
    const BiquadCoefficients lp = design_biquad(FilterType::lowpass, rate, m_frequency, 0.70710678, 0.0);
    const BiquadCoefficients hp = design_biquad(FilterType::highpass, rate, m_frequency, 0.70710678, 0.0);
    m_low1.set_target(lp, 64);
    m_low2.set_target(lp, 64);
    m_high1.set_target(hp, 64);
    m_high2.set_target(hp, 64);
}

void DeEsser::reset() noexcept {
    m_low1.reset();
    m_low2.reset();
    m_high1.reset();
    m_high2.reset();
    m_frequency = frequency.get();
    design();
    m_low1.set_coefficients(m_low1.target());
    m_low2.set_coefficients(m_low2.target());
    m_high1.set_coefficients(m_high1.target());
    m_high2.set_coefficients(m_high2.target());
    m_env = 0.0;
    m_gr = 0.0;
    m_meter.store(0.0f, std::memory_order_relaxed);
}

void DeEsser::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0 || m_high.size() < c) return;
    const double rate = rate_of(sample_rate());
    if (frequency.get() != m_frequency) {
        m_frequency = frequency.get();
        design();
    }
    const double det = time_coefficient(20.0, rate);
    const double aa = time_coefficient(0.5, rate);
    const double ar = time_coefficient(50.0, rate);
    const double t = threshold.get();
    const double most = amount.get();
    double worst = 0.0;
    for (std::size_t f = 0; f < frames; ++f) {
        float* p = interleaved + f * c;
        double peak = 0.0;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const double x = p[ch];
            m_low[ch] = m_low2.process_sample(ch, m_low1.process_sample(ch, x));
            m_high[ch] = m_high2.process_sample(ch, m_high1.process_sample(ch, x));
            peak = std::max(peak, std::fabs(m_high[ch]));
        }
        m_low1.advance();
        m_low2.advance();
        m_high1.advance();
        m_high2.advance();
        m_env = std::max(peak, m_env * det);
        const double over = amplitude_to_db(m_env) - t;
        const double target = -std::min(most, std::max(0.0, over));
        m_gr = target < m_gr ? aa * m_gr + (1.0 - aa) * target : ar * m_gr + (1.0 - ar) * target;
        worst = std::min(worst, m_gr);
        const double g = gain_from_db(m_gr);
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] = static_cast<float>(m_low[ch] + g * m_high[ch]);
    }
    if (m_env < 1e-30) m_env = 0.0;
    m_meter.store(static_cast<float>(worst), std::memory_order_relaxed);
}

} // namespace audio
} // namespace fizmo
