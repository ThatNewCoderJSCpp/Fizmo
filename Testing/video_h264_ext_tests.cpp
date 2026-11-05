#include "../Source/Video/decoder.hpp"
#include "../Source/Video/deinterlace.hpp"
#include "../Source/Video/demux.hpp"
#include "../Source/Video/frame.hpp"
#include "../Source/Video/h264.hpp"
#include "../Source/Video/player.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace fizmo::video;

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

std::string g_data_dir;

std::string data_path(const std::string& name) {
    return g_data_dir + "/" + name;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

void hash_frame(Md5& md5, const VideoFrame& f) {
    int planes = plane_count(f.format);
    std::size_t bps = static_cast<std::size_t>(bytes_per_sample(f.format));
    for (int p = 0; p < planes; ++p) {
        for (std::uint32_t y = 0; y < f.plane_height(p); ++y) md5.update(f.plane(p) + f.stride[p] * y, f.plane_width(p) * bps);
    }
}

struct Decoded {
    std::vector<VideoFrame> frames;
    std::string md5;
    bool ok = true;
};

void finish_hash(Decoded& d) {
    Md5 md5;
    for (const VideoFrame& f : d.frames) hash_frame(md5, f);
    d.md5 = md5.hex();
}

Decoded decode_annexb(const std::vector<std::uint8_t>& data, bool whole, H264Info* info = nullptr) {
    Decoded d;
    H264Decoder dec;
    dec.open_annexb();
    VideoFrame f;
    if (whole) {
        d.ok = dec.decode(data.data(), data.size(), 0.0);
        while (dec.receive(f)) d.frames.push_back(f);
    } else {
        H264FileReader r;
        if (!r.open(data.data(), data.size())) {
            d.ok = false;
            return d;
        }
        std::vector<std::uint8_t> au;
        double pts = 0.0;
        while (r.read(au)) {
            if (!dec.decode(au.data(), au.size(), pts)) d.ok = false;
            pts += 0.04;
            while (dec.receive(f)) d.frames.push_back(f);
        }
    }
    dec.flush();
    while (dec.receive(f)) d.frames.push_back(f);
    if (dec.error_count() != 0) d.ok = false;
    if (info) *info = dec.info();
    finish_hash(d);
    return d;
}

Decoded decode_container(const std::string& path, TrackInfo* track_out) {
    Decoded d;
    Demuxer dm;
    std::string err;
    if (!dm.open(path, &err)) {
        d.ok = false;
        return d;
    }
    int vt = dm.best_track(MediaType::video);
    if (vt < 0) {
        d.ok = false;
        return d;
    }
    for (std::size_t i = 0; i < dm.tracks().size(); ++i) dm.select(static_cast<std::uint32_t>(i), static_cast<int>(i) == vt);
    const TrackInfo& track = dm.tracks()[static_cast<std::size_t>(vt)];
    if (track_out) *track_out = track;
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(track, &err);
    if (!dec) {
        d.ok = false;
        return d;
    }
    Packet pkt;
    VideoFrame f;
    while (dm.read(pkt)) {
        if (pkt.track != track.id && static_cast<int>(pkt.track) != vt) continue;
        if (!dec->decode_packet(pkt)) d.ok = false;
        while (dec->receive(f)) d.frames.push_back(f);
    }
    dec->flush();
    while (dec->receive(f)) d.frames.push_back(f);
    if (dec->error_count() != 0) d.ok = false;
    finish_hash(d);
    return d;
}

struct StreamEntry {
    std::string name;
    std::string format;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t frames = 0;
    bool interlaced = false;
    std::string structure;
    std::string md5;
};

struct ContainerEntry {
    std::string name;
    std::string source;
};

bool load_manifest(std::vector<StreamEntry>& streams, std::vector<ContainerEntry>& containers) {
    std::ifstream in(data_path("h264_ext_manifest.txt"));
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string kind;
        ss >> kind;
        if (kind == "stream") {
            StreamEntry e;
            int il = 0;
            ss >> e.name >> e.format >> e.width >> e.height >> e.frames >> il >> e.structure >> e.md5;
            e.interlaced = il != 0;
            if (!e.name.empty()) streams.push_back(e);
        } else if (kind == "container") {
            ContainerEntry c;
            ss >> c.name >> c.source;
            if (!c.name.empty()) containers.push_back(c);
        }
    }
    return !streams.empty();
}

