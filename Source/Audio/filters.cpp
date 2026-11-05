#include "filters.hpp"
#include "format.hpp"
#include "presets.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;
const double k_sqrt_half = 0.70710678118654752440;
const std::size_t k_type_count = 8;

FilterType type_from(float value) noexcept {
    const long v = std::lround(value);
    if (v <= 0) return FilterType::lowpass;
    if (v >= static_cast<long>(k_type_count) - 1) return FilterType::allpass;
    return static_cast<FilterType>(v);
}

std::size_t stages_from(float slope_db) noexcept {
    const long v = std::lround(slope_db / 12.0f);
    return static_cast<std::size_t>(std::min<long>(4, std::max<long>(1, v)));
}

bool cascadable(FilterType type) noexcept {
    return type != FilterType::peak && type != FilterType::lowshelf && type != FilterType::highshelf;
}

bool gain_type(FilterType type) noexcept {
    return type == FilterType::peak || type == FilterType::lowshelf || type == FilterType::highshelf;
}

BiquadCoefficients lerp(const BiquadCoefficients& a, const BiquadCoefficients& b, double t) noexcept {
    BiquadCoefficients r;
    r.b0 = a.b0 + (b.b0 - a.b0) * t;
    r.b1 = a.b1 + (b.b1 - a.b1) * t;
    r.b2 = a.b2 + (b.b2 - a.b2) * t;
    r.a1 = a.a1 + (b.a1 - a.a1) * t;
    r.a2 = a.a2 + (b.a2 - a.a2) * t;
    return r;
}

double max_difference(const BiquadCoefficients& a, const BiquadCoefficients& b) noexcept {
    double d = std::fabs(a.b0 - b.b0);
    d = std::max(d, std::fabs(a.b1 - b.b1));
    d = std::max(d, std::fabs(a.b2 - b.b2));
    d = std::max(d, std::fabs(a.a1 - b.a1));
    d = std::max(d, std::fabs(a.a2 - b.a2));
    return d;
}

std::size_t tail_of(const BiquadCoefficients& c, double rate) noexcept {
    if (c.is_identity()) return 0;
    const double r = c.pole_radius();
    if (r <= 1e-9) return 2;
    if (r >= 1.0) return static_cast<std::size_t>(rate * 10.0);
    const double n = std::log(1e-6) / std::log(r);
    return static_cast<std::size_t>(std::min(rate * 10.0, std::ceil(n))) + 2;
}

const char* band_parameter_name(std::size_t band, std::size_t field) {
    static const std::vector<std::string> names = [] {
        static const char* const fields[5] = { "enabled", "type", "frequency", "q", "gain" };
        std::vector<std::string> out;
        out.reserve(ParametricEq::max_bands * 5);
        for (std::size_t b = 0; b < ParametricEq::max_bands; ++b) {
            for (std::size_t f = 0; f < 5; ++f) out.push_back("band" + std::to_string(b + 1) + "_" + fields[f]);
        }
        return out;
    }();
    return names[band * 5 + field].c_str();
}

double one_pole(double seconds, double rate, double frames) noexcept {
    if (seconds <= 0.0 || rate <= 0.0) return 1.0;
    return 1.0 - std::exp(-frames / (seconds * rate));
}

} // namespace

const char* filter_type_name(FilterType type) noexcept {
    switch (type) {
        case FilterType::lowpass: return "lowpass";
        case FilterType::highpass: return "highpass";
        case FilterType::bandpass: return "bandpass";
        case FilterType::notch: return "notch";
        case FilterType::peak: return "peak";
        case FilterType::lowshelf: return "lowshelf";
        case FilterType::highshelf: return "highshelf";
        case FilterType::allpass: return "allpass";
    }
    return "unknown";
}

bool BiquadCoefficients::is_identity() const noexcept {
    return b0 == 1.0 && b1 == 0.0 && b2 == 0.0 && a1 == 0.0 && a2 == 0.0;
}

std::complex<double> BiquadCoefficients::response(double frequency, double sample_rate) const noexcept {
    const double w = 2.0 * k_pi * frequency / sample_rate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
}

double BiquadCoefficients::magnitude(double frequency, double sample_rate) const noexcept {
    return std::abs(response(frequency, sample_rate));
}

