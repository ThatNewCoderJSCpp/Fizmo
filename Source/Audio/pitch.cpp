#include "pitch.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const double k_two_pi = 2.0 * k_pi;

float clampf(float v, float lo, float hi) noexcept {
    return std::min(hi, std::max(lo, v));
}

double wrap_phase(double p) noexcept {
    p = std::fmod(p + k_pi, k_two_pi);
    if (p < 0.0) p += k_two_pi;
    return p - k_pi;
}

float sanitize(float v) noexcept {
    if (!(v == v) || v > 1e6f || v < -1e6f) return 0.0f;
    return v;
}

float ring_cubic(const float* ring, std::size_t mask, double pos) noexcept {
    const double fl = std::floor(pos);
    const std::int64_t i = static_cast<std::int64_t>(fl);
    const float t = static_cast<float>(pos - fl);
    const float y0 = ring[static_cast<std::size_t>(i - 1) & mask];
    const float y1 = ring[static_cast<std::size_t>(i) & mask];
    const float y2 = ring[static_cast<std::size_t>(i + 1) & mask];
    const float y3 = ring[static_cast<std::size_t>(i + 2) & mask];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

float env_at(const float* env, std::size_t last, double bin) noexcept {
    if (bin <= 0.0) return env[0];
    if (bin >= static_cast<double>(last)) return env[last];
    const std::size_t i = static_cast<std::size_t>(bin);
    const float t = static_cast<float>(bin - static_cast<double>(i));
    return env[i] + (env[i + 1] - env[i]) * t;
}

class SpectralEnvelope {
public:
    void prepare(std::size_t size, std::size_t iterations = 4) {
        m_fft.resize(size);
        m_size = m_fft.size();
        m_iterations = iterations;
        const std::size_t h = m_size / 2;
        m_log.assign(m_size, 0.0f);
        m_target.assign(h + 1, 0.0f);
        m_lifter.assign(h + 1, 0.0f);
        m_spec.assign(h + 1, std::complex<float>());
    }

    void compute(const float* mag, float* out, float cutoff) noexcept {
        const std::size_t h = m_size / 2;
        const float c = std::max(4.0f, std::min(static_cast<float>(h) * 0.9f, cutoff));
        const float knee = 0.7f * c;
        for (std::size_t q = 0; q <= h; ++q) {
            const float d = static_cast<float>(q);
            float w = 0.0f;
            if (d < knee) w = 1.0f;
            else if (d < c) w = 0.5f + 0.5f * std::cos(static_cast<float>(k_pi) * (d - knee) / (c - knee));
            m_lifter[q] = w;
        }
        for (std::size_t k = 0; k <= h; ++k) m_target[k] = std::log(mag[k] + 1e-9f);
        smooth(out);
        for (std::size_t it = 0; it < m_iterations; ++it) {
            for (std::size_t k = 0; k <= h; ++k) m_target[k] = std::max(m_target[k], out[k]);
            smooth(out);
        }
    }

private:
    void smooth(float* out) noexcept {
        const std::size_t h = m_size / 2;
        for (std::size_t k = 0; k <= h; ++k) m_log[k] = m_target[k];
        for (std::size_t k = 1; k < h; ++k) m_log[m_size - k] = m_target[k];
        m_fft.forward_real(m_log.data(), m_spec.data());
        for (std::size_t q = 0; q <= h; ++q) m_spec[q] *= m_lifter[q];
        m_fft.inverse_real(m_spec.data(), m_log.data());
        for (std::size_t k = 0; k <= h; ++k) out[k] = m_log[k];
    }

    Fft m_fft;
    std::size_t m_size = 0;
    std::size_t m_iterations = 4;
    std::vector<float> m_log;
    std::vector<float> m_target;
    std::vector<float> m_lifter;
    std::vector<std::complex<float>> m_spec;
};

float lifter_cutoff(const PitchEstimate& e, float rate) noexcept {
    if (e.voiced && e.frequency > 0.0f) return 0.7f * rate / e.frequency;
    return rate * 0.002f;
}

struct GrainRequest {
    double position = 0.0;
    float hz = 0.0f;
    bool voiced = false;
    float ratio = 1.0f;
    float formant = 1.0f;
};

class GrainPolicy {
public:
    virtual ~GrainPolicy() = default;
    virtual void decide(GrainRequest& request) noexcept = 0;
};

class Psola {
public:
    void prepare(float rate, std::size_t channels, float min_hz, float max_hz) {
        m_rate = rate;
        m_channels = std::max<std::size_t>(1, channels);
        m_pmax = static_cast<double>(rate) / static_cast<double>(min_hz);
        m_pmin = static_cast<double>(rate) / static_cast<double>(max_hz);
        m_hcap = m_pmax;
        m_delay = static_cast<std::size_t>(std::ceil(2.0 * m_pmax)) + 4;
        m_unvoiced_h = std::max(16.0, static_cast<double>(rate) * 0.005);
        m_size = next_power_of_two(2 * m_delay + 64);
        m_mask = m_size - 1;
        m_in.assign(m_channels * m_size, 0.0f);
        m_mono.assign(m_size, 0.0f);
        m_acc.assign(m_channels * m_size, 0.0f);
        m_wsum.assign(m_size, 0.0f);
        m_det.configure(rate, min_hz, max_hz, 0.15f);
        m_det.set_hop(static_cast<std::size_t>(std::max(32.0f, rate * 0.004f)));
        reset();
    }

    void set_range(float min_hz, float max_hz) noexcept { m_det.set_range(min_hz, max_hz); }

    void reset() noexcept {
        std::fill(m_in.begin(), m_in.end(), 0.0f);
        std::fill(m_mono.begin(), m_mono.end(), 0.0f);
        std::fill(m_acc.begin(), m_acc.end(), 0.0f);
        std::fill(m_wsum.begin(), m_wsum.end(), 0.0f);
        m_det.reset();
        m_n = 0;
        m_next = -static_cast<double>(m_delay);
        m_tracking = false;
        m_last = 0.0;
        m_polarity = 1.0f;
        m_mark_head = 0;
        m_mark_count = 0;
    }

    std::size_t latency() const noexcept { return m_delay; }
    PitchEstimate estimate() const noexcept { return m_det.latest(); }

    void process(float* io, std::size_t frames, GrainPolicy& policy) noexcept {
        if (m_size == 0) return;
        const std::size_t c = m_channels;
        const float inv_c = 1.0f / static_cast<float>(c);
        for (std::size_t f = 0; f < frames; ++f) {
            float* x = io + f * c;
            const std::size_t wi = static_cast<std::size_t>(m_n) & m_mask;
            float sum = 0.0f;
            for (std::size_t ch = 0; ch < c; ++ch) {
                const float v = sanitize(x[ch]);
                m_in[ch * m_size + wi] = v;
                sum += v;
            }
            const float mono = sum * inv_c;
            m_mono[wi] = mono;
            m_det.push_sample(mono);
            track();
            synthesize(policy);
            const std::size_t oi = static_cast<std::size_t>(m_n - static_cast<std::int64_t>(m_delay)) & m_mask;
            const float w = m_wsum[oi];
            const float g = w > 1.0f ? 1.0f / w : 1.0f;
            for (std::size_t ch = 0; ch < c; ++ch) {
                float& a = m_acc[ch * m_size + oi];
                x[ch] = a * g;
                a = 0.0f;
            }
            m_wsum[oi] = 0.0f;
            ++m_n;
        }
    }

private:
    struct Mark {
        double pos = 0.0;
        double period = 0.0;
    };

    float mono_at(std::int64_t i) const noexcept { return m_mono[static_cast<std::size_t>(i) & m_mask]; }

    double refine(std::int64_t i, float sign) const noexcept {
        const float y0 = sign * mono_at(i - 1);
        const float y1 = sign * mono_at(i);
        const float y2 = sign * mono_at(i + 1);
        const float d = y0 - 2.0f * y1 + y2;
        double shift = 0.0;
        if (d < -1e-12f) shift = clampf(0.5f * (y0 - y2) / d, -0.5f, 0.5f);
        return static_cast<double>(i) + shift;
    }

    void add_mark(double pos, double period) noexcept {
        m_marks[m_mark_head] = Mark{ pos, period };
        m_mark_head = (m_mark_head + 1) & 31;
        if (m_mark_count < 32) ++m_mark_count;
    }

    void track() noexcept {
        const PitchEstimate est = m_det.latest();
        if (!est.voiced || est.frequency <= 0.0f) {
            m_tracking = false;
            return;
        }
        const double p = std::min(m_pmax, std::max(m_pmin, static_cast<double>(m_rate) / static_cast<double>(est.frequency)));
        const double n = static_cast<double>(m_n);
        if (!m_tracking) {
            const std::int64_t hi = static_cast<std::int64_t>(std::floor(n - p - 3.0));
            const std::int64_t lo = static_cast<std::int64_t>(std::ceil(n - 2.0 * p - 3.0));
            if (lo < 2) return;
            std::int64_t best = hi;
            float peak = -1.0f;
            for (std::int64_t i = lo; i <= hi; ++i) {
                const float v = std::fabs(mono_at(i));
                if (v > peak) {
                    peak = v;
                    best = i;
                }
            }
            if (peak < 1e-7f) return;
            m_polarity = mono_at(best) >= 0.0f ? 1.0f : -1.0f;
            const double pos = refine(best, m_polarity);
            add_mark(pos, p);
            m_last = pos;
            m_tracking = true;
            return;
        }
        const double predicted = m_last + p;
        const double radius = p / 6.0;
        if (predicted + radius + p + 3.0 > n) return;
        const std::int64_t lo = static_cast<std::int64_t>(std::ceil(predicted - radius));
        const std::int64_t hi = static_cast<std::int64_t>(std::floor(predicted + radius));
        std::int64_t best = static_cast<std::int64_t>(std::floor(predicted + 0.5));
        float peak = -1e30f;
        for (std::int64_t i = lo; i <= hi; ++i) {
            const float v = m_polarity * mono_at(i);
            if (v > peak) {
                peak = v;
                best = i;
            }
        }
        double pos = refine(best, m_polarity);
        if (pos <= m_last + 0.5 * p) pos = predicted;
        add_mark(pos, p);
        m_last = pos;
    }

    const Mark* nearest(double s) const noexcept {
        const Mark* best = nullptr;
        double best_d = 1e300;
        for (std::size_t i = 0; i < m_mark_count; ++i) {
            const Mark& m = m_marks[(m_mark_head + 31 - i) & 31];
            const double d = std::fabs(m.pos - s);
            if (d < best_d) {
                best_d = d;
                best = &m;
            }
            if (m.pos < s - 2.0 * m_pmax) break;
        }
        if (best && best_d <= 1.25 * best->period) return best;
        return nullptr;
    }

    void add_grain(double s, double center, double h, double beta) noexcept {
        const std::int64_t t0 = static_cast<std::int64_t>(std::floor(s - h)) + 1;
        const std::int64_t t1 = static_cast<std::int64_t>(std::ceil(s + h)) - 1;
        const double step = k_pi / h;
        const std::size_t c = m_channels;
        for (std::int64_t t = t0; t <= t1; ++t) {
            const double k = static_cast<double>(t) - s;
            const float w = static_cast<float>(0.5 + 0.5 * std::cos(k * step));
            const double pos = center + k * beta;
            const std::size_t ti = static_cast<std::size_t>(t) & m_mask;
            m_wsum[ti] += w;
            for (std::size_t ch = 0; ch < c; ++ch) {
                m_acc[ch * m_size + ti] += w * ring_cubic(m_in.data() + ch * m_size, m_mask, pos);
            }
        }
    }

    void synthesize(GrainPolicy& policy) noexcept {
        const double limit = static_cast<double>(m_n - static_cast<std::int64_t>(m_delay)) + m_hcap;
        while (m_next <= limit) {
            const double s = m_next;
            GrainRequest r;
            r.position = s;
            const Mark* m = nearest(s);
            if (m) {
                r.voiced = true;
                r.hz = static_cast<float>(static_cast<double>(m_rate) / m->period);
                policy.decide(r);
                const double ratio = clampf(sanitize(r.ratio), 0.25f, 4.0f);
                const double beta = clampf(sanitize(r.formant), 0.25f, 4.0f);
                const double h = std::max(4.0, std::min(m->period / beta, m_hcap));
                add_grain(s, m->pos, h, beta);
                m_next = s + std::max(1.0, m->period / ratio);
            } else {
                policy.decide(r);
                const double beta = clampf(sanitize(r.formant), 0.25f, 4.0f);
                double h = std::min(m_unvoiced_h, std::min(m_hcap, (m_pmax - 3.0) / beta));
                h = std::max(4.0, h);
                add_grain(s, s, h, beta);
                m_next = s + h;
            }
        }
    }

    PitchDetector m_det;
    float m_rate = 48000.0f;
    std::size_t m_channels = 1;
    double m_pmax = 640.0;
    double m_pmin = 48.0;
    double m_hcap = 640.0;
    double m_unvoiced_h = 240.0;
    std::size_t m_delay = 0;
    std::size_t m_size = 0;
    std::size_t m_mask = 0;
    std::vector<float> m_in;
    std::vector<float> m_mono;
    std::vector<float> m_acc;
    std::vector<float> m_wsum;
    std::int64_t m_n = 0;
    double m_next = 0.0;
    bool m_tracking = false;
    double m_last = 0.0;
    float m_polarity = 1.0f;
    Mark m_marks[32];
    std::size_t m_mark_head = 0;
    std::size_t m_mark_count = 0;
};

class StftEngine {
public:
    void prepare(float rate, std::size_t channels, double seconds) {
        m_channels = std::max<std::size_t>(1, channels);
        m_size = std::max<std::size_t>(256, next_power_of_two(static_cast<std::size_t>(rate * seconds)));
        m_hop = m_size / 4;
        m_bins = m_size / 2 + 1;
        m_fft.resize(m_size);
        m_window = make_window(WindowType::hann, m_size, true);
        m_in.assign(m_channels * m_size, 0.0f);
        m_out.assign(m_channels * m_hop, 0.0f);
        m_accum.assign(m_channels * m_size, 0.0f);
        m_frame.assign(m_size, 0.0f);
        m_x.assign(m_channels * m_bins, std::complex<float>());
        m_y.assign(m_channels * m_bins, std::complex<float>());
        m_mag.assign(m_bins, 0.0f);
        reset_stream();
    }

    void reset_stream() noexcept {
        std::fill(m_in.begin(), m_in.end(), 0.0f);
        std::fill(m_out.begin(), m_out.end(), 0.0f);
        std::fill(m_accum.begin(), m_accum.end(), 0.0f);
        m_rover = m_size - m_hop;
    }

    std::size_t latency() const noexcept { return m_size; }
    std::size_t size() const noexcept { return m_size; }
    std::size_t hop() const noexcept { return m_hop; }
    std::size_t bins() const noexcept { return m_bins; }
    std::size_t channels() const noexcept { return m_channels; }

    template <class Frame>
    void run(float* io, std::size_t frames, Frame&& frame) noexcept {
        if (m_size == 0) return;
        const std::size_t c = m_channels;
        const std::size_t lat = m_size - m_hop;
        for (std::size_t f = 0; f < frames; ++f) {
            float* x = io + f * c;
            for (std::size_t ch = 0; ch < c; ++ch) {
                m_in[ch * m_size + m_rover] = sanitize(x[ch]);
                x[ch] = m_out[ch * m_hop + (m_rover - lat)];
            }
            if (++m_rover >= m_size) {
                analyze();
                frame();
                synthesize();
                m_rover = lat;
            }
        }
    }

    void analyze() noexcept {
        for (std::size_t ch = 0; ch < m_channels; ++ch) {
            const float* in = m_in.data() + ch * m_size;
            for (std::size_t i = 0; i < m_size; ++i) m_frame[i] = in[i] * m_window[i];
            m_fft.forward_real(m_frame.data(), m_x.data() + ch * m_bins);
        }
        const float inv = 1.0f / static_cast<float>(m_channels);
        for (std::size_t k = 0; k < m_bins; ++k) {
            float p = 0.0f;
            for (std::size_t ch = 0; ch < m_channels; ++ch) p += std::norm(m_x[ch * m_bins + k]);
            m_mag[k] = std::sqrt(p * inv);
        }
    }

    void synthesize() noexcept {
        const float scale = 2.0f / 3.0f;
        for (std::size_t ch = 0; ch < m_channels; ++ch) {
            m_fft.inverse_real(m_y.data() + ch * m_bins, m_frame.data());
            float* acc = m_accum.data() + ch * m_size;
            for (std::size_t i = 0; i < m_size; ++i) acc[i] += m_frame[i] * m_window[i] * scale;
            float* out = m_out.data() + ch * m_hop;
            for (std::size_t i = 0; i < m_hop; ++i) {
                const float v = acc[i];
                out[i] = std::fabs(v) < 1e-20f ? 0.0f : v;
            }
            std::memmove(acc, acc + m_hop, (m_size - m_hop) * sizeof(float));
            std::fill(acc + (m_size - m_hop), acc + m_size, 0.0f);
            float* in = m_in.data() + ch * m_size;
            std::memmove(in, in + m_hop, (m_size - m_hop) * sizeof(float));
        }
    }

    std::complex<float>* x(std::size_t ch) noexcept { return m_x.data() + ch * m_bins; }
    std::complex<float>* y(std::size_t ch) noexcept { return m_y.data() + ch * m_bins; }
    const float* mag() const noexcept { return m_mag.data(); }

private:
    Fft m_fft;
    std::size_t m_channels = 1;
    std::size_t m_size = 0;
    std::size_t m_hop = 0;
    std::size_t m_bins = 0;
    std::size_t m_rover = 0;
    std::vector<float> m_window;
    std::vector<float> m_in;
    std::vector<float> m_out;
    std::vector<float> m_accum;
    std::vector<float> m_frame;
    std::vector<std::complex<float>> m_x;
    std::vector<std::complex<float>> m_y;
    std::vector<float> m_mag;
};

class PhaseVocoder {
public:
    void prepare(float rate, std::size_t channels) {
        m_stft.prepare(rate, channels, 0.04);
        const std::size_t b = m_stft.bins();
        m_mono.assign(b, std::complex<float>());
        m_phase.assign(b, 0.0f);
        m_prev_phase.assign(b, 0.0f);
        m_omega.assign(b, 0.0f);
        m_env.assign(b, 0.0f);
        m_peaks.assign(b, 0);
        m_bounds.assign(b + 1, 0);
        m_stamp_a.assign(b, 0);
        m_stamp_b.assign(b, 0);
        m_psi_a.assign(b, 0.0f);
        m_psi_b.assign(b, 0.0f);
        m_envelope.prepare(m_stft.size());
        m_rate = rate;
        m_det.configure(rate, 60.0f, 1000.0f, 0.15f);
        reset();
    }

    void reset() noexcept {
        m_stft.reset_stream();
        m_det.reset();
        std::fill(m_prev_phase.begin(), m_prev_phase.end(), 0.0f);
        std::fill(m_stamp_a.begin(), m_stamp_a.end(), 0u);
        std::fill(m_stamp_b.begin(), m_stamp_b.end(), 0u);
        m_frame_id = 2;
        m_prev_hfc = 0.0;
        m_flip = false;
    }

    std::size_t latency() const noexcept { return m_stft.latency(); }

    void process(float* io, std::size_t frames, float ratio, float env_ratio) noexcept {
        m_ratio = ratio;
        m_env_ratio = env_ratio;
        if (std::fabs(env_ratio - ratio) > 1e-4f) {
            const std::size_t c = m_stft.channels();
            const float inv = 1.0f / static_cast<float>(c);
            for (std::size_t f = 0; f < frames; ++f) {
                float sum = 0.0f;
                for (std::size_t ch = 0; ch < c; ++ch) sum += sanitize(io[f * c + ch]);
                m_det.push_sample(sum * inv);
            }
        }
        m_stft.run(io, frames, [this]() noexcept { frame(); });
    }

private:
    void frame() noexcept {
        const std::size_t n = m_stft.size();
        const std::size_t b = m_stft.bins();
        const std::size_t last = b - 1;
        const std::size_t c = m_stft.channels();
        const float* mag = m_stft.mag();
        const double expected = k_two_pi * static_cast<double>(m_stft.hop()) / static_cast<double>(n);
        double hfc = 0.0;
        float peak_mag = 0.0f;
        for (std::size_t k = 0; k < b; ++k) {
            std::complex<float> sum(0.0f, 0.0f);
            float power = 0.0f;
            for (std::size_t ch = 0; ch < c; ++ch) {
                sum += m_stft.x(ch)[k];
                power += std::norm(m_stft.x(ch)[k]);
            }
            if (std::norm(sum) < 0.01f * power) sum = m_stft.x(0)[k];
            m_mono[k] = sum;
            const float ph = std::arg(sum);
            m_phase[k] = ph;
            const double d = wrap_phase(static_cast<double>(ph) - static_cast<double>(m_prev_phase[k]) - expected * static_cast<double>(k));
            m_omega[k] = static_cast<float>(static_cast<double>(k) + d / expected);
            m_prev_phase[k] = ph;
            hfc += static_cast<double>(k) * static_cast<double>(mag[k]) * static_cast<double>(mag[k]);
            peak_mag = std::max(peak_mag, mag[k]);
        }
        const bool transient = hfc > 4.0 * m_prev_hfc && hfc > 1e-6;
        m_prev_hfc = hfc;
        const std::uint32_t id = m_frame_id++;
        m_flip = !m_flip;
        std::uint32_t* cur_stamp = m_flip ? m_stamp_a.data() : m_stamp_b.data();
        float* cur_psi = m_flip ? m_psi_a.data() : m_psi_b.data();
        const std::uint32_t* prev_stamp = m_flip ? m_stamp_b.data() : m_stamp_a.data();
        const float* prev_psi = m_flip ? m_psi_b.data() : m_psi_a.data();
        const float ratio = m_ratio;
        const bool identity = std::fabs(ratio - 1.0f) < 1e-5f && std::fabs(m_env_ratio - 1.0f) < 1e-5f;
        if (identity) {
            for (std::size_t ch = 0; ch < c; ++ch) std::copy(m_stft.x(ch), m_stft.x(ch) + b, m_stft.y(ch));
            return;
        }
        for (std::size_t ch = 0; ch < c; ++ch) std::fill(m_stft.y(ch), m_stft.y(ch) + b, std::complex<float>());
        const bool use_env = std::fabs(m_env_ratio - ratio) > 1e-4f;
        if (use_env) m_envelope.compute(mag, m_env.data(), lifter_cutoff(m_det.latest(), m_rate));
        const float floor = std::max(1e-7f, peak_mag * 1e-5f);
        std::size_t count = 0;
        for (std::size_t k = 1; k < last; ++k) {
            const float m = mag[k];
            if (m <= floor || m <= mag[k - 1] || m < mag[k + 1]) continue;
            if (k >= 2 && m <= mag[k - 2]) continue;
            if (k + 2 <= last && m < mag[k + 2]) continue;
            m_peaks[count++] = k;
        }
        if (count == 0) return;
        m_bounds[0] = 0;
        for (std::size_t i = 1; i < count; ++i) {
            std::size_t lo = m_peaks[i - 1];
            float best = mag[lo];
            for (std::size_t k = m_peaks[i - 1]; k <= m_peaks[i]; ++k) {
                if (mag[k] < best) {
                    best = mag[k];
                    lo = k;
                }
            }
            m_bounds[i] = lo;
        }
        m_bounds[count] = b;
        const double inv_env = 1.0 / static_cast<double>(m_env_ratio);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t p = m_peaks[i];
            const double target = static_cast<double>(m_omega[p]) * static_cast<double>(ratio);
            const std::int64_t q = static_cast<std::int64_t>(std::floor(target + 0.5));
            if (q < 1 || q >= static_cast<std::int64_t>(last)) continue;
            const std::size_t uq = static_cast<std::size_t>(q);
            double psi = static_cast<double>(m_phase[p]);
            if (!transient) {
                const std::size_t cand[5] = { uq, uq - 1, uq + 1, uq - 2, uq + 2 };
                for (std::size_t j = 0; j < 5; ++j) {
                    const std::size_t qq = cand[j];
                    if (qq >= b) continue;
                    if (prev_stamp[qq] + 1 == id) {
                        psi = static_cast<double>(prev_psi[qq]) + expected * target;
                        break;
                    }
                }
            }
            psi = wrap_phase(psi);
            cur_stamp[uq] = id;
            cur_psi[uq] = static_cast<float>(psi);
            const double r = psi - static_cast<double>(m_phase[p]);
            const std::complex<float> rot(static_cast<float>(std::cos(r)), static_cast<float>(std::sin(r)));
            const std::int64_t delta = q - static_cast<std::int64_t>(p);
            const std::size_t lo = m_bounds[i];
            const std::size_t hi = m_bounds[i + 1];
            for (std::size_t k = lo; k < hi; ++k) {
                const std::int64_t kk = static_cast<std::int64_t>(k) + delta;
                if (kk < 0 || kk > static_cast<std::int64_t>(last)) continue;
                const std::size_t ukk = static_cast<std::size_t>(kk);
                std::complex<float> g = rot;
                if (use_env) {
                    const float e = env_at(m_env.data(), last, static_cast<double>(kk) * inv_env) - m_env[k];
                    g *= std::exp(clampf(e, -7.0f, 2.8f));
                }
                for (std::size_t ch = 0; ch < c; ++ch) m_stft.y(ch)[ukk] += m_stft.x(ch)[k] * g;
            }
        }
    }

    StftEngine m_stft;
    SpectralEnvelope m_envelope;
    PitchDetector m_det;
    float m_rate = 48000.0f;
    std::vector<std::complex<float>> m_mono;
    std::vector<float> m_phase;
    std::vector<float> m_prev_phase;
    std::vector<float> m_omega;
    std::vector<float> m_env;
    std::vector<std::size_t> m_peaks;
    std::vector<std::size_t> m_bounds;
    std::vector<std::uint32_t> m_stamp_a;
    std::vector<std::uint32_t> m_stamp_b;
    std::vector<float> m_psi_a;
    std::vector<float> m_psi_b;
    std::uint32_t m_frame_id = 2;
    double m_prev_hfc = 0.0;
    bool m_flip = false;
    float m_ratio = 1.0f;
    float m_env_ratio = 1.0f;
};

