#include "../Source/Audio/mp3_encoder.hpp"

#include "portable_random.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    std::uint8_t buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return out;
}

AudioBuffer test_signal(std::size_t channels, std::uint32_t rate, std::size_t frames, unsigned seed = 1) {
    AudioBuffer b(channels, rate, frames);
    std::mt19937 rng(seed);
    auto noise = [&](auto& gen) { return fizmo_test::random_normal_float(gen, 0.0f, 1.0f); };
    const double r = static_cast<double>(rate);
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / r;
        const double tone = 0.3 * std::sin(2.0 * k_pi * 440.0 * t) + 0.1 * std::sin(2.0 * k_pi * 1870.0 * t + 0.4);
        const double burst = (i % (rate / 4)) < rate / 50 ? 0.4 * std::exp(-static_cast<double>(i % (rate / 4)) / (0.004 * r)) : 0.0;
        for (std::size_t c = 0; c < channels; ++c) {
            const double n = 0.02 * noise(rng) + burst * noise(rng);
            const double pan = c % 2 ? 0.8 : 1.0;
            b.at(i, c) = static_cast<float>(pan * tone + n);
        }
    }
    return b;
}

const int k_br1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 };
const int k_br2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, -1 };
const int k_rate_table[4][3] = {
    { 11025, 12000, 8000 },
    { 0, 0, 0 },
    { 22050, 24000, 16000 },
    { 44100, 48000, 32000 },
};

struct Header {
    int version = 0;
    int bitrate = 0;
    int bitrate_index = 0;
    int rate = 0;
    int padding = 0;
    int mode = 0;
    int mode_ext = 0;
    std::size_t size = 0;
    int samples = 0;
    int side_info = 0;
    int channels = 0;
};

bool parse_header(const std::uint8_t* p, Header& h) {
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;
    h.version = (p[1] >> 3) & 3;
    const int layer = (p[1] >> 1) & 3;
    if (h.version == 1 || layer != 1) return false;
    if ((p[1] & 1) != 1) return false;
    h.bitrate_index = p[2] >> 4;
    const int ri = (p[2] >> 2) & 3;
    if (h.bitrate_index == 0 || h.bitrate_index == 15 || ri == 3) return false;
    h.padding = (p[2] >> 1) & 1;
    h.mode = p[3] >> 6;
    h.mode_ext = (p[3] >> 4) & 3;
    h.rate = k_rate_table[h.version][ri];
    const bool lsf = h.version != 3;
    h.bitrate = lsf ? k_br2[h.bitrate_index] : k_br1[h.bitrate_index];
    h.size = static_cast<std::size_t>((lsf ? 72000 : 144000) * h.bitrate / h.rate + h.padding);
    h.samples = lsf ? 576 : 1152;
    h.channels = h.mode == 3 ? 1 : 2;
    h.side_info = lsf ? (h.channels == 1 ? 9 : 17) : (h.channels == 1 ? 17 : 32);
    return true;
}

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}
    std::uint32_t get(int bits) {
        std::uint32_t v = 0;
        for (int i = 0; i < bits; ++i) {
            const std::size_t byte = m_pos >> 3;
            const int bit = 7 - static_cast<int>(m_pos & 7);
            v = (v << 1) | (byte < m_size ? ((m_data[byte] >> bit) & 1u) : 0u);
            ++m_pos;
        }
        return v;
    }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_pos = 0;
};

std::uint16_t crc16(const std::uint8_t* data, std::size_t size, std::uint16_t crc = 0) {
    for (std::size_t i = 0; i < size; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ data[i]);
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001) : static_cast<std::uint16_t>(crc >> 1);
    }
    return crc;
}

std::uint32_t be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

struct StreamReport {
    bool ok = false;
    std::string problem;
    std::size_t id3_size = 0;
    bool has_info = false;
    bool is_xing = false;
    std::uint32_t xing_frames = 0;
    std::uint32_t xing_bytes = 0;
    int delay = -1;
    int padding = -1;
    bool tag_crc_ok = false;
    bool music_crc_ok = false;
    bool music_length_ok = false;
    bool toc_ok = false;
    std::size_t audio_frames = 0;
    int version = -1;
    int rate = 0;
    int mode = -1;
    int samples_per_frame = 0;
    int min_bitrate = 1000;
    int max_bitrate = 0;
    double average_kbps = 0.0;
    bool any_short = false;
    bool any_ms = false;
};

