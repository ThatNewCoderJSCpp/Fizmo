#include "../Source/Audio/audio.hpp"

#include "portable_random.hpp"

#include <complex>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <stdexcept>
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

std::string temp_path(const std::string& name) {
    const char* dir = std::getenv("TMPDIR");
#if defined(_WIN32)
    if (!dir) dir = std::getenv("TEMP");
#endif
    std::string base = dir ? dir : "/tmp";
    return base + "/" + name;
}

AudioBuffer sine(double freq, std::uint32_t rate, std::size_t channels, std::size_t frames, float amp = 0.5f) {
    AudioBuffer b(channels, rate, frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const float v = amp * static_cast<float>(std::sin(2.0 * k_pi * freq * static_cast<double>(i) / rate));
        for (std::size_t c = 0; c < channels; ++c) b.at(i, c) = v;
    }
    return b;
}

double goertzel(const float* data, std::size_t frames, std::size_t stride, double freq, double rate) {
    const double w = 2.0 * k_pi * freq / rate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        const double s = data[i * stride] + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::sqrt(std::max(0.0, power)) / static_cast<double>(frames);
}

double dominant_frequency(const std::vector<float>& mono, double rate, double lo, double hi) {
    double best = 0.0;
    double best_f = 0.0;
    for (double f = lo; f <= hi; f += 5.0) {
        const double g = goertzel(mono.data(), mono.size(), 1, f, rate);
        if (g > best) {
            best = g;
            best_f = f;
        }
    }
    return best_f;
}

double snr_db(const std::vector<float>& got, const std::vector<float>& want) {
    double sig = 0.0;
    double err = 0.0;
    const std::size_t n = std::min(got.size(), want.size());
    for (std::size_t i = 0; i < n; ++i) {
        sig += static_cast<double>(want[i]) * want[i];
        const double e = static_cast<double>(got[i]) - want[i];
        err += e * e;
    }
    if (err <= 0.0) return 300.0;
    return 10.0 * std::log10(sig / err);
}

void test_formats() {
    std::vector<float> src;
    for (int i = -1000; i <= 1000; ++i) src.push_back(static_cast<float>(i) / 1000.0f);
    src.push_back(1.5f);
    src.push_back(-1.5f);
    struct Case { SampleFormat f; float tol; };
    const Case cases[] = { { SampleFormat::u8, 1.0f / 128.0f }, { SampleFormat::s16, 1.0f / 32768.0f }, { SampleFormat::s24, 1.0f / 8388608.0f }, { SampleFormat::s32, 1e-7f }, { SampleFormat::f32, 0.0f }, { SampleFormat::f64, 0.0f } };
    for (const Case& c : cases) {
        std::vector<std::uint8_t> raw(src.size() * bytes_per_sample(c.f));
        from_float(src.data(), raw.data(), c.f, src.size());
        std::vector<float> back(src.size());
        to_float(raw.data(), c.f, back.data(), src.size());
        float worst = 0.0f;
        for (std::size_t i = 0; i + 2 < src.size(); ++i) {
            if (src[i] > 0.99f) continue;
            worst = std::max(worst, std::fabs(back[i] - src[i]));
        }
        const bool clipped = is_float_format(c.f) || (back[src.size() - 2] <= 1.0f && back[src.size() - 1] >= -1.0f && back[src.size() - 2] > 0.99f);
        check(worst <= c.tol * 0.51f + 1e-9f && clipped, std::string("round trip through ") + format_name(c.f) + " (worst " + std::to_string(worst) + ")");
    }
    std::int16_t s;
    const float one = 1.0f;
    from_float(&one, &s, SampleFormat::s16, 1);
    check(s == 32767, "1.0 becomes 32767 in s16");
    const float minus = -1.0f;
    from_float(&minus, &s, SampleFormat::s16, 1);
    check(s == -32768, "-1.0 becomes -32768 in s16");
    float worst_a = 0.0f;
    float worst_u = 0.0f;
    for (int i = -100; i <= 100; ++i) {
        const float v = static_cast<float>(i) / 100.0f;
        worst_a = std::max(worst_a, std::fabs(alaw_to_float(float_to_alaw(v)) - v) / std::max(0.02f, std::fabs(v)));
        worst_u = std::max(worst_u, std::fabs(ulaw_to_float(float_to_ulaw(v)) - v) / std::max(0.02f, std::fabs(v)));
    }
    check(worst_a < 0.07f, "A-law keeps values within its step size");
    check(worst_u < 0.07f, "mu-law keeps values within its step size");
    check(float_to_ulaw(0.0f) == 0xFF && float_to_alaw(0.0f) == 0xD5, "G.711 encodes silence the standard way");
    bool every_code = true;
    for (int code = 0; code < 256; ++code) {
        if (float_to_alaw(alaw_to_float(static_cast<std::uint8_t>(code))) != code) every_code = false;
    }
    check(every_code, "every A-law code decodes and re-encodes to itself");
    every_code = true;
    for (int code = 0; code < 256; ++code) {
        if (code == 0x7F) continue;
        if (float_to_ulaw(ulaw_to_float(static_cast<std::uint8_t>(code))) != code) every_code = false;
    }
    check(every_code, "every mu-law code decodes and re-encodes to itself");

    float stereo[4] = { 0.2f, 0.4f, -0.2f, 0.6f };
    float mono[2];
    remix(stereo, 2, mono, 1, 2);
    check(std::fabs(mono[0] - 0.3f) < 1e-6f && std::fabs(mono[1] - 0.2f) < 1e-6f, "stereo to mono averages");
    float up[4];
    float m2[2] = { 0.5f, -0.25f };
    remix(m2, 1, up, 2, 2);
    check(up[0] == 0.5f && up[1] == 0.5f && up[2] == -0.25f && up[3] == -0.25f, "mono to stereo copies");
    float six[6] = { 0.1f, 0.2f, 0.4f, 0.9f, 0.1f, 0.3f };
    float two[2];
    remix(six, 6, two, 2, 1);
    check(std::fabs(two[0] - (0.1f + 0.70710678f * 0.4f + 0.70710678f * 0.1f)) < 1e-5f && std::fabs(two[1] - (0.2f + 0.70710678f * 0.4f + 0.70710678f * 0.3f)) < 1e-5f, "5.1 down to stereo drops the LFE and folds in the rest");
    check(std::fabs(db_to_gain(-6.0206f) - 0.5f) < 1e-4f && std::fabs(gain_to_db(2.0f) - 6.0206f) < 1e-3f, "decibel conversions");
}

