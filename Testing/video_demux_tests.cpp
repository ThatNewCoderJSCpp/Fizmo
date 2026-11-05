#include "../Source/Video/demux.hpp"
#include "../Source/Audio/ogg.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <random>
#include <string>
#include <utility>
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

bool near(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) <= eps;
}

std::string temp_path(const std::string& name) {
    const char* dir = std::getenv("TMPDIR");
#if defined(_WIN32)
    if (!dir) dir = std::getenv("TEMP");
#endif
    std::string base = dir ? dir : "/tmp";
    return base + "/" + name;
}

using Bytes = std::vector<std::uint8_t>;

void put8(Bytes& b, std::uint32_t v) { b.push_back(static_cast<std::uint8_t>(v & 0xFF)); }
void put16(Bytes& b, std::uint32_t v) { put8(b, v >> 8); put8(b, v); }
void put24(Bytes& b, std::uint32_t v) { put8(b, v >> 16); put16(b, v & 0xFFFF); }
void put32(Bytes& b, std::uint32_t v) { put16(b, v >> 16); put16(b, v & 0xFFFF); }
void put64(Bytes& b, std::uint64_t v) { put32(b, static_cast<std::uint32_t>(v >> 32)); put32(b, static_cast<std::uint32_t>(v & 0xFFFFFFFFu)); }
void putle16(Bytes& b, std::uint32_t v) { put8(b, v); put8(b, v >> 8); }
void putle32(Bytes& b, std::uint32_t v) { putle16(b, v & 0xFFFF); putle16(b, v >> 16); }
void putle64(Bytes& b, std::uint64_t v) { putle32(b, static_cast<std::uint32_t>(v & 0xFFFFFFFFu)); putle32(b, static_cast<std::uint32_t>(v >> 32)); }
void put_str(Bytes& b, const char* s) { b.insert(b.end(), s, s + std::strlen(s)); }
void append(Bytes& b, const Bytes& x) { b.insert(b.end(), x.begin(), x.end()); }

Bytes box(const char* type, const Bytes& payload) {
    Bytes b;
    put32(b, static_cast<std::uint32_t>(payload.size() + 8));
    b.insert(b.end(), type, type + 4);
    append(b, payload);
    return b;
}

Bytes box64(const char* type, const Bytes& payload) {
    Bytes b;
    put32(b, 1);
    b.insert(b.end(), type, type + 4);
    put64(b, payload.size() + 16);
    append(b, payload);
    return b;
}

Bytes fullbox(const char* type, int version, std::uint32_t flags, const Bytes& payload) {
    Bytes p;
    put8(p, static_cast<std::uint32_t>(version));
    put24(p, flags);
    append(p, payload);
    return box(type, p);
}

Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes b;
    for (const Bytes& p : parts) append(b, p);
    return b;
}

Bytes filled(std::size_t n, std::uint8_t seed) {
    Bytes b(n);
    for (std::size_t i = 0; i < n; ++i) b[i] = static_cast<std::uint8_t>(seed + i * 7);
    return b;
}

void put_matrix(Bytes& b, int rotation) {
    std::int32_t m[9] = {0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000};
    if (rotation == 90) {
        m[0] = 0; m[1] = 0x10000; m[3] = -0x10000; m[4] = 0;
    } else if (rotation == 180) {
        m[0] = -0x10000; m[4] = -0x10000;
    }
    for (int i = 0; i < 9; ++i) put32(b, static_cast<std::uint32_t>(m[i]));
}

Bytes mvhd(std::uint32_t timescale, std::uint32_t duration) {
    Bytes p;
    put32(p, 0); put32(p, 0); put32(p, timescale); put32(p, duration);
    put32(p, 0x10000); put16(p, 0x100); put16(p, 0); put32(p, 0); put32(p, 0);
    put_matrix(p, 0);
    for (int i = 0; i < 6; ++i) put32(p, 0);
    put32(p, 3);
    return fullbox("mvhd", 0, 0, p);
}

Bytes tkhd(std::uint32_t id, std::uint32_t duration, int rotation, std::uint32_t w, std::uint32_t h) {
    Bytes p;
    put32(p, 0); put32(p, 0); put32(p, id); put32(p, 0); put32(p, duration);
    put32(p, 0); put32(p, 0); put16(p, 0); put16(p, 0); put16(p, w ? 0 : 0x100); put16(p, 0);
    put_matrix(p, rotation);
    put32(p, w << 16); put32(p, h << 16);
    return fullbox("tkhd", 0, 3, p);
}

Bytes mdhd(std::uint32_t timescale, std::uint32_t duration, const char* lang) {
    Bytes p;
    put32(p, 0); put32(p, 0); put32(p, timescale); put32(p, duration);
    put16(p, static_cast<std::uint32_t>(((lang[0] - 0x60) << 10) | ((lang[1] - 0x60) << 5) | (lang[2] - 0x60)));
    put16(p, 0);
    return fullbox("mdhd", 0, 0, p);
}

Bytes hdlr(const char* type) {
    Bytes p;
    put32(p, 0);
    put_str(p, type);
    put32(p, 0); put32(p, 0); put32(p, 0);
    put8(p, 0);
    return fullbox("hdlr", 0, 0, p);
}

Bytes elst(std::uint32_t duration, std::int32_t media_time, bool empty_first = false, std::uint32_t empty = 0) {
    Bytes p;
    put32(p, empty_first ? 2 : 1);
    if (empty_first) {
        put32(p, empty); put32(p, 0xFFFFFFFFu); put32(p, 0x10000);
    }
    put32(p, duration); put32(p, static_cast<std::uint32_t>(media_time)); put32(p, 0x10000);
    return box("edts", fullbox("elst", 0, 0, p));
}

