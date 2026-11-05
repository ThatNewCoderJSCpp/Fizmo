#include "noise.hpp"
#include "fft.hpp"
#include "format.hpp"
#include "presets.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <mutex>
#include <utility>

namespace fizmo {
namespace audio {

namespace {

const double k_pi = 3.14159265358979323846;

std::size_t auto_fft_size(std::uint32_t rate) noexcept {
    const double want = static_cast<double>(rate ? rate : 48000) * 0.0427;
    std::size_t n = 256;
    while (n < 16384 && std::fabs(std::log2(static_cast<double>(n * 2) / want)) < std::fabs(std::log2(static_cast<double>(n) / want))) n *= 2;
    return n;
}

struct ProfileData {
    std::uint32_t rate = 0;
    std::size_t size = 0;
    std::vector<std::vector<float>> psd;
};

using Converted = std::vector<std::vector<float>>;

ProfileData measure_profile(const AudioBuffer& noise, std::size_t size) {
    ProfileData out;
    out.rate = noise.sample_rate();
    out.size = size;
    const std::size_t channels = noise.channels();
    const std::size_t bins = size / 2 + 1;
    const std::size_t hop = size / 4;
    out.psd.assign(channels + 1, std::vector<float>(bins, 0.0f));
    std::vector<double> acc((channels + 1) * bins, 0.0);
    const std::vector<float> window = make_window(WindowType::sqrt_hann, size);
    double energy = 0.0;
    for (float w : window) energy += static_cast<double>(w) * w;
    Fft fft(size);
    std::vector<float> frame(size);
    std::vector<std::complex<float>> spec(bins);
    const std::size_t total = noise.frames();
    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        for (std::size_t c = 0; c < channels; ++c) {
            for (std::size_t i = 0; i < size; ++i) {
                const std::size_t f = start + i;
                frame[i] = f < total ? noise.at(f, c) * window[i] : 0.0f;
            }
            fft.forward_real(frame.data(), spec.data());
            for (std::size_t k = 0; k < bins; ++k) {
                const double p = static_cast<double>(std::norm(spec[k]));
                acc[c * bins + k] += p;
                acc[channels * bins + k] += p / static_cast<double>(channels);
            }
        }
        ++count;
        if (start + size >= total) break;
        start += hop;
    }
    double scale = 1.0 / (static_cast<double>(count) * energy);
    if (total < size && total > 0) scale *= static_cast<double>(size) / static_cast<double>(total);
    for (std::size_t c = 0; c <= channels; ++c) {
        for (std::size_t k = 0; k < bins; ++k) out.psd[c][k] = static_cast<float>(acc[c * bins + k] * scale);
    }
    return out;
}

Converted convert_profile(const ProfileData& p, std::uint32_t rate, std::size_t size, std::size_t channels, float energy) {
    const std::size_t bins = size / 2 + 1;
    const std::size_t src_bins = p.size / 2 + 1;
    const std::size_t src_channels = p.psd.size() - 1;
    Converted out(channels + 1, std::vector<float>(bins, 0.0f));
    const double rate_scale = p.rate ? static_cast<double>(rate) / static_cast<double>(p.rate) : 1.0;
    for (std::size_t c = 0; c <= channels; ++c) {
        const std::vector<float>& src = (c < channels && src_channels == channels) ? p.psd[c] : p.psd[src_channels];
        for (std::size_t k = 0; k < bins; ++k) {
            double v = 0.0;
            if (p.rate == rate && p.size == size) {
                v = src[k];
            } else {
                const double hz = static_cast<double>(k) * static_cast<double>(rate) / static_cast<double>(size);
                const double pos = hz * static_cast<double>(p.size) / static_cast<double>(p.rate ? p.rate : rate);
                if (pos >= static_cast<double>(src_bins - 1)) {
                    v = src[src_bins - 1];
                } else {
                    const std::size_t i = static_cast<std::size_t>(pos);
                    const double t = pos - static_cast<double>(i);
                    v = src[i] * (1.0 - t) + src[i + 1] * t;
                }
                v *= rate_scale;
            }
            out[c][k] = static_cast<float>(v * energy);
        }
    }
    return out;
}

struct Unit {
    std::vector<float> power;
    std::vector<float> smooth;
    std::vector<float> minimum;
    std::vector<float> temp;
    std::vector<float> presence;
    std::vector<float> noise;
    std::vector<float> prior;
    std::vector<float> gain;
    std::vector<float> applied;
    bool fresh = true;
};

float coefficient(double seconds, double step) noexcept {
    if (seconds <= 0.0) return 0.0f;
    return static_cast<float>(std::exp(-step / seconds));
}

} // namespace

struct NoiseReduction::State {
    std::size_t requested = 0;
    std::size_t size = 0;
    std::size_t hop = 0;
    std::size_t bins = 0;
    std::size_t channels = 0;
    std::uint32_t rate = 0;
    Fft fft;
    std::vector<float> window;
    float energy = 1.0f;
    float ola_scale = 0.5f;
    std::vector<std::vector<float>> input;
    std::vector<std::vector<float>> accum;
    std::vector<std::vector<float>> output;
    std::vector<std::vector<std::complex<float>>> spectrum;
    std::vector<float> frame;
    std::vector<float> scratch;
    std::vector<Unit> units;
    std::size_t pos = 0;
    std::size_t since_min = 0;
    std::size_t min_window = 1;
    float alpha_s = 0.8f;
    float alpha_d = 0.95f;
    float alpha_p = 0.2f;
    float alpha_dd = 0.98f;
    float delta = 5.0f;