void test_formats() {
    struct Row {
        PixelFormat f;
        int depth;
        int planes;
        int sx;
        int sy;
        int cf;
        const char* name;
    };
    const Row rows[] = {
        {PixelFormat::gray, 8, 1, 0, 0, 0, "gray"},
        {PixelFormat::yuv420p, 8, 3, 1, 1, 1, "yuv420p"},
        {PixelFormat::yuv422p, 8, 3, 1, 0, 2, "yuv422p"},
        {PixelFormat::yuv444p, 8, 3, 0, 0, 3, "yuv444p"},
        {PixelFormat::gray10, 10, 1, 0, 0, 0, "gray10"},
        {PixelFormat::yuv420p10, 10, 3, 1, 1, 1, "yuv420p10"},
        {PixelFormat::yuv422p10, 10, 3, 1, 0, 2, "yuv422p10"},
        {PixelFormat::yuv444p10, 10, 3, 0, 0, 3, "yuv444p10"},
        {PixelFormat::gray12, 12, 1, 0, 0, 0, "gray12"},
        {PixelFormat::yuv420p12, 12, 3, 1, 1, 1, "yuv420p12"},
        {PixelFormat::yuv422p12, 12, 3, 1, 0, 2, "yuv422p12"},
        {PixelFormat::yuv444p12, 12, 3, 0, 0, 3, "yuv444p12"},
        {PixelFormat::gray14, 14, 1, 0, 0, 0, "gray14"},
        {PixelFormat::yuv420p14, 14, 3, 1, 1, 1, "yuv420p14"},
        {PixelFormat::yuv422p14, 14, 3, 1, 0, 2, "yuv422p14"},
        {PixelFormat::yuv444p14, 14, 3, 0, 0, 3, "yuv444p14"},
    };
    bool all = true;
    for (const Row& r : rows) {
        bool ok = bit_depth(r.f) == r.depth && plane_count(r.f) == r.planes && chroma_shift_x(r.f) == r.sx && chroma_shift_y(r.f) == r.sy;
        ok = ok && bytes_per_sample(r.f) == (r.depth > 8 ? 2 : 1) && std::string(format_name(r.f)) == r.name;
        ok = ok && chroma_format_of(r.f) == r.cf && max_sample_value(r.f) == (1 << r.depth) - 1 && pixel_format_for(r.cf, r.depth) == r.f;
        VideoFrame fr;
        ok = ok && fr.allocate(17, 9, r.f);
        ok = ok && fr.plane_width(1) == (r.planes > 1 ? (17u + (1u << r.sx) - 1) >> r.sx : 0u + fr.plane_width(1));
        ok = ok && fr.stride[0] >= 17u * static_cast<unsigned>(bytes_per_sample(r.f));
        if (!ok) {
            all = false;
            std::printf("format helper mismatch for %s\n", r.name);
        }
    }
    check(all, "every pixel format reports depth, planes, chroma shifts, size and name");
    check(bit_depth(PixelFormat::none) == 0 && plane_count(PixelFormat::none) == 0 && chroma_format_of(PixelFormat::none) == -1, "PixelFormat::none helpers");
    check(chroma_format_of(PixelFormat::yuv440p) == -1 && chroma_format_of(PixelFormat::yuv440p10) == -1 && chroma_format_of(PixelFormat::yuv440p12) == -1, "4:4:0 has no chroma_format_idc");
    check(chroma_shift_x(PixelFormat::yuv440p10) == 0 && chroma_shift_y(PixelFormat::yuv440p10) == 1 && plane_count(PixelFormat::yuv440p12) == 3 && bit_depth(PixelFormat::yuv440p12) == 12, "4:4:0 helpers");
    check(pixel_format_for(1, 9) == PixelFormat::yuv420p10 && pixel_format_for(3, 11) == PixelFormat::yuv444p12 && pixel_format_for(0, 13) == PixelFormat::gray14 && pixel_format_for(4, 8) == PixelFormat::none, "odd bit depths map to the next stored depth");
    check(std::string(picture_structure_name(PictureStructure::mbaff_frame)) == "mbaff_frame" && std::string(picture_structure_name(PictureStructure::field_pair)) == "field_pair", "picture structure names");
    const PixelFormat deep[] = {PixelFormat::yuv420p12, PixelFormat::yuv444p14, PixelFormat::gray12, PixelFormat::yuv422p14};
    bool rgba_ok = true;
    for (PixelFormat pf : deep) {
        VideoFrame f;
        f.allocate(4, 2, pf);
        int d = bit_depth(pf);
        for (int p = 0; p < plane_count(pf); ++p) {
            for (std::uint32_t y = 0; y < f.plane_height(p); ++y) {
                std::uint16_t* row = reinterpret_cast<std::uint16_t*>(f.plane(p) + f.stride[p] * y);
                for (std::uint32_t x = 0; x < f.plane_width(p); ++x) row[x] = static_cast<std::uint16_t>(p == 0 ? (235 << (d - 8)) : (128 << (d - 8)));
            }
        }
        std::vector<std::uint8_t> px = to_rgba(f);
        for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
            if (px[i] != 255 || px[i + 1] != 255 || px[i + 2] != 255 || px[i + 3] != 255) rgba_ok = false;
        }
        std::uint16_t* y0 = reinterpret_cast<std::uint16_t*>(f.plane(0));
        y0[0] = static_cast<std::uint16_t>(16 << (d - 8));
        px = to_rgba(f);
        if (px[0] != 0 || px[1] != 0 || px[2] != 0) rgba_ok = false;
    }
    check(rgba_ok, "12- and 14-bit LSB-aligned frames convert limited-range black and white exactly");
    VideoFrame f;
    check(!f.interlaced && f.top_field_first && f.structure == PictureStructure::frame && f.repeat_fields == 0, "VideoFrame interlace fields default to progressive");
    f.allocate(8, 8, PixelFormat::yuv420p);
    f.interlaced = true;
    f.structure = PictureStructure::field_pair;
    f.clear();
    check(!f.interlaced && f.structure == PictureStructure::frame, "clear() resets the interlace fields");
}

