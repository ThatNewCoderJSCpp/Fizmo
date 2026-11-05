#include "../Source/Audio/audio.hpp"
#include "../Source/Audio/presets.hpp"
#include "../Source/Audio/stretch.hpp"

#include "portable_random.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace fizmo::audio;

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define FIZMO_TEST_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define FIZMO_TEST_SANITIZED 1
#endif // FIZMO_TEST_SANITIZED
#endif // FIZMO_TEST_SANITIZED

#if !defined(FIZMO_TEST_SANITIZED) && !defined(_MSC_VER)
#define FIZMO_TEST_COUNT_ALLOCS 1
#endif // FIZMO_TEST_COUNT_ALLOCS

namespace {

std::atomic<long> g_render_allocs{ 0 };
thread_local bool g_in_render = false;
void* (*volatile g_malloc)(std::size_t) = std::malloc;
void (*volatile g_free)(void*) = std::free;

} // namespace

#if defined(FIZMO_TEST_COUNT_ALLOCS)
void* operator new(std::size_t size) {
    if (g_in_render) g_render_allocs.fetch_add(1);
    void* p = g_malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    g_free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    g_free(p);
}
#endif // FIZMO_TEST_COUNT_ALLOCS

namespace {

int g_passed = 0;
int g_failed = 0;
const double k_pi = 3.14159265358979323846;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
        if (std::getenv("FIZMO_VERBOSE")) std::printf("ok: %s\n", what.c_str());
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

void info(const std::string& what) {
    std::printf("  %s\n", what.c_str());
}

std::string num(double v, int digits = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, v);
    return buf;
}

std::string temp_path(const std::string& name) {
    const char* dir = std::getenv("TMPDIR");
#if defined(_WIN32)
    if (!dir) dir = std::getenv("TEMP");
#endif // _WIN32
    std::string base = dir ? dir : "/tmp";
    return base + "/" + name;
}

double db(double gain) {
    return gain > 0.0 ? 20.0 * std::log10(gain) : -200.0;
}

AudioBuffer speech_like(std::uint32_t rate, double seconds, double f0 = 140.0, unsigned seed = 1) {
    const std::size_t frames = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(1, rate, frames);
    std::mt19937 rng(seed);
    auto noise = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 1.0f); };
    const double formants[3][3] = { { 730.0, 1090.0, 2440.0 }, { 270.0, 2290.0, 3010.0 }, { 300.0, 870.0, 2240.0 } };
    const double syllable = 0.33;
    const double voiced = 0.25;
    double phase = 0.0;
    float hp_prev = 0.0f;
    float hp_out = 0.0f;
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        const std::size_t syl = static_cast<std::size_t>(t / syllable);
        const double in_syl = t - static_cast<double>(syl) * syllable;
        const double* f = formants[syl % 3];
        const double pitch = f0 * (1.0 + 0.08 * std::sin(2.0 * k_pi * 0.7 * t) + 0.02 * std::sin(2.0 * k_pi * 5.0 * t));
        phase += 2.0 * k_pi * pitch / rate;
        if (phase > 2.0 * k_pi * 1000.0) phase -= 2.0 * k_pi * 1000.0;
        double v = 0.0;
        if (in_syl < voiced) {
            const double env = std::sin(k_pi * in_syl / voiced);
            for (int h = 1; h * pitch < 5000.0; ++h) {
                const double hz = h * pitch;
                double amp = 0.0;
                for (int k = 0; k < 3; ++k) {
                    const double bw = 80.0 + 40.0 * k;
                    const double d = (hz - f[k]) / bw;
                    amp += (k == 0 ? 1.0 : 0.6 / k) / (1.0 + d * d);
                }
                v += amp * std::sin(h * phase) / std::sqrt(static_cast<double>(h));
            }
            v *= 0.12 * env;
        } else if (syl % 4 == 1 && in_syl < voiced + 0.06) {
            const float n = noise(rng);
            hp_out = 0.6f * (hp_out + n - hp_prev);
            hp_prev = n;
            v = 0.08 * hp_out * std::sin(k_pi * (in_syl - voiced) / 0.06);
        }
        b.at(i, 0) = static_cast<float>(v);
    }
    return b;
}

AudioBuffer chords(std::uint32_t rate, double seconds) {
    const std::size_t frames = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(2, rate, frames);
    const double notes[2][3] = { { 261.63, 329.63, 392.00 }, { 220.00, 261.63, 329.63 } };
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        const std::size_t bar = static_cast<std::size_t>(t / 1.0);
        const double in_bar = t - static_cast<double>(bar);
        const double env = std::exp(-1.5 * in_bar) * std::min(1.0, in_bar / 0.01);
        double l = 0.0;
        double r = 0.0;
        for (int n = 0; n < 3; ++n) {
            const double hz = notes[bar % 2][n];
            for (int h = 1; h <= 4; ++h) {
                const double a = 0.12 / (h * h);
                l += a * std::sin(2.0 * k_pi * hz * h * t * 1.0007);
                r += a * std::sin(2.0 * k_pi * hz * h * t * 0.9993 + 0.3 * n);
            }
        }
        b.at(i, 0) = static_cast<float>(l * env);
        b.at(i, 1) = static_cast<float>(r * env);
    }
    return b;
}

AudioBuffer drums(std::uint32_t rate, double seconds, double bpm = 120.0) {
    const std::size_t frames = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(2, rate, frames);
    std::mt19937 rng(7);
    auto uni = [&](auto& gen) { return fizmo_test::random_float(gen, -1.0f, 1.0f); };
    const double beat = 60.0 / bpm;
    float hp = 0.0f;
    float prev = 0.0f;
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / rate;
        const std::size_t step = static_cast<std::size_t>(t / (beat / 2.0));
        const double in_step = t - static_cast<double>(step) * (beat / 2.0);
        double v = 0.0;
        if (step % 2 == 0) {
            const double f = 50.0 + 70.0 * std::exp(-in_step * 30.0);
            v += 0.8 * std::sin(2.0 * k_pi * f * in_step) * std::exp(-in_step * 12.0);
        }
        const float n = uni(rng);
        hp = 0.9f * (hp + n - prev);
        prev = n;
        if (step % 4 == 2) v += 0.5 * n * std::exp(-in_step * 25.0);
        v += 0.15 * hp * std::exp(-in_step * 60.0);
        b.at(i, 0) = static_cast<float>(v);
        b.at(i, 1) = static_cast<float>(v * 0.9);
    }
    return b;
}

AudioBuffer tone(double hz, std::uint32_t rate, std::size_t channels, double seconds, float amp = 0.5f) {
    const std::size_t frames = static_cast<std::size_t>(seconds * rate);
    AudioBuffer b(channels, rate, frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float v = amp * static_cast<float>(std::sin(2.0 * k_pi * hz * static_cast<double>(i) / rate));
        for (std::size_t c = 0; c < channels; ++c) b.at(i, c) = v;
    }
    return b;
}

std::vector<float> mono_of(const AudioBuffer& b) {
    std::vector<float> m(b.frames());
    for (std::size_t i = 0; i < b.frames(); ++i) {
        float s = 0.0f;
        for (std::size_t c = 0; c < b.channels(); ++c) s += b.at(i, c);
        m[i] = s / static_cast<float>(b.channels());
    }
    return m;
}

double median_pitch(const AudioBuffer& b, double start = 0.0, double end = -1.0, float min_hz = 60.0f, float max_hz = 1200.0f, double* voiced_fraction = nullptr) {
    const std::vector<float> m = mono_of(b);
    PitchDetector d(static_cast<float>(b.sample_rate()), min_hz, max_hz);
    const std::size_t span = d.window_frames();
    const std::size_t hop = b.sample_rate() / 100;
    const std::size_t first = static_cast<std::size_t>(start * b.sample_rate());
    const std::size_t last = end < 0.0 ? m.size() : std::min(m.size(), static_cast<std::size_t>(end * b.sample_rate()));
    std::vector<float> found;
    std::size_t active = 0;
    for (std::size_t s = first; s + span <= last; s += hop) {
        double e = 0.0;
        for (std::size_t i = 0; i < span; ++i) e += static_cast<double>(m[s + i]) * m[s + i];
        if (std::sqrt(e / span) < 0.003) continue;
        ++active;
        const PitchEstimate p = d.analyze(m.data() + s, span);
        if (p.voiced && p.confidence > 0.5f) found.push_back(p.frequency);
    }
    if (voiced_fraction) *voiced_fraction = active ? static_cast<double>(found.size()) / static_cast<double>(active) : 0.0;
    if (found.empty()) return 0.0;
    std::nth_element(found.begin(), found.begin() + static_cast<std::ptrdiff_t>(found.size() / 2), found.end());
    return found[found.size() / 2];
}