StreamReport analyze_stream(const std::vector<std::uint8_t>& d) {
    StreamReport r;
    auto fail = [&](const std::string& why) {
        r.ok = false;
        r.problem = why;
        return r;
    };
    std::size_t pos = 0;
    if (d.size() >= 10 && d[0] == 'I' && d[1] == 'D' && d[2] == '3') {
        r.id3_size = 10 + ((static_cast<std::size_t>(d[6]) << 21) | (static_cast<std::size_t>(d[7]) << 14) | (static_cast<std::size_t>(d[8]) << 7) | d[9]);
        pos = r.id3_size;
    }
    if (pos + 4 > d.size()) return fail("no frames");
    std::vector<std::size_t> starts;
    std::vector<Header> headers;
    while (pos < d.size()) {
        if (pos + 4 > d.size()) return fail("truncated header at " + std::to_string(pos));
        Header h;
        if (!parse_header(&d[pos], h)) return fail("bad header at " + std::to_string(pos));
        if (pos + h.size > d.size()) return fail("frame overruns file at " + std::to_string(pos));
        starts.push_back(pos);
        headers.push_back(h);
        pos += h.size;
    }
    if (pos != d.size()) return fail("frame chain does not end at the end of file");
    const Header& first = headers[0];
    r.version = first.version;
    r.rate = first.rate;
    r.mode = first.mode;
    r.samples_per_frame = first.samples;
    for (const Header& h : headers) {
        if (h.version != first.version || h.rate != first.rate || h.mode != first.mode) return fail("inconsistent frame headers");
    }
    std::size_t audio_start = 0;
    const std::size_t xo = starts[0] + 4 + static_cast<std::size_t>(first.side_info);
    if (xo + 156 <= d.size() && (std::memcmp(&d[xo], "Xing", 4) == 0 || std::memcmp(&d[xo], "Info", 4) == 0)) {
        r.has_info = true;
        r.is_xing = std::memcmp(&d[xo], "Xing", 4) == 0;
        const std::uint32_t flags = be32(&d[xo + 4]);
        if ((flags & 0x0F) != 0x0F) return fail("Xing flags incomplete");
        r.xing_frames = be32(&d[xo + 8]);
        r.xing_bytes = be32(&d[xo + 12]);
        r.toc_ok = true;
        for (int i = 1; i < 100; ++i) {
            if (d[xo + 16 + static_cast<std::size_t>(i)] < d[xo + 15 + static_cast<std::size_t>(i)]) r.toc_ok = false;
        }
        const std::size_t l = xo + 120;
        if (std::memcmp(&d[l], "Fizmo", 5) != 0) return fail("missing Fizmo encoder tag");
        r.delay = (d[l + 21] << 4) | (d[l + 22] >> 4);
        r.padding = ((d[l + 22] & 15) << 8) | d[l + 23];
        const std::uint16_t stored = static_cast<std::uint16_t>((d[l + 34] << 8) | d[l + 35]);
        r.tag_crc_ok = stored == crc16(&d[starts[0]], l + 34 - starts[0]);
        const std::uint16_t music = static_cast<std::uint16_t>((d[l + 32] << 8) | d[l + 33]);
        const std::size_t music_from = starts[0] + headers[0].size;
        r.music_crc_ok = music == crc16(d.data() + music_from, d.size() - music_from);
        r.music_length_ok = be32(&d[l + 28]) == d.size() - r.id3_size;
        audio_start = 1;
    }
    r.audio_frames = headers.size() - audio_start;
    std::uint64_t total_bytes = 0;
    std::int64_t stream_pos = 0;
    std::int64_t prev_end = 0;
    std::vector<int> prev_block(2, 0);
    const bool lsf = first.version != 3;
    const int ngr = lsf ? 1 : 2;
    const int nch = first.channels;
    for (std::size_t f = audio_start; f < headers.size(); ++f) {
        const Header& h = headers[f];
        total_bytes += h.size;
        r.min_bitrate = std::min(r.min_bitrate, h.bitrate);
        r.max_bitrate = std::max(r.max_bitrate, h.bitrate);
        if (h.mode == 1 && (h.mode_ext & 2)) r.any_ms = true;
        if (h.mode != 1 && h.mode_ext != 0) return fail("mode extension set outside joint stereo");
        if (h.mode_ext & 1) return fail("intensity stereo is never used by the encoder");
        BitReader br(&d[starts[f] + 4], static_cast<std::size_t>(h.side_info));
        const int mdb = static_cast<int>(br.get(lsf ? 8 : 9));
        br.get(lsf ? (nch == 1 ? 1 : 2) : (nch == 1 ? 5 : 3));
        if (!lsf) {
            for (int c = 0; c < nch; ++c) {
                if (br.get(4) != 0) return fail("unexpected scfsi");
            }
        }
        long bits = 0;
        for (int gr = 0; gr < ngr; ++gr) {
            for (int c = 0; c < nch; ++c) {
                const int p23 = static_cast<int>(br.get(12));
                const int big = static_cast<int>(br.get(9));
                const int gain = static_cast<int>(br.get(8));
                br.get(lsf ? 9 : 4);
                bits += p23;
                if (big > 288) return fail("big_values out of range");
                if (gain > 255) return fail("global gain out of range");
                const int wsf = static_cast<int>(br.get(1));
                int block = 0;
                if (wsf) {
                    block = static_cast<int>(br.get(2));
                    const int mixed = static_cast<int>(br.get(1));
                    if (block == 0) return fail("window switching with block type 0");
                    if (mixed) return fail("mixed blocks are never used by the encoder");
                    for (int t = 0; t < 2; ++t) {
                        const int ts = static_cast<int>(br.get(5));
                        if (ts == 4 || ts == 14) return fail("invalid Huffman table");
                    }
                    br.get(9);
                } else {
                    for (int t = 0; t < 3; ++t) {
                        const int ts = static_cast<int>(br.get(5));
                        if (ts == 4 || ts == 14) return fail("invalid Huffman table");
                    }
                    const int r0 = static_cast<int>(br.get(4));
                    const int r1 = static_cast<int>(br.get(3));
                    if (r0 + r1 + 2 > 22) return fail("region counts exceed the band table");
                }
                if (!lsf) br.get(1);
                br.get(2);
                const int prev = prev_block[static_cast<std::size_t>(c)];
                if (block == 2) r.any_short = true;
                const bool prev_wants_short = prev == 1 || prev == 2;
                if (prev == 1 && block != 2) return fail("start block not followed by a short block");
                if (block == 1 && prev_wants_short) return fail("start block after a short block");
                if (block == 0 && prev_wants_short) return fail("long block directly after a short block");
                if (block == 3 && !prev_wants_short) return fail("stop block without a preceding short block");
                prev_block[static_cast<std::size_t>(c)] = block;
            }
        }
        const std::int64_t capacity = static_cast<std::int64_t>(h.size) - 4 - h.side_info;
        const std::int64_t data_start = stream_pos - mdb;
        if (data_start < 0) return fail("main_data_begin reaches before the first audio frame");
        if (data_start < prev_end) return fail("main data of consecutive frames overlap");
        const std::int64_t data_end = data_start + (bits + 7) / 8;
        if (data_end > stream_pos + capacity) return fail("main data overruns its frame");
        prev_end = data_end;
        stream_pos += capacity;
    }
    if (r.audio_frames) r.average_kbps = static_cast<double>(total_bytes) * 8.0 * first.rate / (static_cast<double>(r.audio_frames) * first.samples) / 1000.0;
    r.ok = true;
    return r;
}