void test_resampler() {
    const std::size_t n = 48000;
    const AudioBuffer in = sine(1000.0, 44100, 2, n, 0.5f);
    const AudioBuffer out = resample(in, 48000);
    const std::size_t expected = static_cast<std::size_t>(std::ceil(n * 48000.0 / 44100.0));
    check(out.frames() == expected && out.channels() == 2 && out.sample_rate() == 48000, "44.1 kHz to 48 kHz gives the right length");
    const AudioBuffer ref = sine(1000.0, 48000, 2, out.frames(), 0.5f);
    std::vector<float> got(out.samples().begin() + 2000, out.samples().end() - 2000);
    std::vector<float> want(ref.samples().begin() + 2000, ref.samples().end() - 2000);
    const double snr = snr_db(got, want);
    check(snr > 90.0, "44.1 kHz to 48 kHz is clean (SNR " + std::to_string(snr) + " dB)");

    const AudioBuffer high = sine(6500.0, 48000, 1, 48000, 0.5f);
    const AudioBuffer low = resample(high, 16000);
    const AudioBuffer low_ref = sine(6500.0, 16000, 1, low.frames(), 0.5f);
    std::vector<float> g2(low.samples().begin() + 500, low.samples().end() - 500);
    std::vector<float> w2(low_ref.samples().begin() + 500, low_ref.samples().end() - 500);
    check(snr_db(g2, w2) > 60.0, "a 6.5 kHz tone survives going down to 16 kHz (SNR " + std::to_string(snr_db(g2, w2)) + " dB)");
    const AudioBuffer alias = resample(sine(10000.0, 48000, 1, 48000, 0.5f), 16000);
    std::vector<float> a(alias.samples().begin() + 500, alias.samples().end() - 500);
    const double leak = goertzel(a.data(), a.size(), 1, 6000.0, 16000.0);
    check(leak < 0.25 * 1e-3, "a 10 kHz tone doesn't alias into 16 kHz audio (" + std::to_string(20.0 * std::log10(leak / 0.25 + 1e-30)) + " dB)");

    Resampler streaming(2, 44100.0, 48000.0);
    std::vector<float> chunked;
    std::mt19937 rng(7);
    std::size_t pos = 0;
    while (pos < n) {
        const std::size_t take = std::min<std::size_t>(n - pos, 1 + rng() % 1500);
        streaming.process(in.frame(pos), take, chunked);
        pos += take;
    }
    streaming.flush(chunked);
    check(chunked == out.samples(), "feeding the resampler in random pieces gives exactly the same output");

    for (ResampleQuality q : { ResampleQuality::fast, ResampleQuality::medium, ResampleQuality::best }) {
        const AudioBuffer r = resample(sine(440.0, 22050, 1, 22050, 0.5f), 96000, q);
        const AudioBuffer rr = sine(440.0, 96000, 1, r.frames(), 0.5f);
        std::vector<float> g(r.samples().begin() + 8000, r.samples().end() - 8000);
        std::vector<float> w(rr.samples().begin() + 8000, rr.samples().end() - 8000);
        const double s = snr_db(g, w);
        const double need = q == ResampleQuality::fast ? 40.0 : (q == ResampleQuality::medium ? 60.0 : 100.0);
        check(s > need, "quality " + std::to_string(static_cast<int>(q)) + " upsampling SNR " + std::to_string(s) + " dB");
    }
    const AudioBuffer same = resample(in, 44100);
    check(same.samples() == in.samples(), "same-rate resampling copies");
}