double BiquadCoefficients::magnitude_db(double frequency, double sample_rate) const noexcept {
    const double m = magnitude(frequency, sample_rate);
    return m > 1e-15 ? 20.0 * std::log10(m) : -300.0;
}

double BiquadCoefficients::pole_radius() const noexcept {
    const double disc = a1 * a1 - 4.0 * a2;
    if (disc < 0.0) return std::sqrt(std::max(0.0, a2));
    const double s = std::sqrt(disc);
    return std::max(std::fabs((-a1 + s) * 0.5), std::fabs((-a1 - s) * 0.5));
}

BiquadCoefficients design_biquad(FilterType type, double sample_rate, double frequency, double q, double gain_db) {
    if (!(sample_rate > 0.0)) sample_rate = 48000.0;
    if (!(frequency == frequency)) frequency = 1000.0;
    if (!(q == q)) q = k_sqrt_half;
    if (!(gain_db == gain_db)) gain_db = 0.0;
    frequency = std::min(sample_rate * 0.49, std::max(sample_rate * 1e-6, frequency));
    q = std::min(1000.0, std::max(1e-3, q));
    const double w0 = 2.0 * k_pi * frequency / sample_rate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double alpha = sw / (2.0 * q);
    const double a = std::pow(10.0, gain_db / 40.0);
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a0 = 1.0;
    double a1 = 0.0;
    double a2 = 0.0;
    switch (type) {
        case FilterType::lowpass:
            b0 = (1.0 - cw) * 0.5;
            b1 = 1.0 - cw;
            b2 = (1.0 - cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case FilterType::highpass:
            b0 = (1.0 + cw) * 0.5;
            b1 = -(1.0 + cw);
            b2 = (1.0 + cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case FilterType::bandpass:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case FilterType::notch:
            b0 = 1.0;
            b1 = -2.0 * cw;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case FilterType::peak:
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cw;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha / a;
            break;
        case FilterType::lowshelf: {
            const double s = 2.0 * std::sqrt(a) * alpha;
            b0 = a * ((a + 1.0) - (a - 1.0) * cw + s);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cw);
            b2 = a * ((a + 1.0) - (a - 1.0) * cw - s);
            a0 = (a + 1.0) + (a - 1.0) * cw + s;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cw);
            a2 = (a + 1.0) + (a - 1.0) * cw - s;
            break;
        }
        case FilterType::highshelf: {
            const double s = 2.0 * std::sqrt(a) * alpha;
            b0 = a * ((a + 1.0) + (a - 1.0) * cw + s);
            b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cw);
            b2 = a * ((a + 1.0) + (a - 1.0) * cw - s);
            a0 = (a + 1.0) - (a - 1.0) * cw + s;
            a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cw);
            a2 = (a + 1.0) - (a - 1.0) * cw - s;
            break;
        }
        case FilterType::allpass:
            b0 = 1.0 - alpha;
            b1 = -2.0 * cw;
            b2 = 1.0 + alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
    }
    BiquadCoefficients c;
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

std::vector<double> butterworth_q(std::size_t stages) {
    std::vector<double> q;
    if (stages == 0) return q;
    const double n = static_cast<double>(2 * stages);
    for (std::size_t k = 1; k <= stages; ++k) {
        q.push_back(1.0 / (2.0 * std::cos(k_pi * static_cast<double>(2 * k - 1) / (2.0 * n))));
    }
    return q;
}

Biquad::Biquad(std::size_t channels) {
    prepare(channels);
}

void Biquad::prepare(std::size_t channels) {
    m_z1.assign(channels, 0.0);
    m_z2.assign(channels, 0.0);
}

void Biquad::set_coefficients(const BiquadCoefficients& c) noexcept {
    m_c = c;
    m_target = c;
    m_remaining = 0;
}

void Biquad::set_target(const BiquadCoefficients& c, std::size_t ramp_frames) noexcept {
    if (ramp_frames == 0) {
        set_coefficients(c);
        return;
    }
    m_target = c;
    const double inv = 1.0 / static_cast<double>(ramp_frames);
    m_step.b0 = (c.b0 - m_c.b0) * inv;
    m_step.b1 = (c.b1 - m_c.b1) * inv;
    m_step.b2 = (c.b2 - m_c.b2) * inv;
    m_step.a1 = (c.a1 - m_c.a1) * inv;
    m_step.a2 = (c.a2 - m_c.a2) * inv;
    m_remaining = ramp_frames;
}