std::uint64_t expected_frames(std::uint64_t samples, int samples_per_frame) {
    const std::uint64_t needed = samples + 528 + 529;
    const std::uint64_t spf = static_cast<std::uint64_t>(samples_per_frame);
    return std::max<std::uint64_t>(1, (needed + spf - 1) / spf);
}

void check_gapless(const StreamReport& r, std::uint64_t samples, const std::string& what) {
    check(r.ok, what + " parses (" + r.problem + ")");
    if (!r.ok) return;
    check(r.has_info, what + " has an Info/Xing frame");
    check(r.tag_crc_ok, what + " encoder tag CRC");
    check(r.music_crc_ok, what + " music CRC");
    check(r.music_length_ok, what + " music length");
    check(r.toc_ok, what + " TOC is monotonic");
    check(r.xing_frames == r.audio_frames, what + " Xing frame count matches the stream");
    const std::uint64_t frames = expected_frames(samples, r.samples_per_frame);
    check(r.audio_frames == frames, what + " frame count " + std::to_string(r.audio_frames) + " expected " + std::to_string(frames));
    check(r.delay == 528, what + " encoder delay");
    const std::int64_t pad = static_cast<std::int64_t>(r.audio_frames) * r.samples_per_frame - 528 - static_cast<std::int64_t>(samples);
    check(r.padding == pad, what + " padding " + std::to_string(r.padding) + " expected " + std::to_string(pad));
    check(pad >= 529, what + " padding covers the decoder delay");
}

