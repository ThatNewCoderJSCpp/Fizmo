#include "../Source/Video/decoder.hpp"
#include "../Source/Video/demux.hpp"
#include "../Source/Video/player.hpp"
#include "../Source/Video/vp9.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
        std::fflush(stdout);
    }
}

class Md5 {
public:
    void update(const std::uint8_t* data, std::size_t size) {
        m_length += size;
        for (std::size_t i = 0; i < size; ++i) {
            m_buffer[m_used++] = data[i];
            if (m_used == 64) {
                block();
                m_used = 0;
            }
        }
    }

    std::string hex() {
        const std::uint64_t bits = m_length * 8;
        const std::uint8_t one = 0x80;
        const std::uint8_t zero = 0;
        update(&one, 1);
        while (m_used != 56) update(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (8 * i));
        update(len, 8);
        static const char digits[] = "0123456789abcdef";
        std::string out;
        for (int i = 0; i < 4; ++i) {
            for (int b = 0; b < 4; ++b) {
                const std::uint8_t v = static_cast<std::uint8_t>(m_state[i] >> (8 * b));
                out += digits[v >> 4];
                out += digits[v & 15];
            }
        }
        return out;
    }

private:
    static std::uint32_t rotl(std::uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

    void block() {
        static const std::uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static const int r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        std::uint32_t w[16];
        for (int i = 0; i < 16; ++i) w[i] = static_cast<std::uint32_t>(m_buffer[i * 4]) | (static_cast<std::uint32_t>(m_buffer[i * 4 + 1]) << 8) | (static_cast<std::uint32_t>(m_buffer[i * 4 + 2]) << 16) | (static_cast<std::uint32_t>(m_buffer[i * 4 + 3]) << 24);
        std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f;
            int g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) & 15;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) & 15;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) & 15;
            }
            const std::uint32_t t = d;
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

    std::uint32_t m_state[4] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};
    std::uint8_t m_buffer[64] = {};
    std::size_t m_used = 0;
    std::uint64_t m_length = 0;
};

std::string data_dir() {
    const char* env = std::getenv("FIZMO_VP9_TEST_DATA");
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
    return (slash == std::string::npos ? std::string("./") : f.substr(0, slash + 1)) + "video/vp9/";
}

std::string temp_prefix() {
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
    static const std::string tag = std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count() % 1000000007));
    return d + "fizmo_vp9_" + tag + "_";
}

std::vector<std::uint8_t> load_file(const std::string& path) {
    std::vector<std::uint8_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    std::uint8_t buf[65536];
    std::size_t got;
    while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + got);
    std::fclose(f);
    return out;
}

void append_frame(const VideoFrame& f, std::vector<std::uint8_t>* raw, Md5* md5) {
    const int bps = bytes_per_sample(f.format);
    const int planes = plane_count(f.format);
    for (int p = 0; p < planes; ++p) {
        for (std::uint32_t y = 0; y < f.plane_height(p); ++y) {
            const std::uint8_t* row = f.plane(p) + y * f.stride[p];
            const std::size_t n = static_cast<std::size_t>(f.plane_width(p)) * static_cast<std::size_t>(bps);
            if (md5) md5->update(row, n);
            if (raw) raw->insert(raw->end(), row, row + n);
        }
    }
}

struct DecodeResult {
    bool opened = false;
    std::string error;
    std::string md5;
    std::size_t frames = 0;
    std::uint64_t errors = 0;
    std::vector<std::uint8_t> raw;
    std::vector<VideoFrame> keep;
    std::string container;
    TrackInfo track;
};