float semitone_ratio(float semitones) noexcept {
    return static_cast<float>(std::pow(2.0, static_cast<double>(semitones) / 12.0));
}

} // namespace

PitchDetector::PitchDetector(float sample_rate, float min_hz, float max_hz, float threshold) {
    configure(sample_rate, min_hz, max_hz, threshold);
}

void PitchDetector::configure(float sample_rate, float min_hz, float max_hz, float threshold) {
    m_rate = sample_rate > 0.0f ? sample_rate : 48000.0f;
    const float top = m_rate * 0.25f;
    m_config_min = std::min(top / 1.5f, std::max(20.0f, min_hz == min_hz ? min_hz : 60.0f));
    m_config_max = std::min(top, std::max(m_config_min * 1.5f, max_hz == max_hz ? max_hz : 1200.0f));
    m_max_lag = static_cast<std::size_t>(std::ceil(m_rate / m_config_min)) + 2;
    m_window = m_max_lag;
    m_span = m_window + m_max_lag;
    m_fft.resize(next_power_of_two(m_span));
    const std::size_t n = m_fft.size();
    m_ring.assign(2 * m_span, 0.0f);
    m_a.assign(n, 0.0f);
    m_b.assign(n, 0.0f);
    m_corr.assign(n, 0.0f);
    m_scratch.assign(m_span, 0.0f);
    m_prefix.assign(m_span + 1, 0.0);
    m_cmnd.assign(m_max_lag + 2, 1.0f);
    m_spec_a.assign(n / 2 + 1, std::complex<float>());
    m_spec_b.assign(n / 2 + 1, std::complex<float>());
    m_hop = std::max<std::size_t>(1, static_cast<std::size_t>(m_rate * 0.005f));
    set_range(min_hz, max_hz);
    set_threshold(threshold);
    reset();
}