    mutable std::mutex mutex;
    std::shared_ptr<const ProfileData> raw;
    std::shared_ptr<const Converted> pending;
    std::shared_ptr<const Converted> live;
    std::vector<std::shared_ptr<const Converted>> retired;
    std::atomic<bool> dirty{ false };
};

NoiseReduction::NoiseReduction(std::size_t fft_size)
    : mode("mode", 0.0f, 0.0f, 1.0f),
      reduction_db("reduction_db", 18.0f, 0.0f, 60.0f, "dB"),
      sensitivity("sensitivity", 0.5f, 0.0f, 1.0f),
      frequency_smoothing("frequency_smoothing", 0.1f, 0.0f, 1.0f),
      time_smoothing("time_smoothing", 0.2f, 0.0f, 1.0f),
      per_channel("per_channel", 0.0f, 0.0f, 1.0f),
      m_state(new State()) {
    m_state->requested = fft_size ? next_power_of_two(std::max<std::size_t>(fft_size, 64)) : 0;
    m_state->retired.reserve(16);
    add_parameter(mode);
    add_parameter(reduction_db);
    add_parameter(sensitivity);
    add_parameter(frequency_smoothing);
    add_parameter(time_smoothing);
    add_parameter(per_channel);
}

NoiseReduction::~NoiseReduction() = default;

std::size_t NoiseReduction::fft_size() const noexcept {
    if (m_state->size) return m_state->size;
    return m_state->requested ? m_state->requested : auto_fft_size(48000);
}

std::size_t NoiseReduction::hop_size() const noexcept {
    return fft_size() / 4;
}

std::size_t NoiseReduction::latency_frames() const noexcept {
    return fft_size() - 1;
}

void NoiseReduction::prepare(std::uint32_t sample_rate, std::size_t channels) {
    State& s = *m_state;
    if (sample_rate == 0) sample_rate = 48000;
    if (channels == 0) channels = 1;
    set_format(sample_rate, channels);
    s.rate = sample_rate;
    s.channels = channels;
    s.size = s.requested ? s.requested : auto_fft_size(sample_rate);
    s.hop = s.size / 4;
    s.bins = s.size / 2 + 1;
    s.fft.resize(s.size);
    s.window = make_window(WindowType::sqrt_hann, s.size);
    double energy = 0.0;
    double ola = 0.0;
    for (std::size_t i = 0; i < s.size; ++i) energy += static_cast<double>(s.window[i]) * s.window[i];
    for (std::size_t i = 0; i < s.size; i += s.hop) ola += static_cast<double>(s.window[i]) * s.window[i];
    s.energy = static_cast<float>(energy);
    s.ola_scale = static_cast<float>(1.0 / (ola > 0.0 ? ola : 1.0));
    s.input.assign(channels, std::vector<float>(s.size, 0.0f));
    s.accum.assign(channels, std::vector<float>(s.size, 0.0f));
    s.output.assign(channels, std::vector<float>(s.hop, 0.0f));
    s.spectrum.assign(channels, std::vector<std::complex<float>>(s.bins));
    s.frame.assign(s.size, 0.0f);
    s.scratch.assign(s.bins + 1, 0.0f);
    const std::size_t unit_count = channels > 1 ? channels + 1 : 1;
    s.units.assign(unit_count, Unit());
    for (Unit& u : s.units) {
        u.power.assign(s.bins, 0.0f);
        u.smooth.assign(s.bins, 0.0f);
        u.minimum.assign(s.bins, 0.0f);
        u.temp.assign(s.bins, 0.0f);
        u.presence.assign(s.bins, 0.0f);
        u.noise.assign(s.bins, 0.0f);
        u.prior.assign(s.bins, 0.0f);
        u.gain.assign(s.bins, 1.0f);
        u.applied.assign(s.bins, 1.0f);
    }
    const double step = static_cast<double>(s.hop) / static_cast<double>(sample_rate);
    s.alpha_s = coefficient(0.045, step);
    s.alpha_d = coefficient(0.16, step);
    s.alpha_p = coefficient(0.005, step);
    s.alpha_dd = 0.95f;
    s.min_window = std::max<std::size_t>(8, static_cast<std::size_t>(std::ceil(0.8 / step)));
    std::lock_guard<std::mutex> lock(s.mutex);
    s.retired.clear();
    s.pending.reset();
    s.dirty.store(false);
    if (s.raw) {
        s.live = std::make_shared<const Converted>(convert_profile(*s.raw, s.rate, s.size, s.channels, s.energy));
    } else {
        s.live.reset();
    }
    reset();
}

void NoiseReduction::reset() noexcept {
    State& s = *m_state;
    for (auto& v : s.input) std::fill(v.begin(), v.end(), 0.0f);
    for (auto& v : s.accum) std::fill(v.begin(), v.end(), 0.0f);
    for (auto& v : s.output) std::fill(v.begin(), v.end(), 0.0f);
    for (Unit& u : s.units) {
        u.fresh = true;
        std::fill(u.gain.begin(), u.gain.end(), 1.0f);
        std::fill(u.applied.begin(), u.applied.end(), 1.0f);
        std::fill(u.prior.begin(), u.prior.end(), 0.0f);
    }
    s.pos = 0;
    s.since_min = 0;
}

bool NoiseReduction::learn_profile(const AudioBuffer& noise_only) {
    if (noise_only.empty() || noise_only.channels() == 0 || noise_only.sample_rate() == 0) return false;
    State& s = *m_state;
    const std::size_t size = s.requested ? s.requested : auto_fft_size(noise_only.sample_rate());
    auto raw = std::make_shared<const ProfileData>(measure_profile(noise_only, size));
    std::shared_ptr<const Converted> conv;
    if (s.size) conv = std::make_shared<const Converted>(convert_profile(*raw, s.rate, s.size, s.channels, s.energy));
    std::lock_guard<std::mutex> lock(s.mutex);
    s.retired.clear();
    s.raw = std::move(raw);
    if (s.size) {
        s.pending = std::move(conv);
        s.dirty.store(true, std::memory_order_release);
    }
    return true;
}

void NoiseReduction::clear_profile() {
    State& s = *m_state;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.retired.clear();
    s.raw.reset();
    s.pending.reset();
    if (s.size) s.dirty.store(true, std::memory_order_release);
}

bool NoiseReduction::has_profile() const {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->raw != nullptr;
}

std::vector<float> NoiseReduction::profile_spectrum() const {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    if (!m_state->raw) return std::vector<float>();
    return m_state->raw->psd.back();
}

std::string NoiseReduction::save_state() const {
    const State& s = *m_state;
    std::shared_ptr<const ProfileData> raw;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        raw = s.raw;
    }
    if (!raw && s.requested == 0) return std::string();
    JsonValue v = JsonValue::make_object();
    v.set("fft_size", JsonValue::make_number(static_cast<double>(s.requested)));
    if (raw) {
        JsonValue p = JsonValue::make_object();
        p.set("sample_rate", JsonValue::make_number(static_cast<double>(raw->rate)));
        p.set("size", JsonValue::make_number(static_cast<double>(raw->size)));
        JsonValue psd = JsonValue::make_array();
        for (const std::vector<float>& ch : raw->psd) {
            JsonValue row = JsonValue::make_array();
            for (float x : ch) row.push(JsonValue::make_float(x));
            psd.push(std::move(row));
        }
        p.set("psd", std::move(psd));
        v.set("profile", std::move(p));
    }
    return write_json(v, false);
}

