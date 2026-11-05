#include "loudness.hpp"
#include "format.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const float k_neg_inf = -std::numeric_limits<float>::infinity();

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

const double k_bs1770_rate = 48000.0;
const double k_bs1770_shelf[5] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
const double k_bs1770_highpass[5] = { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };

struct KSection {
    double f0;
    double q;
    double high;
    double mid;
    double low;
};

KSection analog_section(const double* c) noexcept {
    const double n2 = c[0] - c[1] + c[2];
    const double n1 = 2.0 * (c[0] - c[2]);
    const double n0 = c[0] + c[1] + c[2];
    const double d2 = 1.0 - c[3] + c[4];
    const double d1 = 2.0 * (1.0 - c[4]);
    const double d0 = 1.0 + c[3] + c[4];
    const double w = std::sqrt(d0 / d2);
    KSection s;
    s.f0 = k_bs1770_rate / k_pi * std::atan(w);
    s.q = w * d2 / d1;
    s.high = n2 / d2;
    s.mid = n1 / d1;
    s.low = n0 / d0;
    return s;
}

void discretize(const KSection& s, double fs, double* b, double* a) noexcept {
    const double f0 = std::min(s.f0, 0.49 * fs);
    const double k = std::tan(k_pi * f0 / fs);
    const double kk = k * k;
    const double kq = k / s.q;
    const double a0 = 1.0 + kq + kk;
    b[0] = (s.high + s.mid * kq + s.low * kk) / a0;
    b[1] = 2.0 * (s.low * kk - s.high) / a0;
    b[2] = (s.high - s.mid * kq + s.low * kk) / a0;
    a[0] = 1.0;
    a[1] = 2.0 * (kk - 1.0) / a0;
    a[2] = (1.0 - kq + kk) / a0;
}

using PhaseTable = std::array<std::array<float, TruePeakDetector::taps_per_phase + 1>, TruePeakDetector::oversampling>;

const PhaseTable& true_peak_table() {
    static const PhaseTable table = [] {
        PhaseTable t{};
        const std::size_t taps = TruePeakDetector::taps_per_phase;
        const double half = static_cast<double>(taps) / 2.0;
        const double extent = half + 1.0;
        const double beta = 7.0;
        const double norm = bessel_i0(beta);
        for (std::size_t k = 0; k < TruePeakDetector::oversampling; ++k) {
            double sum = 0.0;
            std::array<double, TruePeakDetector::taps_per_phase + 1> c{};
            for (std::size_t j = 0; j <= taps; ++j) {
                const double d = static_cast<double>(j) - half + static_cast<double>(k) / static_cast<double>(TruePeakDetector::oversampling);
                double v = 0.0;
                if (k == 0) {
                    v = static_cast<double>(j) == half ? 1.0 : 0.0;
                } else {
                    const double s = std::sin(k_pi * d) / (k_pi * d);
                    const double r = d / extent;
                    const double w = bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / norm;
                    v = s * w;
                }
                c[j] = v;
                sum += v;
            }
            for (std::size_t j = 0; j <= taps; ++j) t[k][j] = static_cast<float>(c[j] / sum);
        }
        return t;
    }();
    return table;
}

float to_db(float linear) noexcept {
    return linear > 0.0f ? 20.0f * std::log10(linear) : k_neg_inf;
}

float energy_to_lufs(double energy) noexcept {
    if (!(energy > 0.0)) return k_neg_inf;
    return static_cast<float>(-0.691 + 10.0 * std::log10(energy));
}

const std::size_t k_ring = TruePeakDetector::taps_per_phase + 1;

} // namespace

TruePeakDetector::TruePeakDetector(std::size_t channels) {
    prepare(channels);
}

void TruePeakDetector::prepare(std::size_t channels) {
    true_peak_table();
    m_channels = channels;
    m_history.assign(channels * 2 * k_ring, 0.0f);
    m_zero.assign(channels, 0.0f);
    m_pos = 0;
}

void TruePeakDetector::reset() noexcept {
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    m_pos = 0;
}

