#include "../Source/Audio/audio.hpp"
#include "../Source/Audio/dynamics.hpp"
#include "../Source/Audio/edit.hpp"
#include "../Source/Audio/filters.hpp"
#include "../Source/Audio/loudness.hpp"

#include "portable_random.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <vector>

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

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

std::string fmt(const char* f, double a, double b = 0.0, double c = 0.0) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), f, a, b, c);
    return buf;
}

const double k_pi = 3.14159265358979323846;

AudioBuffer sine(double freq, std::uint32_t rate, std::size_t channels, std::size_t frames, double amp, double phase = 0.0) {
    AudioBuffer b(channels, rate, frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float v = static_cast<float>(amp * std::sin(2.0 * k_pi * freq * static_cast<double>(i) / rate + phase));
        for (std::size_t c = 0; c < channels; ++c) b.at(i, c) = v;
    }
    return b;
}

AudioBuffer constant(float value, std::uint32_t rate, std::size_t channels, std::size_t frames) {
    AudioBuffer b(channels, rate, frames);
    for (float& s : b.samples()) s = value;
    return b;
}

double tone_amplitude(const float* data, std::size_t frames, std::size_t stride, double freq, double rate) {
    std::complex<double> acc(0.0, 0.0);
    double wsum = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * k_pi * (static_cast<double>(i) + 0.5) / static_cast<double>(frames));
        const double ph = -2.0 * k_pi * freq * static_cast<double>(i) / rate;
        acc += w * static_cast<double>(data[i * stride]) * std::complex<double>(std::cos(ph), std::sin(ph));
        wsum += w;
    }
    return 2.0 * std::abs(acc) / wsum;
}

void run(Effect& e, AudioBuffer& b, std::size_t block = 512) {
    for (std::size_t pos = 0; pos < b.frames(); pos += block) e.process(b.frame(pos), std::min(block, b.frames() - pos));
}

double measure_gain_db(Effect& e, double freq, std::uint32_t rate) {
    const std::size_t frames = rate + rate / 2;
    AudioBuffer in = sine(freq, rate, 2, frames, 0.25);
    AudioBuffer out = in;
    e.prepare(rate, 2);
    e.reset();
    run(e, out);
    const std::size_t start = rate / 2;
    const double a_in = tone_amplitude(in.frame(start), rate, 2, freq, rate);
    const double a_out = tone_amplitude(out.frame(start), rate, 2, freq, rate);
    return 20.0 * std::log10(std::max(1e-12, a_out / a_in));
}

double analog_db(FilterType type, double f, double fs, double f0, double q, double gain_db) {
    const double omega = std::tan(k_pi * f / fs) / std::tan(k_pi * f0 / fs);
    const std::complex<double> s(0.0, omega);
    const double a = std::pow(10.0, gain_db / 40.0);
    const double sa = std::sqrt(a);
    std::complex<double> h;
    const std::complex<double> den = s * s + s / q + 1.0;
    switch (type) {
        case FilterType::lowpass: h = 1.0 / den; break;
        case FilterType::highpass: h = s * s / den; break;
        case FilterType::bandpass: h = (s / q) / den; break;
        case FilterType::notch: h = (s * s + 1.0) / den; break;
        case FilterType::allpass: h = (s * s - s / q + 1.0) / den; break;
        case FilterType::peak: h = (s * s + s * (a / q) + 1.0) / (s * s + s / (a * q) + 1.0); break;
        case FilterType::lowshelf: h = a * (s * s + (sa / q) * s + a) / (a * s * s + (sa / q) * s + 1.0); break;
        case FilterType::highshelf: h = a * (a * s * s + (sa / q) * s + 1.0) / (s * s + (sa / q) * s + a); break;
    }
    return 20.0 * std::log10(std::max(1e-300, std::abs(h)));
}

void test_biquad_design() {
    const FilterType types[] = { FilterType::lowpass, FilterType::highpass, FilterType::bandpass, FilterType::notch, FilterType::peak, FilterType::lowshelf, FilterType::highshelf, FilterType::allpass };
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const double f0s[] = { 50.0, 1000.0, 9000.0 };
    const double qs[] = { 0.5, 0.7071, 3.0 };
    const double gains[] = { -12.0, 6.0 };
    const double probes[] = { 20.0, 100.0, 440.0, 1000.0, 2500.0, 7000.0, 12000.0, 18000.0 };
    double worst = 0.0;
    for (FilterType t : types) {
        for (double fs : rates) {
            for (double f0 : f0s) {
                for (double q : qs) {
                    for (double g : gains) {
                        const BiquadCoefficients c = design_biquad(t, fs, f0, q, g);
                        for (double f : probes) {
                            const double expect = analog_db(t, f, fs, f0, q, g);
                            if (expect < -150.0) continue;
                            worst = std::max(worst, std::fabs(c.magnitude_db(f, fs) - expect));
                        }
                    }
                }
            }
        }
    }
    check(worst < 1e-6, fmt("biquad design matches analog prototype (worst %.3g dB)", worst));
    const BiquadCoefficients lp = design_biquad(FilterType::lowpass, 48000.0, 1000.0, std::sqrt(0.5), 0.0);
    check(near(lp.magnitude_db(1000.0, 48000.0), -3.0103, 1e-3), "butterworth lowpass -3.01 dB at cutoff");
    const BiquadCoefficients pk = design_biquad(FilterType::peak, 48000.0, 2000.0, 1.0, 9.0);
    check(near(pk.magnitude_db(2000.0, 48000.0), 9.0, 1e-9), "peak exact gain at centre");
    const BiquadCoefficients ls = design_biquad(FilterType::lowshelf, 48000.0, 300.0, std::sqrt(0.5), 8.0);
    check(near(ls.magnitude_db(300.0, 48000.0), 4.0, 1e-9) && near(ls.magnitude_db(5.0, 48000.0), 8.0, 0.01), "lowshelf half gain at f0, full gain at DC");
    const BiquadCoefficients ap = design_biquad(FilterType::allpass, 48000.0, 700.0, 2.0, 0.0);
    check(near(ap.magnitude_db(123.0, 48000.0), 0.0, 1e-9) && near(ap.magnitude_db(15000.0, 48000.0), 0.0, 1e-9), "allpass unity magnitude");
    const std::vector<double> bq = butterworth_q(2);
    check(bq.size() == 2 && near(bq[0], 0.5411961, 1e-6) && near(bq[1], 1.3065630, 1e-6), "butterworth Q values 4th order");
}

void test_filter_measured() {
    struct Case {
        FilterType type;
        double f0;
        double q;
        double gain;
    };
    const Case cases[] = {
        { FilterType::lowpass, 1000.0, 0.7071, 0.0 },
        { FilterType::highpass, 400.0, 2.0, 0.0 },
        { FilterType::bandpass, 2000.0, 1.5, 0.0 },
        { FilterType::notch, 3000.0, 1.0, 0.0 },
        { FilterType::peak, 800.0, 1.2, 9.0 },
        { FilterType::lowshelf, 200.0, 0.7071, -6.0 },
        { FilterType::highshelf, 5000.0, 0.7071, 4.5 },
        { FilterType::allpass, 1500.0, 0.8, 0.0 },
    };
    const double probes[] = { 60.0, 250.0, 1000.0, 2000.0, 3100.0, 6000.0, 15000.0 };
    const std::uint32_t rate = 48000;
    for (const Case& c : cases) {
        double worst = 0.0;
        for (double f : probes) {
            Filter filt(c.type, static_cast<float>(c.f0), static_cast<float>(c.q), static_cast<float>(c.gain));
            const double measured = measure_gain_db(filt, f, rate);
            const double expect = analog_db(c.type, f, rate, c.f0, c.q, c.gain);
            if (expect < -60.0) {
                worst = std::max(worst, measured > -50.0 ? 99.0 : 0.0);
                continue;
            }
            worst = std::max(worst, std::fabs(measured - expect));
        }
        check(worst < 0.02, fmt("measured sine gain matches analytic response, type %.0f (worst %.4f dB)", static_cast<double>(c.type), worst));
    }
}