bool NoiseReduction::load_state(const std::string& state) {
    State& s = *m_state;
    JsonValue v;
    if (!parse_json(state, v, nullptr) || !v.is_object()) return false;
    std::size_t requested = s.requested;
    if (const JsonValue* f = v.find("fft_size")) {
        const double n = f->as_number(-1.0);
        if (!(n >= 0.0 && n <= 65536.0)) return false;
        const std::size_t size = static_cast<std::size_t>(n);
        requested = size ? next_power_of_two(std::max<std::size_t>(size, 64)) : 0;
    }
    std::shared_ptr<const ProfileData> raw;
    if (const JsonValue* p = v.find("profile")) {
        if (!p->is_object()) return false;
        const JsonValue* rate = p->find("sample_rate");
        const JsonValue* size = p->find("size");
        const JsonValue* psd = p->find("psd");
        if (!rate || !size || !psd || !psd->is_array() || psd->size() < 1) return false;
        const double r = rate->as_number(0.0);
        const double z = size->as_number(0.0);
        if (!(r >= 1000.0 && r <= 1536000.0) || !(z >= 64.0 && z <= 65536.0)) return false;
        auto data = std::make_shared<ProfileData>();
        data->rate = static_cast<std::uint32_t>(r);
        data->size = static_cast<std::size_t>(z);
        if (next_power_of_two(data->size) != data->size) return false;
        const std::size_t bins = data->size / 2 + 1;
        for (std::size_t c = 0; c < psd->size(); ++c) {
            const JsonValue& row = psd->at(c);
            if (!row.is_array() || row.size() != bins) return false;
            std::vector<float> values(bins);
            for (std::size_t k = 0; k < bins; ++k) {
                const float x = row.at(k).as_float(-1.0f);
                if (!(x >= 0.0f) || std::isinf(x)) return false;
                values[k] = x;
            }
            data->psd.push_back(std::move(values));
        }
        raw = std::move(data);
    }
    s.requested = requested;
    if (!raw) {
        clear_profile();
        return true;
    }
    std::shared_ptr<const Converted> conv;
    if (s.size) conv = std::make_shared<const Converted>(convert_profile(*raw, s.rate, s.size, s.channels, s.energy));
    std::lock_guard<std::mutex> lock(s.mutex);
    s.retired.clear();
    s.raw = std::move(raw);
    if (s.size) {
        s.pending = std::move(conv);
        s.dirty.store(true, std::memory_order_release);
    }
    return true;
}

