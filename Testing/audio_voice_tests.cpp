#include "../Source/Audio/buffer.hpp"
#include "../Source/Audio/effect.hpp"
#include "../Source/Audio/fft.hpp"
#include "../Source/Audio/modulation.hpp"
#include "../Source/Audio/pitch.hpp"
#include "../Source/Audio/time_effects.hpp"
#include "../Source/Audio/voice.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <vector>

namespace {

std::atomic<long> g_allocations{ 0 };
void* (*volatile g_malloc)(std::size_t) = std::malloc;
void (*volatile g_free)(void*) = std::free;

} // namespace

void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (size == 0) size = 1;
    void* p = g_malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* p) noexcept {
    g_free(p);
}

void operator delete[](void* p) noexcept {
    g_free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    g_free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    g_free(p);
}

using namespace fizmo::audio;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

const double k_pi = 3.14159265358979323846;
const float k_rate = 48000.0f;

std::string num(double v) {
    char b[64];
    std::snprintf(b, sizeof(b), "%.4g", v);
    return b;
}

double blep(double t, double dt) {
    if (t < dt) {
        const double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt) {
        const double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

std::vector<float> sine(double freq, std::size_t n, double amp = 0.5) {
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<float>(amp * std::sin(2.0 * k_pi * freq * static_cast<double>(i) / k_rate));
    return v;
}

std::vector<float> saw_glide(double f0, double f1, std::size_t n, double amp = 0.4) {
    std::vector<float> v(n);
    double phase = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double f = f0 + (f1 - f0) * static_cast<double>(i) / static_cast<double>(n);
        const double dt = f / k_rate;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        v[i] = static_cast<float>(amp * (2.0 * phase - 1.0 - blep(phase, dt)));
    }
    return v;
}

std::vector<float> saw(double f, std::size_t n, double amp = 0.4) {
    return saw_glide(f, f, n, amp);
}

struct Resonator {
    double a1 = 0.0;
    double a2 = 0.0;
    double g = 1.0;
    double y1 = 0.0;
    double y2 = 0.0;
    Resonator(double freq, double bw) {
        const double r = std::exp(-k_pi * bw / k_rate);
        a1 = 2.0 * r * std::cos(2.0 * k_pi * freq / k_rate);
        a2 = -r * r;
        g = (1.0 - r) * 2.0;
    }
    double run(double x) {
        const double y = g * x + a1 * y1 + a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};

void normalize(std::vector<float>& v, float peak) {
    float m = 0.0f;
    for (float x : v) m = std::max(m, std::fabs(x));
    if (m > 0.0f) {
        for (float& x : v) x *= peak / m;
    }
}

std::vector<float> vowel(double f0, std::size_t n, const std::vector<double>& formants, const std::vector<double>& bandwidths, double vib_hz = 0.0, double vib_cents = 0.0, double tilt = 0.3) {
    std::vector<float> v(n);
    std::vector<Resonator> res;
    for (std::size_t i = 0; i < formants.size(); ++i) res.emplace_back(formants[i], bandwidths[i]);
    double phase = 0.0;
    double prev = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double f = f0 * std::pow(2.0, vib_cents / 1200.0 * std::sin(2.0 * k_pi * vib_hz * static_cast<double>(i) / k_rate));
        const double dt = f / k_rate;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        const double s = 2.0 * phase - 1.0 - blep(phase, dt);
        const double d = s - prev;
        prev = s;
        double y = tilt * s + d;
        double out = 0.0;
        for (Resonator& r : res) out += r.run(y);
        v[i] = static_cast<float>(out);
    }
    normalize(v, 0.5f);
    return v;
}

std::vector<float> vowel_a(double f0, std::size_t n, double vib_hz = 0.0, double vib_cents = 0.0) {
    return vowel(f0, n, { 730.0, 1090.0, 2440.0 }, { 90.0, 110.0, 170.0 }, vib_hz, vib_cents);
}

std::vector<float> interleave(const std::vector<float>& mono, std::size_t channels, const std::vector<float>& gains = {}) {
    std::vector<float> out(mono.size() * channels);
    for (std::size_t i = 0; i < mono.size(); ++i) {
        for (std::size_t c = 0; c < channels; ++c) out[i * channels + c] = mono[i] * (c < gains.size() ? gains[c] : 1.0f);
    }
    return out;
}

void run(Effect& e, std::vector<float>& data, std::size_t channels, std::size_t block = 256) {
    const std::size_t frames = data.size() / channels;
    for (std::size_t pos = 0; pos < frames; pos += block) e.process(data.data() + pos * channels, std::min(block, frames - pos));
}

float pitch_of(const std::vector<float>& v, std::size_t start, std::size_t len) {
    start = std::min(start, v.size());
    len = std::min(len, v.size() - start);
    return detect_pitch(v.data() + start, len, k_rate, 50.0f, 1500.0f, nullptr);
}

double rel_err(double got, double want) {
    return std::fabs(got - want) / want;
}

double goertzel(const float* data, std::size_t n, double freq) {
    const double w = 2.0 * k_pi * freq / k_rate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * k_pi * static_cast<double>(i) / static_cast<double>(n));
        const double s = data[i] * hann + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2));
}

double envelope_peak(const std::vector<float>& v, std::size_t start, std::size_t len, double f0, double max_hz) {
    std::vector<double> amps;
    for (int k = 1; k * f0 < max_hz; ++k) amps.push_back(goertzel(v.data() + start, len, k * f0));
    std::size_t best = 0;
    for (std::size_t i = 1; i < amps.size(); ++i) {
        if (amps[i] > amps[best]) best = i;
    }
    double delta = 0.0;
    if (best > 0 && best + 1 < amps.size()) {
        const double y0 = std::log(amps[best - 1] + 1e-12);
        const double y1 = std::log(amps[best] + 1e-12);
        const double y2 = std::log(amps[best + 1] + 1e-12);
        const double d = y0 - 2.0 * y1 + y2;
        if (d < 0.0) delta = std::max(-0.5, std::min(0.5, 0.5 * (y0 - y2) / d));
    }
    return (static_cast<double>(best) + 1.0 + delta) * f0;
}