void test_cascaded_slopes() {
    const std::uint32_t rate = 48000;
    const double fc = 500.0;
    const double probes[] = { 100.0, 500.0, 1000.0, 2000.0, 4000.0 };
    for (int stages = 1; stages <= 4; ++stages) {
        const int order = stages * 2;
        double worst_lp = 0.0;
        double worst_hp = 0.0;
        for (double f : probes) {
            const double ratio = std::tan(k_pi * f / rate) / std::tan(k_pi * fc / rate);
            const double lp_expect = -10.0 * std::log10(1.0 + std::pow(ratio, 2.0 * order));
            const double hp_expect = -10.0 * std::log10(1.0 + std::pow(1.0 / ratio, 2.0 * order));
            Filter lp(FilterType::lowpass, static_cast<float>(fc), 0.70710678f, 0.0f, static_cast<float>(12 * stages));
            Filter hp(FilterType::highpass, static_cast<float>(fc), 0.70710678f, 0.0f, static_cast<float>(12 * stages));
            const double ml = measure_gain_db(lp, f, rate);
            const double mh = measure_gain_db(hp, f, rate);
            if (lp_expect > -90.0) worst_lp = std::max(worst_lp, std::fabs(ml - lp_expect));
            if (hp_expect > -90.0) worst_hp = std::max(worst_hp, std::fabs(mh - hp_expect));
        }
        check(worst_lp < 0.05, fmt("%.0f dB/oct lowpass is Butterworth (worst %.4f dB)", 12.0 * stages, worst_lp));
        check(worst_hp < 0.05, fmt("%.0f dB/oct highpass is Butterworth (worst %.4f dB)", 12.0 * stages, worst_hp));
        Filter lp(FilterType::lowpass, static_cast<float>(fc), 0.70710678f, 0.0f, static_cast<float>(12 * stages));
        const double g2 = measure_gain_db(lp, 2000.0, rate);
        const double g4 = measure_gain_db(lp, 4000.0, rate);
        check(near(g2 - g4, 6.0 * order, 1.5), fmt("%.0f dB/oct slope measured between 2k and 4k: %.2f dB", 12.0 * stages, g2 - g4));
    }
    Filter res(FilterType::lowpass, 1000.0f, 4.0f, 0.0f, 24.0f);
    const double peak = measure_gain_db(res, 1000.0, rate);
    check(peak > 9.0, fmt("resonant cascaded lowpass boosts at cutoff (%.2f dB)", peak));
}

void test_eq_response() {
    const std::uint32_t rate = 48000;
    ParametricEq eq(6);
    eq.set_band(0, FilterType::highpass, 40.0f, 0.7071f, 0.0f);
    eq.set_band(1, FilterType::lowshelf, 150.0f, 0.7071f, 4.0f);
    eq.set_band(2, FilterType::peak, 700.0f, 2.0f, -7.5f);
    eq.set_band(3, FilterType::peak, 3200.0f, 0.9f, 5.0f);
    eq.set_band(4, FilterType::highshelf, 9000.0f, 0.7071f, -3.0f);
    eq.set_band(5, FilterType::notch, 12345.0f, 4.0f, 0.0f, false);
    eq.output.set(-2.0f);
    std::vector<float> freqs = { 30.0f, 60.0f, 120.0f, 300.0f, 650.0f, 700.0f, 1500.0f, 3200.0f, 6000.0f, 10000.0f, 16000.0f };
    eq.prepare(rate, 2);
    const std::vector<float> predicted = frequency_response(eq, freqs);
    double worst = 0.0;
    for (std::size_t i = 0; i < freqs.size(); ++i) {
        const double m = measure_gain_db(eq, freqs[i], rate);
        worst = std::max(worst, std::fabs(m - predicted[i]));
    }
    check(worst < 0.03, fmt("parametric EQ frequency_response matches measured (worst %.4f dB)", worst));
    check(near(predicted[5], -7.5 - 2.0 + analog_db(FilterType::lowshelf, 700.0, rate, 150.0, 0.7071, 4.0) + analog_db(FilterType::highpass, 700.0, rate, 40.0, 0.7071, 0.0) + analog_db(FilterType::peak, 700.0, rate, 3200.0, 0.9, 5.0) + analog_db(FilterType::highshelf, 700.0, rate, 9000.0, 0.7071, -3.0), 0.01), "EQ response sums bands in dB");
    check(eq.parameters().size() == 6 * 5 + 1 && eq.parameter("band3_gain") == &eq.band(2).gain, "EQ band parameters registered by name");
    ParametricEq big(40);
    check(big.band_count() == ParametricEq::max_bands, "EQ band count capped at 16");

    GraphicEq geq;
    for (std::size_t i = 0; i < GraphicEq::band_count; ++i) geq.band(i).set(i % 2 ? 12.0f : -12.0f);
    geq.band(5).set(15.0f);
    check(geq.band(5).get() == 12.0f, "graphic EQ clamps to +-12 dB");
    geq.prepare(rate, 2);
    std::vector<float> gf = { 31.5f, 125.0f, 1000.0f, 4000.0f, 16000.0f };
    const std::vector<float> gp = geq.response_db(gf);
    double gworst = 0.0;
    for (std::size_t i = 0; i < gf.size(); ++i) gworst = std::max(gworst, std::fabs(measure_gain_db(geq, gf[i], rate) - gp[i]));
    check(gworst < 0.03, fmt("graphic EQ measured matches response (worst %.4f dB)", gworst));
    GraphicEq flat;
    check(std::fabs(measure_gain_db(flat, 1000.0, rate)) < 1e-4, "flat graphic EQ is transparent");
}

double max_jump(const AudioBuffer& b) {
    double worst = 0.0;
    for (std::size_t i = 1; i < b.frames(); ++i) {
        for (std::size_t c = 0; c < b.channels(); ++c) worst = std::max(worst, static_cast<double>(std::fabs(b.at(i, c) - b.at(i - 1, c))));
    }
    return worst;
}