void test_buffer() {
    AudioBuffer a = sine(100.0, 1000, 2, 1000);
    check(std::fabs(a.duration() - 1.0) < 1e-12 && a.frames() == 1000 && a.sample_count() == 2000, "buffer size and duration");
    AudioBuffer b = a.slice(100, 50);
    check(b.frames() == 50 && b.at(0, 0) == a.at(100, 0), "slice");
    AudioBuffer c = a;
    c.erase(0, 900);
    check(c.frames() == 100 && c.at(0, 1) == a.at(900, 1), "erase");
    c.insert(50, b);
    check(c.frames() == 150 && c.at(50, 0) == b.at(0, 0), "insert");
    AudioBuffer d(2, 1000);
    check(d.mix_in(b, 10, 2.0f) && d.frames() == 60 && std::fabs(d.at(10, 0) - 2.0f * b.at(0, 0)) < 1e-7f, "mix in at an offset grows the buffer");
    AudioBuffer m = a.with_channels(1);
    check(m.channels() == 1 && std::fabs(m.at(5, 0) - a.at(5, 0)) < 1e-7f, "with_channels");
    float expect_peak = 0.0f;
    for (float v : a.samples()) expect_peak = std::max(expect_peak, std::fabs(v));
    check(a.peak() == expect_peak && expect_peak > 0.47f && std::fabs(a.rms() - 0.5f / std::sqrt(2.0f)) < 1e-3f, "peak and rms");
    AudioBuffer n = a;
    n.normalize(1.0f);
    check(std::fabs(n.peak() - 1.0f) < 1e-6f, "normalize");
    check(a.frame_at(0.25) == 250 && std::fabs(a.seconds_at(500) - 0.5) < 1e-12, "time to frame");
    std::vector<float> left = a.channel(0);
    AudioBuffer e(2, 1000, 1000);
    e.set_channel(1, left);
    check(e.at(37, 1) == a.at(37, 0) && e.at(37, 0) == 0.0f, "channel get and set");
    AudioBuffer f = a;
    check(!f.append(sine(1.0, 2000, 2, 10)) && f.append(b) && f.frames() == 1050, "append checks the format");
}

void test_wav() {
    const AudioBuffer src = sine(440.0, 44100, 2, 4410, 0.7f);
    for (SampleFormat f : { SampleFormat::u8, SampleFormat::s16, SampleFormat::s24, SampleFormat::s32, SampleFormat::f32, SampleFormat::f64, SampleFormat::alaw, SampleFormat::ulaw }) {
        const std::string path = temp_path(std::string("fizmo_audio_") + format_name(f) + ".wav");
        std::string err;
        const bool wrote = write_wav(path, src, f, &err, false);
        AudioBuffer back;
        WavInfo info;
        const bool read = read_wav(path, back, &err, &info);
        double worst = 0.0;
        if (read) {
            for (std::size_t i = 0; i < std::min(back.sample_count(), src.sample_count()); ++i) worst = std::max(worst, static_cast<double>(std::fabs(back.samples()[i] - src.samples()[i])));
        }
        const double tol = f == SampleFormat::u8 ? 1.0 / 128 : (f == SampleFormat::alaw || f == SampleFormat::ulaw) ? 0.05 : f == SampleFormat::s16 ? 1.0 / 32768 : 1e-6;
        check(wrote && read && info.format == f && back.frames() == src.frames() && back.channels() == 2 && back.sample_rate() == 44100 && worst <= tol, std::string("WAV round trip as ") + format_name(f) + " " + err);
        std::remove(path.c_str());
    }
    const AudioBuffer six = sine(300.0, 48000, 6, 480, 0.3f);
    const std::vector<std::uint8_t> bytes = encode_wav(six, SampleFormat::s24);
    check(is_wav(bytes.data(), bytes.size()), "is_wav knows a WAV");
    check(bytes.size() > 40 && detail::read_le16(&bytes[20]) == 0xFFFE && detail::read_le32(&bytes[40]) == 0x3F, "six channels are written as WAVE_FORMAT_EXTENSIBLE with the 5.1 mask");
    AudioBuffer back;
    WavInfo info;
    check(read_wav(bytes.data(), bytes.size(), back, nullptr, &info) && back.channels() == 6 && info.channel_mask == 0x3F && back.frames() == 480, "read the six-channel WAV from memory");

    std::vector<std::uint8_t> handmade = encode_wav(sine(1.0, 8000, 1, 8), SampleFormat::s16);
    std::vector<std::uint8_t> list = { 'L', 'I', 'S', 'T', 0, 0, 0, 0, 'I', 'N', 'F', 'O', 'I', 'N', 'A', 'M', 6, 0, 0, 0, 'S', 'o', 'n', 'g', '!', 0, 'I', 'A', 'R', 'T', 3, 0, 0, 0, 'M', 'e', 0, 0 };
    detail::put_le32(&list[4], static_cast<std::uint32_t>(list.size() - 8));
    handmade.insert(handmade.end(), list.begin(), list.end());
    detail::put_le32(&handmade[4], static_cast<std::uint32_t>(handmade.size() - 8));
    check(read_wav(handmade.data(), handmade.size(), back, nullptr, &info) && info.tags.size() == 2 && info.tags[0].second == "Song!" && info.tags[1].first == "artist" && info.tags[1].second == "Me" && back.frames() == 8, "LIST INFO tags after the data are read");

    std::vector<std::uint8_t> truncated = encode_wav(sine(1.0, 8000, 1, 100), SampleFormat::s16);
    detail::put_le32(&truncated[40], 0);
    check(read_wav(truncated.data(), truncated.size(), back) && back.frames() == 100, "a data chunk with a zero size (unfinished recording) reads to the end");
    std::vector<std::uint8_t> junk(64, 0);
    std::string err;
    check(!read_wav(junk.data(), junk.size(), back, &err) && !err.empty(), "garbage isn't a WAV");

    const std::string path = temp_path("fizmo_audio_stream.wav");
    WavWriter w;
    check(w.open(path, 2, 22050, SampleFormat::f32), "open a streaming WAV writer");
    for (int i = 0; i < 10; ++i) w.write(src.slice(static_cast<std::size_t>(i) * 441, 441));
    check(w.close() && w.frames_written() == 4410, "write a WAV in pieces");
    WavReader r;
    check(r.open(path) && r.info().frames == 4410 && r.info().sample_rate == 22050, "the streaming writer's header is right");
    std::vector<float> part(200 * 2);
    r.seek(1000);
    check(r.read(part.data(), 200) == 200 && part[0] == src.at(1000, 0) && r.position() == 1200, "seek and read a WAV");
    r.close();
    std::remove(path.c_str());
}