std::map<std::string, std::string> g_stream_md5;
std::map<std::string, std::size_t> g_stream_frames;

void test_fixtures() {
    std::vector<StreamEntry> streams;
    std::vector<ContainerEntry> containers;
    if (!load_manifest(streams, containers)) {
        check(false, "fixture manifest found in " + g_data_dir);
        return;
    }
    for (const StreamEntry& e : streams) {
        std::vector<std::uint8_t> data;
        if (!read_file(data_path(e.name), data)) {
            check(false, e.name + ": fixture is readable");
            continue;
        }
        H264Info info;
        Decoded a = decode_annexb(data, false, &info);
        check(a.ok, e.name + ": decodes without errors");
        check(a.frames.size() == e.frames, e.name + ": " + std::to_string(e.frames) + " frames (got " + std::to_string(a.frames.size()) + ")");
        check(a.md5 == e.md5, e.name + ": output is bit-exact with the reference decoder");
        bool fmt = !a.frames.empty();
        bool flags = !a.frames.empty();
        for (const VideoFrame& f : a.frames) {
            if (std::string(format_name(f.format)) != e.format || f.width != e.width || f.height != e.height) fmt = false;
            if (f.interlaced != e.interlaced || std::string(picture_structure_name(f.structure)) != e.structure) flags = false;
        }
        check(fmt, e.name + ": frames are " + e.format + " " + std::to_string(e.width) + "x" + std::to_string(e.height));
        check(flags, e.name + ": interlaced=" + std::to_string(e.interlaced) + " structure=" + e.structure);
        check(info.format == (a.frames.empty() ? PixelFormat::none : a.frames[0].format) && info.width == e.width && info.height == e.height, e.name + ": info() reports the output format and size");
        bool expect_fields = e.structure == "field_pair";
        bool expect_mbaff = e.structure == "mbaff_frame";
        check(info.progressive == (!expect_fields && !expect_mbaff) && info.mbaff == expect_mbaff, e.name + ": info() reports the frame/field coding");
        if (e.name.find("lossless") != std::string::npos) check(info.lossless, e.name + ": info() reports lossless coding");
        if (e.name.find("separate") != std::string::npos) check(info.separate_colour_plane && info.chroma_format_idc == 3, e.name + ": info() reports separate colour planes");
        if (e.name.find("bff") != std::string::npos) {
            bool bff = !a.frames.empty();
            for (const VideoFrame& f : a.frames) bff = bff && !f.top_field_first;
            check(bff, e.name + ": bottom field first is reported");
        }
        g_stream_md5[e.name] = e.md5;
        g_stream_frames[e.name] = e.frames;
        if (e.name == "mbaff_cabac.h264" || e.name == "paff_p.h264" || e.name == "hi422_10_cavlc.h264" || e.name == "separate_planes.h264") {
            Decoded w = decode_annexb(data, true);
            check(w.md5 == e.md5, e.name + ": decoding the whole stream in one call is bit-exact");
        }
    }
    for (const ContainerEntry& c : containers) {
        TrackInfo track;
        Decoded d = decode_container(data_path(c.name), &track);
        check(track.codec == "h264" && !track.codec_config.empty() && track.codec_config[0] == 1, c.name + ": demuxer passes an avcC record");
        bool high = track.codec_config.size() > 1 && (track.codec_config[1] == 100 || track.codec_config[1] == 110 || track.codec_config[1] == 122 || track.codec_config[1] == 244);
        check(high, c.name + ": avcC carries a high profile");
        check(d.ok, c.name + ": container decode has no errors");
        check(d.frames.size() == g_stream_frames[c.source] && d.md5 == g_stream_md5[c.source], c.name + ": container decode equals the Annex B decode of " + c.source);
    }
}

