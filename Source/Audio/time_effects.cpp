#include "time_effects.hpp"
#include "fft.hpp"
#include "format.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;

float clampf(float v, float lo, float hi) noexcept {
    return std::min(hi, std::max(lo, v));
}

float flush(float v) noexcept {
    return (v < 1e-20f && v > -1e-20f) || !(v == v) ? 0.0f : v;
}

float smoothing(float rate, float ms) noexcept {
    return 1.0f - std::exp(-1.0f / (0.001f * ms * rate));
}

void mix_law(float mix, float& dry, float& wet) noexcept {
    dry = mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix);
    wet = mix <= 0.5f ? 2.0f * mix : 1.0f;
}

struct TeCoeffs {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

TeCoeffs design_pass(bool highpass, double rate, double freq, double q) noexcept {
    TeCoeffs c;
    const double f = std::min(rate * 0.49, std::max(1.0, freq));
    const double w = 2.0 * k_pi * f / rate;
    const double cw = std::cos(w);
    const double alpha = std::sin(w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    const double b1 = highpass ? -(1.0 + cw) : (1.0 - cw);
    const double b0 = highpass ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5;
    c.b0 = static_cast<float>(b0 / a0);
    c.b1 = static_cast<float>(b1 / a0);
    c.b2 = static_cast<float>(b0 / a0);
    c.a1 = static_cast<float>(-2.0 * cw / a0);
    c.a2 = static_cast<float>((1.0 - alpha) / a0);
    return c;
}

struct TeSection {
    float z1 = 0.0f;
    float z2 = 0.0f;
    float run(const TeCoeffs& c, float x) noexcept {
        const float y = c.b0 * x + z1;
        z1 = flush(c.b1 * x - c.a1 * y + z2);
        z2 = flush(c.b2 * x - c.a2 * y);
        return y;
    }
};

const char* const k_note_names[] = { "1/1", "1/2", "1/2.", "1/2t", "1/4", "1/4.", "1/4t", "1/8", "1/8.", "1/8t", "1/16", "1/16.", "1/16t", "1/32" };
const float k_note_beats[] = { 4.0f, 2.0f, 3.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f, 0.125f };
const std::size_t k_note_count = sizeof(k_note_beats) / sizeof(k_note_beats[0]);

double bessel_i0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 64; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < sum * 1e-12) break;
    }
    return sum;
}

} // namespace

std::size_t delay_note_count() noexcept {
    return k_note_count;
}

const char* delay_note_name(std::size_t index) noexcept {
    return index < k_note_count ? k_note_names[index] : "";
}

float delay_note_beats(std::size_t index) noexcept {
    return index < k_note_count ? k_note_beats[index] : 1.0f;
}

struct Delay::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    std::vector<float> buf;
    std::size_t size = 0;
    std::size_t mask = 0;
    std::size_t pos = 0;
    float delay = 1.0f;
    float k_time = 0.001f;
    float k_param = 0.01f;
    float feedback = 0.0f;
    float mix = 0.0f;
    float cached_low = -1.0f;
    float cached_high = -1.0f;
    TeCoeffs lp;
    TeCoeffs hp;
    std::vector<TeSection> lps;
    std::vector<TeSection> hps;
    std::vector<float> taps;
    std::vector<float> fb;

    float read(std::size_t ch, float d) const noexcept {
        const double p = static_cast<double>(pos) - static_cast<double>(d);
        const double fl = std::floor(p);
        const std::int64_t i = static_cast<std::int64_t>(fl);
        const float t = static_cast<float>(p - fl);
        const float* b = buf.data() + ch * size;
        const float y0 = b[static_cast<std::size_t>(i - 1) & mask];
        const float y1 = b[static_cast<std::size_t>(i) & mask];
        const float y2 = b[static_cast<std::size_t>(i + 1) & mask];
        const float y3 = b[static_cast<std::size_t>(i + 2) & mask];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void write(std::size_t ch, float v) noexcept { buf[ch * size + (pos & mask)] = flush(v); }
};