double goertzel(const std::vector<float>& x, std::size_t first, std::size_t count, double hz, double rate) {
    const double w = 2.0 * k_pi * hz / rate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0; i < count && first + i < x.size(); ++i) {
        const double s = x[first + i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return 2.0 * std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / static_cast<double>(count);
}

double rms_range(const AudioBuffer& b, double start, double end) {
    const std::size_t a = std::min(b.frames(), static_cast<std::size_t>(start * b.sample_rate()));
    const std::size_t z = std::min(b.frames(), static_cast<std::size_t>(end * b.sample_rate()));
    double e = 0.0;
    std::size_t n = 0;
    for (std::size_t i = a; i < z; ++i) {
        for (std::size_t c = 0; c < b.channels(); ++c) {
            e += static_cast<double>(b.at(i, c)) * b.at(i, c);
            ++n;
        }
    }
    return n ? std::sqrt(e / static_cast<double>(n)) : 0.0;
}

bool all_finite(const AudioBuffer& b) {
    for (float v : b.samples()) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

bool identical(const AudioBuffer& a, const AudioBuffer& b) {
    return a.channels() == b.channels() && a.frames() == b.frames() && std::memcmp(a.data(), b.data(), a.sample_count() * sizeof(float)) == 0;
}

double max_abs_diff(const AudioBuffer& a, const AudioBuffer& b) {
    if (a.sample_count() != b.sample_count()) return 1e9;
    double m = 0.0;
    for (std::size_t i = 0; i < a.sample_count(); ++i) m = std::max(m, static_cast<double>(std::fabs(a.data()[i] - b.data()[i])));
    return m;
}

double snr_db(const AudioBuffer& ref, const AudioBuffer& test, std::size_t skip = 0) {
    double s = 0.0;
    double e = 0.0;
    const std::size_t n = std::min(ref.sample_count(), test.sample_count());
    for (std::size_t i = skip * ref.channels(); i < n; ++i) {
        const double r = ref.data()[i];
        const double d = r - test.data()[i];
        s += r * r;
        e += d * d;
    }
    return e > 0.0 ? 10.0 * std::log10(s / e) : 300.0;
}

void test_reverse() {
    std::printf("reverse\n");
    const AudioBuffer src = chords(48000, 2.0);
    AudioBuffer r = src;
    reverse(r);
    bool mirrored = r.frames() == src.frames();
    for (std::size_t i = 0; mirrored && i < src.frames(); ++i) {
        for (std::size_t c = 0; c < 2; ++c) mirrored = mirrored && r.at(i, c) == src.at(src.frames() - 1 - i, c);
    }
    check(mirrored, "reverse puts frame i at n-1-i for every frame and channel");
    AudioBuffer back = r;
    reverse(back);
    check(identical(back, src), "reversing twice gives the original back bit for bit");
    const AudioBuffer speech = speech_like(48000, 2.0);
    AudioBuffer rs = speech;
    reverse(rs);
    const double p1 = median_pitch(speech);
    const double p2 = median_pitch(rs);
    check(std::fabs(p1 - p2) < 3.0 && rs.duration() == speech.duration(), "reversed speech keeps duration and pitch (" + num(p1) + " vs " + num(p2) + " Hz)");
}

void test_reverb_delay() {
    std::printf("reverb and delay\n");
    AudioBuffer click(2, 48000, 48000);
    click.at(100, 0) = 1.0f;
    click.at(100, 1) = 1.0f;
    Delay d;
    d.time_ms.set(250.0f);
    d.feedback.set(0.5f);
    d.mix.set(1.0f);
    d.low_cut.set(20.0f);
    d.high_cut.set(20000.0f);
    AudioBuffer echoed = applied(d, click);
    std::vector<float> m = mono_of(echoed);
    std::size_t first_echo = 0;
    float a1 = 0.0f;
    for (std::size_t i = 110; i < 12000 + 2000 && i < m.size(); ++i) {
        if (std::fabs(m[i]) > a1) {
            a1 = std::fabs(m[i]);
            first_echo = i;
        }
    }
    float a2 = 0.0f;
    std::size_t second_echo = 0;
    for (std::size_t i = first_echo + 6000; i < first_echo + 18000 && i < m.size(); ++i) {
        if (std::fabs(m[i]) > a2) {
            a2 = std::fabs(m[i]);
            second_echo = i;
        }
    }
    auto energy_at = [&](std::size_t c) {
        double e = 0.0;
        for (std::size_t i = c - 300; i < c + 300 && i < m.size(); ++i) e += static_cast<double>(m[i]) * m[i];
        return std::sqrt(e);
    };
    a1 = static_cast<float>(energy_at(first_echo));
    a2 = static_cast<float>(energy_at(second_echo));
    check(first_echo >= 100 + 12000 - 40 && first_echo <= 100 + 12000 + 40, "delay echo lands 250 ms after the click (at " + std::to_string(first_echo) + ")");
    check(second_echo > first_echo + 11900 && second_echo < first_echo + 12100 && a2 / a1 > 0.35f && a2 / a1 < 0.65f, "second echo follows 250 ms later at about feedback level (ratio " + num(a2 / a1) + ")");
    check(echoed.frames() == click.frames() + d.tail_frames(), "delay output includes its tail (" + num(d.tail_frames() / 48000.0) + " s)");

    Reverb rv;
    rv.decay.set(1.5f);
    rv.mix.set(0.5f);
    AudioBuffer dry = speech_like(48000, 2.0).with_channels(2);
    AudioBuffer wet = applied(rv, dry);
    const double tail_s = static_cast<double>(wet.frames() - dry.frames()) / 48000.0;
    check(tail_s > 1.5 && tail_s < 2.5, "reverb adds a tail of " + num(tail_s) + " s for a 1.5 s decay");
    const double e_in = rms_range(dry, 0.0, 2.0);
    const double e1 = rms_range(wet, 2.05, 2.25);
    const double e2 = rms_range(wet, 2.6, 2.8);
    const double e3 = rms_range(wet, 3.2, 3.4);
    info("reverb tail levels relative to input: " + num(db(e1 / e_in)) + ", " + num(db(e2 / e_in)) + ", " + num(db(e3 / e_in)) + " dB");
    check(e1 > e2 && e2 > e3 && db(e1 / e_in) > -30.0, "reverb tail rings after the input stops and decays");
    const double slope = (db(e3) - db(e1)) / 1.15;
    check(slope < -25.0 && slope > -80.0, "reverb decays at about 60 dB per 1.5 s (" + num(slope) + " dB/s)");
    check(all_finite(wet), "reverb output is finite");
    AudioBuffer rv_rev = wet;
    reverse(rv_rev);
    check(rms_range(rv_rev, 0.0, 0.2) > 0.0 && rms_range(rv_rev, 0.0, 0.2) < rms_range(rv_rev, 1.0, 1.2), "reverse reverb swells up into the reversed audio");
}

void test_voice_changer() {
    std::printf("voice changer presets\n");
    const AudioBuffer speech = speech_like(48000, 3.0, 140.0, 3);
    const double base = median_pitch(speech);
    check(base > 120.0 && base < 165.0, "speech material has a pitch near 140 Hz (" + num(base) + ")");
    const double in_rms = speech.rms();
    std::vector<float> m_in = mono_of(speech);
    for (std::size_t p = 0; p < voice_preset_count(); ++p) {
        const VoicePreset vp = static_cast<VoicePreset>(p);
        VoiceChanger vc(vp);
        AudioBuffer out = applied(vc, speech, false);
        const std::string name = voice_preset_name(vp);
        double voiced = 0.0;
        const double pitch = median_pitch(out, 0.0, -1.0, 50.0f, 1200.0f, &voiced);
        const double level = db(out.rms() / in_rms);
        info(name + ": pitch " + num(pitch) + " Hz, voiced " + num(voiced * 100.0, 0) + "%, level " + num(level) + " dB");
        check(all_finite(out) && out.frames() == speech.frames(), name + ": output is finite and the same length");
        check(level > -24.0 && level < 15.0, name + ": output level is reasonable (" + num(level) + " dB)");
        if (vp == VoicePreset::none) {
            check(max_abs_diff(out, speech) < 1e-5, "none: passes the voice through unchanged");
            continue;
        }
        check(max_abs_diff(out, speech) > 0.01, name + ": audibly changes the voice");
        double semis = 0.0;
        bool pitched = true;
        switch (vp) {
            case VoicePreset::helium: semis = 8.0; break;
            case VoicePreset::deep: semis = -6.0; break;
            case VoicePreset::monster: semis = -8.0; break;
            case VoicePreset::feminine: semis = 5.0; break;
            case VoicePreset::masculine: semis = -5.0; break;
            default: pitched = false; break;
        }
        if (pitched) {
            const double want = base * std::pow(2.0, semis / 12.0);
            check(std::fabs(pitch / want - 1.0) < 0.06, name + ": pitch moves " + num(semis, 0) + " semitones (" + num(pitch) + " vs " + num(want) + " Hz)");
        }
        if (vp == VoicePreset::alien) check(std::fabs(pitch - base) > 20.0, "alien: ring modulation moves the perceived pitch (" + num(pitch) + " Hz)");
        if (vp == VoicePreset::robot) check(std::fabs(pitch - 120.0) < 8.0, "robot: voice is re-pitched onto the 120 Hz carrier (" + num(pitch) + ")");
        if (vp == VoicePreset::whisper) check(voiced < 0.35, "whisper: voice becomes mostly unvoiced (" + num(voiced * 100.0, 0) + "% voiced)");
        if (vp == VoicePreset::telephone || vp == VoicePreset::radio) {
            const std::vector<float> m_out = mono_of(out);
            double lo_in = 0.0;
            double lo_out = 0.0;
            for (std::size_t s = 0; s + 4800 < m_in.size(); s += 4800) {
                lo_in += goertzel(m_in, s, 4800, base, 48000.0);
                lo_out += goertzel(m_out, s, 4800, base, 48000.0);
            }
            const double cut = db(lo_out / lo_in) - level;
            check(cut < -6.0, name + ": low fundamental is band-limited away (" + num(cut) + " dB)");
        }
        if (vp == VoicePreset::cave) check(out.frames() == speech.frames() && vc.tail_frames() > 4800, "cave: has a reverb tail of " + num(vc.tail_frames() / 48000.0) + " s");
    }
    VoiceChanger live(VoicePreset::helium);
    live.prepare(48000, 1);
    AudioBuffer stream = speech;
    float jump = 0.0f;
    float prev = 0.0f;
    for (std::size_t pos = 0; pos < stream.frames(); pos += 480) {
        if (pos == 72000) live.preset.set(static_cast<float>(VoicePreset::deep));
        const std::size_t n = std::min<std::size_t>(480, stream.frames() - pos);
        live.process(stream.frame(pos), n);
        for (std::size_t i = 0; i < n; ++i) {
            const float v = stream.at(pos + i, 0);
            jump = std::max(jump, std::fabs(v - prev));
            prev = v;
        }
    }
    const double before = median_pitch(stream, 0.3, 1.4);
    const double after = median_pitch(stream, 1.7, 3.0);
    check(before > base * 1.4 && after < base * 0.85 && jump < 0.5f, "switching presets live crossfades from helium (" + num(before) + ") to deep (" + num(after) + ") without clicks (max step " + num(jump, 3) + ")");
}

void test_pitch_and_stretch() {
    std::printf("pitch shift, time stretch, speed, tempo\n");
    const AudioBuffer t220 = tone(220.0, 48000, 1, 2.0);
    PitchShifter ps(5.0f);
    AudioBuffer shifted = applied(ps, t220, false);
    const double want = 220.0 * std::pow(2.0, 5.0 / 12.0);
    const double got = median_pitch(shifted, 0.3, 1.8);
    check(std::fabs(got - want) < 2.0 && shifted.frames() == t220.frames(), "PitchShifter +5 st moves 220 Hz to " + num(got) + " Hz (want " + num(want) + ")");

    const AudioBuffer speech = speech_like(48000, 4.0, 140.0, 5);
    const AudioBuffer music = chords(48000, 4.0);
    const AudioBuffer beat = drums(48000, 4.0);
    check(detect_stretch_mode(speech) == StretchMode::wsola, "automatic mode picks WSOLA for speech");
    check(detect_stretch_mode(music) == StretchMode::phase_vocoder, "automatic mode picks the phase vocoder for stereo chords");
    const double sp = median_pitch(speech);
    const double tempos[] = { 0.25, 0.5, 0.75, 1.25, 1.5, 2.0, 4.0 };
    for (double t : tempos) {
        const AudioBuffer s = time_stretch(speech, t);
        const std::size_t want_frames = static_cast<std::size_t>(std::floor(speech.frames() / t + 0.5));
        const double p = median_pitch(s);
        check(s.frames() == want_frames && std::fabs(p / sp - 1.0) < 0.03, "speech tempo x" + num(t) + ": " + num(s.duration(), 3) + " s, pitch " + num(p) + " (orig " + num(sp) + ")");
        const AudioBuffer mu = time_stretch(music, t);
        const std::vector<float> mm = mono_of(mu);
        const double c4 = goertzel(mm, 0, std::min<std::size_t>(mm.size(), 24000), 261.63, 48000.0);
        const double off = goertzel(mm, 0, std::min<std::size_t>(mm.size(), 24000), 277.18, 48000.0);
        check(mu.frames() == static_cast<std::size_t>(std::floor(music.frames() / t + 0.5)) && c4 > 8.0 * off && all_finite(mu), "music tempo x" + num(t) + ": " + num(mu.duration(), 3) + " s, chord tones stay in tune (C4/C#4 " + num(db(c4 / off)) + " dB)");
    }
    const AudioBuffer slow_beat = time_stretch(beat, 0.5);
    std::vector<float> bm = mono_of(slow_beat);
    std::vector<std::size_t> onsets;
    double prev_e = 0.0;
    for (std::size_t s = 0; s + 480 < bm.size(); s += 480) {
        double e = 0.0;
        for (std::size_t i = 0; i < 480; ++i) e += static_cast<double>(bm[s + i]) * bm[s + i];
        if (e > 6.0 * prev_e + 0.5 && (onsets.empty() || s - onsets.back() > 24000)) onsets.push_back(s);
        prev_e = e;
    }
    bool spaced = onsets.size() >= 6;
    for (std::size_t i = 1; i < onsets.size(); ++i) spaced = spaced && std::fabs(static_cast<double>(onsets[i] - onsets[i - 1]) - 48000.0) < 1500.0;
    check(spaced, "drums at half tempo hit every 1.0 s instead of 0.5 s (" + std::to_string(onsets.size()) + " kicks)");

    const double semis[] = { -12.0, -7.0, -3.0, 4.0, 7.0, 12.0 };
    for (double s : semis) {
        const AudioBuffer out = change_pitch(speech, s);
        const double p = median_pitch(out);
        const double w = sp * std::pow(2.0, s / 12.0);
        check(out.frames() == speech.frames() && std::fabs(p / w - 1.0) < 0.03, "speech pitch " + num(s, 0) + " st keeps length, pitch " + num(p) + " (want " + num(w) + ")");
        const AudioBuffer mu = change_pitch(music, s);
        const std::vector<float> mm = mono_of(mu);
        const double target = 261.63 * std::pow(2.0, s / 12.0);
        const double hit = goertzel(mm, 0, 24000, target, 48000.0);
        const double miss = goertzel(mm, 0, 24000, 261.63 * std::pow(2.0, (s + 1.0) / 12.0), 48000.0);
        check(mu.frames() == music.frames() && hit > 6.0 * miss, "music pitch " + num(s, 0) + " st moves the chord (" + num(db(hit / miss)) + " dB)");
    }
    const double speeds[] = { 0.5, 0.8, 1.5, 2.0 };
    for (double f : speeds) {
        const AudioBuffer out = change_speed(speech, f);
        const double p = median_pitch(out);
        check(out.frames() == static_cast<std::size_t>(std::floor(speech.frames() / f + 0.5)) && std::fabs(p / (sp * f) - 1.0) < 0.03, "tape speed x" + num(f) + " changes both: " + num(out.duration(), 3) + " s, pitch " + num(p));
    }
    const AudioBuffer both = change_tempo_and_pitch(speech, 1.5, -4.0);
    const double pb = median_pitch(both);
    check(both.frames() == static_cast<std::size_t>(std::floor(speech.frames() / 1.5 + 0.5)) && std::fabs(pb / (sp * std::pow(2.0, -4.0 / 12.0)) - 1.0) < 0.03, "tempo 1.5 and -4 st together (" + num(both.duration(), 3) + " s, " + num(pb) + " Hz)");

    const AudioBuffer same = time_stretch(music, 1.0, StretchQuality::high, StretchMode::phase_vocoder);
    check(snr_db(music, same, 0) > 20.0, "phase vocoder at tempo 1 reproduces the input closely (" + num(snr_db(music, same, 0)) + " dB SNR)");
    const AudioBuffer same_w = time_stretch(speech, 1.0, StretchQuality::high, StretchMode::wsola);
    check(snr_db(speech, same_w, 0) > 30.0, "WSOLA at tempo 1 reproduces the input closely (" + num(snr_db(speech, same_w, 0)) + " dB SNR)");

    TimeStretcher rt(1, 48000, StretchMode::wsola, StretchQuality::normal, 512);
    const std::size_t lat = rt.latency_frames();
    AudioBuffer live(1, 48000, speech.frames());
    std::size_t in_pos = 0;
    std::size_t out_pos = 0;
    std::size_t changes = 0;
    while (out_pos < live.frames()) {
        if (out_pos > 0 && out_pos % 48000 < 256) {
            rt.set_pitch_semitones(changes % 2 ? 3.0 : -3.0);
            ++changes;
        }
        while (rt.available() < 256 && in_pos < speech.frames()) {
            const std::size_t need = std::max<std::size_t>(1, rt.input_required());
            const std::size_t n = std::min(need, speech.frames() - in_pos);
            in_pos += rt.push(speech.frame(in_pos), n);
            if (in_pos >= speech.frames()) rt.finish();
        }
        const std::size_t got = rt.pull(live.frame(out_pos), std::min<std::size_t>(256, live.frames() - out_pos));
        if (got == 0) break;
        out_pos += got;
    }
    check(out_pos == live.frames() && all_finite(live) && lat > 0 && lat < 4800, "real-time stretcher streams in 256-frame blocks with pitch changes (latency " + num(lat / 48.0, 1) + " ms)");
}

void test_eq_dynamics() {
    std::printf("EQ and dynamics\n");
    const double rate = 48000.0;
    auto tone_gain = [&](Effect& e, double hz) {
        const AudioBuffer in = tone(hz, 48000, 1, 1.0, 0.1f);
        AudioBuffer out = applied(e, in, false);
        std::vector<float> a = mono_of(in);
        std::vector<float> b = mono_of(out);
        return db(goertzel(b, 24000, 19200, hz, rate) / goertzel(a, 24000, 19200, hz, rate));
    };
    ParametricEq eq(4);
    eq.set_band(0, FilterType::peak, 1000.0f, 1.0f, 12.0f);
    eq.set_band(1, FilterType::highshelf, 8000.0f, 0.707f, -6.0f);
    const double g1k = tone_gain(eq, 1000.0);
    const double g100 = tone_gain(eq, 100.0);
    const double g15k = tone_gain(eq, 15000.0);
    check(std::fabs(g1k - 12.0) < 0.3 && std::fabs(g100) < 0.5 && std::fabs(g15k + 6.0) < 0.6, "parametric EQ: +" + num(g1k) + " dB at 1 kHz, " + num(g100) + " dB at 100 Hz, " + num(g15k) + " dB at 15 kHz");
    GraphicEq geq;
    geq.band(4).set(-9.0f);
    check(std::fabs(tone_gain(geq, 500.0) + 9.0) < 1.0, "graphic EQ cuts the 500 Hz band");
    Filter lp(FilterType::lowpass, 1000.0f, 0.707f, 0.0f, 24.0f);
    check(tone_gain(lp, 8000.0) < -60.0 && std::fabs(tone_gain(lp, 100.0)) < 0.2, "24 dB/oct lowpass removes 8 kHz");

    Compressor comp(-20.0f, 4.0f, 5.0f, 50.0f);
    comp.knee.set(0.0f);
    comp.set_detection(Detection::rms);
    const AudioBuffer loud = tone(1000.0, 48000, 2, 1.0, 0.5f);
    AudioBuffer squashed = applied(comp, loud, false);
    const double in_db = db(rms_range(loud, 0.5, 1.0));
    const double out_db = db(rms_range(squashed, 0.5, 1.0));
    const double expect = -20.0 + (in_db + 20.0) / 4.0;
    check(std::fabs(out_db - expect) < 0.5, "compressor 4:1 at -20 dB (RMS): " + num(in_db) + " dB in -> " + num(out_db) + " dB out (expect " + num(expect) + ")");
    Compressor peak_comp(-20.0f, 4.0f, 5.0f, 50.0f);
    peak_comp.knee.set(0.0f);
    AudioBuffer peak_squashed = applied(peak_comp, loud, false);
    const double peak_out = db(peak_squashed.slice(24000, 24000).peak());
    check(peak_out < -12.0 && peak_out > -17.5, "compressor 4:1 (peak detection) brings a -6 dB peak down to " + num(peak_out) + " dB");
    const AudioBuffer quiet = tone(1000.0, 48000, 2, 1.0, 0.03f);
    AudioBuffer untouched = applied(comp, quiet, false);
    check(std::fabs(db(untouched.rms() / quiet.rms())) < 0.2, "compressor leaves signal below threshold alone");

    AudioBuffer hot = drums(48000, 4.0);
    hot.apply_gain(4.0f);
    Limiter lim(-1.0f, 50.0f, 5.0f);
    AudioBuffer limited = applied(lim, hot, false);
    const float peak = peak_db(limited);
    check(peak <= -0.99f && peak > -3.0f && hot.peak() > 2.0f, "limiter holds +" + num(db(hot.peak())) + " dB drums under -1 dB ceiling (peak " + num(peak) + ")");
    check(limited.frames() == hot.frames(), "limiter output keeps length (latency compensated)");
}

void test_noise_reduction() {
    std::printf("noise reduction\n");
    AudioBuffer clean(1, 48000, 48000);
    clean.append(speech_like(48000, 4.0, 150.0, 9));
    clean.append(AudioBuffer(1, 48000, 72000));
    AudioBuffer noisy = clean;
    std::mt19937 rng(11);
    auto n = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 0.01f); };
    for (float& v : noisy.samples()) v += n(rng);
    const AudioBuffer profile = noisy.slice(0, 40000);
    NoiseReduction nr;
    nr.learn_profile(profile);
    nr.mode.set(1.0f);
    nr.reduction_db.set(24.0f);
    AudioBuffer out = applied(nr, noisy, false);
    const double before = snr_db(clean, noisy, 48000);
    const double after = snr_db(clean, out, 48000);
    const double floor_before = rms_range(noisy, 5.3, 6.4);
    const double floor_after = rms_range(out, 5.3, 6.4);
    check(after > before + 4.0, "noise reduction improves SNR of speech from " + num(before) + " dB to " + num(after) + " dB");
    check(db(floor_after / floor_before) < -15.0, "noise-only region drops by " + num(db(floor_after / floor_before)) + " dB");
    AudioBuffer auto_out = reduce_noise(noisy);
    const double auto_drop = db(rms_range(auto_out, 5.3, 6.4) / floor_before);
    check(auto_drop < -10.0 && snr_db(clean, auto_out, 48000) > before + 3.0 && all_finite(auto_out), "adaptive mode (no profile) also cleans it: floor " + num(auto_drop) + " dB, SNR " + num(snr_db(clean, auto_out, 48000)) + " dB");
}