long measured_delay(const std::vector<float>& in, const std::vector<float>& out, long max_lag) {
    long best = 0;
    double best_v = -1e300;
    for (long lag = 0; lag <= max_lag; ++lag) {
        double acc = 0.0;
        for (std::size_t i = 0; i + static_cast<std::size_t>(lag) < out.size() && i < in.size(); ++i) acc += static_cast<double>(in[i]) * out[i + static_cast<std::size_t>(lag)];
        if (acc > best_v) {
            best_v = acc;
            best = lag;
        }
    }
    return best;
}

std::vector<float> noise(std::size_t n, float amp, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-amp, amp);
    std::vector<float> v(n);
    for (float& x : v) x = u(rng);
    return v;
}

bool all_finite(const std::vector<float>& v, float bound, bool* subnormal = nullptr) {
    bool ok = true;
    for (float x : v) {
        if (!std::isfinite(x) || std::fabs(x) > bound) ok = false;
        if (subnormal && x != 0.0f && std::fabs(x) < 1.17549435e-38f) *subnormal = true;
    }
    return ok;
}

float peak(const std::vector<float>& v, std::size_t start = 0, std::size_t end = static_cast<std::size_t>(-1)) {
    float m = 0.0f;
    end = std::min(end, v.size());
    for (std::size_t i = start; i < end; ++i) m = std::max(m, std::fabs(v[i]));
    return m;
}

void test_detector() {
    const double freqs[] = { 80.0, 100.0, 150.0, 220.0, 330.0, 440.0, 660.0, 1000.0 };
    double worst_sine = 0.0;
    double worst_saw = 0.0;
    double worst_vowel = 0.0;
    double worst_noisy = 0.0;
    std::mt19937 rng(7);
    std::normal_distribution<float> g(0.0f, 1.0f);
    for (double f : freqs) {
        worst_sine = std::max(worst_sine, rel_err(detect_pitch(sine(f, 24000).data(), 24000, k_rate), f));
        worst_saw = std::max(worst_saw, rel_err(detect_pitch(saw(f, 24000).data(), 24000, k_rate), f));
        std::vector<float> v = vowel_a(f, 24000);
        worst_vowel = std::max(worst_vowel, rel_err(detect_pitch(v.data(), v.size(), k_rate), f));
        std::vector<float> o = vowel(f, 24000, { 400.0, 2000.0, 2600.0 }, { 60.0, 100.0, 140.0 });
        worst_vowel = std::max(worst_vowel, rel_err(detect_pitch(o.data(), o.size(), k_rate), f));
        float rms = 0.0f;
        for (float x : v) rms += x * x;
        rms = std::sqrt(rms / static_cast<float>(v.size()));
        for (float& x : v) x += 0.316f * rms * g(rng);
        worst_noisy = std::max(worst_noisy, rel_err(detect_pitch(v.data(), v.size(), k_rate), f));
    }
    check(worst_sine < 0.01, "YIN on sines 80-1000 Hz within 1% (worst " + num(worst_sine * 100.0) + "%)");
    check(worst_saw < 0.01, "YIN on sawtooth tones 80-1000 Hz within 1% (worst " + num(worst_saw * 100.0) + "%)");
    check(worst_vowel < 0.01, "YIN on synthetic vowels 80-1000 Hz within 1% (worst " + num(worst_vowel * 100.0) + "%)");
    check(worst_noisy < 0.01, "YIN on vowels with 10 dB SNR noise within 1% (worst " + num(worst_noisy * 100.0) + "%)");
    std::vector<float> n = noise(48000, 0.5f, 3);
    float conf = 1.0f;
    const float f = detect_pitch(n.data(), n.size(), k_rate, 50.0f, 1500.0f, &conf);
    check(f == 0.0f && conf < 0.5f, "white noise is unvoiced with low confidence (" + num(conf) + ")");
    PitchDetector d(k_rate, 60.0f, 1000.0f);
    const PitchEstimate silent = d.push(std::vector<float>(4800, 0.0f).data(), 4800);
    check(!silent.voiced && silent.confidence == 0.0f, "silence is unvoiced");
    std::vector<float> v = vowel_a(196.0, 9600);
    const PitchEstimate e = d.push(v.data(), v.size());
    check(e.voiced && rel_err(e.frequency, 196.0) < 0.01 && e.confidence > 0.8f, "streaming detector tracks a vowel (" + num(e.frequency) + " Hz)");
    std::vector<float> nz = noise(9600, 0.5f, 11);
    const PitchEstimate en = d.push(nz.data(), nz.size());
    check(!en.voiced, "streaming detector reports noise as unvoiced");
    d.set_range(300.0f, 1000.0f);
    std::vector<float> high = sine(500.0, 9600);
    const PitchEstimate eh = d.push(high.data(), high.size());
    check(eh.voiced && rel_err(eh.frequency, 500.0) < 0.01, "detector range can be narrowed at runtime");
}

