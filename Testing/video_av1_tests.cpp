#include "../Source/Video/av1.hpp"
#include "../Source/Video/decoder.hpp"
#include "../Source/Video/demux.hpp"
#include "../Source/Video/player.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace fizmo::video;

namespace {

int g_passed = 0;
int g_failed = 0;
int g_skipped = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

class Md5 {
public:
    Md5() {
        m_state[0] = 0x67452301u;
        m_state[1] = 0xefcdab89u;
        m_state[2] = 0x98badcfeu;
        m_state[3] = 0x10325476u;
    }

    void update(const std::uint8_t* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            m_buffer[m_used++] = data[i];
            if (m_used == 64) {
                block(m_buffer);
                m_used = 0;
            }
        }
        m_length += size;
    }

    std::string hex() {
        std::uint64_t bits = m_length * 8;
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (m_used != 56) update(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (8 * i));
        update(len, 8);
        std::string out;
        char buf[3];
        for (int i = 0; i < 4; ++i) {
            for (int b = 0; b < 4; ++b) {
                std::snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned>((m_state[i] >> (8 * b)) & 0xFF));
                out += buf;
            }
        }
        return out;
    }

private:
    static std::uint32_t rotl(std::uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

    void block(const std::uint8_t* p) {
        static const std::uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
        };
        static const int r[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
        };
        std::uint32_t w[16];
        for (int i = 0; i < 16; ++i) {
            w[i] = static_cast<std::uint32_t>(p[i * 4]) | (static_cast<std::uint32_t>(p[i * 4 + 1]) << 8) |
                   (static_cast<std::uint32_t>(p[i * 4 + 2]) << 16) | (static_cast<std::uint32_t>(p[i * 4 + 3]) << 24);
        }
        std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f;
            int g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }
            std::uint32_t t = d;
            d = c;
            c = b;
            b = b + rotl(a + f + k[i] + w[g], r[i]);
            a = t;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    std::uint32_t m_state[4];
    std::uint8_t m_buffer[64];
    std::size_t m_used = 0;
    std::uint64_t m_length = 0;
};

std::string frame_md5(const VideoFrame& f) {
    Md5 m;
    for (int p = 0; p < plane_count(f.format); ++p) {
        const std::size_t row = static_cast<std::size_t>(f.plane_width(p)) * static_cast<std::size_t>(bytes_per_sample(f.format));
        for (std::uint32_t y = 0; y < f.plane_height(p); ++y) m.update(f.plane(p) + f.stride[p] * y, row);
    }
    return m.hex();
}

std::string data_dir() {
    const char* env = std::getenv("FIZMO_AV1_TEST_DATA");
    if (env && *env) {
        std::string d = env;
        if (d.back() != '/' && d.back() != '\\') d += '/';
        return d;
    }
    std::string f = __FILE__;
    for (char& c : f) {
        if (c == '\\') c = '/';
    }
    const std::size_t slash = f.rfind('/');
    std::string dir = slash == std::string::npos ? std::string("./") : f.substr(0, slash + 1);
    return dir + "video/av1/";
}

std::string temp_dir() {
    const char* dir = std::getenv("TMPDIR");
    if (!dir) dir = std::getenv("TMP");
    if (!dir) dir = std::getenv("TEMP");
#ifdef _WIN32
    if (!dir) dir = ".";
#else
    if (!dir) dir = "/tmp";
#endif
    std::string d = dir;
    if (d.back() != '/' && d.back() != '\\') d += '/';
    return d;
}

std::vector<std::uint8_t> load_file(const std::string& path) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    std::uint8_t buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return out;
}

std::string extension(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : name.substr(dot + 1);
}

struct Case {
    std::string file;
    std::string option;
    std::vector<std::string> md5;
};

std::vector<Case> load_manifest(const std::string& path) {
    std::vector<Case> out;
    std::ifstream in(path.c_str());
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        Case c;
        if (!(ls >> c.file >> c.option)) continue;
        std::string m;
        while (ls >> m) c.md5.push_back(m);
        out.push_back(c);
    }
    return out;
}

const Case* find_case(const std::vector<Case>& cases, const std::string& file, const std::string& option) {
    for (const Case& c : cases) {
        if (c.file == file && c.option == option) return &c;
    }
    return nullptr;
}

