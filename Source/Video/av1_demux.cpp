#include "demux_internal.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

enum class Av1EsKind { none, ivf, obu, annexb };

bool leb128_at(const std::uint8_t* p, std::size_t size, std::uint64_t* value, std::size_t* len) {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        if (i >= size) return false;
        v |= static_cast<std::uint64_t>(p[i] & 0x7f) << (i * 7);
        if (!(p[i] & 0x80)) {
            *value = v;
            *len = i + 1;
            return true;
        }
    }
    return false;
}

bool obu_span(const std::uint8_t* p, std::size_t size, int* type, std::size_t* header, std::uint64_t* payload) {
    if (size < 1 || (p[0] & 0x80)) return false;
    *type = (p[0] >> 3) & 15;
    const bool ext = (p[0] >> 2) & 1;
    const bool has_size = (p[0] >> 1) & 1;
    std::size_t hdr = ext ? 2 : 1;
    if (hdr > size) return false;
    if (!has_size) {
        *header = hdr;
        *payload = size - hdr;
        return true;
    }
    std::uint64_t v;
    std::size_t len;
    if (!leb128_at(p + hdr, size - hdr, &v, &len)) return false;
    *header = hdr + len;
    *payload = v;
    return true;
}

Av1EsKind classify(const std::uint8_t* p, std::size_t size) {
    if (!p || size < 2) return Av1EsKind::none;
    if (size >= 32 && std::memcmp(p, "DKIF", 4) == 0) {
        if (std::memcmp(p + 8, "AV01", 4) == 0 || std::memcmp(p + 8, "av01", 4) == 0) return Av1EsKind::ivf;
        return Av1EsKind::none;
    }
    int type;
    std::size_t hdr;
    std::uint64_t payload;
    if (((p[0] >> 1) & 1) && obu_span(p, size, &type, &hdr, &payload)) {
        if (type == 2 && payload == 0) return Av1EsKind::obu;
        if (type == 1 && payload > 2 && payload < 4096 && hdr + payload <= size && ((p[0] >> 2) & 1) == 0) return Av1EsKind::obu;
    }
    std::uint64_t tu;
    std::size_t len;
    if (!leb128_at(p, size, &tu, &len) || tu == 0 || tu > (std::uint64_t(1) << 28)) return Av1EsKind::none;
    std::size_t pos = len;
    std::uint64_t fu;
    if (!leb128_at(p + pos, size - pos, &fu, &len) || fu == 0 || fu + len > tu) return Av1EsKind::none;
    pos += len;
    std::uint64_t ol;
    if (!leb128_at(p + pos, size - pos, &ol, &len) || ol == 0 || ol + len > fu) return Av1EsKind::none;
    pos += len;
    if (pos >= size || (p[pos] & 0x80)) return Av1EsKind::none;
    const int t = (p[pos] >> 3) & 15;
    if (t != 2 && t != 1) return Av1EsKind::none;
    if (t == 2 && ol != static_cast<std::uint64_t>(1 + ((p[pos] >> 2) & 1))) return Av1EsKind::none;
    return Av1EsKind::annexb;
}

class SeqBits {
public:
    SeqBits(const std::uint8_t* data, std::size_t size) : m_data(data), m_bits(size * 8) {}
    std::uint32_t f(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            std::uint32_t bit = 0;
            if (m_pos < m_bits) {
                bit = (m_data[m_pos >> 3] >> (7 - (m_pos & 7))) & 1u;
            } else {
                m_fail = true;
            }
            ++m_pos;
            v = (v << 1) | bit;
        }
        return v;
    }
    std::uint32_t uvlc() {
        int zeros = 0;
        while (!m_fail && f(1) == 0) {
            if (++zeros >= 32) return 0xFFFFFFFFu;
        }
        if (zeros == 0) return 0;
        return f(zeros) + ((1u << zeros) - 1u);
    }
    bool failed() const { return m_fail; }

private:
    const std::uint8_t* m_data;
    std::size_t m_bits;
    std::size_t m_pos = 0;
    bool m_fail = false;
};

struct SeqSummary {
    bool valid = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double fps = 0.0;
    int bit_depth = 8;
};