void test_pitch_shifter() {
    const double shifts[] = { -12.0, -7.0, -3.0, 4.0, 7.0, 12.0 };
    for (int mode = 0; mode < 2; ++mode) {
        double worst = 0.0;
        double worst_vowel = 0.0;
        for (double st : shifts) {
            PitchShifter ps(static_cast<float>(st), static_cast<float>(mode));
            ps.prepare(48000, 1);
            std::vector<float> s = saw(220.0, 72000);
            run(ps, s, 1);
            const double want = 220.0 * std::pow(2.0, st / 12.0);
            worst = std::max(worst, rel_err(pitch_of(s, 24000, 24000), want));
            worst = std::max(worst, rel_err(pitch_of(s, 48000, 24000), want));
            ps.reset();
            std::vector<float> v = vowel_a(160.0, 72000);
            run(ps, v, 1);
            const double want_v = 160.0 * std::pow(2.0, st / 12.0);
            worst_vowel = std::max(worst_vowel, rel_err(pitch_of(v, 24000, 48000), want_v));
        }
        check(worst < 0.01, std::string("PitchShifter mode ") + std::to_string(mode) + " shifts sawtooth pitch exactly (worst " + num(worst * 100.0) + "%)");
        check(worst_vowel < 0.01, std::string("PitchShifter mode ") + std::to_string(mode) + " shifts vowel pitch exactly (worst " + num(worst_vowel * 100.0) + "%)");
    }
    for (int mode = 0; mode < 2; ++mode) {
        PitchShifter ps(0.0f, static_cast<float>(mode));
        ps.prepare(48000, 1);
        std::vector<float> in = noise(12000, 0.5f, 21);
        in.resize(24000, 0.0f);
        std::vector<float> out = in;
        run(ps, out, 1);
        const long lag = measured_delay(in, out, 4000);
        check(lag == static_cast<long>(ps.latency_frames()), std::string("mode ") + std::to_string(mode) + " reported latency " + std::to_string(ps.latency_frames()) + " equals measured delay " + std::to_string(lag));
        double err = 0.0;
        double ref = 0.0;
        for (std::size_t i = 0; i < 10000; ++i) {
            const double d = out[i + static_cast<std::size_t>(lag)] - in[i];
            err += d * d;
            ref += static_cast<double>(in[i]) * in[i];
        }
        check(err < ref * 0.01, std::string("mode ") + std::to_string(mode) + " at 0 semitones reconstructs the input (" + num(10.0 * std::log10(ref / std::max(1e-30, err))) + " dB)");
        ps.semitones.set(7.0f);
        ps.reset();
        AudioBuffer buf(1, 48000, saw(220.0, 30000));
        AudioBuffer res = applied(ps, buf, false);
        check(res.frames() == buf.frames(), std::string("mode ") + std::to_string(mode) + " preserves duration");
    }
    {
        PitchShifter ps(0.0f, 0.0f);
        ps.prepare(48000, 2);
        check(ps.latency_frames() <= 1440, "voice mode latency <= 30 ms at 48 kHz (" + num(ps.latency_frames() / 48.0) + " ms)");
        ps.semitones.set(5.0f);
        std::vector<float> in = interleave(vowel_a(150.0, 48000), 2, { 1.0f, -0.5f });
        run(ps, in, 2);
        double err = 0.0;
        double ref = 0.0;
        for (std::size_t i = 12000; i < 48000; ++i) {
            const double l = in[i * 2];
            const double r = in[i * 2 + 1];
            err += (r + 0.5 * l) * (r + 0.5 * l);
            ref += r * r;
        }
        check(err < ref * 1e-6, "stereo channels stay phase-coherent through voice mode");
        PitchShifter pv(5.0f, 1.0f);
        pv.prepare(48000, 2);
        std::vector<float> in2 = interleave(vowel_a(150.0, 48000), 2, { 1.0f, 0.5f });
        run(pv, in2, 2);
        err = 0.0;
        ref = 0.0;
        for (std::size_t i = 12000; i < 48000; ++i) {
            const double l = in2[i * 2];
            const double r = in2[i * 2 + 1];
            err += (r - 0.5 * l) * (r - 0.5 * l);
            ref += r * r;
        }
        check(err < ref * 1e-4, "stereo channels stay phase-coherent through polyphonic mode");
    }
    for (int mode = 0; mode < 2; ++mode) {
        const double f0 = 100.0;
        std::vector<float> src = vowel(f0, 96000, { 800.0 }, { 300.0 }, 0.0, 0.0, 0.0);
        const double before = envelope_peak(src, 48000, 32768, f0, 4000.0);
        PitchShifter keep(4.0f, static_cast<float>(mode));
        keep.preserve_formants.set(1.0f);
        keep.prepare(48000, 1);
        std::vector<float> a = src;
        run(keep, a, 1);
        const double fa = f0 * std::pow(2.0, 4.0 / 12.0);
        const double pa = pitch_of(a, 48000, 32768);
        const double ea = envelope_peak(a, 48000, 32768, pa, 4000.0);
        PitchShifter move(4.0f, static_cast<float>(mode));
        move.prepare(48000, 1);
        std::vector<float> b = src;
        run(move, b, 1);
        const double pb = pitch_of(b, 48000, 32768);
        const double eb = envelope_peak(b, 48000, 32768, pb, 4000.0);
        check(rel_err(pa, fa) < 0.01 && rel_err(ea, before) < 0.1, std::string("mode ") + std::to_string(mode) + " preserve_formants keeps the envelope peak (" + num(before) + " -> " + num(ea) + " Hz)");
        check(rel_err(eb, before * std::pow(2.0, 4.0 / 12.0)) < 0.1, std::string("mode ") + std::to_string(mode) + " without formant preservation moves the envelope with pitch (" + num(before) + " -> " + num(eb) + " Hz)");
    }
}