void NoiseReduction::run_frame() noexcept {
    State& s = *m_state;
    const std::size_t channels = s.channels;
    const std::size_t bins = s.bins;
    const std::size_t size = s.size;
    const std::size_t hop = s.hop;
    for (std::size_t c = 0; c < channels; ++c) {
        const float* in = s.input[c].data();
        for (std::size_t i = 0; i < size; ++i) s.frame[i] = in[i] * s.window[i];
        s.fft.forward_real(s.frame.data(), s.spectrum[c].data());
        float* power = s.units[c].power.data();
        const std::complex<float>* x = s.spectrum[c].data();
        for (std::size_t k = 0; k < bins; ++k) power[k] = std::norm(x[k]);
    }
    if (channels > 1) {
        float* shared = s.units[channels].power.data();
        const float inv = 1.0f / static_cast<float>(channels);
        for (std::size_t k = 0; k < bins; ++k) {
            float sum = 0.0f;
            for (std::size_t c = 0; c < channels; ++c) sum += s.units[c].power[k];
            shared[k] = sum * inv;
        }
    }
    const bool use_profile = mode.get() >= 0.5f && s.live != nullptr;
    const float beta = std::pow(10.0f, (sensitivity.get() - 0.5f) * 1.2f);
    const float floor_gain = db_to_gain(-reduction_db.get());
    const float blend = frequency_smoothing.get();
    const std::size_t radius = 2;
    const double step = static_cast<double>(hop) / static_cast<double>(s.rate);
    const float rel = coefficient(static_cast<double>(time_smoothing.get()) * 0.1, step);
    const float att = coefficient(static_cast<double>(time_smoothing.get()) * 0.02, step);
    const float tiny = 1e-20f * s.energy;
    const float xi_min = 0.0003f;
    const bool roll = s.since_min + 1 >= s.min_window;
    const bool shared_mask = channels > 1 && per_channel.get() < 0.5f;
    for (std::size_t ui = 0; ui < s.units.size(); ++ui) {
        Unit& u = s.units[ui];
        const bool needed = channels == 1 || (shared_mask ? ui == channels : ui < channels);
        const float* p = u.power.data();
        float* sm = s.scratch.data();
        for (std::size_t k = 0; k < bins; ++k) {
            const float l = p[k > 0 ? k - 1 : 1];
            const float r = p[k + 1 < bins ? k + 1 : k - 1];
            sm[k] = 0.25f * l + 0.5f * p[k] + 0.25f * r;
        }
        if (u.fresh) {
            for (std::size_t k = 0; k < bins; ++k) {
                u.smooth[k] = sm[k];
                u.minimum[k] = sm[k];
                u.temp[k] = sm[k];
                u.presence[k] = 0.0f;
                u.noise[k] = p[k];
                u.prior[k] = 0.0f;
            }
            u.fresh = false;
        } else {
            for (std::size_t k = 0; k < bins; ++k) {
                const float sk = s.alpha_s * u.smooth[k] + (1.0f - s.alpha_s) * sm[k];
                u.smooth[k] = sk;
                u.minimum[k] = std::min(u.minimum[k], sk);
                u.temp[k] = std::min(u.temp[k], sk);
                const float indicator = sk > s.delta * u.minimum[k] ? 1.0f : 0.0f;
                u.presence[k] = s.alpha_p * u.presence[k] + (1.0f - s.alpha_p) * indicator;
                const float ad = s.alpha_d + (1.0f - s.alpha_d) * u.presence[k];
                u.noise[k] = ad * u.noise[k] + (1.0f - ad) * p[k];
            }
            if (roll) {
                for (std::size_t k = 0; k < bins; ++k) {
                    u.minimum[k] = std::min(u.temp[k], u.smooth[k]);
                    u.temp[k] = u.smooth[k];
                }
            }
        }
        if (!needed) continue;
        const float* ref = u.noise.data();
        if (use_profile) {
            const Converted& prof = *s.live;
            ref = prof[channels == 1 ? 0 : ui].data();
        }
        for (std::size_t k = 0; k < bins; ++k) {
            const float lam = ref[k] * beta + tiny;
            const float gamma = std::min(p[k] / lam, 1e6f);
            float xi = s.alpha_dd * u.prior[k] + (1.0f - s.alpha_dd) * std::max(gamma - 1.0f, 0.0f);
            xi = std::max(xi, xi_min);
            const float g = xi / (1.0f + xi);
            const float refined = std::max(g * g * gamma, xi_min);
            u.prior[k] = g * g * gamma;
            u.gain[k] = refined / (1.0f + refined);
        }
        const float* g = u.gain.data();
        if (blend > 0.0f) {
            sm[0] = 0.0f;
            for (std::size_t k = 0; k < bins; ++k) sm[k + 1] = sm[k] + g[k];
        }
        for (std::size_t k = 0; k < bins; ++k) {
            float v = g[k];
            if (blend > 0.0f) {
                const std::size_t lo = k >= radius ? k - radius : 0;
                const std::size_t hi = std::min(bins, k + radius + 1);
                const float avg = (sm[hi] - sm[lo]) / static_cast<float>(hi - lo);
                v += (avg - v) * blend;
            }
            v = std::max(v, floor_gain);
            const float prev = u.applied[k];
            const float a = v > prev ? att : rel;
            u.applied[k] = a * prev + (1.0f - a) * v;
        }
    }
    s.since_min = roll ? 0 : s.since_min + 1;
    for (std::size_t c = 0; c < channels; ++c) {
        const Unit& u = s.units[channels == 1 ? 0 : (shared_mask ? channels : c)];
        std::complex<float>* x = s.spectrum[c].data();
        for (std::size_t k = 0; k < bins; ++k) x[k] *= u.applied[k];
        s.fft.inverse_real(x, s.frame.data());
        float* acc = s.accum[c].data();
        const float scale = s.ola_scale;
        for (std::size_t i = 0; i < size; ++i) acc[i] += s.frame[i] * s.window[i] * scale;
        std::memcpy(s.output[c].data(), acc, hop * sizeof(float));
        std::memmove(acc, acc + hop, (size - hop) * sizeof(float));
        std::fill(acc + (size - hop), acc + size, 0.0f);
        float* in = s.input[c].data();
        std::memmove(in, in + hop, (size - hop) * sizeof(float));
    }
}