Delay::Delay()
    : time_ms("time_ms", 375.0f, 1.0f, 4000.0f, "ms"),
      sync("sync", 0.0f, 0.0f, 1.0f),
      bpm("bpm", 120.0f, 20.0f, 300.0f, "bpm"),
      note("note", 7.0f, 0.0f, static_cast<float>(k_note_count - 1)),
      feedback("feedback", 0.4f, 0.0f, 0.98f),
      pingpong("pingpong", 0.0f, 0.0f, 1.0f),
      low_cut("low_cut", 80.0f, 20.0f, 2000.0f, "Hz"),
      high_cut("high_cut", 8000.0f, 500.0f, 20000.0f, "Hz"),
      mix("mix", 0.35f, 0.0f, 1.0f) {
    add_parameter(time_ms);
    add_parameter(sync);
    add_parameter(bpm);
    add_parameter(note);
    add_parameter(feedback);
    add_parameter(pingpong);
    add_parameter(low_cut);
    add_parameter(high_cut);
    add_parameter(mix);
}

Delay::~Delay() = default;

float Delay::delay_seconds() const noexcept {
    if (sync.get() >= 0.5f) {
        const std::size_t i = static_cast<std::size_t>(note.get() + 0.5f);
        return 60.0f / bpm.get() * delay_note_beats(i);
    }
    return time_ms.get() * 0.001f;
}

void Delay::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.size = next_power_of_two(static_cast<std::size_t>(s.rate * 4.1f) + 16);
    s.mask = s.size - 1;
    s.buf.assign(c * s.size, 0.0f);
    s.k_time = smoothing(s.rate, 60.0f);
    s.k_param = smoothing(s.rate, 10.0f);
    s.lps.assign(c, TeSection());
    s.hps.assign(c, TeSection());
    s.taps.assign(c, 0.0f);
    s.fb.assign(c, 0.0f);
    s.cached_low = -1.0f;
    s.cached_high = -1.0f;
    reset();
}

void Delay::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const float target = clampf(delay_seconds() * s.rate, 3.0f, static_cast<float>(s.size - 8));
    const float lc = low_cut.get();
    const float hc = high_cut.get();
    if (lc != s.cached_low) {
        s.hp = design_pass(true, s.rate, lc, 0.7071);
        s.cached_low = lc;
    }
    if (hc != s.cached_high) {
        s.lp = design_pass(false, s.rate, hc, 0.7071);
        s.cached_high = hc;
    }
    const bool pp = pingpong.get() >= 0.5f && c >= 2;
    const float fb_t = feedback.get();
    const float mix_t = mix.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.delay += (target - s.delay) * s.k_time;
        s.feedback += (fb_t - s.feedback) * s.k_param;
        s.mix += (mix_t - s.mix) * s.k_param;
        float dry = 1.0f;
        float wet = 0.0f;
        mix_law(s.mix, dry, wet);
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            s.taps[ch] = s.read(ch, s.delay);
            s.fb[ch] = s.hps[ch].run(s.hp, s.lps[ch].run(s.lp, s.taps[ch]));
        }
        if (pp) {
            const float in = 0.5f * (x[0] + x[1]);
            s.write(0, in + s.feedback * s.fb[1]);
            s.write(1, s.feedback * s.fb[0]);
            for (std::size_t ch = 2; ch < c; ++ch) s.write(ch, x[ch] + s.feedback * s.fb[ch]);
        } else {
            for (std::size_t ch = 0; ch < c; ++ch) s.write(ch, x[ch] + s.feedback * s.fb[ch]);
        }
        for (std::size_t ch = 0; ch < c; ++ch) x[ch] = x[ch] * dry + s.taps[ch] * wet;
        ++s.pos;
    }
}