Bytes avc1_entry(std::uint32_t w, std::uint32_t h, const Bytes& avcc, bool pasp) {
    Bytes p;
    for (int i = 0; i < 6; ++i) put8(p, 0);
    put16(p, 1);
    for (int i = 0; i < 16; ++i) put8(p, 0);
    put16(p, w); put16(p, h);
    put32(p, 0x480000); put32(p, 0x480000); put32(p, 0); put16(p, 1);
    for (int i = 0; i < 32; ++i) put8(p, 0);
    put16(p, 0x18); put16(p, 0xFFFF);
    append(p, box("avcC", avcc));
    if (pasp) {
        Bytes a;
        put32(a, 4); put32(a, 3);
        append(p, box("pasp", a));
    }
    return box("avc1", p);
}

Bytes audio_entry(const char* type, std::uint16_t channels, std::uint32_t rate, const Bytes& children) {
    Bytes p;
    for (int i = 0; i < 6; ++i) put8(p, 0);
    put16(p, 1);
    put16(p, 0); put16(p, 0); put32(p, 0);
    put16(p, channels); put16(p, 16); put16(p, 0); put16(p, 0);
    put32(p, rate << 16);
    append(p, children);
    return box(type, p);
}

Bytes esds(std::uint8_t oti, const Bytes& dsi) {
    Bytes dec;
    put8(dec, oti); put8(dec, 0x15); put24(dec, 0); put32(dec, 128000); put32(dec, 96000);
    put8(dec, 5); put8(dec, static_cast<std::uint32_t>(dsi.size())); append(dec, dsi);
    Bytes es;
    put16(es, 1); put8(es, 0);
    put8(es, 4); put8(es, static_cast<std::uint32_t>(dec.size())); append(es, dec);
    put8(es, 6); put8(es, 1); put8(es, 2);
    Bytes d;
    put8(d, 3); put8(d, static_cast<std::uint32_t>(es.size())); append(d, es);
    return fullbox("esds", 0, 0, d);
}

Bytes stsd(const Bytes& entry) {
    Bytes p;
    put32(p, 1);
    append(p, entry);
    return fullbox("stsd", 0, 0, p);
}

Bytes table32(const char* type, const std::vector<std::uint32_t>& values, int version = 0) {
    Bytes p;
    put32(p, static_cast<std::uint32_t>(values.size()));
    for (std::uint32_t v : values) put32(p, v);
    return fullbox(type, version, 0, p);
}

Bytes table_pairs(const char* type, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& pairs, int version = 0) {
    Bytes p;
    put32(p, static_cast<std::uint32_t>(pairs.size()));
    for (const auto& v : pairs) {
        put32(p, v.first);
        put32(p, v.second);
    }
    return fullbox(type, version, 0, p);
}

Bytes stsc(const std::vector<std::pair<std::uint32_t, std::uint32_t>>& entries) {
    Bytes p;
    put32(p, static_cast<std::uint32_t>(entries.size()));
    for (const auto& e : entries) {
        put32(p, e.first); put32(p, e.second); put32(p, 1);
    }
    return fullbox("stsc", 0, 0, p);
}

Bytes stsz(const std::vector<std::uint32_t>& sizes) {
    Bytes p;
    put32(p, 0);
    put32(p, static_cast<std::uint32_t>(sizes.size()));
    for (std::uint32_t v : sizes) put32(p, v);
    return fullbox("stsz", 0, 0, p);
}

Bytes co64(const std::vector<std::uint64_t>& offsets) {
    Bytes p;
    put32(p, static_cast<std::uint32_t>(offsets.size()));
    for (std::uint64_t v : offsets) put64(p, v);
    return fullbox("co64", 0, 0, p);
}

Bytes trak(const Bytes& tkhd_box, const Bytes& edts, const Bytes& mdhd_box, const char* handler, const Bytes& stbl_children) {
    const bool video = std::strcmp(handler, "vide") == 0;
    Bytes minf_children = video ? fullbox("vmhd", 0, 1, Bytes(8, 0)) : fullbox("smhd", 0, 0, Bytes(4, 0));
    append(minf_children, box("stbl", stbl_children));
    return box("trak", cat({tkhd_box, edts, box("mdia", cat({mdhd_box, hdlr(handler), box("minf", minf_children)}))}));
}

Bytes ilst_text(const char* key, const char* value) {
    Bytes d;
    put32(d, 1); put32(d, 0);
    put_str(d, value);
    return box(key, box("data", d));
}

struct Mp4Test {
    Bytes file;
    Bytes v[4];
    Bytes a[3];
};

Bytes stz2(const std::vector<std::uint32_t>& sizes) {
    Bytes p;
    put24(p, 0);
    put8(p, 8);
    put32(p, static_cast<std::uint32_t>(sizes.size()));
    for (std::uint32_t v : sizes) put8(p, v);
    return fullbox("stz2", 0, 0, p);
}