void test_edits() {
    std::printf("fades, crossfade, normalize, trim\n");
    AudioBuffer a = tone(440.0, 48000, 2, 2.0, 0.5f);
    fade_in(a, 0.5);
    fade_out(a, 0.5);
    check(a.at(0, 0) == 0.0f && std::fabs(a.at(a.frames() - 1, 0)) < 1e-3f, "fade in starts at silence and fade out ends at silence");
    check(rms_range(a, 0.0, 0.1) < rms_range(a, 0.2, 0.3) && rms_range(a, 0.2, 0.3) < rms_range(a, 0.6, 1.4), "fade in rises monotonically");
    const AudioBuffer x = tone(300.0, 48000, 2, 2.0, 0.5f);
    const AudioBuffer y = tone(300.0, 48000, 2, 2.0, 0.5f);
    const AudioBuffer xf = crossfade(x, y, 0.5, FadeCurve::equal_power);
    check(xf.frames() == x.frames() + y.frames() - 24000, "crossfade overlaps 0.5 s (" + num(xf.duration(), 3) + " s)");
    double lo = 1.0;
    double hi = 0.0;
    for (double t = 1.5; t < 2.0; t += 0.05) {
        const double r = rms_range(xf, t, t + 0.05);
        lo = std::min(lo, r);
        hi = std::max(hi, r);
    }
    check(db(hi / lo) < 3.5, "equal-power crossfade has no deep dip (" + num(db(hi / lo)) + " dB swing)");

    AudioBuffer music = chords(48000, 4.0);
    normalize_peak(music, -3.0f);
    check(std::fabs(peak_db(music) + 3.0f) < 0.01f, "peak normalize hits -3 dBFS (" + num(peak_db(music), 3) + ")");
    AudioBuffer speech = speech_like(48000, 5.0).with_channels(2);
    normalize_loudness(speech, -16.0f, -1.0f);
    const LoudnessResult lr = measure_loudness(speech);
    check(std::fabs(lr.integrated + 16.0f) < 0.5f && lr.true_peak_db <= -0.9f, "loudness normalize reaches " + num(lr.integrated) + " LUFS with true peak " + num(lr.true_peak_db) + " dBTP");

    AudioBuffer padded = speech_like(48000, 2.0);
    const std::size_t core = padded.frames();
    pad(padded, 1.0, 1.5);
    trim_silence(padded, -60.0f, 10.0);
    const double extra = (static_cast<double>(padded.frames()) - static_cast<double>(core)) / 48.0;
    check(padded.frames() <= core + 960 + 10 && padded.frames() + 48000 / 4 > core, "trim silence removes 2.5 s of padding (" + num(extra, 1) + " ms difference from the voice length)");
}