DecodeResult decode_demuxed(Demuxer& dm, bool keep_raw, bool keep_frames) {
    DecodeResult r;
    r.container = dm.format_name();
    const int vt = dm.best_track(MediaType::video);
    if (vt < 0) {
        r.error = "no video track";
        return r;
    }
    r.track = dm.tracks()[static_cast<std::size_t>(vt)];
    for (std::size_t i = 0; i < dm.tracks().size(); ++i) dm.select(static_cast<std::uint32_t>(i), static_cast<int>(i) == vt);
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(r.track, &r.error);
    if (!dec) return r;
    r.opened = true;
    Md5 md5;
    VideoFrame f;
    Packet pkt;
    auto drain = [&]() {
        while (dec->receive(f)) {
            ++r.frames;
            append_frame(f, keep_raw ? &r.raw : nullptr, &md5);
            if (keep_frames) r.keep.push_back(f);
        }
    };
    while (dm.read(pkt)) {
        if (static_cast<int>(pkt.track) != vt) continue;
        std::string err;
        dec->decode_packet(pkt, &err);
        drain();
    }
    dec->flush();
    drain();
    r.errors = dec->error_count();
    r.md5 = md5.hex();
    return r;
}

DecodeResult decode_path(const std::string& path, bool keep_raw = false, bool keep_frames = false) {
    Demuxer dm;
    DecodeResult r;
    if (!dm.open(path, &r.error)) return r;
    return decode_demuxed(dm, keep_raw, keep_frames);
}

DecodeResult decode_memory(const std::vector<std::uint8_t>& data) {
    Demuxer dm;
    DecodeResult r;
    if (!dm.open(data.data(), data.size(), &r.error)) return r;
    return decode_demuxed(dm, false, false);
}

struct Asset {
    std::string name;
    std::size_t frames = 0;
    std::string md5;
};

std::vector<Asset> load_manifest() {
    std::vector<Asset> out;
    const std::vector<std::uint8_t> text = load_file(data_dir() + "md5.txt");
    std::istringstream in(std::string(text.begin(), text.end()));
    Asset a;
    while (in >> a.name >> a.frames >> a.md5) out.push_back(a);
    return out;
}

void test_api() {
    check(video_codec_supported("vp9"), "vp9 supported");
    check(video_codec_display_name("vp9") == "VP9", "vp9 display name: " + video_codec_display_name("vp9"));
    const std::vector<std::string> list = supported_video_codecs();
    check(std::find(list.begin(), list.end(), "vp9") != list.end(), "vp9 listed");
    TrackInfo t;
    t.type = MediaType::video;
    t.codec = "vp9";
    std::string err;
    const std::uint8_t inter_first[] = {0x86, 0x00, 0x40, 0x00};
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(t, &err);
    check(dec != nullptr && std::strcmp(dec->codec(), "vp9") == 0, "create vp9 decoder " + err);
    if (dec) {
        VideoFrame f;
        check(!dec->receive(f), "nothing to receive initially");
        const std::uint8_t junk[] = {0x00, 0x11, 0x22, 0x33, 0x44};
        check(!dec->decode(junk, sizeof(junk), 0.0, &err) && !err.empty(), "garbage rejected");
        check(!dec->receive(f), "no frame from garbage");
        check(!dec->decode(junk, 0, 0.0, &err) || true, "empty packet tolerated");
        check(!dec->decode(inter_first, sizeof(inter_first), 0.0, &err), "inter frame before keyframe rejected");
        check(dec->error_count() >= 1, "errors counted");
    }

    Vp9CodecConfig cfg;
    const std::uint8_t vpcc1[] = {1, 0, 0, 0, 2, 31, (10 << 4) | (1 << 1) | 1, 9, 16, 9, 0, 0};
    check(parse_vp9_codec_config(vpcc1, sizeof(vpcc1), cfg) && cfg.profile == 2 && cfg.level == 31 && cfg.bit_depth == 10 && cfg.chroma_subsampling == 1 && cfg.full_range == 1 && cfg.primaries == 9 && cfg.transfer == 16 && cfg.matrix == 9, "vpcC version 1");
    const std::uint8_t vpcc0[] = {0, 0, 0, 0, 1, 10, (8 << 4) | 2, 0x21};
    check(parse_vp9_codec_config(vpcc0, sizeof(vpcc0), cfg) && cfg.profile == 1 && cfg.bit_depth == 8 && cfg.chroma_subsampling == 2 && cfg.full_range == 1, "vpcC version 0");
    const std::uint8_t mkv[] = {1, 1, 3, 2, 1, 41, 3, 1, 12, 4, 1, 3};
    check(parse_vp9_codec_config(mkv, sizeof(mkv), cfg) && cfg.profile == 3 && cfg.level == 41 && cfg.bit_depth == 12 && cfg.chroma_subsampling == 3, "matroska CodecPrivate");
    check(!parse_vp9_codec_config(nullptr, 0, cfg), "empty config");

    std::vector<std::uint8_t> sf = {0x82, 0x49, 0x83, 0x42, 0x00, 0xAA, 0xBB, 0xCC};
    const std::uint8_t marker = 0xC0 | (0 << 3) | 1;
    sf.push_back(marker);
    sf.push_back(5);
    sf.push_back(3);
    sf.push_back(marker);
    std::size_t off[8];
    std::size_t sz[8];
    check(vp9_split_superframe(sf.data(), sf.size(), off, sz, 8) == 2 && off[0] == 0 && sz[0] == 5 && off[1] == 5 && sz[1] == 3, "superframe split");
    const std::uint8_t plain[] = {0x82, 0x49, 0x83, 0x42};
    check(vp9_split_superframe(plain, sizeof(plain), off, sz, 8) == 1 && sz[0] == 4, "plain frame not split");
    sf[sf.size() - 2] = 200;
    check(vp9_split_superframe(sf.data(), sf.size(), off, sz, 8) <= 1, "oversized superframe entry rejected");
    check(vp9_is_keyframe(plain, sizeof(plain)), "keyframe detected");
    check(!vp9_is_keyframe(inter_first, sizeof(inter_first)), "inter frame not keyframe");
}