Mp4Test build_mp4(bool moov_first, bool wide, int rotation) {
    Mp4Test t;
    for (int i = 0; i < 4; ++i) t.v[i] = filled(static_cast<std::size_t>(10 + i), static_cast<std::uint8_t>(0x10 * (i + 1)));
    for (int i = 0; i < 3; ++i) t.a[i] = filled(static_cast<std::size_t>(4 + i), static_cast<std::uint8_t>(0xA0 + i));
    Bytes mdat_payload;
    std::uint64_t rel_v1 = 0;
    append(mdat_payload, t.v[0]);
    append(mdat_payload, t.v[1]);
    const std::uint64_t rel_a = mdat_payload.size();
    append(mdat_payload, t.a[0]);
    append(mdat_payload, t.a[1]);
    append(mdat_payload, t.a[2]);
    const std::uint64_t rel_v2 = mdat_payload.size();
    append(mdat_payload, t.v[2]);
    append(mdat_payload, t.v[3]);
    Bytes ftyp;
    put_str(ftyp, "isom");
    put32(ftyp, 0x200);
    put_str(ftyp, "isomiso2avc1mp41");
    const Bytes ftyp_box = box("ftyp", ftyp);
    auto make_moov = [&](std::uint64_t mdat_data) {
        const Bytes avcc = {1, 0x64, 0, 0x1F, 0xFF, 0xE0};
        Bytes vstbl = cat({
            stsd(avc1_entry(64, 48, avcc, true)),
            table_pairs("stts", {{4, 512}}),
            table_pairs("ctts", {{1, 1024}, {1, 2048}, {2, 512}}, 1),
            table32("stss", {1}),
            stsc({{1, 2}}),
            wide ? stz2({10, 11, 12, 13}) : stsz({10, 11, 12, 13}),
        });
        if (wide) append(vstbl, co64({mdat_data + rel_v1, mdat_data + rel_v2}));
        else append(vstbl, table32("stco", {static_cast<std::uint32_t>(mdat_data + rel_v1), static_cast<std::uint32_t>(mdat_data + rel_v2)}));
        Bytes astbl = cat({
            stsd(audio_entry("mp4a", 2, 44100, esds(0x40, {0x12, 0x10}))),
            table_pairs("stts", {{3, 1024}}),
            stsc({{1, 3}}),
            stsz({4, 5, 6}),
            table32("stco", {static_cast<std::uint32_t>(mdat_data + rel_a)}),
        });
        Bytes ilst = cat({ilst_text("\xA9nam", "Synthetic"), ilst_text("\xA9" "ART", "Tester")});
        Bytes trkn;
        put32(trkn, 0); put32(trkn, 0); put16(trkn, 0); put16(trkn, 3); put16(trkn, 9); put16(trkn, 0);
        append(ilst, box("trkn", box("data", trkn)));
        Bytes meta_payload = cat({hdlr("mdir"), box("ilst", ilst)});
        return box("moov", cat({
            mvhd(1000, 160),
            trak(tkhd(1, 160, rotation, 64, 48), elst(160, 1024), mdhd(12800, 2048, "eng"), "vide", vstbl),
            trak(tkhd(2, 69, 0, 0, 0), elst(69, 1024), mdhd(44100, 3072, "und"), "soun", astbl),
            box("udta", fullbox("meta", 0, 0, meta_payload)),
        }));
    };
    if (moov_first) {
        const std::size_t moov_size = make_moov(0).size();
        const std::uint64_t data = ftyp_box.size() + moov_size + (wide ? 16 : 8);
        t.file = cat({ftyp_box, make_moov(data), wide ? box64("mdat", mdat_payload) : box("mdat", mdat_payload)});
    } else {
        const std::uint64_t data = ftyp_box.size() + (wide ? 16 : 8);
        t.file = cat({ftyp_box, wide ? box64("mdat", mdat_payload) : box("mdat", mdat_payload), make_moov(data)});
    }
    return t;
}

std::vector<Packet> read_all(Demuxer& d, std::size_t limit = 100000) {
    std::vector<Packet> out;
    Packet p;
    while (out.size() < limit && d.read(p)) out.push_back(p);
    return out;
}