void PitchDetector::set_range(float min_hz, float max_hz) noexcept {
    if (!(min_hz == min_hz)) min_hz = m_config_min;
    if (!(max_hz == max_hz)) max_hz = m_config_max;
    m_min_hz = std::min(m_config_max / 1.2f, std::max(m_config_min, min_hz));
    m_max_hz = std::min(m_config_max, std::max(m_min_hz * 1.2f, max_hz));
}

void PitchDetector::set_threshold(float threshold) noexcept {
    m_threshold = clampf(threshold == threshold ? threshold : 0.15f, 0.01f, 0.5f);
}

void PitchDetector::set_hop(std::size_t frames) noexcept {
    m_hop = std::max<std::size_t>(1, frames);
}

void PitchDetector::reset() noexcept {
    std::fill(m_ring.begin(), m_ring.end(), 0.0f);
    m_write = 0;
    m_count = 0;
    m_latest = PitchEstimate();
}

bool PitchDetector::push_sample(float sample) noexcept {
    if (m_span == 0) return false;
    m_ring[m_write] = sample;
    m_ring[m_write + m_span] = sample;
    if (++m_write == m_span) m_write = 0;
    if (++m_count >= m_hop) {
        m_count = 0;
        m_latest = run(m_ring.data() + m_write);
        return true;
    }
    return false;
}