std::vector<std::vector<std::uint8_t>> split_nals(const std::vector<std::uint8_t>& d) {
    std::vector<std::vector<std::uint8_t>> out;
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i + 3 <= d.size(); ++i) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            starts.push_back(i + 3);
            i += 2;
        }
    }
    for (std::size_t k = 0; k < starts.size(); ++k) {
        std::size_t e = k + 1 < starts.size() ? starts[k + 1] - 3 : d.size();
        while (e > starts[k] && d[e - 1] == 0) --e;
        out.emplace_back(d.begin() + static_cast<std::ptrdiff_t>(starts[k]), d.begin() + static_cast<std::ptrdiff_t>(e));
    }
    return out;
}

void test_avcc_extension() {
    std::vector<std::uint8_t> data;
    if (!read_file(data_path("hi422_10_cavlc.h264"), data)) {
        check(false, "avcC test fixture readable");
        return;
    }
    std::vector<std::vector<std::uint8_t>> nals = split_nals(data);
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
    for (const auto& n : nals) {
        if ((n[0] & 31) == 7 && sps.empty()) sps = n;
        if ((n[0] & 31) == 8 && pps.empty()) pps = n;
    }
    check(!sps.empty() && !pps.empty(), "parameter sets found for the avcC test");
    std::vector<std::uint8_t> avcc = {1, sps[1], sps[2], sps[3], 0xFF, 0xE1};
    avcc.push_back(static_cast<std::uint8_t>(sps.size() >> 8));
    avcc.push_back(static_cast<std::uint8_t>(sps.size()));
    avcc.insert(avcc.end(), sps.begin(), sps.end());
    avcc.push_back(1);
    avcc.push_back(static_cast<std::uint8_t>(pps.size() >> 8));
    avcc.push_back(static_cast<std::uint8_t>(pps.size()));
    avcc.insert(avcc.end(), pps.begin(), pps.end());
    avcc.push_back(0xFC | 2);
    avcc.push_back(0xF8 | 2);
    avcc.push_back(0xF8 | 2);
    avcc.push_back(0);
    H264Decoder dec;
    std::string err;
    check(dec.open(avcc.data(), avcc.size(), &err), "avcC with the high-profile extension opens " + err);
    check(dec.info().chroma_format_idc == 2 && dec.info().bit_depth == 10 && dec.info().bit_depth_chroma == 10 && dec.info().format == PixelFormat::yuv422p10, "avcC reports 4:2:2 10-bit before any picture");
    H264FileReader r;
    r.open(data.data(), data.size());
    std::vector<std::uint8_t> au;
    Decoded d;
    VideoFrame f;
    while (r.read(au)) {
        std::vector<std::vector<std::uint8_t>> units = split_nals(au);
        std::vector<std::uint8_t> pkt;
        for (const auto& u : units) {
            std::uint32_t n = static_cast<std::uint32_t>(u.size());
            pkt.push_back(static_cast<std::uint8_t>(n >> 24));
            pkt.push_back(static_cast<std::uint8_t>(n >> 16));
            pkt.push_back(static_cast<std::uint8_t>(n >> 8));
            pkt.push_back(static_cast<std::uint8_t>(n));
            pkt.insert(pkt.end(), u.begin(), u.end());
        }
        dec.decode(pkt.data(), pkt.size(), 0.0);
        while (dec.receive(f)) d.frames.push_back(f);
    }
    dec.flush();
    while (dec.receive(f)) d.frames.push_back(f);
    finish_hash(d);
    check(d.md5 == g_stream_md5["hi422_10_cavlc.h264"], "length-prefixed 4:2:2 10-bit packets decode bit-exact");
}

VideoFrame make_frame(std::uint32_t w, std::uint32_t h, PixelFormat pf, const std::function<int(int, int, int)>& value, bool interlaced) {
    VideoFrame f;
    f.allocate(w, h, pf);
    f.interlaced = interlaced;
    f.top_field_first = true;
    int bps = bytes_per_sample(pf);
    for (int p = 0; p < plane_count(pf); ++p) {
        for (std::uint32_t y = 0; y < f.plane_height(p); ++y) {
            for (std::uint32_t x = 0; x < f.plane_width(p); ++x) {
                int v = value(p, static_cast<int>(x), static_cast<int>(y));
                std::uint8_t* row = f.plane(p) + f.stride[p] * y;
                if (bps == 2) {
                    reinterpret_cast<std::uint16_t*>(row)[x] = static_cast<std::uint16_t>(v);
                } else {
                    row[x] = static_cast<std::uint8_t>(v);
                }
            }
        }
    }
    return f;
}