void Biquad::reset() noexcept {
    std::fill(m_z1.begin(), m_z1.end(), 0.0);
    std::fill(m_z2.begin(), m_z2.end(), 0.0);
}

void Biquad::advance() noexcept {
    if (m_remaining == 0) return;
    if (--m_remaining == 0) {
        m_c = m_target;
        return;
    }
    m_c.b0 += m_step.b0;
    m_c.b1 += m_step.b1;
    m_c.b2 += m_step.b2;
    m_c.a1 += m_step.a1;
    m_c.a2 += m_step.a2;
}

double Biquad::process_sample(std::size_t channel, double x) noexcept {
    const double y = m_c.b0 * x + m_z1[channel];
    m_z1[channel] = m_c.b1 * x - m_c.a1 * y + m_z2[channel];
    m_z2[channel] = m_c.b2 * x - m_c.a2 * y;
    return y;
}

void Biquad::process_frame(float* frame) noexcept {
    const std::size_t c = m_z1.size();
    for (std::size_t ch = 0; ch < c; ++ch) frame[ch] = static_cast<float>(process_sample(ch, frame[ch]));
    advance();
}

void Biquad::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = m_z1.size();
    if (c == 0) return;
    if (m_remaining == 0) {
        const double b0 = m_c.b0;
        const double b1 = m_c.b1;
        const double b2 = m_c.b2;
        const double a1 = m_c.a1;
        const double a2 = m_c.a2;
        for (std::size_t ch = 0; ch < c; ++ch) {
            double z1 = m_z1[ch];
            double z2 = m_z2[ch];
            float* p = interleaved + ch;
            for (std::size_t f = 0; f < frames; ++f) {
                const double x = p[f * c];
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                p[f * c] = static_cast<float>(y);
            }
            m_z1[ch] = z1;
            m_z2[ch] = z2;
        }
    } else {
        for (std::size_t f = 0; f < frames; ++f) process_frame(interleaved + f * c);
    }
    for (std::size_t ch = 0; ch < c; ++ch) {
        if (std::fabs(m_z1[ch]) < 1e-30) m_z1[ch] = 0.0;
        if (std::fabs(m_z2[ch]) < 1e-30) m_z2[ch] = 0.0;
    }
}

void SmoothedBiquad::design(FilterType type, double sample_rate, double frequency, double q, double gain_db, std::size_t stages, bool active, BiquadCoefficients* out) noexcept {
    for (std::size_t i = 0; i < max_stages; ++i) out[i] = BiquadCoefficients::identity();
    if (!active) return;
    stages = std::min(max_stages, std::max<std::size_t>(1, stages));
    if (!cascadable(type)) stages = 1;
    if (type == FilterType::lowpass || type == FilterType::highpass) {
        const double n = static_cast<double>(2 * stages);
        const double scale = q / k_sqrt_half;
        for (std::size_t k = 1; k <= stages; ++k) {
            double qk = 1.0 / (2.0 * std::cos(k_pi * static_cast<double>(2 * k - 1) / (2.0 * n)));
            if (k == stages) qk *= scale;
            out[k - 1] = design_biquad(type, sample_rate, frequency, qk, gain_db);
        }
        return;
    }
    const BiquadCoefficients c = design_biquad(type, sample_rate, frequency, q, gain_db);
    for (std::size_t i = 0; i < stages; ++i) out[i] = c;
}

void SmoothedBiquad::prepare(double sample_rate, std::size_t channels) {
    m_rate = sample_rate > 0.0 ? sample_rate : 48000.0;
    m_channels = channels;
    for (Biquad& b : m_stages) b.prepare(channels);
    set_smoothing(m_param_seconds, m_coef_seconds);
    snap();
}