void test_matrix() {
    const std::uint32_t rates[9] = { 44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000 };
    const Mp3Mode modes[4] = { Mp3Mode::stereo, Mp3Mode::joint_stereo, Mp3Mode::mono, Mp3Mode::automatic };
    const char* mode_names[4] = { "stereo", "joint", "mono", "auto" };
    for (std::uint32_t rate : rates) {
        const bool lsf = rate < 32000;
        const AudioBuffer input = test_signal(2, rate, rate / 2 + 37, rate);
        for (int m = 0; m < 4; ++m) {
            const int cbr[3] = { lsf ? 8 : 32, lsf ? 64 : 128, lsf ? 160 : 320 };
            for (int pass = 0; pass < 6; ++pass) {
                Mp3EncodeOptions o;
                o.mode = modes[m];
                std::string what = std::to_string(rate) + "/" + mode_names[m];
                if (pass < 3) {
                    o.bitrate_kbps = static_cast<std::uint32_t>(cbr[pass]);
                    what += "/cbr" + std::to_string(cbr[pass]);
                } else {
                    o.vbr = true;
                    o.vbr_quality = pass == 3 ? 0 : (pass == 4 ? 4 : 9);
                    what += "/V" + std::to_string(o.vbr_quality);
                }
                const std::vector<std::uint8_t> mp3 = encode_mp3(input, o);
                const StreamReport r = analyze_stream(mp3);
                check_gapless(r, input.frames(), what);
                if (!r.ok) continue;
                check(r.rate == static_cast<int>(rate), what + " sample rate kept");
                check(r.version == (rate >= 32000 ? 3 : (rate >= 16000 ? 2 : 0)), what + " MPEG version");
                const int expect_mode = m == 2 ? 3 : (m == 0 ? 0 : 1);
                check(r.mode == expect_mode, what + " channel mode");
                if (pass < 3) {
                    check(!r.is_xing, what + " CBR uses an Info tag");
                    check(r.min_bitrate == cbr[pass] && r.max_bitrate == cbr[pass], what + " constant bitrate");
                    check(std::fabs(r.average_kbps - cbr[pass]) < 0.02 * cbr[pass] + 0.5, what + " average bitrate");
                } else {
                    check(r.is_xing, what + " VBR uses a Xing tag");
                }
            }
        }
    }
}

void test_block_switching_and_ms() {
    const AudioBuffer input = test_signal(2, 44100, 44100, 9);
    Mp3EncodeOptions o;
    o.bitrate_kbps = 128;
    const StreamReport r = analyze_stream(encode_mp3(input, o));
    check(r.ok, "transient signal parses");
    check(r.any_short, "transients trigger short blocks");
    AudioBuffer centered(2, 44100, 44100);
    for (std::size_t i = 0; i < centered.frames(); ++i) {
        const float v = 0.4f * static_cast<float>(std::sin(2.0 * k_pi * 330.0 * static_cast<double>(i) / 44100.0));
        centered.at(i, 0) = v;
        centered.at(i, 1) = v;
    }
    const StreamReport c = analyze_stream(encode_mp3(centered, o));
    check(c.ok && c.any_ms, "identical channels use mid/side stereo");
    o.mode = Mp3Mode::stereo;
    const StreamReport s = analyze_stream(encode_mp3(centered, o));
    check(s.ok && !s.any_ms && s.mode == 0, "plain stereo never uses mid/side");
    AudioBuffer steady(1, 44100, 132300);
    for (std::size_t i = 0; i < steady.frames(); ++i) {
        const double t = static_cast<double>(i) / 44100.0;
        const double fade = t < 1.5 ? 0.5 - 0.5 * std::cos(k_pi * t / 1.5) : 1.0;
        steady.at(i, 0) = static_cast<float>(0.5 * fade * std::sin(2.0 * k_pi * 1000.0 * t));
    }
    Mp3EncodeOptions m;
    const StreamReport t = analyze_stream(encode_mp3(steady, m));
    check(t.ok && !t.any_short, "a steady tone stays in long blocks");
}

