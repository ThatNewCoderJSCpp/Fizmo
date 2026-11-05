#include "../Source/Audio/flac.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
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
#endif // _WIN32
    std::string base = dir ? dir : "/tmp";
    return base + "/" + name;
}

std::string label(std::size_t channels, std::uint32_t rate, unsigned bits, std::size_t frames, const char* kind) {
    return std::string(kind) + " " + std::to_string(channels) + "ch " + std::to_string(rate) + "Hz " + std::to_string(bits) + "-bit " + std::to_string(frames) + " frames";
}

AudioBuffer grid_signal(std::size_t channels, std::uint32_t rate, std::size_t frames, unsigned bits, const char* kind, std::uint32_t seed) {
    AudioBuffer b(channels, rate, frames);
    const double scale = static_cast<double>(std::uint64_t(1) << (bits - 1));
    const std::int64_t hi = static_cast<std::int64_t>(scale) - 1;
    const std::int64_t lo = -static_cast<std::int64_t>(scale);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::int64_t> any(lo, hi);
    std::uniform_int_distribution<int> small(-4, 4);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t c = 0; c < channels; ++c) {
            std::int64_t v = 0;
            if (std::strcmp(kind, "noise") == 0) {
                v = any(rng);
            } else if (std::strcmp(kind, "square") == 0) {
                v = ((i / 9 + c) & 1) ? hi : lo;
            } else if (std::strcmp(kind, "silence") == 0) {
                v = 0;
            } else {
                const double s = 0.6 * std::sin(2.0 * k_pi * (220.0 + 110.0 * c) * static_cast<double>(i) / rate) + 0.2 * std::sin(2.0 * k_pi * 3000.0 * static_cast<double>(i) / rate);
                v = static_cast<std::int64_t>(std::floor(s * scale)) + small(rng);
                v = std::max(lo, std::min(hi, v));
            }
            b.at(i, c) = static_cast<float>(static_cast<double>(v) / scale);
        }
    }
    return b;
}

bool same_samples(const AudioBuffer& a, const AudioBuffer& b) {
    return a.channels() == b.channels() && a.sample_rate() == b.sample_rate() && a.frames() == b.frames() && (a.samples().empty() || std::memcmp(a.data(), b.data(), a.sample_count() * sizeof(float)) == 0);
}

bool md5_is(const FlacInfo& info, const char* hex) {
    char text[33];
    for (int i = 0; i < 16; ++i) std::snprintf(text + 2 * i, 3, "%02x", info.md5[i]);
    return std::strcmp(text, hex) == 0;
}

void round_trip(std::size_t channels, std::uint32_t rate, std::size_t frames, unsigned bits, const char* kind, int level = 5) {
    const AudioBuffer src = grid_signal(channels, rate, frames, bits, kind, static_cast<std::uint32_t>(channels * 131 + rate + frames + bits));
    FlacEncodeOptions opt;
    opt.bits_per_sample = bits;
    opt.compression = level;
    opt.verify = true;
    std::string error;
    const std::vector<std::uint8_t> data = encode_flac(src, opt, &error);
    const std::string name = label(channels, rate, bits, frames, kind) + " level " + std::to_string(level);
    check(!data.empty(), "encode " + name + (error.empty() ? "" : " (" + error + ")"));
    check(is_flac(data.data(), data.size()), "is_flac recognises " + name);
    AudioBuffer back;
    FlacInfo info;
    error.clear();
    const bool ok = read_flac(data.data(), data.size(), back, &error, &info);
    check(ok, "decode " + name + (error.empty() ? "" : " (" + error + ")"));
    check(same_samples(src, back), "bit-exact round trip " + name);
    check(info.frames == frames && info.channels == channels && info.sample_rate == rate && info.bits_per_sample == bits, "stream info for " + name);
}

