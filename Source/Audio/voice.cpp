#include "voice.hpp"
#include "fft.hpp"
#include "format.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
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

struct VxCoeffs {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

enum class VxType {
    lowpass,
    highpass,
    bandpass,
    peak
};

VxCoeffs vx_design(VxType type, double rate, double freq, double q, double gain_db) noexcept {
    const double f = std::min(rate * 0.49, std::max(1.0, freq));
    const double w = 2.0 * k_pi * f / rate;
    const double cw = std::cos(w);
    const double sw = std::sin(w);
    const double alpha = sw / (2.0 * std::max(0.05, q));
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
    switch (type) {
        case VxType::lowpass:
            b0 = (1.0 - cw) * 0.5;
            b1 = 1.0 - cw;
            b2 = b0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case VxType::highpass:
            b0 = (1.0 + cw) * 0.5;
            b1 = -(1.0 + cw);
            b2 = b0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case VxType::bandpass:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case VxType::peak: {
            const double a = std::pow(10.0, gain_db / 40.0);
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cw;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha / a;
            break;
        }
    }
    VxCoeffs c;
    c.b0 = static_cast<float>(b0 / a0);
    c.b1 = static_cast<float>(b1 / a0);
    c.b2 = static_cast<float>(b2 / a0);
    c.a1 = static_cast<float>(a1 / a0);
    c.a2 = static_cast<float>(a2 / a0);
    return c;
}

struct VxSection {
    float z1 = 0.0f;
    float z2 = 0.0f;
    float run(const VxCoeffs& c, float x) noexcept {
        const float y = c.b0 * x + z1;
        z1 = flush(c.b1 * x - c.a1 * y + z2);
        z2 = flush(c.b2 * x - c.a2 * y);
        return y;
    }
};

float poly_blep(double t, double dt) noexcept {
    if (t < dt) {
        const double x = t / dt;
        return static_cast<float>(x + x - x * x - 1.0);
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return static_cast<float>(x * x + x + x + 1.0);
    }
    return 0.0f;
}

const char* const k_preset_names[] = { "none", "helium", "deep", "robot", "alien", "monster", "radio", "telephone", "megaphone", "cave", "underwater", "whisper", "feminine", "masculine" };
const std::size_t k_preset_count = sizeof(k_preset_names) / sizeof(k_preset_names[0]);

struct PresetSpec {
    bool use_pitch = false;
    float pitch = 0.0f;
    float formant = 0.0f;
    bool preserve = true;
    bool eq = false;
    float low_cut = 20.0f;
    float high_cut = 22000.0f;
    float peak_hz = 1000.0f;
    float peak_db = 0.0f;
    float peak_q = 1.0f;
    bool vocoder = false;
    float carrier = 0.0f;
    float carrier_hz = 110.0f;
    float bands = 24.0f;
    float sibilance = 0.3f;
    float voc_attack = 4.0f;
    float voc_release = 30.0f;
    float voc_mix = 1.0f;
    bool ring = false;
    float ring_hz = 50.0f;
    float ring_shape = 0.0f;
    float ring_mix = 0.5f;
    bool dist = false;
    float dist_mode = 0.0f;
    float drive = 12.0f;
    float tone = 0.7f;
    float dist_mix = 1.0f;
    float dist_out = 0.0f;
    bool crush = false;
    float bits = 8.0f;
    float crush_hz = 8000.0f;
    bool flanger = false;
    float fl_rate = 0.25f;
    float fl_depth = 2.0f;
    float fl_delay = 1.0f;
    float fl_feedback = 0.5f;
    float fl_mix = 0.5f;
    bool chorus = false;
    float ch_rate = 0.8f;
    float ch_depth = 3.0f;
    float ch_delay = 15.0f;
    float ch_mix = 0.5f;
    bool vibrato = false;
    float vib_rate = 5.0f;
    float vib_depth = 0.5f;
    bool tremolo = false;
    float tr_rate = 5.0f;
    float tr_depth = 0.5f;
    bool delay = false;
    float delay_ms = 300.0f;
    float delay_fb = 0.4f;
    float delay_mix = 0.3f;
    float delay_lc = 100.0f;
    float delay_hc = 5000.0f;
    bool reverb = false;
    float rv_size = 0.5f;
    float rv_decay = 1.5f;
    float rv_damping = 0.5f;
    float rv_predelay = 10.0f;
    float rv_mix = 0.3f;
    float makeup_db = 0.0f;
};

PresetSpec preset_spec(VoicePreset preset) noexcept {
    PresetSpec s;
    switch (preset) {
        case VoicePreset::none:
            break;
        case VoicePreset::helium:
            s.use_pitch = true;
            s.pitch = 8.0f;
            s.preserve = false;
            s.eq = true;
            s.low_cut = 150.0f;
            s.makeup_db = 0.0f;
            break;
        case VoicePreset::deep:
            s.use_pitch = true;
            s.pitch = -6.0f;
            s.preserve = false;
            s.eq = true;
            s.peak_hz = 140.0f;
            s.peak_db = 2.0f;
            s.peak_q = 0.7f;
            s.makeup_db = -1.5f;
            break;
        case VoicePreset::robot:
            s.vocoder = true;
            s.carrier = 0.0f;
            s.carrier_hz = 120.0f;
            s.bands = 24.0f;
            s.sibilance = 0.25f;
            s.voc_attack = 3.0f;
            s.voc_release = 25.0f;
            s.flanger = true;
            s.fl_rate = 0.1f;
            s.fl_depth = 0.0f;
            s.fl_delay = 4.0f;
            s.fl_feedback = 0.5f;
            s.fl_mix = 0.35f;
            s.makeup_db = 4.5f;
            break;
        case VoicePreset::alien:
            s.use_pitch = true;
            s.pitch = 5.0f;
            s.formant = 4.0f;
            s.preserve = true;
            s.ring = true;
            s.ring_hz = 70.0f;
            s.ring_mix = 0.35f;
            s.vibrato = true;
            s.vib_rate = 7.0f;
            s.vib_depth = 0.25f;
            s.flanger = true;
            s.fl_rate = 0.5f;
            s.fl_depth = 1.5f;
            s.fl_delay = 1.0f;
            s.fl_feedback = 0.6f;
            s.fl_mix = 0.4f;
            s.makeup_db = 6.5f;
            break;
        case VoicePreset::monster:
            s.use_pitch = true;
            s.pitch = -8.0f;
            s.formant = -5.0f;
            s.preserve = true;
            s.eq = true;
            s.peak_hz = 120.0f;
            s.peak_db = 3.0f;
            s.peak_q = 0.7f;
            s.dist = true;
            s.dist_mode = 2.0f;
            s.drive = 10.0f;
            s.tone = 0.45f;
            s.dist_mix = 0.45f;
            s.reverb = true;
            s.rv_size = 0.3f;
            s.rv_decay = 0.8f;
            s.rv_damping = 0.6f;
            s.rv_mix = 0.15f;
            s.makeup_db = -3.5f;
            break;
        case VoicePreset::radio:
            s.eq = true;
            s.low_cut = 350.0f;
            s.high_cut = 3800.0f;
            s.peak_hz = 1800.0f;
            s.peak_db = 4.0f;
            s.dist = true;
            s.dist_mode = 0.0f;
            s.drive = 10.0f;
            s.tone = 0.8f;
            s.dist_mix = 0.6f;
            s.makeup_db = -1.0f;
            break;
        case VoicePreset::telephone:
            s.eq = true;
            s.low_cut = 300.0f;
            s.high_cut = 3400.0f;
            s.peak_hz = 1200.0f;
            s.peak_db = 3.0f;
            s.crush = true;
            s.bits = 8.0f;
            s.crush_hz = 8000.0f;
            s.dist = true;
            s.dist_mode = 0.0f;
            s.drive = 6.0f;
            s.tone = 0.8f;
            s.dist_mix = 0.4f;
            s.makeup_db = 0.0f;
            break;
        case VoicePreset::megaphone:
            s.eq = true;
            s.low_cut = 450.0f;
            s.high_cut = 4500.0f;
            s.peak_hz = 2200.0f;
            s.peak_db = 9.0f;
            s.peak_q = 0.9f;
            s.dist = true;
            s.dist_mode = 1.0f;
            s.drive = 16.0f;
            s.tone = 0.55f;
            s.dist_mix = 1.0f;
            s.dist_out = -6.0f;
            s.delay = true;
            s.delay_ms = 70.0f;
            s.delay_fb = 0.12f;
            s.delay_mix = 0.12f;
            s.makeup_db = -3.0f;
            break;
        case VoicePreset::cave:
            s.delay = true;
            s.delay_ms = 320.0f;
            s.delay_fb = 0.4f;
            s.delay_mix = 0.25f;
            s.delay_lc = 200.0f;
            s.delay_hc = 3000.0f;
            s.reverb = true;
            s.rv_size = 0.95f;
            s.rv_decay = 4.0f;
            s.rv_damping = 0.55f;
            s.rv_predelay = 40.0f;
            s.rv_mix = 0.35f;
            s.makeup_db = -2.0f;
            break;
        case VoicePreset::underwater:
            s.eq = true;
            s.low_cut = 60.0f;
            s.high_cut = 650.0f;
            s.peak_hz = 300.0f;
            s.peak_db = 4.0f;
            s.peak_q = 0.8f;
            s.vibrato = true;
            s.vib_rate = 1.8f;
            s.vib_depth = 1.2f;
            s.chorus = true;
            s.ch_rate = 0.4f;
            s.ch_depth = 6.0f;
            s.ch_delay = 18.0f;
            s.ch_mix = 0.5f;
            s.reverb = true;
            s.rv_size = 0.4f;
            s.rv_decay = 1.5f;
            s.rv_damping = 0.9f;
            s.rv_mix = 0.3f;
            s.makeup_db = -1.0f;
            break;
        case VoicePreset::whisper:
            s.vocoder = true;
            s.carrier = 2.0f;
            s.bands = 28.0f;
            s.voc_attack = 2.0f;
            s.voc_release = 30.0f;
            s.sibilance = 0.6f;
            s.voc_mix = 0.9f;
            s.eq = true;
            s.low_cut = 250.0f;
            s.makeup_db = 8.0f;
            break;
        case VoicePreset::feminine:
            s.use_pitch = true;
            s.pitch = 5.0f;
            s.formant = 2.5f;
            s.preserve = true;
            s.eq = true;
            s.low_cut = 150.0f;
            s.makeup_db = 0.0f;
            break;
        case VoicePreset::masculine:
            s.use_pitch = true;
            s.pitch = -5.0f;
            s.formant = -2.5f;
            s.preserve = true;
            s.eq = true;
            s.peak_hz = 150.0f;
            s.peak_db = 2.0f;
            s.peak_q = 0.7f;
            s.makeup_db = 0.0f;
            break;
    }
    return s;
}

void apply_pitch(PitchShifter& e, const PresetSpec& s) noexcept {
    e.mode.set(0.0f);
    e.semitones.set(s.pitch);
    e.cents.set(0.0f);
    e.formant.set(s.formant);
    e.preserve_formants.set(s.preserve ? 1.0f : 0.0f);
}

void apply_eq(VoiceEQ& e, const PresetSpec& s) noexcept {
    e.low_cut.set(s.low_cut);
    e.high_cut.set(s.high_cut);
    e.peak_hz.set(s.peak_hz);
    e.peak_db.set(s.peak_db);
    e.peak_q.set(s.peak_q);
}

void apply_vocoder(Vocoder& e, const PresetSpec& s) noexcept {
    e.carrier.set(s.carrier);
    e.pitch.set(s.carrier_hz);
    e.bands.set(s.bands);
    e.attack_ms.set(s.voc_attack);
    e.release_ms.set(s.voc_release);
    e.sibilance.set(s.sibilance);
    e.shift.set(0.0f);
    e.mix.set(s.voc_mix);
}

void apply_ring(RingModulator& e, const PresetSpec& s) noexcept {
    e.frequency.set(s.ring_hz);
    e.shape.set(s.ring_shape);
    e.mix.set(s.ring_mix);
}

void apply_dist(Distortion& e, const PresetSpec& s) noexcept {
    e.mode.set(s.dist_mode);
    e.drive.set(s.drive);
    e.tone.set(s.tone);
    e.mix.set(s.dist_mix);
    e.output.set(s.dist_out);
    e.oversample.set(1.0f);
}

void apply_crush(Bitcrusher& e, const PresetSpec& s) noexcept {
    e.bits.set(s.bits);
    e.rate_hz.set(s.crush_hz);
    e.mix.set(1.0f);
}

void apply_flanger(Flanger& e, const PresetSpec& s) noexcept {
    e.rate.set(s.fl_rate);
    e.depth_ms.set(s.fl_depth);
    e.delay_ms.set(s.fl_delay);
    e.feedback.set(s.fl_feedback);
    e.mix.set(s.fl_mix);
}

void apply_chorus(Chorus& e, const PresetSpec& s) noexcept {
    e.rate.set(s.ch_rate);
    e.depth_ms.set(s.ch_depth);
    e.delay_ms.set(s.ch_delay);
    e.mix.set(s.ch_mix);
}

void apply_vibrato(Vibrato& e, const PresetSpec& s) noexcept {
    e.rate.set(s.vib_rate);
    e.depth_ms.set(s.vib_depth);
    e.mix.set(1.0f);
}

void apply_tremolo(Tremolo& e, const PresetSpec& s) noexcept {
    e.rate.set(s.tr_rate);
    e.depth.set(s.tr_depth);
}

void apply_delay(Delay& e, const PresetSpec& s) noexcept {
    e.sync.set(0.0f);
    e.time_ms.set(s.delay_ms);
    e.feedback.set(s.delay_fb);
    e.mix.set(s.delay_mix);
    e.low_cut.set(s.delay_lc);
    e.high_cut.set(s.delay_hc);
    e.pingpong.set(0.0f);
}

void apply_reverb(Reverb& e, const PresetSpec& s) noexcept {
    e.size.set(s.rv_size);
    e.decay.set(s.rv_decay);
    e.damping.set(s.rv_damping);
    e.predelay_ms.set(s.rv_predelay);
    e.mix.set(s.rv_mix);
}

} // namespace