void test_no_clicks() {
    const std::uint32_t rate = 48000;
    const double amp = 0.5;
    const double natural = 2.0 * k_pi * 1000.0 / rate * amp;
    {
        Filter f(FilterType::lowpass, 8000.0f, 0.7071f);
        AudioBuffer b = sine(1000.0, rate, 2, rate * 2, amp);
        f.prepare(rate, 2);
        for (std::size_t pos = 0, k = 0; pos < b.frames(); pos += 256, ++k) {
            f.frequency.set(k % 2 ? 150.0f : 12000.0f);
            f.q.set(k % 3 ? 0.7f : 6.0f);
            f.process(b.frame(pos), std::min<std::size_t>(256, b.frames() - pos));
        }
        const double j = max_jump(b);
        check(j < natural * 1.6, fmt("filter sweep between 150 Hz and 12 kHz has no clicks (max step %.4f, sine step %.4f)", j, natural));
    }
    {
        AudioBuffer ref = sine(1000.0, rate, 1, rate, amp);
        Biquad raw(1);
        raw.set_coefficients(design_biquad(FilterType::lowpass, rate, 12000.0, 0.7, 0.0));
        for (std::size_t pos = 0, k = 0; pos < ref.frames(); pos += 256, ++k) {
            raw.set_coefficients(design_biquad(FilterType::lowpass, rate, k % 2 ? 150.0 : 12000.0, k % 3 ? 0.7 : 6.0, 0.0));
            raw.process(ref.frame(pos), std::min<std::size_t>(256, ref.frames() - pos));
        }
        std::printf("  info: unsmoothed coefficient switching max step %.4f\n", max_jump(ref));
    }
    {
        Filter f(FilterType::lowpass, 1000.0f, 0.7071f, 0.0f, 12.0f);
        AudioBuffer b = sine(1000.0, rate, 2, rate * 2, amp);
        f.prepare(rate, 2);
        const FilterType types[] = { FilterType::lowpass, FilterType::highpass, FilterType::peak, FilterType::bandpass, FilterType::lowshelf, FilterType::notch };
        for (std::size_t pos = 0, k = 0; pos < b.frames(); pos += 2048, ++k) {
            f.set_type(types[k % 6]);
            f.slope.set(static_cast<float>(12 * (1 + k % 4)));
            f.gain.set(k % 2 ? 12.0f : -12.0f);
            f.process(b.frame(pos), std::min<std::size_t>(2048, b.frames() - pos));
        }
        const double j = max_jump(b);
        check(j < natural * 2.5, fmt("filter type and slope switching bounded (max step %.4f)", j));
    }
    {
        ParametricEq eq(4);
        eq.set_band(0, FilterType::peak, 1000.0f, 1.0f, 12.0f, false);
        AudioBuffer b = sine(1000.0, rate, 2, rate, amp * 0.25);
        eq.prepare(rate, 2);
        for (std::size_t pos = 0, k = 0; pos < b.frames(); pos += 1024, ++k) {
            eq.band(0).enabled.set(k % 2 ? 1.0f : 0.0f);
            eq.output.set(k % 3 ? 0.0f : -6.0f);
            eq.process(b.frame(pos), std::min<std::size_t>(1024, b.frames() - pos));
        }
        const double j = max_jump(b);
        check(j < natural * 0.25 * 4.0 * 1.3, fmt("EQ band enable toggling has no clicks (max step %.4f)", j));
    }
    {
        Filter f(FilterType::peak, 1000.0f, 1.0f, 0.0f);
        AudioBuffer b = sine(1000.0, rate, 1, rate / 2, 0.1);
        f.prepare(rate, 1);
        f.gain.set(24.0f);
        run(f, b, 64);
        double prev = 0.0;
        bool monotone = true;
        for (std::size_t w = 0; w < 20; ++w) {
            const double a = tone_amplitude(b.frame(w * 480), 480, 1, 1000.0, rate);
            if (a + 1e-3 < prev) monotone = false;
            prev = a;
        }
        check(monotone && near(20.0 * std::log10(prev / 0.1), 24.0, 0.2), "gain change ramps smoothly to target");
    }
    {
        AudioBuffer b(2, rate, rate);
        for (std::size_t i = 0; i < b.frames(); ++i) {
            b.at(i, 0) = 0.5f + 0.1f * static_cast<float>(std::sin(2.0 * k_pi * 440.0 * static_cast<double>(i) / rate));
            b.at(i, 1) = -0.25f;
        }
        DcBlocker dc(10.0f);
        dc.prepare(rate, 2);
        run(dc, b);
        double mean0 = 0.0;
        double mean1 = 0.0;
        for (std::size_t i = rate / 2; i < rate; ++i) {
            mean0 += b.at(i, 0);
            mean1 += b.at(i, 1);
        }
        mean0 /= rate / 2;
        mean1 /= rate / 2;
        const double a = tone_amplitude(b.frame(rate / 2), rate / 2, 2, 440.0, rate);
        check(std::fabs(mean0) < 1e-3 && std::fabs(mean1) < 1e-3 && near(a, 0.1, 0.002), "DC blocker removes offset, keeps tone");
    }
}

void test_compressor_static() {
    const std::uint32_t rate = 48000;
    for (float knee_db : { 0.0f, 6.0f, 12.0f }) {
        double worst = 0.0;
        for (double level = -60.0; level <= 0.0; level += 3.0) {
            Compressor comp(-24.0f, 4.0f, 0.1f, 5.0f);
            comp.knee.set(knee_db);
            const float a = static_cast<float>(std::pow(10.0, level / 20.0));
            AudioBuffer b = constant(a, rate, 2, rate / 2);
            comp.prepare(rate, 2);
            run(comp, b);
            const double out_db = 20.0 * std::log10(b.at(b.frames() - 1, 0) / a);
            double expect = 0.0;
            const double over = level + 24.0;
            if (2.0 * std::fabs(over) <= knee_db && knee_db > 0.0f) expect = (0.25 - 1.0) * (over + knee_db / 2.0) * (over + knee_db / 2.0) / (2.0 * knee_db);
            else if (over > 0.0) expect = over * (0.25 - 1.0);
            worst = std::max(worst, std::fabs(out_db - expect));
            worst = std::max(worst, std::fabs(comp.static_gain_db(static_cast<float>(level)) - expect));
        }
        check(worst < 0.01, fmt("compressor static curve, knee %.0f dB (worst %.4f dB)", knee_db, worst));
    }
    {
        Compressor comp(-30.0f, 3.0f, 1.0f, 50.0f);
        comp.set_detection(Detection::rms);
        AudioBuffer b = sine(1000.0, rate, 2, rate, 0.5);
        comp.prepare(rate, 2);
        run(comp, b);
        const double in_rms_db = 20.0 * std::log10(0.5 / std::sqrt(2.0));
        const double expect = (in_rms_db + 30.0) * (1.0 / 3.0 - 1.0);
        const double got = 20.0 * std::log10(tone_amplitude(b.frame(rate / 2), rate / 2, 2, 1000.0, rate) / 0.5);
        check(near(got, expect, 0.25), fmt("RMS detection compresses sine by RMS level (got %.2f, want %.2f)", got, expect));
    }
    {
        Compressor comp(-20.0f, 4.0f);
        comp.knee.set(0.0f);
        comp.auto_makeup.set(1.0f);
        comp.makeup.set(1.0f);
        check(near(comp.makeup_db(), 1.0 + 0.5 * 20.0 * 0.75, 1e-4), "auto makeup is half the reduction at 0 dBFS plus manual makeup");
    }
}