PitchEstimate PitchDetector::push(const float* mono, std::size_t frames) noexcept {
    for (std::size_t i = 0; i < frames; ++i) push_sample(mono[i]);
    return m_latest;
}

PitchEstimate PitchDetector::analyze(const float* mono, std::size_t frames) noexcept {
    if (m_span == 0) return PitchEstimate();
    const float* w = nullptr;
    if (frames >= m_span) {
        w = mono + (frames - m_span);
    } else {
        std::fill(m_scratch.begin(), m_scratch.end(), 0.0f);
        std::copy(mono, mono + frames, m_scratch.begin() + static_cast<std::ptrdiff_t>(m_span - frames));
        w = m_scratch.data();
    }
    m_latest = run(w);
    return m_latest;
}

PitchEstimate PitchDetector::run(const float* x) noexcept {
    PitchEstimate result;
    const std::size_t w = m_window;
    const std::size_t span = m_span;
    m_prefix[0] = 0.0;
    for (std::size_t i = 0; i < span; ++i) {
        const double v = static_cast<double>(x[i]);
        m_prefix[i + 1] = m_prefix[i] + v * v;
    }
    if (!(m_prefix[span] / static_cast<double>(span) > 1e-10)) return result;
    std::fill(m_a.begin(), m_a.end(), 0.0f);
    std::fill(m_b.begin(), m_b.end(), 0.0f);
    std::copy(x, x + w, m_a.begin());
    std::copy(x, x + span, m_b.begin());
    m_fft.forward_real(m_a.data(), m_spec_a.data());
    m_fft.forward_real(m_b.data(), m_spec_b.data());
    for (std::size_t k = 0; k < m_spec_a.size(); ++k) m_spec_a[k] = std::conj(m_spec_a[k]) * m_spec_b[k];
    m_fft.inverse_real(m_spec_a.data(), m_corr.data());
    const std::size_t tau_min = std::max<std::size_t>(2, static_cast<std::size_t>(std::floor(m_rate / m_max_hz)));
    const std::size_t tau_max = std::min(m_max_lag - 1, static_cast<std::size_t>(std::ceil(m_rate / m_min_hz)));
    const std::size_t tau_end = std::min(m_max_lag, tau_max + 1);
    const double e0 = m_prefix[w];
    double running = 0.0;
    m_cmnd[0] = 1.0f;
    for (std::size_t tau = 1; tau <= tau_end; ++tau) {
        const double et = m_prefix[tau + w] - m_prefix[tau];
        const double d = std::max(0.0, e0 + et - 2.0 * static_cast<double>(m_corr[tau]));
        running += d;
        m_cmnd[tau] = running > 0.0 ? static_cast<float>(d * static_cast<double>(tau) / running) : 1.0f;
    }
    std::size_t best = 0;
    bool found = false;
    for (std::size_t tau = tau_min; tau <= tau_max; ++tau) {
        if (m_cmnd[tau] < m_threshold) {
            while (tau + 1 <= tau_end && m_cmnd[tau + 1] < m_cmnd[tau]) ++tau;
            best = tau;
            found = true;
            break;
        }
    }
    if (!found) {
        best = tau_min;
        for (std::size_t tau = tau_min; tau <= tau_max; ++tau) {
            if (m_cmnd[tau] < m_cmnd[best]) best = tau;
        }
    }
    double tau_f = static_cast<double>(best);
    if (best >= 1 && best + 1 <= tau_end) {
        const double y0 = m_cmnd[best - 1];
        const double y1 = m_cmnd[best];
        const double y2 = m_cmnd[best + 1];
        const double d = y0 - 2.0 * y1 + y2;
        if (d > 1e-12) tau_f += std::min(1.0, std::max(-1.0, 0.5 * (y0 - y2) / d));
    }
    const float score = m_cmnd[best];
    result.confidence = clampf(1.0f - score, 0.0f, 1.0f);
    result.frequency = static_cast<float>(static_cast<double>(m_rate) / tau_f);
    result.voiced = score < 2.0f * m_threshold && (found || best < tau_max);
    return result;
}

