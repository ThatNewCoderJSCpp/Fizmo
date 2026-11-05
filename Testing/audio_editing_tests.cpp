#include "../Source/Audio/audio.hpp"
#include "../Source/Audio/noise.hpp"
#include "../Source/Audio/timeline.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

const double k_pi = 3.14159265358979323846;

std::string fmt(double v, int digits = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, v);
    return buf;
}

std::string temp_path(const std::string& name) {
    const char* dir = std::getenv("TMPDIR");
#if defined(_WIN32)
    if (!dir) dir = std::getenv("TEMP");
#endif
    std::string base = dir ? dir : "/tmp";
    return base + "/" + name;
}

AudioBuffer speech_like(std::uint32_t rate, std::size_t channels, double seconds, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    const std::size_t n = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(channels, rate, n);
    std::size_t i = 0;
    double phase = 0.0;
    while (i < n) {
        i += static_cast<std::size_t>((0.05 + 0.12 * uni(rng)) * rate);
        if (i >= n) break;
        const std::size_t len = static_cast<std::size_t>((0.15 + 0.2 * uni(rng)) * rate);
        const double f0 = 100.0 + 120.0 * uni(rng);
        const double glide = (uni(rng) - 0.5) * 60.0;
        const double f1 = 400.0 + 500.0 * uni(rng);
        const double f2 = 1100.0 + 1300.0 * uni(rng);
        const double amp = 0.15 + 0.25 * uni(rng);
        for (std::size_t k = 0; k < len && i + k < n; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(len);
            double env = std::sin(k_pi * t);
            env = env * env * amp;
            const double f = f0 + glide * t + 3.0 * std::sin(2.0 * k_pi * 5.0 * static_cast<double>(k) / rate);
            phase += 2.0 * k_pi * f / rate;
            double v = 0.0;
            for (int h = 1; h * f < rate * 0.45 && h < 40; ++h) {
                const double hf = h * f;
                const double a = 1.0 / h * (1.0 / (1.0 + std::pow((hf - f1) / 150.0, 2.0)) + 0.6 / (1.0 + std::pow((hf - f2) / 250.0, 2.0)) + 0.05);
                v += a * std::sin(h * phase);
            }
            for (std::size_t c = 0; c < channels; ++c) b.at(i + k, c) = static_cast<float>(env * v * (c ? 0.9 : 1.0));
        }
        i += len;
    }
    return b;
}

AudioBuffer make_noise(std::uint32_t rate, std::size_t channels, double seconds, bool pink, double rms, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> gauss(0.0f, 1.0f);
    const std::size_t n = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(channels, rate, n);
    for (std::size_t c = 0; c < channels; ++c) {
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const float w = gauss(rng);
            float v = w;
            if (pink) {
                b0 = 0.99886 * b0 + w * 0.0555179;
                b1 = 0.99332 * b1 + w * 0.0750759;
                b2 = 0.96900 * b2 + w * 0.1538520;
                b3 = 0.86650 * b3 + w * 0.3104856;
                b4 = 0.55000 * b4 + w * 0.5329522;
                b5 = -0.7616 * b5 - w * 0.0168980;
                v = static_cast<float>(b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362);
                b6 = w * 0.115926;
            }
            b.at(i, c) = v;
        }
    }
    double e = 0.0;
    for (float v : b.samples()) e += static_cast<double>(v) * v;
    b.apply_gain(static_cast<float>(rms / std::sqrt(e / static_cast<double>(b.sample_count()))));
    return b;
}

double snr_db(const AudioBuffer& got, const AudioBuffer& want, std::size_t skip_frames = 0) {
    double s = 0.0;
    double e = 0.0;
    const std::size_t n = std::min(got.sample_count(), want.sample_count());
    for (std::size_t i = skip_frames * want.channels(); i < n; ++i) {
        s += static_cast<double>(want.data()[i]) * want.data()[i];
        const double d = static_cast<double>(got.data()[i]) - want.data()[i];
        e += d * d;
    }
    if (e <= 0.0) return 300.0;
    return 10.0 * std::log10(s / e);
}

double tone_level(const AudioBuffer& b, std::size_t channel, std::size_t from, std::size_t count, double freq) {
    const double w = 2.0 * k_pi * freq / b.sample_rate();
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = from; i < from + count && i < b.frames(); ++i) {
        const double s = b.at(i, channel) + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return 2.0 * std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / static_cast<double>(count);
}

class Latent : public Effect {
public:
    explicit Latent(std::size_t delay) : m_delay(delay) {}
    const char* name() const noexcept override { return "latent"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override {
        set_format(sample_rate, channels);
        m_ring.assign((m_delay + 1) * channels, 0.0f);
        m_pos = 0;
    }
    void process(float* interleaved, std::size_t frames) noexcept override {
        const std::size_t c = channels();
        const std::size_t len = m_delay + 1;
        for (std::size_t f = 0; f < frames; ++f) {
            float* io = interleaved + f * c;
            for (std::size_t ch = 0; ch < c; ++ch) m_ring[m_pos * c + ch] = io[ch];
            const std::size_t rd = (m_pos + 1) % len;
            for (std::size_t ch = 0; ch < c; ++ch) io[ch] = m_ring[rd * c + ch];
            m_pos = rd;
        }
    }
    void reset() noexcept override {
        std::fill(m_ring.begin(), m_ring.end(), 0.0f);
        m_pos = 0;
    }
    std::size_t latency_frames() const noexcept override { return m_delay; }

private:
    std::size_t m_delay;
    std::vector<float> m_ring;
    std::size_t m_pos = 0;
};

class Scale : public Effect {
public:
    Scale() : amount("amount", 1.0f, 0.0f, 4.0f) { add_parameter(amount); }
    const char* name() const noexcept override { return "scale"; }
    void prepare(std::uint32_t sample_rate, std::size_t channels) override { set_format(sample_rate, channels); }
    void process(float* interleaved, std::size_t frames) noexcept override {
        const float a = amount.get();
        for (std::size_t i = 0; i < frames * channels(); ++i) interleaved[i] *= a;
    }
    void reset() noexcept override {}