void test_compressor_timing() {
    const std::uint32_t rate = 48000;
    Compressor comp(-20.0f, 10.0f, 10.0f, 100.0f);
    comp.knee.set(0.0f);
    const std::size_t pre = rate / 2;
    const std::size_t loud = rate;
    const std::size_t post = rate;
    AudioBuffer b(2, rate, pre + loud + post);
    for (std::size_t i = 0; i < b.frames(); ++i) {
        const float v = (i >= pre && i < pre + loud) ? 1.0f : 0.01f;
        b.at(i, 0) = v;
        b.at(i, 1) = v;
    }
    AudioBuffer in = b;
    comp.prepare(rate, 2);
    run(comp, b, 128);
    auto gr_at = [&](std::size_t i) { return 20.0 * std::log10(b.at(i, 0) / in.at(i, 0)); };
    const double full = -18.0;
    const double after_attack = gr_at(pre + rate / 100 - 1);
    check(near(after_attack, full * (1.0 - std::exp(-1.0)), 0.3), fmt("attack time constant: %.2f dB after 10 ms (want %.2f)", after_attack, full * (1.0 - std::exp(-1.0))));
    check(near(gr_at(pre + loud - 1), full, 0.01), "steady state reduction -18 dB");
    const double after_release = gr_at(pre + loud + rate / 10 - 1);
    check(near(after_release, full * std::exp(-1.0), 0.3), fmt("release time constant: %.2f dB after 100 ms (want %.2f)", after_release, full * std::exp(-1.0)));
    check(near(gr_at(b.frames() - 1), 0.0, 0.01), "fully released after 1 s");
    {
        Compressor m(-20.0f, 10.0f, 1.0f, 100.0f);
        m.knee.set(0.0f);
        AudioBuffer c = constant(1.0f, rate, 2, rate / 4);
        m.prepare(rate, 2);
        run(m, c);
        check(near(m.gain_reduction_db(), -18.0, 0.05), fmt("gain reduction meter reads %.2f dB", m.gain_reduction_db()));
    }
    {
        Compressor la(0.0f, 1.0f);
        la.lookahead.set(5.0f);
        la.prepare(rate, 2);
        check(la.latency_frames() == 240, "lookahead 5 ms = 240 frames latency");
        AudioBuffer s = sine(300.0, rate, 2, 4096, 0.3);
        AudioBuffer o = s;
        run(la, o, 100);
        double err = 0.0;
        for (std::size_t i = 240; i < o.frames(); ++i) err = std::max(err, static_cast<double>(std::fabs(o.at(i, 1) - s.at(i - 240, 1))));
        check(err < 1e-6, "lookahead delays audio by latency_frames");
        Compressor lb(-20.0f, 10.0f, 1.0f, 100.0f);
        lb.knee.set(0.0f);
        lb.lookahead.set(5.0f);
        AudioBuffer step(1, rate, rate / 2);
        for (std::size_t i = 0; i < step.frames(); ++i) step.at(i, 0) = i >= rate / 4 ? 1.0f : 0.01f;
        AudioBuffer stepped = applied(lb, step, false);
        const double first = 20.0 * std::log10(stepped.at(rate / 4, 0));
        check(first < -10.0, fmt("lookahead reduces gain before the transient arrives (%.2f dB at onset)", first));
    }
    {
        AudioBuffer lowf = sine(50.0, rate, 2, rate, 0.5);
        Compressor plain(-30.0f, 8.0f, 1.0f, 50.0f);
        Compressor hp(-30.0f, 8.0f, 1.0f, 50.0f);
        hp.sidechain_hpf.set(500.0f);
        AudioBuffer a = applied(plain, lowf);
        AudioBuffer c = applied(hp, lowf);
        const double ga = tone_amplitude(a.frame(rate / 2), rate / 2, 2, 50.0, rate);
        const double gc = tone_amplitude(c.frame(rate / 2), rate / 2, 2, 50.0, rate);
        check(20.0 * std::log10(gc / ga) > 10.0, fmt("sidechain highpass ignores bass (%.1f dB less reduction)", 20.0 * std::log10(gc / ga)));
    }
    {
        Compressor unlinked(-30.0f, 10.0f, 1.0f, 50.0f);
        unlinked.knee.set(0.0f);
        unlinked.link.set(0.0f);
        AudioBuffer s(2, rate, rate / 2);
        for (std::size_t i = 0; i < s.frames(); ++i) {
            s.at(i, 0) = 1.0f;
            s.at(i, 1) = 0.01f;
        }
        Compressor linked(-30.0f, 10.0f, 1.0f, 50.0f);
        linked.knee.set(0.0f);
        AudioBuffer u = applied(unlinked, s);
        AudioBuffer l = applied(linked, s);
        check(near(u.at(u.frames() - 1, 1), 0.01, 1e-5) && near(20.0 * std::log10(l.at(l.frames() - 1, 1) / 0.01), -27.0, 0.05), "stereo link applies loud channel reduction to both");
    }
}

AudioBuffer nasty_signal(std::uint32_t rate, std::size_t frames) {
    std::mt19937 rng(1234);
    auto n = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 1.0f); };
    AudioBuffer b(2, rate, frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        float v = 0.0f;
        const std::size_t seg = (i / (rate / 4)) % 6;
        if (seg == 0) v = 3.0f * n(rng);
        if (seg == 1) v = (std::fmod(t * 60.0, 1.0) < 0.5 ? 4.0f : -4.0f);
        if (seg == 2) v = (i % 997 == 0) ? 16.0f : 0.05f * n(rng);
        if (seg == 3) v = static_cast<float>(6.0 * std::sin(2.0 * k_pi * (100.0 + 20000.0 * std::fmod(t, 0.25) * 4.0) * t));
        if (seg == 4) v = static_cast<float>(5.0 * std::sin(2.0 * k_pi * rate * 0.25 * t + 0.785398));
        if (seg == 5) v = (i % 2 ? 7.0f : -7.0f) * (std::fmod(t * 8.0, 1.0) < 0.3 ? 1.0f : 0.1f);
        b.at(i, 0) = v;
        b.at(i, 1) = i % 3 == 0 ? -v : 0.5f * v;
    }
    return b;
}

void test_limiter() {
    for (std::uint32_t rate : { 44100u, 48000u, 96000u }) {
        for (bool tp : { false, true }) {
            Limiter lim(-1.0f, 30.0f, 3.0f, tp);
            AudioBuffer b = nasty_signal(rate, rate * 3);
            lim.prepare(rate, 2);
            std::mt19937 rng(7);
            auto bs = [&](auto& gen) { return fizmo_test::random_int<int>(gen, 1, 700); };
            for (std::size_t pos = 0; pos < b.frames();) {
                const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(bs(rng)), b.frames() - pos);
                lim.process(b.frame(pos), n);
                pos += n;
            }
            const float ceiling = db_to_gain(-1.0f);
            float worst = 0.0f;
            for (float s : b.samples()) worst = std::max(worst, std::fabs(s));
            check(worst <= ceiling, fmt("limiter never exceeds ceiling (rate %.0f, tp %.0f, peak %.6f dB)", rate, tp ? 1.0 : 0.0, 20.0 * std::log10(worst)));
            if (tp) {
                const float tpdb = true_peak_db(b);
                check(tpdb <= -1.0f + 0.35f, fmt("true-peak limiter output true peak %.2f dBTP (rate %.0f)", tpdb, rate));
            }
        }
    }
    {
        const std::uint32_t rate = 48000;
        Limiter lim(-1.0f, 50.0f, 5.0f);
        AudioBuffer b = sine(100.0, rate, 2, rate * 2, 2.0);
        lim.prepare(rate, 2);
        run(lim, b);
        const float ceiling = db_to_gain(-1.0f);
        std::size_t at_ceiling = 0;
        float peak = 0.0f;
        for (std::size_t i = rate; i < b.frames(); ++i) {
            const float v = std::fabs(b.at(i, 0));
            peak = std::max(peak, v);
            if (v >= ceiling * 0.99999f) ++at_ceiling;
        }
        check(peak > ceiling * 0.93f && at_ceiling < rate / 100, fmt("limiter rides gain instead of clipping (peak %.3f, %.0f samples at ceiling)", peak, static_cast<double>(at_ceiling)));
        const double h3 = tone_amplitude(b.frame(rate), rate, 2, 300.0, rate) / tone_amplitude(b.frame(rate), rate, 2, 100.0, rate);
        check(h3 < 0.03, fmt("limiter distortion on steady sine low (3rd harmonic %.4f)", h3));
        check(lim.gain_reduction_db() < -5.0, "limiter gain reduction meter");
    }
    {
        const std::uint32_t rate = 48000;
        Limiter lim(-1.0f, 50.0f, 5.0f, true);
        lim.prepare(rate, 1);
        check(lim.latency_frames() == 240 - 1 + TruePeakDetector::taps_per_phase / 2, "true-peak limiter latency");
        AudioBuffer q = sine(440.0, rate, 1, 9000, 0.25);
        AudioBuffer o = applied(lim, q, false);
        double err = 0.0;
        for (std::size_t i = 0; i < q.frames(); ++i) err = std::max(err, static_cast<double>(std::fabs(o.at(i, 0) - q.at(i, 0))));
        check(err < 1e-6 && o.frames() == q.frames(), "quiet signal passes unchanged and latency-compensated by apply()");
        AudioBuffer fs4 = sine(12000.0, rate, 1, rate, 1.0, k_pi / 4.0);
        Limiter samp(-1.0f, 50.0f, 5.0f, false);
        const float before = true_peak_db(applied(samp, fs4, false));
        const float after = true_peak_db(applied(lim, fs4, false));
        check(before > -0.2f && after <= -0.9f, fmt("fs/4 sine: sample limiter leaves %.2f dBTP, true-peak limiter %.2f dBTP", before, after));
    }
}