void test_timeline() {
    std::printf("timeline render with automation\n");
    Timeline tl(48000, 2);
    Track& music = tl.add_track("music");
    music.add_clip(make_clip(tone(220.0, 48000, 2, 4.0, 0.5f), 0.0));
    music.effects->add(std::make_shared<Gain>(0.0f));
    AutomationLane& g = music.lane(AutomationTarget::effect_parameter, 0, "db");
    g.add(0.0, 0.0f);
    g.add(2.0, -20.0f);
    g.add(4.0, -20.0f);
    Track& voice = tl.add_track("voice", 1);
    voice.add_clip(make_clip(tone(1000.0, 48000, 1, 1.0, 0.25f), 1.0));
    AutomationLane& vol = voice.lane(AutomationTarget::volume);
    vol.add(0.0, 0.0f);
    vol.add(1.5, 0.0f);
    vol.add(1.6, -120.0f);
    Track& fx = tl.add_track("fx");
    fx.add_clip(make_clip(tone(500.0, 48000, 2, 0.2, 0.3f), 3.0));
    auto rv = std::make_shared<Reverb>();
    rv->decay.set(1.0f);
    rv->damping.set(0.0f);
    rv->mix.set(0.5f);
    fx.effects->add(rv);
    RenderOptions ro;
    ro.include_tail = true;
    AudioBuffer out = render(tl, ro);
    check(out.frames() > 4 * 48000 && out.duration() < 6.0, "timeline renders 4 s plus the reverb tail (" + num(out.duration()) + " s)");
    std::vector<float> m(out.frames());
    for (std::size_t i = 0; i < out.frames(); ++i) m[i] = out.at(i, 0);
    auto level = [&](double hz, double t) { return db(goertzel(m, static_cast<std::size_t>(t * 48000), 4800, hz, 48000.0)); };
    const double ref = db(0.5);
    double worst = 0.0;
    for (double t = 0.05; t < 2.9; t += 0.25) worst = std::max(worst, std::fabs(level(220.0, t) - ref - g.value_at(t + 0.05)));
    check(worst < 0.3, "effect-parameter automation follows the 0 -> -20 dB ramp on the music track (worst error " + num(worst, 3) + " dB)");
    const double v1 = level(1000.0, 1.2);
    const double v2 = level(1000.0, 1.7);
    check(v1 > -25.0 && v2 < v1 - 60.0, "volume automation mutes the voice track after 1.6 s (" + num(v1) + " -> " + num(v2) + " dB)");
    const double ring = level(500.0, 3.3);
    const double late = level(500.0, 3.8);
    check(ring > -45.0 && late < ring - 10.0 && late > -120.0, "reverb on the fx track rings past the end of its clip and decays (" + num(ring) + " -> " + num(late) + " dB)");
}

