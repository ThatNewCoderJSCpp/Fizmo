#include "modulation.hpp"
#include "fft.hpp"

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

float hash_unit(std::int64_t index) noexcept {
    std::uint32_t h = static_cast<std::uint32_t>(static_cast<std::uint64_t>(index) & 0xFFFFFFFFu) ^ static_cast<std::uint32_t>(static_cast<std::uint64_t>(index) >> 32);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h) * (2.0f / 4294967295.0f) - 1.0f;
}

LfoShape to_shape(float v) noexcept {
    const int i = static_cast<int>(v + 0.5f);
    return static_cast<LfoShape>(std::min(5, std::max(0, i)));
}

float smoothing(float rate, float ms) noexcept {
    return 1.0f - std::exp(-1.0f / (0.001f * ms * rate));
}

class Lines {
public:
    void prepare(std::size_t channels, std::size_t max_delay) {
        m_channels = std::max<std::size_t>(1, channels);
        m_size = next_power_of_two(max_delay + 8);
        m_mask = m_size - 1;
        m_buf.assign(m_channels * m_size, 0.0f);
        m_pos = 0;
    }

    void clear() noexcept {
        std::fill(m_buf.begin(), m_buf.end(), 0.0f);
        m_pos = 0;
    }

    float read(std::size_t ch, float delay) const noexcept {
        const float d = clampf(delay, 3.0f, static_cast<float>(m_size - 4));
        const double pos = static_cast<double>(m_pos) - static_cast<double>(d);
        const double fl = std::floor(pos);
        const std::int64_t i = static_cast<std::int64_t>(fl);
        const float t = static_cast<float>(pos - fl);
        const float* b = m_buf.data() + ch * m_size;
        const float y0 = b[static_cast<std::size_t>(i - 1) & m_mask];
        const float y1 = b[static_cast<std::size_t>(i) & m_mask];
        const float y2 = b[static_cast<std::size_t>(i + 1) & m_mask];
        const float y3 = b[static_cast<std::size_t>(i + 2) & m_mask];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void write(std::size_t ch, float v) noexcept { m_buf[ch * m_size + (m_pos & m_mask)] = flush(v); }
    void advance() noexcept { ++m_pos; }

private:
    std::size_t m_channels = 1;
    std::size_t m_size = 0;
    std::size_t m_mask = 0;
    std::size_t m_pos = 0;
    std::vector<float> m_buf;
};

float channel_offset(std::size_t ch, std::size_t channels, float spread) noexcept {
    if (channels < 2) return 0.0f;
    return 0.5f * spread * static_cast<float>(ch) / static_cast<float>(channels - 1);
}

} // namespace

float lfo_value(LfoShape shape, double cycles) noexcept {
    const double fl = std::floor(cycles);
    const float p = static_cast<float>(cycles - fl);
    float tri = 0.0f;
    if (p < 0.25f) tri = 4.0f * p;
    else if (p < 0.75f) tri = 2.0f - 4.0f * p;
    else tri = 4.0f * p - 4.0f;
    switch (shape) {
        case LfoShape::sine: return static_cast<float>(std::sin(2.0 * k_pi * static_cast<double>(p)));
        case LfoShape::triangle: return tri;
        case LfoShape::square: return clampf(4.0f * tri, -1.0f, 1.0f);
        case LfoShape::saw_up: return 2.0f * p - 1.0f;
        case LfoShape::saw_down: return 1.0f - 2.0f * p;
        case LfoShape::random: {
            const std::int64_t i = static_cast<std::int64_t>(fl);
            const float a = hash_unit(i);
            const float b = hash_unit(i + 1);
            const float t = p * p * (3.0f - 2.0f * p);
            return a + (b - a) * t;
        }
    }
    return 0.0f;
}

struct Chorus::State {
    Lines lines;
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float k_mod = 0.1f;
    float mix = 0.5f;
    float feedback = 0.0f;
    std::vector<float> delays;
    std::vector<float> last;
};

Chorus::Chorus()
    : rate("rate", 0.8f, 0.01f, 10.0f, "Hz"),
      depth_ms("depth_ms", 3.0f, 0.0f, 20.0f, "ms"),
      delay_ms("delay_ms", 15.0f, 1.0f, 40.0f, "ms"),
      voices("voices", 2.0f, 1.0f, 4.0f),
      feedback("feedback", 0.0f, -0.95f, 0.95f),
      mix("mix", 0.5f, 0.0f, 1.0f),
      spread("spread", 1.0f, 0.0f, 1.0f),
      shape("shape", 0.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth_ms);
    add_parameter(delay_ms);
    add_parameter(voices);
    add_parameter(feedback);
    add_parameter(mix);
    add_parameter(spread);
    add_parameter(shape);
}