void NoiseReduction::process(float* interleaved, std::size_t frames) noexcept {
    State& s = *m_state;
    if (s.size == 0 || s.channels == 0) return;
    if (s.dirty.load(std::memory_order_acquire) && s.mutex.try_lock()) {
        if (s.live) s.retired.push_back(std::move(s.live));
        s.live = s.pending;
        s.dirty.store(false);
        s.mutex.unlock();
    }
    const std::size_t channels = s.channels;
    const std::size_t base = s.size - s.hop;
    for (std::size_t f = 0; f < frames; ++f) {
        float* io = interleaved + f * channels;
        for (std::size_t c = 0; c < channels; ++c) s.input[c][base + s.pos] = io[c];
        if (++s.pos == s.hop) {
            run_frame();
            s.pos = 0;
        }
        for (std::size_t c = 0; c < channels; ++c) io[c] = s.output[c][s.pos];
    }
}

AudioBuffer reduce_noise(const AudioBuffer& input, const AudioBuffer* noise_sample, float reduction_db) {
    if (input.empty() || input.channels() == 0 || input.sample_rate() == 0) return input;
    NoiseReduction nr;
    nr.reduction_db.set(reduction_db);
    nr.prepare(input.sample_rate(), input.channels());
    if (noise_sample && !noise_sample->empty() && nr.learn_profile(*noise_sample)) {
        nr.mode.set(1.0f);
    } else {
        nr.mode.set(0.0f);
    }
    return applied(nr, input, false);
}

struct HumRemover::State {
    static const std::size_t max_harmonics = 40;
    std::size_t channels = 0;
    double rate = 48000.0;
    double coeffs[max_harmonics][5] = {};
    std::vector<double> z;
    std::size_t active = 0;
    double current_base = -1.0;
    double current_width = -1.0;
    std::size_t current_count = 0;
    double g_coeff[6] = {};
    std::vector<double> g_s1;
    std::vector<double> g_s2;
    std::size_t g_count = 0;
    std::size_t g_window = 24000;
    float candidate = 0.0f;
    int votes = 0;
    std::atomic<float> detected{ 0.0f };
    std::atomic<float> active_base{ 0.0f };
};

HumRemover::HumRemover(float initial)
    : frequency("frequency", initial, 0.0f, 70.0f, "Hz"),
      harmonics("harmonics", 8.0f, 1.0f, 40.0f),
      width("width", 2.0f, 0.25f, 20.0f, "Hz"),
      m_state(new State()) {
    add_parameter(frequency);
    add_parameter(harmonics);
    add_parameter(width);
}

HumRemover::~HumRemover() = default;

float HumRemover::detected_frequency() const noexcept {
    return m_state->detected.load(std::memory_order_relaxed);
}

float HumRemover::active_frequency() const noexcept {
    return m_state->active_base.load(std::memory_order_relaxed);
}

void HumRemover::prepare(std::uint32_t sample_rate, std::size_t channels) {
    State& s = *m_state;
    if (sample_rate == 0) sample_rate = 48000;
    if (channels == 0) channels = 1;
    set_format(sample_rate, channels);
    s.channels = channels;
    s.rate = static_cast<double>(sample_rate);
    s.z.assign(channels * State::max_harmonics * 2, 0.0);
    s.g_window = std::max<std::size_t>(1024, sample_rate / 2);
    s.g_s1.assign(channels * 6, 0.0);
    s.g_s2.assign(channels * 6, 0.0);
    const double freqs[6] = { 50.0, 100.0, 150.0, 60.0, 120.0, 180.0 };
    for (int i = 0; i < 6; ++i) s.g_coeff[i] = 2.0 * std::cos(2.0 * k_pi * freqs[i] / s.rate);
    s.detected.store(0.0f);
    reset();
}