void SmoothedBiquad::set_smoothing(double parameter_seconds, double coefficient_seconds) noexcept {
    m_param_seconds = parameter_seconds;
    m_coef_seconds = coefficient_seconds;
    m_param_alpha = one_pole(parameter_seconds, m_rate, static_cast<double>(block_frames));
    m_coef_alpha = one_pole(coefficient_seconds, m_rate, static_cast<double>(block_frames));
}

void SmoothedBiquad::set(FilterType type, double frequency, double q, double gain_db, std::size_t stages, bool active) noexcept {
    frequency = std::min(m_rate * 0.49, std::max(1e-3, frequency));
    q = std::min(1000.0, std::max(1e-3, q));
    if (type != m_type || frequency != m_target_freq || q != m_target_q || gain_db != m_target_gain || stages != m_target_stages || active != m_active) {
        m_type = type;
        m_target_freq = frequency;
        m_target_q = q;
        m_target_gain = gain_db;
        m_target_stages = stages;
        m_active = active;
        m_settled = false;
    }
}

void SmoothedBiquad::snap() noexcept {
    m_log_freq = std::log(m_target_freq);
    m_log_q = std::log(m_target_q);
    m_gain = m_target_gain;
    design(m_type, m_rate, m_target_freq, m_target_q, m_target_gain, m_target_stages, m_active, m_coef.data());
    for (std::size_t i = 0; i < max_stages; ++i) m_stages[i].set_coefficients(m_coef[i]);
    m_settled = true;
    m_countdown = 0;
}

void SmoothedBiquad::reset() noexcept {
    for (Biquad& b : m_stages) b.reset();
    snap();
}

void SmoothedBiquad::update() noexcept {
    m_countdown = block_frames;
    if (m_settled) return;
    const double lf = std::log(m_target_freq);
    const double lq = std::log(m_target_q);
    bool params_done = true;
    m_log_freq += (lf - m_log_freq) * m_param_alpha;
    if (std::fabs(lf - m_log_freq) < 1e-4) m_log_freq = lf; else params_done = false;
    m_log_q += (lq - m_log_q) * m_param_alpha;
    if (std::fabs(lq - m_log_q) < 1e-4) m_log_q = lq; else params_done = false;
    m_gain += (m_target_gain - m_gain) * m_param_alpha;
    if (std::fabs(m_target_gain - m_gain) < 1e-4) m_gain = m_target_gain; else params_done = false;
    BiquadCoefficients d[max_stages];
    design(m_type, m_rate, std::exp(m_log_freq), std::exp(m_log_q), m_gain, m_target_stages, m_active, d);
    bool coef_done = true;
    for (std::size_t i = 0; i < max_stages; ++i) {
        BiquadCoefficients next = lerp(m_coef[i], d[i], m_coef_alpha);
        if (max_difference(next, d[i]) < 1e-9) next = d[i]; else coef_done = false;
        m_coef[i] = next;
        m_stages[i].set_target(next, block_frames);
    }
    m_settled = params_done && coef_done;
}

bool SmoothedBiquad::bypassed() const noexcept {
    for (const Biquad& b : m_stages) {
        if (b.ramping() || !b.coefficients().is_identity()) return false;
    }
    return true;
}

void SmoothedBiquad::process(float* interleaved, std::size_t frames) noexcept {
    if (m_channels == 0) return;
    std::size_t pos = 0;
    while (pos < frames) {
        if (m_countdown == 0) update();
        const std::size_t n = std::min(m_countdown, frames - pos);
        float* p = interleaved + pos * m_channels;
        for (Biquad& b : m_stages) {
            if (!b.ramping() && b.coefficients().is_identity()) {
                b.reset();
                continue;
            }
            b.process(p, n);
        }
        m_countdown -= n;
        pos += n;
    }
}

std::size_t SmoothedBiquad::tail_frames() const noexcept {
    BiquadCoefficients d[max_stages];
    design(m_type, m_rate, m_target_freq, m_target_q, m_target_gain, m_target_stages, m_active, d);
    std::size_t n = 0;
    for (std::size_t i = 0; i < max_stages; ++i) n += tail_of(d[i], m_rate);
    return n;
}