void test_mp4_basic(bool moov_first, bool wide) {
    const std::string tag = std::string("mp4 ") + (moov_first ? "moov-first" : "moov-last") + (wide ? " 64-bit" : "");
    Mp4Test t = build_mp4(moov_first, wide, 90);
    check(is_mp4(t.file.data(), t.file.size()), tag + ": is_mp4");
    check(detect_container(t.file.data(), t.file.size()) == "mp4", tag + ": detect");
    Demuxer d;
    std::string err;
    check(d.open(t.file.data(), t.file.size(), &err), tag + ": open " + err);
    if (!d.is_open()) return;
    check(d.format_name() == "mp4", tag + ": format name");
    check(d.tracks().size() == 2, tag + ": two tracks");
    if (d.tracks().size() != 2) return;
    const TrackInfo& v = d.tracks()[0];
    const TrackInfo& a = d.tracks()[1];
    check(v.type == MediaType::video && v.codec == "h264", tag + ": video h264");
    check(v.codec_config == Bytes({1, 0x64, 0, 0x1F, 0xFF, 0xE0}), tag + ": avcC config");
    check(v.width == 64 && v.height == 48, tag + ": video size");
    check(v.display_width == 85 && v.display_height == 48, tag + ": pasp display size");
    check(v.rotation == 90, tag + ": rotation");
    check(v.language == "eng", tag + ": language");
    check(v.frame_count == 4, tag + ": frame count");
    check(near(v.frame_rate, 25.0), tag + ": frame rate");
    check(a.type == MediaType::audio && a.codec == "aac", tag + ": audio aac");
    check(a.codec_config == Bytes({0x12, 0x10}), tag + ": esds DecoderSpecificInfo");
    check(a.sample_rate == 44100 && a.channels == 2, tag + ": audio format");
    check(a.bitrate == 96000, tag + ": esds bitrate");
    check(a.language.empty(), tag + ": und language");
    check(near(d.duration(), 0.16), tag + ": duration");
    check(d.best_track(MediaType::video) == 0 && d.best_track(MediaType::audio) == 1 && d.best_track(MediaType::subtitle) == -1, tag + ": best track");
    bool title = false, artist = false, track = false;
    for (const auto& kv : d.tags()) {
        title = title || (kv.first == "title" && kv.second == "Synthetic");
        artist = artist || (kv.first == "artist" && kv.second == "Tester");
        track = track || (kv.first == "track" && kv.second == "3/9");
    }
    check(title && artist && track, tag + ": ilst tags");
    std::vector<Packet> p = read_all(d);
    check(p.size() == 7, tag + ": packet count");
    if (p.size() != 7) return;
    const std::uint32_t order[7] = {0, 0, 1, 1, 1, 0, 0};
    bool ok_order = true;
    for (int i = 0; i < 7; ++i) ok_order = ok_order && p[static_cast<std::size_t>(i)].track == order[i];
    check(ok_order, tag + ": interleaved file order");
    for (std::size_t i = 1; i < p.size(); ++i) check(p[i].position > p[i - 1].position, tag + ": positions increase");
    check(p[0].data == t.v[0] && p[1].data == t.v[1] && p[5].data == t.v[2] && p[6].data == t.v[3], tag + ": video data");
    check(p[2].data == t.a[0] && p[3].data == t.a[1] && p[4].data == t.a[2], tag + ": audio data");
    check(near(p[0].pts, 0.0) && near(p[0].dts, -0.08), tag + ": first video pts/dts after edit list");
    check(near(p[1].pts, 0.12) && near(p[1].dts, -0.04), tag + ": second video pts/dts");
    check(near(p[5].pts, 0.04) && near(p[6].pts, 0.08), tag + ": B-frame pts");
    check(p[0].keyframe && !p[1].keyframe && !p[5].keyframe, tag + ": stss keyframes");
    check(near(p[0].duration, 0.04), tag + ": video duration");
    check(near(p[2].pts, -1024.0 / 44100) && near(p[3].pts, 0.0) && near(p[4].pts, 1024.0 / 44100), tag + ": audio priming edit");
    check(p[2].keyframe && p[3].keyframe, tag + ": audio keyframes");
    check(d.seek(0.0), tag + ": rewind");
    Packet first;
    check(d.read(first) && d.read(first) && first.track == 0, tag + ": read two video packets");
    d.select(1, false);
    check(d.read(first) && first.track == 0 && first.data == t.v[2], tag + ": deselect audio mid-stream");
    d.select(1, true);
    check(!d.read(first) || first.track == 0, tag + ": reselected audio does not jump backwards");
    d.select(1, false);
    check(!d.selected(1) && d.selected(0), tag + ": selection state");
    check(d.seek(0.05), tag + ": seek");
    std::vector<Packet> q = read_all(d);
    check(!q.empty() && q[0].keyframe && near(q[0].pts, 0.0), tag + ": seek lands on keyframe");
    bool only_video = true;
    for (const Packet& x : q) only_video = only_video && x.track == 0;
    check(only_video && q.size() == 4, tag + ": deselected track skipped");
}

Bytes build_fragmented(Bytes frags[2][3]) {
    Bytes ftyp;
    put_str(ftyp, "iso5");
    put32(ftyp, 0x200);
    put_str(ftyp, "iso5iso6mp41");
    const Bytes avcc = {1, 0x42, 0, 0x1E, 0xFF, 0xE0};
    Bytes vstbl = cat({stsd(avc1_entry(32, 32, avcc, false)), table_pairs("stts", {}), stsc({}), stsz({}), table32("stco", {})});
    Bytes trex;
    put32(trex, 1); put32(trex, 1); put32(trex, 1000); put32(trex, 0); put32(trex, 0x10000);
    Bytes moov = box("moov", cat({
        mvhd(1000, 0),
        trak(tkhd(1, 0, 0, 32, 32), Bytes(), mdhd(25000, 0, "und"), "vide", vstbl),
        box("mvex", fullbox("trex", 0, 0, trex)),
    }));
    Bytes file = cat({box("ftyp", ftyp), moov});
    for (int f = 0; f < 2; ++f) {
        for (int i = 0; i < 3; ++i) frags[f][i] = filled(static_cast<std::size_t>(20 + 3 * f + i), static_cast<std::uint8_t>(0x30 + 16 * f + i));
        Bytes mfhd;
        put32(mfhd, static_cast<std::uint32_t>(f + 1));
        Bytes tfhd;
        put32(tfhd, 1);
        Bytes tfdt;
        put64(tfdt, static_cast<std::uint64_t>(f) * 3000);
        auto make_trun = [&](std::int32_t data_offset) {
            Bytes p;
            put32(p, 3);
            put32(p, static_cast<std::uint32_t>(data_offset));
            put32(p, 0x2000000);
            for (int i = 0; i < 3; ++i) put32(p, static_cast<std::uint32_t>(frags[f][i].size()));
            return fullbox("trun", 0, 0x1 | 0x4 | 0x200, p);
        };
        auto make_moof = [&](std::int32_t data_offset) {
            return box("moof", cat({fullbox("mfhd", 0, 0, mfhd), box("traf", cat({fullbox("tfhd", 0, 0x20000, tfhd), fullbox("tfdt", 1, 0, tfdt), make_trun(data_offset)}))}));
        };
        const Bytes probe = make_moof(0);
        const Bytes moof = make_moof(static_cast<std::int32_t>(probe.size() + 8));
        Bytes mdat;
        for (int i = 0; i < 3; ++i) append(mdat, frags[f][i]);
        append(file, moof);
        append(file, box("mdat", mdat));
    }
    return file;
}