void test_mixer() {
    Mixer mixer(48000, 2);
    const AudioBuffer stereo = sine(1000.0, 48000, 2, 4800, 0.5f);
    Mixer::VoiceId id = mixer.play(stereo);
    check(id != 0 && mixer.is_playing(id) && mixer.voice_count() == 1, "play a sound");
    std::vector<float> out(9600 * 2);
    mixer.render(out.data(), 9600);
    bool same = true;
    for (std::size_t i = 0; i < 4800 * 2; ++i) same = same && out[i] == stereo.samples()[i];
    bool silent_after = true;
    for (std::size_t i = 4800 * 2; i < out.size(); ++i) silent_after = silent_after && out[i] == 0.0f;
    check(same && silent_after, "the mixer plays a same-rate sound sample for sample, then stops");
    check(!mixer.is_playing(id) && mixer.voice_count() == 0, "a finished sound isn't playing any more");

    const AudioBuffer mono = sine(500.0, 48000, 1, 480, 0.5f);
    id = mixer.play(mono, PlayOptions{ 1.0f, -1.0f });
    std::vector<float> o2(480 * 2);
    mixer.render(o2.data(), 480);
    bool left_only = true;
    for (std::size_t i = 0; i < 480; ++i) left_only = left_only && o2[2 * i] == mono.samples()[i] && o2[2 * i + 1] == 0.0f;
    check(left_only, "a mono sound panned hard left only plays on the left");

    PlayOptions looping;
    looping.loop = true;
    id = mixer.play(mono, looping);
    std::vector<float> o3(1200 * 2);
    mixer.render(o3.data(), 1200);
    bool looped = true;
    for (std::size_t i = 0; i < 1200; ++i) looped = looped && std::fabs(o3[2 * i] - mono.samples()[i % 480]) < 1e-6f;
    check(looped && mixer.is_playing(id), "a looping sound wraps around seamlessly");
    mixer.stop(id);
    mixer.render(o3.data(), 16);
    check(!mixer.is_playing(id), "stop ends a looping sound");

    const AudioBuffer low = sine(1000.0, 24000, 1, 24000, 0.5f);
    id = mixer.play(low);
    std::vector<float> o4(48000 * 2 + 200);
    mixer.render(o4.data(), 48000 + 100);
    std::vector<float> l4;
    for (std::size_t i = 0; i < 48000; ++i) l4.push_back(o4[2 * i]);
    check(std::fabs(dominant_frequency(l4, 48000.0, 500.0, 1500.0) - 1000.0) <= 5.0 && o4[2 * 48050] == 0.0f, "a 24 kHz sound plays at the right pitch and length in a 48 kHz mixer");

    PlayOptions fast;
    fast.speed = 2.0f;
    id = mixer.play(sine(440.0, 48000, 1, 48000, 0.5f), fast);
    std::vector<float> o5(24000 * 2);
    mixer.render(o5.data(), 24000);
    std::vector<float> l5;
    for (std::size_t i = 0; i < 24000; ++i) l5.push_back(o5[2 * i]);
    std::vector<float> o5b(100 * 2);
    mixer.render(o5b.data(), 100);
    check(std::fabs(dominant_frequency(l5, 48000.0, 500.0, 1500.0) - 880.0) <= 5.0 && !mixer.is_playing(id), "speed 2 plays an octave up in half the time");

    const AudioBuffer dc(1, 48000, std::vector<float>(48000, 0.8f));
    PlayOptions fade;
    fade.fade_in = 0.1;
    id = mixer.play(dc, fade);
    std::vector<float> o6(9600 * 2);
    mixer.render(o6.data(), 9600);
    check(o6[0] < 0.01f && std::fabs(o6[2 * 2400] - 0.4f) < 0.01f && std::fabs(o6[2 * 6000] - 0.8f) < 1e-5f, "fade in ramps up over its time");
    mixer.stop(id, 0.05);
    std::vector<float> o7(4800 * 2);
    mixer.render(o7.data(), 4800);
    check(std::fabs(o7[0] - 0.8f) < 0.02f && std::fabs(o7[2 * 1200] - 0.4f) < 0.02f && o7[2 * 2400 + 10] == 0.0f && !mixer.is_playing(id), "stop with a fade fades out, then ends");

    id = mixer.play(dc, PlayOptions{});
    mixer.set_gain(id, 0.0f);
    std::vector<float> o8(4800 * 2);
    mixer.render(o8.data(), 4800);
    check(o8[0] > 0.7f && std::fabs(o8[2 * 4799]) < 1e-4f, "changing the gain glides instead of clicking");
    mixer.seek(id, 0.5);
    mixer.render(o8.data(), 10);
    check(std::fabs(mixer.position(id) - (0.5 + 10.0 / 48000.0)) < 1e-6, "seek moves the play position");
    mixer.set_paused(id, true);
    mixer.render(o8.data(), 480);
    const double held = mixer.position(id);
    mixer.render(o8.data(), 480);
    check(mixer.is_paused(id) && mixer.position(id) == held && o8[0] == 0.0f, "a paused sound stays put and is silent");
    mixer.stop_all();
    mixer.render(o8.data(), 1);
    check(mixer.voice_count() == 0, "stop_all stops everything");

    std::vector<Mixer::VoiceId> ids;
    const Sound shared = make_sound(AudioBuffer(1, 48000, std::vector<float>(480, 0.01f)));
    for (int i = 0; i < 100; ++i) ids.push_back(mixer.play(shared));
    mixer.set_master_gain(0.5f);
    std::vector<float> o9(480 * 2);
    mixer.render(o9.data(), 480);
    check(std::fabs(o9[100] - 0.5f) < 1e-4f && std::fabs(mixer.output_peak() - 0.5f) < 1e-4f, "a hundred voices sum, then the master gain applies");
    mixer.set_master_gain(1.0f);

    std::atomic<bool> run{ true };
    std::thread audio([&] {
        std::vector<float> buf(256 * 2);
        while (run.load()) mixer.render(buf.data(), 256);
    });
    std::mt19937 rng(3);
    std::vector<Mixer::VoiceId> live;
    for (int i = 0; i < 3000; ++i) {
        const int op = static_cast<int>(rng() % 4);
        if (op == 0 || live.empty()) {
            PlayOptions po;
            po.loop = (rng() % 2) == 0;
            live.push_back(mixer.play(shared, po));
        } else if (op == 1) {
            mixer.stop(live[rng() % live.size()], (rng() % 2) ? 0.01 : 0.0);
        } else if (op == 2) {
            const Mixer::VoiceId v = live[rng() % live.size()];
            mixer.set_gain(v, 0.5f);
            mixer.set_pan(v, -0.5f);
            mixer.seek(v, 0.001);
            (void)mixer.position(v);
        } else {
            (void)mixer.voice_count();
        }
    }
    mixer.stop_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    run.store(false);
    audio.join();
    check(mixer.voice_count() == 0, "the mixer survives one thread controlling voices while another renders");
}