Av1Options options_for(const std::string& option, int threads) {
    Av1Options o;
    o.threads = threads;
    if (option == "nograin") o.apply_film_grain = false;
    if (option.compare(0, 3, "op=") == 0) o.operating_point = std::atoi(option.c_str() + 3);
    return o;
}

struct Decoded {
    std::vector<std::string> md5;
    std::vector<double> pts;
    std::vector<VideoFrame> frames;
    std::vector<int> per_unit;
    std::vector<bool> disposable;
    bool open = false;
    int failures = 0;
};

template <typename D>
void drain(D& dec, Decoded& out, bool keep) {
    VideoFrame f;
    while (dec.receive(f)) {
        out.md5.push_back(frame_md5(f));
        out.pts.push_back(f.pts);
        if (keep) out.frames.push_back(f);
    }
}

std::vector<std::vector<std::uint8_t>> temporal_units(const std::vector<std::uint8_t>& data, std::vector<double>* pts = nullptr) {
    std::vector<std::vector<std::uint8_t>> out;
    Av1FileReader reader;
    if (!reader.open(data.data(), data.size())) return out;
    std::vector<std::uint8_t> tu;
    double p = 0.0;
    while (reader.read(tu, &p)) {
        out.push_back(tu);
        if (pts) pts->push_back(p);
    }
    return out;
}

Decoded decode_units(const std::vector<std::vector<std::uint8_t>>& units, const Av1Options& options, bool keep = false, const std::vector<bool>* skip = nullptr) {
    Decoded out;
    Av1Decoder dec;
    out.open = dec.open();
    dec.set_options(options);
    for (std::size_t i = 0; i < units.size(); ++i) {
        out.disposable.push_back(dec.is_disposable(units[i].data(), units[i].size()));
        if (skip && i < skip->size() && (*skip)[i]) {
            out.per_unit.push_back(0);
            continue;
        }
        const std::size_t before = out.md5.size();
        if (!dec.decode(units[i].data(), units[i].size(), static_cast<double>(i), nullptr)) ++out.failures;
        drain(dec, out, keep);
        out.per_unit.push_back(static_cast<int>(out.md5.size() - before));
    }
    dec.flush();
    drain(dec, out, keep);
    return out;
}

Decoded decode_file_reader(const std::vector<std::uint8_t>& data, const Av1Options& options, bool keep = false) {
    Decoded out;
    Av1FileReader reader;
    if (!reader.open(data.data(), data.size())) return out;
    Av1Decoder dec;
    out.open = dec.open();
    dec.set_options(options);
    std::vector<std::uint8_t> tu;
    double pts = 0.0;
    while (reader.read(tu, &pts)) {
        if (!dec.decode(tu.data(), tu.size(), pts, nullptr)) ++out.failures;
        drain(dec, out, keep);
    }
    dec.flush();
    drain(dec, out, keep);
    return out;
}

Decoded decode_demuxed(const std::vector<std::uint8_t>& data, const Av1Options* options, std::string* format = nullptr) {
    Decoded out;
    Demuxer demux;
    std::string err;
    if (!demux.open(data.data(), data.size(), &err)) return out;
    if (format) *format = demux.format_name();
    const int index = demux.best_track(MediaType::video);
    if (index < 0) return out;
    const TrackInfo track = demux.tracks()[static_cast<std::size_t>(index)];
    if (track.codec != "av1") return out;
    const std::uint32_t chosen = static_cast<std::uint32_t>(index);
    for (std::size_t i = 0; i < demux.tracks().size(); ++i) demux.select(static_cast<std::uint32_t>(i), i == chosen);
    Packet pkt;
    if (options) {
        Av1Decoder dec;
        out.open = track.codec_config.empty() ? dec.open() : dec.open(track.codec_config.data(), track.codec_config.size(), &err);
        if (!out.open) return out;
        dec.set_options(*options);
        while (demux.read(pkt)) {
            if (pkt.track != chosen) continue;
            if (!dec.decode(pkt.data.data(), pkt.data.size(), pkt.pts, nullptr)) ++out.failures;
            drain(dec, out, false);
        }
        dec.flush();
        drain(dec, out, false);
        return out;
    }
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(track, &err);
    if (!dec) return out;
    out.open = true;
    while (demux.read(pkt)) {
        if (pkt.track != chosen) continue;
        if (!dec->decode_packet(pkt, nullptr)) ++out.failures;
        drain(*dec, out, false);
    }
    dec->flush();
    drain(*dec, out, false);
    return out;
}