void test_mp4_fragmented() {
    Bytes frags[2][3];
    Bytes file = build_fragmented(frags);
    Demuxer d;
    std::string err;
    check(d.open(file.data(), file.size(), &err), "fmp4: open " + err);
    if (!d.is_open()) return;
    check(d.tracks().size() == 1 && d.tracks()[0].codec == "h264", "fmp4: track");
    check(near(d.duration(), 0.24), "fmp4: duration from fragment scan");
    std::vector<Packet> p = read_all(d);
    check(p.size() == 6, "fmp4: packet count");
    if (p.size() != 6) return;
    bool data_ok = true;
    for (int f = 0; f < 2; ++f) {
        for (int i = 0; i < 3; ++i) data_ok = data_ok && p[static_cast<std::size_t>(f * 3 + i)].data == frags[f][i];
    }
    check(data_ok, "fmp4: packet data");
    check(p[0].keyframe && !p[1].keyframe && p[3].keyframe, "fmp4: first-sample flags vs trex defaults");
    check(near(p[0].pts, 0.0) && near(p[1].pts, 0.04) && near(p[3].pts, 0.12), "fmp4: tfdt timing");
    check(d.seek(0.2), "fmp4: seek");
    std::vector<Packet> q = read_all(d);
    check(q.size() == 3 && q[0].keyframe && near(q[0].pts, 0.12), "fmp4: seek to second fragment keyframe");
    check(d.seek(0.05, false), "fmp4: seek forward");
    q = read_all(d);
    check(!q.empty() && near(q[0].pts, 0.12), "fmp4: seek to next keyframe");
    check(d.seek(0.0), "fmp4: seek start");
    q = read_all(d);
    check(q.size() == 6, "fmp4: seek start reads all");
}

void test_mp4_opus_dops() {
    Bytes dops;
    put8(dops, 0); put8(dops, 2); put16(dops, 312); put32(dops, 48000); put16(dops, 0); put8(dops, 0);
    Bytes sample = {0xF8, 1, 2, 3};
    Bytes astbl = cat({
        stsd(audio_entry("Opus", 2, 48000, box("dOps", dops))),
        table_pairs("stts", {{1, 960}}),
        stsc({{1, 1}}),
        stsz({4}),
    });
    Bytes ftyp;
    put_str(ftyp, "isom");
    put32(ftyp, 0);
    const Bytes ftyp_box = box("ftyp", ftyp);
    const std::uint32_t data = static_cast<std::uint32_t>(ftyp_box.size() + 8);
    append(astbl, table32("stco", {data}));
    Bytes file = cat({ftyp_box, box("mdat", sample), box("moov", cat({mvhd(1000, 20), trak(tkhd(1, 20, 0, 0, 0), elst(13, 312), mdhd(48000, 960, "deu"), "soun", astbl)}))});
    Demuxer d;
    check(d.open(file.data(), file.size()), "mp4 opus: open");
    if (!d.is_open() || d.tracks().empty()) return;
    const TrackInfo& t = d.tracks()[0];
    Bytes head;
    put_str(head, "OpusHead");
    put8(head, 1); put8(head, 2); putle16(head, 312); putle32(head, 48000); putle16(head, 0); put8(head, 0);
    check(t.codec == "opus" && t.codec_config == head, "mp4 opus: dOps converted to OpusHead");
    check(t.language == "deu" && t.sample_rate == 48000, "mp4 opus: info");
    Packet p;
    check(d.read(p) && p.data == sample && near(p.pts, -312.0 / 48000), "mp4 opus: packet with pre-skip edit");
}

void test_mp4_errors() {
    Mp4Test t = build_mp4(false, false, 0);
    Demuxer d;
    std::string err;
    Bytes truncated(t.file.begin(), t.file.begin() + 60);
    check(!d.open(truncated.data(), truncated.size(), &err) && !err.empty(), "mp4: truncated before moov fails with error");
    Bytes junk(64, 0x55);
    check(!d.open(junk.data(), junk.size(), &err), "unknown data rejected");
    check(!d.open(nullptr, 0, &err), "empty data rejected");
    check(!d.open(temp_path("fizmo_video_demux_missing.mp4"), &err), "missing file rejected");
    Packet p;
    check(!d.read(p) && d.tracks().empty() && d.best_track(MediaType::video) == -1, "closed demuxer is inert");
    std::mt19937 rng(7);
    int survived = 0;
    for (int i = 0; i < 300; ++i) {
        Bytes m = t.file;
        for (int k = 0; k < 4; ++k) m[rng() % m.size()] = static_cast<std::uint8_t>(rng());
        if (rng() % 3 == 0) m.resize(rng() % m.size());
        Demuxer f;
        if (f.open(m.data(), m.size())) {
            std::size_t n = 0;
            while (f.read(p) && n < 100000) ++n;
            f.seek(0.04);
            while (f.read(p) && n < 200000) ++n;
        }
        ++survived;
    }
    check(survived == 300, "mp4: mutated files handled");
}

Bytes ebml_id(std::uint32_t id) {
    Bytes b;
    if (id > 0xFFFFFF) put8(b, id >> 24);
    if (id > 0xFFFF) put8(b, (id >> 16) & 0xFF);
    if (id > 0xFF) put8(b, (id >> 8) & 0xFF);
    put8(b, id & 0xFF);
    return b;
}

Bytes el(std::uint32_t id, const Bytes& payload) {
    Bytes b = ebml_id(id);
    put8(b, 0x01);
    for (int i = 6; i >= 0; --i) put8(b, static_cast<std::uint32_t>((payload.size() >> (8 * i)) & 0xFF));
    append(b, payload);
    return b;
}

Bytes el_unknown(std::uint32_t id, const Bytes& payload) {
    Bytes b = ebml_id(id);
    put8(b, 0x01);
    for (int i = 0; i < 7; ++i) put8(b, 0xFF);
    append(b, payload);
    return b;
}

Bytes el_uint(std::uint32_t id, std::uint64_t v) {
    Bytes p;
    put64(p, v);
    return el(id, p);
}

Bytes el_float(std::uint32_t id, double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, 8);
    Bytes p;
    put64(p, bits);
    return el(id, p);
}

Bytes el_str(std::uint32_t id, const char* s) {
    Bytes p;
    put_str(p, s);
    return el(id, p);
}