void Delay::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    std::fill(s.buf.begin(), s.buf.end(), 0.0f);
    s.pos = 0;
    s.delay = clampf(delay_seconds() * s.rate, 3.0f, static_cast<float>(s.size - 8));
    s.feedback = feedback.get();
    s.mix = mix.get();
    for (TeSection& q : s.lps) q = TeSection();
    for (TeSection& q : s.hps) q = TeSection();
}

std::size_t Delay::tail_frames() const noexcept {
    if (!m_state) return 0;
    const double d = static_cast<double>(delay_seconds()) * static_cast<double>(m_state->rate);
    const double fb = feedback.get();
    double total = d + 1.0;
    if (fb >= 0.001) {
        const double echoes = std::ceil(std::log(0.001) / std::log(fb));
        total = d * (echoes + 1.0);
    }
    if (pingpong.get() >= 0.5f) total += d;
    total = std::min(total, 120.0 * static_cast<double>(m_state->rate));
    return static_cast<std::size_t>(total);
}

struct Reverb::State {
    static constexpr std::size_t lines = 8;
    float rate = 48000.0f;
    std::size_t channels = 1;
    std::vector<float> buf;
    std::size_t size = 0;
    std::size_t mask = 0;
    std::size_t pos = 0;
    float len[lines] = {};
    float gain[lines] = {};
    float damp_state[lines] = {};
    float mod_c[lines] = {};
    float mod_s[lines] = {};
    float mod_rc[lines] = {};
    float mod_rs[lines] = {};
    std::vector<float> pre;
    std::size_t pre_size = 0;
    std::size_t pre_mask = 0;
    std::vector<float> diff;
    std::size_t diff_size = 0;
    std::size_t diff_mask = 0;
    std::size_t diff_len[8] = {};
    float k_param = 0.01f;
    float k_len = 0.001f;
    float mix = 0.3f;
    float width = 1.0f;
};

namespace {

const float k_line_ms[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.1f, 73.3f };
const float k_diff_ms[8] = { 4.77f, 3.60f, 12.73f, 9.31f, 5.11f, 3.85f, 13.62f, 9.96f };
const float k_mod_hz[8] = { 0.13f, 0.21f, 0.29f, 0.37f, 0.43f, 0.53f, 0.61f, 0.71f };

float line_scale(float size) noexcept {
    return 0.4f + 1.2f * size;
}

} // namespace

Reverb::Reverb()
    : size("size", 0.5f, 0.0f, 1.0f),
      decay("decay", 2.0f, 0.1f, 30.0f, "s"),
      damping("damping", 0.5f, 0.0f, 1.0f),
      predelay_ms("predelay_ms", 20.0f, 0.0f, 500.0f, "ms"),
      diffusion("diffusion", 0.7f, 0.0f, 1.0f),
      width("width", 1.0f, 0.0f, 1.0f),
      mix("mix", 0.3f, 0.0f, 1.0f) {
    add_parameter(size);
    add_parameter(decay);
    add_parameter(damping);
    add_parameter(predelay_ms);
    add_parameter(diffusion);
    add_parameter(width);
    add_parameter(mix);
}

Reverb::~Reverb() = default;

void Reverb::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.size = next_power_of_two(static_cast<std::size_t>(0.125f * s.rate) + 16);
    s.mask = s.size - 1;
    s.buf.assign(State::lines * s.size, 0.0f);
    s.pre_size = next_power_of_two(static_cast<std::size_t>(0.51f * s.rate) + 16);
    s.pre_mask = s.pre_size - 1;
    s.pre.assign(2 * s.pre_size, 0.0f);
    s.diff_size = next_power_of_two(static_cast<std::size_t>(0.015f * s.rate) + 16);
    s.diff_mask = s.diff_size - 1;
    s.diff.assign(8 * s.diff_size, 0.0f);
    for (std::size_t i = 0; i < 8; ++i) s.diff_len[i] = std::max<std::size_t>(1, static_cast<std::size_t>(k_diff_ms[i] * 0.001f * s.rate));
    for (std::size_t i = 0; i < State::lines; ++i) {
        const double w = 2.0 * k_pi * static_cast<double>(k_mod_hz[i]) / static_cast<double>(s.rate);
        s.mod_rc[i] = static_cast<float>(std::cos(w));
        s.mod_rs[i] = static_cast<float>(std::sin(w));
    }
    s.k_param = smoothing(s.rate, 10.0f);
    s.k_len = smoothing(s.rate, 100.0f);
    reset();
}