void test_round_trips() {
    const unsigned depths[] = { 16, 24 };
    for (unsigned bits : depths) {
        round_trip(1, 44100, 20000, bits, "music");
        round_trip(2, 44100, 20000, bits, "music");
        round_trip(6, 48000, 12000, bits, "music");
        round_trip(2, 7350, 9000, bits, "music");
        round_trip(2, 96000, 9000, bits, "music");
        round_trip(2, 192000, 9000, bits, "music");
        round_trip(1, 11025, 1, bits, "music");
        round_trip(2, 44100, 1, bits, "noise");
        round_trip(8, 44100, 3, bits, "noise");
        round_trip(2, 44100, 30000, bits, "silence");
        round_trip(2, 44100, 30000, bits, "noise");
        round_trip(2, 44100, 30000, bits, "square");
        round_trip(3, 32000, 4097, bits, "music");
    }
    for (int level = 0; level <= 8; ++level) round_trip(2, 44100, 50000, 16, "music", level);
    round_trip(2, 48000, 10000, 24, "noise", 8);
    round_trip(1, 8000, 4096, 16, "music", 0);
}

void test_pcm_depths() {
    const unsigned depths[] = { 4, 8, 12, 16, 20, 24, 32 };
    for (unsigned bits : depths) {
        const std::size_t channels = 2;
        const std::size_t frames = 7000;
        std::mt19937 rng(bits);
        const std::int64_t hi = (std::int64_t(1) << (bits - 1)) - 1;
        const std::int64_t lo = -(std::int64_t(1) << (bits - 1));
        std::uniform_int_distribution<std::int64_t> any(lo, hi);
        std::vector<std::int32_t> pcm(frames * channels);
        for (std::size_t i = 0; i < frames; ++i) {
            const double s = std::sin(static_cast<double>(i) * 0.013);
            pcm[2 * i] = static_cast<std::int32_t>(std::max<double>(static_cast<double>(lo), std::min<double>(static_cast<double>(hi), std::floor(s * static_cast<double>(hi)))));
            pcm[2 * i + 1] = static_cast<std::int32_t>(i % 1000 < 500 ? any(rng) : pcm[2 * i]);
        }
        const std::string path = temp_path("fizmo_flac_pcm_" + std::to_string(bits) + ".flac");
        FlacEncodeOptions opt;
        opt.bits_per_sample = bits;
        opt.verify = true;
        FlacWriter w;
        std::string error;
        check(w.open(path, channels, 48000, opt, &error), std::to_string(bits) + "-bit writer opens");
        check(w.write_pcm(pcm.data(), 1234) && w.write_pcm(pcm.data() + 1234 * channels, frames - 1234), std::to_string(bits) + "-bit writer accepts samples");
        check(w.frames_written() == frames, std::to_string(bits) + "-bit writer counts frames");
        check(w.close(&error), std::to_string(bits) + "-bit writer closes" + (error.empty() ? "" : " (" + error + ")"));
        FlacReader r;
        check(r.open(path, &error), std::to_string(bits) + "-bit reader opens");
        std::vector<std::int32_t> back(frames * channels + 64);
        const std::size_t got = r.read_pcm(back.data(), frames + 32);
        check(got == frames && std::memcmp(back.data(), pcm.data(), frames * channels * sizeof(std::int32_t)) == 0, std::to_string(bits) + "-bit PCM round trip is exact");
        check(r.error_count() == 0, std::to_string(bits) + "-bit stream decodes without errors");
        std::remove(path.c_str());
    }
}