float detect_pitch(const float* mono, std::size_t frames, float sample_rate) {
    return detect_pitch(mono, frames, sample_rate, 50.0f, 1500.0f, nullptr);
}

float detect_pitch(const float* mono, std::size_t frames, float sample_rate, float min_hz, float max_hz, float* confidence) {
    if (confidence) *confidence = 0.0f;
    if (!mono || frames == 0 || !(sample_rate > 0.0f)) return 0.0f;
    PitchDetector d(sample_rate, min_hz, max_hz);
    const std::size_t span = d.window_frames();
    std::vector<float> freqs;
    std::vector<float> confs;
    float best_conf = 0.0f;
    if (frames <= span) {
        const PitchEstimate e = d.analyze(mono, frames);
        best_conf = e.confidence;
        if (e.voiced) {
            freqs.push_back(e.frequency);
            confs.push_back(e.confidence);
        }
    } else {
        const std::size_t hop = std::max<std::size_t>(1, span / 4);
        for (std::size_t start = 0; start + span <= frames; start += hop) {
            const PitchEstimate e = d.analyze(mono + start, span);
            best_conf = std::max(best_conf, e.confidence);
            if (e.voiced) {
                freqs.push_back(e.frequency);
                confs.push_back(e.confidence);
            }
        }
    }
    if (freqs.empty()) {
        if (confidence) *confidence = best_conf;
        return 0.0f;
    }
    std::vector<float> sorted = freqs;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2), sorted.end());
    if (confidence) {
        std::nth_element(confs.begin(), confs.begin() + static_cast<std::ptrdiff_t>(confs.size() / 2), confs.end());
        *confidence = confs[confs.size() / 2];
    }
    return sorted[sorted.size() / 2];
}