std::size_t voice_preset_count() noexcept {
    return k_preset_count;
}

const char* voice_preset_name(VoicePreset preset) noexcept {
    const std::size_t i = static_cast<std::size_t>(preset);
    return i < k_preset_count ? k_preset_names[i] : "";
}

struct VoiceEQ::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    float cached[5] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    bool use_low = false;
    bool use_high = false;
    bool use_peak = false;
    VxCoeffs hp;
    VxCoeffs lp;
    VxCoeffs pk;
    std::vector<VxSection> sections;
};

VoiceEQ::VoiceEQ()
    : low_cut("low_cut", 20.0f, 20.0f, 2000.0f, "Hz"),
      high_cut("high_cut", 22000.0f, 500.0f, 22000.0f, "Hz"),
      peak_hz("peak_hz", 1000.0f, 50.0f, 12000.0f, "Hz"),
      peak_db("peak_db", 0.0f, -24.0f, 24.0f, "dB"),
      peak_q("peak_q", 1.0f, 0.2f, 10.0f) {
    add_parameter(low_cut);
    add_parameter(high_cut);
    add_parameter(peak_hz);
    add_parameter(peak_db);
    add_parameter(peak_q);
}

VoiceEQ::~VoiceEQ() = default;

void VoiceEQ::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    m_state->rate = static_cast<float>(r);
    m_state->channels = c;
    m_state->sections.assign(c * 5, VxSection());
    for (float& v : m_state->cached) v = -1.0f;
    reset();
}