void test_md5_and_info() {
    AudioBuffer one(1, 44100, 1);
    std::vector<std::uint8_t> data = encode_flac(one);
    FlacInfo info;
    AudioBuffer back;
    check(read_flac(data.data(), data.size(), back, nullptr, &info), "single silent sample decodes");
    check(md5_is(info, "c4103f122d27677c9db144cae1394a66"), "MD5 of one zero 16-bit sample matches the known digest");
    AudioBuffer two(2, 44100, 1);
    two.at(0, 0) = 1.0f / 32768.0f;
    two.at(0, 1) = -1.0f / 32768.0f;
    data = encode_flac(two);
    check(read_flac(data.data(), data.size(), back, nullptr, &info), "two-sample stream decodes");
    check(md5_is(info, "937c58ede5c80464814a18be9e542be4"), "MD5 of samples 1 and -1 matches the known digest");
    const AudioBuffer music = grid_signal(2, 44100, 100000, 16, "music", 7);
    data = encode_flac(music);
    check(read_flac(data.data(), data.size(), back, nullptr, &info), "long stream decodes");
    check(info.min_block_size == 2048 && info.max_block_size == 2048, "default block size is 2048");
    check(info.min_frame_size > 0 && info.max_frame_size >= info.min_frame_size, "frame sizes are recorded");
    check(info.has_seek_table, "write_flac/encode_flac add a seek table");
    FlacEncodeOptions opt;
    opt.compression = 0;
    data = encode_flac(music, opt);
    check(read_flac(data.data(), data.size(), back, nullptr, &info) && info.max_block_size == 2048 && same_samples(back, music), "level 0 uses 2048-sample blocks");
    const AudioBuffer high = grid_signal(2, 96000, 20000, 16, "music", 9);
    data = encode_flac(high);
    check(read_flac(data.data(), data.size(), back, nullptr, &info) && info.max_block_size == 4096 && same_samples(back, high), "rates above 48 kHz use 4096-sample blocks");
    opt.block_size = 1000;
    data = encode_flac(music, opt);
    check(read_flac(data.data(), data.size(), back, nullptr, &info) && info.max_block_size == 1000 && same_samples(back, music), "custom block size round trips");
    const std::vector<std::uint8_t> l0 = encode_flac(music, FlacEncodeOptions());
    FlacEncodeOptions l8;
    l8.compression = 8;
    const std::vector<std::uint8_t> big = encode_flac(music, l8);
    check(big.size() <= l0.size(), "level 8 is no larger than level 5");
    AudioBuffer noisy(2, 44100, 1000);
    for (std::size_t i = 0; i < noisy.sample_count(); ++i) noisy.data()[i] = static_cast<float>(0.3 * std::sin(static_cast<double>(i) * 0.0123456));
    data = encode_flac(noisy);
    check(read_flac(data.data(), data.size(), back), "off-grid floats encode");
    double worst = 0.0;
    for (std::size_t i = 0; i < noisy.sample_count(); ++i) worst = std::max(worst, std::fabs(static_cast<double>(back.data()[i]) - noisy.data()[i]));
    check(worst <= 1.5 / 32768.0, "dithered 16-bit error stays within 1.5 LSB");
    AudioBuffer clip(1, 44100, 4);
    clip.at(0, 0) = 1.0f;
    clip.at(1, 0) = -1.0f;
    clip.at(2, 0) = 2.0f;
    clip.at(3, 0) = std::nanf("");
    data = encode_flac(clip);
    check(read_flac(data.data(), data.size(), back) && back.at(0, 0) == 32767.0f / 32768.0f && back.at(1, 0) == -1.0f && back.at(2, 0) == 32767.0f / 32768.0f && back.at(3, 0) == 0.0f, "out-of-range and NaN input is clamped");
}