void test_formant_shifter() {
    const double f0 = 100.0;
    std::vector<float> src = vowel(f0, 96000, { 800.0 }, { 300.0 }, 0.0, 0.0, 0.0);
    const double before = envelope_peak(src, 48000, 32768, f0, 4000.0);
    for (double st : { 4.0, -4.0 }) {
        FormantShifter fs(static_cast<float>(st));
        fs.prepare(48000, 1);
        std::vector<float> v = src;
        run(fs, v, 1);
        const double p = pitch_of(v, 48000, 32768);
        const double e = envelope_peak(v, 48000, 32768, p, 4000.0);
        const double want = before * std::pow(2.0, st / 12.0);
        check(rel_err(p, f0) < 0.01, "FormantShifter " + num(st) + " st keeps pitch (" + num(p) + " Hz)");
        check(rel_err(e, want) < 0.1, "FormantShifter " + num(st) + " st moves the envelope peak " + num(before) + " -> " + num(e) + " Hz (want " + num(want) + ")");
    }
    FormantShifter id(0.0f);
    id.prepare(48000, 1);
    std::vector<float> in = noise(12000, 0.5f, 5);
    in.resize(20000, 0.0f);
    std::vector<float> out = in;
    run(id, out, 1);
    check(measured_delay(in, out, 3000) == static_cast<long>(id.latency_frames()), "FormantShifter latency matches its report");
}

void test_autotune() {
    {
        AutoTune at;
        at.scale.set(static_cast<float>(MusicalScale::major));
        at.prepare(48000, 1);
        std::vector<float> v = vowel_a(450.0, 48000);
        run(at, v, 1);
        const double p = pitch_of(v, 12000, 36000);
        check(rel_err(p, 440.0) < 0.005, "AutoTune snaps a +39 cent tone to A4 (" + num(p) + " Hz)");
        check(std::fabs(at.target_note() - 69.0f) < 0.01f && at.voiced(), "AutoTune exposes the target note");
        check(std::fabs(at.correction_cents() + 39.0f) < 5.0f, "AutoTune exposes the correction in cents (" + num(at.correction_cents()) + ")");
    }
    {
        AutoTune at;
        at.prepare(48000, 1);
        std::vector<float> v = saw_glide(430.0, 452.0, 72000);
        run(at, v, 1);
        double worst = 0.0;
        for (std::size_t start = 9600; start + 4800 <= 72000; start += 4800) worst = std::max(worst, rel_err(pitch_of(v, start, 4800), 440.0));
        check(worst < 0.01, "AutoTune holds a gliding tone on A4 (worst " + num(worst * 100.0) + "%)");
    }
    {
        AutoTune at;
        at.scale.set(static_cast<float>(MusicalScale::major));
        at.prepare(48000, 1);
        std::vector<float> v = vowel_a(405.0, 48000);
        run(at, v, 1);
        const double g4 = 440.0 * std::pow(2.0, -2.0 / 12.0);
        const double p = pitch_of(v, 12000, 36000);
        check(rel_err(p, g4) < 0.01, "AutoTune in C major moves 405 Hz to G4 instead of G#4 (" + num(p) + " Hz)");
    }
    {
        AutoTune at;
        at.scale.set(7.0f);
        at.mask.set(static_cast<float>(1u << 4));
        at.key.set(0.0f);
        at.prepare(48000, 1);
        std::vector<float> v = vowel_a(300.0, 48000);
        run(at, v, 1);
        const double e4 = 440.0 * std::pow(2.0, -5.0 / 12.0);
        const double p = pitch_of(v, 12000, 36000);
        check(rel_err(p, e4) < 0.01, "AutoTune custom mask with only E snaps 300 Hz to E4 (" + num(p) + " Hz)");
    }
    {
        AutoTune hard;
        AutoTune slow;
        slow.retune_ms.set(400.0f);
        hard.prepare(48000, 1);
        slow.prepare(48000, 1);
        std::vector<float> a = vowel_a(440.0, 72000, 5.5, 35.0);
        std::vector<float> b = a;
        run(hard, a, 1);
        run(slow, b, 1);
        auto spread = [](const std::vector<float>& v) {
            double lo = 1e9;
            double hi = 0.0;
            for (std::size_t s = 24000; s + 2400 <= v.size(); s += 1200) {
                const double p = pitch_of(v, s, 2400);
                if (p <= 0.0) continue;
                lo = std::min(lo, p);
                hi = std::max(hi, p);
            }
            return 1200.0 * std::log2(hi / lo);
        };
        const double sa = spread(a);
        const double sb = spread(b);
        check(sa < 40.0 && sb > sa * 2.0, "hard retune flattens vibrato (" + num(sa) + " ct) and slow retune keeps it (" + num(sb) + " ct)");
    }
}