void Reverb::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const float scale = line_scale(size.get());
    float target[State::lines];
    const float rt = std::max(0.05f, decay.get()) * s.rate;
    for (std::size_t i = 0; i < State::lines; ++i) {
        target[i] = k_line_ms[i] * 0.001f * s.rate * scale;
        s.gain[i] = std::pow(10.0f, -3.0f * s.len[i] / rt);
    }
    const float fc = 20000.0f * std::pow(0.025f, damping.get());
    const float damp = fc >= 0.45f * s.rate ? 0.0f : std::exp(-2.0f * static_cast<float>(k_pi) * fc / s.rate);
    const std::size_t pd = std::min(s.pre_size - 2, static_cast<std::size_t>(predelay_ms.get() * 0.001f * s.rate));
    const float g = 0.75f * diffusion.get();
    const float mod_depth = 0.0003f * s.rate;
    const float mix_t = mix.get();
    const float width_t = width.get();
    const float inv_root = 1.0f / std::sqrt(8.0f);
    for (std::size_t f = 0; f < frames; ++f) {
        s.mix += (mix_t - s.mix) * s.k_param;
        s.width += (width_t - s.width) * s.k_param;
        float* x = io + f * c;
        const float in_l = x[0];
        const float in_r = c > 1 ? x[1] : x[0];
        const std::size_t pw = s.pos & s.pre_mask;
        s.pre[pw] = flush(in_l);
        s.pre[s.pre_size + pw] = flush(in_r);
        const std::size_t pr = (s.pos - pd) & s.pre_mask;
        float side[2] = { s.pre[pr], s.pre[s.pre_size + pr] };
        for (std::size_t sd = 0; sd < 2; ++sd) {
            float v = side[sd];
            for (std::size_t k = 0; k < 4; ++k) {
                const std::size_t idx = sd * 4 + k;
                float* d = s.diff.data() + idx * s.diff_size;
                const float delayed = d[(s.pos - s.diff_len[idx]) & s.diff_mask];
                const float w = flush(v + g * delayed);
                d[s.pos & s.diff_mask] = w;
                v = delayed - g * w;
            }
            side[sd] = v;
        }
        float y[State::lines];
        for (std::size_t i = 0; i < State::lines; ++i) {
            s.len[i] += (target[i] - s.len[i]) * s.k_len;
            const float mc = s.mod_c[i] * s.mod_rc[i] - s.mod_s[i] * s.mod_rs[i];
            const float ms = s.mod_s[i] * s.mod_rc[i] + s.mod_c[i] * s.mod_rs[i];
            s.mod_c[i] = mc;
            s.mod_s[i] = ms;
            const float l = s.len[i] + mod_depth * (1.0f + ms);
            const double p = static_cast<double>(s.pos) - static_cast<double>(l);
            const double fl = std::floor(p);
            const std::size_t i0 = static_cast<std::size_t>(static_cast<std::int64_t>(fl)) & s.mask;
            const float t = static_cast<float>(p - fl);
            const float* b = s.buf.data() + i * s.size;
            const float a = b[i0];
            const float v = a + (b[(i0 + 1) & s.mask] - a) * t;
            s.damp_state[i] = flush(v * (1.0f - damp) + s.damp_state[i] * damp);
            y[i] = s.damp_state[i] * s.gain[i];
        }
        const float out_l = 0.35f * (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]);
        const float out_r = 0.35f * (y[0] - y[1] + y[2] - y[3] - y[4] + y[5] - y[6] + y[7]);
        float h[State::lines];
        for (std::size_t i = 0; i < State::lines; ++i) h[i] = y[i];
        for (std::size_t len = 1; len < State::lines; len <<= 1) {
            for (std::size_t i = 0; i < State::lines; i += len << 1) {
                for (std::size_t j = i; j < i + len; ++j) {
                    const float a = h[j];
                    const float b = h[j + len];
                    h[j] = a + b;
                    h[j + len] = a - b;
                }
            }
        }
        for (std::size_t i = 0; i < State::lines; ++i) {
            const float inject = (i & 1u) ? side[1] : side[0];
            s.buf[i * s.size + (s.pos & s.mask)] = flush(h[i] * inv_root + 0.5f * inject);
        }
        const float mid = 0.5f * (out_l + out_r);
        const float sid = 0.5f * (out_l - out_r) * s.width;
        float dry = 1.0f;
        float wet = 0.0f;
        mix_law(s.mix, dry, wet);
        if (c == 1) {
            x[0] = x[0] * dry + mid * wet;
        } else {
            x[0] = x[0] * dry + (mid + sid) * wet;
            x[1] = x[1] * dry + (mid - sid) * wet;
            for (std::size_t ch = 2; ch < c; ++ch) x[ch] = x[ch] * dry + mid * wet;
        }
        ++s.pos;
    }
    for (std::size_t i = 0; i < State::lines; ++i) {
        const float m = std::sqrt(s.mod_c[i] * s.mod_c[i] + s.mod_s[i] * s.mod_s[i]);
        if (m > 0.0f) {
            s.mod_c[i] /= m;
            s.mod_s[i] /= m;
        }
    }
}