float TruePeakDetector::push_frame(const float* frame, float* per_channel) noexcept {
    const PhaseTable& t = true_peak_table();
    float best = 0.0f;
    m_pos = (m_pos + 1) % k_ring;
    for (std::size_t ch = 0; ch < m_channels; ++ch) {
        float* h = m_history.data() + ch * 2 * k_ring;
        h[m_pos] = frame[ch];
        h[m_pos + k_ring] = frame[ch];
        const float* newest = h + m_pos + k_ring;
        float peak = 0.0f;
        for (std::size_t k = 0; k < oversampling; ++k) {
            const auto& c = t[k];
            float acc = 0.0f;
            for (std::size_t j = 0; j <= taps_per_phase; ++j) acc += c[j] * newest[-static_cast<std::ptrdiff_t>(j)];
            peak = std::max(peak, std::fabs(acc));
        }
        if (per_channel) per_channel[ch] = std::max(per_channel[ch], peak);
        best = std::max(best, peak);
    }
    return best;
}

float TruePeakDetector::push_frame(const float* frame) noexcept {
    return push_frame(frame, nullptr);
}

float TruePeakDetector::process(const float* interleaved, std::size_t frames) noexcept {
    float best = 0.0f;
    for (std::size_t f = 0; f < frames; ++f) best = std::max(best, push_frame(interleaved + f * m_channels));
    return best;
}

float TruePeakDetector::flush() noexcept {
    float best = 0.0f;
    for (std::size_t i = 0; i < latency_frames(); ++i) best = std::max(best, push_frame(m_zero.data()));
    return best;
}

void LoudnessMeter::Hist::clear() noexcept {
    std::fill(count.begin(), count.end(), 0);
    std::fill(energy.begin(), energy.end(), 0.0);
    total = 0;
    total_energy = 0.0;
}

void LoudnessMeter::Hist::add(double loudness, double e) noexcept {
    if (count.empty()) return;
    double idx = std::floor((loudness - histogram_min) / histogram_step);
    idx = std::min(static_cast<double>(histogram_bins - 1), std::max(0.0, idx));
    const std::size_t i = static_cast<std::size_t>(idx);
    ++count[i];
    energy[i] += e;
    ++total;
    total_energy += e;
}

LoudnessMeter::LoudnessMeter()
    : m_momentary(k_neg_inf), m_short_term(k_neg_inf), m_integrated(k_neg_inf), m_range(0.0f),
      m_true_peak(0.0f), m_sample_peak(0.0f), m_max_momentary(k_neg_inf), m_max_short_term(k_neg_inf) {}

LoudnessMeter::LoudnessMeter(std::uint32_t sample_rate, std::size_t channels) : LoudnessMeter() {
    prepare(sample_rate, channels);
}

void LoudnessMeter::prepare(std::uint32_t sample_rate, std::size_t channels) {
    m_rate = sample_rate ? sample_rate : 48000;
    m_channels = channels;
    m_weights.assign(channels, 1.0);
    if (channels == 4) {
        m_weights[2] = 1.41;
        m_weights[3] = 1.41;
    } else if (channels == 5) {
        m_weights[3] = 1.41;
        m_weights[4] = 1.41;
    } else if (channels >= 6) {
        m_weights[3] = 0.0;
        for (std::size_t c = 4; c < channels; ++c) m_weights[c] = 1.41;
    }
    m_state.assign(channels * 4, 0.0);
    const double fs = static_cast<double>(m_rate);
    const KSection shelf = analog_section(k_bs1770_shelf);
    const KSection highpass = analog_section(k_bs1770_highpass);
    discretize(shelf, fs, m_pb, m_pa);
    discretize(highpass, fs, m_rb, m_ra);
    m_gate.count.assign(histogram_bins, 0);
    m_gate.energy.assign(histogram_bins, 0.0);
    m_lra.count.assign(histogram_bins, 0);
    m_lra.energy.assign(histogram_bins, 0.0);
    m_tp.prepare(channels);
    reset();
}

void LoudnessMeter::reset() noexcept {
    std::fill(m_state.begin(), m_state.end(), 0.0);
    m_frames = 0;
    m_subblocks = 0;
    m_next_boundary = m_rate / 10;
    m_acc = 0.0;
    m_acc_frames = 0;
    for (std::size_t i = 0; i < 30; ++i) {
        m_ring_energy[i] = 0.0;
        m_ring_frames[i] = 0;
    }
    m_gate.clear();
    m_lra.clear();
    m_tp.reset();
    m_tp_max = 0.0f;
    m_sp_max = 0.0f;
    m_momentary.store(k_neg_inf);
    m_short_term.store(k_neg_inf);
    m_integrated.store(k_neg_inf);
    m_range.store(0.0f);
    m_true_peak.store(0.0f);
    m_sample_peak.store(0.0f);
    m_max_momentary.store(k_neg_inf);
    m_max_short_term.store(k_neg_inf);
}

