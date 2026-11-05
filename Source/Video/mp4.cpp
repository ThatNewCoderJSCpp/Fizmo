#include "demux_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

constexpr std::uint32_t fourcc(const char (&s)[5]) noexcept {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[0])) << 24) | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[2])) << 8) | static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[3]));
}

const std::uint64_t k_max_moov = std::uint64_t(1) << 29;
const std::uint64_t k_max_moof = std::uint64_t(1) << 26;
const std::size_t k_max_samples = std::size_t(1) << 24;
const std::size_t k_max_trun_samples = std::size_t(1) << 20;
const std::size_t k_max_top_boxes = std::size_t(1) << 22;
const std::size_t k_max_queue = std::size_t(1) << 20;

struct Box {
    std::uint32_t type = 0;
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t start = 0;
};

bool next_box(ByteReader& r, Box& box) {
    if (r.left() < 8) return false;
    const std::size_t start = r.pos();
    std::uint64_t size = r.be32();
    box.type = r.be32();
    std::uint64_t header = 8;
    if (size == 1) {
        if (r.left() < 8) return false;
        size = r.be64();
        header = 16;
    } else if (size == 0) {
        size = header + r.left();
    }
    if (size < header) return false;
    std::uint64_t body = size - header;
    if (body > r.left()) body = r.left();
    box.data = r.here();
    box.size = static_cast<std::size_t>(body);
    box.start = start;
    r.skip(box.size);
    return true;
}

bool find_box(const std::uint8_t* data, std::size_t size, std::uint32_t type, Box& out) {
    ByteReader r(data, size);
    Box b;
    while (next_box(r, b)) {
        if (b.type == type) {
            out = b;
            return true;
        }
    }
    return false;
}

bool find_box_deep(const std::uint8_t* data, std::size_t size, std::uint32_t type, Box& out, int depth = 0) {
    if (depth > 4) return false;
    ByteReader r(data, size);
    Box b;
    while (next_box(r, b)) {
        if (b.type == type) {
            out = b;
            return true;
        }
        if (b.type == fourcc("wave") || b.type == fourcc("sinf")) {
            if (find_box_deep(b.data, b.size, type, out, depth + 1)) return true;
        }
    }
    return false;
}

std::string fourcc_string(std::uint32_t v) {
    std::string s(4, ' ');
    for (int i = 0; i < 4; ++i) s[static_cast<std::size_t>(i)] = static_cast<char>((v >> (24 - 8 * i)) & 0xFF);
    return s;
}

struct Sample {
    std::uint64_t offset = 0;
    std::int64_t dts = 0;
    std::uint32_t size = 0;
    std::int32_t cts = 0;
    std::uint32_t duration = 0;
    bool key = true;
};

struct Edit {
    std::uint64_t duration = 0;
    std::int64_t media_time = 0;
};

struct FragSpan {
    std::size_t track = 0;
    double min_pts = 0.0;
    double max_end = 0.0;
    std::vector<double> keys;
};

struct Fragment {
    std::uint64_t offset = 0;
    std::vector<std::int64_t> next_dts;
    std::vector<FragSpan> spans;
};

struct QueuedSample {
    std::size_t track = 0;
    Sample sample;
};

struct Mp4Track {
    std::uint32_t track_id = 0;
    std::uint32_t handler = 0;
    std::uint32_t timescale = 0;
    std::uint64_t media_duration = 0;
    std::uint64_t tkhd_duration = 0;
    bool enabled_flag = true;
    double matrix[4] = {1.0, 0.0, 0.0, 1.0};
    double tkhd_width = 0.0;
    double tkhd_height = 0.0;
    std::vector<Edit> edits;
    std::int64_t media_time = 0;
    std::int64_t dts_shift = 0;
    double start_offset = 0.0;
    std::vector<Sample> samples;
    std::size_t cursor = 0;
    std::uint32_t trex_duration = 0;
    std::uint32_t trex_size = 0;
    std::uint32_t trex_flags = 0;
    std::int64_t next_dts = 0;
    std::uint64_t total_bytes = 0;
    std::uint32_t frame_bytes = 0;
    bool pcm = false;
    std::uint32_t pasp_h = 0;
    std::uint32_t pasp_v = 0;
    SeekTarget target;

    double seconds(std::int64_t t) const noexcept {
        return static_cast<double>(t - media_time) / static_cast<double>(timescale) + start_offset;
    }

    double dts_seconds(std::int64_t t) const noexcept {
        return seconds(t - dts_shift);
    }
};

struct TrakTables {
    Box stsd, stts, ctts, stss, stsc, stsz, stz2, stco, co64;
    bool has_stsd = false, has_stts = false, has_ctts = false, has_stss = false, has_stsc = false;
    bool has_stsz = false, has_stz2 = false, has_stco = false, has_co64 = false;
};

std::uint32_t descriptor_length(ByteReader& r) {
    std::uint32_t len = 0;
    for (int i = 0; i < 4; ++i) {
        const std::uint8_t b = r.u8();
        len = (len << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
    }
    return len;
}

struct EsdsInfo {
    std::uint8_t object_type = 0;
    std::uint32_t avg_bitrate = 0;
    std::vector<std::uint8_t> config;
    bool ok = false;
};

void parse_descriptors(ByteReader& r, EsdsInfo& info, int depth) {
    if (depth > 4) return;
    while (r.left() >= 2 && r.ok()) {
        const std::uint8_t tag = r.u8();
        const std::uint32_t len = descriptor_length(r);
        if (r.failed()) return;
        const std::size_t avail = std::min<std::size_t>(len, r.left());
        ByteReader sub(r.here(), avail);
        r.skip(avail);
        if (tag == 3) {
            sub.skip(2);
            const std::uint8_t flags = sub.u8();
            if (flags & 0x80) sub.skip(2);
            if (flags & 0x40) sub.skip(sub.u8());
            if (flags & 0x20) sub.skip(2);
            if (sub.ok()) parse_descriptors(sub, info, depth + 1);
        } else if (tag == 4) {
            info.object_type = sub.u8();
            sub.skip(1 + 3 + 4);
            info.avg_bitrate = sub.be32();
            info.ok = sub.ok();
            if (sub.ok()) parse_descriptors(sub, info, depth + 1);
        } else if (tag == 5) {
            if (info.config.empty()) info.config.assign(sub.data(), sub.data() + sub.size());
        }
    }
}

EsdsInfo parse_esds(const Box& box) {
    EsdsInfo info;
    if (box.size < 4) return info;
    ByteReader r(box.data + 4, box.size - 4);
    parse_descriptors(r, info, 0);
    return info;
}

std::string codec_from_oti(std::uint8_t oti, const std::string& fallback) {
    switch (oti) {
        case 0x40: case 0x66: case 0x67: case 0x68: return "aac";
        case 0x69: case 0x6B: return "mp3";
        case 0x20: return "mpeg4";
        case 0x21: return "h264";
        case 0x23: return "hevc";
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: return "mpeg2video";
        case 0x6A: return "mpeg1video";
        case 0x6C: return "mjpeg";
        case 0xA5: return "ac3";
        case 0xA6: return "eac3";
        case 0xA9: return "dts";
        case 0xAD: return "opus";
        case 0xDD: return "vorbis";
        default: return fallback;
    }
}

std::string pcm_codec(bool is_float, bool little, bool is_signed, int bits) {
    if (is_float) {
        if (bits == 64) return little ? "pcm_f64le" : "pcm_f64be";
        return little ? "pcm_f32le" : "pcm_f32be";
    }
    if (bits == 8) return is_signed ? "pcm_s8" : "pcm_u8";
    const std::string b = bits == 24 ? "24" : bits == 32 ? "32" : "16";
    return std::string("pcm_s") + b + (little ? "le" : "be");
}

std::string iso_language(std::uint16_t code) {
    if (code == 0 || code == 0x7FFF) return std::string();
    if (code < 0x400) return std::string();
    std::string s(3, ' ');
    for (int i = 0; i < 3; ++i) {
        const int c = ((code >> (10 - 5 * i)) & 31) + 0x60;
        if (c < 'a' || c > 'z') return std::string();
        s[static_cast<std::size_t>(i)] = static_cast<char>(c);
    }
    return s == "und" ? std::string() : s;
}

class Mp4Reader : public Reader {
public:
    bool open(Source& source, std::string* error) override;
    bool read(Packet& out) override;
    bool seek(double seconds, bool before) override;
    bool rewind() override;
    void selection_changed(std::size_t index, bool on) override;

private:
    bool parse_moov(const std::uint8_t* data, std::size_t size);
    void parse_trak(const std::uint8_t* data, std::size_t size);
    bool parse_sample_entry(Mp4Track& t, TrackInfo& info, const Box& stsd);
    void build_samples(Mp4Track& t, TrackInfo& info, const TrakTables& tables);
    void finish_track(Mp4Track& t, TrackInfo& info);
    void parse_meta(const std::uint8_t* data, std::size_t size);
    void parse_ilst(const std::uint8_t* data, std::size_t size);
    void parse_udta(const std::uint8_t* data, std::size_t size);
    bool parse_moof(std::uint64_t offset, std::vector<std::int64_t>& next_dts, std::vector<QueuedSample>& out);
    bool read_top_header(std::uint64_t pos, std::uint32_t& type, std::uint64_t& header, std::uint64_t& size);
    void build_fragment_index();
    bool seek_tables(double seconds, bool before);
    bool seek_fragments(double seconds, bool before);
    bool emit(std::size_t index, const Sample& s, Packet& out);
    int track_by_id(std::uint32_t id) const noexcept;

    Source* m_src = nullptr;
    std::vector<Mp4Track> m_tracks;
    std::uint32_t m_movie_timescale = 0;
    std::uint64_t m_movie_duration = 0;
    std::uint64_t m_fragment_duration = 0;
    bool m_fragmented = false;
    std::uint64_t m_frag_start = 0;
    std::uint64_t m_scan_pos = 0;
    std::vector<QueuedSample> m_queue;
    std::size_t m_queue_pos = 0;
    bool m_index_built = false;
    std::vector<Fragment> m_index;
    std::uint64_t m_last_offset = 0;
};

void Mp4Reader::selection_changed(std::size_t index, bool on) {
    if (!on || index >= m_tracks.size()) return;
    Mp4Track& t = m_tracks[index];
    t.cursor = 0;
    while (t.cursor < t.samples.size() && t.samples[t.cursor].offset < m_last_offset) ++t.cursor;
    t.target.active = false;
}

int Mp4Reader::track_by_id(std::uint32_t id) const noexcept {
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].track_id == id) return static_cast<int>(i);
    }
    return -1;
}