SeqSummary parse_seq(const std::uint8_t* p, std::size_t size) {
    SeqSummary s;
    SeqBits b(p, size);
    const int profile = static_cast<int>(b.f(3));
    b.f(1);
    const int reduced = static_cast<int>(b.f(1));
    int decoder_model_info_present = 0;
    int buffer_delay_length = 0;
    if (reduced) {
        b.f(5);
    } else {
        const int timing = static_cast<int>(b.f(1));
        if (timing) {
            const std::uint32_t units = b.f(32);
            const std::uint32_t scale = b.f(32);
            const int equal = static_cast<int>(b.f(1));
            std::uint32_t ticks = 0;
            if (equal) ticks = b.uvlc();
            if (units && scale) s.fps = static_cast<double>(scale) / (static_cast<double>(units) * (static_cast<double>(ticks) + 1.0));
            decoder_model_info_present = static_cast<int>(b.f(1));
            if (decoder_model_info_present) {
                buffer_delay_length = static_cast<int>(b.f(5)) + 1;
                b.f(32);
                b.f(5);
                b.f(5);
            }
        }
        const int initial_display_delay_present = static_cast<int>(b.f(1));
        const int ops = static_cast<int>(b.f(5)) + 1;
        for (int i = 0; i < ops; ++i) {
            b.f(12);
            const int level = static_cast<int>(b.f(5));
            if (level > 7) b.f(1);
            if (decoder_model_info_present) {
                if (b.f(1)) {
                    b.f(buffer_delay_length);
                    b.f(buffer_delay_length);
                    b.f(1);
                }
            }
            if (initial_display_delay_present) {
                if (b.f(1)) b.f(4);
            }
        }
    }
    const int wbits = static_cast<int>(b.f(4)) + 1;
    const int hbits = static_cast<int>(b.f(4)) + 1;
    s.width = b.f(wbits) + 1;
    s.height = b.f(hbits) + 1;
    if (!reduced && b.f(1)) {
        b.f(4);
        b.f(3);
    }
    b.f(3);
    if (!reduced) {
        b.f(4);
        const int order_hint = static_cast<int>(b.f(1));
        if (order_hint) b.f(2);
        const int choose_sct = static_cast<int>(b.f(1));
        int force_sct = 2;
        if (!choose_sct) force_sct = static_cast<int>(b.f(1));
        if (force_sct > 0) {
            if (!b.f(1)) b.f(1);
        }
        if (order_hint) b.f(3);
    }
    b.f(3);
    const int high = static_cast<int>(b.f(1));
    if (profile == 2 && high) {
        s.bit_depth = b.f(1) ? 12 : 10;
    } else {
        s.bit_depth = high ? 10 : 8;
    }
    s.valid = !b.failed() && s.width > 0 && s.height > 0 && profile <= 2;
    return s;
}

class Av1EsDemux : public Reader {
public:
    bool open(Source& source, std::string* error) override;
    bool read(Packet& out) override;
    bool seek(double seconds, bool before) override;

private:
    struct TuEntry {
        std::uint64_t offset;
        std::uint64_t size;
        double pts;
        bool key;
    };

    bool scan_obus(const std::vector<std::uint8_t>& head, bool* key, SeqSummary* seq) const;
    bool convert_annexb(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) const;

    Source* m_src = nullptr;
    Av1EsKind m_kind = Av1EsKind::none;
    std::vector<TuEntry> m_tus;
    std::size_t m_next = 0;
    double m_dur = 1.0 / 25.0;
};

bool Av1EsDemux::scan_obus(const std::vector<std::uint8_t>& tu, bool* key, SeqSummary* seq) const {
    std::size_t pos = 0;
    while (pos < tu.size()) {
        int type;
        std::size_t hdr;
        std::uint64_t payload;
        if (!obu_span(tu.data() + pos, tu.size() - pos, &type, &hdr, &payload)) return false;
        if (type == 1) {
            *key = true;
            if (seq && !seq->valid && pos + hdr < tu.size()) *seq = parse_seq(tu.data() + pos + hdr, std::min<std::size_t>(tu.size() - pos - hdr, static_cast<std::size_t>(payload)));
        }
        if (payload > tu.size() - pos - hdr) break;
        pos += hdr + static_cast<std::size_t>(payload);
    }
    return true;
}