void test_fft_and_effects() {
    for (std::size_t n : { 4u, 8u, 64u, 1024u }) {
        Fft fft(n);
        std::mt19937 rng(static_cast<unsigned>(n));
        auto u = [&](auto& gen) { return fizmo_test::random_float(gen, -1.0f, 1.0f); };
        std::vector<float> x(n);
        for (float& v : x) v = u(rng);
        std::vector<std::complex<float>> X(n / 2 + 1);
        fft.forward_real(x.data(), X.data());
        double worst = 0.0;
        for (std::size_t k = 0; k <= n / 2; ++k) {
            std::complex<double> ref(0.0, 0.0);
            for (std::size_t i = 0; i < n; ++i) ref += static_cast<double>(x[i]) * std::polar(1.0, -2.0 * k_pi * static_cast<double>(k * i) / static_cast<double>(n));
            worst = std::max(worst, std::abs(ref - std::complex<double>(X[k].real(), X[k].imag())));
        }
        std::vector<float> back(n);
        fft.inverse_real(X.data(), back.data());
        double round = 0.0;
        for (std::size_t i = 0; i < n; ++i) round = std::max(round, static_cast<double>(std::fabs(back[i] - x[i])));
        std::vector<std::complex<float>> c(n), C(n), cb(n);
        for (std::size_t i = 0; i < n; ++i) c[i] = std::complex<float>(x[i], u(rng));
        fft.forward(c.data(), C.data());
        fft.inverse(C.data(), cb.data());
        double cround = 0.0;
        for (std::size_t i = 0; i < n; ++i) cround = std::max(cround, static_cast<double>(std::abs(cb[i] - c[i])));
        check(worst < 1e-4 * static_cast<double>(n) && round < 1e-5 && cround < 1e-5, "FFT of size " + std::to_string(n) + " matches a direct DFT and inverts");
    }
    check(next_power_of_two(1000) == 1024 && next_power_of_two(1024) == 1024, "next_power_of_two");
    const std::vector<float> hann = make_window(WindowType::hann, 8);
    check(hann[0] == 0.0f && std::fabs(hann[4] - 1.0f) < 1e-6f, "Hann window");

    EffectChain chain;
    auto g1 = std::make_shared<Gain>(-6.0206f);
    auto g2 = std::make_shared<Gain>(0.0f);
    chain.add(g1);
    chain.add(g2);
    AudioBuffer ones(2, 48000, std::vector<float>(2 * 48000, 1.0f));
    apply(chain, ones);
    check(std::fabs(ones.at(0, 0) - 0.5f) < 1e-4f && std::fabs(ones.at(47999, 1) - 0.5f) < 1e-4f, "an effect chain applies each effect");
    check(g1->set("db", 6.0206f) && std::fabs(g1->get("db") - 6.0206f) < 1e-6f && !g1->set("nope", 1.0f), "effect parameters by name");
    g1->db.set(1000.0f);
    check(g1->db.get() == 48.0f, "parameters clamp to their range");
    g1->db.set(0.0f);
    g2->set_enabled(false);
    chain.prepare(48000, 2);
    std::vector<float> block(2 * 256, 1.0f);
    chain.process(block.data(), 256);
    check(std::fabs(block[0] - 1.0f) < 1e-6f, "disabled effects are skipped");
    chain.remove(0);
    chain.process(block.data(), 256);
    check(chain.size() == 1, "removing an effect");
    std::atomic<bool> run{ true };
    std::thread audio([&] {
        std::vector<float> b(2 * 128, 0.5f);
        while (run.load()) chain.process(b.data(), 128);
    });
    for (int i = 0; i < 2000; ++i) {
        chain.add(std::make_shared<Gain>(-1.0f));
        if (chain.size() > 5) chain.remove(0);
        if (i % 7 == 0) chain.move(0, chain.size() - 1);
    }
    run.store(false);
    audio.join();
    check(chain.size() <= 5, "an effect chain can be edited while audio runs through it");
}