bool Mp4Reader::read_top_header(std::uint64_t pos, std::uint32_t& type, std::uint64_t& header, std::uint64_t& size) {
    std::uint8_t h[16];
    const std::uint64_t file = m_src->size();
    if (pos >= file || file - pos < 8) return false;
    if (!m_src->read_exact(pos, h, 8)) return false;
    ByteReader r(h, 8);
    size = r.be32();
    type = r.be32();
    header = 8;
    if (size == 1) {
        if (file - pos < 16 || !m_src->read_exact(pos + 8, h + 8, 8)) return false;
        ByteReader r2(h + 8, 8);
        size = r2.be64();
        header = 16;
    } else if (size == 0) {
        size = file - pos;
    }
    return size >= header;
}

bool Mp4Reader::open(Source& source, std::string* error) {
    m_src = &source;
    const std::uint64_t file = source.size();
    std::uint64_t pos = 0;
    bool have_moov = false;
    std::uint32_t brand = 0;
    std::uint64_t first_moof = 0;
    std::size_t count = 0;
    while (pos < file && count++ < k_max_top_boxes) {
        std::uint32_t type = 0;
        std::uint64_t header = 0;
        std::uint64_t size = 0;
        if (!read_top_header(pos, type, header, size)) break;
        if (type == fourcc("ftyp") && brand == 0) {
            std::uint8_t b[4];
            if (size >= header + 4 && source.read_exact(pos + header, b, 4)) brand = ByteReader(b, 4).be32();
        } else if (type == fourcc("moov") && !have_moov) {
            std::uint64_t body = size - header;
            if (body > file - pos - header) body = file - pos - header;
            if (body > k_max_moov) {
                if (error) *error = "moov box too large";
                return false;
            }
            std::vector<std::uint8_t> buf;
            if (!source.read_vector(pos + header, static_cast<std::size_t>(body), buf)) {
                if (error) *error = "can't read moov";
                return false;
            }
            if (!parse_moov(buf.data(), buf.size())) {
                if (error) *error = "invalid moov";
                return false;
            }
            have_moov = true;
            m_frag_start = pos + size;
            if (first_moof) m_frag_start = first_moof;
            break;
        } else if (type == fourcc("moof") && first_moof == 0) {
            first_moof = pos;
        }
        if (size > file - pos) break;
        pos += size;
    }
    if (!have_moov) {
        if (error) *error = "no moov box";
        return false;
    }
    if (brand == fourcc("qt  ")) format = "mov";
    else if (brand == fourcc("M4A ") || brand == fourcc("M4B ") || brand == fourcc("M4P ")) format = "m4a";
    else if ((brand >> 8) == ((fourcc("3gp4") >> 8)) || (brand >> 8) == (fourcc("3g2a") >> 8)) format = "3gp";
    else if (brand == 0) format = "mov";
    else format = "mp4";
    for (Mp4Track& t : m_tracks) {
        if (!t.samples.empty()) t.next_dts = t.samples.back().dts + t.samples.back().duration;
    }
    m_scan_pos = m_frag_start;
    if (m_movie_timescale) duration = static_cast<double>(m_movie_duration) / m_movie_timescale;
    if (m_fragment_duration && m_movie_timescale) duration = std::max(duration, static_cast<double>(m_fragment_duration) / m_movie_timescale);
    if (m_fragmented && m_fragment_duration == 0) {
        build_fragment_index();
        for (const Fragment& f : m_index) {
            for (const FragSpan& s : f.spans) {
                if (s.track < tracks.size()) {
                    duration = std::max(duration, s.max_end);
                    tracks[s.track].duration = std::max(tracks[s.track].duration, s.max_end - std::max(0.0, m_tracks[s.track].start_offset));
                }
            }
        }
    }
    double longest = 0.0;
    for (const TrackInfo& info : tracks) longest = std::max(longest, info.duration);
    if (duration <= 0.0) duration = longest;
    return true;
}

bool Mp4Reader::parse_moov(const std::uint8_t* data, std::size_t size) {
    ByteReader r(data, size);
    Box b;
    bool mvhd = false;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> trex;
    std::vector<std::uint32_t> trex_dur, trex_size;
    while (next_box(r, b)) {
        if (b.type == fourcc("mvhd")) {
            ByteReader m(b.data, b.size);
            const std::uint8_t version = m.u8();
            m.skip(3);
            if (version == 1) {
                m.skip(16);
                m_movie_timescale = m.be32();
                m_movie_duration = m.be64();
            } else {
                m.skip(8);
                m_movie_timescale = m.be32();
                m_movie_duration = m.be32();
                if (m_movie_duration == 0xFFFFFFFFu) m_movie_duration = 0;
            }
            mvhd = m.ok();
        } else if (b.type == fourcc("trak")) {
            if (m_tracks.size() < k_max_tracks) parse_trak(b.data, b.size);
        } else if (b.type == fourcc("mvex")) {
            m_fragmented = true;
            ByteReader mr(b.data, b.size);
            Box c;
            while (next_box(mr, c)) {
                ByteReader cr(c.data, c.size);
                if (c.type == fourcc("trex")) {
                    cr.skip(4);
                    const std::uint32_t id = cr.be32();
                    cr.skip(4);
                    const std::uint32_t d = cr.be32();
                    const std::uint32_t s = cr.be32();
                    const std::uint32_t f = cr.be32();
                    if (cr.ok()) {
                        trex.emplace_back(id, f);
                        trex_dur.push_back(d);
                        trex_size.push_back(s);
                    }
                } else if (c.type == fourcc("mehd")) {
                    const std::uint8_t version = cr.u8();
                    cr.skip(3);
                    m_fragment_duration = version == 1 ? cr.be64() : cr.be32();
                    if (cr.failed()) m_fragment_duration = 0;
                }
            }
        } else if (b.type == fourcc("udta")) {
            parse_udta(b.data, b.size);
        } else if (b.type == fourcc("meta")) {
            parse_meta(b.data, b.size);
        }
    }
    if (!mvhd || m_movie_timescale == 0) m_movie_timescale = 1000;
    for (std::size_t i = 0; i < trex.size(); ++i) {
        const int idx = track_by_id(trex[i].first);
        if (idx < 0) continue;
        Mp4Track& t = m_tracks[static_cast<std::size_t>(idx)];
        t.trex_flags = trex[i].second;
        t.trex_duration = trex_dur[i];
        t.trex_size = trex_size[i];
    }
    for (std::size_t i = 0; i < m_tracks.size(); ++i) finish_track(m_tracks[i], tracks[i]);
    return true;
}