void LoudnessMeter::set_channel_weight(std::size_t channel, float weight) noexcept {
    if (channel < m_weights.size()) m_weights[channel] = weight;
}

float LoudnessMeter::channel_weight(std::size_t channel) const noexcept {
    return channel < m_weights.size() ? static_cast<float>(m_weights[channel]) : 0.0f;
}

void LoudnessMeter::push(const float* interleaved, std::size_t frames) noexcept {
    if (m_channels == 0 || !interleaved) return;
    const std::size_t c = m_channels;
    for (std::size_t f = 0; f < frames; ++f) {
        const float* p = interleaved + f * c;
        double sum = 0.0;
        for (std::size_t ch = 0; ch < c; ++ch) {
            double* s = m_state.data() + ch * 4;
            const double x = p[ch];
            m_sp_max = std::max(m_sp_max, std::fabs(p[ch]));
            const double y1 = m_pb[0] * x + s[0];
            s[0] = m_pb[1] * x - m_pa[1] * y1 + s[1];
            s[1] = m_pb[2] * x - m_pa[2] * y1;
            const double y2 = m_rb[0] * y1 + s[2];
            s[2] = m_rb[1] * y1 - m_ra[1] * y2 + s[3];
            s[3] = m_rb[2] * y1 - m_ra[2] * y2;
            sum += m_weights[ch] * y2 * y2;
        }
        if (m_true_peak_on) m_tp_max = std::max(m_tp_max, m_tp.push_frame(p));
        m_acc += sum;
        ++m_acc_frames;
        ++m_frames;
        if (m_frames >= m_next_boundary) end_subblock();
    }
    for (double& s : m_state) {
        if (std::fabs(s) < 1e-30) s = 0.0;
    }
    m_sample_peak.store(m_sp_max, std::memory_order_relaxed);
    m_true_peak.store(std::max(m_tp_max, m_sp_max), std::memory_order_relaxed);
}

bool LoudnessMeter::push(const AudioBuffer& buffer) noexcept {
    if (buffer.channels() != m_channels || buffer.sample_rate() != m_rate) return false;
    push(buffer.data(), buffer.frames());
    return true;
}

void LoudnessMeter::finish() noexcept {
    if (m_true_peak_on) m_tp_max = std::max(m_tp_max, m_tp.flush());
    m_true_peak.store(std::max(m_tp_max, m_sp_max), std::memory_order_relaxed);
}

void LoudnessMeter::end_subblock() noexcept {
    const std::size_t slot = static_cast<std::size_t>(m_subblocks % 30);
    m_ring_energy[slot] = m_acc;
    m_ring_frames[slot] = m_acc_frames;
    m_acc = 0.0;
    m_acc_frames = 0;
    ++m_subblocks;
    m_next_boundary = ((m_subblocks + 1) * static_cast<std::uint64_t>(m_rate)) / 10;
    if (m_subblocks >= 4) {
        double e = 0.0;
        std::uint64_t n = 0;
        for (std::uint64_t i = 0; i < 4; ++i) {
            const std::size_t s = static_cast<std::size_t>((m_subblocks - 1 - i) % 30);
            e += m_ring_energy[s];
            n += m_ring_frames[s];
        }
        const double z = n ? e / static_cast<double>(n) : 0.0;
        const float l = energy_to_lufs(z);
        m_momentary.store(l, std::memory_order_relaxed);
        if (l > m_max_momentary.load(std::memory_order_relaxed)) m_max_momentary.store(l, std::memory_order_relaxed);
        if (l > static_cast<float>(histogram_min)) {
            m_gate.add(static_cast<double>(l), z);
            compute_integrated();
        }
    }
    if (m_subblocks >= 30) {
        double e = 0.0;
        std::uint64_t n = 0;
        for (std::size_t i = 0; i < 30; ++i) {
            e += m_ring_energy[i];
            n += m_ring_frames[i];
        }
        const double z = n ? e / static_cast<double>(n) : 0.0;
        const float l = energy_to_lufs(z);
        m_short_term.store(l, std::memory_order_relaxed);
        if (l > m_max_short_term.load(std::memory_order_relaxed)) m_max_short_term.store(l, std::memory_order_relaxed);
        if (l > static_cast<float>(histogram_min)) {
            m_lra.add(static_cast<double>(l), z);
            compute_range();
        }
    }
}