void test_edge_cases() {
    {
        const AudioBuffer empty(2, 44100, 0);
        const std::vector<std::uint8_t> mp3 = encode_mp3(empty);
        const StreamReport r = analyze_stream(mp3);
        check_gapless(r, 0, "empty buffer");
    }
    {
        const AudioBuffer none;
        check(encode_mp3(none).empty(), "a buffer without channels gives no output");
        std::string error;
        check(!write_mp3(temp_path("fizmo_mp3_none.mp3"), none, {}, &error) && !error.empty(), "write_mp3 reports an error for a buffer without channels");
        const AudioBuffer no_rate(2, 0, 100);
        check(encode_mp3(no_rate).empty(), "a buffer without a sample rate gives no output");
    }
    {
        AudioBuffer one(1, 44100, 1);
        one.at(0, 0) = 0.5f;
        const StreamReport r = analyze_stream(encode_mp3(one));
        check_gapless(r, 1, "one sample");
        check(r.ok && r.mode == 3, "one sample mono");
    }
    {
        const AudioBuffer six = test_signal(6, 48000, 24000, 3);
        const StreamReport r = analyze_stream(encode_mp3(six));
        check_gapless(r, 24000, "six channels");
        check(r.ok && r.mode != 3 && r.rate == 48000, "six channels are downmixed to stereo");
        Mp3EncodeOptions o;
        o.mode = Mp3Mode::mono;
        const StreamReport m = analyze_stream(encode_mp3(six, o));
        check(m.ok && m.mode == 3, "six channels downmixed to mono on request");
    }
    {
        const std::size_t n = 37000;
        const AudioBuffer odd = test_signal(2, 37000, n, 4);
        const StreamReport r = analyze_stream(encode_mp3(odd));
        const std::uint64_t out = (static_cast<std::uint64_t>(n) * 32000 + 36999) / 37000;
        check(r.ok && r.rate == 32000, "37000 Hz input is resampled to 32000 Hz");
        check_gapless(r, out, "37000 Hz input");
    }
    {
        const AudioBuffer high = test_signal(2, 96000, 9600, 5);
        const StreamReport r = analyze_stream(encode_mp3(high));
        check(r.ok && r.rate == 48000, "96000 Hz input is resampled to 48000 Hz");
        check_gapless(r, 4800, "96000 Hz input");
        const AudioBuffer low = test_signal(1, 5000, 5000, 6);
        const StreamReport l = analyze_stream(encode_mp3(low));
        check(l.ok && l.rate == 8000, "5000 Hz input is resampled to 8000 Hz");
        check_gapless(l, 8000, "5000 Hz input");
    }
    {
        const AudioBuffer silence(2, 44100, 44100);
        const std::vector<std::uint8_t> mp3 = encode_mp3(silence);
        const StreamReport r = analyze_stream(mp3);
        check_gapless(r, 44100, "silence");
    }
    {
        AudioBuffer square(2, 44100, 44100);
        for (std::size_t i = 0; i < square.frames(); ++i) {
            const float v = (i / 50) % 2 ? 1.0f : -1.0f;
            square.at(i, 0) = v;
            square.at(i, 1) = -v;
        }
        Mp3EncodeOptions o;
        o.bitrate_kbps = 320;
        check_gapless(analyze_stream(encode_mp3(square, o)), 44100, "full scale square wave 320k");
        o.bitrate_kbps = 32;
        check_gapless(analyze_stream(encode_mp3(square, o)), 44100, "full scale square wave 32k");
    }
    {
        Mp3EncodeOptions o;
        o.write_xing = false;
        const StreamReport r = analyze_stream(encode_mp3(test_signal(2, 44100, 10000, 7), o));
        check(r.ok && !r.has_info, "write_xing=false omits the Info frame");
        check(r.ok && r.audio_frames == expected_frames(10000, 1152), "frame count without Info frame");
    }
}

void test_bad_samples() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    AudioBuffer bad = test_signal(2, 44100, 20000, 8);
    AudioBuffer clean = bad;
    for (std::size_t i = 0; i < bad.frames(); i += 97) {
        bad.at(i, 0) = nan;
        clean.at(i, 0) = 0.0f;
        bad.at(i + 1 < bad.frames() ? i + 1 : i, 1) = inf;
        clean.at(i + 1 < clean.frames() ? i + 1 : i, 1) = 1.0f;
    }
    for (std::size_t i = 50; i < bad.frames(); i += 211) {
        bad.at(i, 0) = -inf;
        clean.at(i, 0) = -1.0f;
        bad.at(i, 1) = 1e30f;
        clean.at(i, 1) = 1.0f;
    }
    bad.at(3, 1) = -7.5f;
    clean.at(3, 1) = -1.0f;
    const std::vector<std::uint8_t> a = encode_mp3(bad);
    const std::vector<std::uint8_t> b = encode_mp3(clean);
    check_gapless(analyze_stream(a), 20000, "NaN/Inf input");
    check(a == b, "NaN, infinities and out-of-range samples are sanitized");
    AudioBuffer garbage(2, 22050, 30000);
    std::mt19937 rng(11);
    auto bits = [&](auto& gen) { return fizmo_test::random_int<std::uint32_t>(gen, 0, std::numeric_limits<std::uint32_t>::max()); };
    for (float& v : garbage.samples()) {
        std::uint32_t u = bits(rng);
        std::memcpy(&v, &u, sizeof(v));
    }
    Mp3EncodeOptions o;
    o.vbr = true;
    check_gapless(analyze_stream(encode_mp3(garbage, o)), 30000, "random bit pattern input");
}