void test_codecs() {
    const AudioBuffer music = sine(440.0, 44100, 2, 44100, 0.5f);
    struct Case { const char* ext; AudioFileType type; double min_snr; };
    const Case cases[] = { { "wav", AudioFileType::wav, 85.0 }, { "flac", AudioFileType::flac, 85.0 }, { "mp3", AudioFileType::mp3, 20.0 } };
    for (const Case& c : cases) {
        const std::string path = temp_path(std::string("fizmo_codec.") + c.ext);
        SaveOptions so;
        so.tags = { { "title", "Test Tone" }, { "artist", "Fizmo" } };
        std::string err;
        const bool saved = save_audio(path, music, so, &err);
        AudioBuffer back;
        AudioFileInfo info;
        const bool loaded = saved && load_audio(path, back, &err, &info);
        double snr = 0.0;
        if (loaded && back.frames() == music.frames()) {
            std::vector<float> g(back.samples().begin() + 4410, back.samples().end() - 4410);
            std::vector<float> w(music.samples().begin() + 4410, music.samples().end() - 4410);
            snr = snr_db(g, w);
        }
        check(saved && loaded && info.type == c.type && back.frames() == music.frames() && back.channels() == 2 && back.sample_rate() == 44100 && snr > c.min_snr,
              std::string("save and load ") + c.ext + " (SNR " + std::to_string(snr) + " dB) " + err);
        if (c.type != AudioFileType::wav) check(info.tag("title") == "Test Tone" && info.tag("ARTIST") == "Fizmo", std::string("tags survive in ") + c.ext);
        check(detect_audio_file(path) == c.type, std::string("detect ") + c.ext + " by content");
        const std::string renamed = temp_path("fizmo_codec_mislabeled.dat");
        std::rename(path.c_str(), renamed.c_str());
        AudioReader reader;
        check(reader.open(renamed, &err) && reader.info().type == c.type, std::string("a mislabeled ") + c.ext + " file still opens " + err);
        std::vector<float> part(100 * 2);
        check(reader.seek_seconds(0.5) && reader.read(part.data(), 100) == 100 && reader.position() == 22150, std::string("seek and read through AudioReader (") + c.ext + ")");
        const float expect = music.at(22050, 0);
        check(std::fabs(part[0] - expect) < (c.type == AudioFileType::mp3 ? 0.1f : 1e-3f), std::string("seeking lands on the right sample (") + c.ext + ")");
        reader.close();
        std::remove(renamed.c_str());
    }
    std::vector<std::uint8_t> mem = encode_audio(music, AudioFileType::flac);
    AudioBuffer from_mem;
    check(!mem.empty() && detect_audio_type(mem.data(), mem.size()) == AudioFileType::flac && load_audio(mem.data(), mem.size(), from_mem) && from_mem.frames() == music.frames(), "encode to memory and load from memory");
    std::string err;
    check(!save_audio(temp_path("x.unknownext"), music, SaveOptions(), &err) && !err.empty(), "saving to an unknown extension explains itself");
    AudioBuffer nothing;
    check(!load_audio(temp_path("definitely_missing_file.wav"), nothing, &err) && !err.empty(), "loading a missing file fails with a message");
    check(audio_type_from_extension("Song.MP3") == AudioFileType::mp3 && audio_type_from_extension("a.b/c") == AudioFileType::unknown, "extension detection");
}