void VoiceEQ::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const float values[5] = { low_cut.get(), high_cut.get(), peak_hz.get(), peak_db.get(), peak_q.get() };
    if (!std::equal(values, values + 5, s.cached)) {
        std::copy(values, values + 5, s.cached);
        s.use_low = values[0] > 20.5f;
        s.use_high = values[1] < std::min(21000.0f, 0.45f * s.rate);
        s.use_peak = std::fabs(values[3]) > 0.05f;
        s.hp = vx_design(VxType::highpass, s.rate, values[0], 0.7071, 0.0);
        s.lp = vx_design(VxType::lowpass, s.rate, values[1], 0.7071, 0.0);
        s.pk = vx_design(VxType::peak, s.rate, values[2], values[4], values[3]);
    }
    const std::size_t c = s.channels;
    for (std::size_t f = 0; f < frames; ++f) {
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) {
            VxSection* q = s.sections.data() + ch * 5;
            float v = x[ch] == x[ch] ? x[ch] : 0.0f;
            if (s.use_low) v = q[1].run(s.hp, q[0].run(s.hp, v));
            if (s.use_high) v = q[3].run(s.lp, q[2].run(s.lp, v));
            if (s.use_peak) v = q[4].run(s.pk, v);
            x[ch] = v;
        }
    }
}