bool same(const std::vector<std::string>& a, const std::vector<std::string>& b, std::string* detail = nullptr) {
    if (a.size() != b.size()) {
        if (detail) *detail = "frame count " + std::to_string(a.size()) + " vs reference " + std::to_string(b.size());
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            if (detail) *detail = "frame " + std::to_string(i) + " differs";
            return false;
        }
    }
    return true;
}

bool is_elementary(const std::string& ext) {
    return ext == "ivf" || ext == "obu" || ext == "av1b";
}

void test_manifest(const std::string& dir, const std::vector<Case>& cases) {
    int frames = 0;
    for (const Case& c : cases) {
        const std::vector<std::uint8_t> data = load_file(dir + c.file);
        const std::string label = c.file + (c.option == "-" ? std::string() : " [" + c.option + "]");
        if (data.empty()) {
            check(false, label + ": test file is readable");
            continue;
        }
        const std::string ext = extension(c.file);
        const Av1Options opts = options_for(c.option, 0);
        Decoded d = is_elementary(ext) ? decode_file_reader(data, opts) : decode_demuxed(data, &opts);
        std::string detail;
        check(d.open && same(d.md5, c.md5, &detail), label + ": bit-exact with libdav1d (" + std::to_string(c.md5.size()) + " frames) " + detail);
        frames += static_cast<int>(d.md5.size());
        if (is_elementary(ext)) {
            Decoded single = decode_file_reader(data, options_for(c.option, 1));
            check(same(single.md5, c.md5, &detail), label + ": single-threaded decode is bit-exact " + detail);
        }
        if (c.option == "-" && ext != "av1b") {
            std::string fmt;
            Decoded f = decode_demuxed(data, nullptr, &fmt);
            check(f.open && same(f.md5, c.md5, &detail), label + ": Demuxer (" + fmt + ") + create_video_decoder is bit-exact " + detail);
        }
    }
    std::printf("manifest: %zu cases, %d frames compared\n", cases.size(), frames);
}

void test_annexb(const std::string& dir, const std::vector<Case>& cases) {
    const std::vector<std::uint8_t> annexb = load_file(dir + "ii.av1b");
    const std::vector<std::uint8_t> ivf = load_file(dir + "ii.ivf");
    const Case* ref = find_case(cases, "ii.ivf", "-");
    check(detect_av1_stream(annexb.data(), annexb.size()) == Av1StreamFormat::annexb, "Annex B stream is detected as annexb");
    check(detect_av1_stream(ivf.data(), ivf.size()) == Av1StreamFormat::ivf, "IVF stream is detected as ivf");
    const std::vector<std::uint8_t> obu = load_file(dir + "gm.obu");
    check(detect_av1_stream(obu.data(), obu.size()) == Av1StreamFormat::obu, "low-overhead .obu stream is detected as obu");
    check(is_av1_obu_stream(obu.data(), obu.size()), "is_av1_obu_stream accepts a low-overhead stream");
    check(detect_container(ivf.data(), ivf.size()) == "av1", "detect_container reports av1 for IVF/AV01");
    check(detect_container(obu.data(), obu.size()) == "av1", "detect_container reports av1 for a .obu stream");
    if (!ref) return;
    Decoded d = decode_file_reader(annexb, Av1Options());
    check(same(d.md5, ref->md5), "Annex B (length-delimited) stream decodes bit-exact");
    Av1FileReader r;
    check(r.open(ivf.data(), ivf.size()) && r.format() == Av1StreamFormat::ivf && r.ivf_width() > 0 && r.frame_rate() > 0.0, "Av1FileReader reports IVF dimensions and frame rate");
}