Bytes block(int track, std::int16_t tc, std::uint8_t flags, const Bytes& payload) {
    Bytes b;
    put8(b, 0x80 | static_cast<std::uint32_t>(track));
    put16(b, static_cast<std::uint16_t>(tc));
    put8(b, flags);
    append(b, payload);
    return b;
}

struct MkvTest {
    Bytes file;
    std::vector<Bytes> video;
    std::vector<Bytes> audio;
    std::uint64_t cluster2_rel = 0;
};

MkvTest build_mkv(bool unknown_sizes, bool garbage) {
    MkvTest t;
    for (int i = 0; i < 6; ++i) t.video.push_back(filled(static_cast<std::size_t>(30 + i), static_cast<std::uint8_t>(0x40 + 9 * i)));
    for (int i = 0; i < 8; ++i) t.audio.push_back(filled(static_cast<std::size_t>(5 + (i % 3)), static_cast<std::uint8_t>(0xC0 + i)));
    for (Bytes& a : t.audio) a[0] = 0xF8;
    Bytes header = cat({el_uint(0x4286, 1), el_uint(0x42F7, 1), el_str(0x4282, "webm"), el_uint(0x4287, 4), el_uint(0x4285, 2)});
    Bytes opus_head;
    put_str(opus_head, "OpusHead");
    put8(opus_head, 1); put8(opus_head, 2); putle16(opus_head, 312); putle32(opus_head, 48000); putle16(opus_head, 0); put8(opus_head, 0);
    Bytes info = el(0x1549A966, cat({el_uint(0x2AD7B1, 1000000), el_float(0x4489, 200.0), el_str(0x7BA9, "Mkv Title")}));
    Bytes strip_enc = el(0x6D80, el(0x6240, cat({el_uint(0x5031, 0), el_uint(0x5032, 1), el_uint(0x5033, 0), el(0x5034, cat({el_uint(0x4254, 3), el(0x4255, Bytes({0xAB, 0xCD}))}))})));
    Bytes tracks = el(0x1654AE6B, cat({
        el(0xAE, cat({el_uint(0xD7, 1), el_uint(0x73C5, 111), el_uint(0x83, 1), el_str(0x86, "V_VP9"), el_uint(0x23E383, 40000000),
                      el(0xE0, cat({el_uint(0xB0, 320), el_uint(0xBA, 240), el_uint(0x54B0, 640), el_uint(0x54BA, 240)})), strip_enc})),
        el(0xAE, cat({el_uint(0xD7, 2), el_uint(0x73C5, 222), el_uint(0x83, 2), el_str(0x86, "A_OPUS"), el(0x63A2, opus_head), el_uint(0x56AA, 0),
                      el_str(0x22B59C, "fre"), el_str(0x536E, "French"), el_uint(0x88, 0), el(0xE1, cat({el_float(0xB5, 48000.0), el_uint(0x9F, 2)}))})),
    }));
    Bytes tags = el(0x1254C367, cat({
        el(0x7373, cat({el(0x63C0, el_uint(0x68CA, 50)), el(0x67C8, cat({el_str(0x45A3, "ARTIST"), el_str(0x4487, "Someone")}))})),
        el(0x7373, cat({el(0x63C0, el_uint(0x63C5, 222)), el(0x67C8, cat({el_str(0x45A3, "DURATION"), el_str(0x4487, "00:00:00.180000000")}))})),
    }));
    auto strip = [](const Bytes& v) { return Bytes(v.begin() + 2, v.end()); };
    for (Bytes& v : t.video) {
        v[0] = 0xAB;
        v[1] = 0xCD;
    }
    Bytes xiph;
    put8(xiph, 2);
    put8(xiph, static_cast<std::uint32_t>(t.audio[1].size()));
    put8(xiph, static_cast<std::uint32_t>(t.audio[2].size()));
    append(xiph, t.audio[1]); append(xiph, t.audio[2]); append(xiph, t.audio[3]);
    Bytes ebml_lace;
    put8(ebml_lace, 2);
    put8(ebml_lace, 0x80 | static_cast<std::uint32_t>(t.audio[5].size()));
    const int diff = static_cast<int>(t.audio[6].size()) - static_cast<int>(t.audio[5].size());
    put8(ebml_lace, 0x80 | static_cast<std::uint32_t>(diff + 63));
    append(ebml_lace, t.audio[5]); append(ebml_lace, t.audio[6]); append(ebml_lace, t.audio[7]);
    Bytes c1 = cat({
        el_uint(0xE7, 0),
        el(0xA3, block(1, 0, 0x80, strip(t.video[0]))),
        el(0xA3, block(2, 0, 0x80, t.audio[0])),
        el(0xA3, block(1, 40, 0x00, strip(t.video[1]))),
        el(0xA3, block(2, 20, 0x82, xiph)),
        el(0xA0, cat({el(0xA1, block(1, 80, 0x00, strip(t.video[2]))), el_uint(0xFB, 40), el_uint(0x9B, 40)})),
    });
    Bytes c2 = cat({
        el_uint(0xE7, 120),
        el(0xA3, block(1, 0, 0x80, strip(t.video[3]))),
        el(0xA3, block(2, -40, 0x80, t.audio[4])),
        el(0xA3, block(1, 40, 0x00, strip(t.video[4]))),
        el(0xA3, block(2, 0, 0x86, ebml_lace)),
        el(0xA0, cat({el(0xA1, block(1, 80, 0x00, strip(t.video[5]))), el_uint(0xFB, 40)})),
    });
    Bytes cluster1 = unknown_sizes ? el_unknown(0x1F43B675, c1) : el(0x1F43B675, c1);
    Bytes cluster2 = unknown_sizes ? el_unknown(0x1F43B675, c2) : el(0x1F43B675, c2);
    Bytes junk;
    if (garbage) junk = Bytes(37, 0xEE);
    Bytes seg_body = cat({info, tracks, tags, cluster1, junk});
    t.cluster2_rel = seg_body.size();
    append(seg_body, cluster2);
    Bytes cues = el(0x1C53BB6B, cat({
        el(0xBB, cat({el_uint(0xB3, 0), el(0xB7, cat({el_uint(0xF7, 1), el_uint(0xF1, info.size() + tracks.size() + tags.size())}))})),
        el(0xBB, cat({el_uint(0xB3, 120), el(0xB7, cat({el_uint(0xF7, 1), el_uint(0xF1, t.cluster2_rel)}))})),
    }));
    if (!unknown_sizes) append(seg_body, cues);
    t.file = cat({el(0x1A45DFA3, header), unknown_sizes ? el_unknown(0x18538067, seg_body) : el(0x18538067, seg_body)});
    return t;
}