void test_streaming_and_files() {
    const AudioBuffer input = test_signal(2, 44100, 70000, 12);
    Mp3EncodeOptions o;
    o.bitrate_kbps = 160;
    const std::vector<std::uint8_t> whole = encode_mp3(input, o);
    const std::string path = temp_path("fizmo_mp3_stream.mp3");
    Mp3Writer w;
    std::string error;
    check(w.open(path, 2, 44100, o, &error), "Mp3Writer opens (" + error + ")");
    check(w.is_open(), "Mp3Writer is open");
    check(w.output_sample_rate() == 44100 && w.output_channels() == 2, "Mp3Writer output format");
    check(w.encoder_delay() == 528, "Mp3Writer encoder delay");
    std::mt19937 rng(3);
    auto chunk = [&](auto& gen) { return fizmo_test::random_int<std::size_t>(gen, 1, 5000); };
    std::size_t done = 0;
    bool ok = true;
    while (done < input.frames()) {
        const std::size_t n = std::min(chunk(rng), input.frames() - done);
        ok = w.write(input.frame(done), n) && ok;
        done += n;
    }
    check(ok, "Mp3Writer accepts chunks");
    check(w.frames_written() == input.frames(), "Mp3Writer counts frames");
    check(w.close(&error), "Mp3Writer closes (" + error + ")");
    check(!w.is_open(), "Mp3Writer is closed");
    const std::vector<std::uint8_t> streamed = read_file(path);
    check(streamed == whole, "streaming output matches encode_mp3 byte for byte");
    const std::string path2 = temp_path("fizmo_mp3_whole.mp3");
    check(write_mp3(path2, input, o, &error), "write_mp3 succeeds (" + error + ")");
    check(read_file(path2) == whole, "write_mp3 output matches encode_mp3");
    check(encode_mp3(input, o) == whole, "encoding is deterministic");
    Mp3Writer resampling;
    check(resampling.open(temp_path("fizmo_mp3_resample.mp3"), 1, 44100, {}, &error), "writer opens for buffer writes");
    const AudioBuffer other_rate = test_signal(1, 22050, 11025, 13);
    check(resampling.write(other_rate), "writer resamples a buffer with another rate");
    const AudioBuffer wrong_channels = test_signal(2, 44100, 100, 13);
    check(!resampling.write(wrong_channels), "writer rejects a buffer with the wrong channel count");
    check(resampling.close(&error), "writer closes after buffer writes");
    const StreamReport r = analyze_stream(read_file(temp_path("fizmo_mp3_resample.mp3")));
    check_gapless(r, 22050, "buffer resampled through the writer");
    Mp3Writer closed;
    check(!closed.write(input.data(), 10), "a closed writer rejects writes");
    check(!closed.close(nullptr), "closing a writer that was never opened fails");
    check(!closed.open(temp_path("no_such_dir_fizmo/x/y.mp3"), 2, 44100, {}, &error) && !error.empty(), "opening an impossible path fails");
    std::remove(path.c_str());
    std::remove(path2.c_str());
    std::remove(temp_path("fizmo_mp3_resample.mp3").c_str());
}

std::string find_text_frame(const std::vector<std::uint8_t>& d, const char* id) {
    if (d.size() < 10 || d[3] != 4) return std::string();
    const std::size_t total = 10 + ((static_cast<std::size_t>(d[6]) << 21) | (static_cast<std::size_t>(d[7]) << 14) | (static_cast<std::size_t>(d[8]) << 7) | d[9]);
    std::size_t p = 10;
    while (p + 10 <= total) {
        const std::size_t size = (static_cast<std::size_t>(d[p + 4]) << 21) | (static_cast<std::size_t>(d[p + 5]) << 14) | (static_cast<std::size_t>(d[p + 6]) << 7) | d[p + 7];
        if (std::memcmp(&d[p], id, 4) == 0 && size >= 1 && d[p + 10] == 3) return std::string(reinterpret_cast<const char*>(&d[p + 11]), size - 1);
        p += 10 + size;
    }
    return std::string();
}