Filter::Filter(FilterType t, float f, float qv, float gain_db, float slope_db)
    : type("type", static_cast<float>(t), 0.0f, static_cast<float>(k_type_count - 1)),
      frequency("frequency", f, 10.0f, 24000.0f, "Hz"),
      q("q", qv, 0.025f, 40.0f),
      gain("gain", gain_db, -36.0f, 36.0f, "dB"),
      slope("slope", slope_db, 12.0f, 48.0f, "dB/oct") {
    add_parameter(type);
    add_parameter(frequency);
    add_parameter(q);
    add_parameter(gain);
    add_parameter(slope);
}

FilterType Filter::filter_type() const noexcept {
    return type_from(type.get());
}

std::size_t Filter::stages() const noexcept {
    return cascadable(filter_type()) ? stages_from(slope.get()) : 1;
}

std::vector<BiquadCoefficients> Filter::target_coefficients() const {
    BiquadCoefficients d[SmoothedBiquad::max_stages];
    const double rate = sample_rate() ? static_cast<double>(sample_rate()) : 48000.0;
    SmoothedBiquad::design(filter_type(), rate, frequency.get(), q.get(), gain.get(), stages(), true, d);
    return std::vector<BiquadCoefficients>(d, d + stages());
}

void Filter::push_parameters() noexcept {
    m_filter.set(filter_type(), frequency.get(), q.get(), gain.get(), stages(), true);
}

void Filter::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_filter.prepare(static_cast<double>(sample_rate), channels);
    push_parameters();
    m_filter.snap();
}

void Filter::process(float* interleaved, std::size_t frames) noexcept {
    push_parameters();
    m_filter.process(interleaved, frames);
}

void Filter::reset() noexcept {
    push_parameters();
    m_filter.reset();
}

std::size_t Filter::tail_frames() const noexcept {
    return m_filter.tail_frames();
}

ParametricEq::Band::Band(std::size_t index)
    : enabled(band_parameter_name(index, 0), 0.0f, 0.0f, 1.0f),
      type(band_parameter_name(index, 1), static_cast<float>(FilterType::peak), 0.0f, static_cast<float>(k_type_count - 1)),
      frequency(band_parameter_name(index, 2), static_cast<float>(31.25 * std::pow(2.0, static_cast<double>(index) * 9.0 / 15.0)), 10.0f, 24000.0f, "Hz"),
      q(band_parameter_name(index, 3), 1.0f, 0.025f, 40.0f),
      gain(band_parameter_name(index, 4), 0.0f, -36.0f, 36.0f, "dB") {}

FilterType ParametricEq::Band::filter_type() const noexcept {
    return type_from(type.get());
}

ParametricEq::ParametricEq(std::size_t bands) : output("output", 0.0f, -36.0f, 36.0f, "dB") {
    bands = std::min(max_bands, std::max<std::size_t>(1, bands));
    for (std::size_t i = 0; i < bands; ++i) {
        m_bands.push_back(std::unique_ptr<Band>(new Band(i)));
        Band& b = *m_bands.back();
        add_parameter(b.enabled);
        add_parameter(b.type);
        add_parameter(b.frequency);
        add_parameter(b.q);
        add_parameter(b.gain);
    }
    add_parameter(output);
    m_filters.resize(bands);
}

void ParametricEq::set_band(std::size_t index, FilterType t, float f, float qv, float gain_db, bool on) noexcept {
    if (index >= m_bands.size()) return;
    Band& b = *m_bands[index];
    b.type.set(static_cast<float>(t));
    b.frequency.set(f);
    b.q.set(qv);
    b.gain.set(gain_db);
    b.enabled.set(on ? 1.0f : 0.0f);
}

void ParametricEq::push_parameters() noexcept {
    for (std::size_t i = 0; i < m_bands.size(); ++i) {
        const Band& b = *m_bands[i];
        const FilterType t = b.filter_type();
        const float g = b.gain.get();
        const bool active = b.is_enabled() && !(gain_type(t) && g == 0.0f);
        m_filters[i].set(t, b.frequency.get(), b.q.get(), g, 1, active);
    }
}

void ParametricEq::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    for (SmoothedBiquad& f : m_filters) f.prepare(static_cast<double>(sample_rate), channels);
    m_smooth = static_cast<float>(one_pole(0.02, static_cast<double>(sample_rate), 1.0));
    reset();
}