bool Av1EsDemux::convert_annexb(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) const {
    out.clear();
    std::size_t pos = 0;
    std::uint64_t tuSize;
    std::size_t len;
    if (!leb128_at(in.data(), in.size(), &tuSize, &len)) return false;
    pos = len;
    const std::size_t tuEnd = std::min<std::size_t>(in.size(), pos + static_cast<std::size_t>(std::min<std::uint64_t>(tuSize, in.size())));
    while (pos < tuEnd) {
        std::uint64_t fu;
        if (!leb128_at(in.data() + pos, tuEnd - pos, &fu, &len)) break;
        pos += len;
        const std::size_t fuEnd = std::min<std::size_t>(tuEnd, pos + static_cast<std::size_t>(std::min<std::uint64_t>(fu, tuEnd)));
        while (pos < fuEnd) {
            std::uint64_t ol;
            if (!leb128_at(in.data() + pos, fuEnd - pos, &ol, &len)) {
                pos = fuEnd;
                break;
            }
            pos += len;
            const std::size_t olen = static_cast<std::size_t>(std::min<std::uint64_t>(ol, fuEnd - pos));
            const std::uint8_t* o = in.data() + pos;
            if (olen >= 1) {
                const bool ext = (o[0] >> 2) & 1;
                const bool hasSize = (o[0] >> 1) & 1;
                const std::size_t hdr = ext ? 2 : 1;
                if (hasSize || olen < hdr) {
                    out.insert(out.end(), o, o + olen);
                } else {
                    out.push_back(static_cast<std::uint8_t>(o[0] | 2));
                    if (ext) out.push_back(o[1]);
                    std::uint64_t v = olen - hdr;
                    do {
                        std::uint8_t byte = static_cast<std::uint8_t>(v & 0x7f);
                        v >>= 7;
                        if (v) byte |= 0x80;
                        out.push_back(byte);
                    } while (v);
                    out.insert(out.end(), o + hdr, o + olen);
                }
            }
            pos += olen;
        }
        pos = fuEnd;
    }
    return true;
}

bool Av1EsDemux::open(Source& source, std::string* error) {
    m_src = &source;
    std::uint8_t head[64];
    const std::size_t got = source.read_at(0, head, sizeof(head));
    m_kind = classify(head, got);
    if (m_kind == Av1EsKind::none) {
        if (error) *error = "not an AV1 stream";
        return false;
    }
    const std::uint64_t total = source.size();
    SeqSummary seq;
    std::uint32_t ivfW = 0;
    std::uint32_t ivfH = 0;
    double ivfTimebase = 0.0;
    std::vector<std::uint8_t> buf;
    if (m_kind == Av1EsKind::ivf) {
        const std::size_t hdr = static_cast<std::size_t>(head[6] | (head[7] << 8));
        ivfW = static_cast<std::uint32_t>(head[12] | (head[13] << 8));
        ivfH = static_cast<std::uint32_t>(head[14] | (head[15] << 8));
        const std::uint32_t rate = static_cast<std::uint32_t>(head[16] | (head[17] << 8) | (head[18] << 16)) | (static_cast<std::uint32_t>(head[19]) << 24);
        const std::uint32_t scale = static_cast<std::uint32_t>(head[20] | (head[21] << 8) | (head[22] << 16)) | (static_cast<std::uint32_t>(head[23]) << 24);
        if (rate && scale) ivfTimebase = static_cast<double>(scale) / rate;
        std::uint64_t pos = hdr < 32 ? 32 : hdr;
        while (pos + 12 <= total) {
            std::uint8_t fh[12];
            if (!source.read_exact(pos, fh, 12)) break;
            const std::uint64_t len = static_cast<std::uint64_t>(fh[0] | (fh[1] << 8) | (fh[2] << 16)) | (static_cast<std::uint64_t>(fh[3]) << 24);
            std::uint64_t ts = 0;
            for (int i = 0; i < 8; ++i) ts |= static_cast<std::uint64_t>(fh[4 + i]) << (8 * i);
            pos += 12;
            if (len == 0 || len > total - pos || len > k_max_packet) break;
            const std::size_t peek = static_cast<std::size_t>(std::min<std::uint64_t>(len, 4096));
            bool key = false;
            if (source.read_vector(pos, peek, buf)) scan_obus(buf, &key, &seq);
            TuEntry e;
            e.offset = pos;
            e.size = len;
            e.pts = ivfTimebase > 0.0 ? static_cast<double>(ts) * ivfTimebase : -1.0;
            e.key = key;
            m_tus.push_back(e);
            pos += len;
        }
    } else if (m_kind == Av1EsKind::obu) {
        std::uint64_t pos = 0;
        std::uint64_t tuStart = 0;
        bool key = false;
        bool started = false;
        while (pos < total) {
            std::uint8_t oh[16];
            const std::size_t n = source.read_at(pos, oh, sizeof(oh));
            int type;
            std::size_t hdr;
            std::uint64_t payload;
            if (n == 0 || !obu_span(oh, n, &type, &hdr, &payload) || !((oh[0] >> 1) & 1)) break;
            if (payload > total - pos - hdr) break;
            if (type == 2 && started) {
                TuEntry e;
                e.offset = tuStart;
                e.size = pos - tuStart;
                e.pts = -1.0;
                e.key = key;
                m_tus.push_back(e);
                tuStart = pos;
                key = false;
            }
            started = true;
            if (type == 1) {
                key = true;
                if (!seq.valid && payload < 4096 && source.read_vector(pos + hdr, static_cast<std::size_t>(payload), buf)) seq = parse_seq(buf.data(), buf.size());
            }
            pos += hdr + payload;
        }
        if (started && pos > tuStart) {
            TuEntry e;
            e.offset = tuStart;
            e.size = pos - tuStart;
            e.pts = -1.0;
            e.key = key;
            m_tus.push_back(e);
        }
    } else {
        std::uint64_t pos = 0;
        while (pos < total) {
            std::uint8_t lb[8];
            const std::size_t n = source.read_at(pos, lb, sizeof(lb));
            std::uint64_t tu;
            std::size_t len;
            if (!leb128_at(lb, n, &tu, &len) || tu == 0 || tu > total - pos - len || tu > k_max_packet) break;
            TuEntry e;
            e.offset = pos;
            e.size = tu + len;
            e.pts = -1.0;
            e.key = false;
            if (source.read_vector(pos, static_cast<std::size_t>(std::min<std::uint64_t>(e.size, 8192)), buf)) {
                std::vector<std::uint8_t> conv;
                convert_annexb(buf, conv);
                scan_obus(conv, &e.key, &seq);
            }
            m_tus.push_back(e);
            pos += e.size;
        }
    }
    if (m_tus.empty()) {
        if (error) *error = "empty AV1 stream";
        return false;
    }
    double fps = seq.fps > 0.5 && seq.fps < 1000.0 ? seq.fps : 25.0;
    if (m_kind == Av1EsKind::ivf && m_tus.size() > 1 && m_tus[0].pts >= 0.0) {
        const double span = m_tus.back().pts - m_tus.front().pts;
        if (span > 0.0) fps = static_cast<double>(m_tus.size() - 1) / span;
    }
    m_dur = 1.0 / fps;
    for (std::size_t i = 0; i < m_tus.size(); ++i) {
        if (m_tus[i].pts < 0.0) m_tus[i].pts = static_cast<double>(i) * m_dur;
    }
    TrackInfo t;
    t.id = 0;
    t.type = MediaType::video;
    t.codec = "av1";
    t.width = seq.valid ? seq.width : ivfW;
    t.height = seq.valid ? seq.height : ivfH;
    t.display_width = t.width;
    t.display_height = t.height;
    t.frame_rate = fps;
    t.timescale = 90000.0;
    t.frame_count = m_tus.size();
    t.duration = (m_tus.back().pts - m_tus.front().pts) + m_dur;
    t.bits_per_sample = static_cast<std::uint16_t>(seq.valid ? seq.bit_depth : 8);
    t.is_default = true;
    tracks.push_back(t);
    enabled.assign(1, 1);
    duration = t.duration;
    format = m_kind == Av1EsKind::ivf ? "ivf" : (m_kind == Av1EsKind::annexb ? "av1 annexb" : "obu");
    m_next = 0;
    return true;
}