void LoudnessMeter::compute_integrated() noexcept {
    if (m_gate.total == 0) {
        m_integrated.store(k_neg_inf, std::memory_order_relaxed);
        return;
    }
    const double abs_mean = m_gate.total_energy / static_cast<double>(m_gate.total);
    const double gate = -0.691 + 10.0 * std::log10(abs_mean) - 10.0;
    const double gi = std::floor((gate - histogram_min) / histogram_step);
    double e = 0.0;
    std::uint64_t n = 0;
    for (std::size_t i = 0; i < histogram_bins; ++i) {
        if (m_gate.count[i] == 0) continue;
        const double di = static_cast<double>(i);
        if (di < gi) continue;
        if (di == gi) {
            const double mean = -0.691 + 10.0 * std::log10(m_gate.energy[i] / static_cast<double>(m_gate.count[i]));
            if (!(mean > gate)) continue;
        }
        e += m_gate.energy[i];
        n += m_gate.count[i];
    }
    m_integrated.store(n ? energy_to_lufs(e / static_cast<double>(n)) : k_neg_inf, std::memory_order_relaxed);
}

void LoudnessMeter::compute_range() noexcept {
    if (m_lra.total == 0) {
        m_range.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const double abs_mean = m_lra.total_energy / static_cast<double>(m_lra.total);
    const double gate = -0.691 + 10.0 * std::log10(abs_mean) - 20.0;
    const double gi = std::floor((gate - histogram_min) / histogram_step);
    std::size_t first = histogram_bins;
    std::uint64_t n = 0;
    for (std::size_t i = 0; i < histogram_bins; ++i) {
        if (m_lra.count[i] == 0) continue;
        const double di = static_cast<double>(i);
        if (di < gi) continue;
        if (di == gi) {
            const double mean = -0.691 + 10.0 * std::log10(m_lra.energy[i] / static_cast<double>(m_lra.count[i]));
            if (!(mean > gate)) continue;
        }
        if (first == histogram_bins) first = i;
        n += m_lra.count[i];
    }
    if (n == 0) {
        m_range.store(0.0f, std::memory_order_relaxed);
        return;
    }
    const std::uint64_t lo_index = static_cast<std::uint64_t>(std::llround(static_cast<double>(n - 1) * 0.10));
    const std::uint64_t hi_index = static_cast<std::uint64_t>(std::llround(static_cast<double>(n - 1) * 0.95));
    double lo = 0.0;
    double hi = 0.0;
    bool have_lo = false;
    std::uint64_t cum = 0;
    for (std::size_t i = first; i < histogram_bins; ++i) {
        if (m_lra.count[i] == 0) continue;
        cum += m_lra.count[i];
        const double centre = histogram_min + (static_cast<double>(i) + 0.5) * histogram_step;
        if (!have_lo && cum > lo_index) {
            lo = centre;
            have_lo = true;
        }
        if (cum > hi_index) {
            hi = centre;
            break;
        }
    }
    m_range.store(static_cast<float>(std::max(0.0, hi - lo)), std::memory_order_relaxed);
}

float LoudnessMeter::true_peak_db() const noexcept {
    return to_db(m_true_peak.load(std::memory_order_relaxed));
}

float LoudnessMeter::sample_peak_db() const noexcept {
    return to_db(m_sample_peak.load(std::memory_order_relaxed));
}

LoudnessResult LoudnessMeter::result() const noexcept {
    LoudnessResult r;
    r.integrated = integrated();
    r.range = loudness_range();
    r.true_peak_db = true_peak_db();
    r.sample_peak_db = sample_peak_db();
    r.max_momentary = max_momentary();
    r.max_short_term = max_short_term();
    r.momentary = momentary();
    r.short_term = short_term();
    return r;
}

LoudnessResult measure_loudness(const AudioBuffer& buffer) {
    LoudnessMeter meter;
    if (buffer.channels() == 0 || buffer.sample_rate() == 0) return meter.result();
    meter.prepare(buffer.sample_rate(), buffer.channels());
    meter.push(buffer.data(), buffer.frames());
    meter.finish();
    return meter.result();
}

void normalize_loudness(AudioBuffer& buffer, float target_lufs, float max_true_peak_db) {
    const LoudnessResult r = measure_loudness(buffer);
    if (!std::isfinite(r.integrated)) return;
    float gain = target_lufs - r.integrated;
    if (std::isfinite(r.true_peak_db) && r.true_peak_db + gain > max_true_peak_db) gain = max_true_peak_db - r.true_peak_db;
    buffer.apply_gain(db_to_gain(gain));
}

float peak_db(const AudioBuffer& buffer) {
    float p = 0.0f;
    for (float s : buffer.samples()) p = std::max(p, std::fabs(s));
    return to_db(p);
}

float true_peak_db(const AudioBuffer& buffer) {
    if (buffer.channels() == 0) return k_neg_inf;
    TruePeakDetector tp(buffer.channels());
    float p = tp.process(buffer.data(), buffer.frames());
    p = std::max(p, tp.flush());
    return to_db(p);
}

void normalize_peak(AudioBuffer& buffer, float target_db) {
    const float p = peak_db(buffer);
    if (!std::isfinite(p)) return;
    buffer.apply_gain(db_to_gain(target_db - p));
}

SpectrumAnalyzer::SpectrumAnalyzer(std::size_t fft_size, WindowType window) : m_window_type(window), m_written(0) {
    m_ring_size = max_fft_size * 2;
    m_ring.reset(new std::atomic<float>[m_ring_size]);
    for (std::size_t i = 0; i < m_ring_size; ++i) m_ring[i].store(0.0f, std::memory_order_relaxed);
    m_size = std::min(max_fft_size, std::max(min_fft_size, next_power_of_two(fft_size)));
    rebuild();
}

void SpectrumAnalyzer::prepare(std::uint32_t sample_rate, std::size_t channels) {
    m_rate = sample_rate ? sample_rate : 48000;
    m_channels = channels ? channels : 1;
    clear();
}

void SpectrumAnalyzer::clear() noexcept {
    for (std::size_t i = 0; i < m_ring_size; ++i) m_ring[i].store(0.0f, std::memory_order_relaxed);
    m_written.store(0, std::memory_order_release);
    m_last_written = 0;
    m_fresh = true;
}

void SpectrumAnalyzer::push(const float* interleaved, std::size_t frames) noexcept {
    if (!interleaved) return;
    const std::size_t c = m_channels;
    const float scale = 1.0f / static_cast<float>(c);
    std::uint64_t w = m_written.load(std::memory_order_relaxed);
    for (std::size_t f = 0; f < frames; ++f) {
        const float* p = interleaved + f * c;
        float s = 0.0f;
        for (std::size_t ch = 0; ch < c; ++ch) s += p[ch];
        m_ring[static_cast<std::size_t>(w % m_ring_size)].store(s * scale, std::memory_order_relaxed);
        ++w;
    }
    m_written.store(w, std::memory_order_release);
}

void SpectrumAnalyzer::rebuild() {
    m_fft.resize(m_size);
    m_window = make_window(m_window_type, m_size);
    double sum = 0.0;
    for (float v : m_window) sum += v;
    m_window_scale = sum > 0.0 ? static_cast<float>(2.0 / sum) : 1.0f;
    m_frame.assign(m_size, 0.0f);
    m_spectrum.assign(m_size / 2 + 1, std::complex<float>());
    m_power.assign(m_size / 2 + 1, 0.0f);
    m_db.assign(m_size / 2 + 1, -200.0f);
    m_peak_db.assign(m_size / 2 + 1, -200.0f);
    m_peak_age.assign(m_size / 2 + 1, 0.0f);
    m_fresh = true;
}

void SpectrumAnalyzer::set_fft_size(std::size_t size) {
    const std::size_t n = std::min(max_fft_size, std::max(min_fft_size, next_power_of_two(size)));
    if (n == m_size) return;
    m_size = n;
    rebuild();
}

void SpectrumAnalyzer::set_window(WindowType window) {
    m_window_type = window;
    rebuild();
}

void SpectrumAnalyzer::set_smoothing(float amount) noexcept {
    m_smoothing = std::min(0.999f, std::max(0.0f, amount));
}

void SpectrumAnalyzer::set_peak_hold(float hold_seconds, float decay_db_per_second) noexcept {
    m_hold = std::max(0.0f, hold_seconds);
    m_decay = std::max(0.0f, decay_db_per_second);
}

bool SpectrumAnalyzer::update() {
    const std::uint64_t w = m_written.load(std::memory_order_acquire);
    if (w == 0) return false;
    for (std::size_t i = 0; i < m_size; ++i) {
        const std::uint64_t back = static_cast<std::uint64_t>(m_size - i);
        float s = 0.0f;
        if (back <= w) s = m_ring[static_cast<std::size_t>((w - back) % m_ring_size)].load(std::memory_order_relaxed);
        m_frame[i] = s * m_window[i];
    }
    m_fft.forward_real(m_frame.data(), m_spectrum.data());
    const float dt = static_cast<float>(static_cast<double>(w - m_last_written) / static_cast<double>(m_rate));
    m_last_written = w;
    const std::size_t nb = m_size / 2 + 1;
    for (std::size_t k = 0; k < nb; ++k) {
        float scale = m_window_scale;
        if (k == 0 || k == nb - 1) scale *= 0.5f;
        const float mag = std::abs(m_spectrum[k]) * scale;
        const float p = mag * mag;
        m_power[k] = m_fresh ? p : m_smoothing * m_power[k] + (1.0f - m_smoothing) * p;
        const float db = std::max(-200.0f, 10.0f * std::log10(m_power[k] + 1e-30f));
        m_db[k] = db;
        if (m_fresh || db >= m_peak_db[k]) {
            m_peak_db[k] = db;
            m_peak_age[k] = 0.0f;
        } else {
            const float before = m_peak_age[k];
            m_peak_age[k] += dt;
            const float over = m_peak_age[k] - std::max(before, m_hold);
            if (m_peak_age[k] > m_hold && over > 0.0f) m_peak_db[k] = std::max(db, m_peak_db[k] - m_decay * over);
        }
    }
    m_fresh = false;
    return true;
}

float SpectrumAnalyzer::bin_frequency(std::size_t bin) const noexcept {
    return static_cast<float>(static_cast<double>(bin) * static_cast<double>(m_rate) / static_cast<double>(m_size));
}

std::vector<float> SpectrumAnalyzer::band_frequencies(std::size_t count, float min_hz, float max_hz) const {
    std::vector<float> out(count);
    if (count == 0) return out;
    min_hz = std::max(1.0f, min_hz);
    max_hz = std::max(min_hz * 1.001f, max_hz);
    const double ratio = std::log(static_cast<double>(max_hz) / static_cast<double>(min_hz));
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = static_cast<float>(static_cast<double>(min_hz) * std::exp(ratio * (static_cast<double>(i) + 0.5) / static_cast<double>(count)));
    }
    return out;
}