void test_assets() {
    const std::vector<Asset> assets = load_manifest();
    check(assets.size() >= 15, "manifest loaded from " + data_dir());
    for (const Asset& a : assets) {
        const auto t0 = std::chrono::steady_clock::now();
        DecodeResult r = decode_path(data_dir() + a.name);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        check(r.opened, a.name + ": opened " + r.error);
        check(r.frames == a.frames, a.name + ": frame count " + std::to_string(r.frames) + " expected " + std::to_string(a.frames));
        check(r.md5 == a.md5, a.name + ": bit-exact md5 " + r.md5);
        check(r.errors == 0, a.name + ": no decode errors");
        check(r.track.codec == "vp9", a.name + ": codec vp9");
        const std::string ext = a.name.substr(a.name.rfind('.') + 1);
        if (ext == "ivf") check(r.container == "ivf", a.name + ": ivf container " + r.container);
        if (ext == "webm") check(r.container == "webm", a.name + ": webm container " + r.container);
        if (ext == "mp4") check(r.container == "mp4", a.name + ": mp4 container " + r.container);
        (void)ms;
    }
}

void test_formats_and_color() {
    struct Expect {
        const char* name;
        PixelFormat format;
        ColorMatrix matrix;
    };
    const Expect ex[] = {
        {"base.ivf", PixelFormat::yuv420p, ColorMatrix::unspecified},
        {"p1_422.webm", PixelFormat::yuv422p, ColorMatrix::unspecified},
        {"p1_440.ivf", PixelFormat::yuv440p, ColorMatrix::unspecified},
        {"p1_444.mp4", PixelFormat::yuv444p, ColorMatrix::bt709},
        {"p1_gbrp.ivf", PixelFormat::yuv444p, ColorMatrix::rgb},
        {"p2_10.ivf", PixelFormat::yuv420p10, ColorMatrix::unspecified},
        {"p2_12.webm", PixelFormat::yuv420p12, ColorMatrix::unspecified},
        {"p3_422_10.ivf", PixelFormat::yuv422p10, ColorMatrix::unspecified},
        {"p3_444_12.mp4", PixelFormat::yuv444p12, ColorMatrix::unspecified},
        {"p3_440_12.ivf", PixelFormat::yuv440p12, ColorMatrix::unspecified},
        {"p3_gbrp10.ivf", PixelFormat::yuv444p10, ColorMatrix::rgb},
    };
    for (const Expect& e : ex) {
        DecodeResult r = decode_path(data_dir() + e.name, false, true);
        if (r.keep.empty()) {
            check(false, std::string(e.name) + ": frames for format check");
            continue;
        }
        const VideoFrame& f = r.keep.front();
        check(f.format == e.format, std::string(e.name) + ": pixel format " + format_name(f.format));
        if (e.matrix != ColorMatrix::unspecified) check(f.color.matrix == e.matrix, std::string(e.name) + ": color matrix");
        if (e.matrix == ColorMatrix::rgb) check(f.color.range == ColorRange::full, std::string(e.name) + ": rgb is full range");
        if (e.matrix == ColorMatrix::bt709) check(f.color.range == ColorRange::full && f.color.matrix_coefficients == 1, std::string(e.name) + ": bt709 full range");
    }
    DecodeResult s = decode_path(data_dir() + "scaled.ivf", false, true);
    const std::uint32_t sizes[][2] = {{352, 288}, {256, 192}, {512, 384}, {448, 320}, {448, 320}, {320, 256}, {320, 256}};
    bool ok = s.keep.size() == 7;
    for (std::size_t i = 0; ok && i < 7; ++i) ok = s.keep[i].width == sizes[i][0] && s.keep[i].height == sizes[i][1];
    check(ok, "reference scaling stream changes frame size");
    DecodeResult b = decode_path(data_dir() + "base.webm", false, true);
    bool mono = b.keep.size() == 24;
    for (std::size_t i = 1; mono && i < b.keep.size(); ++i) mono = b.keep[i].pts > b.keep[i - 1].pts;
    check(mono, "hidden alt-ref frames produce monotonic pts");
    check(!b.keep.empty() && b.keep.front().keyframe, "first frame is a keyframe");
}