void test_info(const std::string& dir) {
    struct Expect {
        const char* file;
        int depth;
        bool mono;
        int ssx;
        int ssy;
        PixelFormat format;
    };
    const Expect expect[] = {
        {"base8.ivf", 8, false, 1, 1, av1_pixel_format(8, false, 1, 1)},
        {"b10.ivf", 10, false, 1, 1, av1_pixel_format(10, false, 1, 1)},
        {"b12.ivf", 12, false, 1, 1, av1_pixel_format(12, false, 1, 1)},
        {"b444.ivf", 8, false, 0, 0, av1_pixel_format(8, false, 0, 0)},
        {"b422_10.ivf", 10, false, 1, 0, av1_pixel_format(10, false, 1, 0)},
        {"gray8.ivf", 8, true, 1, 1, av1_pixel_format(8, true, 1, 1)},
        {"gray10.ivf", 10, true, 1, 1, av1_pixel_format(10, true, 1, 1)},
    };
    for (const Expect& e : expect) {
        const std::vector<std::uint8_t> data = load_file(dir + e.file);
        Decoded d = decode_file_reader(data, Av1Options(), true);
        Av1Decoder dec;
        dec.open();
        auto units = temporal_units(data);
        if (!units.empty()) dec.decode(units[0].data(), units[0].size(), 0.0);
        const Av1Info& info = dec.info();
        check(info.bit_depth == e.depth && info.monochrome == e.mono && info.subsampling_x == e.ssx && info.subsampling_y == e.ssy, std::string(e.file) + ": sequence header colour config is reported");
        bool fmt_ok = !d.frames.empty() && e.format != PixelFormat::none;
        for (const VideoFrame& f : d.frames) {
            if (f.format != e.format || bit_depth(f.format) != e.depth || plane_count(f.format) != (e.mono ? 1 : 3)) fmt_ok = false;
            if (f.width != info.width || f.height != info.height) fmt_ok = false;
        }
        check(fmt_ok, std::string(e.file) + ": frames use pixel format " + format_name(e.format));
        bool range_ok = !d.frames.empty();
        const int maxv = (1 << e.depth) - 1;
        for (const VideoFrame& f : d.frames) {
            if (bytes_per_sample(f.format) != 2) break;
            for (int p = 0; p < plane_count(f.format) && range_ok; ++p) {
                for (std::uint32_t y = 0; y < f.plane_height(p) && range_ok; ++y) {
                    const std::uint8_t* row = f.plane(p) + f.stride[p] * y;
                    for (std::uint32_t x = 0; x < f.plane_width(p); ++x) {
                        const int v = row[2 * x] | (row[2 * x + 1] << 8);
                        if (v > maxv) {
                            range_ok = false;
                            break;
                        }
                    }
                }
            }
        }
        check(range_ok, std::string(e.file) + ": high bit depth samples are little-endian and LSB-aligned");
    }
    {
        const std::vector<std::uint8_t> data = load_file(dir + "still.ivf");
        Av1Decoder dec;
        dec.open();
        auto units = temporal_units(data);
        for (auto& u : units) dec.decode(u.data(), u.size(), 0.0);
        check(dec.info().still_picture && dec.frames_decoded() == 1, "still picture with reduced still picture header decodes");
    }
    {
        const std::vector<std::uint8_t> data = load_file(dir + "grain10.ivf");
        Av1Decoder dec;
        dec.open();
        auto units = temporal_units(data);
        for (auto& u : units) dec.decode(u.data(), u.size(), 0.0);
        check(dec.info().film_grain, "film grain parameters are reported");
    }
    {
        const std::vector<std::uint8_t> data = load_file(dir + "aom_svc.ivf");
        Av1Decoder dec;
        dec.open();
        auto units = temporal_units(data);
        if (!units.empty()) dec.decode(units[0].data(), units[0].size(), 0.0);
        check(dec.info().operating_points > 1, "scalable stream reports several operating points");
    }
    {
        const std::vector<std::uint8_t> data = load_file(dir + "svt_resize.ivf");
        Decoded d = decode_file_reader(data, Av1Options(), true);
        bool varied = false;
        for (const VideoFrame& f : d.frames) {
            if (!d.frames.empty() && (f.width != d.frames[0].width || f.height != d.frames[0].height)) varied = true;
        }
        check(varied, "frame size changes are output at the coded size of each frame");
    }
    {
        const std::vector<std::uint8_t> data = load_file(dir + "base8.mp4");
        Demuxer demux;
        bool ok = demux.open(data.data(), data.size());
        const int index = ok ? demux.best_track(MediaType::video) : -1;
        ok = ok && index >= 0 && demux.tracks()[static_cast<std::size_t>(index)].codec == "av1" && !demux.tracks()[static_cast<std::size_t>(index)].codec_config.empty();
        check(ok, "MP4 av01 sample entry exposes codec av1 with its av1C record");
        check(video_codec_supported("av1") && video_codec_display_name("av1") == "AV1", "av1 is listed as a supported video codec");
    }
}

