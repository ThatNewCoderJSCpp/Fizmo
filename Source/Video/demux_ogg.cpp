#include "demux_internal.hpp"

#include "../Audio/file_io.hpp"
#include "../Audio/ogg.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

enum class OggKind { vorbis, opus, flac, theora };

const std::size_t k_max_pending = 4096;
const std::size_t k_header_packet_limit = 4096;

struct PendingPacket {
    Packet packet;
    std::int64_t samples = 0;
};

struct OggStream {
    std::uint32_t serial = 0;
    std::size_t track = 0;
    OggKind kind = OggKind::vorbis;
    std::vector<std::vector<std::uint8_t>> headers;
    std::size_t header_target = 0;
    bool headers_done = false;
    bool flac_open_count = false;
    VorbisState vorbis;
    std::uint32_t rate = 0;
    std::int64_t pre_skip = 0;
    int granule_shift = 0;
    std::int64_t next = -1;
    std::vector<PendingPacket> pending;
    SeekTarget target;
};

bool is_header_packet(OggKind kind, const std::vector<std::uint8_t>& d) {
    if (d.empty()) return false;
    switch (kind) {
        case OggKind::vorbis: return (d[0] & 1) != 0;
        case OggKind::opus: return d.size() >= 8 && (std::memcmp(d.data(), "OpusHead", 8) == 0 || std::memcmp(d.data(), "OpusTags", 8) == 0);
        case OggKind::flac: return !(d.size() >= 2 && d[0] == 0xFF && (d[1] & 0xFE) == 0xF8);
        case OggKind::theora: return (d[0] & 0x80) != 0;
    }
    return false;
}

class OggDemux : public Reader {
public:
    bool open(Source& source, std::string* error) override;
    bool read(Packet& out) override;
    bool seek(double seconds, bool before) override;

private:
    OggStream* stream_for(std::uint32_t serial) noexcept;
    bool identify(const audio::OggPacket& p);
    void add_header(OggStream& s, const std::vector<std::uint8_t>& data);
    void finish_headers(OggStream& s);
    void handle_data(OggStream& s, audio::OggPacket& p);
    void resolve(OggStream& s, std::int64_t granule, bool eos);
    void flush_all();
    std::int64_t packet_samples(OggStream& s, const std::vector<std::uint8_t>& d);
    double to_seconds(const OggStream& s, std::int64_t v) const noexcept;
    std::int64_t granule_to_samples(const OggStream& s, std::int64_t g) const noexcept;

    audio::OggReader m_ogg;
    std::vector<OggStream> m_streams;
    std::vector<Packet> m_ready;
    std::size_t m_ready_pos = 0;
    bool m_ended = false;
};

OggStream* OggDemux::stream_for(std::uint32_t serial) noexcept {
    for (OggStream& s : m_streams) {
        if (s.serial == serial) return &s;
    }
    return nullptr;
}

std::int64_t OggDemux::granule_to_samples(const OggStream& s, std::int64_t g) const noexcept {
    if (s.kind == OggKind::theora) {
        const std::int64_t mask = (std::int64_t(1) << s.granule_shift) - 1;
        return (g >> s.granule_shift) + (g & mask);
    }
    return g;
}

double OggDemux::to_seconds(const OggStream& s, std::int64_t v) const noexcept {
    if (s.rate == 0) return 0.0;
    return static_cast<double>(v - s.pre_skip) / static_cast<double>(s.rate);
}