std::string find_smpb(const std::vector<std::uint8_t>& d) {
    if (d.size() < 10 || d[0] != 'I' || d[1] != 'D' || d[2] != '3' || d[3] != 4) return std::string();
    const std::size_t total = 10 + ((static_cast<std::size_t>(d[6]) << 21) | (static_cast<std::size_t>(d[7]) << 14) | (static_cast<std::size_t>(d[8]) << 7) | d[9]);
    std::size_t p = 10;
    while (p + 10 <= total && total <= d.size()) {
        const std::size_t size = (static_cast<std::size_t>(d[p + 4]) << 21) | (static_cast<std::size_t>(d[p + 5]) << 14) | (static_cast<std::size_t>(d[p + 6]) << 7) | d[p + 7];
        if (std::memcmp(&d[p], "COMM", 4) == 0 && size > 13 && d[p + 10] == 0 && std::memcmp(&d[p + 14], "iTunSMPB", 8) == 0 && d[p + 22] == 0) {
            return std::string(reinterpret_cast<const char*>(&d[p + 23]), size - 13);
        }
        p += 10 + size;
    }
    return std::string();
}

void check_smpb(const std::vector<std::uint8_t>& mp3, std::uint64_t samples, const std::string& what) {
    const std::string smpb = find_smpb(mp3);
    check(smpb.size() == 116, what + " has an iTunSMPB comment");
    if (smpb.size() != 116) return;
    const StreamReport r = analyze_stream(mp3);
    const unsigned long delay = std::strtoul(smpb.substr(10, 8).c_str(), nullptr, 16);
    const unsigned long padding = std::strtoul(smpb.substr(19, 8).c_str(), nullptr, 16);
    const unsigned long long count = std::strtoull(smpb.substr(28, 16).c_str(), nullptr, 16);
    check(delay == 528 && static_cast<int>(padding) == r.padding && count == samples, what + " iTunSMPB matches the Info tag (" + smpb.substr(0, 44) + ")");
}

void test_smpb() {
    const AudioBuffer input = test_signal(2, 44100, 30000, 21);
    check_smpb(encode_mp3(input), 30000, "default encode");
    Mp3EncodeOptions o;
    o.vbr = true;
    o.tags = { { "title", "gapless" }, { "comment", "kept first" } };
    const std::vector<std::uint8_t> tagged = encode_mp3(input, o);
    check_smpb(tagged, 30000, "tagged VBR encode");
    check(find_text_frame(tagged, "COMM") == std::string("eng\0kept first", 14), "the user comment stays the first text COMM frame");
    const std::string path = temp_path("fizmo_mp3_smpb.mp3");
    Mp3Writer w;
    std::string error;
    check(w.open(path, 2, 44100, {}, &error) && w.write(input) && w.close(&error), "writer encodes to a file");
    check_smpb(read_file(path), 30000, "file writer");
    Mp3EncodeOptions none;
    none.write_xing = false;
    check(find_smpb(encode_mp3(input, none)).empty(), "write_xing=false writes no iTunSMPB");
    std::remove(path.c_str());
}

void test_tags() {
    Mp3EncodeOptions o;
    o.tags = {
        { "title", "Grüße aus Fizmo" }, { "artist", "Christian" }, { "album", "Arbor" }, { "track", "3/12" },
        { "date", "2026" }, { "genre", "Electronic" }, { "comment", "made with fizmo" }, { "Mood", "calm" },
    };
    const std::vector<std::uint8_t> mp3 = encode_mp3(test_signal(2, 44100, 5000, 14), o);
    check(mp3.size() > 10 && mp3[0] == 'I' && mp3[1] == 'D' && mp3[2] == '3' && mp3[3] == 4 && mp3[4] == 0, "ID3v2.4 header");
    check(find_text_frame(mp3, "TIT2") == "Grüße aus Fizmo", "UTF-8 title");
    check(find_text_frame(mp3, "TPE1") == "Christian", "artist");
    check(find_text_frame(mp3, "TALB") == "Arbor", "album");
    check(find_text_frame(mp3, "TRCK") == "3/12", "track");
    check(find_text_frame(mp3, "TDRC") == "2026", "date");
    check(find_text_frame(mp3, "TCON") == "Electronic", "genre");
    check(find_text_frame(mp3, "COMM") == std::string("eng\0made with fizmo", 19), "comment");
    check(find_text_frame(mp3, "TXXX") == std::string("Mood\0calm", 9), "custom tag as TXXX");
    const StreamReport r = analyze_stream(mp3);
    check_gapless(r, 5000, "tagged file");
    check(r.ok && r.id3_size > 10, "frames follow the ID3 tag");
}