void HumRemover::reset() noexcept {
    State& s = *m_state;
    std::fill(s.z.begin(), s.z.end(), 0.0);
    std::fill(s.g_s1.begin(), s.g_s1.end(), 0.0);
    std::fill(s.g_s2.begin(), s.g_s2.end(), 0.0);
    s.g_count = 0;
    s.votes = 0;
    s.candidate = 0.0f;
    s.current_base = -1.0;
}

void HumRemover::update_filters(double base, std::size_t count, double bandwidth) noexcept {
    State& s = *m_state;
    s.current_base = base;
    s.current_count = count;
    s.current_width = bandwidth;
    s.active = 0;
    s.active_base.store(static_cast<float>(base), std::memory_order_relaxed);
    if (base <= 0.0) return;
    for (std::size_t h = 1; h <= count && h <= State::max_harmonics; ++h) {
        const double f = base * static_cast<double>(h);
        if (f >= s.rate * 0.45) break;
        const double w0 = 2.0 * k_pi * f / s.rate;
        const double q = f / bandwidth;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        const double cw = std::cos(w0);
        double* c = s.coeffs[s.active];
        c[0] = 1.0 / a0;
        c[1] = -2.0 * cw / a0;
        c[2] = 1.0 / a0;
        c[3] = -2.0 * cw / a0;
        c[4] = (1.0 - alpha) / a0;
        ++s.active;
    }
}

void HumRemover::detect(const float* interleaved, std::size_t frames) noexcept {
    State& s = *m_state;
    const std::size_t channels = s.channels;
    double* s1 = s.g_s1.data();
    double* s2 = s.g_s2.data();
    for (std::size_t f = 0; f < frames; ++f) {
        for (std::size_t c = 0; c < channels; ++c) {
            const double x = interleaved[f * channels + c];
            double* a = s1 + c * 6;
            double* b = s2 + c * 6;
            for (int i = 0; i < 6; ++i) {
                const double v = x + s.g_coeff[i] * a[i] - b[i];
                b[i] = a[i];
                a[i] = v;
            }
        }
        if (++s.g_count < s.g_window) continue;
        double e[2] = { 0.0, 0.0 };
        for (std::size_t j = 0; j < channels * 6; ++j) {
            const std::size_t i = j % 6;
            e[i / 3] += s1[j] * s1[j] + s2[j] * s2[j] - s.g_coeff[i] * s1[j] * s2[j];
            s1[j] = 0.0;
            s2[j] = 0.0;
        }
        const double w = static_cast<double>(s.g_window) * static_cast<double>(channels);
        const double threshold = 1e-10 * w * w;
        float pick = 0.0f;
        if (e[0] > 4.0 * e[1] && e[0] > threshold) pick = 50.0f;
        else if (e[1] > 4.0 * e[0] && e[1] > threshold) pick = 60.0f;
        const float now = s.detected.load(std::memory_order_relaxed);
        if (pick != 0.0f && pick != now) {
            if (now == 0.0f || (pick == s.candidate && ++s.votes >= 2)) {
                s.detected.store(pick, std::memory_order_relaxed);
                s.votes = 0;
            } else if (pick != s.candidate) {
                s.candidate = pick;
                s.votes = 1;
            }
        } else {
            s.votes = 0;
        }
        s.g_count = 0;
    }
}

void HumRemover::process(float* interleaved, std::size_t frames) noexcept {
    State& s = *m_state;
    if (s.channels == 0) return;
    const float manual = frequency.get();
    if (manual <= 0.0f) detect(interleaved, frames);
    const double base = manual > 0.0f ? static_cast<double>(manual) : static_cast<double>(s.detected.load(std::memory_order_relaxed));
    const std::size_t count = static_cast<std::size_t>(std::lround(harmonics.get()));
    const double bw = static_cast<double>(width.get());
    if (base != s.current_base || count != s.current_count || bw != s.current_width) update_filters(base, count, bw);
    if (s.active == 0) return;
    const std::size_t channels = s.channels;
    for (std::size_t c = 0; c < channels; ++c) {
        double* z = s.z.data() + c * State::max_harmonics * 2;
        for (std::size_t f = 0; f < frames; ++f) {
            float* p = interleaved + f * channels + c;
            double x = *p;
            for (std::size_t h = 0; h < s.active; ++h) {
                const double* k = s.coeffs[h];
                double* st = z + h * 2;
                const double y = k[0] * x + st[0];
                st[0] = k[1] * x - k[3] * y + st[1];
                st[1] = k[2] * x - k[4] * y;
                x = y;
            }
            *p = static_cast<float>(x);
        }
    }
}