void Reverb::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    std::fill(s.buf.begin(), s.buf.end(), 0.0f);
    std::fill(s.pre.begin(), s.pre.end(), 0.0f);
    std::fill(s.diff.begin(), s.diff.end(), 0.0f);
    s.pos = 0;
    const float scale = line_scale(size.get());
    for (std::size_t i = 0; i < State::lines; ++i) {
        s.len[i] = k_line_ms[i] * 0.001f * s.rate * scale;
        s.damp_state[i] = 0.0f;
        const double ph = 2.0 * k_pi * static_cast<double>(i) / static_cast<double>(State::lines);
        s.mod_c[i] = static_cast<float>(std::cos(ph));
        s.mod_s[i] = static_cast<float>(std::sin(ph));
    }
    s.mix = mix.get();
    s.width = width.get();
}

std::size_t Reverb::tail_frames() const noexcept {
    if (!m_state) return 0;
    const double seconds = 1.1 * static_cast<double>(decay.get()) + 0.001 * static_cast<double>(predelay_ms.get()) + 0.15;
    return static_cast<std::size_t>(seconds * static_cast<double>(m_state->rate));
}

struct Distortion::State {
    static constexpr std::size_t taps = 129;
    static constexpr std::size_t delay = 32;
    float rate = 48000.0f;
    std::size_t channels = 1;
    float h[taps] = {};
    float poly[4][33] = {};
    std::size_t poly_len[4] = {};
    std::vector<float> xhist;
    std::vector<float> zhist;
    std::vector<float> dry;
    std::vector<float> plain;
    std::vector<float> tone;
    std::vector<float> dc_x;
    std::vector<float> dc_y;
    std::size_t n = 0;
    float k_param = 0.01f;
    float mix = 1.0f;
    float gain = 1.0f;
    float out = 1.0f;
    float dc_r = 0.999f;
};

namespace {

float shape_sample(int mode, float u) noexcept {
    switch (mode) {
        case 1: return clampf(u, -1.0f, 1.0f);
        case 2: return std::tanh(u + 0.25f) - 0.2449186624f;
        case 3: return std::sin(u * 1.5707963267948966f);
        default: return std::tanh(u);
    }
}

} // namespace