void test_voice_changer() {
    const std::size_t n = voice_preset_count();
    check(n == 14, "fourteen voice presets");
    for (std::size_t p = 0; p < n; ++p) {
        VoiceChanger vc(static_cast<VoicePreset>(p));
        vc.prepare(48000, 2);
        check(vc.latency_frames() <= 1440, std::string(voice_preset_name(static_cast<VoicePreset>(p))) + " latency <= 30 ms");
        std::vector<float> loud = noise(96000 * 2, 1.0f, static_cast<unsigned>(p + 1));
        run(vc, loud, 2);
        std::vector<float> quiet(48000 * 2 * 8, 0.0f);
        run(vc, quiet, 2);
        std::vector<float> after(48000 * 2, 0.0f);
        run(vc, after, 2);
        bool sub = false;
        const bool ok = all_finite(loud, 8.0f, &sub) && all_finite(quiet, 8.0f, &sub) && all_finite(after, 8.0f, &sub);
        check(ok && !sub, std::string(voice_preset_name(static_cast<VoicePreset>(p))) + " stays finite and bounded on full-scale noise and silence (peak " + num(peak(loud)) + ")");
        check(peak(after) < 1e-3f, std::string(voice_preset_name(static_cast<VoicePreset>(p))) + " decays to silence (" + num(peak(after)) + ")");
        std::vector<float> v = interleave(vowel_a(140.0, 48000), 2);
        run(vc, v, 2);
        check(all_finite(v, 8.0f) && peak(v) > 0.01f, std::string(voice_preset_name(static_cast<VoicePreset>(p))) + " passes a voice");
    }
    {
        VoiceChanger vc(VoicePreset::none);
        vc.prepare(48000, 1);
        std::vector<float> in = vowel_a(150.0, 24000);
        std::vector<float> out = in;
        run(vc, out, 1);
        double err = 0.0;
        for (std::size_t i = 0; i + vc.latency_frames() < out.size(); ++i) err = std::max(err, static_cast<double>(std::fabs(out[i + vc.latency_frames()] - in[i])));
        check(err < 1e-6, "preset none is a pure delay of latency_frames()");
    }
    {
        VoiceChanger vc(VoicePreset::none);
        vc.prepare(48000, 1);
        std::vector<float> v = vowel_a(150.0, 48000 * 6);
        const std::size_t block = 256;
        float worst_jump = 0.0f;
        float steady_jump = 0.0f;
        std::size_t switches = 0;
        for (std::size_t pos = 0; pos < v.size(); pos += block) {
            if (pos % 19200 == 0 && pos > 0) {
                vc.preset.set(static_cast<float>((switches * 5 + 1) % voice_preset_count()));
                ++switches;
            }
            const bool fading_before = vc.crossfading();
            vc.process(v.data() + pos, std::min(block, v.size() - pos));
            for (std::size_t i = std::max<std::size_t>(pos, 1); i < std::min(pos + block, v.size()); ++i) {
                const float j = std::fabs(v[i] - v[i - 1]);
                if (vc.crossfading() || fading_before) worst_jump = std::max(worst_jump, j);
                else steady_jump = std::max(steady_jump, j);
            }
        }
        check(all_finite(v, 8.0f) && worst_jump <= steady_jump * 1.5f + 0.02f, "switching presets crossfades without clicks (jump " + num(worst_jump) + " vs steady " + num(steady_jump) + ")");
    }
    for (std::size_t p = 0; p < n; ++p) {
        std::shared_ptr<EffectChain> chain = make_voice_chain(static_cast<VoicePreset>(p));
        AudioBuffer b(2, 48000, interleave(vowel_a(140.0, 24000), 2));
        apply(*chain, b);
        check(all_finite(b.samples(), 8.0f), std::string("custom chain for ") + voice_preset_name(static_cast<VoicePreset>(p)) + " runs");
    }
}

void test_ring_and_vocoder() {
    RingModulator rm;
    rm.frequency.set(100.0f);
    rm.prepare(48000, 1);
    std::vector<float> v = sine(1000.0, 48000);
    run(rm, v, 1);
    const double side = goertzel(v.data(), 48000, 1100.0) + goertzel(v.data(), 48000, 900.0);
    const double centre = goertzel(v.data(), 48000, 1000.0);
    check(side > 100.0 * centre, "ring modulator produces sum and difference tones");
    Vocoder voc;
    voc.prepare(48000, 1);
    std::vector<float> vv = vowel_a(200.0, 48000);
    run(voc, vv, 1);
    const float p = pitch_of(vv, 12000, 36000);
    check(rel_err(p, 110.0) < 0.01 && all_finite(vv, 8.0f), "vocoder output follows the carrier pitch (" + num(p) + " Hz)");
    Vocoder side_voc;
    side_voc.carrier.set(3.0f);
    side_voc.prepare(48000, 1);
    std::vector<float> car = saw(330.0, 24000, 0.5);
    std::vector<float> mod = vowel_a(150.0, 24000);
    std::size_t done = 0;
    for (std::size_t pos = 0; pos < mod.size(); pos += 480) {
        done += side_voc.push_sidechain(car.data() + pos, std::min<std::size_t>(480, car.size() - pos));
        side_voc.process(mod.data() + pos, std::min<std::size_t>(480, mod.size() - pos));
    }
    const float ps = pitch_of(mod, 6000, 18000);
    check(done == car.size() && rel_err(ps, 330.0) < 0.01, "vocoder sidechain carrier through the lock-free ring (" + num(ps) + " Hz)");
}