void test_pts_and_reset(const std::string& dir, const std::vector<Case>& cases) {
    const std::vector<std::uint8_t> data = load_file(dir + "base8.ivf");
    const Case* ref = find_case(cases, "base8.ivf", "-");
    if (!ref) return;
    std::vector<double> pts;
    auto units = temporal_units(data, &pts);
    Av1Decoder dec;
    dec.open();
    Decoded out;
    for (std::size_t i = 0; i < units.size() / 2; ++i) {
        dec.decode(units[i].data(), units[i].size(), pts[i]);
        drain(dec, out, false);
    }
    dec.reset();
    VideoFrame f;
    check(!dec.receive(f), "reset() drops pending output");
    out = Decoded();
    for (std::size_t i = 0; i < units.size(); ++i) {
        dec.decode(units[i].data(), units[i].size(), pts[i]);
        drain(dec, out, false);
    }
    check(same(out.md5, ref->md5), "decoding restarts bit-exact after reset()");
    bool pts_ok = out.pts.size() == pts.size();
    for (std::size_t i = 0; pts_ok && i < pts.size(); ++i) pts_ok = out.pts[i] == pts[i];
    check(pts_ok, "presentation timestamps are carried to the output frames");
    Av1Decoder late;
    late.open();
    Decoded partial;
    int failures = 0;
    for (std::size_t i = 1; i < units.size(); ++i) {
        if (!late.decode(units[i].data(), units[i].size(), pts[i])) ++failures;
        drain(late, partial, false);
    }
    check(partial.md5.empty() && failures > 0, "a stream without its sequence header and key frame is rejected without output");
    for (std::size_t i = 0; i < units.size(); ++i) {
        late.decode(units[i].data(), units[i].size(), pts[i]);
        drain(late, partial, false);
    }
    check(same(partial.md5, ref->md5), "decoding recovers once a key frame arrives");
    std::vector<std::uint8_t> whole;
    for (auto& u : units) whole.insert(whole.end(), u.begin(), u.end());
    Av1Decoder one;
    one.open();
    Decoded all;
    one.decode(whole.data(), whole.size(), 0.0);
    drain(one, all, false);
    check(same(all.md5, ref->md5), "a whole OBU stream passed in one call decodes bit-exact");
}

void test_disposable(const std::string& dir, const std::vector<Case>& cases) {
    int dropped_total = 0;
    for (const char* name : {"base8.ivf", "svt_base.ivf", "rav1e.ivf", "aom_svct.ivf", "svt_open.ivf"}) {
        const std::vector<std::uint8_t> data = load_file(dir + name);
        const Case* ref = find_case(cases, name, "-");
        if (!ref) ref = find_case(cases, name, "op=0");
        if (!ref) continue;
        auto units = temporal_units(data);
        Decoded full = decode_units(units, Av1Options());
        std::vector<std::string> expect;
        std::size_t pos = 0;
        int dropped = 0;
        for (std::size_t i = 0; i < units.size(); ++i) {
            for (int k = 0; k < full.per_unit[i]; ++k) {
                if (!full.disposable[i]) expect.push_back(full.md5[pos]);
                ++pos;
            }
            if (full.disposable[i]) ++dropped;
        }
        dropped_total += dropped;
        Decoded skipped = decode_units(units, Av1Options(), false, &full.disposable);
        check(same(full.md5, ref->md5) && same(skipped.md5, expect), std::string(name) + ": dropping " + std::to_string(dropped) + " disposable units leaves the other frames bit-exact");
    }
    check(dropped_total > 0, "some temporal units are reported as disposable");
}

std::uint32_t g_rng = 0x12345678u;

std::uint32_t next_random() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