void SpectrumAnalyzer::bands(float* out, std::size_t count, float min_hz, float max_hz, bool peaks) const {
    if (!out || count == 0) return;
    const std::vector<float>& src = peaks ? m_peak_db : m_db;
    min_hz = std::max(1.0f, min_hz);
    max_hz = std::max(min_hz * 1.001f, max_hz);
    const double ratio = std::log(static_cast<double>(max_hz) / static_cast<double>(min_hz));
    const double bin_hz = static_cast<double>(m_rate) / static_cast<double>(m_size);
    const std::size_t nb = src.size();
    for (std::size_t i = 0; i < count; ++i) {
        const double lo = static_cast<double>(min_hz) * std::exp(ratio * static_cast<double>(i) / static_cast<double>(count));
        const double hi = static_cast<double>(min_hz) * std::exp(ratio * static_cast<double>(i + 1) / static_cast<double>(count));
        const std::size_t b0 = static_cast<std::size_t>(std::ceil(lo / bin_hz));
        const std::size_t b1 = static_cast<std::size_t>(std::ceil(hi / bin_hz));
        float best = -200.0f;
        bool any = false;
        for (std::size_t b = b0; b < b1 && b < nb; ++b) {
            best = std::max(best, src[b]);
            any = true;
        }
        if (!any) {
            const double centre = std::sqrt(lo * hi) / bin_hz;
            const std::size_t k = static_cast<std::size_t>(std::floor(centre));
            if (k + 1 < nb) {
                const double t = centre - static_cast<double>(k);
                best = static_cast<float>(src[k] + (src[k + 1] - src[k]) * t);
            } else if (nb) {
                best = src[nb - 1];
            }
        }
        out[i] = best;
    }
}