void test_gate_and_expander() {
    const std::uint32_t rate = 48000;
    const double levels[] = { -60.0, -37.0, -43.0, -50.0, -43.0, -30.0 };
    const bool expect_open[] = { false, true, true, false, false, true };
    NoiseGate gate(-40.0f, 6.0f, 1.0f, 50.0f, 20.0f, -80.0f);
    gate.prepare(rate, 2);
    const std::size_t seg = rate / 2;
    bool all = true;
    std::string seen;
    for (std::size_t s = 0; s < 6; ++s) {
        AudioBuffer b = sine(1000.0, rate, 2, seg, std::pow(10.0, levels[s] / 20.0));
        AudioBuffer in = b;
        run(gate, b, 256);
        const bool open = gate.is_open();
        seen += open ? "O" : "C";
        if (open != expect_open[s]) all = false;
        const double g = 20.0 * std::log10(std::max(1e-12, tone_amplitude(b.frame(seg / 2), seg / 2, 2, 1000.0, rate) / tone_amplitude(in.frame(seg / 2), seg / 2, 2, 1000.0, rate)));
        if (expect_open[s] && !near(g, 0.0, 0.05)) all = false;
        if (!expect_open[s] && g > -79.0) all = false;
    }
    check(all, "gate opens above threshold, holds open within hysteresis, closes below, needs threshold to reopen: " + seen);
    {
        NoiseGate g(-40.0f, 6.0f, 1.0f, 100.0f, 10.0f, -60.0f);
        g.prepare(rate, 1);
        AudioBuffer loud = sine(1000.0, rate, 1, rate / 4, 0.5);
        run(g, loud);
        AudioBuffer quiet(1, rate, rate / 2);
        std::size_t closed_at = 0;
        for (std::size_t pos = 0; pos < quiet.frames(); pos += 48) {
            g.process(quiet.frame(pos), 48);
            if (!g.is_open() && closed_at == 0) closed_at = pos + 48;
        }
        const double ms = 1000.0 * static_cast<double>(closed_at) / rate;
        check(ms > 100.0 && ms < 200.0, fmt("gate hold: closes %.1f ms after signal stops (hold 100 ms)", ms));
    }
    {
        NoiseGate g(-40.0f, 6.0f, 0.1f, 0.0f, 50.0f, -20.0f);
        AudioBuffer q = sine(1000.0, rate, 1, rate, 0.001);
        AudioBuffer o = applied(g, q);
        const double att = 20.0 * std::log10(tone_amplitude(o.frame(rate / 2), rate / 2, 1, 1000.0, rate) / 0.001);
        check(near(att, -20.0, 0.1), fmt("gate range sets closed attenuation (%.2f dB)", att));
    }
    {
        double worst = 0.0;
        for (double level = -80.0; level <= 0.0; level += 5.0) {
            Expander ex(-30.0f, 3.0f, 0.1f, 5.0f, -40.0f);
            const float a = static_cast<float>(std::pow(10.0, level / 20.0));
            AudioBuffer b = constant(a, rate, 1, rate / 2);
            ex.prepare(rate, 1);
            run(ex, b);
            const double got = 20.0 * std::log10(b.at(b.frames() - 1, 0) / a);
            const double expect = level >= -30.0 ? 0.0 : std::max(-40.0, (level + 30.0) * 2.0);
            worst = std::max(worst, std::fabs(got - expect));
            worst = std::max(worst, std::fabs(ex.static_gain_db(static_cast<float>(level)) - expect));
        }
        check(worst < 0.01, fmt("expander static curve with range floor (worst %.4f dB)", worst));
    }
    {
        DeEsser de(5000.0f, -30.0f, 8.0f);
        AudioBuffer hi = sine(9000.0, rate, 2, rate, 0.3);
        AudioBuffer lo = sine(150.0, rate, 2, rate, 0.3);
        AudioBuffer ho = applied(de, hi);
        const double hg = 20.0 * std::log10(tone_amplitude(ho.frame(rate / 2), rate / 2, 2, 9000.0, rate) / 0.3);
        const float meter = de.gain_reduction_db();
        AudioBuffer lo_out = applied(de, lo);
        const double lg = 20.0 * std::log10(tone_amplitude(lo_out.frame(rate / 2), rate / 2, 2, 150.0, rate) / 0.3);
        check(hg < -6.0 && hg > -9.0 && meter < -7.0, fmt("de-esser reduces sibilant band (%.2f dB, meter %.2f)", hg, meter));
        check(near(lg, 0.0, 0.05), fmt("de-esser leaves low frequencies (%.3f dB)", lg));
    }
}

AudioBuffer tone_sections(std::uint32_t rate, std::size_t channels, const std::vector<std::pair<double, double>>& sections, double freq = 1000.0) {
    AudioBuffer b(channels, rate);
    std::size_t start = 0;
    for (const auto& s : sections) {
        const std::size_t n = static_cast<std::size_t>(std::llround(s.first * rate));
        const double amp = s.second <= -200.0 ? 0.0 : std::pow(10.0, s.second / 20.0);
        b.resize(start + n);
        for (std::size_t i = 0; i < n; ++i) {
            const float v = static_cast<float>(amp * std::sin(2.0 * k_pi * freq * static_cast<double>(start + i) / rate));
            for (std::size_t c = 0; c < channels; ++c) b.at(start + i, c) = v;
        }
        start += n;
    }
    return b;
}