void VoiceEQ::reset() noexcept {
    if (!m_state) return;
    for (VxSection& q : m_state->sections) q = VxSection();
}

struct RingModulator::State {
    float rate = 48000.0f;
    std::size_t channels = 1;
    double phase = 0.0;
    float k_param = 0.01f;
    float mix = 1.0f;
};

RingModulator::RingModulator()
    : frequency("frequency", 440.0f, 0.1f, 5000.0f, "Hz"),
      shape("shape", 0.0f, 0.0f, 5.0f),
      mix("mix", 1.0f, 0.0f, 1.0f) {
    add_parameter(frequency);
    add_parameter(shape);
    add_parameter(mix);
}

RingModulator::~RingModulator() = default;

void RingModulator::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    m_state->rate = static_cast<float>(r);
    m_state->channels = c;
    m_state->k_param = smoothing(m_state->rate, 10.0f);
    reset();
}

void RingModulator::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const double inc = static_cast<double>(frequency.get()) / static_cast<double>(s.rate);
    const LfoShape sh = static_cast<LfoShape>(std::min(5, std::max(0, static_cast<int>(shape.get() + 0.5f))));
    const float mix_t = mix.get();
    for (std::size_t f = 0; f < frames; ++f) {
        s.phase += inc;
        if (s.phase >= 1048576.0) s.phase -= 1048576.0;
        s.mix += (mix_t - s.mix) * s.k_param;
        const float m = lfo_value(sh, s.phase);
        const float g = 1.0f - s.mix + s.mix * m;
        float* x = io + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) x[ch] *= g;
    }
}

void RingModulator::reset() noexcept {
    if (!m_state) return;
    m_state->phase = 0.0;
    m_state->mix = mix.get();
}

struct Vocoder::State {
    static constexpr std::size_t max_bands = 32;
    float rate = 48000.0f;
    std::size_t channels = 1;
    std::size_t count = 0;
    float cached_bands = -1.0f;
    float cached_shift = -100.0f;
    VxCoeffs mod_c[max_bands];
    VxCoeffs car_c[max_bands];
    VxSection mod_s[max_bands][2];
    VxSection car_s[max_bands][2];
    float env_m[max_bands] = {};
    float env_c[max_bands] = {};
    VxCoeffs sib_c;
    VxSection sib_s[2];
    double phase = 0.0;
    std::uint32_t noise = 0x9E3779B9u;
    float k_param = 0.01f;
    float mix = 1.0f;
    std::vector<float> ring;
    std::size_t ring_mask = 0;
    std::atomic<std::size_t> write{ 0 };
    std::atomic<std::size_t> read{ 0 };

    void design(std::size_t n, float shift_st) noexcept {
        count = n;
        const double lo = 100.0;
        const double hi = std::min(8000.0, 0.45 * static_cast<double>(rate));
        const double ratio = std::pow(hi / lo, 1.0 / static_cast<double>(n));
        const double q = 1.0 / (std::sqrt(ratio) - 1.0 / std::sqrt(ratio));
        const double factor = std::pow(2.0, static_cast<double>(shift_st) / 12.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double fc = lo * std::pow(ratio, static_cast<double>(i) + 0.5);
            mod_c[i] = vx_design(VxType::bandpass, rate, fc, q, 0.0);
            car_c[i] = vx_design(VxType::bandpass, rate, std::min(0.45 * static_cast<double>(rate), fc * factor), q, 0.0);
        }
    }
};