void test_matroska(bool unknown_sizes, bool garbage) {
    const std::string tag = std::string("mkv") + (unknown_sizes ? " live" : "") + (garbage ? " damaged" : "");
    MkvTest t = build_mkv(unknown_sizes, garbage);
    check(is_matroska(t.file.data(), t.file.size()), tag + ": is_matroska");
    check(detect_container(t.file.data(), t.file.size()) == "webm", tag + ": detect webm");
    const std::string path = temp_path("fizmo_video_demux_test.webm");
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f) {
        std::fwrite(t.file.data(), 1, t.file.size(), f);
        std::fclose(f);
    }
    Demuxer d;
    std::string err;
    check(d.open(path, &err), tag + ": open from file " + err);
    if (!d.is_open()) return;
    check(d.format_name() == "webm", tag + ": format name");
    check(d.tracks().size() == 2, tag + ": tracks");
    if (d.tracks().size() != 2) return;
    const TrackInfo& v = d.tracks()[0];
    const TrackInfo& a = d.tracks()[1];
    check(v.codec == "vp9" && v.width == 320 && v.height == 240 && v.display_width == 640, tag + ": video info");
    check(near(v.frame_rate, 25.0), tag + ": DefaultDuration frame rate");
    check(a.codec == "opus" && a.sample_rate == 48000 && a.channels == 2 && a.codec_config.size() == 19, tag + ": audio info");
    check(a.language == "fre" && a.name == "French" && !a.is_default, tag + ": audio flags");
    check(near(a.duration, 0.18), tag + ": track DURATION tag");
    check(near(d.duration(), 0.2), tag + ": segment duration");
    check(d.best_track(MediaType::audio) == 1, tag + ": best audio is the only audio");
    bool title = false, artist = false;
    for (const auto& kv : d.tags()) {
        title = title || (kv.first == "title" && kv.second == "Mkv Title");
        artist = artist || (kv.first == "artist" && kv.second == "Someone");
    }
    check(title && artist, tag + ": tags");
    std::vector<Packet> p = read_all(d);
    std::vector<Packet> vp, ap;
    for (const Packet& x : p) (x.track == 0 ? vp : ap).push_back(x);
    check(vp.size() == 6 && ap.size() == 8, tag + ": packet counts");
    if (vp.size() != 6 || ap.size() != 8) return;
    bool vdata = true;
    for (int i = 0; i < 6; ++i) vdata = vdata && vp[static_cast<std::size_t>(i)].data == t.video[static_cast<std::size_t>(i)];
    check(vdata, tag + ": header stripping restored");
    bool adata = true;
    for (int i = 0; i < 8; ++i) adata = adata && ap[static_cast<std::size_t>(i)].data == t.audio[static_cast<std::size_t>(i)];
    check(adata, tag + ": lacing (xiph, ebml) data");
    check(vp[0].keyframe && !vp[1].keyframe && !vp[2].keyframe && vp[3].keyframe, tag + ": keyframe flags");
    check(near(vp[2].pts, 0.08) && near(vp[2].duration, 0.04) && near(vp[3].pts, 0.12), tag + ": block timing");
    check(near(ap[1].pts, 0.02) && near(ap[2].pts, 0.04) && near(ap[3].pts, 0.06), tag + ": xiph lace timing from opus TOC");
    check(near(ap[4].pts, 0.08) && near(ap[5].pts, 0.12), tag + ": negative relative timecode and ebml lace");
    check(d.seek(0.15), tag + ": seek");
    std::vector<Packet> q = read_all(d);
    check(!q.empty() && q[0].track == 0 && q[0].keyframe && near(q[0].pts, 0.12), tag + ": seek to keyframe");
    bool audio_from = true;
    for (const Packet& x : q) {
        if (x.track == 1) audio_from = audio_from && x.pts + x.duration > 0.12 - 1e-9;
    }
    check(audio_from, tag + ": audio after seek starts at keyframe time");
    check(d.seek(0.0), tag + ": seek to start");
    q = read_all(d);
    check(q.size() == 14, tag + ": seek to start reads everything");
    d.select(0, false);
    check(d.seek(0.09), tag + ": audio-only seek");
    q = read_all(d);
    check(!q.empty() && q[0].track == 1 && q[0].pts <= 0.09 + 1e-9 && q[0].pts + q[0].duration > 0.09 - 1e-9, tag + ": audio-only seek lands on covering packet");
    std::remove(path.c_str());
}