void test_codecs() {
    std::printf("codecs\n");
    const AudioBuffer music = chords(44100, 3.0);
    const std::vector<float> ref = mono_of(music);
    for (const std::string& ext : writable_audio_extensions()) {
        const std::string path = temp_path("fizmo_features." + ext);
        SaveOptions so;
        if (ext == "wav") so.wav_format = SampleFormat::f32;
        std::string err;
        const bool saved = save_audio(path, music, so, &err);
        AudioBuffer back;
        AudioFileInfo fi;
        const bool loaded = saved && load_audio(path, back, &err, &fi);
        check(saved && loaded, ext + ": save and load (" + err + ")");
        if (!loaded) continue;
        const long diff = static_cast<long>(back.frames()) - static_cast<long>(music.frames());
        const std::vector<float> got = mono_of(back);
        double worst = 0.0;
        const double hz[] = { 261.63, 329.63, 392.0, 220.0 };
        for (double f : hz) {
            const double a = goertzel(ref, 0, std::min(ref.size(), got.size()) - 4096, f, 44100.0);
            const double b = goertzel(got, 0, std::min(ref.size(), got.size()) - 4096, f, 44100.0);
            worst = std::max(worst, std::fabs(db(b / a)));
        }
        info(ext + ": " + std::to_string(back.frames()) + " frames (" + std::to_string(diff) + "), " + std::to_string(fi.sample_rate) + " Hz, worst tone error " + num(worst, 3) + " dB, SNR " + num(snr_db(music, back)) + " dB");
        check(back.sample_rate() == 44100 && back.channels() == 2, ext + ": keeps rate and channels");
        if (ext == "wav") {
            check(identical(back, music), "wav f32: bit exact round trip");
        } else if (ext == "flac") {
            AudioBuffer err_buf = back;
            for (std::size_t i = 0; i < err_buf.sample_count(); ++i) err_buf.data()[i] -= music.data()[i];
            const double err_db = db(err_buf.rms());
            check(back.frames() == music.frames() && err_db < -90.0, "flac: 16-bit round trip, error only at the 16-bit noise floor (" + num(err_db) + " dBFS)");
        } else {
            check(std::labs(diff) <= 2304 && worst < 1.0, ext + ": lossy round trip keeps length and tones (" + num(worst, 2) + " dB)");
        }
        std::remove(path.c_str());
    }
    const SampleFormat formats[] = { SampleFormat::u8, SampleFormat::s16, SampleFormat::s24, SampleFormat::s32 };
    const double max_err[] = { -50.0, -98.0, -140.0, -140.0 };
    for (int i = 0; i < 4; ++i) {
        const std::string path = temp_path("fizmo_features_fmt.wav");
        SaveOptions so;
        so.wav_format = formats[i];
        so.dither = false;
        AudioBuffer back;
        const bool ok = save_audio(path, music, so) && load_audio(path, back);
        double e = 0.0;
        for (std::size_t k = 0; ok && k < back.sample_count() && k < music.sample_count(); ++k) e += static_cast<double>(back.data()[k] - music.data()[k]) * (back.data()[k] - music.data()[k]);
        const double err_db = ok ? 10.0 * std::log10(e / static_cast<double>(music.sample_count()) + 1e-30) : 0.0;
        check(ok && back.frames() == music.frames() && err_db < max_err[i], std::string("wav ") + format_name(formats[i]) + ": error " + num(err_db) + " dBFS");
        std::remove(path.c_str());
    }
}

class Wobble : public Effect {
public:
    Wobble() : depth("depth", 0.25f, 0.0f, 1.0f), rate_hz("rate_hz", 3.0f, 0.1f, 20.0f, "Hz") {
        add_parameter(depth);
        add_parameter(rate_hz);
    }
    const char* name() const noexcept override { return "wobble"; }
    const char* type() const noexcept override { return "test_wobble"; }
    void prepare(std::uint32_t rate, std::size_t ch) override {
        set_format(rate, ch);
        reset();
    }
    void process(float* x, std::size_t frames) noexcept override {
        const std::size_t c = channels();
        for (std::size_t f = 0; f < frames; ++f) {
            const float g = 1.0f - depth.get() * 0.5f * (1.0f + static_cast<float>(std::sin(m_phase))) * m_shape;
            m_phase += 2.0 * k_pi * rate_hz.get() / sample_rate();
            for (std::size_t ch = 0; ch < c; ++ch) x[f * c + ch] *= g;
        }
    }
    void reset() noexcept override { m_phase = 0.0; }
    std::string save_state() const override {
        JsonValue v = JsonValue::make_object();
        v.set("shape", JsonValue::make_float(m_shape));
        v.set("label", JsonValue::make_string(label));
        return write_json(v, false);
    }
    bool load_state(const std::string& s) override {
        JsonValue v;
        if (!parse_json(s, v) || !v.find("shape")) return false;
        m_shape = v.find("shape")->as_float();
        if (v.find("label")) label = v.find("label")->as_string();
        return true;
    }

    Parameter depth;
    Parameter rate_hz;
    std::string label = "plain";
    float m_shape = 1.0f;

private:
    double m_phase = 0.0;
};

AudioBuffer render_chain(Effect& e, const AudioBuffer& in) {
    AudioBuffer out = in;
    e.prepare(in.sample_rate(), in.channels());
    e.reset();
    const std::size_t tail = e.tail_frames();
    out.resize(in.frames() + std::min<std::size_t>(tail, 48000));
    for (std::size_t pos = 0; pos < out.frames(); pos += 512) e.process(out.frame(pos), std::min<std::size_t>(512, out.frames() - pos));
    return out;
}