void ParametricEq::process(float* interleaved, std::size_t frames) noexcept {
    push_parameters();
    for (SmoothedBiquad& f : m_filters) f.process(interleaved, frames);
    const float target = db_to_gain(output.get());
    const std::size_t c = channels();
    if (c == 0) return;
    if (target == m_output && target == 1.0f) return;
    for (std::size_t f = 0; f < frames; ++f) {
        m_output += (target - m_output) * m_smooth;
        if (std::fabs(target - m_output) < 1e-7f) m_output = target;
        float* p = interleaved + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] *= m_output;
    }
}

void ParametricEq::reset() noexcept {
    push_parameters();
    for (SmoothedBiquad& f : m_filters) f.reset();
    m_output = db_to_gain(output.get());
}

std::size_t ParametricEq::tail_frames() const noexcept {
    std::size_t n = 0;
    for (const SmoothedBiquad& f : m_filters) n += f.tail_frames();
    return n;
}

std::string ParametricEq::save_state() const {
    return "{\"bands\":" + std::to_string(m_bands.size()) + "}";
}

bool ParametricEq::load_state(const std::string& state) {
    JsonValue v;
    if (!parse_json(state, v, nullptr) || !v.is_object()) return false;
    const JsonValue* b = v.find("bands");
    if (!b || !b->is_number()) return false;
    const double want = b->as_number();
    if (!(want >= 1.0 && want <= static_cast<double>(max_bands))) return false;
    const std::size_t n = static_cast<std::size_t>(want);
    if (n == m_bands.size()) return true;
    std::vector<std::unique_ptr<Band>> bands;
    for (std::size_t i = 0; i < n; ++i) {
        if (i < m_bands.size()) {
            bands.push_back(std::move(m_bands[i]));
        } else {
            bands.push_back(std::unique_ptr<Band>(new Band(i)));
        }
    }
    m_bands = std::move(bands);
    clear_parameters();
    for (const std::unique_ptr<Band>& band : m_bands) {
        add_parameter(band->enabled);
        add_parameter(band->type);
        add_parameter(band->frequency);
        add_parameter(band->q);
        add_parameter(band->gain);
    }
    add_parameter(output);
    m_filters.clear();
    m_filters.resize(n);
    if (sample_rate()) prepare(sample_rate(), channels());
    return true;
}

std::vector<float> frequency_response(const ParametricEq& eq, const std::vector<float>& freqs) {
    const double rate = eq.sample_rate() ? static_cast<double>(eq.sample_rate()) : 48000.0;
    std::vector<double> total(freqs.size(), static_cast<double>(eq.output.get()));
    for (std::size_t i = 0; i < eq.band_count(); ++i) {
        const ParametricEq::Band& b = eq.band(i);
        if (!b.is_enabled()) continue;
        const FilterType t = b.filter_type();
        BiquadCoefficients d[SmoothedBiquad::max_stages];
        SmoothedBiquad::design(t, rate, b.frequency.get(), b.q.get(), b.gain.get(), 1, true, d);
        for (std::size_t k = 0; k < freqs.size(); ++k) total[k] += d[0].magnitude_db(static_cast<double>(freqs[k]), rate);
    }
    std::vector<float> out(freqs.size());
    for (std::size_t k = 0; k < freqs.size(); ++k) out[k] = static_cast<float>(std::max(-300.0, total[k]));
    return out;
}

const float GraphicEq::centers[GraphicEq::band_count] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

GraphicEq::GraphicEq() : output("output", 0.0f, -24.0f, 24.0f, "dB") {
    static const char* const names[band_count] = { "31Hz", "63Hz", "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz", "8kHz", "16kHz" };
    for (std::size_t i = 0; i < band_count; ++i) {
        m_gains.push_back(std::unique_ptr<Parameter>(new Parameter(names[i], 0.0f, -12.0f, 12.0f, "dB")));
        add_parameter(*m_gains.back());
    }
    add_parameter(output);
    m_filters.resize(band_count);
}