void test_seeking() {
    const AudioBuffer music = grid_signal(2, 44100, 300000, 16, "music", 11);
    const std::string with_table = temp_path("fizmo_flac_seek_table.flac");
    const std::string without_table = temp_path("fizmo_flac_seek_plain.flac");
    FlacEncodeOptions opt;
    opt.block_size = 1152;
    check(write_flac(with_table, music, opt), "write file with seek table");
    FlacWriter w;
    check(w.open(without_table, 2, 44100, opt) && w.write(music) && w.close(), "stream file without seek table");
    std::vector<std::uint8_t> blob;
    {
        std::FILE* f = std::fopen(without_table.c_str(), "rb");
        if (f) {
            std::uint8_t tmp[4096];
            std::size_t n;
            while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) blob.insert(blob.end(), tmp, tmp + n);
            std::fclose(f);
        }
    }
    const std::string* paths[2] = { &with_table, &without_table };
    for (int k = 0; k < 3; ++k) {
        FlacReader r;
        bool opened = k < 2 ? r.open(*paths[k]) : r.open(blob.data(), blob.size());
        const std::string name = k == 0 ? "seek table" : k == 1 ? "bisection" : "memory bisection";
        check(opened, "open for " + name);
        check(r.info().has_seek_table == (k == 0), "seek table presence for " + name);
        std::mt19937 rng(static_cast<std::uint32_t>(k + 3));
        std::uniform_int_distribution<std::uint64_t> pick(0, music.frames() - 1);
        bool all_ok = true;
        std::vector<std::uint64_t> targets = { 0, 1, 1151, 1152, 1153, music.frames() - 1, music.frames() - 1152 };
        for (int i = 0; i < 60; ++i) targets.push_back(pick(rng));
        std::vector<float> got(2 * 2000);
        for (std::uint64_t t : targets) {
            if (!r.seek(t) || r.position() != t) {
                all_ok = false;
                continue;
            }
            const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(2000, music.frames() - t));
            const std::size_t n = r.read(got.data(), 2000);
            if (n != want || std::memcmp(got.data(), music.frame(static_cast<std::size_t>(t)), n * 2 * sizeof(float)) != 0) all_ok = false;
            if (r.position() != t + n) all_ok = false;
        }
        check(all_ok, "sample-accurate seeking with " + name);
        check(r.seek(music.frames() + 10) && r.read(got.data(), 10) == 0, "seeking past the end with " + name);
        check(r.seek(5) && r.read(got.data(), 1) == 1 && got[0] == music.at(5, 0), "seeking back after the end with " + name);
        check(r.error_count() == 0, "seeking reports no errors with " + name);
    }
    std::remove(with_table.c_str());
    std::remove(without_table.c_str());
}

void test_tags_and_id3() {
    const AudioBuffer music = grid_signal(2, 44100, 5000, 16, "music", 5);
    FlacEncodeOptions opt;
    opt.tags = { { "title", "Song" }, { "Artist", "Christian" }, { "album", "Ünïcødé" }, { "track", "7" }, { "date", "2026" }, { "genre", "Test=Value" } };
    const std::string path = temp_path("fizmo_flac_tags.flac");
    check(write_flac(path, music, opt), "write tagged file");
    AudioBuffer back;
    FlacInfo info;
    std::string error;
    check(read_flac(path, back, &error, &info), "read tagged file" + (error.empty() ? "" : " (" + error + ")"));
    auto find = [&](const char* key) {
        for (const auto& t : info.tags) {
            if (t.first == key) return t.second;
        }
        return std::string("<missing>");
    };
    check(find("title") == "Song" && find("artist") == "Christian" && find("album") == "Ünïcødé", "basic tags survive");
    check(find("tracknumber") == "7" && find("date") == "2026" && find("genre") == "Test=Value", "track, date and genre tags survive");
    check(same_samples(back, music), "tagged file audio is intact");
    std::vector<std::uint8_t> data = encode_flac(music, opt);
    std::vector<std::uint8_t> id3 = { 'I', 'D', '3', 4, 0, 0, 0, 0, 0, 20 };
    id3.insert(id3.end(), 20, 0);
    id3.insert(id3.end(), data.begin(), data.end());
    check(is_flac(id3.data(), id3.size()), "is_flac skips an ID3v2 tag");
    check(read_flac(id3.data(), id3.size(), back) && same_samples(back, music), "decoding skips an ID3v2 tag");
    check(!is_flac(data.data(), 3) && !is_flac(nullptr, 0) && !is_flac(id3.data(), 12), "is_flac rejects short input");
    std::remove(path.c_str());
}