int sample(const VideoFrame& f, int p, int x, int y) {
    const std::uint8_t* row = f.plane(p) + f.stride[p] * static_cast<std::size_t>(y);
    return bytes_per_sample(f.format) == 2 ? reinterpret_cast<const std::uint16_t*>(row)[x] : row[x];
}

bool same_pixels(const VideoFrame& a, const VideoFrame& b) {
    if (a.width != b.width || a.height != b.height || a.format != b.format) return false;
    for (int p = 0; p < plane_count(a.format); ++p) {
        for (std::uint32_t y = 0; y < a.plane_height(p); ++y) {
            for (std::uint32_t x = 0; x < a.plane_width(p); ++x) {
                if (sample(a, p, static_cast<int>(x), static_cast<int>(y)) != sample(b, p, static_cast<int>(x), static_cast<int>(y))) return false;
            }
        }
    }
    return true;
}

void test_deinterlacer() {
    auto ramp = [](int p, int x, int y) { return 40 + p * 10 + y * 2 + (x & 7); };
    VideoFrame still = make_frame(32, 24, PixelFormat::yuv420p, ramp, true);
    VideoFrame out;
    check(deinterlace_bob(still, out) && !out.interlaced && out.structure == PictureStructure::frame, "bob produces a progressive frame");
    bool bob_ok = true;
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 32; ++x) {
            int want = (y & 1) ? sample(still, 0, x, y - 1) : sample(still, 0, x, y);
            if (sample(out, 0, x, y) != want) bob_ok = false;
        }
    }
    check(bob_ok, "bob keeps the top field and repeats its lines");
    VideoFrame lin;
    deinterlace_field(nullptr, still, nullptr, lin, DeinterlaceMode::linear, 0, true);
    bool lin_ok = true;
    for (int y = 1; y < 23; y += 2) {
        for (int x = 0; x < 32; ++x) {
            if (sample(lin, 0, x, y) != (sample(still, 0, x, y - 1) + sample(still, 0, x, y + 1) + 1) / 2) lin_ok = false;
        }
    }
    check(lin_ok, "linear interpolates the missing field lines");
    Deinterlacer d;
    VideoFrame a = still;
    a.pts = 0.0;
    VideoFrame b = still;
    b.pts = 0.04;
    VideoFrame c = still;
    c.pts = 0.08;
    d.push(a);
    VideoFrame got;
    check(!d.pull(got), "yadif waits for the next frame");
    d.push(b);
    d.push(c);
    d.flush();
    int n = 0;
    bool exact = true;
    while (d.pull(got)) {
        ++n;
        if (!same_pixels(got, still) || got.interlaced) exact = false;
    }
    check(n == 3 && exact, "yadif reconstructs static interlaced content exactly");
    check(d.frames_deinterlaced() == 3, "deinterlacer counts processed frames");
    DeinterlaceOptions fo;
    fo.mode = DeinterlaceMode::yadif;
    fo.field_rate = true;
    Deinterlacer fr(fo);
    std::vector<VideoFrame> moving;
    for (int i = 0; i < 4; ++i) {
        VideoFrame m = make_frame(48, 32, PixelFormat::yuv422p10, [&](int p, int x, int y) {
            int field_time = 2 * i + (y & 1);
            int v = ((x + field_time * 3) / 6) % 2 ? 900 : 100;
            return p == 0 ? v : 512;
        }, true);
        m.pts = i * 0.04;
        moving.push_back(m);
        fr.push(m);
    }
    fr.flush();
    std::vector<VideoFrame> outs;
    while (fr.pull(got)) outs.push_back(got);
    check(outs.size() == 8, "field-rate yadif doubles the frame rate");
    bool pts_ok = outs.size() == 8;
    bool keep_ok = outs.size() == 8;
    bool range_ok = true;
    for (std::size_t k = 0; k < outs.size(); ++k) {
        const VideoFrame& o = outs[k];
        const VideoFrame& src = moving[k / 2];
        double want = (k / 2) * 0.04 + ((k & 1) ? 0.02 : 0.0);
        if (std::fabs(o.pts - want) > 1e-9) pts_ok = false;
        int keep = (k & 1) ? 1 : 0;
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 48; ++x) {
                int v = sample(o, 0, x, y);
                if ((y & 1) == keep && v != sample(src, 0, x, y)) keep_ok = false;
                if (v < 100 || v > 900) range_ok = false;
            }
        }
        if (bit_depth(o.format) != 10 || o.format != PixelFormat::yuv422p10) range_ok = false;
    }
    check(pts_ok, "field-rate output frames get field timestamps");
    check(keep_ok, "each output keeps its own field untouched");
    check(range_ok, "10-bit 4:2:2 output stays within the input range");
    DeinterlaceOptions po;
    Deinterlacer pass(po);
    VideoFrame prog = make_frame(16, 16, PixelFormat::gray12, [](int, int x, int y) { return (x * 97 + y * 13) % 4096; }, false);
    pass.push(prog);
    check(pass.pull(got) && same_pixels(got, prog) && pass.frames_deinterlaced() == 0, "automatic mode passes progressive frames through without delay");
    po.only_interlaced = false;
    po.mode = DeinterlaceMode::yadif_spatial;
    Deinterlacer forced(po);
    forced.push(prog);
    check(forced.pull(got) && forced.frames_deinterlaced() == 1 && !got.interlaced, "forced mode deinterlaces every frame");
    po.mode = DeinterlaceMode::weave;
    Deinterlacer weave(po);
    weave.push(still);
    check(weave.pull(got) && same_pixels(got, still), "weave leaves frames untouched");
    Deinterlacer rs;
    rs.push(still);
    rs.reset();
    rs.flush();
    check(!rs.pull(got), "reset drops pending frames");
    check(std::string(deinterlace_mode_name(DeinterlaceMode::yadif)) == "yadif" && std::string(deinterlace_setting_name(DeinterlaceSetting::automatic)) == "auto", "mode and setting names");
    std::vector<std::uint8_t> data;
    if (read_file(data_path("mbaff_cabac.h264"), data)) {
        Decoded dec = decode_annexb(data, false);
        Deinterlacer y;
        for (const VideoFrame& f : dec.frames) y.push(f);
        y.flush();
        int count = 0;
        bool prog_out = true;
        bool kept = true;
        std::size_t i = 0;
        while (y.pull(got)) {
            if (got.interlaced || got.width != dec.frames[i].width) prog_out = false;
            for (std::uint32_t row = 0; row < got.height; row += 2) {
                if (std::memcmp(got.plane(0) + got.stride[0] * row, dec.frames[i].plane(0) + dec.frames[i].stride[0] * row, got.width) != 0) kept = false;
            }
            ++count;
            ++i;
        }
        check(count == static_cast<int>(dec.frames.size()) && prog_out && kept, "decoded MBAFF frames deinterlace and keep their top field");
    }
}