void GraphicEq::push_parameters() noexcept {
    for (std::size_t i = 0; i < band_count; ++i) {
        const float g = m_gains[i]->get();
        const bool active = g != 0.0f && centers[i] < static_cast<float>(sample_rate()) * 0.49f;
        m_filters[i].set(FilterType::peak, centers[i], 1.41421356, g, 1, active);
    }
}

void GraphicEq::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    for (SmoothedBiquad& f : m_filters) f.prepare(static_cast<double>(sample_rate), channels);
    m_smooth = static_cast<float>(one_pole(0.02, static_cast<double>(sample_rate), 1.0));
    reset();
}

void GraphicEq::process(float* interleaved, std::size_t frames) noexcept {
    push_parameters();
    for (SmoothedBiquad& f : m_filters) f.process(interleaved, frames);
    const float target = db_to_gain(output.get());
    const std::size_t c = channels();
    if (c == 0) return;
    if (target == m_output && target == 1.0f) return;
    for (std::size_t f = 0; f < frames; ++f) {
        m_output += (target - m_output) * m_smooth;
        if (std::fabs(target - m_output) < 1e-7f) m_output = target;
        float* p = interleaved + f * c;
        for (std::size_t ch = 0; ch < c; ++ch) p[ch] *= m_output;
    }
}

void GraphicEq::reset() noexcept {
    push_parameters();
    for (SmoothedBiquad& f : m_filters) f.reset();
    m_output = db_to_gain(output.get());
}

std::size_t GraphicEq::tail_frames() const noexcept {
    std::size_t n = 0;
    for (const SmoothedBiquad& f : m_filters) n += f.tail_frames();
    return n;
}

std::vector<float> GraphicEq::response_db(const std::vector<float>& freqs) const {
    const double rate = sample_rate() ? static_cast<double>(sample_rate()) : 48000.0;
    std::vector<double> total(freqs.size(), static_cast<double>(output.get()));
    for (std::size_t i = 0; i < band_count; ++i) {
        const float g = m_gains[i]->get();
        if (g == 0.0f || centers[i] >= static_cast<float>(rate) * 0.49f) continue;
        const BiquadCoefficients c = design_biquad(FilterType::peak, rate, centers[i], 1.41421356, g);
        for (std::size_t k = 0; k < freqs.size(); ++k) total[k] += c.magnitude_db(static_cast<double>(freqs[k]), rate);
    }
    return std::vector<float>(total.begin(), total.end());
}

DcBlocker::DcBlocker(float frequency_hz) : frequency("frequency", frequency_hz, 1.0f, 200.0f, "Hz") {
    add_parameter(frequency);
}

void DcBlocker::prepare(std::uint32_t sample_rate, std::size_t channels) {
    set_format(sample_rate, channels);
    m_x1.assign(channels, 0.0);
    m_y1.assign(channels, 0.0);
}

void DcBlocker::process(float* interleaved, std::size_t frames) noexcept {
    const std::size_t c = channels();
    if (c == 0 || m_x1.size() < c) return;
    const double rate = sample_rate() ? static_cast<double>(sample_rate()) : 48000.0;
    const double r = std::exp(-2.0 * k_pi * static_cast<double>(frequency.get()) / rate);
    for (std::size_t ch = 0; ch < c; ++ch) {
        double x1 = m_x1[ch];
        double y1 = m_y1[ch];
        float* p = interleaved + ch;
        for (std::size_t f = 0; f < frames; ++f) {
            const double x = p[f * c];
            const double y = x - x1 + r * y1;
            x1 = x;
            y1 = y;
            p[f * c] = static_cast<float>(y);
        }
        m_x1[ch] = x1;
        m_y1[ch] = std::fabs(y1) < 1e-30 ? 0.0 : y1;
    }
}

void DcBlocker::reset() noexcept {
    std::fill(m_x1.begin(), m_x1.end(), 0.0);
    std::fill(m_y1.begin(), m_y1.end(), 0.0);
}

std::size_t DcBlocker::tail_frames() const noexcept {
    const double rate = sample_rate() ? static_cast<double>(sample_rate()) : 48000.0;
    const double r = std::exp(-2.0 * k_pi * static_cast<double>(frequency.get()) / rate);
    return static_cast<std::size_t>(std::ceil(std::log(1e-6) / std::log(r)));
}

} // namespace audio
} // namespace fizmo