void test_checker_detects_damage() {
    const std::vector<std::uint8_t> good = encode_mp3(test_signal(2, 44100, 20000, 16));
    const StreamReport r = analyze_stream(good);
    check(r.ok && r.tag_crc_ok && r.music_crc_ok, "reference stream is clean");
    if (!r.ok || good.size() < 2000) return;
    std::vector<std::uint8_t> header = good;
    const std::size_t base = r.id3_size;
    Header info;
    check(parse_header(good.data() + base, info), "the Info frame header parses");
    const std::size_t at = base + info.size;
    header[at + 2] = static_cast<std::uint8_t>(header[at + 2] ^ 0x10);
    check(!analyze_stream(header).ok, "a damaged bitrate field breaks the frame chain");
    std::vector<std::uint8_t> tag = good;
    tag[base + 36 + 120 + 25] = static_cast<std::uint8_t>(tag[base + 36 + 120 + 25] ^ 1);
    const StreamReport t = analyze_stream(tag);
    check(t.ok && !t.tag_crc_ok, "a damaged encoder tag fails its CRC");
    std::vector<std::uint8_t> music = good;
    music[music.size() - 3] = static_cast<std::uint8_t>(music[music.size() - 3] ^ 0x55);
    const StreamReport m = analyze_stream(music);
    check(m.ok && !m.music_crc_ok, "damaged audio fails the music CRC");
    std::vector<std::uint8_t> reservoir = good;
    reservoir[at + 4] = 0xFF;
    reservoir[at + 5] = static_cast<std::uint8_t>(reservoir[at + 5] | 0x80);
    check(!analyze_stream(reservoir).ok, "an impossible main_data_begin is detected");
}

void test_helpers_and_rates() {
    check(mp3_encoder_sample_rate(44100) == 44100, "44100 is supported");
    check(mp3_encoder_sample_rate(37000) == 32000, "37000 maps to 32000");
    check(mp3_encoder_sample_rate(40000) == 44100, "40000 maps to 44100");
    check(mp3_encoder_sample_rate(192000) == 48000, "192000 maps to 48000");
    check(mp3_encoder_sample_rate(1000) == 8000, "1000 maps to 8000");
    check(mp3_encoder_sample_rate(23000) == 22050, "23000 maps to 22050");
    check(mp3_encoder_bitrate(44100, 192) == 192, "192 kbps at 44100");
    check(mp3_encoder_bitrate(44100, 1000) == 320, "bitrate clamps to 320");
    check(mp3_encoder_bitrate(44100, 1) == 32, "bitrate clamps to 32");
    check(mp3_encoder_bitrate(22050, 192) == 160, "MPEG-2 bitrate clamps to 160");
    check(mp3_encoder_bitrate(8000, 150) == 144, "nearest MPEG-2.5 bitrate");
    const AudioBuffer input = test_signal(2, 44100, 88200, 15);
    double previous = 1e9;
    bool decreasing = true;
    for (int v : { 0, 3, 6, 9 }) {
        Mp3EncodeOptions o;
        o.vbr = true;
        o.vbr_quality = v;
        const StreamReport r = analyze_stream(encode_mp3(input, o));
        check(r.ok, "VBR V" + std::to_string(v) + " parses");
        if (r.average_kbps > previous) decreasing = false;
        previous = r.average_kbps;
    }
    check(decreasing, "VBR bitrate falls as vbr_quality rises");
    for (int q : { 0, 5, 9 }) {
        Mp3EncodeOptions o;
        o.quality = q;
        o.bitrate_kbps = 96;
        check_gapless(analyze_stream(encode_mp3(input, o)), 88200, "quality " + std::to_string(q));
    }
}

} // namespace

int main() {
    test_helpers_and_rates();
    test_checker_detects_damage();
    test_edge_cases();
    test_bad_samples();
    test_block_switching_and_ms();
    test_tags();
    test_smpb();
    test_streaming_and_files();
    test_matrix();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