void test_direct_api() {
    const std::vector<std::uint8_t> file = load_file(data_dir() + "p2_10.ivf");
    Demuxer dm;
    check(dm.open(file.data(), file.size()), "demux p2_10 from memory");
    Vp9Decoder dec;
    check(dec.open(), "Vp9Decoder open");
    dec.set_threads(3);
    check(dec.threads() == 3, "threads set");
    Packet pkt;
    VideoFrame f;
    std::size_t frames = 0;
    while (dm.read(pkt)) {
        std::string err;
        check(dec.decode(pkt.data.data(), pkt.data.size(), pkt.pts, &err), "direct decode " + err);
        while (dec.receive(f)) ++frames;
    }
    check(frames == 10, "direct frames");
    check(dec.info().profile == 2 && dec.info().bit_depth == 10 && dec.info().width == 128 && dec.info().height == 96, "Vp9Info populated");
    check(dec.frames_decoded() == 10, "frames_decoded");
    dec.reset();
    check(!dec.receive(f), "reset drops output");
    dec.close();
    check(!dec.is_open(), "closed");
}

void test_threads_and_seek() {
    const std::vector<Asset> assets = load_manifest();
    for (const Asset& a : assets) {
        if (a.name != "tiles.ivf" && a.name != "base.ivf") continue;
        const std::vector<std::uint8_t> file = load_file(data_dir() + a.name);
        for (int threads = 1; threads <= 4; threads += 3) {
            Demuxer dm;
            dm.open(file.data(), file.size());
            Vp9Decoder dec;
            dec.open();
            dec.set_threads(threads);
            Md5 md5;
            Packet pkt;
            VideoFrame f;
            while (dm.read(pkt)) {
                dec.decode(pkt.data.data(), pkt.data.size(), pkt.pts);
                while (dec.receive(f)) append_frame(f, nullptr, &md5);
            }
            dec.flush();
            while (dec.receive(f)) append_frame(f, nullptr, &md5);
            check(md5.hex() == a.md5, a.name + ": bit-exact with " + std::to_string(threads) + " threads");
        }
    }
    DecodeResult all = decode_path(data_dir() + "base.ivf", false, true);
    for (const char* name : {"base.ivf", "base.webm", "base.mp4"}) {
        VideoFrame f;
        std::string err;
        const std::string path = data_dir() + name;
        const bool ok = extract_frame(path, 0.62, f, &err);
        check(ok, std::string(name) + ": extract_frame " + err);
        if (ok && all.keep.size() > 15) {
            bool same = false;
            for (const VideoFrame& k : all.keep) same = same || (k.pts == f.pts && k.data.size() == f.data.size() && std::equal(f.data.begin(), f.data.end(), k.data.begin()));
            check(same, std::string(name) + ": extracted frame matches sequential decode");
            check(std::fabs(f.pts - 0.6) < 0.021, std::string(name) + ": extracted frame time " + std::to_string(f.pts));
        }
        MediaInfo mi = probe_media(path);
        check(mi.valid && mi.video_codec == "vp9" && mi.video_supported && mi.width == 176 && mi.height == 144, std::string(name) + ": probe");
    }
    MediaPlayer p;
    std::string err;
    check(p.open(data_dir() + "p1_444.mp4", &err) && p.has_video() && p.width() == 128 && p.height() == 96, "MediaPlayer opens vp9 mp4 " + err);
    p.close();
    check(p.open(data_dir() + "lossless.ivf", &err) && p.has_video(), "MediaPlayer opens vp9 ivf " + err);
    p.close();
}