struct PitchShifter::State : GrainPolicy {
    Psola psola;
    PhaseVocoder vocoder;
    int active_mode = 0;
    float ratio = 1.0f;
    float formant = 1.0f;
    bool preserve = false;
    std::atomic<float> detected{ 0.0f };

    void decide(GrainRequest& r) noexcept override {
        r.ratio = ratio;
        r.formant = preserve ? formant : ratio * formant;
    }
};

PitchShifter::PitchShifter(float initial_semitones, float initial_mode)
    : semitones("semitones", initial_semitones, -24.0f, 24.0f, "st"),
      cents("cents", 0.0f, -100.0f, 100.0f, "ct"),
      mode("mode", initial_mode, 0.0f, 1.0f),
      preserve_formants("preserve_formants", 0.0f, 0.0f, 1.0f),
      formant("formant", 0.0f, -12.0f, 12.0f, "st") {
    add_parameter(semitones);
    add_parameter(cents);
    add_parameter(mode);
    add_parameter(preserve_formants);
    add_parameter(formant);
}

PitchShifter::~PitchShifter() = default;

void PitchShifter::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t rate = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(rate, c);
    if (!m_state) m_state.reset(new State());
    m_state->psola.prepare(static_cast<float>(rate), c, 75.0f, 1000.0f);
    m_state->vocoder.prepare(static_cast<float>(rate), c);
    reset();
}