std::vector<float> SpectrumAnalyzer::bands(std::size_t count, float min_hz, float max_hz, bool peaks) const {
    std::vector<float> out(count, -200.0f);
    bands(out.data(), count, min_hz, max_hz, peaks);
    return out;
}

void LevelMeter::prepare(std::uint32_t sample_rate, std::size_t channels) {
    m_rate = sample_rate ? sample_rate : 48000;
    m_channels = channels;
    m_state.assign(channels, State());
    m_peak.reset(new std::atomic<float>[channels ? channels : 1]);
    m_rms.reset(new std::atomic<float>[channels ? channels : 1]);
    m_hold.reset(new std::atomic<float>[channels ? channels : 1]);
    m_clip.reset(new std::atomic<bool>[channels ? channels : 1]);
    reset();
}

void LevelMeter::reset() noexcept {
    for (std::size_t c = 0; c < m_channels; ++c) {
        m_state[c] = State();
        m_peak[c].store(0.0f, std::memory_order_relaxed);
        m_rms[c].store(0.0f, std::memory_order_relaxed);
        m_hold[c].store(0.0f, std::memory_order_relaxed);
        m_clip[c].store(false, std::memory_order_relaxed);
    }
}

void LevelMeter::push(const float* interleaved, std::size_t frames) noexcept {
    if (!interleaved || m_channels == 0 || frames == 0) return;
    const std::size_t c = m_channels;
    const double rate = static_cast<double>(m_rate);
    const double dt = static_cast<double>(frames) / rate;
    const double a = m_rms_seconds > 0.0f ? std::exp(-1.0 / (static_cast<double>(m_rms_seconds) * rate)) : 0.0;
    const float fall = std::pow(10.0f, -m_decay_db * static_cast<float>(dt) / 20.0f);
    for (std::size_t ch = 0; ch < c; ++ch) {
        State& s = m_state[ch];
        float block_peak = 0.0f;
        double ms = s.mean_square;
        for (std::size_t f = 0; f < frames; ++f) {
            const float x = interleaved[f * c + ch];
            const float ax = std::fabs(x);
            block_peak = std::max(block_peak, ax);
            ms = a * ms + (1.0 - a) * static_cast<double>(x) * static_cast<double>(x);
        }
        s.mean_square = ms < 1e-30 ? 0.0 : ms;
        s.peak = std::max(block_peak, s.peak * fall);
        if (block_peak >= s.hold) {
            s.hold = block_peak;
            s.hold_age = 0.0;
        } else {
            const double before = s.hold_age;
            s.hold_age += dt;
            const double hold = static_cast<double>(m_hold_seconds);
            if (s.hold_age > hold) {
                const double over = s.hold_age - std::max(before, hold);
                s.hold = std::max(block_peak, s.hold * std::pow(10.0f, -m_decay_db * static_cast<float>(over) / 20.0f));
            }
        }
        m_peak[ch].store(s.peak, std::memory_order_relaxed);
        m_rms[ch].store(static_cast<float>(std::sqrt(s.mean_square)), std::memory_order_relaxed);
        m_hold[ch].store(s.hold, std::memory_order_relaxed);
        if (block_peak >= 1.0f) m_clip[ch].store(true, std::memory_order_relaxed);
    }
}

float LevelMeter::peak(std::size_t channel) const noexcept {
    return channel < m_channels ? m_peak[channel].load(std::memory_order_relaxed) : 0.0f;
}

float LevelMeter::rms(std::size_t channel) const noexcept {
    return channel < m_channels ? m_rms[channel].load(std::memory_order_relaxed) : 0.0f;
}

float LevelMeter::hold(std::size_t channel) const noexcept {
    return channel < m_channels ? m_hold[channel].load(std::memory_order_relaxed) : 0.0f;
}

float LevelMeter::peak_db(std::size_t channel) const noexcept {
    return to_db(peak(channel));
}

float LevelMeter::rms_db(std::size_t channel) const noexcept {
    return to_db(rms(channel));
}

float LevelMeter::hold_db(std::size_t channel) const noexcept {
    return to_db(hold(channel));
}

bool LevelMeter::clipped(std::size_t channel) const noexcept {
    return channel < m_channels && m_clip[channel].load(std::memory_order_relaxed);
}

void LevelMeter::clear_clip() noexcept {
    for (std::size_t c = 0; c < m_channels; ++c) m_clip[c].store(false, std::memory_order_relaxed);
}

} // namespace audio
} // namespace fizmo