std::uint32_t g_rng = 0x12345678u;

std::uint32_t rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

void mutate(std::vector<std::uint8_t>& d, std::size_t keep_prefix) {
    if (d.size() <= keep_prefix + 1) return;
    const std::size_t span = d.size() - keep_prefix;
    const int kind = static_cast<int>(rnd() % 6);
    const int count = 1 + static_cast<int>(rnd() % 8);
    for (int i = 0; i < count; ++i) {
        const std::size_t at = keep_prefix + rnd() % span;
        if (at >= d.size()) break;
        if (kind == 0) d[at] ^= static_cast<std::uint8_t>(1u << (rnd() % 8));
        else if (kind == 1) d[at] = static_cast<std::uint8_t>(rnd());
        else if (kind == 2) d[at] = (rnd() & 1) ? 0xFF : 0x00;
        else if (kind == 3) {
            d.resize(keep_prefix + rnd() % span);
            break;
        } else if (kind == 4) d.insert(d.begin() + static_cast<std::ptrdiff_t>(at), static_cast<std::uint8_t>(rnd()));
        else d.erase(d.begin() + static_cast<std::ptrdiff_t>(at));
    }
}

void test_fuzz() {
    int iters = 400;
    if (const char* e = std::getenv("FIZMO_VP9_FUZZ_ITERS")) iters = std::atoi(e);
    const std::vector<Asset> assets = load_manifest();
    if (assets.empty() || iters <= 0) return;
    std::vector<std::vector<std::vector<std::uint8_t>>> packets;
    std::vector<std::vector<std::uint8_t>> files;
    for (const Asset& a : assets) {
        if (a.name == "tiles.ivf") continue;
        files.push_back(load_file(data_dir() + a.name));
        Demuxer dm;
        std::vector<std::vector<std::uint8_t>> list;
        if (dm.open(files.back().data(), files.back().size())) {
            Packet p;
            while (dm.read(p)) list.push_back(p.data);
        }
        packets.push_back(list);
    }
    const auto t0 = std::chrono::steady_clock::now();
    std::size_t frames = 0;
    std::size_t errors = 0;
    double worst = 0.0;
    for (int it = 0; it < iters; ++it) {
        const std::size_t pick = rnd() % packets.size();
        const auto s0 = std::chrono::steady_clock::now();
        if (it % 4 == 3) {
            std::vector<std::uint8_t> f = files[pick];
            mutate(f, 0);
            DecodeResult r = decode_memory(f);
            frames += r.frames;
        } else {
            Vp9Decoder dec;
            dec.open();
            dec.set_threads(1 + it % 3);
            VideoFrame f;
            const std::vector<std::vector<std::uint8_t>>& list = packets[pick];
            const int victims = 1 + static_cast<int>(rnd() % 3);
            std::vector<std::size_t> hit;
            for (int v = 0; v < victims && !list.empty(); ++v) hit.push_back(rnd() % list.size());
            for (std::size_t i = 0; i < list.size(); ++i) {
                std::vector<std::uint8_t> d = list[i];
                if (std::find(hit.begin(), hit.end(), i) != hit.end()) mutate(d, 0);
                if (!dec.decode(d.data(), d.size(), static_cast<double>(i))) ++errors;
                while (dec.receive(f)) {
                    ++frames;
                    check(f.width > 0 && f.height > 0 && !f.data.empty(), "fuzz output frame is sane");
                }
            }
            dec.flush();
            while (dec.receive(f)) ++frames;
        }
        worst = std::max(worst, std::chrono::duration<double>(std::chrono::steady_clock::now() - s0).count());
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(worst < 20.0, "no fuzz iteration hangs (worst " + std::to_string(worst) + " s)");
    std::printf("fuzz: %d iterations, %zu frames, %zu rejected packets, %.1f s\n", iters, frames, errors, secs);
}

bool have_ffmpeg() {
    if (std::getenv("FIZMO_VP9_NO_FFMPEG")) return false;
#ifdef _WIN32
    return std::system("ffmpeg -hide_banner -loglevel error -version > NUL 2>&1") == 0;
#else
    return std::system("ffmpeg -hide_banner -loglevel error -version > /dev/null 2>&1") == 0;
#endif
}

bool run(const std::string& cmd) {
    return std::system(cmd.c_str()) == 0;
}

void compare_with_ffmpeg(const std::string& label, const std::string& stream, double* fps) {
    const std::string ref = temp_prefix() + "ref.raw";
    if (!run("ffmpeg -v error -y -i \"" + stream + "\" -fps_mode passthrough -autoscale 0 -f rawvideo \"" + ref + "\"")) {
        ++g_skipped;
        std::printf("skip %s: ffmpeg decode failed\n", label.c_str());
        return;
    }
    const std::vector<std::uint8_t> want = load_file(ref);
    std::remove(ref.c_str());
    const auto t0 = std::chrono::steady_clock::now();
    DecodeResult r = decode_path(stream, true, false);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (fps && secs > 0.0) *fps = static_cast<double>(r.frames) / secs;
    bool same = r.raw.size() == want.size() && r.raw == want;
    std::string where;
    if (!same) {
        std::size_t i = 0;
        const std::size_t n = std::min(r.raw.size(), want.size());
        while (i < n && r.raw[i] == want[i]) ++i;
        where = " first difference at byte " + std::to_string(i) + " sizes " + std::to_string(r.raw.size()) + "/" + std::to_string(want.size());
    }
    check(r.opened && same && r.errors == 0, label + ": bit-exact against ffmpeg" + where + " " + r.error);
}

void test_ffmpeg() {
    if (!have_ffmpeg()) {
        ++g_skipped;
        std::printf("skip: ffmpeg not found, generated-stream tests not run\n");
        return;
    }
    struct Case {
        const char* name;
        const char* src;
        const char* size;
        int frames;
        const char* pix;
        const char* opts;
        const char* ext;
    };
    const Case cases[] = {
        {"g420", "testsrc2", "320x240", 30, "yuv420p", "-b:v 300k -auto-alt-ref 1 -lag-in-frames 16", "webm"},
        {"g420q", "mandelbrot", "200x120", 20, "yuv420p", "-crf 50 -b:v 0", "ivf"},
        {"g422", "mandelbrot", "208x120", 12, "yuv422p", "-b:v 300k", "mp4"},
        {"g440", "testsrc2", "160x128", 12, "yuv440p", "-b:v 300k", "ivf"},
        {"g444", "testsrc2", "160x128", 12, "yuv444p", "-b:v 300k", "webm"},
        {"grgb", "testsrc2", "160x96", 10, "gbrp", "-b:v 300k", "ivf"},
        {"g10", "mandelbrot", "192x112", 12, "yuv420p10le", "-b:v 300k", "ivf"},
        {"g12", "testsrc2", "192x112", 12, "yuv420p12le", "-b:v 300k", "mkv"},
        {"g422_12", "testsrc2", "160x96", 10, "yuv422p12le", "-b:v 300k", "ivf"},
        {"g444_10", "mandelbrot", "160x96", 10, "yuv444p10le", "-b:v 300k", "ivf"},
        {"g440_10", "mandelbrot", "160x96", 10, "yuv440p10le", "-b:v 300k", "ivf"},
        {"grgb12", "testsrc2", "160x96", 8, "gbrp12le", "-b:v 300k", "ivf"},
        {"glossless", "testsrc2", "176x144", 8, "yuv420p", "-lossless 1", "ivf"},
        {"glossless444", "mandelbrot", "96x80", 6, "yuv444p10le", "-lossless 1", "ivf"},
        {"gerr", "mandelbrot", "240x160", 20, "yuv420p", "-b:v 200k -error-resilient 1", "ivf"},
        {"gfpar", "mandelbrot", "240x160", 20, "yuv420p", "-b:v 200k -frame-parallel 1", "webm"},
        {"gaq1", "testsrc2", "240x160", 16, "yuv420p", "-b:v 200k -aq-mode 1", "ivf"},
        {"gaq2", "mandelbrot", "240x160", 16, "yuv420p", "-b:v 200k -aq-mode 2", "ivf"},
        {"gsharp", "mandelbrot", "240x160", 12, "yuv420p", "-b:v 200k -sharpness 7", "ivf"},
        {"godd", "mandelbrot", "131x77", 12, "yuv420p", "-b:v 150k", "ivf"},
        {"gtiny", "testsrc2", "8x8", 6, "yuv420p", "-b:v 50k", "ivf"},
        {"gtiles", "testsrc2", "1100x200", 4, "yuv420p", "-b:v 1M -tile-columns 2 -tile-rows 1", "ivf"},
        {"gtiles4", "testsrc2", "1100x200", 3, "yuv420p", "-b:v 1M -tile-columns 2 -tile-rows 2 -threads 4", "webm"},
    };
    for (const Case& c : cases) {
        const std::string out = temp_prefix() + c.name + "." + c.ext;
        const std::string cmd = std::string("ffmpeg -v error -y -f lavfi -i ") + c.src + "=size=" + c.size + ":rate=25 -frames:v " + std::to_string(c.frames) + " -pix_fmt " + c.pix + " -c:v libvpx-vp9 " + c.opts + " \"" + out + "\"";
        if (!run(cmd)) {
            ++g_skipped;
            std::printf("skip %s: libvpx-vp9 encode failed\n", c.name);
            continue;
        }
        compare_with_ffmpeg(c.name, out, nullptr);
        std::remove(out.c_str());
    }
    if (std::getenv("FIZMO_VP9_BENCH")) {
        const std::string out = temp_prefix() + "bench.webm";
        if (run("ffmpeg -v error -y -f lavfi -i testsrc2=size=1920x1080:rate=30 -frames:v 60 -c:v libvpx-vp9 -b:v 4M -tile-columns 2 -deadline realtime -cpu-used 8 \"" + out + "\"")) {
            double fps = 0.0;
            compare_with_ffmpeg("1080p", out, &fps);
            std::printf("1080p decode: %.1f fps\n", fps);
            std::remove(out.c_str());
        }
    }
}

} // namespace

int main() {
    test_api();
    test_assets();
    test_formats_and_color();
    test_direct_api();
    test_threads_and_seek();
    test_fuzz();
    test_ffmpeg();
    std::printf("vp9 tests: %d passed, %d failed, %d skipped\n", g_passed, g_failed, g_skipped);
    return g_failed == 0 ? 0 : 1;
}