void PitchShifter::process(float* interleaved, std::size_t frames) noexcept {
    if (!m_state || !interleaved) return;
    State& s = *m_state;
    const int m = mode.get() >= 0.5f ? 1 : 0;
    if (m != s.active_mode) {
        if (m == 0) s.psola.reset();
        else s.vocoder.reset();
        s.active_mode = m;
    }
    s.ratio = semitone_ratio(semitones.get() + cents.get() * 0.01f);
    s.formant = semitone_ratio(formant.get());
    s.preserve = preserve_formants.get() >= 0.5f;
    if (m == 0) {
        s.psola.process(interleaved, frames, s);
        const PitchEstimate e = s.psola.estimate();
        s.detected.store(e.voiced ? e.frequency : 0.0f, std::memory_order_relaxed);
    } else {
        s.vocoder.process(interleaved, frames, s.ratio, s.preserve ? s.formant : s.ratio * s.formant);
    }
}

void PitchShifter::reset() noexcept {
    if (!m_state) return;
    m_state->psola.reset();
    m_state->vocoder.reset();
    m_state->active_mode = mode.get() >= 0.5f ? 1 : 0;
    m_state->detected.store(0.0f, std::memory_order_relaxed);
}

std::size_t PitchShifter::latency_frames() const noexcept {
    if (!m_state) return 0;
    return mode.get() >= 0.5f ? m_state->vocoder.latency() : m_state->psola.latency();
}

float PitchShifter::detected_hz() const noexcept {
    return m_state ? m_state->detected.load(std::memory_order_relaxed) : 0.0f;
}

struct FormantShifter::State {
    StftEngine stft;
    SpectralEnvelope envelope;
    PitchDetector det;
    std::vector<float> env;
    float rate = 48000.0f;
    float alpha = 1.0f;

    void frame() noexcept {
        const std::size_t b = stft.bins();
        const std::size_t last = b - 1;
        const std::size_t c = stft.channels();
        if (std::fabs(alpha - 1.0f) < 1e-4f) {
            for (std::size_t ch = 0; ch < c; ++ch) std::copy(stft.x(ch), stft.x(ch) + b, stft.y(ch));
            return;
        }
        envelope.compute(stft.mag(), env.data(), lifter_cutoff(det.latest(), rate));
        const double inv = 1.0 / static_cast<double>(alpha);
        for (std::size_t k = 0; k < b; ++k) {
            const float e = env_at(env.data(), last, static_cast<double>(k) * inv) - env[k];
            const float g = std::exp(clampf(e, -7.0f, 2.8f));
            for (std::size_t ch = 0; ch < c; ++ch) stft.y(ch)[k] = stft.x(ch)[k] * g;
        }
    }
};

FormantShifter::FormantShifter(float semitones) : shift("shift", semitones, -12.0f, 12.0f, "st") {
    add_parameter(shift);
}

FormantShifter::~FormantShifter() = default;

void FormantShifter::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t rate = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(rate, c);
    if (!m_state) m_state.reset(new State());
    m_state->stft.prepare(static_cast<float>(rate), c, 0.02);
    m_state->envelope.prepare(m_state->stft.size());
    m_state->det.configure(static_cast<float>(rate), 60.0f, 1000.0f, 0.15f);
    m_state->rate = static_cast<float>(rate);
    m_state->env.assign(m_state->stft.bins(), 0.0f);
    reset();
}

void FormantShifter::process(float* interleaved, std::size_t frames) noexcept {
    if (!m_state || !interleaved) return;
    State& s = *m_state;
    s.alpha = semitone_ratio(shift.get());
    const std::size_t c = s.stft.channels();
    const float inv = 1.0f / static_cast<float>(c);
    for (std::size_t f = 0; f < frames; ++f) {
        float sum = 0.0f;
        for (std::size_t ch = 0; ch < c; ++ch) sum += sanitize(interleaved[f * c + ch]);
        s.det.push_sample(sum * inv);
    }
    s.stft.run(interleaved, frames, [&s]() noexcept { s.frame(); });
}

void FormantShifter::reset() noexcept {
    if (!m_state) return;
    m_state->stft.reset_stream();
    m_state->det.reset();
}

std::size_t FormantShifter::latency_frames() const noexcept {
    return m_state ? m_state->stft.latency() : 0;
}

unsigned scale_mask(MusicalScale scale) noexcept {
    switch (scale) {
        case MusicalScale::chromatic: return 0xFFFu;
        case MusicalScale::major: return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11);
        case MusicalScale::minor: return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10);
        case MusicalScale::harmonic_minor: return (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 11);
        case MusicalScale::pentatonic_major: return (1u << 0) | (1u << 2) | (1u << 4) | (1u << 7) | (1u << 9);
        case MusicalScale::pentatonic_minor: return (1u << 0) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 10);
        case MusicalScale::blues: return (1u << 0) | (1u << 3) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 10);
        case MusicalScale::custom: return 0xFFFu;
    }
    return 0xFFFu;
}