bool OggDemux::identify(const audio::OggPacket& p) {
    if (m_streams.size() >= k_max_tracks) return false;
    const std::vector<std::uint8_t>& d = p.data;
    OggStream s;
    s.serial = p.serial;
    TrackInfo info;
    info.id = p.serial;
    info.is_default = true;
    if (d.size() >= 30 && d[0] == 1 && std::memcmp(d.data() + 1, "vorbis", 6) == 0) {
        s.kind = OggKind::vorbis;
        s.header_target = 3;
        info.type = MediaType::audio;
        info.codec = "vorbis";
        info.channels = d[11];
        info.sample_rate = audio::detail::read_le32(d.data() + 12);
        const std::uint32_t nominal = audio::detail::read_le32(d.data() + 20);
        if (nominal && nominal < 0x80000000u) info.bitrate = nominal;
        s.rate = info.sample_rate;
    } else if (d.size() >= 19 && std::memcmp(d.data(), "OpusHead", 8) == 0) {
        s.kind = OggKind::opus;
        s.header_target = 2;
        info.type = MediaType::audio;
        info.codec = "opus";
        info.channels = d[9];
        info.sample_rate = 48000;
        s.pre_skip = audio::detail::read_le16(d.data() + 10);
        s.rate = 48000;
    } else if (d.size() >= 51 && d[0] == 0x7F && std::memcmp(d.data() + 1, "FLAC", 4) == 0 && std::memcmp(d.data() + 9, "fLaC", 4) == 0) {
        s.kind = OggKind::flac;
        const std::uint16_t count = static_cast<std::uint16_t>((d[7] << 8) | d[8]);
        s.header_target = count ? 1u + count : 0;
        s.flac_open_count = count == 0;
        info.type = MediaType::audio;
        info.codec = "flac";
        const std::uint8_t* si = d.data() + 17;
        info.sample_rate = (static_cast<std::uint32_t>(si[10]) << 12) | (static_cast<std::uint32_t>(si[11]) << 4) | (si[12] >> 4);
        info.channels = static_cast<std::uint16_t>(((si[12] >> 1) & 7) + 1);
        info.bits_per_sample = static_cast<std::uint16_t>((((si[12] & 1) << 4) | (si[13] >> 4)) + 1);
        s.rate = info.sample_rate;
    } else if (d.size() >= 42 && d[0] == 0x80 && std::memcmp(d.data() + 1, "theora", 6) == 0) {
        s.kind = OggKind::theora;
        s.header_target = 3;
        info.type = MediaType::video;
        info.codec = "theora";
        ByteReader r(d.data() + 14, d.size() - 14);
        info.width = r.be24();
        info.height = r.be24();
        r.skip(2);
        const std::uint32_t num = r.be32();
        const std::uint32_t den = r.be32();
        if (num && den) {
            info.frame_rate = static_cast<double>(num) / den;
            s.rate = static_cast<std::uint32_t>(std::max(1.0, info.frame_rate + 0.5));
        }
        info.display_width = info.width;
        info.display_height = info.height;
        s.granule_shift = ((d[40] & 3) << 3) | (d[41] >> 5);
        if (num == 0 || den == 0 || num / den > 1000) return false;
    } else {
        return false;
    }
    if (s.rate == 0) return false;
    info.timescale = s.rate;
    s.track = tracks.size();
    s.headers.push_back(d);
    tracks.push_back(info);
    enabled.push_back(1);
    m_streams.push_back(std::move(s));
    OggStream& added = m_streams.back();
    if (added.header_target == 1) finish_headers(added);
    return true;
}

void OggDemux::add_header(OggStream& s, const std::vector<std::uint8_t>& data) {
    s.headers.push_back(data);
    if (s.kind == OggKind::vorbis && s.headers.size() == 2 && data.size() > 7) {
        parse_vorbis_comments(data.data() + 7, data.size() - 7, tags);
    } else if (s.kind == OggKind::opus && s.headers.size() == 2 && data.size() > 8) {
        parse_vorbis_comments(data.data() + 8, data.size() - 8, tags);
    } else if (s.kind == OggKind::flac && !data.empty() && (data[0] & 0x7F) == 4 && data.size() > 4) {
        parse_vorbis_comments(data.data() + 4, data.size() - 4, tags);
    } else if (s.kind == OggKind::theora && s.headers.size() == 2 && data.size() > 7) {
        parse_vorbis_comments(data.data() + 7, data.size() - 7, tags);
    }
    if (!s.flac_open_count && s.headers.size() >= s.header_target) finish_headers(s);
}

void OggDemux::finish_headers(OggStream& s) {
    s.headers_done = true;
    TrackInfo& info = tracks[s.track];
    switch (s.kind) {
        case OggKind::vorbis:
            info.codec_config = xiph_lace(s.headers);
            if (s.headers.size() >= 3) s.vorbis.init(s.headers[0].data(), s.headers[0].size(), s.headers[2].data(), s.headers[2].size());
            break;
        case OggKind::opus:
            info.codec_config = s.headers[0];
            break;
        case OggKind::flac: {
            const char magic[] = "fLaC";
            info.codec_config.assign(magic, magic + 4);
            for (std::size_t i = 0; i < s.headers.size(); ++i) {
                const std::vector<std::uint8_t>& h = s.headers[i];
                if (i == 0) {
                    if (h.size() > 13) info.codec_config.insert(info.codec_config.end(), h.begin() + 13, h.end());
                } else {
                    info.codec_config.insert(info.codec_config.end(), h.begin(), h.end());
                }
            }
            break;
        }
        case OggKind::theora:
            info.codec_config = xiph_lace(s.headers);
            break;
    }
}

std::int64_t OggDemux::packet_samples(OggStream& s, const std::vector<std::uint8_t>& d) {
    switch (s.kind) {
        case OggKind::vorbis: return s.vorbis.packet_samples(d.data(), d.size());
        case OggKind::opus: return opus_packet_samples(d.data(), d.size());
        case OggKind::flac: return flac_frame_samples(d.data(), d.size());
        case OggKind::theora: return 1;
    }
    return 0;
}

void OggDemux::resolve(OggStream& s, std::int64_t granule, bool eos) {
    if (s.pending.empty()) return;
    std::int64_t t;
    if (s.next >= 0) {
        t = s.next;
    } else if (granule >= 0) {
        std::int64_t sum = 0;
        for (const PendingPacket& p : s.pending) sum += p.samples;
        t = granule_to_samples(s, granule) - sum;
    } else {
        s.pending.clear();
        return;
    }
    const std::int64_t end = granule >= 0 ? granule_to_samples(s, granule) : -1;
    for (std::size_t i = 0; i < s.pending.size(); ++i) {
        PendingPacket& pp = s.pending[i];
        std::int64_t samples = pp.samples;
        if (eos && end >= 0 && i + 1 == s.pending.size() && t + samples > end) samples = std::max<std::int64_t>(0, end - t);
        Packet& p = pp.packet;
        p.pts = to_seconds(s, t);
        p.dts = p.pts;
        p.duration = static_cast<double>(samples) / s.rate;
        t += pp.samples;
        if (!passes_target(s.target, p)) continue;
        m_ready.push_back(std::move(p));
    }
    s.pending.clear();
    s.next = (end >= 0 && !eos) ? end : t;
}