Distortion::Distortion()
    : mode("mode", 0.0f, 0.0f, 3.0f),
      drive("drive", 12.0f, 0.0f, 48.0f, "dB"),
      tone("tone", 0.7f, 0.0f, 1.0f),
      mix("mix", 1.0f, 0.0f, 1.0f),
      output("output", 0.0f, -24.0f, 12.0f, "dB"),
      oversample("oversample", 1.0f, 0.0f, 1.0f) {
    add_parameter(mode);
    add_parameter(drive);
    add_parameter(tone);
    add_parameter(mix);
    add_parameter(output);
    add_parameter(oversample);
}

Distortion::~Distortion() = default;

void Distortion::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    const double fc = 0.115;
    const double beta = 8.0;
    const double mid = static_cast<double>(State::taps - 1) * 0.5;
    const double norm = bessel_i0(beta);
    double sum = 0.0;
    double hd[State::taps];
    for (std::size_t m = 0; m < State::taps; ++m) {
        const double t = static_cast<double>(m) - mid;
        const double sinc = t == 0.0 ? 2.0 * fc : std::sin(2.0 * k_pi * fc * t) / (k_pi * t);
        const double r2 = t / mid;
        const double win = bessel_i0(beta * std::sqrt(std::max(0.0, 1.0 - r2 * r2))) / norm;
        hd[m] = sinc * win;
        sum += hd[m];
    }
    for (std::size_t m = 0; m < State::taps; ++m) s.h[m] = static_cast<float>(hd[m] / sum);
    for (std::size_t j = 0; j < 4; ++j) {
        std::size_t k = 0;
        for (std::size_t m = j; m < State::taps; m += 4) s.poly[j][k++] = 4.0f * s.h[m];
        s.poly_len[j] = k;
    }
    s.xhist.assign(c * 64, 0.0f);
    s.zhist.assign(c * 256, 0.0f);
    s.dry.assign(c * 64, 0.0f);
    s.plain.assign(c * 64, 0.0f);
    s.tone.assign(c, 0.0f);
    s.dc_x.assign(c, 0.0f);
    s.dc_y.assign(c, 0.0f);
    s.k_param = smoothing(s.rate, 10.0f);
    s.dc_r = std::exp(-2.0f * static_cast<float>(k_pi) * 10.0f / s.rate);
    reset();
}

void Distortion::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const int md = std::min(3, std::max(0, static_cast<int>(mode.get() + 0.5f)));
    const bool os = oversample.get() >= 0.5f;
    const float gain_t = db_to_gain(drive.get());
    const float out_t = db_to_gain(output.get());
    const float mix_t = mix.get();
    const float fc = 800.0f * std::pow(25.0f, tone.get());
    const bool use_tone = fc < 0.45f * s.rate;
    const float ta = use_tone ? std::exp(-2.0f * static_cast<float>(k_pi) * fc / s.rate) : 0.0f;
    for (std::size_t f = 0; f < frames; ++f) {
        s.gain += (gain_t - s.gain) * s.k_param;
        s.out += (out_t - s.out) * s.k_param;
        s.mix += (mix_t - s.mix) * s.k_param;
        const std::size_t n = s.n;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float in = x[ch] == x[ch] ? x[ch] : 0.0f;
            float* dry = s.dry.data() + ch * 64;
            dry[n & 63] = in;
            const float delayed_dry = dry[(n - State::delay) & 63];
            float* xh = s.xhist.data() + ch * 64;
            xh[n & 63] = in;
            float* plain = s.plain.data() + ch * 64;
            plain[n & 63] = shape_sample(md, in * s.gain);
            float wet = 0.0f;
            if (os) {
                float* zh = s.zhist.data() + ch * 256;
                for (std::size_t j = 0; j < 4; ++j) {
                    float u = 0.0f;
                    const float* p = s.poly[j];
                    for (std::size_t i = 0; i < s.poly_len[j]; ++i) u += p[i] * xh[(n - i) & 63];
                    const std::size_t zi = 4 * n + j;
                    zh[zi & 255] = flush(shape_sample(md, u * s.gain));
                    if (j == 0) {
                        float acc = 0.0f;
                        for (std::size_t m = 0; m < State::taps; ++m) acc += s.h[m] * zh[(zi - m) & 255];
                        wet = acc;
                    }
                }
            } else {
                wet = plain[(n - State::delay) & 63];
            }
            const float dcy = wet - s.dc_x[ch] + s.dc_r * s.dc_y[ch];
            s.dc_x[ch] = wet;
            s.dc_y[ch] = flush(dcy);
            wet = dcy;
            if (use_tone) {
                s.tone[ch] = flush(wet * (1.0f - ta) + s.tone[ch] * ta);
                wet = s.tone[ch];
            }
            x[ch] = (delayed_dry * (1.0f - s.mix) + wet * s.mix) * s.out;
        }
        ++s.n;
    }
}