bool Av1EsDemux::read(Packet& out) {
    while (m_next < m_tus.size()) {
        const TuEntry& e = m_tus[m_next];
        const std::size_t idx = m_next++;
        if (!is_enabled(0)) continue;
        out.track = 0;
        if (m_kind == Av1EsKind::annexb) {
            std::vector<std::uint8_t> raw;
            if (!m_src->read_vector(e.offset, static_cast<std::size_t>(e.size), raw)) return false;
            convert_annexb(raw, out.data);
        } else {
            if (!m_src->read_vector(e.offset, static_cast<std::size_t>(e.size), out.data)) return false;
        }
        out.pts = e.pts;
        out.dts = e.pts;
        out.duration = m_dur;
        out.keyframe = e.key;
        out.position = e.offset;
        out.discard_padding = 0.0;
        (void)idx;
        return true;
    }
    return false;
}

bool Av1EsDemux::seek(double seconds, bool before) {
    if (m_tus.empty()) return false;
    std::size_t best = 0;
    bool found = false;
    for (std::size_t i = 0; i < m_tus.size(); ++i) {
        if (!m_tus[i].key) continue;
        const double t = m_tus[i].pts;
        if (before) {
            if (t <= seconds + 1e-6 || !found) {
                best = i;
                found = true;
            }
            if (t > seconds + 1e-6) break;
        } else if (t >= seconds - 1e-6) {
            best = i;
            found = true;
            break;
        }
    }
    if (!found) {
        if (before) {
            best = 0;
        } else {
            m_next = m_tus.size();
            return true;
        }
    }
    m_next = best;
    return true;
}

} // namespace

bool is_av1_elementary(const void* data, std::size_t size) noexcept {
    return classify(static_cast<const std::uint8_t*>(data), size) != Av1EsKind::none;
}

std::unique_ptr<Reader> make_av1_es_reader() {
    return std::unique_ptr<Reader>(new Av1EsDemux());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