Chorus::~Chorus() = default;

void Chorus::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.lines.prepare(c, static_cast<std::size_t>(0.062f * s.rate) + 8);
    s.k_param = smoothing(s.rate, 10.0f);
    s.k_mod = smoothing(s.rate, 1.0f);
    s.delays.assign(c * 4, 0.0f);
    s.last.assign(c, 0.0f);
    reset();
}

void Chorus::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const int v = std::min(4, std::max(1, static_cast<int>(voices.get() + 0.5f)));
    const float vgain = 1.0f / std::sqrt(static_cast<float>(v));
    const float vavg = 1.0f / static_cast<float>(v);
    const LfoShape sh = to_shape(shape.get());
    const float ms = s.rate * 0.001f;
    const float base = delay_ms.get() * ms;
    const float depth = depth_ms.get() * ms;
    const float sp = spread.get();
    const float mix_t = mix.get();
    const float fb_t = feedback.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.mix += (mix_t - s.mix) * s.k_param;
        s.feedback += (fb_t - s.feedback) * s.k_param;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const double off = channel_offset(ch, c, sp);
            float sum = 0.0f;
            for (int k = 0; k < v; ++k) {
                const float l = lfo_value(sh, s.phase + off + static_cast<double>(k) / static_cast<double>(v));
                float& d = s.delays[ch * 4 + static_cast<std::size_t>(k)];
                d += (base + depth * 0.5f * (1.0f + l) - d) * s.k_mod;
                sum += s.lines.read(ch, d);
            }
            const float wet = sum * vgain;
            const float in = x[ch];
            s.lines.write(ch, in + s.feedback * s.last[ch]);
            s.last[ch] = flush(sum * vavg);
            x[ch] = in * (1.0f - s.mix) + wet * s.mix;
        }
        s.lines.advance();
    }
}

void Chorus::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    s.lines.clear();
    s.phase = 0.0;
    s.mix = mix.get();
    s.feedback = feedback.get();
    const float d = (delay_ms.get() + 0.5f * depth_ms.get()) * s.rate * 0.001f;
    std::fill(s.delays.begin(), s.delays.end(), d);
    std::fill(s.last.begin(), s.last.end(), 0.0f);
}

struct Flanger::State {
    Lines lines;
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float k_mod = 0.1f;
    float mix = 0.5f;
    float feedback = 0.0f;
    std::vector<float> delays;
    std::vector<float> last;
};

Flanger::Flanger()
    : rate("rate", 0.25f, 0.01f, 10.0f, "Hz"),
      depth_ms("depth_ms", 2.0f, 0.0f, 10.0f, "ms"),
      delay_ms("delay_ms", 1.0f, 0.1f, 10.0f, "ms"),
      feedback("feedback", 0.5f, -0.98f, 0.98f),
      mix("mix", 0.5f, 0.0f, 1.0f),
      spread("spread", 0.5f, 0.0f, 1.0f),
      shape("shape", 1.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth_ms);
    add_parameter(delay_ms);
    add_parameter(feedback);
    add_parameter(mix);
    add_parameter(spread);
    add_parameter(shape);
}

Flanger::~Flanger() = default;

void Flanger::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.lines.prepare(c, static_cast<std::size_t>(0.021f * s.rate) + 8);
    s.k_param = smoothing(s.rate, 10.0f);
    s.k_mod = smoothing(s.rate, 1.0f);
    s.delays.assign(c, 0.0f);
    s.last.assign(c, 0.0f);
    reset();
}

void Flanger::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const LfoShape sh = to_shape(shape.get());
    const float ms = s.rate * 0.001f;
    const float base = delay_ms.get() * ms;
    const float depth = depth_ms.get() * ms;
    const float sp = spread.get();
    const float mix_t = mix.get();
    const float fb_t = feedback.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.mix += (mix_t - s.mix) * s.k_param;
        s.feedback += (fb_t - s.feedback) * s.k_param;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float l = lfo_value(sh, s.phase + channel_offset(ch, c, sp));
            float& d = s.delays[ch];
            d += (base + depth * 0.5f * (1.0f + l) - d) * s.k_mod;
            const float wet = s.lines.read(ch, d);
            const float in = x[ch];
            s.lines.write(ch, in + s.feedback * wet);
            x[ch] = in * (1.0f - s.mix) + wet * s.mix;
        }
        s.lines.advance();
    }
}