void mutate(std::vector<std::vector<std::uint8_t>>& units) {
    if (units.empty()) return;
    const int edits = 1 + static_cast<int>(next_random() % 8);
    for (int e = 0; e < edits; ++e) {
        std::vector<std::uint8_t>& u = units[next_random() % units.size()];
        if (u.empty()) continue;
        const std::size_t at = next_random() % u.size();
        switch (next_random() % 7) {
        case 0:
            u[at] ^= static_cast<std::uint8_t>(1u << (next_random() % 8));
            break;
        case 1:
            u[at] = static_cast<std::uint8_t>(next_random());
            break;
        case 2:
            u.resize(at);
            break;
        case 3: {
            const std::size_t len = std::min<std::size_t>(u.size() - at, 1 + next_random() % 16);
            for (std::size_t i = 0; i < len; ++i) u[at + i] = static_cast<std::uint8_t>(next_random());
            break;
        }
        case 4:
            u[at] = (next_random() & 1) ? 0xFF : 0x00;
            break;
        case 5: {
            const std::size_t len = std::min<std::size_t>(u.size() - at, 1 + next_random() % 32);
            u.erase(u.begin() + static_cast<std::ptrdiff_t>(at), u.begin() + static_cast<std::ptrdiff_t>(at + len));
            break;
        }
        default: {
            const std::size_t a = next_random() % units.size();
            const std::size_t b = next_random() % units.size();
            std::swap(units[a], units[b]);
            break;
        }
        }
    }
}

void test_fuzz(const std::string& dir, const std::vector<Case>& cases) {
    int iterations = 240;
    if (const char* env = std::getenv("FIZMO_AV1_FUZZ_ITERATIONS")) iterations = std::atoi(env);
    if (const char* env = std::getenv("FIZMO_AV1_FUZZ_SEED")) g_rng = static_cast<std::uint32_t>(std::strtoul(env, nullptr, 10)) | 1u;
    const char* names[] = {"base8.ivf", "gm.ivf", "ibc.ivf", "grain10.ivf", "b444_12.ivf", "tiles.ivf", "svt_resize.ivf", "sb128.ivf", "aom_svc.ivf", "ii.ivf", "svt_superres.ivf", "lossless.ivf", "gray10.ivf", "b422.ivf", "aq3.ivf", "errres.ivf"};
    const std::size_t count = sizeof(names) / sizeof(names[0]);
    std::vector<std::vector<std::vector<std::uint8_t>>> sources;
    for (const char* n : names) sources.push_back(temporal_units(load_file(dir + n)));
    double slowest = 0.0;
    std::uint64_t frames = 0;
    std::uint64_t errors = 0;
    Av1Decoder shared;
    shared.open();
    for (int it = 0; it < iterations; ++it) {
        std::vector<std::vector<std::uint8_t>> units = sources[static_cast<std::size_t>(it) % count];
        mutate(units);
        const auto t0 = std::chrono::steady_clock::now();
        Av1Decoder fresh;
        fresh.open();
        Av1Options o;
        o.threads = (it & 1) ? 1 : 0;
        o.apply_film_grain = (it & 2) == 0;
        fresh.set_options(o);
        Av1Decoder& dec = (it % 5 == 4) ? shared : fresh;
        VideoFrame f;
        for (auto& u : units) {
            dec.decode(u.data(), u.size(), 0.0);
            while (dec.receive(f)) {
                ++frames;
                if (f.data.size() < f.offset[0]) ++errors;
            }
        }
        errors += dec.error_count();
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (s > slowest) slowest = s;
    }
    std::printf("fuzz: %d mutated streams, %llu frames output, %llu decode errors, slowest %.2fs\n", iterations, static_cast<unsigned long long>(frames), static_cast<unsigned long long>(errors), slowest);
    check(slowest < 60.0, "mutated streams never hang the decoder");
    const Case* ref = find_case(cases, "gm.ivf", "-");
    if (ref) {
        shared.reset();
        Decoded out;
        for (auto& u : sources[1]) {
            shared.decode(u.data(), u.size(), 0.0);
            drain(shared, out, false);
        }
        check(same(out.md5, ref->md5), "after decoding corrupt data and reset(), a clean stream decodes bit-exact");
    }
}

#ifdef _WIN32
const char* k_null = "NUL";
#else
const char* k_null = "/dev/null";
#endif

bool run(const std::string& cmd) {
    const std::string full = cmd + " >" + k_null + " 2>&1";
    return std::system(full.c_str()) == 0;
}

bool has_encoder(const std::string& name) {
    return run("ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=s=64x64:d=0.04 -frames:v 1 -c:v " + name + " -f null -");
}