Vocoder::Vocoder()
    : carrier("carrier", 0.0f, 0.0f, 3.0f),
      pitch("pitch", 110.0f, 20.0f, 1000.0f, "Hz"),
      bands("bands", 24.0f, 16.0f, 32.0f),
      attack_ms("attack_ms", 4.0f, 0.5f, 100.0f, "ms"),
      release_ms("release_ms", 30.0f, 5.0f, 500.0f, "ms"),
      sibilance("sibilance", 0.3f, 0.0f, 1.0f),
      shift("shift", 0.0f, -12.0f, 12.0f, "st"),
      mix("mix", 1.0f, 0.0f, 1.0f) {
    add_parameter(carrier);
    add_parameter(pitch);
    add_parameter(bands);
    add_parameter(attack_ms);
    add_parameter(release_ms);
    add_parameter(sibilance);
    add_parameter(shift);
    add_parameter(mix);
}

Vocoder::~Vocoder() = default;

void Vocoder::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    s.k_param = smoothing(s.rate, 10.0f);
    s.sib_c = vx_design(VxType::highpass, s.rate, std::min(5000.0f, 0.4f * s.rate), 0.7071, 0.0);
    const std::size_t size = next_power_of_two(static_cast<std::size_t>(s.rate) + 1);
    s.ring.assign(size, 0.0f);
    s.ring_mask = size - 1;
    s.write.store(0);
    s.read.store(0);
    s.cached_bands = -1.0f;
    reset();
}

std::size_t Vocoder::push_sidechain(const float* mono, std::size_t frames) noexcept {
    if (!m_state || !mono) return 0;
    State& s = *m_state;
    if (s.ring.empty()) return 0;
    const std::size_t w = s.write.load(std::memory_order_relaxed);
    const std::size_t r = s.read.load(std::memory_order_acquire);
    const std::size_t space = s.ring.size() - (w - r);
    const std::size_t n = std::min(frames, space);
    for (std::size_t i = 0; i < n; ++i) s.ring[(w + i) & s.ring_mask] = mono[i] == mono[i] ? mono[i] : 0.0f;
    s.write.store(w + n, std::memory_order_release);
    return n;
}

std::size_t Vocoder::sidechain_available() const noexcept {
    if (!m_state) return 0;
    return m_state->write.load(std::memory_order_acquire) - m_state->read.load(std::memory_order_acquire);
}

void Vocoder::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const float nb = std::floor(bands.get() + 0.5f);
    const float sh = shift.get();
    if (nb != s.cached_bands || sh != s.cached_shift) {
        s.design(static_cast<std::size_t>(clampf(nb, 1.0f, static_cast<float>(State::max_bands))), sh);
        s.cached_bands = nb;
        s.cached_shift = sh;
    }
    const int src = std::min(3, std::max(0, static_cast<int>(carrier.get() + 0.5f)));
    const double dt = std::min(0.45, static_cast<double>(pitch.get()) / static_cast<double>(s.rate));
    const float att = smoothing(s.rate, attack_ms.get());
    const float rel = smoothing(s.rate, release_ms.get());
    const float sib = sibilance.get();
    const float mix_t = mix.get();
    const float inv_c = 1.0f / static_cast<float>(c);
    const std::size_t n = s.count;
    std::size_t rd = s.read.load(std::memory_order_relaxed);
    const std::size_t wr = src == 3 ? s.write.load(std::memory_order_acquire) : rd;
    for (std::size_t f = 0; f < frames; ++f) {
        s.mix += (mix_t - s.mix) * s.k_param;
        float* x = io + f * c;
        float m = 0.0f;
        for (std::size_t ch = 0; ch < c; ++ch) m += x[ch] == x[ch] ? x[ch] : 0.0f;
        m *= inv_c;
        float car = 0.0f;
        if (src == 0 || src == 1) {
            s.phase += dt;
            if (s.phase >= 1.0) s.phase -= 1.0;
            if (src == 0) {
                car = static_cast<float>(2.0 * s.phase - 1.0) - poly_blep(s.phase, dt);
            } else {
                double t2 = s.phase + 0.5;
                if (t2 >= 1.0) t2 -= 1.0;
                car = (s.phase < 0.5 ? 1.0f : -1.0f) + poly_blep(s.phase, dt) - poly_blep(t2, dt);
            }
        } else if (src == 2) {
            s.noise ^= s.noise << 13;
            s.noise ^= s.noise >> 17;
            s.noise ^= s.noise << 5;
            car = static_cast<float>(s.noise) * (2.0f / 4294967295.0f) - 1.0f;
        } else if (rd != wr) {
            car = s.ring[rd & s.ring_mask];
            ++rd;
        }
        float out = 0.0f;
        for (std::size_t b = 0; b < n; ++b) {
            const float mb = s.mod_s[b][1].run(s.mod_c[b], s.mod_s[b][0].run(s.mod_c[b], m));
            const float cb = s.car_s[b][1].run(s.car_c[b], s.car_s[b][0].run(s.car_c[b], car));
            const float am = std::fabs(mb);
            const float ac = std::fabs(cb);
            s.env_m[b] = flush(s.env_m[b] + (am > s.env_m[b] ? att : rel) * (am - s.env_m[b]));
            s.env_c[b] = flush(s.env_c[b] + (ac > s.env_c[b] ? att : rel) * (ac - s.env_c[b]));
            out += cb * s.env_m[b] / (s.env_c[b] + 1e-4f);
        }
        if (sib > 0.0f) out += sib * s.sib_s[1].run(s.sib_c, s.sib_s[0].run(s.sib_c, m));
        out = flush(out);
        for (std::size_t ch = 0; ch < c; ++ch) x[ch] = x[ch] * (1.0f - s.mix) + out * s.mix;
    }
    if (src == 3) s.read.store(rd, std::memory_order_release);
}