void test_modulation() {
    std::vector<std::unique_ptr<Effect>> effects;
    effects.emplace_back(new Chorus());
    effects.emplace_back(new Flanger());
    effects.emplace_back(new Phaser());
    effects.emplace_back(new Tremolo());
    effects.emplace_back(new Vibrato());
    effects.emplace_back(new AutoPan());
    for (auto& e : effects) {
        for (int shape = 0; shape < 6; ++shape) {
            e->set("shape", static_cast<float>(shape));
            e->set("feedback", 0.95f);
            e->prepare(48000, 2);
            std::vector<float> v = noise(48000 * 2, 1.0f, 9);
            run(*e, v, 2);
            std::vector<float> z(48000 * 2 * 4, 0.0f);
            run(*e, z, 2);
            check(all_finite(v, 20.0f) && all_finite(z, 20.0f) && peak(z, 48000 * 2 * 3) < 1e-3f, std::string(e->name()) + " shape " + std::to_string(shape) + " is stable");
        }
    }
    Tremolo t;
    t.depth.set(1.0f);
    t.rate.set(4.0f);
    t.prepare(48000, 1);
    std::vector<float> ones(48000, 1.0f);
    run(t, ones, 1);
    float lo = 1.0f;
    float hi = 0.0f;
    for (std::size_t i = 4800; i < ones.size(); ++i) {
        lo = std::min(lo, ones[i]);
        hi = std::max(hi, ones[i]);
    }
    check(lo < 0.02f && hi > 0.98f, "tremolo at full depth swings from silence to unity");
    Vibrato vib;
    vib.rate.set(5.0f);
    vib.depth_ms.set(2.0f);
    vib.prepare(48000, 1);
    std::vector<float> s = sine(440.0, 48000);
    run(vib, s, 1);
    double lo_f = 1e9;
    double hi_f = 0.0;
    for (std::size_t st = 4800; st + 1200 <= s.size(); st += 600) {
        const double p = pitch_of(s, st, 1200);
        if (p <= 0.0) continue;
        lo_f = std::min(lo_f, p);
        hi_f = std::max(hi_f, p);
    }
    check(hi_f / lo_f > 1.02, "vibrato modulates pitch (" + num(lo_f) + " .. " + num(hi_f) + " Hz)");
    AutoPan pan;
    pan.rate.set(2.0f);
    pan.prepare(48000, 2);
    std::vector<float> st2(48000 * 2, 0.5f);
    run(pan, st2, 2);
    bool left = false;
    bool right = false;
    for (std::size_t i = 0; i < 48000; ++i) {
        if (st2[i * 2] > 0.65f && st2[i * 2 + 1] < 0.05f) left = true;
        if (st2[i * 2 + 1] > 0.65f && st2[i * 2] < 0.05f) right = true;
    }
    check(left && right, "autopan sweeps between the channels");
    Phaser ph;
    ph.feedback.set(0.0f);
    ph.depth.set(0.0f);
    ph.mix.set(0.5f);
    ph.min_hz.set(1000.0f);
    ph.max_hz.set(1000.0f);
    ph.stages.set(2.0f);
    ph.prepare(48000, 1);
    std::vector<float> at_notch = sine(1000.0, 48000);
    run(ph, at_notch, 1);
    std::vector<float> off_notch = sine(100.0, 48000);
    ph.reset();
    run(ph, off_notch, 1);
    check(peak(at_notch, 24000) < 0.05f && peak(off_notch, 24000) > 0.4f, "phaser notches at its allpass frequency");
}