struct AutoTune::State : GrainPolicy {
    AutoTune* owner = nullptr;
    Psola psola;
    float rate = 48000.0f;
    float prepared_min = 75.0f;
    unsigned mask = 0xFFFu;
    int key = 0;
    float retune = 0.0f;
    float humanize = 0.0f;
    bool preserve = true;
    float reference = 440.0f;
    float lo = 75.0f;
    float hi = 1000.0f;
    bool active = false;
    int target = 0;
    double last_pos = 0.0;
    double held = 0.0;
    double correction = 0.0;

    bool allowed(int note) const noexcept {
        const int pc = ((note - key) % 12 + 12) % 12;
        return ((mask >> pc) & 1u) != 0;
    }

    int nearest(double midi) const noexcept {
        const int base = static_cast<int>(std::floor(midi));
        int best = static_cast<int>(std::floor(midi + 0.5));
        double best_d = 1e9;
        for (int n = base - 6; n <= base + 7; ++n) {
            if (!allowed(n)) continue;
            const double d = std::fabs(static_cast<double>(n) - midi);
            if (d < best_d) {
                best_d = d;
                best = n;
            }
        }
        return best;
    }

    void decide(GrainRequest& r) noexcept override {
        if (!r.voiced || r.hz < lo || r.hz > hi) {
            r.ratio = 1.0f;
            r.formant = 1.0f;
            active = false;
            owner->m_voiced.store(false, std::memory_order_relaxed);
            return;
        }
        const double midi = 69.0 + 12.0 * std::log2(static_cast<double>(r.hz) / static_cast<double>(reference));
        const bool onset = !active;
        if (onset || !allowed(target) || std::fabs(midi - static_cast<double>(target)) > 0.62) {
            const int next = nearest(midi);
            if (onset || next != target) held = 0.0;
            target = next;
        }
        const double dt = onset ? 0.0 : std::max(0.0, r.position - last_pos);
        last_pos = r.position;
        held += dt;
        const double desired = static_cast<double>(target) - midi;
        const double sustain = std::min(1.0, held / (0.25 * static_cast<double>(rate)));
        const double tau = static_cast<double>(retune) * 0.001 * static_cast<double>(rate) + static_cast<double>(humanize) * sustain * 0.2 * static_cast<double>(rate);
        if (onset || tau < 1.0) correction = desired;
        else correction += (desired - correction) * (1.0 - std::exp(-dt / tau));
        active = true;
        const float ratio = static_cast<float>(std::pow(2.0, correction / 12.0));
        r.ratio = ratio;
        r.formant = preserve ? 1.0f : ratio;
        owner->m_voiced.store(true, std::memory_order_relaxed);
        owner->m_detected_hz.store(r.hz, std::memory_order_relaxed);
        owner->m_detected_note.store(static_cast<float>(midi), std::memory_order_relaxed);
        owner->m_target_note.store(static_cast<float>(target), std::memory_order_relaxed);
        owner->m_correction.store(static_cast<float>(correction * 100.0), std::memory_order_relaxed);
    }
};

AutoTune::AutoTune()
    : key("key", 0.0f, 0.0f, 11.0f),
      scale("scale", 0.0f, 0.0f, 7.0f),
      mask("mask", 4095.0f, 0.0f, 4095.0f),
      retune_ms("retune_ms", 0.0f, 0.0f, 1000.0f, "ms"),
      humanize("humanize", 0.0f, 0.0f, 1.0f),
      preserve_formants("preserve_formants", 1.0f, 0.0f, 1.0f),
      min_hz("min_hz", 75.0f, 50.0f, 1000.0f, "Hz"),
      max_hz("max_hz", 1000.0f, 100.0f, 2000.0f, "Hz"),
      reference_hz("reference_hz", 440.0f, 400.0f, 480.0f, "Hz") {
    add_parameter(key);
    add_parameter(scale);
    add_parameter(mask);
    add_parameter(retune_ms);
    add_parameter(humanize);
    add_parameter(preserve_formants);
    add_parameter(min_hz);
    add_parameter(max_hz);
    add_parameter(reference_hz);
}

AutoTune::~AutoTune() = default;

void AutoTune::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t rate = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(rate, c);
    if (!m_state) m_state.reset(new State());
    m_state->owner = this;
    m_state->rate = static_cast<float>(rate);
    m_state->prepared_min = min_hz.get();
    m_state->psola.prepare(static_cast<float>(rate), c, m_state->prepared_min, 2000.0f);
    reset();
}

void AutoTune::process(float* interleaved, std::size_t frames) noexcept {
    if (!m_state || !interleaved) return;
    State& s = *m_state;
    const int sc = static_cast<int>(scale.get() + 0.5f);
    s.mask = sc >= 7 ? (static_cast<unsigned>(mask.get() + 0.5f) & 0xFFFu) : scale_mask(static_cast<MusicalScale>(sc));
    if (s.mask == 0) s.mask = 0xFFFu;
    s.key = static_cast<int>(key.get() + 0.5f) % 12;
    s.retune = retune_ms.get();
    s.humanize = humanize.get();
    s.preserve = preserve_formants.get() >= 0.5f;
    s.reference = reference_hz.get();
    s.lo = std::max(min_hz.get(), s.prepared_min);
    s.hi = std::max(s.lo * 1.2f, max_hz.get());
    s.psola.set_range(s.lo, s.hi);
    s.psola.process(interleaved, frames, s);
}

void AutoTune::reset() noexcept {
    if (!m_state) return;
    m_state->psola.reset();
    m_state->active = false;
    m_state->correction = 0.0;
    m_state->held = 0.0;
    m_voiced.store(false, std::memory_order_relaxed);
}

std::size_t AutoTune::latency_frames() const noexcept {
    return m_state ? m_state->psola.latency() : 0;
}

} // namespace audio
} // namespace fizmo