    Parameter amount;
};

std::shared_ptr<const AudioBuffer> constant(std::uint32_t rate, std::size_t channels, std::size_t frames, float value) {
    AudioBuffer b(channels, rate, frames);
    for (float& v : b.samples()) v = value;
    return std::make_shared<const AudioBuffer>(std::move(b));
}

std::shared_ptr<const AudioBuffer> impulse(std::uint32_t rate, std::size_t channels, std::size_t frames, std::size_t at, float value = 1.0f) {
    AudioBuffer b(channels, rate, frames);
    for (std::size_t c = 0; c < channels; ++c) b.at(at, c) = value;
    return std::make_shared<const AudioBuffer>(std::move(b));
}

void test_noise_reduction() {
    const std::uint32_t rate = 48000;
    for (std::size_t ch = 1; ch <= 2; ++ch) {
        for (int pink = 0; pink < 2; ++pink) {
            const AudioBuffer clean = speech_like(rate, ch, 8.0, 1);
            const double level = clean.rms();
            const AudioBuffer noise = make_noise(rate, ch, 8.0, pink != 0, level, 2);
            const AudioBuffer profile = make_noise(rate, ch, 2.0, pink != 0, level, 3);
            AudioBuffer noisy = clean;
            noisy.mix_in(noise);
            const double in = snr_db(noisy, clean, rate);
            const std::string label = std::string(ch == 1 ? "mono " : "stereo ") + (pink ? "pink" : "white");
            NoiseReduction learned;
            learned.reduction_db.set(30.0f);
            learned.prepare(rate, ch);
            check(learned.learn_profile(profile) && learned.has_profile(), "learn a noise profile (" + label + ")");
            learned.mode.set(1.0f);
            const AudioBuffer out_l = applied(learned, noisy, false);
            const double gain_l = snr_db(out_l, clean, rate) - in;
            std::printf("  noise reduction %s, input SNR %s dB: learned +%s dB", label.c_str(), fmt(in).c_str(), fmt(gain_l).c_str());
            check(out_l.frames() == noisy.frames(), "noise reduction keeps the length (" + label + ")");
            check(gain_l >= 10.0, "learned-profile noise reduction improves SNR by >= 10 dB (" + label + ", got " + fmt(gain_l) + ")");
            NoiseReduction adaptive;
            adaptive.reduction_db.set(30.0f);
            const AudioBuffer out_a = applied(adaptive, noisy, false);
            const double gain_a = snr_db(out_a, clean, rate) - in;
            std::printf(", adaptive +%s dB\n", fmt(gain_a).c_str());
            check(gain_a >= 6.0, "adaptive noise reduction improves SNR by >= 6 dB (" + label + ", got " + fmt(gain_a) + ")");
        }
    }
    const AudioBuffer clean = speech_like(rate, 2, 6.0, 7);
    NoiseReduction adaptive;
    const AudioBuffer out = applied(adaptive, clean, false);
    const double att = 20.0 * std::log10(out.rms() / clean.rms());
    check(std::fabs(att) <= 1.0, "adaptive mode leaves a clean signal within 1 dB (" + fmt(att, 3) + " dB)");
    NoiseReduction learned;
    learned.learn_profile(make_noise(rate, 2, 1.0, true, clean.rms() * 0.001, 4));
    learned.mode.set(1.0f);
    const AudioBuffer out2 = applied(learned, clean, false);
    const double att2 = 20.0 * std::log10(out2.rms() / clean.rms());
    check(std::fabs(att2) <= 1.0, "learned mode with a -60 dB noise floor leaves a clean signal within 1 dB (" + fmt(att2, 3) + " dB)");
    std::printf("  clean signal level change: adaptive %s dB, learned %s dB\n", fmt(att, 3).c_str(), fmt(att2, 3).c_str());

    NoiseReduction transparent;
    transparent.reduction_db.set(0.0f);
    transparent.prepare(rate, 2);
    check(transparent.latency_frames() == 2047 && transparent.fft_size() == 2048 && transparent.hop_size() == 512, "48 kHz uses a 2048-point FFT with 75% overlap and 2047 frames of latency");
    AudioBuffer probe = make_noise(rate, 2, 0.5, false, 0.1, 5);
    AudioBuffer passed = probe;
    passed.resize(probe.frames() + transparent.latency_frames());
    transparent.reset();
    for (std::size_t pos = 0; pos < passed.frames(); pos += 333) transparent.process(passed.frame(pos), std::min<std::size_t>(333, passed.frames() - pos));
    double worst = 0.0;
    for (std::size_t i = 0; i < probe.frames(); ++i) {
        for (std::size_t c = 0; c < 2; ++c) worst = std::max(worst, static_cast<double>(std::fabs(passed.at(i + 2047, c) - probe.at(i, c))));
    }
    check(worst < 1e-4, "with 0 dB of reduction the STFT reconstructs the input exactly, delayed by latency_frames() (" + std::to_string(worst) + ")");
    NoiseReduction small_rate;
    small_rate.prepare(16000, 1);
    check(small_rate.fft_size() == 512 && small_rate.latency_frames() == 511, "16 kHz picks a 512-point FFT");

    AudioBuffer stereo_clean = speech_like(rate, 2, 6.0, 11);
    AudioBuffer left_noise = make_noise(rate, 1, 6.0, false, stereo_clean.rms(), 12);
    AudioBuffer stereo_noisy = stereo_clean;
    for (std::size_t i = 0; i < stereo_noisy.frames(); ++i) stereo_noisy.at(i, 0) += left_noise.at(i, 0);
    NoiseReduction shared;
    shared.reduction_db.set(30.0f);
    NoiseReduction separate;
    separate.reduction_db.set(30.0f);
    separate.per_channel.set(1.0f);
    const AudioBuffer out_shared = applied(shared, stereo_noisy, false);
    const AudioBuffer out_sep = applied(separate, stereo_noisy, false);
    AudioBuffer right_clean(1, rate, stereo_clean.frames());
    AudioBuffer right_shared(1, rate, stereo_clean.frames());
    AudioBuffer right_sep(1, rate, stereo_clean.frames());
    for (std::size_t i = 0; i < stereo_clean.frames(); ++i) {
        right_clean.at(i, 0) = stereo_clean.at(i, 1);
        right_shared.at(i, 0) = out_shared.at(i, 1);
        right_sep.at(i, 0) = out_sep.at(i, 1);
    }
    check(snr_db(right_sep, right_clean, rate) > snr_db(right_shared, right_clean, rate) + 3.0, "per-channel masks keep a clean right channel cleaner than a shared mask");

    AudioBuffer offline_noise = make_noise(rate, 1, 1.5, false, 0.05, 21);
    AudioBuffer offline_in = speech_like(rate, 1, 5.0, 22);
    const AudioBuffer offline_clean = offline_in;
    offline_in.mix_in(make_noise(rate, 1, 5.0, false, 0.05, 23));
    const AudioBuffer offline_out = reduce_noise(offline_in, &offline_noise, 24.0f);
    check(offline_out.frames() == offline_in.frames() && snr_db(offline_out, offline_clean, rate) > snr_db(offline_in, offline_clean, rate) + 8.0, "reduce_noise() with a noise sample cleans a recording offline");
    const AudioBuffer offline_adaptive = reduce_noise(offline_in, nullptr, 24.0f);
    check(snr_db(offline_adaptive, offline_clean, rate) > snr_db(offline_in, offline_clean, rate) + 5.0, "reduce_noise() without a noise sample adapts on its own");

    NoiseReduction live;
    live.mode.set(1.0f);
    live.prepare(rate, 2);
    std::atomic<bool> run{ true };
    std::thread audio([&] {
        std::vector<float> block(256 * 2, 0.0f);
        std::mt19937 rng(3);
        std::normal_distribution<float> gauss(0.0f, 0.1f);
        while (run.load()) {
            for (float& v : block) v = gauss(rng);
            live.process(block.data(), 256);
        }
    });
    for (int i = 0; i < 40; ++i) {
        live.learn_profile(make_noise(rate, 2, 0.1, (i % 2) != 0, 0.1, static_cast<unsigned>(30 + i)));
        if (i % 7 == 0) live.clear_profile();
        live.reduction_db.set(static_cast<float>(i % 30));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    run.store(false);
    audio.join();
    check(true, "learning profiles while the audio thread runs doesn't crash");

    AudioBuffer bench = make_noise(rate, 2, 10.0, true, 0.1, 40);
    NoiseReduction timed;
    timed.prepare(rate, 2);
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t pos = 0; pos < bench.frames(); pos += 480) timed.process(bench.frame(pos), std::min<std::size_t>(480, bench.frames() - pos));
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  NoiseReduction 48 kHz stereo: %.4f x real time (%.2f ms per second of audio), latency %zu frames (%.1f ms)\n", secs / 10.0, secs * 100.0, timed.latency_frames(), 1000.0 * static_cast<double>(timed.latency_frames()) / rate);
    check(secs / 10.0 < 1.0, "noise reduction runs faster than real time");
}

void test_hum_remover() {
    const std::uint32_t rate = 48000;
    const std::size_t n = rate * 4;
    for (double base : { 50.0, 60.0 }) {
        AudioBuffer x(2, rate, n);
        for (std::size_t i = 0; i < n; ++i) {
            const double t = static_cast<double>(i) / rate;
            double v = 0.2 * std::sin(2.0 * k_pi * 730.0 * t);
            for (int h = 1; h <= 6; ++h) v += 0.1 / h * std::sin(2.0 * k_pi * base * h * t + h);
            x.at(i, 0) = static_cast<float>(v);
            x.at(i, 1) = static_cast<float>(-v);
        }
        HumRemover hum;
        const AudioBuffer y = applied(hum, x, false);
        check(hum.detected_frequency() == static_cast<float>(base), "hum remover detects " + fmt(base, 0) + " Hz mains");
        double worst = -300.0;
        for (int h = 1; h <= 6; ++h) {
            const double before = tone_level(x, 0, 2 * rate, rate, base * h);
            const double after = tone_level(y, 0, 2 * rate, rate, base * h);
            worst = std::max(worst, 20.0 * std::log10(after / before));
        }
        check(worst < -40.0, "hum and its harmonics drop by more than 40 dB at " + fmt(base, 0) + " Hz (worst " + fmt(worst) + " dB)");
        const double tone = 20.0 * std::log10(tone_level(y, 1, 2 * rate, rate, 730.0) / tone_level(x, 1, 2 * rate, rate, 730.0));
        check(std::fabs(tone) < 0.2, "music at 730 Hz passes the hum remover untouched (" + fmt(tone, 3) + " dB)");
    }
    AudioBuffer quiet(1, rate, rate * 2);
    for (std::size_t i = 0; i < quiet.frames(); ++i) quiet.at(i, 0) = static_cast<float>(0.3 * std::sin(2.0 * k_pi * 440.0 * i / rate));
    HumRemover none;
    const AudioBuffer same = applied(none, quiet, false);
    check(none.detected_frequency() == 0.0f && snr_db(same, quiet) > 100.0, "without hum the auto mode leaves audio alone");
    HumRemover manual(60.0f);
    AudioBuffer m(1, rate, rate * 2);
    for (std::size_t i = 0; i < m.frames(); ++i) m.at(i, 0) = static_cast<float>(0.2 * std::sin(2.0 * k_pi * 180.0 * i / rate));
    const AudioBuffer mo = applied(manual, m, false);
    check(manual.active_frequency() == 60.0f && tone_level(mo, 0, rate, rate, 180.0) < 0.002, "a manual 60 Hz setting notches the 3rd harmonic");
}

void test_click_remover() {
    const std::uint32_t rate = 48000;
    const AudioBuffer clean = speech_like(rate, 2, 6.0, 5);
    AudioBuffer clicky = clean;
    std::mt19937 rng(9);
    std::uniform_int_distribution<std::size_t> where(rate / 4, clean.frames() - 100);
    for (int k = 0; k < 40; ++k) {
        const std::size_t p = where(rng);
        const int width = 1 + k % 3;
        for (int j = 0; j < width; ++j) clicky.at(p + static_cast<std::size_t>(j), static_cast<std::size_t>(k % 2)) += (k % 4 < 2 ? 0.6f : -0.5f) * (1.0f - 0.2f * static_cast<float>(j));
    }
    ClickRemover remover;
    const AudioBuffer fixed = applied(remover, clicky, false);
    const double before = snr_db(clicky, clean);
    const double after = snr_db(fixed, clean);
    std::printf("  click remover: SNR %s dB -> %s dB, %llu repairs\n", fmt(before).c_str(), fmt(after).c_str(), static_cast<unsigned long long>(remover.clicks_repaired()));
    check(after > before + 20.0, "click remover repairs impulsive clicks (" + fmt(before) + " -> " + fmt(after) + " dB)");
    check(remover.latency_frames() == 1023, "click remover reports its latency");
    ClickRemover gentle;
    const AudioBuffer untouched = applied(gentle, clean, false);
    check(snr_db(untouched, clean) > 50.0, "click remover leaves a clean signal essentially unchanged (" + fmt(snr_db(untouched, clean)) + " dB)");
}

Timeline basic_timeline() {
    return Timeline(48000, 2);
}

void test_clips_and_fades() {
    Timeline tl = basic_timeline();
    Track& t = tl.add_track("drums");
    t.add_clip(make_clip(impulse(48000, 1, 1000, 10), 1.0, "hit"));
    RenderOptions opt;
    AudioBuffer out = render(tl, 0.0, 1.1, opt);
    std::size_t nonzero = 0;
    std::size_t where = 0;
    for (std::size_t i = 0; i < out.frames(); ++i) {
        if (out.at(i, 0) != 0.0f) {
            ++nonzero;
            where = i;
        }
    }
    check(out.frames() == 52800 && nonzero == 1 && where == 48010 && out.at(48010, 0) == 1.0f && out.at(48010, 1) == 1.0f, "a clip lands on the exact sample (" + std::to_string(where) + ")");

    Clip offset = make_clip(impulse(48000, 1, 1000, 500), 0.5);
    offset.source_start = 400.0 / 48000.0;
    offset.length = 600.0 / 48000.0;
    Timeline tl2 = basic_timeline();
    tl2.add_track().add_clip(offset);
    AudioBuffer out2 = render(tl2, 0.0, 0.6);
    check(out2.at(24100, 0) == 1.0f && out2.peak() == 1.0f, "source_start offsets into the source sample-accurately");
    AudioBuffer late = render(tl2, 0.5, 0.6);
    check(late.frames() == 4800 && late.at(100, 0) == 1.0f, "rendering from a later start lines up");

    Timeline tl3 = basic_timeline();
    Track& a = tl3.add_track();
    a.add_clip(make_clip(constant(48000, 2, 4800, 0.25f), 0.0));
    a.add_clip(make_clip(constant(48000, 2, 4800, 0.5f), 0.05));
    tl3.add_track().add_clip(make_clip(constant(48000, 1, 4800, 0.125f), 0.05));
    AudioBuffer out3 = render(tl3, 0.0, 0.2);
    check(out3.at(100, 0) == 0.25f && out3.at(2400, 1) == 0.875f && out3.at(5000, 0) == 0.625f && out3.at(7300, 0) == 0.0f, "overlapping clips on a track and across tracks sum");

    Timeline tl4 = basic_timeline();
    Clip faded = make_clip(constant(48000, 1, 48000, 1.0f), 0.0);
    faded.fade_in = 0.1;
    faded.fade_out = 0.2;
    faded.fade_out_shape = FadeShape::equal_power;
    tl4.add_track().add_clip(faded);
    AudioBuffer out4 = render(tl4, 0.0, 1.0);
    check(out4.at(0, 0) == 0.0f && std::fabs(out4.at(2400, 0) - 0.5f) < 1e-6f && out4.at(4800, 0) == 1.0f, "linear fade in hits 0, 0.5 and 1 at the right samples");
    const float eq = out4.at(47999 - 4800, 0);
    check(std::fabs(eq - static_cast<float>(std::sin(0.25 * k_pi))) < 1e-5f && out4.at(47999, 0) == 0.0f, "equal-power fade out is -3 dB halfway and reaches silence on the last sample");
    check(fade_gain(FadeShape::s_curve, 0.5) == 0.5f && fade_gain(FadeShape::exponential, 0.5) < 0.2f && fade_gain(FadeShape::logarithmic, 0.5) > 0.8f, "fade shapes have the expected curvature");
    Clip g = faded;
    g.gain_db = -6.0f;
    check(std::fabs(g.gain_at(0.5) - db_to_gain(-6.0f)) < 1e-6f && std::fabs(g.gain_at(0.05) - 0.5f * db_to_gain(-6.0f)) < 1e-4f, "Clip::gain_at reports clip gain with fades");

    Clip c = make_clip(constant(48000, 1, 48000, 1.0f), 2.0, "take");
    c.fade_in = 0.1;
    c.fade_out = 0.1;
    Clip right;
    check(c.split(2.4, right) && std::fabs(c.length - 0.4) < 1e-12 && std::fabs(right.position - 2.4) < 1e-12 && std::fabs(right.source_start - 0.4) < 1e-12 && std::fabs(right.length - 0.6) < 1e-12 && c.fade_out == 0.0 && right.fade_in == 0.0 && right.fade_out == 0.1, "split divides a clip at a time and keeps the outer fades");
    check(!c.split(5.0, right), "split outside the clip fails");
    Clip trim = make_clip(constant(48000, 1, 48000, 1.0f), 1.0);
    trim.trim_start(1.25);
    check(std::fabs(trim.position - 1.25) < 1e-12 && std::fabs(trim.source_start - 0.25) < 1e-12 && std::fabs(trim.length - 0.75) < 1e-12, "trim_start moves the left edge and keeps the audio in place");
    trim.trim_start(0.0);
    check(std::fabs(trim.position - 1.0) < 1e-12 && trim.source_start == 0.0, "trim_start can't reveal audio before the source begins");
    trim.trim_end(5.0);
    check(std::fabs(trim.length - 1.0) < 1e-12, "trim_end can't extend past the source");
    trim.trim_end(1.5);
    check(std::fabs(trim.length - 0.5) < 1e-12, "trim_end shortens the clip");
    Clip dup = trim.duplicate(3.0);
    dup.move_to(4.0);
    check(dup.position == 4.0 && dup.source == trim.source && dup.length == trim.length, "duplicate and move_to");
    Track tr;
    tr.add_clip(make_clip(constant(48000, 1, 48000, 1.0f), 0.0));
    check(tr.split_clip(0, 0.25) && tr.clips.size() == 2 && tr.clips_at(0.5).size() == 1 && tr.clips_at(0.5)[0] == 1 && tr.length() == 1.0, "Track::split_clip and clips_at");
    Timeline tl5 = basic_timeline();
    Track& split_track = tl5.add_track();
    split_track.add_clip(make_clip(speech_like(48000, 2, 1.0, 3), 0.0));
    const AudioBuffer whole = render(tl5, 0.0, 1.0);
    split_track.split_clip(0, 0.3337);
    const AudioBuffer halves = render(tl5, 0.0, 1.0);
    check(snr_db(halves, whole) > 200.0, "splitting a clip doesn't change the rendered audio");
}

void test_automation() {
    AutomationLane lane(AutomationTarget::volume);
    check(lane.value_at(1.0) == lane.default_value, "an empty lane returns its default");
    lane.add(1.0, 0.0f, Curve::linear);
    lane.add(2.0, 10.0f, Curve::hold);
    lane.add(3.0, 20.0f, Curve::s_curve);
    lane.add(4.0, 40.0f, Curve::bezier, 0.0f);
    lane.add(5.0, 0.0f);
    check(lane.value_at(0.0) == 0.0f && lane.value_at(1.0) == 0.0f && lane.value_at(1.5) == 5.0f && lane.value_at(1.25) == 2.5f, "linear segments interpolate exactly");
    check(lane.value_at(2.0) == 10.0f && lane.value_at(2.999) == 10.0f && lane.value_at(3.0) == 20.0f, "hold segments step at the next point");
    check(std::fabs(lane.value_at(3.5) - 30.0f) < 1e-5f && lane.value_at(3.25) < 25.0f, "s-curve segments ease in and out");
    check(std::fabs(lane.value_at(4.5) - 20.0f) < 1e-4f && lane.value_at(6.0) == 0.0f, "zero-tension bezier is linear and the last value holds");
    AutomationLane exp_lane;
    exp_lane.add(0.0, 100.0f, Curve::exponential);
    exp_lane.add(1.0, 10000.0f);
    check(std::fabs(exp_lane.value_at(0.5) - 1000.0f) < 0.1f, "exponential segments between positive values are geometric");
    AutomationLane bez;
    bez.add(0.0, 0.0f, Curve::bezier, 0.8f);
    bez.add(1.0, 1.0f);
    AutomationLane bez2;
    bez2.add(0.0, 0.0f, Curve::bezier, -0.8f);
    bez2.add(1.0, 1.0f);
    bool monotone = true;
    float prev = -1.0f;
    for (int i = 0; i <= 100; ++i) {
        const float v = bez.value_at(i / 100.0);
        monotone = monotone && v >= prev;
        prev = v;
    }
    check(monotone && bez.value_at(0.5) > 0.7f && bez2.value_at(0.5) < 0.3f, "bezier tension bends the curve and stays monotone");
    AutomationLane jump;
    jump.add(1.0, 0.0f);
    jump.add(1.0, 1.0f);
    jump.add(2.0, 1.0f);
    check(jump.value_at(0.999) == 0.0f && jump.value_at(1.0) == 1.0f, "two points at the same time make a jump");
    const std::size_t moved = lane.move(0, 6.0, 7.0f);
    check(moved == lane.size() - 1 && lane.point(moved).time == 6.0 && lane.value_at(5.5) == 3.5f, "moving a point re-sorts the lane");
    check(lane.remove(moved) && lane.size() == 4, "removing a point");
    lane.remove_range(2.5, 3.5);
    check(lane.size() == 3, "remove_range");
    std::vector<float> rendered(48000);
    AutomationLane ramp;
    ramp.add(0.0, 0.0f);
    ramp.add(1.0, 1.0f);
    ramp.render_frames(0, 48000.0, rendered.data(), rendered.size());
    bool exact = true;
    for (std::size_t i = 0; i < rendered.size(); i += 997) exact = exact && rendered[i] == ramp.value_at(static_cast<double>(i) / 48000.0);
    std::vector<float> rendered2(100);
    ramp.render(0.25, 48000.0, rendered2.data(), rendered2.size());
    check(exact && std::fabs(rendered2[0] - 0.25f) < 1e-6f && std::fabs(rendered2[48] - (0.25f + 0.001f)) < 1e-6f, "rendering a lane per sample matches value_at");
    float backwards = 0.0f;
    for (int i = 100; i >= 0; --i) backwards += ramp.value_at(i / 100.0);
    check(std::fabs(backwards - 50.5f) < 1e-3f, "value_at is right for non-sequential lookups too");

    Timeline tl = basic_timeline();
    Track& t = tl.add_track();
    t.add_clip(make_clip(constant(48000, 2, 48000, 1.0f), 0.0));
    AutomationLane& vol = t.lane(AutomationTarget::volume);
    vol.add(0.0, -120.0f);
    vol.add(0.5, 0.0f, Curve::s_curve);
    vol.add(1.0, -20.0f);
    AudioBuffer out = render(tl, 0.0, 1.0);
    bool per_sample = true;
    for (std::size_t i = 0; i < out.frames(); ++i) {
        const float db = vol.value_at(static_cast<double>(i) / 48000.0);
        const float want = db <= -120.0f ? 0.0f : db_to_gain(db);
        if (out.at(i, 0) != want || out.at(i, 1) != want) {
            per_sample = false;
            break;
        }
    }
    check(per_sample, "volume automation is applied per sample");

    Timeline tp = basic_timeline();
    Track& pt = tp.add_track("mono", 1);
    pt.add_clip(make_clip(constant(48000, 1, 4800, 1.0f), 0.0));
    AutomationLane& pan = pt.lane(AutomationTarget::pan);
    pan.add(0.0, -1.0f);
    pan.add(0.1, 1.0f);
    AudioBuffer po = render(tp, 0.0, 0.1);
    check(po.at(0, 0) == 1.0f && po.at(0, 1) == 0.0f && std::fabs(po.at(3600, 0) - 0.5f) < 1e-3f && po.at(3600, 1) == 1.0f, "pan automation moves a mono track across the stereo field per sample");

    Timeline tm = basic_timeline();
    Track& mt = tm.add_track();
    mt.add_clip(make_clip(constant(48000, 2, 4800, 1.0f), 0.0));
    AutomationLane& mute = mt.lane(AutomationTarget::mute);
    mute.add(0.0, 0.0f, Curve::hold);
    mute.add(0.05, 1.0f, Curve::hold);
    AudioBuffer mo = render(tm, 0.0, 0.1);
    check(mo.at(2399, 0) == 1.0f && mo.at(2400, 0) == 0.0f && mo.at(4000, 1) == 0.0f, "mute automation switches on the exact sample");

    Timeline te = basic_timeline();
    Track& et = te.add_track();
    et.add_clip(make_clip(constant(48000, 2, 48000, 1.0f), 0.0));
    auto scale = std::make_shared<Scale>();
    et.effects->add(scale);
    AutomationLane& sl = et.lane(AutomationTarget::effect_parameter, 0, "amount");
    sl.add(0.0, 1.0f);
    sl.add(1.0, 0.0f);
    AudioBuffer eo = render(te, 0.0, 1.0);
    bool blocks = true;
    for (std::size_t i = 0; i < eo.frames(); ++i) {
        const std::size_t start = i - i % 64;
        if (eo.at(i, 0) != sl.value_at(static_cast<double>(start) / 48000.0)) {
            blocks = false;
            break;
        }
    }
    check(blocks, "effect parameter automation updates the parameter every 64 frames");
    check(std::fabs(scale->amount.get() - sl.value_at((48000.0 - 64.0) / 48000.0)) < 1e-6f, "the automated parameter is left at the last block's value");
    Track& fake = te.add_track();
    fake.lane(AutomationTarget::effect_parameter, 3, "missing").add(0.0, 1.0f);
    fake.lane(AutomationTarget::effect_parameter, 0, "nope").add(0.0, 1.0f);
    AudioBuffer still = render(te, 0.0, 0.1);
    check(still.frames() == 4800, "lanes aimed at missing effects or parameters are ignored");

    Timeline master = basic_timeline();
    master.add_track().add_clip(make_clip(constant(48000, 2, 4800, 1.0f), 0.0));
    master.master_volume_db = -6.0f;
    AudioBuffer mv = render(master, 0.0, 0.05);
    check(std::fabs(mv.at(100, 0) - db_to_gain(-6.0f)) < 1e-6f, "master volume applies");
    AutomationLane& ml = master.master_lane(AutomationTarget::volume);
    ml.add(0.0, 0.0f, Curve::hold);
    ml.add(0.1, -120.0f, Curve::hold);
    AudioBuffer mv2 = render(master, 0.0, 0.1);
    check(mv2.at(100, 0) == 1.0f && mv2.at(4799, 1) == 1.0f, "master volume automation overrides the master fader");
}

void test_mute_solo() {
    Timeline tl = basic_timeline();
    Track& a = tl.add_track("a");
    a.add_clip(make_clip(constant(48000, 2, 4800, 0.5f), 0.0));
    Track& b = tl.add_track("b");
    b.add_clip(make_clip(constant(48000, 2, 4800, 0.25f), 0.0));
    Track& c = tl.add_track("c");
    c.add_clip(make_clip(constant(48000, 2, 4800, 0.125f), 0.0));
    auto level_at = [&](const Timeline& t) { return render(t, 0.0, 0.01).at(100, 0); };
    check(level_at(tl) == 0.875f, "all tracks play");
    tl.tracks[1].mute = true;
    check(level_at(tl) == 0.625f, "a muted track is silent");
    tl.tracks[2].solo = true;
    check(level_at(tl) == 0.125f, "solo plays only soloed tracks");
    tl.tracks[1].solo = true;
    check(level_at(tl) == 0.125f, "mute beats solo");
    tl.tracks[1].mute = false;
    check(level_at(tl) == 0.375f, "several soloed tracks play together");
    tl.tracks[1].solo = false;
    tl.tracks[2].solo = false;
    tl.tracks[0].clips[0].muted = true;
    check(level_at(tl) == 0.375f, "a muted clip is silent");
    tl.tracks[0].volume_db = -120.0f;
    tl.tracks[0].clips[0].muted = false;
    check(level_at(tl) == 0.375f, "-120 dB is silence");
    tl.tracks[0].volume_db = 0.0f;
    tl.tracks[0].pan = 1.0f;
    AudioBuffer panned = render(tl, 0.0, 0.01);
    check(panned.at(100, 0) == 0.375f && panned.at(100, 1) == 0.875f, "static pan uses a balance law on stereo tracks");
}

void test_latency_compensation() {
    for (int master_latent = 0; master_latent < 2; ++master_latent) {
        Timeline tl = basic_timeline();
        Track& a = tl.add_track("latent");
        a.add_clip(make_clip(impulse(48000, 2, 100, 0), 0.5));
        a.effects->add(std::make_shared<Latent>(1000));
        Track& b = tl.add_track("dry");
        b.add_clip(make_clip(impulse(48000, 2, 100, 0), 0.5));
        Track& c = tl.add_track("other");
        c.add_clip(make_clip(impulse(48000, 1, 100, 0), 0.5));
        c.effects->add(std::make_shared<Latent>(77));
        if (master_latent) tl.master_effects->add(std::make_shared<Latent>(333));
        AudioBuffer out = render(tl, 0.0, 1.0);
        std::size_t count = 0;
        for (std::size_t i = 0; i < out.frames(); ++i) {
            if (out.at(i, 0) != 0.0f) ++count;
        }
        check(out.at(24000, 0) == 3.0f && out.at(24000, 1) == 3.0f && count == 1, std::string("tracks with different effect latency stay sample-aligned") + (master_latent ? " (with a latent master chain)" : ""));
    }
    Timeline start_mid = basic_timeline();
    Track& t = start_mid.add_track();
    t.add_clip(make_clip(impulse(48000, 2, 100, 0), 0.5));
    t.effects->add(std::make_shared<Latent>(4000));
    AudioBuffer from = render(start_mid, 0.45, 0.55);
    check(from.at(2400, 0) == 1.0f, "latency compensation works when rendering from the middle");
    Timeline tail = basic_timeline();
    tail.add_track().add_clip(make_clip(impulse(48000, 2, 10, 0), 0.0));
    RenderOptions with_tail;
    with_tail.include_tail = true;
    RenderOptions without_tail;
    without_tail.include_tail = false;
    check(render(tail, with_tail).frames() == 10 && render(tail, without_tail).frames() == 10, "a timeline without tails renders its exact length");
}

void test_resampled_clips() {
    const std::uint32_t src_rate = 44100;
    AudioBuffer src(1, src_rate, src_rate);
    src.at(441, 0) = 1.0f;
    for (std::size_t i = 22050; i < 44100; ++i) src.at(i, 0) = static_cast<float>(0.5 * std::sin(2.0 * k_pi * 1000.0 * (static_cast<double>(i) - 22050.0) / src_rate));
    auto shared = std::make_shared<const AudioBuffer>(std::move(src));
    Timeline tl = basic_timeline();
    tl.add_track().add_clip(make_clip(shared, 0.25));
    AudioBuffer out = render(tl, 0.0, 1.25);
    std::size_t peak_at = 0;
    float peak = 0.0f;
    for (std::size_t i = 0; i < 24000; ++i) {
        if (std::fabs(out.at(i, 0)) > peak) {
            peak = std::fabs(out.at(i, 0));
            peak_at = i;
        }
    }
    check(peak_at == 12000 + 480, "a 44.1 kHz clip lands at the right 48 kHz sample (" + std::to_string(peak_at) + ")");
    std::size_t onset = 0;
    for (std::size_t i = 12000 + 23000; i < out.frames(); ++i) {
        if (std::fabs(out.at(i, 0)) > 0.05f) {
            onset = i;
            break;
        }
    }
    check(onset >= 36000 && onset < 36000 + 12, "the tone starts half a second into the clip (" + std::to_string(onset) + ")");
    const double at1k = tone_level(out, 0, 40000, 12000, 1000.0);
    const double at1088 = tone_level(out, 0, 40000, 12000, 1088.4);
    check(at1k > 0.45 && at1088 < 0.05, "the resampled tone keeps its 1 kHz pitch");
    check(tl.cache()->size() == 1, "the resampled source is cached");
    render(tl, 0.0, 0.5);
    check(tl.cache()->size() == 1, "rendering again reuses the cache");
    RenderOptions at96;
    at96.sample_rate = 96000;
    AudioBuffer hi = render(tl, 0.0, 0.5, at96);
    float hp = 0.0f;
    std::size_t hpi = 0;
    for (std::size_t i = 0; i < hi.frames(); ++i) {
        if (std::fabs(hi.at(i, 0)) > hp) {
            hp = std::fabs(hi.at(i, 0));
            hpi = i;
        }
    }
    check(hi.sample_rate() == 96000 && hpi == 24960, "rendering at another rate keeps timing (" + std::to_string(hpi) + ")");
}

Timeline rich_timeline(std::shared_ptr<NoiseReduction>* nr_out = nullptr) {
    Timeline tl(48000, 2);
    Track& voice = tl.add_track("voice", 1);
    AudioBuffer v = speech_like(48000, 1, 3.0, 31);
    v.mix_in(make_noise(48000, 1, 3.0, false, 0.02, 32));
    Clip vc = make_clip(v, 0.2, "vox");
    vc.fade_in = 0.05;
    vc.fade_out = 0.3;
    vc.fade_out_shape = FadeShape::s_curve;
    voice.add_clip(vc);
    auto nr = std::make_shared<NoiseReduction>();
    voice.effects->add(nr);
    voice.effects->add(std::make_shared<Gain>(-3.0f));
    AutomationLane& g = voice.lane(AutomationTarget::effect_parameter, 1, "db");
    g.add(0.0, -3.0f);
    g.add(1.5, -12.0f, Curve::bezier, 0.5f);
    g.add(2.5, 0.0f);
    voice.pan = -0.3f;
    Track& music = tl.add_track("music");
    AudioBuffer m(2, 44100, 44100 * 2);
    for (std::size_t i = 0; i < m.frames(); ++i) {
        m.at(i, 0) = static_cast<float>(0.2 * std::sin(2.0 * k_pi * 220.0 * i / 44100.0));
        m.at(i, 1) = static_cast<float>(0.2 * std::sin(2.0 * k_pi * 330.0 * i / 44100.0));
    }
    Clip mc = make_clip(m, 0.0, "pad");
    mc.gain_db = -2.0f;
    mc.fade_in = 0.5;
    mc.fade_in_shape = FadeShape::equal_power;
    music.add_clip(mc);
    music.add_clip(mc.duplicate(1.7));
    AutomationLane& vol = music.lane(AutomationTarget::volume);
    vol.add(0.0, 0.0f);
    vol.add(1.0, -10.0f, Curve::exponential, 0.3f);
    vol.add(2.0, 0.0f);
    music.lane(AutomationTarget::pan).add(0.0, 0.0f);
    music.lane(AutomationTarget::pan).add(3.0, 0.5f);
    music.effects->add(std::make_shared<Latent>(300));
    tl.master_effects->add(std::make_shared<Gain>(-1.0f));
    tl.master_lane(AutomationTarget::volume).add(3.0, 0.0f);
    tl.master_lane(AutomationTarget::volume).add(3.6, -40.0f);
    if (nr_out) *nr_out = nr;
    return tl;
}

void test_player_matches_offline() {
    Timeline tl = rich_timeline();
    const AudioBuffer offline = render(tl, 0.0, tl.length());
    TimelinePlayer player(48000, 2);
    player.update(tl);
    player.play();
    AudioBuffer live(2, 48000, offline.frames());
    std::mt19937 rng(5);
    std::uniform_int_distribution<std::size_t> sizes(1, 1500);
    std::size_t pos = 0;
    while (pos < live.frames()) {
        const std::size_t n = std::min(sizes(rng), live.frames() - pos);
        player.render(live.frame(pos), n);
        pos += n;
    }
    bool same = true;
    std::size_t first_diff = 0;
    for (std::size_t i = 0; i < live.sample_count(); ++i) {
        if (live.data()[i] != offline.data()[i]) {
            same = false;
            first_diff = i;
            break;
        }
    }
    check(offline.frames() > 100000 && offline.peak() > 0.05f, "the rich timeline renders audio");
    check(same, "TimelinePlayer output equals offline render sample for sample" + (same ? std::string() : " (first difference at sample " + std::to_string(first_diff) + ")"));
    check(std::fabs(player.position() - static_cast<double>(offline.frames()) / 48000.0) < 1e-9, "the player position advances with rendering");

    const AudioBuffer offline_mid = render(tl, 1.25, 2.25);
    player.seek(1.25);
    check(player.position() == 1.25, "position() reflects a seek immediately");
    AudioBuffer after_seek(2, 48000, 48000);
    for (std::size_t p = 0; p < 48000; p += 480) player.render(after_seek.frame(p), 480);
    check(snr_db(after_seek, offline_mid) >= 300.0, "seeking while playing matches an offline render from the same point");
}

void test_transport_and_loop() {
    Timeline tl = basic_timeline();
    AudioBuffer ramp(1, 48000, 48000 * 2);
    for (std::size_t i = 0; i < ramp.frames(); ++i) ramp.at(i, 0) = static_cast<float>(i) / 96000.0f;
    tl.add_track().add_clip(make_clip(ramp, 0.0));
    TimelinePlayer player(48000, 2);
    player.update(tl);
    std::vector<float> buf(480 * 2);
    player.render(buf.data(), 480);
    check(buf[0] == 0.0f && buf[960 - 1] == 0.0f && player.state() == TimelinePlayer::State::stopped, "a stopped player outputs silence");
    player.seek(0.5);
    player.play();
    player.render(buf.data(), 480);
    check(buf[0] == ramp.at(24000, 0) && buf[2 * 479] == ramp.at(24479, 0), "play starts at the seek position");
    player.pause();
    player.render(buf.data(), 480);
    check(buf[0] == 0.0f && player.position_frame() == 24480 && player.state() == TimelinePlayer::State::paused, "pause holds the position and outputs silence");
    player.play();
    player.render(buf.data(), 480);
    check(buf[0] == ramp.at(24480, 0), "resume continues where it paused");
    player.stop();
    check(player.position_frame() == 24000 && player.state() == TimelinePlayer::State::stopped, "stop returns to where playback started");
    player.render(buf.data(), 480);
    check(player.position_frame() == 24000, "stop keeps the position while rendering silence");

    player.set_loop(1.0, 1.01, true);
    player.seek(0.99);
    player.play();
    std::vector<float> out(2000 * 2);
    for (std::size_t p = 0; p < 2000; p += 100) player.render(out.data() + p * 2, 100);
    bool looped = true;
    for (std::size_t i = 0; i < 2000; ++i) {
        const std::size_t frame = i < 480 ? 47520 + i : 48000 + (i - 480) % 480;
        if (out[i * 2] != ramp.at(frame, 0)) {
            looped = false;
            break;
        }
    }
    check(looped, "the loop region repeats seamlessly on the exact samples");
    check(player.position_frame() >= 48000 && player.position_frame() < 48480, "the position stays inside the loop");
    player.set_looping(false);
    player.render(out.data(), 1000);
    const std::int64_t after = player.position_frame();
    check(after > 48480, "turning the loop off lets playback run past it");

    TimelinePlayer latent_player(48000, 2);
    Timeline lt = basic_timeline();
    Track& lt_track = lt.add_track();
    lt_track.add_clip(make_clip(ramp, 0.0));
    lt_track.effects->add(std::make_shared<Latent>(700));
    latent_player.update(lt);
    latent_player.set_loop(0.5, 0.6, true);
    latent_player.seek(0.5);
    latent_player.play();
    std::vector<float> lo(9600 * 2);
    for (std::size_t p = 0; p < 9600; p += 256) latent_player.render(lo.data() + p * 2, std::min<std::size_t>(256, 9600 - p));
    bool latent_loop = true;
    for (std::size_t i = 0; i < 9600; ++i) {
        if (lo[i * 2] != ramp.at(24000 + i % 4800, 0)) {
            latent_loop = false;
            break;
        }
    }
    check(latent_loop, "loops stay seamless through a latent effect (lookahead wraps with the loop)");
}

void test_editing_while_playing() {
    Timeline tl = rich_timeline();
    TimelinePlayer player(48000, 2);
    player.update(tl);
    player.set_loop(0.0, 2.0, true);
    player.play();
    std::atomic<bool> run{ true };
    std::atomic<std::size_t> blocks{ 0 };
    std::atomic<bool> finite{ true };
    std::thread audio([&] {
        std::vector<float> buf(256 * 2);
        while (run.load()) {
            player.render(buf.data(), 256);
            for (float v : buf) {
                if (!std::isfinite(v)) finite.store(false);
            }
            blocks.fetch_add(1);
            volatile double p = player.position();
            (void)p;
        }
    });
    std::mt19937 rng(77);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    auto shared = std::make_shared<const AudioBuffer>(speech_like(48000, 2, 0.5, 8));
    for (int i = 0; i < 200; ++i) {
        Track& t = tl.tracks[static_cast<std::size_t>(i) % tl.tracks.size()];
        switch (i % 9) {
            case 0: t.add_clip(make_clip(shared, uni(rng) * 3.0)); break;
            case 1: if (t.clips.size() > 1) t.remove_clip(0); break;
            case 2: t.volume_db = static_cast<float>(-uni(rng) * 20.0); break;
            case 3: t.lane(AutomationTarget::pan).add(uni(rng) * 3.0, static_cast<float>(uni(rng) * 2.0 - 1.0)); break;
            case 4: t.effects->add(std::make_shared<Gain>(-1.0f)); break;
            case 5: if (t.effects->size() > 2) t.effects->remove(t.effects->size() - 1); break;
            case 6: player.seek(uni(rng) * 3.0); break;
            case 7: if (!t.clips.empty()) t.split_clip(0, t.clips[0].position + t.clips[0].length * 0.5); break;
            case 8: tl.add_track("extra").add_clip(make_clip(shared, uni(rng))); break;
        }
        if (i % 50 == 25) player.pause();
        if (i % 50 == 30) player.play();
        if (i % 40 == 10) player.set_loop(uni(rng), 1.0 + uni(rng) * 2.0, i % 80 == 10);
        player.update(tl);
        std::this_thread::sleep_for(std::chrono::microseconds(300));
    }
    const std::size_t before = blocks.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    run.store(false);
    audio.join();
    check(blocks.load() > before && player.snapshots_applied() > 0, "the audio thread kept rendering and picked up new snapshots");
    check(finite.load(), "editing while playing produced only finite samples");
    Timeline fresh = basic_timeline();
    fresh.add_track().add_clip(make_clip(constant(48000, 2, 4800, 0.5f), 0.0));
    TimelinePlayer p2(48000, 2);
    p2.update(fresh);
    p2.play();
    std::vector<float> buf(64 * 2);
    p2.render(buf.data(), 64);
    check(buf[0] == 0.5f, "a fresh snapshot plays");
    fresh.tracks[0].volume_db = -6.0f;
    p2.update(fresh);
    p2.render(buf.data(), 64);
    check(std::fabs(buf[0] - 0.5f * db_to_gain(-6.0f)) < 1e-6f && p2.position_frame() == 128, "an edit lands at the next block without a seek");
}

void test_grid_and_markers() {
    Timeline tl = basic_timeline();
    tl.tempo = 90.0;
    tl.beats_per_bar = 3;
    check(std::fabs(tl.beats_to_seconds(3.0) - 2.0) < 1e-12 && std::fabs(tl.seconds_to_beats(2.0) - 3.0) < 1e-12, "beats <-> seconds");
    check(std::fabs(tl.bars_to_seconds(2.0) - 4.0) < 1e-12 && std::fabs(tl.seconds_to_bars(6.0) - 3.0) < 1e-12 && std::fabs(tl.bar_length() - 2.0) < 1e-12, "bars <-> seconds");
    check(std::fabs(tl.snap(1.1, 1.0) - 4.0 / 3.0) < 1e-12 && std::fabs(tl.snap(0.2, 0.25) - 1.0 / 6.0) < 1e-12, "snap to the beat grid");
    tl.add_marker(5.0, "chorus");
    tl.add_marker(1.0, "verse");
    check(tl.markers.size() == 2 && tl.markers[0].name == "verse" && tl.remove_marker(0) && tl.markers[0].name == "chorus", "markers stay sorted");
    check(tl.length() == 0.0, "an empty timeline has no length");
    tl.add_track().add_clip(make_clip(constant(48000, 1, 4800, 1.0f), 2.0));
    check(std::fabs(tl.length() - 2.1) < 1e-12, "length is the end of the last clip");
    check(tl.remove_track(0) && tl.tracks.empty(), "remove_track");
}

void test_export() {
    Timeline tl = rich_timeline();
    const AudioBuffer offline = render(tl);
    const std::string path = temp_path("fizmo_timeline_export.wav");
    std::string err;
    check(export_wav(tl, path, SampleFormat::f32, &err), "export the timeline as a 32-bit float WAV " + err);
    AudioBuffer back;
    check(read_wav(path, back, &err) && back.frames() == offline.frames() && snr_db(back, offline) >= 300.0, "the exported WAV matches the render exactly");
    check(export_wav(tl, path, SampleFormat::s16, &err) && read_wav(path, back, &err) && snr_db(back, offline) > 70.0, "16-bit export");
    std::remove(path.c_str());
    std::size_t frames = 0;
    std::string got_path;
    check(export_audio(tl, "song.mp3", [&](const AudioBuffer& b, const std::string& p) {
        frames = b.frames();
        got_path = p;
        return true;
    }) && frames == offline.frames() && got_path == "song.mp3", "export_audio hands the render to any encoder");
    RenderOptions cancel;
    cancel.progress = [](double p) { return p < 0.5; };
    check(!render_stream(tl, 0.0, tl.length(), cancel, [](const float*, std::size_t) { return true; }), "progress can cancel a render");
    RenderOptions mono;
    mono.channels = 1;
    check(render(tl, 0.0, 0.5, mono).channels() == 1, "render to a different channel count");
}

void test_recorder() {
    TrackRecorder rec;
    check(rec.begin(48000, 2, 1.5), "begin a manual recording");
    check(!rec.begin(48000, 2, 1.5), "can't begin twice");
    std::vector<float> chunk(480 * 2);
    for (int b = 0; b < 100; ++b) {
        for (std::size_t i = 0; i < 480; ++i) {
            chunk[i * 2] = static_cast<float>(b * 480 + static_cast<int>(i)) / 48000.0f;
            chunk[i * 2 + 1] = -chunk[i * 2];
        }
        rec.push(chunk.data(), 480);
    }
    rec.set_latency_offset(0.01);
    Clip take = rec.finish("take 1");
    bool intact = take.source && take.source->frames() == 48000;
    for (std::size_t i = 0; intact && i < 48000; i += 101) intact = take.source->at(i, 0) == static_cast<float>(i) / 48000.0f && take.source->at(i, 1) == -static_cast<float>(i) / 48000.0f;
    check(intact && take.name == "take 1", "the recorder captures every frame in order");
    check(std::fabs(take.position - 1.49) < 1e-12 && std::fabs(take.length - 1.0) < 1e-12, "the take is placed at the start position minus the latency offset");
    TrackRecorder early;
    early.set_latency_offset(0.25);
    early.begin(48000, 1, 0.1);
    std::vector<float> mono(48000, 0.5f);
    early.push(mono.data(), mono.size());
    Clip e = early.finish();
    check(e.position == 0.0 && std::fabs(e.source_start - 0.15) < 1e-12 && std::fabs(e.length - 0.85) < 1e-12, "a take that would start before zero is trimmed");
    TrackRecorder overflow;
    overflow.begin(8000, 1, 0.0);
    std::vector<float> big(8000 * 6, 0.1f);
    overflow.push(big.data(), big.size());
    Clip o = overflow.finish();
    check(overflow.dropped_frames() == 8000 * 2 && o.source->frames() == 8000 * 4, "a full ring drops frames and counts them instead of blocking");

    std::string err;
    if (!set_backend("null", &err)) {
        check(false, "switch to the null backend " + err);
        return;
    }
    Timeline tl = basic_timeline();
    tl.add_track().add_clip(make_clip(constant(48000, 2, 48000 * 4, 0.1f), 0.0));
    TimelinePlayer player(48000, 2);
    player.update(tl);
    StreamConfig out_cfg;
    out_cfg.latency_ms = 10;
    check(player.start(out_cfg, &err), "the player opens a playback stream " + err);
    player.seek(1.0);
    player.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    TrackRecorder live;
    StreamConfig in_cfg;
    in_cfg.sample_rate = 48000;
    in_cfg.channels = 1;
    in_cfg.latency_ms = 10;
    check(live.start(in_cfg, &player, 0.0, &err), "start recording from a capture stream " + err);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    check(live.recording() && live.recorded_seconds() > 0.15, "frames arrive from the capture stream");
    Clip got = live.finish("overdub");
    const double expected = live.start_position();
    check(got.length > 0.15 && got.length < 0.6 && got.source->channels() == 1, "the overdub clip has the captured audio (" + fmt(got.length, 3) + " s)");
    check(expected > 1.05 && expected < 1.3 && got.position < expected && got.position > expected - 0.1, "the overdub lands at the transport position minus device latency (" + fmt(got.position, 3) + " vs " + fmt(expected, 3) + ")");
    player.stop_output();
    set_backend("auto");
}

} // namespace

int main() {
    std::printf("noise reduction\n");
    test_noise_reduction();
    std::printf("hum remover\n");
    test_hum_remover();
    std::printf("click remover\n");
    test_click_remover();
    std::printf("timeline\n");
    test_clips_and_fades();
    test_automation();
    test_mute_solo();
    test_latency_compensation();
    test_resampled_clips();
    test_player_matches_offline();
    test_transport_and_loop();
    test_editing_while_playing();
    test_grid_and_markers();
    test_export();
    test_recorder();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