const VideoFrame* settle(MediaPlayer& p, std::uint64_t& serial) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    const VideoFrame* f = nullptr;
    while (std::chrono::steady_clock::now() < end) {
        p.current_frame(f, serial);
        if (f && !p.is_seeking()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    p.current_frame(f, serial);
    return f;
}

void test_player() {
    PlayerOptions defaults;
    check(defaults.deinterlace == DeinterlaceSetting::automatic && defaults.deinterlace_mode == DeinterlaceMode::yadif && !defaults.deinterlace_field_rate, "player deinterlaces automatically by default");
    std::string path = data_path("mbaff_cabac.mp4");
    {
        PlayerOptions o;
        o.enable_audio = false;
        MediaPlayer p(o);
        std::string err;
        check(p.open(path, &err), "player opens an interlaced mp4 " + err);
        std::uint64_t serial = 0;
        const VideoFrame* f = settle(p, serial);
        check(f && !f->interlaced && f->format == PixelFormat::yuv420p, "automatic mode shows deinterlaced frames");
        check(p.stats().frames_deinterlaced > 0 && p.is_deinterlacing() && p.stats().deinterlacing, "player reports that it is deinterlacing");
        p.set_deinterlace(DeinterlaceSetting::off);
        p.set_deinterlace_mode(DeinterlaceMode::bob, true);
        check(p.deinterlace() == DeinterlaceSetting::off && p.deinterlace_mode() == DeinterlaceMode::bob && p.deinterlace_field_rate(), "deinterlace settings can be changed");
        p.close();
    }
    {
        PlayerOptions o;
        o.enable_audio = false;
        o.deinterlace = DeinterlaceSetting::off;
        MediaPlayer p(o);
        check(p.open(path), "player opens with deinterlacing off");
        std::uint64_t serial = 0;
        const VideoFrame* f = settle(p, serial);
        check(f && f->interlaced && f->structure == PictureStructure::mbaff_frame, "with deinterlacing off frames keep their interlace flags");
        check(p.stats().frames_deinterlaced == 0 && !p.is_deinterlacing(), "nothing is deinterlaced when off");
    }
    {
        PlayerOptions o;
        o.enable_audio = false;
        o.deinterlace_mode = DeinterlaceMode::linear;
        o.deinterlace_field_rate = true;
        MediaPlayer p(o);
        check(p.open(data_path("paff_p.mp4")), "player opens a field-coded mp4");
        std::uint64_t serial = 0;
        const VideoFrame* f = settle(p, serial);
        check(f && !f->interlaced && f->width == 160 && f->height == 128, "field pictures are woven, then deinterlaced at field rate");
    }
}

void test_resilience() {
    const char* names[] = {"mbaff_cabac.h264", "mbaff_cavlc_temporal.h264", "paff_p.h264", "paff_bfields.h264", "hi444_cabac.h264", "depth14_444.h264", "separate_planes.h264", "lossless444_cabac.h264", "mbaff_422_10.h264", "hi10_cavlc.h264"};
    std::vector<std::vector<std::uint8_t>> inputs;
    for (const char* n : names) {
        std::vector<std::uint8_t> d;
        if (read_file(data_path(n), d)) inputs.push_back(d);
    }
    if (inputs.empty()) {
        check(false, "resilience inputs readable");
        return;
    }
    std::mt19937 rng(1234);
    int runs = 0;
    std::size_t frames = 0;
    bool sane = true;
    double worst = 0.0;
    for (int it = 0; it < 300; ++it) {
        std::vector<std::uint8_t> d = inputs[static_cast<std::size_t>(it) % inputs.size()];
        int kind = static_cast<int>(rng() % 5);
        int n = 1 + static_cast<int>(rng() % 12);
        if (kind == 0) {
            for (int k = 0; k < n; ++k) d[24 + rng() % (d.size() - 24)] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
        } else if (kind == 1) {
            for (int k = 0; k < n; ++k) d[rng() % d.size()] = static_cast<std::uint8_t>(rng());
        } else if (kind == 2) {
            d.resize(1 + d.size() * (rng() % 1000) / 1000);
        } else if (kind == 3) {
            std::size_t at = rng() % d.size();
            std::size_t len = rng() % 300;
            d.erase(d.begin() + static_cast<std::ptrdiff_t>(at), d.begin() + static_cast<std::ptrdiff_t>(std::min(d.size(), at + len)));
        } else {
            std::size_t at = rng() % d.size();
            std::size_t len = rng() % 300;
            std::vector<std::uint8_t> copy(d.begin() + static_cast<std::ptrdiff_t>(at), d.begin() + static_cast<std::ptrdiff_t>(std::min(d.size(), at + len)));
            d.insert(d.begin() + static_cast<std::ptrdiff_t>(rng() % d.size()), copy.begin(), copy.end());
        }
        auto t0 = std::chrono::steady_clock::now();
        Decoded out = decode_annexb(d, false);
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        worst = std::max(worst, dt);
        ++runs;
        frames += out.frames.size();
        for (const VideoFrame& f : out.frames) {
            if (f.empty() || f.width == 0 || f.height == 0 || f.width > 4096 || f.data.size() < f.offset[plane_count(f.format) - 1]) sane = false;
        }
    }
    check(runs == 300 && sane, "300 mutated interlaced/high-bit-depth streams decode without crashing (" + std::to_string(frames) + " frames)");
    check(worst < 10.0, "no mutated stream stalls the decoder");
}

bool have_ffmpeg() {
#if defined(_WIN32)
    return std::system("ffmpeg -hide_banner -encoders 2>NUL | findstr libx264 >NUL") == 0;
#else
    return std::system("ffmpeg -hide_banner -encoders 2>/dev/null | grep -q libx264") == 0;
#endif // _WIN32
}

void test_live_ffmpeg() {
    if (std::getenv("FIZMO_SKIP_FFMPEG")) return;
    if (!have_ffmpeg()) {
        std::printf("note: ffmpeg with libx264 not found, skipping live comparisons\n");
        return;
    }
    struct Live {
        const char* name;
        const char* pix;
        const char* args;
        bool interlaced;
    };
    const Live cases[] = {
        {"live_mbaff_420_cabac", "yuv420p", "-flags +ildct+ilme -x264-params tff=1:bframes=3:weightp=2:ref=3:direct=auto", true},
        {"live_mbaff_422_10_cavlc", "yuv422p10le", "-coder 0 -flags +ildct+ilme -x264-params bff=1:bframes=2:direct=temporal", true},
        {"live_mbaff_444_10", "yuv444p10le", "-flags +ildct+ilme -x264-params tff=1:bframes=2:8x8dct=1", true},
        {"live_mbaff_gray10", "gray10le", "-flags +ildct+ilme -x264-params tff=1:bframes=2", true},
        {"live_hi10_slices", "yuv420p10le", "-x264-params slices=3:bframes=3:b-pyramid=normal:weightp=2", false},
        {"live_444_lossless", "yuv444p", "-qp 0 -x264-params bframes=2", false},
        {"live_422_cqm", "yuv422p", "-x264-params cqm=jvt:bframes=2:ref=3:partitions=all", false},
    };
#if defined(_WIN32)
    std::string tmp = std::getenv("TEMP") ? std::getenv("TEMP") : ".";
    const char* quiet = " >NUL 2>&1";
#else
    std::string tmp = "/tmp";
    const char* quiet = " >/dev/null 2>&1";
#endif // _WIN32
    std::string src = "testsrc2=size=176x144:rate=50,rotate=angle=t*1.5:fillcolor=gray";
    int compared = 0;
    for (const Live& c : cases) {
        std::string base = tmp + "/fizmo_h264_ext_" + c.name;
        std::string filter = src + (c.interlaced ? ",interlace=scan=tff" : "");
        std::string enc = "ffmpeg -hide_banner -loglevel error -y -f lavfi -i \"" + filter + "\" -frames:v 8 -c:v libx264 -pix_fmt " + c.pix + " " + c.args + " -f h264 \"" + base + ".h264\"" + quiet;
        if (std::system(enc.c_str()) != 0) {
            check(false, std::string(c.name) + ": ffmpeg encode");
            continue;
        }
        bool gray = std::strncmp(c.pix, "gray", 4) == 0;
        std::string dec = "ffmpeg -hide_banner -loglevel error -y -i \"" + base + ".h264\" -fps_mode passthrough -f rawvideo " + (gray ? std::string() : std::string("-pix_fmt ") + c.pix + " ") + "\"" + base + ".yuv\"" + quiet;
        if (std::system(dec.c_str()) != 0) {
            check(false, std::string(c.name) + ": ffmpeg decode");
            continue;
        }
        std::vector<std::uint8_t> stream;
        std::vector<std::uint8_t> ref;
        read_file(base + ".h264", stream);
        read_file(base + ".yuv", ref);
        Decoded mine = decode_annexb(stream, false);
        std::vector<std::uint8_t> got;
        for (const VideoFrame& f : mine.frames) {
            int planes = plane_count(f.format);
            std::size_t bps = static_cast<std::size_t>(bytes_per_sample(f.format));
            for (int p = 0; p < planes; ++p) {
                for (std::uint32_t y = 0; y < f.plane_height(p); ++y) got.insert(got.end(), f.plane(p) + f.stride[p] * y, f.plane(p) + f.stride[p] * y + f.plane_width(p) * bps);
            }
            if (gray && !f.empty()) {
                std::size_t luma = static_cast<std::size_t>(f.width) * f.height * bps;
                std::size_t native = luma * 3 / 2;
                if (!ref.empty() && ref.size() == mine.frames.size() * native) {
                    std::vector<std::uint8_t> filler(native - luma, 0);
                    std::size_t at = got.size();
                    got.resize(at + filler.size());
                    for (std::size_t k = 0; k < filler.size(); k += bps) {
                        unsigned v = bps == 2 ? 512u : 128u;
                        got[at + k] = static_cast<std::uint8_t>(v & 0xFF);
                        if (bps == 2) got[at + k + 1] = static_cast<std::uint8_t>(v >> 8);
                    }
                }
            }
        }
        check(mine.ok && !got.empty() && got == ref, std::string(c.name) + ": bit-exact with ffmpeg (" + std::to_string(mine.frames.size()) + " frames)");
        ++compared;
        std::remove((base + ".h264").c_str());
        std::remove((base + ".yuv").c_str());
    }
    std::printf("live ffmpeg comparisons: %d\n", compared);
}

std::string default_data_dir() {
    std::string file = __FILE__;
    std::size_t slash = file.find_last_of("/\\");
    std::string dir = slash == std::string::npos ? std::string(".") : file.substr(0, slash);
    return dir + "/video/h264";
}

} // namespace

int main(int argc, char** argv) {
    g_data_dir = argc > 1 ? argv[1] : (std::getenv("FIZMO_H264_TEST_DATA") ? std::getenv("FIZMO_H264_TEST_DATA") : default_data_dir());
    test_formats();
    test_fixtures();
    test_avcc_extension();
    test_deinterlacer();
    test_player();
    test_resilience();
    test_live_ffmpeg();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