void test_errors() {
    std::string error;
    AudioBuffer out;
    check(!read_flac(temp_path("fizmo_flac_missing_file.flac"), out, &error) && !error.empty(), "missing file reports an error");
    const std::uint8_t junk[] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0 };
    error.clear();
    check(!read_flac(junk, sizeof junk, out, &error) && !error.empty(), "non-FLAC data reports an error");
    const std::uint8_t bare[] = { 'f', 'L', 'a', 'C' };
    error.clear();
    check(!read_flac(bare, sizeof bare, out, &error) && !error.empty(), "FLAC marker without metadata reports an error");
    FlacWriter w;
    FlacEncodeOptions opt;
    error.clear();
    check(!w.open(temp_path("fizmo_flac_bad.flac"), 0, 44100, opt, &error) && !error.empty(), "zero channels is rejected");
    check(!w.open(temp_path("fizmo_flac_bad.flac"), 9, 44100, opt, &error), "nine channels is rejected");
    check(!w.open(temp_path("fizmo_flac_bad.flac"), 2, 0, opt, &error), "zero sample rate is rejected");
    opt.bits_per_sample = 33;
    check(!w.open(temp_path("fizmo_flac_bad.flac"), 2, 44100, opt, &error), "33-bit samples are rejected");
    opt.bits_per_sample = 16;
    opt.block_size = 8;
    check(!w.open(temp_path("fizmo_flac_bad.flac"), 2, 44100, opt, &error), "tiny block size is rejected");
    std::remove(temp_path("fizmo_flac_bad.flac").c_str());
    check(encode_flac(AudioBuffer()).empty(), "encoding an empty buffer without channels fails");
    check(!write_flac(temp_path("fizmo_flac_bad.flac"), AudioBuffer(), FlacEncodeOptions(), &error), "writing an empty buffer without channels fails");

    const AudioBuffer music = grid_signal(2, 44100, 60000, 16, "music", 9);
    const std::vector<std::uint8_t> good = encode_flac(music);
    std::vector<std::uint8_t> bad = good;
    bad[bad.size() / 2] ^= 0x10;
    error.clear();
    AudioBuffer back;
    FlacInfo info;
    const bool flipped_ok = read_flac(bad.data(), bad.size(), back, &error, &info);
    check(!flipped_ok && !error.empty(), "a flipped bit in the audio is reported");
    check(back.frames() + 4096 >= music.frames() && back.frames() <= music.frames(), "decoding continues after a damaged frame");
    std::size_t matching = 0;
    for (std::size_t i = 0; i + 1 < back.frames() && i < music.frames(); ++i) {
        if (back.at(i, 0) == music.at(i, 0)) ++matching;
    }
    check(matching * 10 >= music.frames() * 8, "undamaged frames still decode correctly");
    FlacReader r;
    check(r.open(bad.data(), bad.size()), "damaged stream still opens");
    std::vector<float> tmp(2 * 70000);
    const std::size_t got = r.read(tmp.data(), 70000);
    check(r.error_count() >= 1 && !r.last_error().empty() && got == music.frames(), "reader resyncs and fills the lost frame with silence");
    for (std::size_t cut : { good.size() - 1, good.size() - 100, good.size() / 3, std::size_t(60), std::size_t(41), std::size_t(10) }) {
        error.clear();
        const bool ok = read_flac(good.data(), cut, back, &error);
        check(!ok && !error.empty(), "truncation at " + std::to_string(cut) + " bytes is reported");
    }
    bad = good;
    for (std::size_t i = 300; i < bad.size(); i += 997) bad[i] = static_cast<std::uint8_t>(bad[i] * 31 + 7);
    error.clear();
    check(!read_flac(bad.data(), bad.size(), back, &error) && !error.empty(), "heavily damaged stream is reported");
    std::mt19937 rng(1234);
    std::size_t survived = 0;
    const int rounds = 300;
    for (int i = 0; i < rounds; ++i) {
        bad = good;
        const int flips = 1 + static_cast<int>(rng() % 8);
        for (int f = 0; f < flips; ++f) bad[rng() % bad.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
        if (rng() % 3 == 0) bad.resize(rng() % bad.size());
        FlacReader fr;
        if (fr.open(bad.data(), bad.size())) {
            fr.read(tmp.data(), 70000);
            fr.seek(rng() % 70000);
            fr.read(tmp.data(), 1000);
        }
        ++survived;
    }
    check(survived == static_cast<std::size_t>(rounds), "random damage never crashes the decoder");
}

} // namespace

int main() {
    test_round_trips();
    test_pcm_depths();
    test_md5_and_info();
    test_seeking();
    test_tags_and_id3();
    test_errors();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