void Mp4Reader::parse_trak(const std::uint8_t* data, std::size_t size) {
    Mp4Track t;
    TrackInfo info;
    TrakTables tables;
    std::string name;
    bool have_mdhd = false;
    ByteReader r(data, size);
    Box b;
    std::uint16_t lang = 0;
    while (next_box(r, b)) {
        if (b.type == fourcc("tkhd")) {
            ByteReader h(b.data, b.size);
            const std::uint8_t version = h.u8();
            const std::uint32_t flags = h.be24();
            t.enabled_flag = (flags & 1) != 0;
            if (version == 1) {
                h.skip(16);
                t.track_id = h.be32();
                h.skip(4);
                t.tkhd_duration = h.be64();
            } else {
                h.skip(8);
                t.track_id = h.be32();
                h.skip(4);
                t.tkhd_duration = h.be32();
            }
            h.skip(8 + 2 + 2 + 2 + 2);
            std::int32_t m[9];
            for (int i = 0; i < 9; ++i) m[i] = static_cast<std::int32_t>(h.be32());
            const std::uint32_t w = h.be32();
            const std::uint32_t hh = h.be32();
            if (h.ok()) {
                t.matrix[0] = m[0] / 65536.0;
                t.matrix[1] = m[1] / 65536.0;
                t.matrix[2] = m[3] / 65536.0;
                t.matrix[3] = m[4] / 65536.0;
                t.tkhd_width = w / 65536.0;
                t.tkhd_height = hh / 65536.0;
            }
        } else if (b.type == fourcc("edts")) {
            Box e;
            if (find_box(b.data, b.size, fourcc("elst"), e)) {
                ByteReader er(e.data, e.size);
                const std::uint8_t version = er.u8();
                er.skip(3);
                std::uint32_t n = er.be32();
                const std::size_t entry = version == 1 ? 20 : 12;
                n = static_cast<std::uint32_t>(std::min<std::size_t>(n, er.left() / entry));
                for (std::uint32_t i = 0; i < n && t.edits.size() < 4096; ++i) {
                    Edit ed;
                    if (version == 1) {
                        ed.duration = er.be64();
                        ed.media_time = static_cast<std::int64_t>(er.be64());
                    } else {
                        ed.duration = er.be32();
                        ed.media_time = static_cast<std::int32_t>(er.be32());
                    }
                    er.skip(4);
                    t.edits.push_back(ed);
                }
            }
        } else if (b.type == fourcc("mdia")) {
            ByteReader mr(b.data, b.size);
            Box c;
            while (next_box(mr, c)) {
                if (c.type == fourcc("mdhd")) {
                    ByteReader h(c.data, c.size);
                    const std::uint8_t version = h.u8();
                    h.skip(3);
                    if (version == 1) {
                        h.skip(16);
                        t.timescale = h.be32();
                        t.media_duration = h.be64();
                    } else {
                        h.skip(8);
                        t.timescale = h.be32();
                        t.media_duration = h.be32();
                        if (t.media_duration == 0xFFFFFFFFu) t.media_duration = 0;
                    }
                    lang = h.be16();
                    have_mdhd = h.ok();
                } else if (c.type == fourcc("hdlr")) {
                    ByteReader h(c.data, c.size);
                    h.skip(8);
                    t.handler = h.be32();
                } else if (c.type == fourcc("minf")) {
                    Box stbl;
                    if (find_box(c.data, c.size, fourcc("stbl"), stbl)) {
                        ByteReader sr(stbl.data, stbl.size);
                        Box s;
                        while (next_box(sr, s)) {
                            switch (s.type) {
                                case fourcc("stsd"): tables.stsd = s; tables.has_stsd = true; break;
                                case fourcc("stts"): tables.stts = s; tables.has_stts = true; break;
                                case fourcc("ctts"): tables.ctts = s; tables.has_ctts = true; break;
                                case fourcc("stss"): tables.stss = s; tables.has_stss = true; break;
                                case fourcc("stsc"): tables.stsc = s; tables.has_stsc = true; break;
                                case fourcc("stsz"): tables.stsz = s; tables.has_stsz = true; break;
                                case fourcc("stz2"): tables.stz2 = s; tables.has_stz2 = true; break;
                                case fourcc("stco"): tables.stco = s; tables.has_stco = true; break;
                                case fourcc("co64"): tables.co64 = s; tables.has_co64 = true; break;
                                default: break;
                            }
                        }
                    }
                }
            }
        } else if (b.type == fourcc("udta")) {
            Box n;
            if (find_box(b.data, b.size, fourcc("name"), n)) {
                ByteReader nr(n.data, n.size);
                name = nr.str(n.size);
            }
        }
    }
    if (!have_mdhd || t.timescale == 0 || !tables.has_stsd) return;
    if (track_by_id(t.track_id) >= 0) return;
    info.id = t.track_id;
    info.timescale = t.timescale;
    info.language = iso_language(lang);
    info.name = name;
    info.is_default = t.enabled_flag;
    switch (t.handler) {
        case fourcc("vide"): info.type = MediaType::video; break;
        case fourcc("soun"): info.type = MediaType::audio; break;
        case fourcc("sbtl"): case fourcc("text"): case fourcc("subt"): case fourcc("clcp"): info.type = MediaType::subtitle; break;
        default: info.type = MediaType::other; break;
    }
    if (!parse_sample_entry(t, info, tables.stsd)) info.codec.clear();
    build_samples(t, info, tables);
    m_tracks.push_back(std::move(t));
    tracks.push_back(std::move(info));
}