void test_loudness() {
    {
        AudioBuffer b = tone_sections(48000, 2, { { 20.0, -23.0 } });
        LoudnessMeter m(48000, 2);
        m.push(b.data(), b.frames());
        check(near(m.integrated(), -23.0, 0.1) && near(m.momentary(), -23.0, 0.1) && near(m.short_term(), -23.0, 0.1), fmt("EBU 3341 #1: -23 dBFS stereo sine: I %.3f M %.3f S %.3f", m.integrated(), m.momentary(), m.short_term()));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 20.0, -33.0 } }));
        check(near(r.integrated, -33.0, 0.1), fmt("EBU 3341 #2: -33 dBFS: I %.3f", r.integrated));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 } }));
        check(near(r.integrated, -23.0, 0.1), fmt("EBU 3341 #3 relative gate: I %.3f", r.integrated));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 10.0, -72.0 }, { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 }, { 10.0, -72.0 } }));
        check(near(r.integrated, -23.0, 0.1), fmt("EBU 3341 #4 absolute+relative gate: I %.3f", r.integrated));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 20.0, -26.0 }, { 20.1, -20.0 }, { 20.0, -26.0 } }));
        check(near(r.integrated, -23.0, 0.1), fmt("EBU 3341 #5 -26/-20/-26: I %.3f", r.integrated));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 10.0, -23.0 }, { 10.0, -300.0 } }));
        check(near(r.integrated, -23.0, 0.1), fmt("digital silence gated out: I %.3f", r.integrated));
    }
    {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, { { 0.3, -23.0 } }));
        check(std::isinf(r.integrated) && r.integrated < 0.0f, "shorter than one gating block gives -inf");
    }
    for (std::uint32_t rate : { 44100u, 32000u, 96000u, 192000u }) {
        const LoudnessResult r = measure_loudness(tone_sections(rate, 2, { { 10.0, -23.0 } }));
        check(near(r.integrated, -23.0, 0.1), fmt("-23 dBFS sine at %.0f Hz: I %.3f", rate, r.integrated));
    }
    {
        const double max_m = measure_loudness(tone_sections(48000, 2, { { 5.0, -30.0 }, { 1.0, -20.0 }, { 5.0, -30.0 } })).max_momentary;
        check(near(max_m, -20.0, 0.1), fmt("max momentary catches 1 s burst: %.3f", max_m));
    }
    struct LraCase {
        std::vector<std::pair<double, double>> sections;
        double expect;
    };
    const LraCase lra_cases[] = {
        { { { 20.0, -20.0 }, { 20.0, -30.0 } }, 10.0 },
        { { { 20.0, -20.0 }, { 20.0, -15.0 } }, 5.0 },
        { { { 20.0, -40.0 }, { 20.0, -20.0 } }, 20.0 },
        { { { 20.0, -50.0 }, { 20.0, -35.0 }, { 20.0, -20.0 }, { 20.0, -35.0 }, { 20.0, -50.0 } }, 15.0 },
    };
    int n = 1;
    for (const LraCase& c : lra_cases) {
        const LoudnessResult r = measure_loudness(tone_sections(48000, 2, c.sections));
        check(near(r.range, c.expect, 1.0), fmt("EBU 3342 LRA case %.0f: %.2f LU (want %.0f +-1)", n, r.range, c.expect));
        ++n;
    }
    {
        const LoudnessResult r = measure_loudness(sine(12000.0, 48000, 2, 48000 * 3, 0.5, k_pi / 4.0));
        check(near(r.sample_peak_db, -9.03, 0.05), fmt("fs/4 45deg sine sample peak %.3f dBFS", r.sample_peak_db));
        check(r.true_peak_db < -6.02 + 0.2 && r.true_peak_db > -6.02 - 0.4, fmt("fs/4 45deg sine true peak %.3f dBTP (want -6.02 +0.2/-0.4)", r.true_peak_db));
    }
    {
        const double probes[] = { 997.0, 5000.0, 11025.0, 15000.0, 18000.0 };
        double worst = 0.0;
        for (double f : probes) {
            for (double ph : { 0.0, 0.3, 1.1 }) {
                AudioBuffer t = sine(f, 48000, 1, 48000, 0.5, ph);
                fade_in(t, 0.01, FadeCurve::s_curve);
                fade_out(t, 0.01, FadeCurve::s_curve);
                const float tp = true_peak_db(t);
                worst = std::max(worst, std::fabs(tp + 6.0206));
            }
        }
        check(worst < 0.1, fmt("true peak of sines up to 18 kHz within 0.1 dB (worst %.3f)", worst));
    }
    {
        AudioBuffer b(1, 48000, 48000);
        b.at(24000, 0) = 1.0f;
        const float tp = true_peak_db(b);
        check(tp >= 0.0f && tp < 0.01f, fmt("impulse true peak equals sample peak (%.3f)", tp));
    }
    {
        AudioBuffer b = tone_sections(48000, 6, { { 10.0, -23.0 } });
        for (std::size_t i = 0; i < b.frames(); ++i) {
            b.at(i, 2) = 0.0f;
            b.at(i, 3) = 0.0f;
            b.at(i, 4) = 0.0f;
            b.at(i, 5) = 0.0f;
        }
        const double lr = measure_loudness(b).integrated;
        for (std::size_t i = 0; i < b.frames(); ++i) b.at(i, 3) = 0.9f * b.at(i, 0);
        const double lfe = measure_loudness(b).integrated;
        for (std::size_t i = 0; i < b.frames(); ++i) {
            b.at(i, 3) = 0.0f;
            b.at(i, 4) = b.at(i, 0);
        }
        const double ls = measure_loudness(b).integrated;
        check(near(lr, -23.0, 0.1) && near(lfe, lr, 1e-4) && near(ls, -23.0 + 10.0 * std::log10(2.0 + 1.41) - 10.0 * std::log10(2.0), 0.1), fmt("5.1 weights: LR %.2f, +LFE %.2f, +Ls %.2f", lr, lfe, ls));
    }
    {
        AudioBuffer b = tone_sections(48000, 2, { { 8.0, -30.0 } });
        normalize_loudness(b, -16.0f, -1.0f);
        const LoudnessResult r = measure_loudness(b);
        check(near(r.integrated, -16.0, 0.05), fmt("normalize_loudness reaches target (%.3f)", r.integrated));
        AudioBuffer c = tone_sections(48000, 2, { { 8.0, -30.0 } });
        normalize_loudness(c, 0.0f, -1.0f);
        const LoudnessResult rc = measure_loudness(c);
        check(near(rc.true_peak_db, -1.0, 0.05) && near(rc.integrated, -1.0, 0.1), fmt("normalize_loudness respects true-peak ceiling (%.3f dBTP)", rc.true_peak_db));
        AudioBuffer d = sine(440.0, 48000, 2, 4800, 0.1);
        normalize_peak(d, -3.0f);
        check(near(peak_db(d), -3.0, 1e-4), "normalize_peak");
    }
    {
        AudioBuffer b = tone_sections(48000, 2, { { 4.0, -23.0 } });
        LoudnessMeter m(48000, 2);
        std::atomic<bool> done(false);
        std::atomic<int> bad(0);
        std::thread reader([&] {
            while (!done.load()) {
                const float v = m.momentary();
                if (!(std::isinf(v) || (v > -40.0f && v < 0.0f))) ++bad;
                std::this_thread::yield();
            }
        });
        for (std::size_t pos = 0; pos < b.frames(); pos += 480) m.push(b.frame(pos), std::min<std::size_t>(480, b.frames() - pos));
        done.store(true);
        reader.join();
        check(bad.load() == 0 && near(m.momentary(), -23.0, 0.1), "live meter readable from another thread while pushing");
    }
}