void Flanger::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    s.lines.clear();
    s.phase = 0.0;
    s.mix = mix.get();
    s.feedback = feedback.get();
    const float d = (delay_ms.get() + 0.5f * depth_ms.get()) * s.rate * 0.001f;
    std::fill(s.delays.begin(), s.delays.end(), d);
    std::fill(s.last.begin(), s.last.end(), 0.0f);
}

struct Phaser::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float mix = 0.5f;
    float feedback = 0.0f;
    std::vector<float> x1;
    std::vector<float> y1;
    std::vector<float> last;
};

Phaser::Phaser()
    : rate("rate", 0.5f, 0.01f, 10.0f, "Hz"),
      depth("depth", 1.0f, 0.0f, 1.0f),
      stages("stages", 4.0f, 2.0f, 12.0f),
      feedback("feedback", 0.5f, -0.95f, 0.95f),
      min_hz("min_hz", 200.0f, 20.0f, 5000.0f, "Hz"),
      max_hz("max_hz", 2000.0f, 100.0f, 18000.0f, "Hz"),
      mix("mix", 0.5f, 0.0f, 1.0f),
      spread("spread", 0.5f, 0.0f, 1.0f),
      shape("shape", 0.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth);
    add_parameter(stages);
    add_parameter(feedback);
    add_parameter(min_hz);
    add_parameter(max_hz);
    add_parameter(mix);
    add_parameter(spread);
    add_parameter(shape);
}

Phaser::~Phaser() = default;

void Phaser::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.k_param = smoothing(s.rate, 10.0f);
    s.x1.assign(c * 12, 0.0f);
    s.y1.assign(c * 12, 0.0f);
    s.last.assign(c, 0.0f);
    reset();
}

void Phaser::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const LfoShape sh = to_shape(shape.get());
    const std::size_t n = static_cast<std::size_t>(std::min(12, std::max(2, static_cast<int>(stages.get() + 0.5f))));
    const float top = s.rate * 0.45f;
    const float lo = std::min(top * 0.5f, min_hz.get());
    const float hi = clampf(max_hz.get(), lo, top);
    const float span = std::log(hi / lo);
    const float dp = depth.get();
    const float sp = spread.get();
    const float mix_t = mix.get();
    const float fb_t = feedback.get();
    const float w = static_cast<float>(k_pi) / s.rate;
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.mix += (mix_t - s.mix) * s.k_param;
        s.feedback += (fb_t - s.feedback) * s.k_param;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float l = lfo_value(sh, s.phase + channel_offset(ch, c, sp));
            const float fc = lo * std::exp(span * (0.5f + 0.5f * dp * l));
            const float t = std::tan(w * fc);
            const float a = (t - 1.0f) / (t + 1.0f);
            const float in = x[ch];
            float u = in + s.feedback * s.last[ch];
            float* px = s.x1.data() + ch * 12;
            float* py = s.y1.data() + ch * 12;
            for (std::size_t k = 0; k < n; ++k) {
                const float y = a * u + px[k] - a * py[k];
                px[k] = u;
                py[k] = flush(y);
                u = py[k];
            }
            s.last[ch] = u;
            x[ch] = in * (1.0f - s.mix) + u * s.mix;
        }
    }
}

void Phaser::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    s.phase = 0.0;
    s.mix = mix.get();
    s.feedback = feedback.get();
    std::fill(s.x1.begin(), s.x1.end(), 0.0f);
    std::fill(s.y1.begin(), s.y1.end(), 0.0f);
    std::fill(s.last.begin(), s.last.end(), 0.0f);
}

struct Tremolo::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float depth = 0.5f;
};

Tremolo::Tremolo()
    : rate("rate", 5.0f, 0.01f, 30.0f, "Hz"),
      depth("depth", 0.5f, 0.0f, 1.0f),
      spread("spread", 0.0f, 0.0f, 1.0f),
      shape("shape", 0.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth);
    add_parameter(spread);
    add_parameter(shape);
}

Tremolo::~Tremolo() = default;