void OggDemux::handle_data(OggStream& s, audio::OggPacket& p) {
    if (p.after_gap && s.kind == OggKind::vorbis && s.next < 0) s.vorbis.reset();
    PendingPacket pp;
    pp.samples = packet_samples(s, p.data);
    pp.packet.track = static_cast<std::uint32_t>(s.track);
    pp.packet.position = p.page_offset;
    pp.packet.keyframe = s.kind != OggKind::theora || p.data.empty() || (p.data[0] & 0x40) == 0;
    pp.packet.data = std::move(p.data);
    if (is_enabled(s.track)) {
        if (s.pending.size() >= k_max_pending) s.pending.clear();
        s.pending.push_back(std::move(pp));
    } else if (s.next >= 0) {
        s.next += pp.samples;
    }
    if (p.granule >= 0) {
        if (is_enabled(s.track)) resolve(s, p.granule, p.eos);
        else s.next = p.eos ? -1 : granule_to_samples(s, p.granule);
    }
}

void OggDemux::flush_all() {
    for (OggStream& s : m_streams) {
        if (s.next >= 0) resolve(s, -1, false);
        s.pending.clear();
    }
}

bool OggDemux::open(Source& source, std::string* error) {
    format = "ogg";
    bool ok;
    if (source.memory()) ok = m_ogg.open(source.memory(), static_cast<std::size_t>(source.size()), error);
    else ok = m_ogg.open(source.path(), error);
    if (!ok) return false;
    audio::OggPacket p;
    std::size_t count = 0;
    while (count++ < k_header_packet_limit && m_ogg.read_packet(p)) {
        OggStream* s = stream_for(p.serial);
        if (!s) {
            if (p.bos) identify(p);
            continue;
        }
        if (!s->headers_done) {
            if (is_header_packet(s->kind, p.data) && s->headers.size() < 64) {
                add_header(*s, p.data);
                continue;
            }
            finish_headers(*s);
        }
        if (is_header_packet(s->kind, p.data)) continue;
        handle_data(*s, p);
        bool all = true;
        for (const OggStream& st : m_streams) all = all && st.headers_done;
        if (all) break;
    }
    for (OggStream& s : m_streams) {
        if (!s.headers_done) finish_headers(s);
    }
    if (m_streams.empty()) {
        if (error) *error = "no supported streams in Ogg file";
        return false;
    }
    for (OggStream& s : m_streams) {
        audio::OggPage page;
        if (m_ogg.last_granule_page(s.serial, 0, m_ogg.size(), page)) {
            const double d = to_seconds(s, granule_to_samples(s, page.granule));
            if (d > 0.0) {
                tracks[s.track].duration = d;
                duration = std::max(duration, d);
            }
        }
        if (s.kind == OggKind::vorbis) s.vorbis.reset();
    }
    return true;
}

bool OggDemux::read(Packet& out) {
    while (m_ready_pos >= m_ready.size()) {
        m_ready.clear();
        m_ready_pos = 0;
        if (m_ended) return false;
        audio::OggPacket p;
        if (!m_ogg.read_packet(p)) {
            flush_all();
            m_ended = true;
            continue;
        }
        OggStream* s = stream_for(p.serial);
        if (!s) continue;
        if (is_header_packet(s->kind, p.data)) continue;
        handle_data(*s, p);
    }
    out = std::move(m_ready[m_ready_pos++]);
    return true;
}

bool OggDemux::seek(double seconds, bool before) {
    (void)before;
    int ref = reference_track();
    OggStream* rs = nullptr;
    for (OggStream& s : m_streams) {
        if (static_cast<int>(s.track) == ref) rs = &s;
    }
    if (rs && rs->kind == OggKind::theora) {
        for (OggStream& s : m_streams) {
            if (s.kind != OggKind::theora && is_enabled(s.track)) rs = &s;
        }
    }
    m_ready.clear();
    m_ready_pos = 0;
    m_ended = false;
    for (OggStream& s : m_streams) {
        s.pending.clear();
        s.next = -1;
        s.vorbis.reset();
        s.target.active = true;
        s.target.want_key = false;
        s.target.time = seconds;
    }
    std::uint64_t offset = 0;
    if (rs && rs->kind != OggKind::theora) {
        const std::int64_t target = static_cast<std::int64_t>(seconds * rs->rate) + rs->pre_skip;
        audio::OggPage page;
        if (m_ogg.find_granule(rs->serial, target, 0, m_ogg.size(), page)) offset = page.offset;
    }
    return m_ogg.seek(offset);
}

} // namespace

std::unique_ptr<Reader> make_ogg_reader() {
    return std::unique_ptr<Reader>(new OggDemux());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