void Vocoder::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    for (std::size_t b = 0; b < State::max_bands; ++b) {
        s.mod_s[b][0] = s.mod_s[b][1] = VxSection();
        s.car_s[b][0] = s.car_s[b][1] = VxSection();
        s.env_m[b] = 0.0f;
        s.env_c[b] = 0.0f;
    }
    s.sib_s[0] = s.sib_s[1] = VxSection();
    s.phase = 0.0;
    s.mix = mix.get();
}

namespace {

struct Slot {
    PitchShifter pitch;
    VoiceEQ eq;
    Vocoder vocoder;
    RingModulator ring;
    Distortion dist;
    Bitcrusher crush;
    Flanger flanger;
    Chorus chorus;
    Vibrato vibrato;
    Tremolo tremolo;
    Delay delay;
    Reverb reverb;
    PresetSpec spec;
    int preset = -1;
    std::size_t channels = 1;
    std::vector<float> comp;
    std::size_t comp_size = 0;
    std::size_t comp_mask = 0;
    std::size_t comp_pos = 0;
    std::size_t comp_delay = 0;
    float makeup = 1.0f;

    void prepare(std::uint32_t rate, std::size_t c) {
        channels = c;
        pitch.prepare(rate, c);
        eq.prepare(rate, c);
        vocoder.prepare(rate, c);
        ring.prepare(rate, c);
        dist.prepare(rate, c);
        crush.prepare(rate, c);
        flanger.prepare(rate, c);
        chorus.prepare(rate, c);
        vibrato.prepare(rate, c);
        tremolo.prepare(rate, c);
        delay.prepare(rate, c);
        reverb.prepare(rate, c);
        pitch.mode.set(0.0f);
        comp_size = next_power_of_two(pitch.latency_frames() + dist.latency_frames() + 64);
        comp_mask = comp_size - 1;
        comp.assign(comp_size * c, 0.0f);
        preset = -1;
    }

    void configure(int index, std::size_t pitch_latency, std::size_t dist_latency) noexcept {
        preset = index;
        spec = preset_spec(static_cast<VoicePreset>(index));
        if (spec.use_pitch) {
            apply_pitch(pitch, spec);
            pitch.reset();
        }
        if (spec.eq) {
            apply_eq(eq, spec);
            eq.reset();
        }
        if (spec.vocoder) {
            apply_vocoder(vocoder, spec);
            vocoder.reset();
        }
        if (spec.ring) {
            apply_ring(ring, spec);
            ring.reset();
        }
        if (spec.dist) {
            apply_dist(dist, spec);
            dist.reset();
        }
        if (spec.crush) {
            apply_crush(crush, spec);
            crush.reset();
        }
        if (spec.flanger) {
            apply_flanger(flanger, spec);
            flanger.reset();
        }
        if (spec.chorus) {
            apply_chorus(chorus, spec);
            chorus.reset();
        }
        if (spec.vibrato) {
            apply_vibrato(vibrato, spec);
            vibrato.reset();
        }
        if (spec.tremolo) {
            apply_tremolo(tremolo, spec);
            tremolo.reset();
        }
        if (spec.delay) {
            apply_delay(delay, spec);
            delay.reset();
        }
        if (spec.reverb) {
            apply_reverb(reverb, spec);
            reverb.reset();
        }
        comp_delay = (spec.use_pitch ? 0 : pitch_latency) + (spec.dist ? 0 : dist_latency);
        std::fill(comp.begin(), comp.end(), 0.0f);
        comp_pos = 0;
        makeup = db_to_gain(spec.makeup_db);
    }

    void process(float* buf, std::size_t frames) noexcept {
        const std::size_t c = channels;
        if (spec.use_pitch) pitch.process(buf, frames);
        if (comp_delay > 0) {
            for (std::size_t f = 0; f < frames; ++f) {
                float* x = buf + f * c;
                const std::size_t w = comp_pos & comp_mask;
                const std::size_t r = (comp_pos - comp_delay) & comp_mask;
                for (std::size_t ch = 0; ch < c; ++ch) {
                    comp[w * c + ch] = x[ch];
                    x[ch] = comp[r * c + ch];
                }
                ++comp_pos;
            }
        }
        if (spec.crush) crush.process(buf, frames);
        if (spec.eq) eq.process(buf, frames);
        if (spec.vocoder) vocoder.process(buf, frames);
        if (spec.ring) ring.process(buf, frames);
        if (spec.dist) dist.process(buf, frames);
        if (spec.flanger) flanger.process(buf, frames);
        if (spec.chorus) chorus.process(buf, frames);
        if (spec.vibrato) vibrato.process(buf, frames);
        if (spec.tremolo) tremolo.process(buf, frames);
        if (spec.delay) delay.process(buf, frames);
        if (spec.reverb) reverb.process(buf, frames);
        if (makeup != 1.0f) {
            for (std::size_t i = 0; i < frames * c; ++i) buf[i] *= makeup;
        }
    }