bool Mp4Reader::parse_sample_entry(Mp4Track& t, TrackInfo& info, const Box& stsd) {
    ByteReader r(stsd.data, stsd.size);
    r.skip(4);
    const std::uint32_t count = r.be32();
    if (count == 0 || r.failed()) return false;
    Box entry;
    if (!next_box(r, entry)) return false;
    std::uint32_t format = entry.type;
    ByteReader e(entry.data, entry.size);
    e.skip(8);
    if (e.failed()) return false;
    const std::uint8_t* children = nullptr;
    std::size_t children_size = 0;
    std::string codec;
    bool is_video_entry = info.type == MediaType::video;
    bool is_audio_entry = info.type == MediaType::audio;
    switch (format) {
        case fourcc("avc1"): case fourcc("avc3"): case fourcc("hvc1"): case fourcc("hev1"): case fourcc("av01"): case fourcc("vp09"):
        case fourcc("vp08"): case fourcc("mp4v"): case fourcc("encv"): case fourcc("s263"): case fourcc("h263"): case fourcc("jpeg"):
        case fourcc("mjpa"): case fourcc("mjpb"): case fourcc("apch"): case fourcc("apcn"): case fourcc("apcs"): case fourcc("apco"):
        case fourcc("ap4h"): case fourcc("dvh1"): case fourcc("dvhe"): case fourcc("mp2v"):
            is_video_entry = true;
            is_audio_entry = false;
            break;
        case fourcc("mp4a"): case fourcc("Opus"): case fourcc("fLaC"): case fourcc("alac"): case fourcc(".mp3"): case fourcc("ac-3"):
        case fourcc("ec-3"): case fourcc("sowt"): case fourcc("twos"): case fourcc("raw "): case fourcc("in24"): case fourcc("in32"):
        case fourcc("fl32"): case fourcc("fl64"): case fourcc("lpcm"): case fourcc("ulaw"): case fourcc("alaw"): case fourcc("ipcm"):
        case fourcc("fpcm"): case fourcc("enca"): case fourcc("samr"): case fourcc("sawb"):
            is_audio_entry = true;
            is_video_entry = false;
            break;
        default:
            break;
    }
    std::uint16_t qt_version = 0;
    std::uint32_t lpcm_flags = 0;
    std::uint32_t bytes_per_frame = 0;
    if (is_video_entry) {
        e.skip(16);
        info.width = e.be16();
        info.height = e.be16();
        e.skip(4 + 4 + 4 + 2 + 32);
        info.bits_per_sample = e.be16();
        e.skip(2);
        if (e.failed()) return false;
        children = e.here();
        children_size = e.left();
    } else if (is_audio_entry) {
        qt_version = e.be16();
        e.skip(2 + 4);
        info.channels = e.be16();
        info.bits_per_sample = e.be16();
        e.skip(4);
        info.sample_rate = e.be32() >> 16;
        if (qt_version == 1) {
            e.skip(8);
            bytes_per_frame = e.be32();
            e.skip(4);
        } else if (qt_version == 2) {
            e.skip(4);
            const std::uint64_t bits = e.be64();
            double rate;
            std::memcpy(&rate, &bits, sizeof(rate));
            if (rate > 0.0 && rate < 1e7) info.sample_rate = static_cast<std::uint32_t>(rate + 0.5);
            const std::uint32_t ch = e.be32();
            info.channels = static_cast<std::uint16_t>(std::min<std::uint32_t>(ch, 65535));
            e.skip(4);
            info.bits_per_sample = static_cast<std::uint16_t>(std::min<std::uint32_t>(e.be32(), 64));
            lpcm_flags = e.be32();
            bytes_per_frame = e.be32();
            e.skip(4);
        }
        if (e.failed()) return false;
        if (info.sample_rate == 0) info.sample_rate = t.timescale;
        children = e.here();
        children_size = e.left();
    } else {
        children = e.here();
        children_size = e.left();
    }
    Box child;
    if (format == fourcc("encv") || format == fourcc("enca")) {
        Box frma;
        if (find_box_deep(children, children_size, fourcc("frma"), frma) && frma.size >= 4) format = ByteReader(frma.data, 4).be32();
    }
    switch (format) {
        case fourcc("avc1"): case fourcc("avc3"):
            codec = "h264";
            if (find_box(children, children_size, fourcc("avcC"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("hvc1"): case fourcc("hev1"): case fourcc("dvh1"): case fourcc("dvhe"):
            codec = "hevc";
            if (find_box(children, children_size, fourcc("hvcC"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("av01"):
            codec = "av1";
            if (find_box(children, children_size, fourcc("av1C"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("vp09"): case fourcc("vp08"):
            codec = format == fourcc("vp09") ? "vp9" : "vp8";
            if (find_box(children, children_size, fourcc("vpcC"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("s263"): case fourcc("h263"): codec = "h263"; break;
        case fourcc("jpeg"): case fourcc("mjpa"): case fourcc("mjpb"): codec = "mjpeg"; break;
        case fourcc("apch"): case fourcc("apcn"): case fourcc("apcs"): case fourcc("apco"): case fourcc("ap4h"): codec = "prores"; break;
        case fourcc("mp2v"): codec = "mpeg2video"; break;
        case fourcc("mp4v"): case fourcc("mp4a"): {
            codec = format == fourcc("mp4v") ? "mpeg4" : "aac";
            if (find_box_deep(children, children_size, fourcc("esds"), child)) {
                EsdsInfo es = parse_esds(child);
                if (es.ok) codec = codec_from_oti(es.object_type, codec);
                if (codec != "mp3") info.codec_config = es.config;
                info.bitrate = es.avg_bitrate;
            }
            break;
        }
        case fourcc("Opus"): {
            codec = "opus";
            if (find_box(children, children_size, fourcc("dOps"), child) && child.size >= 11) {
                ByteReader o(child.data, child.size);
                o.skip(1);
                const std::uint8_t channels = o.u8();
                const std::uint16_t pre_skip = o.be16();
                const std::uint32_t rate = o.be32();
                const std::uint16_t gain = o.be16();
                const std::uint8_t family = o.u8();
                std::vector<std::uint8_t>& h = info.codec_config;
                const char magic[] = "OpusHead";
                h.assign(magic, magic + 8);
                h.push_back(1);
                h.push_back(channels);
                h.push_back(static_cast<std::uint8_t>(pre_skip & 0xFF));
                h.push_back(static_cast<std::uint8_t>(pre_skip >> 8));
                for (int i = 0; i < 4; ++i) h.push_back(static_cast<std::uint8_t>((rate >> (8 * i)) & 0xFF));
                h.push_back(static_cast<std::uint8_t>(gain & 0xFF));
                h.push_back(static_cast<std::uint8_t>(gain >> 8));
                h.push_back(family);
                if (family != 0) {
                    const std::size_t extra = std::min<std::size_t>(2u + channels, o.left());
                    h.insert(h.end(), o.here(), o.here() + extra);
                }
                if (channels) info.channels = channels;
            }
            info.sample_rate = 48000;
            break;
        }
        case fourcc("fLaC"):
            codec = "flac";
            if (find_box(children, children_size, fourcc("dfLa"), child) && child.size > 4) {
                const char magic[] = "fLaC";
                info.codec_config.assign(magic, magic + 4);
                info.codec_config.insert(info.codec_config.end(), child.data + 4, child.data + child.size);
            }
            break;
        case fourcc("alac"):
            codec = "alac";
            if (find_box_deep(children, children_size, fourcc("alac"), child)) {
                if (child.size >= 28) info.codec_config.assign(child.data + 4, child.data + 28);
                else if (child.size >= 24) info.codec_config.assign(child.data, child.data + 24);
            }
            if (info.codec_config.size() == 24) {
                info.bits_per_sample = info.codec_config[5];
                info.channels = info.codec_config[9];
                info.sample_rate = ByteReader(info.codec_config.data() + 20, 4).be32();
            }
            break;
        case fourcc(".mp3"): codec = "mp3"; break;
        case fourcc("ac-3"):
            codec = "ac3";
            if (find_box(children, children_size, fourcc("dac3"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("ec-3"):
            codec = "eac3";
            if (find_box(children, children_size, fourcc("dec3"), child)) info.codec_config.assign(child.data, child.data + child.size);
            break;
        case fourcc("samr"): codec = "amr_nb"; break;
        case fourcc("sawb"): codec = "amr_wb"; break;
        case fourcc("ulaw"): codec = "pcm_mulaw"; break;
        case fourcc("alaw"): codec = "pcm_alaw"; break;
        case fourcc("sowt"): codec = pcm_codec(false, true, true, info.bits_per_sample); break;
        case fourcc("twos"): codec = pcm_codec(false, false, true, info.bits_per_sample); break;
        case fourcc("raw "): codec = "pcm_u8"; info.bits_per_sample = 8; break;
        case fourcc("in24"): case fourcc("in32"): case fourcc("fl32"): case fourcc("fl64"): {
            Box enda;
            bool little = false;
            if (find_box_deep(children, children_size, fourcc("enda"), enda) && enda.size >= 2) little = ByteReader(enda.data, 2).be16() != 0;
            const bool fl = format == fourcc("fl32") || format == fourcc("fl64");
            const int bits = format == fourcc("in24") ? 24 : format == fourcc("fl64") ? 64 : 32;
            info.bits_per_sample = static_cast<std::uint16_t>(bits);
            codec = pcm_codec(fl, little, true, bits);
            break;
        }
        case fourcc("lpcm"):
            codec = pcm_codec((lpcm_flags & 1) != 0, (lpcm_flags & 2) == 0, (lpcm_flags & 4) != 0, info.bits_per_sample);
            break;
        case fourcc("ipcm"): case fourcc("fpcm"): {
            bool little = false;
            if (find_box(children, children_size, fourcc("pcmC"), child) && child.size >= 6) {
                little = (child.data[4] & 1) != 0;
                info.bits_per_sample = child.data[5];
            }
            codec = pcm_codec(format == fourcc("fpcm"), little, true, info.bits_per_sample);
            break;
        }
        case fourcc("tx3g"):
            codec = "mov_text";
            info.codec_config.assign(entry.data + 8 < entry.data + entry.size ? entry.data + 8 : entry.data + entry.size, entry.data + entry.size);
            if (info.type == MediaType::other) info.type = MediaType::subtitle;
            break;
        case fourcc("wvtt"): codec = "webvtt"; info.type = MediaType::subtitle; break;
        case fourcc("stpp"): codec = "ttml"; info.type = MediaType::subtitle; break;
        case fourcc("c608"): codec = "eia_608"; info.type = MediaType::subtitle; break;
        default:
            codec = fourcc_string(format);
            break;
    }
    if (is_video_entry && info.type == MediaType::other) info.type = MediaType::video;
    if (is_audio_entry && info.type == MediaType::other) info.type = MediaType::audio;
    if (find_box(children, children_size, fourcc("pasp"), child) && child.size >= 8) {
        ByteReader p(child.data, 8);
        t.pasp_h = p.be32();
        t.pasp_v = p.be32();
    }
    if (find_box(children, children_size, fourcc("btrt"), child) && child.size >= 12) {
        ByteReader p(child.data, 12);
        p.skip(8);
        const std::uint32_t avg = p.be32();
        if (avg && info.bitrate == 0) info.bitrate = avg;
    }
    if (codec == "aac") {
        AacConfig c;
        if (parse_aac_config(info.codec_config.data(), info.codec_config.size(), c)) {
            if (info.sample_rate != c.sample_rate && info.sample_rate != c.extension_rate && info.sample_rate != 2 * c.sample_rate) info.sample_rate = c.sample_rate;
            if (c.channels) info.channels = static_cast<std::uint16_t>(c.channels);
        }
    }
    if (codec.compare(0, 4, "pcm_") == 0) {
        t.pcm = true;
        const std::uint32_t natural = static_cast<std::uint32_t>(info.channels) * ((info.bits_per_sample + 7u) / 8u);
        t.frame_bytes = bytes_per_frame ? bytes_per_frame : natural;
    }
    info.codec = codec;
    return true;
}

void Mp4Reader::build_samples(Mp4Track& t, TrackInfo& info, const TrakTables& tables) {
    const std::uint64_t file = m_src->size();
    std::vector<std::uint64_t> chunks;
    if (tables.has_stco || tables.has_co64) {
        const bool wide = !tables.has_stco;
        const Box& b = wide ? tables.co64 : tables.stco;
        ByteReader r(b.data, b.size);
        r.skip(4);
        std::size_t n = r.be32();
        n = std::min<std::size_t>(n, r.left() / (wide ? 8 : 4));
        chunks.reserve(n);
        for (std::size_t i = 0; i < n; ++i) chunks.push_back(wide ? r.be64() : r.be32());
    }
    std::vector<std::uint32_t> stsc_first, stsc_count;
    if (tables.has_stsc) {
        ByteReader r(tables.stsc.data, tables.stsc.size);
        r.skip(4);
        std::size_t n = r.be32();
        n = std::min<std::size_t>(n, r.left() / 12);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t first = r.be32();
            const std::uint32_t spc = r.be32();
            r.skip(4);
            if (first == 0) continue;
            if (!stsc_first.empty() && first <= stsc_first.back()) continue;
            stsc_first.push_back(first);
            stsc_count.push_back(spc);
        }
    }
    std::uint32_t constant_size = 0;
    std::size_t sample_count = 0;
    std::vector<std::uint32_t> sizes;
    if (tables.has_stsz) {
        ByteReader r(tables.stsz.data, tables.stsz.size);
        r.skip(4);
        constant_size = r.be32();
        sample_count = r.be32();
        if (r.failed()) sample_count = 0;
        if (constant_size == 0) {
            sample_count = std::min<std::size_t>(sample_count, r.left() / 4);
            sample_count = std::min(sample_count, k_max_samples);
            sizes.resize(sample_count);
            for (std::size_t i = 0; i < sample_count; ++i) sizes[i] = r.be32();
        }
    } else if (tables.has_stz2) {
        ByteReader r(tables.stz2.data, tables.stz2.size);
        r.skip(4 + 3);
        const std::uint8_t field = r.u8();
        sample_count = r.be32();
        if (r.failed() || (field != 4 && field != 8 && field != 16)) sample_count = 0;
        sample_count = std::min<std::size_t>(sample_count, field ? r.left() * 8 / field : 0);
        sample_count = std::min(sample_count, k_max_samples);
        sizes.resize(sample_count);
        const std::uint8_t* nibbles = r.here();
        for (std::size_t i = 0; i < sample_count; ++i) {
            if (field == 16) sizes[i] = r.be16();
            else if (field == 8) sizes[i] = r.u8();
            else sizes[i] = (i & 1) ? (nibbles[i / 2] & 15u) : (nibbles[i / 2] >> 4);
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> stts;
    if (tables.has_stts) {
        ByteReader r(tables.stts.data, tables.stts.size);
        r.skip(4);
        std::size_t n = r.be32();
        n = std::min<std::size_t>(n, r.left() / 8);
        stts.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t c = r.be32();
            const std::uint32_t d = r.be32();
            stts.emplace_back(c, d);
        }
    }
    bool all_unit = true;
    for (const auto& e : stts) {
        if (e.first && e.second != 1) all_unit = false;
    }
    if (t.pcm && constant_size && t.frame_bytes && all_unit) {
        const std::size_t limit = static_cast<std::size_t>(std::min<std::uint64_t>(sample_count, file / t.frame_bytes + 1));
        std::size_t done = 0;
        std::size_t s_index = 0;
        std::int64_t dts = 0;
        for (std::size_t c = 0; c < chunks.size() && done < limit; ++c) {
            while (s_index + 1 < stsc_first.size() && c + 1 >= stsc_first[s_index + 1]) ++s_index;
            std::uint64_t spc = stsc_count.empty() ? 1 : stsc_count[s_index];
            spc = std::min<std::uint64_t>(spc, limit - done);
            std::uint64_t offset = chunks[c];
            while (spc > 0 && t.samples.size() < k_max_samples) {
                const std::uint32_t frames = static_cast<std::uint32_t>(std::min<std::uint64_t>(spc, 1024));
                Sample s;
                s.offset = offset;
                s.size = frames * t.frame_bytes;
                s.dts = dts;
                s.duration = frames;
                s.key = true;
                t.samples.push_back(s);
                offset += s.size;
                dts += frames;
                spc -= frames;
                done += frames;
            }
        }
    } else {
        if (constant_size) {
            sample_count = static_cast<std::size_t>(std::min<std::uint64_t>(sample_count, file / constant_size + 1));
            sample_count = std::min(sample_count, k_max_samples);
        }
        t.samples.reserve(std::min<std::size_t>(sample_count, 1u << 20));
        std::size_t s_index = 0;
        std::size_t n = 0;
        for (std::size_t c = 0; c < chunks.size() && n < sample_count; ++c) {
            while (s_index + 1 < stsc_first.size() && c + 1 >= stsc_first[s_index + 1]) ++s_index;
            const std::uint64_t spc = stsc_count.empty() ? 0 : stsc_count[s_index];
            std::uint64_t offset = chunks[c];
            for (std::uint64_t k = 0; k < spc && n < sample_count; ++k) {
                Sample s;
                s.offset = offset;
                s.size = constant_size ? constant_size : sizes[n];
                offset += s.size;
                t.samples.push_back(s);
                ++n;
            }
        }
        std::size_t i = 0;
        std::int64_t dts = 0;
        std::uint32_t last = 1;
        for (const auto& e : stts) {
            for (std::uint32_t k = 0; k < e.first && i < t.samples.size(); ++k) {
                t.samples[i].dts = dts;
                t.samples[i].duration = e.second;
                dts += e.second;
                ++i;
            }
            last = e.second;
            if (i >= t.samples.size()) break;
        }
        for (; i < t.samples.size(); ++i) {
            t.samples[i].dts = dts;
            t.samples[i].duration = last;
            dts += last;
        }
        if (tables.has_ctts) {
            ByteReader r(tables.ctts.data, tables.ctts.size);
            r.skip(4);
            std::size_t cn = r.be32();
            cn = std::min<std::size_t>(cn, r.left() / 8);
            std::size_t j = 0;
            for (std::size_t e = 0; e < cn && j < t.samples.size(); ++e) {
                const std::uint32_t c = r.be32();
                const std::int32_t off = static_cast<std::int32_t>(r.be32());
                for (std::uint32_t k = 0; k < c && j < t.samples.size(); ++k) t.samples[j++].cts = off;
            }
        }
        if (tables.has_stss) {
            ByteReader r(tables.stss.data, tables.stss.size);
            r.skip(4);
            std::size_t sn = r.be32();
            sn = std::min<std::size_t>(sn, r.left() / 4);
            if (sn > 0 && info.type != MediaType::audio) {
                for (Sample& s : t.samples) s.key = false;
                for (std::size_t e = 0; e < sn; ++e) {
                    const std::uint32_t idx = r.be32();
                    if (idx >= 1 && idx <= t.samples.size()) t.samples[idx - 1].key = true;
                }
            }
        }
    }
    for (const Sample& s : t.samples) t.total_bytes += s.size;
}

void Mp4Reader::finish_track(Mp4Track& t, TrackInfo& info) {
    double empty = 0.0;
    double edit_total = 0.0;
    bool found = false;
    for (const Edit& e : t.edits) {
        if (e.media_time == -1) {
            if (!found) empty += static_cast<double>(e.duration) / m_movie_timescale;
            edit_total += static_cast<double>(e.duration) / m_movie_timescale;
            continue;
        }
        if (e.media_time < 0) continue;
        if (!found) {
            found = true;
            t.media_time = e.media_time;
        }
        edit_total += static_cast<double>(e.duration) / m_movie_timescale;
    }
    t.start_offset = empty;
    std::size_t non_empty = 0;
    std::uint64_t edit_duration = 0;
    for (const Edit& e : t.edits) {
        if (e.media_time >= 0) {
            ++non_empty;
            if (non_empty == 1) edit_duration = e.duration;
        }
    }
    if (found && non_empty == 1 && !t.samples.empty()) {
        if (edit_duration > 0) {
            const double scaled = static_cast<double>(edit_duration) * t.timescale / m_movie_timescale;
            const std::int64_t end = t.media_time + static_cast<std::int64_t>(std::min(scaled + 0.5, 9e18));
            bool has_ctts = false;
            for (const Sample& s : t.samples) has_ctts = has_ctts || s.cts != 0;
            bool waited = false;
            for (std::size_t i = 0; i < t.samples.size(); ++i) {
                const Sample& s = t.samples[i];
                const std::int64_t frame = i + 1 < t.samples.size() ? t.samples[i + 1].dts - s.dts : static_cast<std::int64_t>(s.duration);
                const bool is_audio = info.type == MediaType::audio;
                if (s.dts + s.cts + frame >= end && (s.key || is_audio)) {
                    if (!is_audio && has_ctts && !waited) {
                        waited = true;
                        continue;
                    }
                    t.samples.resize(i + 1);
                    break;
                }
            }
        }
        if (info.type != MediaType::audio) {
            bool have = false;
            std::int64_t lowest = 0;
            for (const Sample& s : t.samples) {
                const std::int64_t pts = s.dts + s.cts;
                if (pts >= t.media_time && (!have || pts < lowest)) {
                    lowest = pts;
                    have = true;
                }
            }
            if (have && lowest > t.media_time) t.media_time = lowest;
        }
    }
    for (const Sample& s : t.samples) {
        if (s.cts < 0) t.dts_shift = std::max<std::int64_t>(t.dts_shift, -static_cast<std::int64_t>(s.cts));
    }
    const double ts = static_cast<double>(t.timescale);
    double span = 0.0;
    if (!t.samples.empty()) {
        const Sample& last = t.samples.back();
        span = static_cast<double>(last.dts + last.duration - t.samples.front().dts) / ts;
    }
    if (found && edit_total > 0.0) info.duration = edit_total;
    else if (t.media_duration) info.duration = static_cast<double>(t.media_duration) / ts;
    else info.duration = span;
    info.frame_count = t.pcm ? 0 : t.samples.size();
    if (t.pcm && !t.samples.empty()) info.frame_count = static_cast<std::uint64_t>(t.samples.back().dts + t.samples.back().duration);
    if (info.type == MediaType::video) {
        if (span > 0.0 && !t.samples.empty()) info.frame_rate = static_cast<double>(t.samples.size()) / span;
        double a = t.matrix[0];
        double b = t.matrix[1];
        int rot = 0;
        if (std::fabs(a) > 1e-9 || std::fabs(b) > 1e-9) {
            const double deg = std::atan2(b, a) * 180.0 / 3.14159265358979323846;
            rot = static_cast<int>(std::lround(deg / 90.0)) * 90;
            rot = ((rot % 360) + 360) % 360;
        }
        info.rotation = rot;
        info.display_width = info.width;
        info.display_height = info.height;
        if (t.pasp_h && t.pasp_v && t.pasp_h != t.pasp_v) {
            info.display_width = static_cast<std::uint32_t>(std::lround(static_cast<double>(info.width) * t.pasp_h / t.pasp_v));
        } else if (t.tkhd_width >= 1.0 && t.tkhd_height >= 1.0 && info.width && info.height) {
            const double tw = t.tkhd_width;
            const double th = t.tkhd_height;
            const bool swapped = std::fabs(tw - info.height) < 1.0 && std::fabs(th - info.width) < 1.0 && info.width != info.height;
            if (!swapped && (std::fabs(tw - info.width) >= 1.0 || std::fabs(th - info.height) >= 1.0)) {
                info.display_width = static_cast<std::uint32_t>(std::lround(tw * info.height / th));
            }
        }
    }
    if (info.bitrate == 0 && info.duration > 0.0 && t.total_bytes) {
        info.bitrate = static_cast<std::uint32_t>(std::min(4e9, static_cast<double>(t.total_bytes) * 8.0 / info.duration));
    }
}

void Mp4Reader::parse_udta(const std::uint8_t* data, std::size_t size) {
    ByteReader r(data, size);
    Box b;
    while (next_box(r, b)) {
        if (b.type == fourcc("meta")) {
            parse_meta(b.data, b.size);
        } else if ((b.type >> 24) == 0xA9 && b.size >= 4) {
            ByteReader s(b.data, b.size);
            const std::uint16_t len = s.be16();
            s.skip(2);
            if (len <= s.left()) {
                const std::string value = s.str(len);
                std::string key;
                switch (b.type) {
                    case fourcc("\xA9nam"): key = "title"; break;
                    case fourcc("\xA9" "ART"): key = "artist"; break;
                    case fourcc("\xA9" "alb"): key = "album"; break;
                    case fourcc("\xA9" "day"): key = "date"; break;
                    case fourcc("\xA9" "cmt"): key = "comment"; break;
                    case fourcc("\xA9" "too"): key = "encoder"; break;
                    default: break;
                }
                bool exists = false;
                for (const auto& t : tags) exists = exists || t.first == key;
                if (!key.empty() && !exists) add_tag(tags, key, value);
            }
        }
    }
}

void Mp4Reader::parse_meta(const std::uint8_t* data, std::size_t size) {
    std::size_t skip = 4;
    if (size >= 8) {
        const std::uint32_t t = ByteReader(data + 4, 4).be32();
        if (t == fourcc("hdlr") || t == fourcc("ilst") || t == fourcc("keys")) skip = 0;
    }
    if (size < skip) return;
    Box ilst;
    if (find_box(data + skip, size - skip, fourcc("ilst"), ilst)) parse_ilst(ilst.data, ilst.size);
}

void Mp4Reader::parse_ilst(const std::uint8_t* data, std::size_t size) {
    ByteReader r(data, size);
    Box item;
    while (next_box(r, item)) {
        std::string key;
        switch (item.type) {
            case fourcc("\xA9nam"): key = "title"; break;
            case fourcc("\xA9" "ART"): key = "artist"; break;
            case fourcc("aART"): key = "album_artist"; break;
            case fourcc("\xA9" "alb"): key = "album"; break;
            case fourcc("\xA9" "day"): key = "date"; break;
            case fourcc("\xA9" "gen"): key = "genre"; break;
            case fourcc("\xA9" "cmt"): key = "comment"; break;
            case fourcc("\xA9" "wrt"): key = "composer"; break;
            case fourcc("\xA9" "too"): key = "encoder"; break;
            case fourcc("\xA9" "grp"): key = "grouping"; break;
            case fourcc("\xA9" "lyr"): key = "lyrics"; break;
            case fourcc("desc"): key = "description"; break;
            case fourcc("cprt"): case fourcc("\xA9" "cpy"): key = "copyright"; break;
            case fourcc("trkn"): key = "track"; break;
            case fourcc("disk"): key = "disc"; break;
            case fourcc("----"): key = "-"; break;
            default: break;
        }
        if (key.empty()) continue;
        ByteReader ir(item.data, item.size);
        Box c;
        std::string name;
        while (next_box(ir, c)) {
            if (c.type == fourcc("name") && c.size >= 4) {
                name = ByteReader(c.data + 4, c.size - 4).str(c.size - 4);
            } else if (c.type == fourcc("data") && c.size >= 8) {
                ByteReader d(c.data, c.size);
                const std::uint32_t kind = d.be32() & 0xFFFFFF;
                d.skip(4);
                std::string value;
                if (key == "track" || key == "disc") {
                    if (d.left() >= 6) {
                        d.skip(2);
                        const std::uint16_t n = d.be16();
                        const std::uint16_t total = d.be16();
                        value = std::to_string(n);
                        if (total) value += "/" + std::to_string(total);
                    }
                } else if (kind == 1 || kind == 0) {
                    value = d.str(d.left());
                } else if (kind == 21 && d.left() >= 1 && d.left() <= 8) {
                    std::uint64_t v = d.be_n(d.left());
                    value = std::to_string(v);
                }
                if (value.empty()) continue;
                if (key == "-") {
                    if (!name.empty()) add_tag(tags, to_lower(name), value);
                } else {
                    add_tag(tags, key, value);
                }
            }
        }
    }
}

bool Mp4Reader::parse_moof(std::uint64_t offset, std::vector<std::int64_t>& next_dts, std::vector<QueuedSample>& out) {
    std::uint32_t type = 0;
    std::uint64_t header = 0;
    std::uint64_t size = 0;
    if (!read_top_header(offset, type, header, size) || type != fourcc("moof")) return false;
    std::uint64_t body = size - header;
    const std::uint64_t file = m_src->size();
    if (body > file - offset - header) body = file - offset - header;
    if (body > k_max_moof) return false;
    std::vector<std::uint8_t> buf;
    if (!m_src->read_vector(offset + header, static_cast<std::size_t>(body), buf)) return false;
    ByteReader r(buf.data(), buf.size());
    Box traf;
    std::uint64_t previous_end = offset;
    bool first_traf = true;
    while (next_box(r, traf)) {
        if (traf.type != fourcc("traf")) continue;
        Box tfhd;
        if (!find_box(traf.data, traf.size, fourcc("tfhd"), tfhd)) continue;
        ByteReader h(tfhd.data, tfhd.size);
        h.skip(1);
        const std::uint32_t flags = h.be24();
        const std::uint32_t id = h.be32();
        const int index = track_by_id(id);
        if (index < 0 || h.failed()) continue;
        const std::size_t ti = static_cast<std::size_t>(index);
        Mp4Track& t = m_tracks[ti];
        std::uint64_t base = offset;
        if (flags & 0x1) base = h.be64();
        else if (!(flags & 0x20000) && !first_traf) base = previous_end;
        if (flags & 0x2) h.skip(4);
        std::uint32_t def_duration = t.trex_duration;
        std::uint32_t def_size = t.trex_size;
        std::uint32_t def_flags = t.trex_flags;
        if (flags & 0x8) def_duration = h.be32();
        if (flags & 0x10) def_size = h.be32();
        if (flags & 0x20) def_flags = h.be32();
        if (h.failed()) continue;
        first_traf = false;
        std::int64_t dts = next_dts[ti];
        Box tfdt;
        if (find_box(traf.data, traf.size, fourcc("tfdt"), tfdt)) {
            ByteReader d(tfdt.data, tfdt.size);
            const std::uint8_t version = d.u8();
            d.skip(3);
            const std::uint64_t v = version == 1 ? d.be64() : d.be32();
            if (d.ok() && v < (std::uint64_t(1) << 62)) dts = static_cast<std::int64_t>(v);
        }
        ByteReader tr(traf.data, traf.size);
        Box trun;
        std::uint64_t data_pos = base;
        while (next_box(tr, trun)) {
            if (trun.type != fourcc("trun")) continue;
            ByteReader u(trun.data, trun.size);
            const std::uint8_t version = u.u8();
            const std::uint32_t tf = u.be24();
            std::size_t count = u.be32();
            if (tf & 0x1) {
                const std::int32_t off = static_cast<std::int32_t>(u.be32());
                data_pos = static_cast<std::uint64_t>(static_cast<std::int64_t>(base) + off);
            }
            std::uint32_t first_flags = 0;
            const bool has_first = (tf & 0x4) != 0;
            if (has_first) first_flags = u.be32();
            if (u.failed()) break;
            std::size_t per = 0;
            if (tf & 0x100) per += 4;
            if (tf & 0x200) per += 4;
            if (tf & 0x400) per += 4;
            if (tf & 0x800) per += 4;
            if (per) count = std::min(count, u.left() / per);
            else count = std::min<std::size_t>(count, 1u << 16);
            count = std::min(count, k_max_trun_samples);
            for (std::size_t i = 0; i < count; ++i) {
                Sample s;
                s.duration = (tf & 0x100) ? u.be32() : def_duration;
                s.size = (tf & 0x200) ? u.be32() : def_size;
                std::uint32_t sf = (tf & 0x400) ? u.be32() : def_flags;
                if (i == 0 && has_first) sf = first_flags;
                if (tf & 0x800) {
                    const std::uint32_t c = u.be32();
                    s.cts = version == 0 ? static_cast<std::int32_t>(c) : static_cast<std::int32_t>(c);
                }
                if (u.failed()) break;
                s.key = (sf & 0x10000) == 0;
                if (tracks[ti].type == MediaType::audio) s.key = true;
                s.offset = data_pos;
                s.dts = dts;
                data_pos += s.size;
                dts += s.duration;
                if (s.offset > file) break;
                if (out.size() >= k_max_queue) break;
                out.push_back(QueuedSample{ti, s});
            }
            if (data_pos > file) break;
        }
        previous_end = data_pos;
        next_dts[ti] = dts;
    }
    return true;
}

void Mp4Reader::build_fragment_index() {
    if (m_index_built) return;
    m_index_built = true;
    if (!m_fragmented) return;
    std::vector<std::int64_t> next(m_tracks.size());
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        next[i] = m_tracks[i].samples.empty() ? 0 : m_tracks[i].samples.back().dts + m_tracks[i].samples.back().duration;
    }
    std::uint64_t pos = m_frag_start;
    const std::uint64_t file = m_src->size();
    std::size_t count = 0;
    std::vector<QueuedSample> samples;
    while (pos < file && count++ < k_max_top_boxes) {
        std::uint32_t type = 0;
        std::uint64_t header = 0;
        std::uint64_t size = 0;
        if (!read_top_header(pos, type, header, size)) break;
        if (type == fourcc("moof")) {
            Fragment f;
            f.offset = pos;
            f.next_dts = next;
            samples.clear();
            if (parse_moof(pos, next, samples)) {
                for (const QueuedSample& q : samples) {
                    const Mp4Track& t = m_tracks[q.track];
                    const double pts = t.seconds(q.sample.dts + q.sample.cts);
                    const double end = pts + static_cast<double>(q.sample.duration) / t.timescale;
                    FragSpan* span = nullptr;
                    for (FragSpan& s : f.spans) {
                        if (s.track == q.track) span = &s;
                    }
                    if (!span) {
                        f.spans.emplace_back();
                        span = &f.spans.back();
                        span->track = q.track;
                        span->min_pts = pts;
                        span->max_end = end;
                    }
                    span->min_pts = std::min(span->min_pts, pts);
                    span->max_end = std::max(span->max_end, end);
                    if (q.sample.key && tracks[q.track].type == MediaType::video) span->keys.push_back(pts);
                }
                m_index.push_back(std::move(f));
            }
        }
        if (size > file - pos) break;
        pos += size;
    }
}

bool Mp4Reader::emit(std::size_t index, const Sample& s, Packet& out) {
    const Mp4Track& t = m_tracks[index];
    if (s.size > k_max_packet) return false;
    if (!m_src->read_vector(s.offset, s.size, out.data)) return false;
    out.track = static_cast<std::uint32_t>(index);
    out.pts = t.seconds(s.dts + s.cts);
    out.dts = t.dts_seconds(s.dts);
    out.duration = static_cast<double>(s.duration) / t.timescale;
    out.keyframe = s.key;
    out.position = s.offset;
    return true;
}

bool Mp4Reader::read(Packet& out) {
    for (;;) {
        int best = -1;
        std::uint64_t best_offset = 0;
        for (std::size_t i = 0; i < m_tracks.size(); ++i) {
            Mp4Track& t = m_tracks[i];
            if (!is_enabled(i) || t.cursor >= t.samples.size()) continue;
            const std::uint64_t off = t.samples[t.cursor].offset;
            if (best < 0 || off < best_offset) {
                best = static_cast<int>(i);
                best_offset = off;
            }
        }
        if (best >= 0) {
            Mp4Track& t = m_tracks[static_cast<std::size_t>(best)];
            const Sample s = t.samples[t.cursor++];
            m_last_offset = s.offset;
            if (emit(static_cast<std::size_t>(best), s, out)) return true;
            continue;
        }
        if (!m_fragmented) return false;
        if (m_queue_pos < m_queue.size()) {
            const QueuedSample q = m_queue[m_queue_pos++];
            m_last_offset = q.sample.offset;
            if (!is_enabled(q.track)) continue;
            if (!emit(q.track, q.sample, out)) continue;
            if (!passes_target(m_tracks[q.track].target, out)) continue;
            return true;
        }
        m_queue.clear();
        m_queue_pos = 0;
        const std::uint64_t file = m_src->size();
        bool got = false;
        while (!got && m_scan_pos < file) {
            std::uint32_t type = 0;
            std::uint64_t header = 0;
            std::uint64_t size = 0;
            if (!read_top_header(m_scan_pos, type, header, size)) {
                m_scan_pos = file;
                break;
            }
            if (type == fourcc("moof")) {
                std::vector<std::int64_t> next(m_tracks.size());
                for (std::size_t i = 0; i < m_tracks.size(); ++i) next[i] = m_tracks[i].next_dts;
                if (parse_moof(m_scan_pos, next, m_queue)) {
                    for (std::size_t i = 0; i < m_tracks.size(); ++i) m_tracks[i].next_dts = next[i];
                    std::stable_sort(m_queue.begin(), m_queue.end(), [](const QueuedSample& a, const QueuedSample& b) { return a.sample.offset < b.sample.offset; });
                    got = !m_queue.empty();
                }
            }
            if (size > file - m_scan_pos) m_scan_pos = file;
            else m_scan_pos += size;
        }
        if (!got) return false;
    }
}

bool Mp4Reader::seek_tables(double seconds, bool before) {
    const int ref = reference_track();
    if (ref < 0) return false;
    m_last_offset = 0;
    const Mp4Track& rt = m_tracks[static_cast<std::size_t>(ref)];
    double ref_time = seconds;
    std::size_t ref_index = rt.samples.size();
    if (tracks[static_cast<std::size_t>(ref)].type == MediaType::video) {
        std::size_t best = rt.samples.size();
        std::size_t first_key = rt.samples.size();
        double best_pts = 0.0;
        for (std::size_t i = 0; i < rt.samples.size(); ++i) {
            const Sample& s = rt.samples[i];
            if (!s.key) continue;
            if (first_key == rt.samples.size()) first_key = i;
            const double pts = rt.seconds(s.dts + s.cts);
            if (before) {
                if (pts <= seconds + 1e-9 && (best == rt.samples.size() || pts >= best_pts)) {
                    best = i;
                    best_pts = pts;
                }
            } else if (pts >= seconds - 1e-9) {
                if (best == rt.samples.size() || pts < best_pts) {
                    best = i;
                    best_pts = pts;
                }
            }
        }
        if (best == rt.samples.size() && before) best = first_key;
        ref_index = best;
        if (best < rt.samples.size()) ref_time = rt.seconds(rt.samples[best].dts + rt.samples[best].cts);
    }
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        Mp4Track& t = m_tracks[i];
        t.target.active = false;
        if (static_cast<int>(i) == ref && tracks[i].type == MediaType::video) {
            t.cursor = ref_index;
            continue;
        }
        std::size_t idx = t.samples.size();
        if (tracks[i].type == MediaType::video) {
            for (std::size_t k = 0; k < t.samples.size(); ++k) {
                if (t.samples[k].key && t.seconds(t.samples[k].dts + t.samples[k].cts) <= ref_time + 1e-9) idx = k;
            }
            if (idx == t.samples.size()) idx = 0;
        } else {
            for (std::size_t k = 0; k < t.samples.size(); ++k) {
                const Sample& s = t.samples[k];
                const double pts = t.seconds(s.dts + s.cts);
                if (pts + static_cast<double>(s.duration) / t.timescale > ref_time + 1e-9) {
                    idx = k;
                    break;
                }
            }
        }
        t.cursor = idx;
    }
    m_queue.clear();
    m_queue_pos = 0;
    m_scan_pos = m_frag_start;
    for (Mp4Track& t : m_tracks) t.next_dts = t.samples.empty() ? 0 : t.samples.back().dts + t.samples.back().duration;
    return true;
}

bool Mp4Reader::seek_fragments(double seconds, bool before) {
    build_fragment_index();
    m_last_offset = 0;
    for (Mp4Track& t : m_tracks) {
        t.cursor = t.samples.size();
        t.target.active = false;
    }
    m_queue.clear();
    m_queue_pos = 0;
    if (m_index.empty()) {
        m_scan_pos = m_src->size();
        return false;
    }
    const int ref = reference_track();
    if (ref < 0) return false;
    const std::size_t rt = static_cast<std::size_t>(ref);
    double ref_time = seconds;
    bool ref_video = tracks[rt].type == MediaType::video;
    bool have_key = false;
    double key_pts = 0.0;
    double first_key = 0.0;
    bool have_first = false;
    if (ref_video) {
        for (const Fragment& f : m_index) {
            for (const FragSpan& s : f.spans) {
                if (s.track != rt) continue;
                for (double k : s.keys) {
                    if (!have_first || k < first_key) {
                        first_key = k;
                        have_first = true;
                    }
                    if (before) {
                        if (k <= seconds + 1e-9 && (!have_key || k > key_pts)) {
                            key_pts = k;
                            have_key = true;
                        }
                    } else if (k >= seconds - 1e-9 && (!have_key || k < key_pts)) {
                        key_pts = k;
                        have_key = true;
                    }
                }
            }
        }
        if (!have_key && before && have_first) {
            key_pts = first_key;
            have_key = true;
        }
        if (!have_key) {
            m_scan_pos = m_src->size();
            return true;
        }
        ref_time = key_pts;
    }
    std::size_t start = m_index.size();
    for (std::size_t i = 0; i < m_tracks.size(); ++i) {
        if (!is_enabled(i)) continue;
        std::size_t found = m_index.size();
        std::size_t first_seen = m_index.size();
        for (std::size_t fi = 0; fi < m_index.size(); ++fi) {
            for (const FragSpan& s : m_index[fi].spans) {
                if (s.track != i) continue;
                if (first_seen == m_index.size()) first_seen = fi;
                if (i == rt && ref_video) {
                    for (double k : s.keys) {
                        if (std::fabs(k - key_pts) < 1e-9 && found == m_index.size()) found = fi;
                    }
                } else if (s.min_pts <= ref_time + 1e-9) {
                    found = fi;
                }
            }
        }
        if (found == m_index.size()) found = first_seen;
        if (found < start) start = found;
        Mp4Track& t = m_tracks[i];
        t.target.active = true;
        t.target.want_key = i == rt && ref_video;
        t.target.time = ref_time;
    }
    if (start == m_index.size()) start = 0;
    m_scan_pos = m_index[start].offset;
    for (std::size_t i = 0; i < m_tracks.size(); ++i) m_tracks[i].next_dts = m_index[start].next_dts[i];
    return true;
}

bool Mp4Reader::seek(double seconds, bool before) {
    bool tables = false;
    for (const Mp4Track& t : m_tracks) tables = tables || !t.samples.empty();
    if (!m_fragmented) return seek_tables(seconds, before);
    if (!tables) return seek_fragments(seconds, before);
    build_fragment_index();
    const int ref = reference_track();
    if (ref < 0 || m_index.empty()) return seek_tables(seconds, before);
    const std::size_t rt = static_cast<std::size_t>(ref);
    const Mp4Track& t = m_tracks[rt];
    const bool video = tracks[rt].type == MediaType::video;
    bool have_frag = false;
    double frag_first = 0.0;
    for (const Fragment& f : m_index) {
        for (const FragSpan& sp : f.spans) {
            if (sp.track != rt) continue;
            if (video) {
                for (double k : sp.keys) {
                    if (!have_frag || k < frag_first) frag_first = k;
                    have_frag = true;
                }
            } else {
                if (!have_frag || sp.min_pts < frag_first) frag_first = sp.min_pts;
                have_frag = true;
            }
        }
    }
    if (!have_frag) return seek_tables(seconds, before);
    double last_table_key = -1e300;
    for (const Sample& smp : t.samples) {
        if (!video || smp.key) last_table_key = std::max(last_table_key, t.seconds(smp.dts + smp.cts));
    }
    const bool use_fragments = before ? seconds >= frag_first - 1e-9 : seconds > last_table_key + 1e-9;
    return use_fragments ? seek_fragments(seconds, before) : seek_tables(seconds, before);
}

bool Mp4Reader::rewind() {
    bool tables = false;
    for (const Mp4Track& t : m_tracks) tables = tables || !t.samples.empty();
    if (m_fragmented && !tables) return seek_fragments(-1.0e6, true);
    m_last_offset = 0;
    for (Mp4Track& t : m_tracks) {
        t.cursor = 0;
        t.target.active = false;
        t.next_dts = t.samples.empty() ? 0 : t.samples.back().dts + t.samples.back().duration;
    }
    m_queue.clear();
    m_queue_pos = 0;
    m_scan_pos = m_frag_start;
    return true;
}

} // namespace

std::unique_ptr<Reader> make_mp4_reader() {
    return std::unique_ptr<Reader>(new Mp4Reader());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