void Distortion::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    std::fill(s.xhist.begin(), s.xhist.end(), 0.0f);
    std::fill(s.zhist.begin(), s.zhist.end(), 0.0f);
    std::fill(s.dry.begin(), s.dry.end(), 0.0f);
    std::fill(s.plain.begin(), s.plain.end(), 0.0f);
    std::fill(s.tone.begin(), s.tone.end(), 0.0f);
    std::fill(s.dc_x.begin(), s.dc_x.end(), 0.0f);
    std::fill(s.dc_y.begin(), s.dc_y.end(), 0.0f);
    s.n = 64;
    s.gain = db_to_gain(drive.get());
    s.out = db_to_gain(output.get());
    s.mix = mix.get();
}

std::size_t Distortion::latency_frames() const noexcept {
    return m_state ? State::delay : 0;
}

struct Bitcrusher::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 1.0;
    std::vector<float> held;
    float k_param = 0.01f;
    float mix = 1.0f;
};

Bitcrusher::Bitcrusher()
    : bits("bits", 8.0f, 1.0f, 24.0f),
      rate_hz("rate_hz", 48000.0f, 200.0f, 192000.0f, "Hz"),
      mix("mix", 1.0f, 0.0f, 1.0f) {
    add_parameter(bits);
    add_parameter(rate_hz);
    add_parameter(mix);
}

Bitcrusher::~Bitcrusher() = default;

void Bitcrusher::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    m_state->rate = static_cast<float>(r);
    m_state->channels = c;
    m_state->held.assign(c, 0.0f);
    m_state->k_param = smoothing(m_state->rate, 10.0f);
    reset();
}

void Bitcrusher::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const float levels = std::pow(2.0f, bits.get() - 1.0f);
    const float inv = 1.0f / levels;
    const double step = std::min(1.0, static_cast<double>(rate_hz.get()) / static_cast<double>(s.rate));
    const float mix_t = mix.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.mix += (mix_t - s.mix) * s.k_param;
        float* x = io + f * c;
        s.phase += step;
        if (s.phase >= 1.0) {
            s.phase -= 1.0;
            for (std::size_t ch = 0; ch < c; ++ch) {
                const float v = clampf(x[ch] == x[ch] ? x[ch] : 0.0f, -1.0f, 1.0f);
                s.held[ch] = clampf(std::floor(v * levels + 0.5f) * inv, -1.0f, 1.0f);
            }
        }
        for (std::size_t ch = 0; ch < c; ++ch) x[ch] = x[ch] * (1.0f - s.mix) + s.held[ch] * s.mix;
    }
}

void Bitcrusher::reset() noexcept {
    if (!m_state) return;
    m_state->phase = 1.0;
    std::fill(m_state->held.begin(), m_state->held.end(), 0.0f);
    m_state->mix = mix.get();
}

} // namespace audio
} // namespace fizmo