struct ClickRemover::State {
    std::size_t block = 512;
    std::size_t order = 32;
    std::size_t channels = 0;
    double rate = 48000.0;
    std::vector<std::vector<float>> buffer;
    std::vector<std::vector<float>> output;
    std::vector<double> windowed;
    std::vector<double> window;
    std::vector<double> r;
    std::vector<double> a;
    std::vector<double> tmp;
    std::vector<double> c;
    std::vector<float> residual;
    static const std::size_t reach = 64;
    std::vector<float> magnitude;
    std::vector<float> backward;
    std::vector<float> local;
    std::vector<double> fwd;
    std::vector<double> bwd;
    std::size_t pos = 0;
    std::atomic<std::uint64_t> repaired{ 0 };
};

ClickRemover::ClickRemover()
    : sensitivity("sensitivity", 0.5f, 0.0f, 1.0f),
      max_width("max_width", 2.0f, 0.05f, 5.0f, "ms"),
      m_state(new State()) {
    add_parameter(sensitivity);
    add_parameter(max_width);
}

ClickRemover::~ClickRemover() = default;

std::size_t ClickRemover::latency_frames() const noexcept {
    return 2 * m_state->block - 1;
}

std::uint64_t ClickRemover::clicks_repaired() const noexcept {
    return m_state->repaired.load(std::memory_order_relaxed);
}

void ClickRemover::prepare(std::uint32_t sample_rate, std::size_t channels) {
    State& s = *m_state;
    if (sample_rate == 0) sample_rate = 48000;
    if (channels == 0) channels = 1;
    set_format(sample_rate, channels);
    s.channels = channels;
    s.rate = static_cast<double>(sample_rate);
    s.block = sample_rate > 64000 ? 1024 : 512;
    s.order = 32;
    const std::size_t n = 3 * s.block;
    s.buffer.assign(channels, std::vector<float>(n, 0.0f));
    s.output.assign(channels, std::vector<float>(s.block, 0.0f));
    s.windowed.assign(n, 0.0);
    s.window.resize(n);
    for (std::size_t i = 0; i < n; ++i) s.window[i] = 0.5 - 0.5 * std::cos(2.0 * k_pi * (static_cast<double>(i) + 0.5) / static_cast<double>(n));
    s.r.assign(s.order + 1, 0.0);
    s.a.assign(s.order + 1, 0.0);
    s.tmp.assign(s.order + 1, 0.0);
    s.c.assign(s.order + 1, 0.0);
    s.residual.assign(n, 0.0f);
    s.magnitude.assign(n, 0.0f);
    s.backward.assign(n, 0.0f);
    s.local.assign(2 * State::reach + 2, 0.0f);
    s.fwd.assign(s.block + 2 * s.order + 8, 0.0);
    s.bwd.assign(s.block + 2 * s.order + 8, 0.0);
    reset();
}

void ClickRemover::reset() noexcept {
    State& s = *m_state;
    for (auto& v : s.buffer) std::fill(v.begin(), v.end(), 0.0f);
    for (auto& v : s.output) std::fill(v.begin(), v.end(), 0.0f);
    s.pos = 0;
}