void test_null_device() {
    std::string err;
    check(set_backend("null", &err), "switch to the null backend " + err);
    check(backend_name() == "null", "the null backend is active");
    std::atomic<std::size_t> calls{ 0 };
    Stream s;
    StreamConfig cfg;
    cfg.sample_rate = 48000;
    cfg.channels = 2;
    cfg.latency_ms = 10;
    check(s.open_playback(cfg, [&](float* out, std::size_t frames) {
        ++calls;
        for (std::size_t i = 0; i < frames * 2; ++i) out[i] = 0.0f;
    }, &err), "open a null playback stream " + err);
    check(s.start(&err), "start it");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    s.stop();
    const double secs = s.seconds_processed();
    check(calls > 10 && secs > 0.12 && secs < 0.32, "the null device runs in real time (" + std::to_string(secs) + " s)");
    s.close();
    Stream bad;
    check(bad.open_playback(cfg, [](float*, std::size_t) { throw std::runtime_error("boom"); }) && bad.start(), "open a stream whose callback throws");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    check(bad.failed() && bad.error().find("boom") != std::string::npos, "a throwing callback fails the stream instead of crashing");
    bad.close();
    set_backend("auto");
}

void test_real_devices() {
    const char* want = std::getenv("FIZMO_AUDIO_DEVICE_TESTS");
    if (!want || std::string(want) != "1") {
        std::printf("skipping device tests (set FIZMO_AUDIO_DEVICE_TESTS=1 to run them)\n");
        return;
    }
    std::string err;
    const std::string name = backend_name();
    check(!name.empty() && name != "null", "a real backend is picked (" + name + ")");
    std::printf("backend: %s\n", name.c_str());
    for (const DeviceInfo& d : devices()) std::printf("  %-8s %s%s  [%s] %u Hz %u ch\n", device_kind_name(d.kind), d.name.c_str(), d.is_default ? " (default)" : "", d.id.c_str(), d.sample_rate, d.channels);
    const auto out_dev = default_device(DeviceKind::output);
    const auto loop_dev = default_device(DeviceKind::loopback);
    const auto in_dev = default_device(DeviceKind::input);
    const bool has_loopback = name != "alsa" && !std::getenv("FIZMO_AUDIO_NO_LOOPBACK");
    check(out_dev.has_value() && in_dev.has_value() && (loop_dev.has_value() || !has_loopback), "there's a default output, input and loopback device");

    std::mutex mutex;
    std::vector<float> recorded;
    Stream rec;
    StreamConfig rc;
    rc.kind = DeviceKind::loopback;
    rc.sample_rate = 48000;
    rc.channels = 2;
    if (has_loopback) {
        check(rec.open_capture(rc, [&](const float* in, std::size_t frames) {
            std::lock_guard<std::mutex> lock(mutex);
            recorded.insert(recorded.end(), in, in + frames * 2);
        }, &err), "open a system-audio recording " + err);
    } else if (name == "alsa") {
        check(!rec.open_capture(rc, [](const float*, std::size_t) {}, &err) && !err.empty(), "a backend without system-audio recording says so: " + err);
    }
    std::atomic<std::uint64_t> phase{ 0 };
    Stream play;
    StreamConfig pc;
    pc.sample_rate = 44100;
    pc.channels = 2;
    check(play.open_playback(pc, [&](float* out, std::size_t frames) {
        std::uint64_t p = phase.load();
        for (std::size_t i = 0; i < frames; ++i, ++p) {
            const float v = 0.4f * static_cast<float>(std::sin(2.0 * k_pi * 440.0 * static_cast<double>(p) / 44100.0));
            out[2 * i] = v;
            out[2 * i + 1] = 0.5f * v;
        }
        phase.store(p);
    }, &err), "open playback at 44.1 kHz on the default device " + err);
    check(play.sample_rate() == 44100 && play.channels() == 2, "playback keeps the asked-for format");
    if (has_loopback) check(rec.start(&err), "start recording " + err);
    check(play.start(&err), "start playing " + err);
    const auto warm_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (std::chrono::steady_clock::now() < warm_deadline) {
        const std::uint64_t before = play.frames_processed();
        std::size_t rec_before;
        {
            std::lock_guard<std::mutex> lock(mutex);
            rec_before = recorded.size();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        const double play_rate = static_cast<double>(play.frames_processed() - before) / 0.25;
        std::size_t rec_after;
        {
            std::lock_guard<std::mutex> lock(mutex);
            rec_after = recorded.size();
        }
        const double rec_rate = static_cast<double>((rec_after - rec_before) / 2) / 0.25;
        if (std::fabs(play_rate - 44100.0) < 4410.0 && (!has_loopback || std::fabs(rec_rate - 48000.0) < 4800.0)) break;
    }
    std::size_t rec_start;
    {
        std::lock_guard<std::mutex> lock(mutex);
        rec_start = recorded.size();
    }
    const std::uint64_t play_start = play.frames_processed();
    const auto t0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double played = static_cast<double>(play.frames_processed() - play_start) / 44100.0;
    std::vector<float> window;
    {
        std::lock_guard<std::mutex> lock(mutex);
        window.assign(recorded.begin() + static_cast<std::ptrdiff_t>(rec_start), recorded.end());
    }
    const double heard = static_cast<double>(window.size() / 2) / 48000.0;
    play.stop();
    rec.stop();
    std::printf("in %.2f s: played %.2f s on '%s', recorded %.2f s from '%s'\n", elapsed, played, play.device_name().c_str(), heard, rec.device_name().c_str());
    check(std::fabs(played - elapsed) < 0.25 * elapsed, "playback runs in real time");
    if (has_loopback) check(std::fabs(heard - elapsed) < 0.25 * elapsed, "system-audio recording runs in real time");
    std::vector<float> left;
    std::vector<float> right;
    for (std::size_t i = 0; i + 1 < window.size(); i += 2) {
        left.push_back(window[i]);
        right.push_back(window[i + 1]);
    }
    float lp = 0.0f;
    float rp = 0.0f;
    for (float v : left) lp = std::max(lp, std::fabs(v));
    for (float v : right) rp = std::max(rp, std::fabs(v));
    if (!has_loopback) {
        std::printf("(this backend can't record system audio, so the tone check is skipped)\n");
    } else if (left.size() > 24000) {
        const double f = dominant_frequency(left, 48000.0, 100.0, 2000.0);
        check(std::fabs(f - 440.0) <= 5.0, "the recording hears the 440 Hz tone that was played (" + std::to_string(f) + " Hz)");
        check(lp > 0.3f && lp < 0.5f && rp > 0.15f && rp < 0.25f, "left and right levels come through (" + std::to_string(lp) + ", " + std::to_string(rp) + ")");
    } else {
        check(false, "recorded enough system audio to analyze (" + std::to_string(left.size()) + " frames)");
    }
    play.close();
    rec.close();

    std::vector<float> mic;
    Stream in;
    StreamConfig ic;
    ic.kind = DeviceKind::input;
    check(in.open_capture(ic, [&](const float* data, std::size_t frames) {
        std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t i = 0; i < frames; ++i) mic.push_back(data[i * in.channels()]);
    }, &err) && in.start(&err), "record from the default microphone " + err);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    std::size_t mic_start;
    {
        std::lock_guard<std::mutex> lock(mutex);
        mic_start = mic.size();
    }
    const auto m0 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    const double mic_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - m0).count();
    in.stop();
    std::size_t mic_frames;
    {
        std::lock_guard<std::mutex> lock(mutex);
        mic_frames = mic.size() - mic_start;
    }
    const double mic_secs = static_cast<double>(mic_frames) / in.sample_rate();
    std::printf("microphone '%s' at %u Hz, %u ch: %.2f s of audio in %.2f s\n", in.device_name().c_str(), in.sample_rate(), in.channels(), mic_secs, mic_elapsed);
    check(std::fabs(mic_secs - mic_elapsed) < 0.3 * mic_elapsed, "the microphone delivers audio in real time");
    const char* tone = std::getenv("FIZMO_AUDIO_MIC_TONE");
    if (tone && mic.size() > 4800) {
        const double f = dominant_frequency(mic, in.sample_rate(), 100.0, 4000.0);
        check(std::fabs(f - std::atof(tone)) <= 10.0, "the microphone hears its test tone (" + std::to_string(f) + " Hz)");
    }
    in.close();

    if (out_dev) {
        Stream direct;
        StreamConfig dc;
        dc.device = out_dev->id;
        dc.latency_ms = 50;
        check(direct.open_playback(dc, [](float* out, std::size_t frames) { std::memset(out, 0, frames * sizeof(float) * 2); }, &err), "open a device by its id " + err);
        check(direct.sample_rate() == out_dev->sample_rate || out_dev->sample_rate == 0, "a stream without a rate uses the device's rate");
        direct.close();
    }
    if (has_loopback) {
        std::vector<float> heard_mix;
        Stream listen;
        StreamConfig lc;
        lc.kind = DeviceKind::loopback;
        lc.sample_rate = 48000;
        lc.channels = 1;
        Mixer board(48000, 2);
        bool ok = listen.open_capture(lc, [&](const float* data, std::size_t frames) {
            std::lock_guard<std::mutex> lock(mutex);
            heard_mix.insert(heard_mix.end(), data, data + frames);
        }, &err) && listen.start(&err) && board.start(StreamConfig(), &err);
        check(ok, "start a mixer on the default output " + err);
        PlayOptions po;
        po.loop = true;
        board.play(sine(660.0, 22050, 1, 2205, 0.6f), po);
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        std::vector<float> tail;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (heard_mix.size() > 24000) tail.assign(heard_mix.end() - 24000, heard_mix.end());
        }
        board.stop_output();
        listen.close();
        check(!tail.empty() && std::fabs(dominant_frequency(tail, 48000.0, 300.0, 1500.0) - 660.0) <= 5.0, "a sound played through the mixer reaches the speakers");
    }
    Stream missing;
    StreamConfig mc;
    mc.device = "no-such-device-anywhere";
    check(!missing.open_playback(mc, [](float*, std::size_t) {}, &err) && !err.empty(), "opening a missing device fails with a message: " + err);
}

} // namespace

int main() {
    test_formats();
    test_resampler();
    test_buffer();
    test_wav();
    test_mixer();
    test_fft_and_effects();
    test_codecs();
    test_null_device();
    test_real_devices();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