std::vector<std::string> dav1d_md5(const std::string& path, const std::string& extra) {
    std::vector<std::string> out;
    const std::string txt = path + ".md5.txt";
    if (!run("ffmpeg -hide_banner -loglevel error -y " + extra + " -c:v libdav1d -i \"" + path + "\" -f framemd5 -autoscale 0 -fps_mode passthrough \"" + txt + "\"")) return out;
    std::ifstream in(txt.c_str());
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::size_t comma = line.rfind(',');
        if (comma == std::string::npos) continue;
        std::string m = line.substr(comma + 1);
        while (!m.empty() && (m[0] == ' ' || m[0] == '\t')) m.erase(0, 1);
        while (!m.empty() && (m.back() == '\r' || m.back() == ' ')) m.pop_back();
        out.push_back(m);
    }
    in.close();
    std::remove(txt.c_str());
    return out;
}

void test_player(const std::string& dir, const std::vector<Case>& cases) {
    struct Entry {
        const char* file;
        const char* ref;
        const char* container;
    };
    const Entry entries[] = {
        {"gm.ivf", "gm.ivf", "ivf"},
        {"gm.obu", "gm.obu", "obu"},
        {"ii.av1b", "ii.ivf", "av1 annexb"},
        {"base8.mp4", "base8.ivf", "mp4"},
    };
    for (const Entry& e : entries) {
        const std::string path = dir + e.file;
        const Case* ref = find_case(cases, e.ref, "-");
        MediaInfo mi = probe_media(path);
        check(mi.valid && mi.container == e.container && mi.video_codec == "av1" && mi.video_supported && mi.tracks.size() == 1 && mi.width > 0, std::string("probe ") + e.file + " (" + mi.container + ")");
        const std::vector<std::uint8_t> data = load_file(path);
        for (int from_memory = 0; from_memory < 2; ++from_memory) {
            const std::string how = std::string(e.file) + (from_memory ? " from memory" : " from a file");
            MediaPlayer p;
            std::string err;
            const bool opened = from_memory ? p.open(data.data(), data.size(), &err) : p.open(path, &err);
            check(opened && p.has_video() && !p.has_audio() && p.width() == mi.width && p.height() == mi.height && p.video_error().empty(), "MediaPlayer opens " + how + " with video " + err + p.video_error());
            const VideoFrame* f = nullptr;
            std::uint64_t serial = 0;
            bool shown = false;
            for (int i = 0; i < 4000 && !shown; ++i) {
                p.current_frame(f, serial);
                shown = f != nullptr && !p.is_seeking();
                if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            check(shown, "MediaPlayer shows a frame of " + how);
            if (shown && ref && !ref->md5.empty()) check(frame_md5(*f) == ref->md5[0], "MediaPlayer's first frame of " + how + " matches the reference");
            p.play();
            const bool ended = [&]() {
                for (int i = 0; i < 8000; ++i) {
                    if (p.ended()) return true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return p.ended();
            }();
            check(ended && p.stats().frames_decoded > 1, "MediaPlayer plays " + how + " to the end");
            p.close();
        }
    }
}

void test_ffmpeg() {
    if (!run("ffmpeg -hide_banner -version") || !has_encoder("libaom-av1") || !run("ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=s=64x64:d=0.04 -frames:v 1 -c:v libaom-av1 -f ivf - | ffmpeg -hide_banner -loglevel error -c:v libdav1d -f ivf -i - -f null -")) {
        std::printf("ffmpeg with libaom-av1 and libdav1d not found; skipping generated-stream tests\n");
        ++g_skipped;
        return;
    }
    struct Gen {
        const char* name;
        const char* encoder;
        const char* source;
        const char* pix;
        const char* args;
        bool grain;
    };
    const Gen gens[] = {
        {"aom8", "libaom-av1", "testsrc2=s=160x96:r=25", "yuv420p", "-cpu-used 8 -crf 40 -g 8", false},
        {"aom10", "libaom-av1", "mandelbrot=s=96x80:r=25", "yuv420p10le", "-cpu-used 7 -crf 30 -g 6 -tiles 2x1", false},
        {"aom444", "libaom-av1", "testsrc=s=72x56:r=25", "yuv444p", "-cpu-used 8 -crf 35 -aom-params enable-chroma-deltaq=1", false},
        {"aom422_12", "libaom-av1", "rgbtestsrc=s=64x48:r=25", "yuv422p12le", "-cpu-used 8 -crf 32", false},
        {"aomgrain", "libaom-av1", "testsrc2=s=96x64:r=25,noise=alls=30:allf=t", "yuv420p", "-cpu-used 8 -crf 38 -denoise-noise-level 20", true},
        {"aomscreen", "libaom-av1", "testsrc=s=128x64:r=25", "yuv420p", "-cpu-used 8 -crf 30 -aom-params tune-content=screen:enable-palette=1:enable-intrabc=1", false},
        {"svt", "libsvtav1", "testsrc2=s=192x128:r=25", "yuv420p", "-preset 10 -crf 40 -g 16", false},
        {"svt10", "libsvtav1", "mandelbrot=s=160x96:r=25", "yuv420p10le", "-preset 11 -crf 35", false},
        {"rav1e", "librav1e", "testsrc2=s=128x80:r=25", "yuv420p", "-speed 10 -qp 120", false},
    };
    const char* containers[] = {"ivf", "mp4", "mkv", "webm", "obu"};
    const std::string base = temp_dir() + "fizmo_av1_test_";
    int streams = 0;
    for (const Gen& g : gens) {
        if (!has_encoder(g.encoder)) {
            std::printf("%s not available in ffmpeg; skipping %s\n", g.encoder, g.name);
            ++g_skipped;
            continue;
        }
        const std::string ivf = base + g.name + ".ivf";
        if (!run(std::string("ffmpeg -hide_banner -loglevel error -y -f lavfi -i ") + g.source + " -frames:v 9 -pix_fmt " + g.pix + " -c:v " + g.encoder + " " + g.args + " \"" + ivf + "\"")) {
            check(false, std::string(g.name) + ": ffmpeg encode");
            continue;
        }
        ++streams;
        const std::vector<std::string> ref = dav1d_md5(ivf, "");
        for (const char* c : containers) {
            const std::string path = base + g.name + "." + c;
            if (std::string(c) != "ivf" && !run("ffmpeg -hide_banner -loglevel error -y -i \"" + ivf + "\" -c copy" + (std::string(c) == "obu" ? " -f obu" : "") + " \"" + path + "\"")) {
                check(false, std::string(g.name) + ": ffmpeg remux to " + c);
                continue;
            }
            const std::vector<std::uint8_t> data = load_file(path);
            std::string detail;
            Decoded d = decode_demuxed(data, nullptr);
            check(!ref.empty() && d.open && same(d.md5, ref, &detail), std::string(g.name) + "." + c + ": Demuxer + create_video_decoder matches libdav1d " + detail);
            if (std::string(c) == "ivf" || std::string(c) == "obu") {
                Decoded r = decode_file_reader(data, Av1Options());
                check(same(r.md5, ref, &detail), std::string(g.name) + "." + c + ": Av1FileReader decode matches libdav1d " + detail);
            }
            if (std::string(c) != "ivf") std::remove(path.c_str());
        }
        if (g.grain) {
            const std::vector<std::string> ng = dav1d_md5(ivf, "-filmgrain 0");
            const std::vector<std::uint8_t> data = load_file(ivf);
            Av1Options o;
            o.apply_film_grain = false;
            Decoded d = decode_file_reader(data, o);
            std::string detail;
            check(!ng.empty() && same(d.md5, ng, &detail), std::string(g.name) + ": film grain disabled matches libdav1d " + detail);
        }
        std::remove(ivf.c_str());
    }
    std::printf("ffmpeg: %d generated streams checked in %zu containers\n", streams, sizeof(containers) / sizeof(containers[0]));
}

} // namespace

int main() {
    const std::string dir = data_dir();
    const std::vector<Case> cases = load_manifest(dir + "manifest.txt");
    check(!cases.empty(), "the test manifest " + dir + "manifest.txt is readable");
    if (!cases.empty()) {
        test_manifest(dir, cases);
        test_annexb(dir, cases);
        test_info(dir);
        test_pts_and_reset(dir, cases);
        test_disposable(dir, cases);
        test_fuzz(dir, cases);
        test_player(dir, cases);
    }
    if (!std::getenv("FIZMO_AV1_SKIP_FFMPEG")) test_ffmpeg();
    std::printf("av1 tests: %d passed, %d failed, %d skipped\n", g_passed, g_failed, g_skipped);
    return g_failed == 0 ? 0 : 1;
}