void test_time_effects() {
    {
        Delay d;
        d.time_ms.set(100.0f);
        d.feedback.set(0.5f);
        d.mix.set(1.0f);
        d.low_cut.set(20.0f);
        d.high_cut.set(20000.0f);
        AudioBuffer imp(1, 48000, 4800);
        for (std::size_t i = 0; i < 65; ++i) imp.at(i, 0) = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * k_pi * static_cast<double>(i) / 64.0));
        AudioBuffer out = applied(d, imp, true);
        check(out.frames() >= 4800 + d.tail_frames(), "delay keeps its tail");
        check(std::fabs(out.at(4832, 0) - 1.0f) < 0.02f && std::fabs(out.at(9632, 0) - 0.5f) < 0.05f && std::fabs(out.at(4700, 0)) < 1e-3f, "delay echoes at the set time with feedback (" + num(out.at(4832, 0)) + ", " + num(out.at(9632, 0)) + ")");
        std::size_t last = 0;
        for (std::size_t i = 0; i < out.frames(); ++i) {
            if (std::fabs(out.at(i, 0)) > 1e-3f) last = i;
        }
        check(last < out.frames() && d.tail_frames() >= last - 4800, "delay tail_frames covers the decay to -60 dB");
        check(d.tail_frames() < 48000 * 2, "delay tail_frames is not excessive (" + std::to_string(d.tail_frames()) + ")");
    }
    {
        Delay d;
        d.sync.set(1.0f);
        d.bpm.set(120.0f);
        d.note.set(4.0f);
        d.mix.set(1.0f);
        d.feedback.set(0.0f);
        d.low_cut.set(20.0f);
        d.high_cut.set(20000.0f);
        d.pingpong.set(1.0f);
        d.prepare(48000, 2);
        std::vector<float> v(48000 * 2 * 2, 0.0f);
        v[0] = 1.0f;
        v[1] = 1.0f;
        run(d, v, 2);
        check(std::fabs(v[24000 * 2] - 1.0f) < 0.1f && std::string(delay_note_name(4)) == "1/4", "tempo-synced quarter note at 120 bpm is 500 ms");
        d.feedback.set(0.8f);
        d.reset();
        std::fill(v.begin(), v.end(), 0.0f);
        v[0] = 1.0f;
        v[1] = 1.0f;
        run(d, v, 2);
        check(std::fabs(v[24000 * 2]) > 0.8f && std::fabs(v[24000 * 2 + 1]) < 1e-3f && std::fabs(v[48000 * 2 + 1]) > 0.5f && std::fabs(v[48000 * 2]) < 1e-3f, "ping-pong alternates channels");
    }
    {
        Reverb r;
        r.decay.set(1.5f);
        r.damping.set(0.0f);
        r.predelay_ms.set(0.0f);
        r.mix.set(1.0f);
        AudioBuffer imp(2, 48000, 4800);
        imp.at(0, 0) = 1.0f;
        imp.at(0, 1) = 1.0f;
        AudioBuffer out = applied(r, imp, true);
        std::vector<double> edc(out.frames() + 1, 0.0);
        for (std::size_t i = out.frames(); i-- > 0;) edc[i] = edc[i + 1] + static_cast<double>(out.at(i, 0)) * out.at(i, 0);
        std::size_t t5 = 0;
        std::size_t t25 = 0;
        for (std::size_t i = 0; i < out.frames(); ++i) {
            const double db = 10.0 * std::log10(edc[i] / edc[0] + 1e-30);
            if (!t5 && db < -5.0) t5 = i;
            if (!t25 && db < -25.0) t25 = i;
        }
        const double rt = 3.0 * static_cast<double>(t25 - t5) / 48000.0;
        check(rel_err(rt, 1.5) < 0.35, "reverb RT60 follows decay (" + num(rt) + " s for 1.5 s)");
        check(10.0 * std::log10(edc[out.frames() - 4800] / edc[0] + 1e-30) < -60.0, "reverb tail_frames covers the decay");
        double ll = 0.0;
        double rr = 0.0;
        double lr = 0.0;
        for (std::size_t i = 0; i < out.frames(); ++i) {
            ll += static_cast<double>(out.at(i, 0)) * out.at(i, 0);
            rr += static_cast<double>(out.at(i, 1)) * out.at(i, 1);
            lr += static_cast<double>(out.at(i, 0)) * out.at(i, 1);
        }
        check(std::fabs(lr) / std::sqrt(ll * rr) < 0.5, "reverb output is decorrelated between channels");
        Reverb big;
        big.decay.set(30.0f);
        big.size.set(1.0f);
        big.prepare(48000, 2);
        std::vector<float> v = noise(48000 * 4, 1.0f, 2);
        run(big, v, 2);
        check(all_finite(v, 20.0f), "long reverb stays bounded on full-scale noise");
    }
    {
        Distortion d;
        d.mode.set(1.0f);
        d.drive.set(0.0f);
        d.tone.set(1.0f);
        for (int os = 0; os < 2; ++os) {
            d.oversample.set(static_cast<float>(os));
            d.prepare(48000, 1);
            std::vector<float> v(2000, 0.0f);
            v[100] = 0.5f;
            run(d, v, 1);
            std::size_t at = 0;
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (std::fabs(v[i]) > std::fabs(v[at])) at = i;
            }
            check(at == 100 + d.latency_frames(), "distortion latency report matches (oversample " + std::to_string(os) + ")");
        }
        auto alias_db = [](float oversample, int mode) {
            Distortion e;
            e.mode.set(static_cast<float>(mode));
            e.drive.set(18.0f);
            e.tone.set(1.0f);
            e.oversample.set(oversample);
            e.prepare(48000, 1);
            const std::size_t n = 32768;
            std::vector<float> v = sine(4500.0, n + 8192, 0.5);
            run(e, v, 1);
            Fft fft(n);
            std::vector<float> w = make_window(WindowType::blackman_harris, n);
            std::vector<float> x(n);
            for (std::size_t i = 0; i < n; ++i) x[i] = v[8192 + i] * w[i];
            std::vector<std::complex<float>> spec(n / 2 + 1);
            fft.forward_real(x.data(), spec.data());
            std::vector<bool> harmonic(n / 2 + 1, false);
            for (std::size_t k = 0; k * 4500 < 24000; ++k) {
                const long bin = static_cast<long>(static_cast<double>(k) * 4500.0 / 48000.0 * static_cast<double>(n) + 0.5);
                for (long b = bin - 6; b <= bin + 6; ++b) {
                    if (b >= 0 && b <= static_cast<long>(n / 2)) harmonic[static_cast<std::size_t>(b)] = true;
                }
            }
            double alias = 0.0;
            double total = 0.0;
            for (std::size_t k = 0; k <= n / 2; ++k) {
                const double p = std::norm(spec[k]);
                if (k * 48000 > 20000 * n) continue;
                if (harmonic[k]) total += p;
                else alias += p;
            }
            return 10.0 * std::log10(alias / total + 1e-30);
        };
        for (int mode = 0; mode < 4; ++mode) {
            const double plain = alias_db(0.0f, mode);
            const double over = alias_db(1.0f, mode);
            check(over < plain - 15.0, "4x oversampling reduces aliasing for mode " + std::to_string(mode) + " (" + num(plain) + " dB -> " + num(over) + " dB)");
        }
        Distortion tube;
        tube.mode.set(2.0f);
        tube.drive.set(24.0f);
        tube.prepare(48000, 1);
        std::vector<float> v = sine(200.0, 48000, 0.8);
        run(tube, v, 1);
        double mean = 0.0;
        for (std::size_t i = 24000; i < v.size(); ++i) mean += v[i];
        check(std::fabs(mean / 24000.0) < 1e-3, "tube mode removes its DC offset");
    }
    {
        Bitcrusher b;
        b.bits.set(4.0f);
        b.rate_hz.set(8000.0f);
        b.prepare(48000, 1);
        std::vector<float> v = sine(100.0, 4800, 0.9);
        run(b, v, 1);
        bool quantized = true;
        bool held = true;
        for (std::size_t i = 0; i < v.size(); ++i) {
            const float q = v[i] * 8.0f;
            if (std::fabs(q - std::round(q)) > 1e-4f) quantized = false;
            if (i % 6 != 0 && i > 0 && v[i] != v[i - 1]) held = false;
        }
        check(quantized && held, "bitcrusher quantizes to 4 bits and holds at 8 kHz");
    }
}