    std::size_t tail() const noexcept {
        std::size_t t = 0;
        if (spec.delay) t = std::max(t, delay.tail_frames());
        if (spec.reverb) t = std::max(t, reverb.tail_frames());
        return t;
    }
};

} // namespace

struct VoiceChanger::State {
    static constexpr std::size_t chunk = 256;
    Slot slots[2];
    int active = 0;
    bool fading = false;
    std::size_t fade_pos = 0;
    std::size_t fade_len = 1;
    std::size_t channels = 1;
    float rate = 48000.0f;
    std::size_t pitch_latency = 0;
    std::size_t dist_latency = 0;
    std::size_t latency = 0;
    std::vector<float> a;
    std::vector<float> b;
    std::vector<float> dry;
    std::size_t dry_mask = 0;
    std::size_t dry_pos = 0;
    std::vector<float> history;
    std::size_t history_mask = 0;
    std::size_t history_pos = 0;
    std::size_t warm = 0;
    std::vector<float> warm_buf;
    float k_param = 0.01f;
    float mix = 1.0f;
    float gain = 1.0f;

    void warm_up(Slot& slot) noexcept {
        const std::size_t c = channels;
        const std::size_t n = std::min(warm, history_pos);
        const std::size_t start = history_pos - n;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t h = (start + i) & history_mask;
            std::memcpy(warm_buf.data() + i * c, history.data() + h * c, c * sizeof(float));
        }
        for (std::size_t off = 0; off < n; off += chunk) slot.process(warm_buf.data() + off * c, std::min(chunk, n - off));
    }
};

VoiceChanger::VoiceChanger(VoicePreset initial)
    : preset("preset", static_cast<float>(initial), 0.0f, static_cast<float>(k_preset_count - 1)),
      mix("mix", 1.0f, 0.0f, 1.0f),
      output_gain("output_gain", 0.0f, -24.0f, 12.0f, "dB") {
    add_parameter(preset);
    add_parameter(mix);
    add_parameter(output_gain);
    m_current.store(static_cast<int>(initial));
}

VoiceChanger::~VoiceChanger() = default;

void VoiceChanger::prepare(std::uint32_t sample_rate, std::size_t channel_count) {
    const std::uint32_t r = sample_rate ? sample_rate : 48000;
    const std::size_t c = channel_count ? channel_count : 1;
    set_format(r, c);
    if (!m_state) m_state.reset(new State());
    State& s = *m_state;
    s.rate = static_cast<float>(r);
    s.channels = c;
    for (Slot& slot : s.slots) slot.prepare(r, c);
    s.pitch_latency = s.slots[0].pitch.latency_frames();
    s.dist_latency = s.slots[0].dist.latency_frames();
    s.latency = s.pitch_latency + s.dist_latency;
    s.a.assign(State::chunk * c, 0.0f);
    s.b.assign(State::chunk * c, 0.0f);
    const std::size_t dry_size = next_power_of_two(s.latency + State::chunk + 1);
    s.dry.assign(dry_size * c, 0.0f);
    s.dry_mask = dry_size - 1;
    s.warm = s.latency + 256;
    const std::size_t hist = next_power_of_two(s.warm + 1);
    s.history.assign(hist * c, 0.0f);
    s.history_mask = hist - 1;
    s.warm_buf.assign(s.warm * c, 0.0f);
    s.fade_len = std::max<std::size_t>(1, static_cast<std::size_t>(0.04f * s.rate));
    s.k_param = smoothing(s.rate, 10.0f);
    reset();
}