void test_matroska_fuzz() {
    MkvTest t = build_mkv(false, false);
    std::mt19937 rng(11);
    int done = 0;
    for (int i = 0; i < 400; ++i) {
        Bytes m = t.file;
        for (int k = 0; k < 3; ++k) m[rng() % m.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
        if (rng() % 4 == 0) m.resize(rng() % m.size() + 1);
        Demuxer d;
        if (d.open(m.data(), m.size())) {
            Packet p;
            std::size_t n = 0;
            while (d.read(p) && n < 100000) ++n;
            d.seek(0.1);
            while (d.read(p) && n < 200000) ++n;
        }
        ++done;
    }
    check(done == 400, "mkv: mutated files handled");
}

Bytes ogg_page(std::uint8_t flags, std::int64_t granule, std::uint32_t serial, std::uint32_t seq, const std::vector<Bytes>& packets) {
    Bytes lacing, body;
    for (const Bytes& p : packets) {
        std::size_t n = p.size();
        while (n >= 255) {
            lacing.push_back(255);
            n -= 255;
        }
        lacing.push_back(static_cast<std::uint8_t>(n));
        append(body, p);
    }
    Bytes page;
    put_str(page, "OggS");
    put8(page, 0);
    put8(page, flags);
    putle64(page, static_cast<std::uint64_t>(granule));
    putle32(page, serial);
    putle32(page, seq);
    putle32(page, 0);
    put8(page, static_cast<std::uint32_t>(lacing.size()));
    append(page, lacing);
    append(page, body);
    const std::uint32_t crc = fizmo::audio::detail::ogg_crc(page.data(), page.size());
    page[22] = static_cast<std::uint8_t>(crc & 0xFF);
    page[23] = static_cast<std::uint8_t>((crc >> 8) & 0xFF);
    page[24] = static_cast<std::uint8_t>((crc >> 16) & 0xFF);
    page[25] = static_cast<std::uint8_t>(crc >> 24);
    return page;
}

void test_ogg_opus() {
    Bytes head;
    put_str(head, "OpusHead");
    put8(head, 1); put8(head, 1); putle16(head, 312); putle32(head, 44100); putle16(head, 0); put8(head, 0);
    Bytes tags;
    put_str(tags, "OpusTags");
    putle32(tags, 4);
    put_str(tags, "test");
    putle32(tags, 1);
    putle32(tags, 12);
    put_str(tags, "TITLE=Hello!");
    std::vector<Bytes> packets;
    for (int i = 0; i < 5; ++i) packets.push_back(Bytes({0xF8, static_cast<std::uint8_t>(i), 0x11, 0x22}));
    Bytes file = cat({
        ogg_page(2, 0, 77, 0, {head}),
        ogg_page(0, 0, 77, 1, {tags}),
        ogg_page(0, 3 * 960, 77, 2, {packets[0], packets[1], packets[2]}),
        ogg_page(4, 4 * 960 + 500, 77, 3, {packets[3], packets[4]}),
    });
    check(detect_container(file.data(), file.size()) == "ogg", "ogg: detect");
    Demuxer d;
    std::string err;
    check(d.open(file.data(), file.size(), &err), "ogg: open " + err);
    if (!d.is_open() || d.tracks().size() != 1) {
        check(false, "ogg: one track");
        return;
    }
    const TrackInfo& t = d.tracks()[0];
    check(t.codec == "opus" && t.codec_config == head && t.channels == 1 && t.sample_rate == 48000, "ogg: opus track info");
    check(near(d.duration(), (4 * 960 + 500 - 312) / 48000.0), "ogg: duration from last granule");
    bool title = false;
    for (const auto& kv : d.tags()) title = title || (kv.first == "title" && kv.second == "Hello!");
    check(title, "ogg: OpusTags");
    std::vector<Packet> p = read_all(d);
    check(p.size() == 5, "ogg: packet count");
    if (p.size() != 5) return;
    bool ok = true;
    for (int i = 0; i < 5; ++i) {
        ok = ok && p[static_cast<std::size_t>(i)].data == packets[static_cast<std::size_t>(i)];
        ok = ok && near(p[static_cast<std::size_t>(i)].pts, (i * 960 - 312) / 48000.0);
    }
    check(ok, "ogg: packet data and pre-skip timing");
    check(near(p[3].duration, 0.02) && near(p[4].duration, 500.0 / 48000), "ogg: end trimming on last packet");
    check(d.seek(0.05), "ogg: seek");
    std::vector<Packet> q = read_all(d);
    check(!q.empty() && q[0].pts <= 0.05 + 1e-9 && q[0].pts + q[0].duration > 0.05, "ogg: seek lands on covering packet");
    check(d.seek(0.0), "ogg: seek start");
    q = read_all(d);
    check(q.size() == 5 && q[0].data == packets[0], "ogg: seek start skips header packets");
}

void test_codec_helpers() {
    Demuxer d;
    check(d.format_name().empty() && d.duration() == 0.0 && d.tags().empty(), "empty demuxer defaults");
    check(!is_mp4(nullptr, 0) && !is_matroska(nullptr, 0) && detect_container(nullptr, 0).empty(), "detect on null");
    check(std::string(media_type_name(MediaType::subtitle)) == "subtitle", "media type names");
    Demuxer moved;
    Mp4Test t = build_mp4(true, false, 0);
    check(moved.open(t.file.data(), t.file.size()), "open for move");
    Demuxer target(std::move(moved));
    check(target.is_open() && target.tracks().size() == 2 && target.tracks()[0].rotation == 0, "move construct keeps state");
    Packet p;
    check(target.read(p) && p.track == 0, "moved demuxer reads");
}

} // namespace

int main() {
    test_mp4_basic(false, false);
    test_mp4_basic(true, false);
    test_mp4_basic(true, true);
    test_mp4_basic(false, true);
    test_mp4_fragmented();
    test_mp4_opus_dops();
    test_mp4_errors();
    test_matroska(false, false);
    test_matroska(true, false);
    test_matroska(false, true);
    test_matroska(true, true);
    test_matroska_fuzz();
    test_ogg_opus();
    test_codec_helpers();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