void Tremolo::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    m_state->rate = static_cast<float>(r);
    m_state->channels = c;
    m_state->k_param = smoothing(m_state->rate, 10.0f);
    reset();
}

void Tremolo::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const LfoShape sh = to_shape(shape.get());
    const float sp = spread.get();
    const float depth_t = depth.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.depth += (depth_t - s.depth) * s.k_param;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float l = lfo_value(sh, s.phase + channel_offset(ch, c, sp));
            x[ch] *= 1.0f - s.depth * 0.5f * (1.0f - l);
        }
    }
}

void Tremolo::reset() noexcept {
    if (!m_state) return;
    m_state->phase = 0.0;
    m_state->depth = depth.get();
}

struct Vibrato::State {
    Lines lines;
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float k_mod = 0.1f;
    float mix = 1.0f;
    std::vector<float> delays;
};

Vibrato::Vibrato()
    : rate("rate", 5.0f, 0.01f, 20.0f, "Hz"),
      depth_ms("depth_ms", 1.5f, 0.0f, 10.0f, "ms"),
      mix("mix", 1.0f, 0.0f, 1.0f),
      spread("spread", 0.0f, 0.0f, 1.0f),
      shape("shape", 0.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth_ms);
    add_parameter(mix);
    add_parameter(spread);
    add_parameter(shape);
}

Vibrato::~Vibrato() = default;

void Vibrato::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.lines.prepare(c, static_cast<std::size_t>(0.021f * s.rate) + 8);
    s.k_param = smoothing(s.rate, 10.0f);
    s.k_mod = smoothing(s.rate, 1.0f);
    s.delays.assign(c, 0.0f);
    reset();
}

void Vibrato::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const LfoShape sh = to_shape(shape.get());
    const float ms = s.rate * 0.001f;
    const float base = 0.3f * ms;
    const float depth = depth_ms.get() * ms;
    const float sp = spread.get();
    const float mix_t = mix.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.mix += (mix_t - s.mix) * s.k_param;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            const float l = lfo_value(sh, s.phase + channel_offset(ch, c, sp));
            float& d = s.delays[ch];
            d += (base + depth * (1.0f + l) - d) * s.k_mod;
            const float in = x[ch];
            s.lines.write(ch, in);
            const float wet = s.lines.read(ch, d);
            x[ch] = in * (1.0f - s.mix) + wet * s.mix;
        }
        s.lines.advance();
    }
}

void Vibrato::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    s.lines.clear();
    s.phase = 0.0;
    s.mix = mix.get();
    const float d = (0.3f + depth_ms.get()) * s.rate * 0.001f;
    std::fill(s.delays.begin(), s.delays.end(), d);
}

std::size_t Vibrato::latency_frames() const noexcept {
    if (!m_state || mix.get() < 0.5f) return 0;
    return static_cast<std::size_t>((0.3f + depth_ms.get()) * m_state->rate * 0.001f + 0.5f);
}

struct AutoPan::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float depth = 1.0f;
};

AutoPan::AutoPan()
    : rate("rate", 1.0f, 0.01f, 20.0f, "Hz"),
      depth("depth", 1.0f, 0.0f, 1.0f),
      shape("shape", 0.0f, 0.0f, 5.0f) {
    add_parameter(rate);
    add_parameter(depth);
    add_parameter(shape);
}

AutoPan::~AutoPan() = default;

void AutoPan::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    m_state->rate = static_cast<float>(r);
    m_state->channels = c;
    m_state->k_param = smoothing(m_state->rate, 10.0f);
    reset();
}

void AutoPan::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    if (c < 2) return;
    const double inc = static_cast<double>(rate.get()) / static_cast<double>(s.rate);
    const LfoShape sh = to_shape(shape.get());
    const float depth_t = depth.get();
    const float quarter = static_cast<float>(k_pi * 0.25);
    const float root2 = std::sqrt(2.0f);
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        s.depth += (depth_t - s.depth) * s.k_param;
        const float pan = s.depth * lfo_value(sh, s.phase);
        const float angle = (pan + 1.0f) * quarter;
        float* x = io + f * c;
        x[0] *= std::cos(angle) * root2;
        x[1] *= std::sin(angle) * root2;
    }
}

void AutoPan::reset() noexcept {
    if (!m_state) return;
    m_state->phase = 0.0;
    m_state->depth = depth.get();
}

} // namespace audio
} // namespace fizmo