void VoiceChanger::process(float* io, std::size_t frames) noexcept {
    if (!m_state || !io) return;
    State& s = *m_state;
    const std::size_t c = s.channels;
    const float mix_t = mix.get();
    const float gain_t = db_to_gain(output_gain.get());
    for (std::size_t off = 0; off < frames; off += State::chunk) {
        const std::size_t n = std::min(State::chunk, frames - off);
        float* x = io + off * c;
        const int target = std::min(static_cast<int>(k_preset_count) - 1, std::max(0, static_cast<int>(preset.get() + 0.5f)));
        if (!s.fading && target != s.slots[s.active].preset) {
            Slot& next = s.slots[1 - s.active];
            next.configure(target, s.pitch_latency, s.dist_latency);
            s.warm_up(next);
            s.fading = true;
            s.fade_pos = 0;
            m_fading.store(true, std::memory_order_relaxed);
        }
        for (std::size_t i = 0; i < n * c; ++i) {
            const float v = x[i];
            s.a[i] = v == v ? v : 0.0f;
        }
        s.slots[s.active].process(s.a.data(), n);
        if (s.fading) {
            for (std::size_t i = 0; i < n * c; ++i) {
                const float v = x[i];
                s.b[i] = v == v ? v : 0.0f;
            }
            s.slots[1 - s.active].process(s.b.data(), n);
        }
        for (std::size_t f = 0; f < n; ++f) {
            float* p = x + f * c;
            const std::size_t hp = s.history_pos & s.history_mask;
            const std::size_t dw = s.dry_pos & s.dry_mask;
            const std::size_t dr = (s.dry_pos - s.latency) & s.dry_mask;
            float wa = 1.0f;
            float wb = 0.0f;
            if (s.fading) {
                const float t = static_cast<float>(s.fade_pos) / static_cast<float>(s.fade_len);
                wb = 0.5f - 0.5f * std::cos(static_cast<float>(k_pi) * std::min(1.0f, t));
                wa = 1.0f - wb;
                ++s.fade_pos;
            }
            s.mix += (mix_t - s.mix) * s.k_param;
            s.gain += (gain_t - s.gain) * s.k_param;
            for (std::size_t ch = 0; ch < c; ++ch) {
                const float in = p[ch] == p[ch] ? p[ch] : 0.0f;
                s.history[hp * c + ch] = in;
                s.dry[dw * c + ch] = in;
                float wet = s.a[f * c + ch] * wa;
                if (s.fading) wet += s.b[f * c + ch] * wb;
                const float out = (s.dry[dr * c + ch] * (1.0f - s.mix) + wet * s.mix) * s.gain;
                p[ch] = out == out ? out : 0.0f;
            }
            ++s.history_pos;
            ++s.dry_pos;
        }
        if (s.fading && s.fade_pos >= s.fade_len) {
            s.active = 1 - s.active;
            s.fading = false;
            m_fading.store(false, std::memory_order_relaxed);
            m_current.store(s.slots[s.active].preset, std::memory_order_relaxed);
        }
    }
}

void VoiceChanger::reset() noexcept {
    if (!m_state) return;
    State& s = *m_state;
    const int target = std::min(static_cast<int>(k_preset_count) - 1, std::max(0, static_cast<int>(preset.get() + 0.5f)));
    s.active = 0;
    s.fading = false;
    s.fade_pos = 0;
    s.slots[0].configure(target, s.pitch_latency, s.dist_latency);
    s.slots[1].preset = -1;
    std::fill(s.dry.begin(), s.dry.end(), 0.0f);
    std::fill(s.history.begin(), s.history.end(), 0.0f);
    s.dry_pos = 0;
    s.history_pos = 0;
    s.mix = mix.get();
    s.gain = db_to_gain(output_gain.get());
    m_current.store(target, std::memory_order_relaxed);
    m_fading.store(false, std::memory_order_relaxed);
}

std::size_t VoiceChanger::latency_frames() const noexcept {
    return m_state ? m_state->latency : 0;
}

std::size_t VoiceChanger::tail_frames() const noexcept {
    if (!m_state) return 0;
    const State& s = *m_state;
    std::size_t t = s.slots[s.active].tail();
    if (s.fading) t = std::max(t, s.slots[1 - s.active].tail());
    return t;
}

std::shared_ptr<EffectChain> make_voice_chain(VoicePreset preset) {
    auto chain = std::make_shared<EffectChain>();
    const PresetSpec s = preset_spec(preset);
    if (s.use_pitch) {
        auto e = std::make_shared<PitchShifter>();
        apply_pitch(*e, s);
        chain->add(e);
    }
    if (s.crush) {
        auto e = std::make_shared<Bitcrusher>();
        apply_crush(*e, s);
        chain->add(e);
    }
    if (s.eq) {
        auto e = std::make_shared<VoiceEQ>();
        apply_eq(*e, s);
        chain->add(e);
    }
    if (s.vocoder) {
        auto e = std::make_shared<Vocoder>();
        apply_vocoder(*e, s);
        chain->add(e);
    }
    if (s.ring) {
        auto e = std::make_shared<RingModulator>();
        apply_ring(*e, s);
        chain->add(e);
    }
    if (s.dist) {
        auto e = std::make_shared<Distortion>();
        apply_dist(*e, s);
        chain->add(e);
    }
    if (s.flanger) {
        auto e = std::make_shared<Flanger>();
        apply_flanger(*e, s);
        chain->add(e);
    }
    if (s.chorus) {
        auto e = std::make_shared<Chorus>();
        apply_chorus(*e, s);
        chain->add(e);
    }
    if (s.vibrato) {
        auto e = std::make_shared<Vibrato>();
        apply_vibrato(*e, s);
        chain->add(e);
    }
    if (s.tremolo) {
        auto e = std::make_shared<Tremolo>();
        apply_tremolo(*e, s);
        chain->add(e);
    }
    if (s.delay) {
        auto e = std::make_shared<Delay>();
        apply_delay(*e, s);
        chain->add(e);
    }
    if (s.reverb) {
        auto e = std::make_shared<Reverb>();
        apply_reverb(*e, s);
        chain->add(e);
    }
    if (s.makeup_db != 0.0f) chain->add(std::make_shared<Gain>(s.makeup_db));
    return chain;
}

} // namespace audio
} // namespace fizmo