void test_meters() {
    const std::uint32_t rate = 48000;
    {
        LevelMeter lm;
        lm.prepare(rate, 2);
        AudioBuffer b = sine(1000.0, rate, 2, rate * 3, 0.5);
        for (std::size_t i = 0; i < b.frames(); ++i) b.at(i, 1) *= 0.5f;
        for (std::size_t pos = 0; pos < b.frames(); pos += 512) lm.push(b.frame(pos), std::min<std::size_t>(512, b.frames() - pos));
        check(near(lm.peak_db(0), -6.02, 0.05) && near(lm.peak_db(1), -12.04, 0.05), fmt("level meter peak %.3f / %.3f", lm.peak_db(0), lm.peak_db(1)));
        check(near(lm.rms_db(0), -9.03, 0.1), fmt("level meter rms %.3f", lm.rms_db(0)));
        check(!lm.clipped(0), "no clip");
        lm.set_hold_time(0.5f);
        lm.set_peak_decay(20.0f);
        AudioBuffer silent(2, rate, rate / 4);
        lm.push(silent.data(), silent.frames());
        check(near(lm.hold_db(0), -6.02, 0.05) && lm.peak_db(0) < -10.0f, "peak hold holds while peak decays");
        AudioBuffer more(2, rate, rate / 2);
        lm.push(more.data(), more.frames());
        check(near(lm.hold_db(0), -6.02 - 20.0 * 0.25, 0.1), fmt("hold decays after hold time (%.2f)", lm.hold_db(0)));
        AudioBuffer clip = constant(1.2f, rate, 2, 16);
        lm.push(clip.data(), clip.frames());
        check(lm.clipped(0), "clip indicator");
        lm.clear_clip();
        check(!lm.clipped(0), "clip clear");
    }
    {
        SpectrumAnalyzer sa(4096);
        sa.prepare(rate, 2);
        const double f = 93.0 * rate / 4096.0;
        AudioBuffer b = sine(f, rate, 2, 8192, 0.5);
        sa.push(b.data(), b.frames());
        check(sa.update(), "spectrum update");
        const std::vector<float>& m = sa.magnitudes_db();
        const std::size_t best = static_cast<std::size_t>(std::max_element(m.begin(), m.end()) - m.begin());
        check(best == 93 && near(m[93], -6.02, 0.05), fmt("spectrum peak at bin %.0f, %.3f dB", static_cast<double>(best), m[93]));
        const std::vector<float> bands = sa.bands(32, 20.0f, 20000.0f);
        const std::vector<float> centres = sa.band_frequencies(32, 20.0f, 20000.0f);
        std::size_t bb = static_cast<std::size_t>(std::max_element(bands.begin(), bands.end()) - bands.begin());
        check(centres[bb] / f > 0.75 && centres[bb] / f < 1.33, "log band output peaks around tone");
        sa.set_fft_size(100000);
        check(sa.fft_size() == SpectrumAnalyzer::max_fft_size, "fft size clamped high");
        sa.set_fft_size(10);
        check(sa.fft_size() == SpectrumAnalyzer::min_fft_size, "fft size clamped low");
        sa.set_fft_size(1024);
        sa.set_smoothing(0.0f);
        sa.set_peak_hold(0.0f, 60.0f);
        sa.update();
        AudioBuffer z(2, rate, rate / 2);
        sa.push(z.data(), z.frames());
        sa.update();
        const std::size_t k = static_cast<std::size_t>(std::lround(f * 1024.0 / rate));
        check(sa.magnitudes_db()[k] < -150.0f && near(sa.peaks_db()[k], sa.peaks_db()[k], 0.0) && sa.peaks_db()[k] < -6.0f - 60.0f * 0.5f + 1.0f && sa.peaks_db()[k] > -60.0f, fmt("peak hold decays %.2f", sa.peaks_db()[k]));
        std::atomic<bool> stop(false);
        std::thread producer([&] {
            AudioBuffer s = sine(1000.0, rate, 2, 256, 0.25);
            while (!stop.load()) sa.push(s.data(), s.frames());
        });
        for (int i = 0; i < 200; ++i) sa.update();
        stop.store(true);
        producer.join();
        check(true, "spectrum analyzer concurrent push/update");
    }
}