void test_allocations() {
    std::vector<std::pair<std::string, std::unique_ptr<Effect>>> effects;
    effects.emplace_back("pitch voice", std::unique_ptr<Effect>(new PitchShifter(7.0f, 0.0f)));
    effects.emplace_back("pitch poly", std::unique_ptr<Effect>(new PitchShifter(7.0f, 1.0f)));
    effects.emplace_back("pitch poly formant", std::unique_ptr<Effect>(new PitchShifter(7.0f, 1.0f)));
    effects.back().second->set("preserve_formants", 1.0f);
    effects.emplace_back("formant", std::unique_ptr<Effect>(new FormantShifter(3.0f)));
    effects.emplace_back("autotune", std::unique_ptr<Effect>(new AutoTune()));
    effects.emplace_back("ring", std::unique_ptr<Effect>(new RingModulator()));
    effects.emplace_back("vocoder", std::unique_ptr<Effect>(new Vocoder()));
    effects.emplace_back("eq", std::unique_ptr<Effect>(new VoiceEQ()));
    effects.emplace_back("chorus", std::unique_ptr<Effect>(new Chorus()));
    effects.emplace_back("flanger", std::unique_ptr<Effect>(new Flanger()));
    effects.emplace_back("phaser", std::unique_ptr<Effect>(new Phaser()));
    effects.emplace_back("tremolo", std::unique_ptr<Effect>(new Tremolo()));
    effects.emplace_back("vibrato", std::unique_ptr<Effect>(new Vibrato()));
    effects.emplace_back("autopan", std::unique_ptr<Effect>(new AutoPan()));
    effects.emplace_back("delay", std::unique_ptr<Effect>(new Delay()));
    effects.emplace_back("reverb", std::unique_ptr<Effect>(new Reverb()));
    effects.emplace_back("distortion", std::unique_ptr<Effect>(new Distortion()));
    effects.emplace_back("bitcrusher", std::unique_ptr<Effect>(new Bitcrusher()));
    effects.emplace_back("voice changer", std::unique_ptr<Effect>(new VoiceChanger(VoicePreset::robot)));
    std::vector<float> v = interleave(vowel_a(150.0, 24000), 2);
    for (auto& item : effects) {
        Effect& e = *item.second;
        e.prepare(48000, 2);
        std::vector<float> data = v;
        const long before = g_allocations.load();
        run(e, data, 2, 128);
        e.set("semitones", -3.0f);
        e.set("mode", 0.0f);
        e.set("bands", 30.0f);
        e.set("preset", 5.0f);
        run(e, data, 2, 128);
        e.set("preset", 9.0f);
        e.reset();
        run(e, data, 2, 333);
        const long after = g_allocations.load();
        check(after == before, item.first + " process() and reset() do not allocate (" + std::to_string(after - before) + ")");
    }
}

void test_cpu() {
    const std::size_t seconds = 10;
    const std::size_t frames = 48000 * seconds;
    const std::vector<float> src = interleave(vowel_a(140.0, frames, 5.0, 30.0), 2);
    auto measure = [&](Effect& e) {
        e.prepare(48000, 2);
        std::vector<float> data = src;
        const auto t0 = std::chrono::steady_clock::now();
        run(e, data, 2, 256);
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(t1 - t0).count() / static_cast<double>(seconds);
    };
    std::printf("real-time factor at 48 kHz stereo, 256-frame blocks:\n");
    std::vector<std::pair<std::string, std::unique_ptr<Effect>>> effects;
    effects.emplace_back("PitchShifter voice", std::unique_ptr<Effect>(new PitchShifter(5.0f, 0.0f)));
    effects.emplace_back("PitchShifter poly", std::unique_ptr<Effect>(new PitchShifter(5.0f, 1.0f)));
    effects.emplace_back("PitchShifter poly+formant", std::unique_ptr<Effect>(new PitchShifter(5.0f, 1.0f)));
    effects.back().second->set("preserve_formants", 1.0f);
    effects.emplace_back("FormantShifter", std::unique_ptr<Effect>(new FormantShifter(3.0f)));
    effects.emplace_back("AutoTune", std::unique_ptr<Effect>(new AutoTune()));
    effects.emplace_back("RingModulator", std::unique_ptr<Effect>(new RingModulator()));
    effects.emplace_back("Vocoder 32 bands", std::unique_ptr<Effect>(new Vocoder()));
    effects.back().second->set("bands", 32.0f);
    effects.emplace_back("VoiceEQ", std::unique_ptr<Effect>(new VoiceEQ()));
    effects.emplace_back("Chorus 4 voices", std::unique_ptr<Effect>(new Chorus()));
    effects.back().second->set("voices", 4.0f);
    effects.emplace_back("Flanger", std::unique_ptr<Effect>(new Flanger()));
    effects.emplace_back("Phaser 12 stages", std::unique_ptr<Effect>(new Phaser()));
    effects.back().second->set("stages", 12.0f);
    effects.emplace_back("Tremolo", std::unique_ptr<Effect>(new Tremolo()));
    effects.emplace_back("Vibrato", std::unique_ptr<Effect>(new Vibrato()));
    effects.emplace_back("AutoPan", std::unique_ptr<Effect>(new AutoPan()));
    effects.emplace_back("Delay", std::unique_ptr<Effect>(new Delay()));
    effects.emplace_back("Reverb", std::unique_ptr<Effect>(new Reverb()));
    effects.emplace_back("Distortion 4x", std::unique_ptr<Effect>(new Distortion()));
    effects.emplace_back("Bitcrusher", std::unique_ptr<Effect>(new Bitcrusher()));
    for (auto& item : effects) std::printf("  %-28s %6.2f%%\n", item.first.c_str(), 100.0 * measure(*item.second));
    double worst = 0.0;
    for (std::size_t p = 0; p < voice_preset_count(); ++p) {
        VoiceChanger vc(static_cast<VoicePreset>(p));
        const double rtf = measure(vc);
        worst = std::max(worst, rtf);
        std::printf("  VoiceChanger %-15s %6.2f%%\n", voice_preset_name(static_cast<VoicePreset>(p)), 100.0 * rtf);
    }
    const char* strict = std::getenv("FIZMO_CPU_BUDGET");
    if (strict) check(worst < 0.15, "every VoiceChanger preset runs under 15% of one core (worst " + num(worst * 100.0) + "%)");
}

} // namespace

int main() {
    test_detector();
    test_pitch_shifter();
    test_formant_shifter();
    test_autotune();
    test_voice_changer();
    test_ring_and_vocoder();
    test_modulation();
    test_time_effects();
    test_allocations();
    test_cpu();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