void ClickRemover::run_block(std::size_t channel) noexcept {
    State& s = *m_state;
    float* x = s.buffer[channel].data();
    const std::size_t n = 3 * s.block;
    const std::size_t p = s.order;
    const std::size_t b = s.block;
    for (std::size_t i = 0; i < n; ++i) s.magnitude[i] = std::fabs(x[i]);
    std::nth_element(s.magnitude.begin(), s.magnitude.begin() + static_cast<std::ptrdiff_t>(n / 2), s.magnitude.begin() + static_cast<std::ptrdiff_t>(n));
    const double clip = 6.0 * static_cast<double>(s.magnitude[n / 2]) / 0.6745;
    for (std::size_t i = 0; i < n; ++i) s.windowed[i] = std::max(-clip, std::min(clip, static_cast<double>(x[i]))) * s.window[i];
    for (std::size_t lag = 0; lag <= p; ++lag) {
        double acc = 0.0;
        for (std::size_t i = lag; i < n; ++i) acc += s.windowed[i] * s.windowed[i - lag];
        s.r[lag] = acc;
    }
    std::fill(s.c.begin(), s.c.end(), 0.0);
    if (s.r[0] > 1e-14) {
        s.r[0] *= 1.0001;
        std::fill(s.a.begin(), s.a.end(), 0.0);
        s.a[0] = 1.0;
        double err = s.r[0];
        bool stable = true;
        for (std::size_t i = 1; i <= p; ++i) {
            double acc = s.r[i];
            for (std::size_t j = 1; j < i; ++j) acc += s.a[j] * s.r[i - j];
            const double k = -acc / err;
            for (std::size_t j = 1; j < i; ++j) s.tmp[j] = s.a[j] + k * s.a[i - j];
            for (std::size_t j = 1; j < i; ++j) s.a[j] = s.tmp[j];
            s.a[i] = k;
            err *= 1.0 - k * k;
            if (err <= 0.0) {
                stable = false;
                break;
            }
        }
        if (stable) {
            for (std::size_t j = 1; j <= p; ++j) s.c[j] = -s.a[j];
        }
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        s.residual[i] = 0.0f;
        s.backward[i] = 0.0f;
    }
    for (std::size_t i = p; i < n; ++i) {
        double pred = 0.0;
        for (std::size_t j = 1; j <= p; ++j) pred += s.c[j] * x[i - j];
        const float e = static_cast<float>(x[i] - pred);
        s.residual[i] = e;
        s.magnitude[count++] = std::fabs(e);
    }
    for (std::size_t i = 0; i + p < n; ++i) {
        double pred = 0.0;
        for (std::size_t j = 1; j <= p; ++j) pred += s.c[j] * x[i + j];
        s.backward[i] = static_cast<float>(x[i] - pred);
    }
    const std::size_t mid = count / 2;
    std::nth_element(s.magnitude.begin(), s.magnitude.begin() + static_cast<std::ptrdiff_t>(mid), s.magnitude.begin() + static_cast<std::ptrdiff_t>(count));
    const double sigma = static_cast<double>(s.magnitude[mid]) / 0.6745;
    const double k = 4.0 + 8.0 * (1.0 - static_cast<double>(sensitivity.get()));
    const std::size_t reach = State::reach;
    auto scale = [&](const std::vector<float>& res, std::size_t at, std::size_t lo_bound, std::size_t hi_bound) -> double {
        const std::size_t lo = std::max(lo_bound, at > reach ? at - reach : 0);
        const std::size_t hi = std::min(hi_bound, at + reach + 1);
        std::size_t m = 0;
        for (std::size_t q = lo; q < hi; ++q) s.local[m++] = std::fabs(res[q]);
        if (m == 0) return 0.0;
        std::nth_element(s.local.begin(), s.local.begin() + static_cast<std::ptrdiff_t>(m / 2), s.local.begin() + static_cast<std::ptrdiff_t>(m));
        return static_cast<double>(s.local[m / 2]) / 0.6745;
    };
    auto exceeds = [&](std::size_t at) -> bool {
        const double ef = std::fabs(static_cast<double>(s.residual[at]));
        const double eb = std::fabs(static_cast<double>(s.backward[at]));
        const double floor = std::max(1e-5, 0.5 * k * sigma);
        if (ef < floor || eb < floor) return false;
        if (ef <= k * scale(s.residual, at, p, n)) return false;
        return eb > k * scale(s.backward, at, 0, n - p);
    };
    const std::size_t limit = std::min<std::size_t>(b - p - 4, std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<double>(max_width.get()) * 0.001 * s.rate)));
    const std::size_t gap = 3;
    std::size_t i = b;
    while (i < 2 * b) {
        if (!exceeds(i)) {
            ++i;
            continue;
        }
        std::size_t last = i;
        for (std::size_t m = i + 1; m < n - p - 1 && m <= last + gap && m - i <= limit + gap; ++m) {
            if (exceeds(m)) last = m;
        }
        const std::size_t start = i - 1;
        const std::size_t end = std::min(last + 1, n - p - 1);
        const std::size_t len = end - start + 1;
        if (len > limit + 2) {
            i = last + 1;
            continue;
        }
        double* f = s.fwd.data();
        for (std::size_t k = 0; k < p; ++k) f[k] = x[start - p + k];
        for (std::size_t k = 0; k < len; ++k) {
            double pred = 0.0;
            for (std::size_t j = 1; j <= p; ++j) pred += s.c[j] * f[p + k - j];
            f[p + k] = pred;
        }
        double* w = s.bwd.data();
        for (std::size_t k = 0; k < p; ++k) w[len + k] = x[end + 1 + k];
        for (std::size_t k = len; k-- > 0;) {
            double pred = 0.0;
            for (std::size_t j = 1; j <= p; ++j) pred += s.c[j] * w[k + j];
            w[k] = pred;
        }
        for (std::size_t k = 0; k < len; ++k) {
            const double t = static_cast<double>(k + 1) / static_cast<double>(len + 1);
            x[start + k] = static_cast<float>((1.0 - t) * f[p + k] + t * w[k]);
        }
        s.repaired.fetch_add(1, std::memory_order_relaxed);
        i = end + 1;
    }
}

void ClickRemover::process(float* interleaved, std::size_t frames) noexcept {
    State& s = *m_state;
    if (s.channels == 0) return;
    const std::size_t channels = s.channels;
    const std::size_t b = s.block;
    for (std::size_t f = 0; f < frames; ++f) {
        float* io = interleaved + f * channels;
        for (std::size_t c = 0; c < channels; ++c) s.buffer[c][2 * b + s.pos] = io[c];
        if (++s.pos == b) {
            for (std::size_t c = 0; c < channels; ++c) {
                run_block(c);
                float* buf = s.buffer[c].data();
                std::memcpy(s.output[c].data(), buf + b, b * sizeof(float));
                std::memmove(buf, buf + b, 2 * b * sizeof(float));
            }
            s.pos = 0;
        }
        for (std::size_t c = 0; c < channels; ++c) io[c] = s.output[c][s.pos];
    }
}

} // namespace audio
} // namespace fizmo