void test_edit() {
    const std::uint32_t rate = 1000;
    {
        AudioBuffer b = constant(1.0f, rate, 2, 2000);
        fade_in(b, 1.0, FadeCurve::linear);
        check(near(b.at(0, 0), 0.0005, 1e-6) && near(b.at(499, 1), 0.4995, 1e-6) && b.at(1000, 0) == 1.0f, "linear fade in");
        fade_out(b, 0.5, FadeCurve::equal_power);
        check(near(b.at(1999, 0), std::sin(0.5 / 500.0 * k_pi / 2.0), 1e-6) && near(b.at(1750, 0), std::cos(250.5 / 500.0 * k_pi / 2.0), 1e-6), "equal power fade out");
    }
    {
        check(near(fade_shape(FadeCurve::logarithmic, 0.5), std::log10(500.5) / 3.0, 1e-6) && near(fade_shape(FadeCurve::exponential, 0.5), (std::sqrt(1000.0) - 1.0) / 999.0, 1e-6), "log/exp shapes");
        check(near(fade_shape(FadeCurve::s_curve, 0.25), 0.5 - 0.5 * std::cos(k_pi * 0.25), 1e-7) && fade_shape(FadeCurve::s_curve, 0.0) == 0.0f && fade_shape(FadeCurve::s_curve, 1.0) == 1.0f, "s-curve shape");
        bool mono = true;
        for (FadeCurve c : { FadeCurve::linear, FadeCurve::equal_power, FadeCurve::logarithmic, FadeCurve::exponential, FadeCurve::s_curve }) {
            for (int i = 1; i <= 100; ++i) {
                if (fade_shape(c, i / 100.0) < fade_shape(c, (i - 1) / 100.0)) mono = false;
            }
            if (fade_shape(c, 0.0) != 0.0f || std::fabs(fade_shape(c, 1.0) - 1.0f) > 1e-6f) mono = false;
        }
        check(mono, "all fade curves monotone 0..1");
        AudioBuffer a = constant(1.0f, rate, 1, 300);
        AudioBuffer r = a;
        fade_in(a, 0.3, FadeCurve::exponential);
        fade_out(r, 0.3, FadeCurve::exponential);
        reverse(r);
        bool same = true;
        for (std::size_t i = 0; i < 300; ++i) same = same && near(a.at(i, 0), r.at(i, 0), 1e-6);
        check(same, "fade_out is the mirror of fade_in");
        AudioBuffer g = constant(1.0f, rate, 1, 100);
        fade(g, 10, 20, 0.5f, 2.0f, FadeCurve::linear);
        check(g.at(9, 0) == 1.0f && near(g.at(10, 0), 0.5 + 1.5 * 0.025, 1e-6) && near(g.at(29, 0), 0.5 + 1.5 * 0.975, 1e-6) && g.at(30, 0) == 1.0f, "fade between arbitrary gains");
    }
    {
        AudioBuffer a = constant(1.0f, rate, 2, 1500);
        AudioBuffer b = constant(1.0f, rate, 2, 800);
        AudioBuffer x = crossfade(a, b, 0.5, FadeCurve::linear);
        double worst = 0.0;
        for (std::size_t i = 0; i < x.frames(); ++i) worst = std::max(worst, std::fabs(x.at(i, 0) - 1.0));
        check(x.frames() == 1500 + 800 - 500 && worst < 1e-6, "linear crossfade length and constant gain");
        AudioBuffer p = sine(37.0, rate, 1, 1000, 1.0);
        AudioBuffer q = sine(91.0, rate, 1, 1000, 1.0, 1.0);
        AudioBuffer y = crossfade(p, q, 0.25, FadeCurve::equal_power);
        double sum = 0.0;
        for (std::size_t i = 0; i < 250; ++i) {
            const double t = (i + 0.5) / 250.0;
            const double gi = std::sin(t * k_pi / 2.0);
            const double go = std::cos(t * k_pi / 2.0);
            sum = std::max(sum, std::fabs(gi * gi + go * go - 1.0));
            sum = std::max(sum, std::fabs(y.at(750 + i, 0) - (p.at(750 + i, 0) * go + q.at(i, 0) * gi)));
        }
        check(y.frames() == 1750 && sum < 1e-5, "equal power crossfade is power complementary");
        AudioBuffer m = crossfade(a, AudioBuffer(1, rate, 100), 10.0, FadeCurve::linear);
        check(m.frames() == 1500 && m.channels() == 2, "crossfade clamps overlap and converts channels");
    }
    {
        AudioBuffer b = constant(1.0f, rate, 1, 3000);
        apply_gain_envelope(b, { { 2.0, -20.0 }, { 0.0, -6.0 }, { 1.0, 0.0 } });
        check(near(b.at(0, 0), std::pow(10.0, -6.0 / 20.0), 1e-5) && near(b.at(500, 0), std::pow(10.0, -3.0 / 20.0), 1e-5) && near(b.at(1500, 0), std::pow(10.0, -10.0 / 20.0), 1e-5) && near(b.at(2999, 0), 0.1, 1e-6), "gain envelope interpolates in dB, sorts, holds ends");
    }
    {
        AudioBuffer b(2, rate, 4);
        for (std::size_t i = 0; i < 4; ++i) {
            b.at(i, 0) = static_cast<float>(i);
            b.at(i, 1) = 10.0f + static_cast<float>(i);
        }
        reverse(b);
        check(b.at(0, 0) == 3.0f && b.at(0, 1) == 13.0f && b.at(3, 0) == 0.0f, "reverse");
        invert_polarity(b);
        check(b.at(0, 0) == -3.0f, "invert polarity");
        remove_dc(b);
        check(near(b.at(0, 0) + b.at(1, 0) + b.at(2, 0) + b.at(3, 0), 0.0, 1e-6) && near(b.at(0, 1), -1.5, 1e-6), "remove dc");
        swap_channels(b);
        check(near(b.at(0, 0), -1.5, 1e-6), "swap channels");
        AudioBuffer mono = stereo_to_mono(b);
        check(mono.channels() == 1 && near(mono.at(0, 0), (b.at(0, 0) + b.at(0, 1)) / 2.0, 1e-6), "stereo to mono averages");
        AudioBuffer st = mono_to_stereo(mono);
        check(st.channels() == 2 && st.at(2, 0) == mono.at(2, 0) && st.at(2, 1) == mono.at(2, 0), "mono to stereo duplicates");
        pad(st, 0.002, 0.003);
        check(st.frames() == 9 && st.at(0, 0) == 0.0f && st.at(2, 1) == mono.at(0, 0) && st.at(8, 1) == 0.0f, "pad");
        AudioBuffer mx = mix(constant(0.5f, rate, 2, 3), constant(1.0f, rate, 2, 5), 0.25f);
        check(mx.frames() == 5 && near(mx.at(0, 0), 0.75, 1e-6) && near(mx.at(4, 1), 0.25, 1e-6), "mix extends and scales");
        AudioBuffer mm = mix(constant(0.5f, rate, 2, 3), constant(1.0f, rate, 1, 3), 1.0f);
        check(mm.channels() == 2 && mm.at(1, 1) > 0.5f, "mix converts channels");
    }
    {
        AudioBuffer b(2, rate);
        auto add = [&](double seconds, float v) {
            const std::size_t start = b.frames();
            const std::size_t n = static_cast<std::size_t>(std::llround(seconds * rate));
            b.resize(start + n);
            for (std::size_t i = 0; i < n; ++i) {
                b.at(start + i, 0) = v * ((i / 3) % 2 ? 1.0f : -1.0f);
                b.at(start + i, 1) = 0.0f;
            }
        };
        add(0.5, 0.0f);
        add(1.0, 0.5f);
        add(1.0, 0.0001f);
        add(0.5, 0.5f);
        add(0.05, 0.0f);
        add(0.5, 0.5f);
        add(0.7, 0.0f);
        const std::vector<FrameRange> s = detect_silence(b, -60.0f, 0.1);
        check(s.size() == 3 && s[0] == FrameRange(0, 500) && s[1] == FrameRange(1500, 2500) && s[2] == FrameRange(3550, 4250), "detect silence with minimum duration");
        const std::vector<AudioBuffer> parts = split_on_silence(b, -60.0f, 0.3, 10.0);
        check(parts.size() == 2 && parts[0].frames() == 1000 + 20 && parts[1].frames() == 1050 + 20, "split on silence with keep margin");
        AudioBuffer t = b;
        const std::vector<FrameRange> removed = trim_silence(t, -60.0f, 10.0);
        check(removed.size() == 2 && removed[0] == FrameRange(0, 490) && removed[1] == FrameRange(3560, 4250) && t.frames() == 3560 - 490, "trim silence returns removed ranges");
        AudioBuffer z(1, rate, 100);
        const std::vector<FrameRange> all = trim_silence(z);
        check(all.size() == 1 && all[0] == FrameRange(0, 100) && z.frames() == 0, "trim all-silent buffer");
    }
}

void test_misc() {
    Filter f;
    f.prepare(48000, 2);
    check(f.tail_frames() > 0 && f.tail_frames() < 48000, "filter reports finite tail");
    check(f.parameter("slope") == &f.slope && f.set("q", 2.0f) && near(f.q.get(), 2.0, 1e-6), "filter parameters by name");
    f.set("type", 99.0f);
    check(f.filter_type() == FilterType::allpass, "type parameter clamps");
    f.set_type(FilterType::peak);
    f.slope.set(48.0f);
    check(f.stages() == 1, "shelf/peak types are not cascaded");
    f.set_type(FilterType::bandpass);
    check(f.stages() == 4 && f.target_coefficients().size() == 4, "bandpass cascades");
    EffectChain chain;
    chain.add(std::make_shared<Compressor>());
    chain.add(std::make_shared<Limiter>(-1.0f, 50.0f, 2.0f));
    chain.add(std::make_shared<ParametricEq>());
    chain.prepare(48000, 2);
    check(chain.latency_frames() == 96 - 1, "effect chain sums limiter latency");
    AudioBuffer b = nasty_signal(48000, 48000);
    chain.process(b.data(), b.frames());
    bool finite = true;
    for (float s : b.samples()) finite = finite && std::isfinite(s);
    check(finite, "chain output finite on nasty signal");
    AudioBuffer d = sine(1000.0, 48000, 2, 4800, 1e-30);
    Filter tiny(FilterType::lowpass, 20.0f, 30.0f);
    apply(tiny, d);
    finite = true;
    for (float s : d.samples()) finite = finite && std::isfinite(s);
    check(finite, "denormal-range input stays finite");
}

} // namespace

int main() {
    test_biquad_design();
    test_filter_measured();
    test_cascaded_slopes();
    test_eq_response();
    test_no_clicks();
    test_compressor_static();
    test_compressor_timing();
    test_limiter();
    test_gate_and_expander();
    test_loudness();
    test_meters();
    test_edit();
    test_misc();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