void test_presets() {
    std::printf("presets and custom effects\n");
    const std::vector<EffectTypeInfo> types = effect_types();
    const char* expected[] = { "gain", "chain", "preset", "dc_blocker", "filter", "parametric_eq", "graphic_eq", "compressor", "limiter", "noise_gate", "expander", "de_esser", "noise_reduction", "hum_remover", "click_remover", "pitch_shifter", "formant_shifter", "autotune", "voice_eq", "ring_modulator", "vocoder", "voice_changer", "chorus", "flanger", "phaser", "tremolo", "vibrato", "autopan", "delay", "reverb", "distortion", "bitcrusher" };
    bool all_types = true;
    bool all_roundtrip = true;
    bool names_match = true;
    std::string bad;
    for (const char* t : expected) {
        std::shared_ptr<Effect> e = create_effect(t);
        EffectTypeInfo ti;
        if (!e || !effect_type_info(t, ti) || ti.category.empty() || ti.display_name.empty()) {
            all_types = false;
            bad += std::string(" ") + t;
            continue;
        }
        if (std::string(e->type()) != t) {
            names_match = false;
            bad += std::string(" ") + t;
        }
        std::mt19937 rng(static_cast<unsigned>(std::strlen(t)));
        for (Parameter* p : e->parameters()) {
            auto u = [&](auto& gen) { return fizmo_test::random_float(gen, p->minimum(), p->maximum()); };
            p->set(u(rng));
        }
        e->set_enabled(false);
        const std::string json = save_effect(*e);
        std::vector<std::string> warn;
        std::shared_ptr<Effect> back = load_effect(json, nullptr, &warn);
        bool same = back && std::string(back->type()) == t && !back->enabled() && warn.empty() && back->parameters().size() == e->parameters().size();
        for (std::size_t i = 0; same && i < e->parameters().size(); ++i) same = back->parameters()[i]->get() == e->parameters()[i]->get();
        if (!same) {
            all_roundtrip = false;
            bad += std::string(" rt:") + t;
        }
    }
    check(all_types && types.size() >= 32, "registry has all " + std::to_string(sizeof(expected) / sizeof(expected[0])) + " built-in effect types with categories (" + std::to_string(types.size()) + " total)" + bad);
    check(names_match, "every built-in reports its registry type");
    check(all_roundtrip, "every built-in effect saves and loads every parameter exactly");
    std::size_t voice_n = effect_types("voice").size();
    check(voice_n >= 7 && effect_types("dynamics").size() == 5, "effects can be listed by category (" + std::to_string(voice_n) + " voice effects)");

    std::mt19937 rng(99);
    auto bits = [&](auto& gen) { return fizmo_test::random_int<std::uint32_t>(gen, 0, std::numeric_limits<std::uint32_t>::max()); };
    bool floats_exact = true;
    for (int i = 0; i < 200000 && floats_exact; ++i) {
        std::uint32_t u = bits(rng);
        float f;
        std::memcpy(&f, &u, sizeof(f));
        if (!std::isfinite(f)) continue;
        JsonValue v;
        parse_json(write_json(JsonValue::make_float(f), false), v);
        const float g = v.as_float();
        floats_exact = std::memcmp(&f, &g, sizeof(f)) == 0;
        if (!floats_exact) info("float mismatch for " + num(f, 12));
    }
    bool doubles_exact = true;
    auto ud = [&](auto& gen) { return fizmo_test::random_real(gen, -1e6, 1e6); };
    for (int i = 0; i < 20000 && doubles_exact; ++i) {
        const double d = ud(rng) * std::pow(10.0, static_cast<int>(bits(rng) % 40) - 20);
        JsonValue v;
        parse_json(write_json(JsonValue::make_number(d), false), v);
        doubles_exact = v.as_number() == d;
    }
    check(floats_exact && doubles_exact, "floats and doubles round-trip exactly through JSON text");
    JsonValue tricky;
    const bool parsed = parse_json("{\"s\":\"a\\u00e9\\ud83d\\ude00\\n\\\"q\\\"\",\"n\":[-0.5e-3,1E2,0],\"t\":true,\"z\":null}", tricky);
    check(parsed && tricky.find("s")->as_string() == "a\xc3\xa9\xf0\x9f\x98\x80\n\"q\"" && tricky.find("n")->at(0).as_number() == -0.0005 && tricky.find("t")->as_bool() && tricky.find("z")->is_null(), "JSON reader handles escapes, surrogate pairs, exponents and literals");
    JsonValue reparsed;
    check(parse_json(write_json(tricky), reparsed) && write_json(reparsed) == write_json(tricky), "JSON writer output parses back to the same document");
    std::string err;
    JsonValue junk;
    check(!parse_json("{\"a\": [1, 2,, 3]}", junk, &err) && err.find("line 1") != std::string::npos, "malformed JSON is rejected with a position (" + err + ")");
    std::string deep(5000, '[');
    check(!parse_json(deep, junk, &err), "deeply nested JSON is rejected instead of overflowing the stack");

    const AudioBuffer noise_sample = [] {
        AudioBuffer b(1, 48000, 24000);
        std::mt19937 r(3);
        auto n = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 0.02f); };
        for (float& v : b.samples()) v = n(r);
        return b;
    }();
    AudioBuffer input = speech_like(48000, 3.0, 150.0, 21);
    {
        std::mt19937 r(4);
        auto n = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 0.02f); };
        for (float& v : input.samples()) v += n(r);
    }
    auto build = [&]() {
        auto chain = std::make_shared<EffectChain>();
        auto nr = std::make_shared<NoiseReduction>();
        nr->learn_profile(noise_sample);
        nr->reduction_db.set(20.0f);
        chain->add(nr);
        auto gate = std::make_shared<NoiseGate>(-50.0f);
        chain->add(gate);
        auto peq = std::make_shared<ParametricEq>(5);
        peq->set_band(0, FilterType::highpass, 90.0f, 0.707f, 0.0f);
        peq->set_band(3, FilterType::peak, 2500.0f, 1.3f, 4.5f);
        peq->set_band(4, FilterType::highshelf, 9000.0f, 0.7f, -3.25f);
        chain->add(peq);
        chain->add(std::make_shared<Compressor>(-24.0f, 3.0f));
        auto nested = std::make_shared<EffectChain>();
        auto ch = std::make_shared<Chorus>();
        ch->mix.set(0.33333334f);
        nested->add(ch);
        auto dl = std::make_shared<Delay>();
        dl->time_ms.set(123.456f);
        nested->add(dl);
        auto off = std::make_shared<Reverb>();
        off->set_enabled(false);
        nested->add(off);
        chain->add(nested);
        auto ps = std::make_shared<PitchShifter>(3.7f);
        chain->add(ps);
        chain->add(std::make_shared<VoiceChanger>(VoicePreset::robot));
        auto w = std::make_shared<Wobble>();
        w->depth.set(0.6f);
        w->m_shape = 0.75f;
        w->label = "custom \"quoted\" label";
        chain->add(w);
        chain->add(std::make_shared<Limiter>(-1.5f));
        return chain;
    };
    check(register_effect("test_wobble", [] { return std::make_shared<Wobble>(); }, "Wobble", "custom"), "custom effect type registers");
    check(has_effect_type("test_wobble") && effect_types("custom").size() == 1, "custom type is listed in its category");
    std::shared_ptr<EffectChain> original = build();
    PresetInfo pinfo;
    pinfo.name = "Robot Radio \xe2\x9c\x93";
    pinfo.author = "Christian";
    pinfo.description = "line one\nline two";
    pinfo.category = "voice";
    pinfo.tags = { "robot", "fun" };
    pinfo.version = "1.2";
    const std::string json = save_preset(*original, pinfo);
    const std::string path = temp_path("fizmo_features_preset.json");
    check(save_preset_file(path, *original, pinfo, &err), "preset saves to a file");
    EffectChain loaded;
    PresetInfo linfo;
    std::vector<std::string> warnings;
    const bool ok = load_preset_file(path, loaded, &linfo, &err, &warnings);
    check(ok && warnings.empty() && loaded.size() == original->size(), "preset loads back with no warnings (" + std::to_string(loaded.size()) + " effects)");
    for (const std::string& w : warnings) info("warning: " + w);
    check(linfo.name == pinfo.name && linfo.author == pinfo.author && linfo.description == pinfo.description && linfo.tags == pinfo.tags && linfo.version == "1.2" && linfo.category == "voice", "preset info (name, author, description, tags, version) round-trips");
    std::shared_ptr<EffectChain> nested_back = std::dynamic_pointer_cast<EffectChain>(loaded.at(4));
    check(nested_back && nested_back->size() == 3 && !nested_back->at(2)->enabled() && nested_back->at(1)->get("time_ms") == 123.456f, "nested chain, enabled flags and parameters survive");
    std::shared_ptr<ParametricEq> peq_back = std::dynamic_pointer_cast<ParametricEq>(loaded.at(2));
    check(peq_back && peq_back->band_count() == 5 && peq_back->band(4).gain.get() == -3.25f, "parametric EQ band count state survives");
    std::shared_ptr<NoiseReduction> nr_back = std::dynamic_pointer_cast<NoiseReduction>(loaded.at(0));
    std::shared_ptr<NoiseReduction> nr_orig = std::dynamic_pointer_cast<NoiseReduction>(original->at(0));
    check(nr_back && nr_back->has_profile() && nr_back->profile_spectrum() == nr_orig->profile_spectrum(), "noise reduction learned profile survives exactly");
    std::shared_ptr<Wobble> w_back = std::dynamic_pointer_cast<Wobble>(loaded.at(7));
    check(w_back && w_back->m_shape == 0.75f && w_back->label == "custom \"quoted\" label" && w_back->depth.get() == 0.6f, "custom registered effect keeps its parameters and state");
    const AudioBuffer out_a = render_chain(*original, input);
    const AudioBuffer out_b = render_chain(loaded, input);
    check(identical(out_a, out_b), "chain -> JSON -> chain renders bit-identical audio (" + std::to_string(out_a.frames()) + " frames, max diff " + num(max_abs_diff(out_a, out_b), 9) + ")");
    check(save_preset(loaded, linfo) == json, "re-saving a loaded preset reproduces the same JSON text");
    std::remove(path.c_str());

    std::string edited = json;
    const std::string target = "\"type\": \"test_wobble\"";
    const std::size_t at = edited.find(target);
    if (at != std::string::npos) edited.replace(at, target.size(), "\"type\": \"someone_elses_effect\"");
    const std::string pname = "\"threshold\": -24";
    const std::size_t pat = edited.find(pname);
    if (pat != std::string::npos) edited.replace(pat, pname.size(), "\"threshhold\": -24");
    EffectChain partial;
    warnings.clear();
    const bool partial_ok = load_preset(edited, partial, nullptr, &err, &warnings);
    bool saw_type = false;
    bool saw_param = false;
    for (const std::string& w : warnings) {
        saw_type = saw_type || w.find("someone_elses_effect") != std::string::npos;
        saw_param = saw_param || w.find("threshhold") != std::string::npos;
    }
    check(partial_ok && saw_type && saw_param && partial.size() == original->size(), "unknown effect types and parameters only warn (" + std::to_string(warnings.size()) + " warnings)");
    std::shared_ptr<MissingEffect> missing = std::dynamic_pointer_cast<MissingEffect>(partial.at(7));
    check(missing && missing->missing_type() == "someone_elses_effect" && save_preset(partial).find("someone_elses_effect") != std::string::npos, "unknown effects are kept as pass-through placeholders and re-saved intact");
    EffectChain untouched;
    untouched.add(std::make_shared<Gain>(-6.0f));
    check(!load_preset("{\"format\":\"fizmo-audio-preset\",\"chain\":", untouched, nullptr, &err) && untouched.size() == 1 && !err.empty(), "a broken preset fails cleanly and leaves the chain alone (" + err + ")");
    check(!load_preset("{\"format\":\"something-else\",\"chain\":{}}", untouched, nullptr, &err), "files of another format are rejected");

    const std::vector<std::string> names = builtin_voice_preset_names();
    check(names.size() == voice_preset_count() && names[1] == "helium", "built-in voice presets are listed by name (" + std::to_string(names.size()) + ")");
    bool presets_identical = true;
    std::string failing;
    const AudioBuffer speech = speech_like(48000, 2.0, 140.0, 8);
    for (std::size_t i = 0; i < voice_preset_count(); ++i) {
        const VoicePreset vp = static_cast<VoicePreset>(i);
        std::shared_ptr<EffectChain> a = builtin_voice_preset(vp);
        std::shared_ptr<EffectChain> b = std::make_shared<EffectChain>();
        PresetInfo bi;
        const bool lok = load_preset(builtin_voice_preset_json(vp), *b, &bi, &err);
        if (!lok || !a || bi.name != names[i] || !identical(render_chain(*a, speech), render_chain(*b, speech))) {
            presets_identical = false;
            failing += " " + names[i];
        }
    }
    check(presets_identical, "every built-in voice preset is an editable chain that round-trips through JSON identically" + failing);
    std::shared_ptr<EffectChain> by_name = builtin_voice_preset("monster");
    check(by_name && by_name->size() >= 3, "built-in presets can be fetched by name (monster has " + std::to_string(by_name ? by_name->size() : 0) + " effects)");

    PresetEffect pe(builtin_voice_preset(VoicePreset::helium));
    pe.prepare(48000, 1);
    AudioBuffer live = speech_like(48000, 3.0, 140.0, 12);
    float max_step_switch = 0.0f;
    float prev = 0.0f;
    bool saw_fade = false;
    for (std::size_t pos = 0; pos < live.frames(); pos += 256) {
        if (pos == 48000 * 3 / 2 - (48000 * 3 / 2) % 256) pe.set_chain(builtin_voice_preset(VoicePreset::deep));
        const std::size_t n = std::min<std::size_t>(256, live.frames() - pos);
        pe.process(live.frame(pos), n);
        saw_fade = saw_fade || pe.crossfading();
        for (std::size_t i = 0; i < n; ++i) {
            max_step_switch = std::max(max_step_switch, std::fabs(live.at(pos + i, 0) - prev));
            prev = live.at(pos + i, 0);
        }
    }
    const double p_before = median_pitch(live, 0.3, 1.4);
    const double p_after = median_pitch(live, 1.7, 3.0);
    check(saw_fade && p_before > 190.0 && p_after < 115.0 && max_step_switch < 0.5f, "PresetEffect swaps helium -> deep with a crossfade (" + num(p_before) + " -> " + num(p_after) + " Hz, max step " + num(max_step_switch, 3) + ")");
    PresetEffect dry_only(builtin_voice_preset(VoicePreset::helium));
    dry_only.mix.set(0.0f);
    const AudioBuffer dry_in = speech_like(48000, 1.0);
    AudioBuffer dry_out = applied(dry_only, dry_in, false);
    check(identical(dry_out, dry_in), "PresetEffect mix=0 returns the latency-aligned dry signal exactly");
    PresetEffect saver(original);
    saver.set_info(pinfo);
    auto outer = std::make_shared<EffectChain>();
    outer->add(std::make_shared<Gain>(-2.0f));
    outer->add(std::shared_ptr<Effect>(new PresetEffect(original)));
    const std::string outer_json = save_preset(*outer);
    EffectChain outer_back;
    warnings.clear();
    check(load_preset(outer_json, outer_back, nullptr, &err, &warnings) && warnings.empty(), "a preset used as one effect inside another chain saves and loads");
    std::shared_ptr<PresetEffect> inner = std::dynamic_pointer_cast<PresetEffect>(outer_back.at(1));
    check(inner && inner->chain() && inner->chain()->size() == original->size(), "the nested preset keeps its whole chain");
    PresetEffect from_text;
    check(from_text.load(json, &err) && from_text.info().name == pinfo.name && from_text.chain()->size() == original->size(), "PresetEffect loads a shared preset file's text directly");

    std::atomic<bool> stop{ false };
    PresetEffect threaded(builtin_voice_preset(VoicePreset::robot));
    threaded.prepare(48000, 2);
    std::thread swapper([&]() {
        for (int i = 0; i < 40 && !stop.load(); ++i) {
            threaded.set_chain(builtin_voice_preset(static_cast<VoicePreset>(1 + i % (voice_preset_count() - 1))));
            threaded.mix.set(static_cast<float>(i % 5) / 4.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    std::vector<float> block(256 * 2);
    bool finite = true;
    for (int i = 0; i < 1500; ++i) {
        for (std::size_t k = 0; k < block.size(); ++k) block[k] = 0.2f * static_cast<float>(std::sin(0.03 * static_cast<double>(i * 256 + static_cast<int>(k / 2))));
        threaded.process(block.data(), 256);
        for (float v : block) finite = finite && std::isfinite(v);
    }
    stop.store(true);
    swapper.join();
    check(finite, "set_chain from another thread while processing stays finite");
    unregister_effect("test_wobble");
    check(!has_effect_type("test_wobble"), "custom effect types can be unregistered");
}

void render_blocks(Mixer& mixer, std::vector<float>* capture, double seconds, Mixer::VoiceId pace = 0) {
    const std::size_t total = static_cast<std::size_t>(seconds * mixer.sample_rate());
    std::vector<float> block(512 * mixer.channels());
    std::size_t done = 0;
    while (done < total) {
        if (pace) {
            for (int w = 0; w < 50 && mixer.is_playing(pace) && mixer.buffered_seconds(pace) < 0.3; ++w) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const std::size_t n = std::min<std::size_t>(512, total - done);
        g_in_render = true;
        mixer.render(block.data(), n);
        g_in_render = false;
        if (capture) capture->insert(capture->end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(n * mixer.channels()));
        done += n;
    }
}

AudioBuffer as_buffer(const std::vector<float>& v, std::size_t channels, std::uint32_t rate) {
    return AudioBuffer(channels, rate, v);
}

void test_mixer_stretch() {
    std::printf("mixer speed / pitch playback\n");
    Mixer mixer(48000, 2);
    const AudioBuffer t = tone(220.0, 44100, 1, 4.0, 0.4f);
    g_render_allocs.store(0);
    PlayOptions po;
    po.speed = 1.5f;
    po.preserve_pitch = true;
    Mixer::VoiceId id = mixer.play(t, po);
    check(mixer.is_stretched(id), "preserve_pitch allocates a stretcher for the voice");
    std::vector<float> cap;
    render_blocks(mixer, &cap, 1.0);
    const double pos1 = mixer.position(id);
    AudioBuffer out = as_buffer(cap, 2, 48000);
    const double p = median_pitch(out, 0.2, 1.0);
    check(std::fabs(p - 220.0) < 2.5, "speed 1.5 with preserve_pitch keeps 220 Hz (" + num(p) + ")");
    check(std::fabs(pos1 - 1.5) < 0.05, "position advances 1.5 s per second (" + num(pos1, 3) + ")");
    render_blocks(mixer, &cap, 2.5);
    check(!mixer.is_playing(id), "the voice finishes after about 4 / 1.5 s");
    out = as_buffer(cap, 2, 48000);
    std::size_t last = 0;
    for (std::size_t i = 0; i < out.frames(); ++i) {
        if (std::fabs(out.at(i, 0)) > 0.01f) last = i;
    }
    check(std::fabs(static_cast<double>(last) / 48000.0 - 4.0 / 1.5) < 0.06, "stretched playback lasts " + num(last / 48000.0, 3) + " s (want 2.667)");

    PlayOptions tape;
    tape.speed = 1.5f;
    id = mixer.play(t, tape);
    cap.clear();
    render_blocks(mixer, &cap, 1.0);
    check(!mixer.is_stretched(id) && std::fabs(median_pitch(as_buffer(cap, 2, 48000), 0.2, 1.0) - 330.0) < 3.0, "default speed 1.5 is still tape-style (330 Hz)");
    mixer.stop(id);

    PlayOptions up;
    up.pitch = 7.0f;
    id = mixer.play(t, up);
    cap.clear();
    render_blocks(mixer, &cap, 1.0);
    const double p7 = median_pitch(as_buffer(cap, 2, 48000), 0.2, 1.0);
    check(std::fabs(p7 - 220.0 * std::pow(2.0, 7.0 / 12.0)) < 3.0 && std::fabs(mixer.position(id) - 1.0) < 0.05, "pitch +7 st plays at " + num(p7) + " Hz at normal speed");
    mixer.set_pitch(id, -12.0f);
    cap.clear();
    render_blocks(mixer, &cap, 1.0);
    check(std::fabs(median_pitch(as_buffer(cap, 2, 48000), 0.3, 1.0) - 110.0) < 2.0, "set_pitch changes pitch live (110 Hz)");
    mixer.seek(id, 0.5);
    render_blocks(mixer, nullptr, 0.1);
    check(std::fabs(mixer.position(id) - 0.6) < 0.03, "seek works on stretched voices (" + num(mixer.position(id), 3) + ")");
    mixer.stop(id);
    PlayOptions lp;
    lp.loop = true;
    lp.preserve_pitch = true;
    lp.speed = 2.0f;
    id = mixer.play(t, lp);
    render_blocks(mixer, nullptr, 3.0);
    check(mixer.is_playing(id) && std::fabs(mixer.position(id) - 2.0) < 0.06, "looping stretched voice wraps its position (" + num(mixer.position(id), 3) + ")");
    mixer.stop_all();
    render_blocks(mixer, nullptr, 0.05);
#if defined(FIZMO_TEST_COUNT_ALLOCS)
    check(g_render_allocs.load() == 0, "render did not allocate with stretched voices (" + std::to_string(g_render_allocs.load()) + ")");
#endif // FIZMO_TEST_COUNT_ALLOCS
}

void test_mixer_streaming(bool quick) {
    std::printf("mixer streaming from files\n");
    const double length = quick ? 60.0 : 600.0;
    const std::uint32_t rate = 44100;
    const std::string path = temp_path(quick ? "fizmo_features_stream_short.flac" : "fizmo_features_stream_10min.flac");
    {
        AudioBuffer chirp(1, rate, static_cast<std::size_t>(length * rate));
        for (std::size_t i = 0; i < chirp.frames(); ++i) {
            const double t = static_cast<double>(i) / rate;
            chirp.at(i, 0) = static_cast<float>(0.5 * std::sin(2.0 * k_pi * (200.0 * t + 0.5 * t * t)));
        }
        SaveOptions so;
        so.flac_compression = 2;
        const auto t0 = std::chrono::steady_clock::now();
        check(save_audio(path, chirp, so), "generated a " + num(length / 60.0, 0) + "-minute FLAC chirp");
        info("encoded in " + num(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count()) + " s");
    }
    auto freq_at = [](double t) { return 200.0 + t; };
    Mixer mixer(48000, 2);
    std::string err;
    g_render_allocs.store(0);
    Mixer::VoiceId id = mixer.play_file(path, PlayOptions(), &err);
    check(id != 0 && mixer.is_streaming(id), "play_file starts a streaming voice (" + err + ")");
    check(std::fabs(mixer.duration(id) - length) < 0.01, "duration is known without decoding (" + num(mixer.duration(id), 3) + " s)");
    check(mixer.buffered_seconds(id) > 0.4, "stream is prefilled before playback (" + num(mixer.buffered_seconds(id), 3) + " s)");
    std::vector<float> cap;
    render_blocks(mixer, &cap, 2.0, id);
    double p = median_pitch(as_buffer(cap, 2, 48000), 1.5, 2.0, 100.0f, 1500.0f);
    check(std::fabs(p - freq_at(1.75)) < 2.0 && std::fabs(mixer.position(id) - 2.0) < 0.02, "start plays the right audio (" + num(p) + " Hz at " + num(mixer.position(id), 3) + " s)");
    const double seek_to = length / 2.0;
    mixer.seek(id, seek_to);
    cap.clear();
    render_blocks(mixer, &cap, 1.0, id);
    p = median_pitch(as_buffer(cap, 2, 48000), 0.5, 1.0, 100.0f, 1500.0f);
    const double here = mixer.position(id);
    check(std::fabs(p - freq_at(here - 0.25)) < 3.0 && here > seek_to + 0.5 && here < seek_to + 1.01, "seek to " + num(seek_to, 0) + " s plays from there (" + num(p) + " Hz, position " + num(here, 3) + ")");
    mixer.set_speed(id, 2.0f);
    cap.clear();
    render_blocks(mixer, &cap, 1.0, id);
    const double after_speed = mixer.position(id);
    p = median_pitch(as_buffer(cap, 2, 48000), 0.5, 1.0, 100.0f, 2500.0f);
    check(std::fabs(after_speed - here - 2.0) < 0.05 && std::fabs(p - 2.0 * freq_at(after_speed - 0.5)) < 8.0, "speed 2 on a stream doubles rate and pitch (" + num(p) + " Hz, +" + num(after_speed - here, 3) + " s)");
    mixer.set_speed(id, 1.0f);
    mixer.set_paused(id, true);
    cap.clear();
    render_blocks(mixer, &cap, 0.5);
    const double paused_pos = mixer.position(id);
    check(as_buffer(cap, 2, 48000).peak() == 0.0f && std::fabs(paused_pos - after_speed) < 1e-9 && mixer.is_paused(id), "pause holds position and outputs silence");
    mixer.set_paused(id, false);
    mixer.seek(id, length - 2.0);
    cap.clear();
    std::size_t blocks = 0;
    while (mixer.is_playing(id) && blocks < 2000) {
        render_blocks(mixer, &cap, 512.0 / 48000.0, id);
        ++blocks;
    }
    std::size_t last = 0;
    std::size_t first = 0;
    bool any = false;
    const AudioBuffer tail = as_buffer(cap, 2, 48000);
    for (std::size_t i = 0; i < tail.frames(); ++i) {
        if (std::fabs(tail.at(i, 0)) > 0.01f) {
            if (!any) first = i;
            any = true;
            last = i;
        }
    }
    const double played = static_cast<double>(last - first) / 48000.0;
    check(!mixer.is_playing(id) && any && std::fabs(played - 2.0) < 0.02 && first < 24000, "stream ends at end of file (" + num(played, 3) + " s played after seeking 2 s before the end, started after " + num(first / 48.0, 1) + " ms)");
    check(mixer.stream_underruns(id) == 0, "no underruns while paced like a sound card");

    const auto t0 = std::chrono::steady_clock::now();
    id = mixer.play_file(path, PlayOptions(), &err);
    std::size_t frames = 0;
    std::vector<float> block(512 * 2);
    double max_jump = 0.0;
    float prev = 0.0f;
    bool started = false;
    while (mixer.is_playing(id) && frames < static_cast<std::size_t>((length + 5.0) * 48000.0)) {
        for (int w = 0; w < 50 && mixer.buffered_seconds(id) < 0.3 && mixer.is_playing(id); ++w) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g_in_render = true;
        mixer.render(block.data(), 512);
        g_in_render = false;
        for (std::size_t i = 0; i < 512; ++i) {
            const float v = block[i * 2];
            if (started && frames + i < static_cast<std::size_t>((length - 0.01) * 48000.0)) max_jump = std::max(max_jump, static_cast<double>(std::fabs(v - prev)));
            prev = v;
            started = true;
        }
        frames += 512;
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double expected_jump = 0.5 * 2.0 * k_pi * freq_at(length) / 48000.0;
    check(std::fabs(static_cast<double>(frames) - length * 48000.0) < 1024.0, "whole " + num(length, 0) + " s file streams through (" + std::to_string(frames) + " frames at 48 kHz from 44.1 kHz mono, " + num(length / wall, 0) + "x real time)");
    check(max_jump < expected_jump * 1.1 && mixer.stream_underruns(id) == 0, "no gaps or glitches across the whole stream (max step " + num(max_jump, 4) + " vs " + num(expected_jump, 4) + ")");

    PlayOptions lo;
    lo.loop = true;
    lo.start = length - 1.0;
    id = mixer.play_file(path, lo, &err);
    render_blocks(mixer, nullptr, 2.0, id);
    check(mixer.is_playing(id) && std::fabs(mixer.position(id) - 1.0) < 0.05, "looping stream wraps to the start (" + num(mixer.position(id), 3) + ")");
    mixer.stop(id);

    PlayOptions pp;
    pp.preserve_pitch = true;
    pp.speed = 1.5f;
    const double pp_start = std::min(100.0, length / 3.0);
    pp.start = pp_start;
    id = mixer.play_file(path, pp, &err);
    cap.clear();
    render_blocks(mixer, &cap, 1.0, id);
    const double pos_pp = mixer.position(id);
    p = median_pitch(as_buffer(cap, 2, 48000), 0.6, 1.0, 100.0f, 1500.0f);
    check(std::fabs(pos_pp - pp_start - 1.5) < 0.06 && std::fabs(p - freq_at(pos_pp - 0.3)) < 4.0, "preserve_pitch on a stream: 1.5x tempo, pitch stays " + num(p) + " Hz at " + num(pos_pp, 3) + " s");
    mixer.set_preserve_pitch(id, false);
    cap.clear();
    render_blocks(mixer, &cap, 1.0, id);
    p = median_pitch(as_buffer(cap, 2, 48000), 0.6, 1.0, 100.0f, 2000.0f);
    check(std::fabs(p - 1.5 * freq_at(mixer.position(id) - 0.3)) < 8.0, "turning preserve_pitch off gives tape speed again (" + num(p) + " Hz)");
    mixer.stop(id);

    std::vector<Mixer::VoiceId> many;
    for (int i = 0; i < 8; ++i) {
        PlayOptions o;
        o.start = length / 10.0 * i;
        o.gain = 0.1f;
        many.push_back(mixer.play_file(path, o, &err));
    }
    render_blocks(mixer, nullptr, 2.0, many.back());
    bool all_ok = true;
    for (int i = 0; i < 8; ++i) all_ok = all_ok && std::fabs(mixer.position(many[static_cast<std::size_t>(i)]) - (length / 10.0 * i + 2.0)) < 0.05;
    check(all_ok, "eight simultaneous streams (soundboard) each keep their own position");
    for (Mixer::VoiceId v : many) mixer.seek(v, 1.0);
    render_blocks(mixer, nullptr, 0.5, many.back());
    mixer.stop_all();
    render_blocks(mixer, nullptr, 0.05);
#if defined(FIZMO_TEST_COUNT_ALLOCS)
    check(g_render_allocs.load() == 0, "streaming render path did not allocate (" + std::to_string(g_render_allocs.load()) + ")");
#endif // FIZMO_TEST_COUNT_ALLOCS
    std::string bad_err;
    check(mixer.play_file(temp_path("fizmo_no_such_file.flac"), PlayOptions(), &bad_err) == 0 && !bad_err.empty(), "missing files report an error");
    if (!quick) std::remove(path.c_str());
}

bool want(int argc, char** argv, const char* name) {
    if (argc < 2) return true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    const bool quick = std::getenv("FIZMO_QUICK") != nullptr;
    if (want(argc, argv, "reverse")) test_reverse();
    if (want(argc, argv, "reverb")) test_reverb_delay();
    if (want(argc, argv, "voice")) test_voice_changer();
    if (want(argc, argv, "stretch")) test_pitch_and_stretch();
    if (want(argc, argv, "eq")) test_eq_dynamics();
    if (want(argc, argv, "noise")) test_noise_reduction();
    if (want(argc, argv, "edit")) test_edits();
    if (want(argc, argv, "timeline")) test_timeline();
    if (want(argc, argv, "codecs")) test_codecs();
    if (want(argc, argv, "presets")) test_presets();
    if (want(argc, argv, "mixer")) test_mixer_stretch();
    if (want(argc, argv, "stream")) test_mixer_streaming(quick);
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
