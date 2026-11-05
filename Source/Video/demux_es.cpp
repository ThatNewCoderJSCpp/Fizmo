#include "demux_internal.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

class EsBits {
public:
    EsBits(const std::uint8_t* data, std::size_t size) : m_data(data), m_bits(size * 8) {}

    std::uint32_t u(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            if (m_pos >= m_bits) {
                m_fail = true;
                return v << (n - i);
            }
            v = (v << 1) | ((m_data[m_pos >> 3] >> (7 - (m_pos & 7))) & 1u);
            ++m_pos;
        }
        return v;
    }

    std::uint32_t ue() {
        int zeros = 0;
        while (!m_fail && u(1) == 0) {
            if (++zeros > 31) {
                m_fail = true;
                return 0;
            }
        }
        if (zeros == 0) return 0;
        return ((1u << zeros) - 1) + u(zeros);
    }

    std::int32_t se() {
        const std::uint32_t k = ue();
        return (k & 1) ? static_cast<std::int32_t>((k + 1) / 2) : -static_cast<std::int32_t>(k / 2);
    }

    void skip(std::size_t n) {
        m_pos += n;
        if (m_pos > m_bits) m_fail = true;
    }

    bool failed() const { return m_fail; }

private:
    const std::uint8_t* m_data;
    std::size_t m_bits;
    std::size_t m_pos = 0;
    bool m_fail = false;
};

struct EsRps {
    int neg = 0;
    int pos = 0;
    int delta[32] = {};
};

struct EsSps {
    bool valid = false;
    int chroma = 1;
    bool separate = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int log2_poc = 4;
    std::uint32_t tick = 0;
    std::uint32_t scale = 0;
    bool field_seq = false;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
};

struct EsPps {
    bool valid = false;
    int sps = 0;
    int extra_bits = 0;
    bool output_flag = false;
};

void unescape(const std::uint8_t* p, std::size_t n, std::vector<std::uint8_t>& out) {
    out.clear();
    int zeros = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (zeros >= 2 && p[i] == 3) {
            zeros = 0;
            continue;
        }
        out.push_back(p[i]);
        zeros = p[i] == 0 ? zeros + 1 : 0;
    }
}

void skip_ptl(EsBits& b, int max_sub) {
    b.skip(88);
    b.skip(8);
    bool prof[8] = {};
    bool lev[8] = {};
    for (int i = 0; i < max_sub; ++i) {
        prof[i] = b.u(1) != 0;
        lev[i] = b.u(1) != 0;
    }
    if (max_sub > 0) {
        for (int i = max_sub; i < 8; ++i) b.skip(2);
    }
    for (int i = 0; i < max_sub; ++i) {
        if (prof[i]) b.skip(88);
        if (lev[i]) b.skip(8);
    }
}

bool skip_scaling_list(EsBits& b) {
    for (int size_id = 0; size_id < 4; ++size_id) {
        for (int m = 0; m < 6; m += (size_id == 3) ? 3 : 1) {
            if (!b.u(1)) {
                b.ue();
            } else {
                const int n = std::min(64, 1 << (4 + (size_id << 1)));
                if (size_id > 1) b.se();
                for (int i = 0; i < n; ++i) b.se();
            }
            if (b.failed()) return false;
        }
    }
    return true;
}

bool parse_rps(EsBits& b, int idx, int count, std::vector<EsRps>& sets, EsRps& out) {
    bool inter = false;
    if (idx != 0) inter = b.u(1) != 0;
    out = EsRps();
    if (inter) {
        std::uint32_t d = 1;
        if (idx == count) d = b.ue() + 1;
        if (d > static_cast<std::uint32_t>(idx)) return false;
        const EsRps& ref = sets[static_cast<std::size_t>(idx) - d];
        const int sign = static_cast<int>(b.u(1));
        const std::uint32_t abs_m1 = b.ue();
        if (abs_m1 > 32767) return false;
        const int drps = (1 - 2 * sign) * (static_cast<int>(abs_m1) + 1);
        const int n = ref.neg + ref.pos;
        bool use[33];
        for (int j = 0; j <= n; ++j) {
            const bool u = b.u(1) != 0;
            use[j] = true;
            if (!u) use[j] = b.u(1) != 0;
        }
        int neg = 0;
        int pos = 0;
        int tmp_neg[40];
        int tmp_pos[40];
        for (int j = ref.pos - 1; j >= 0; --j) {
            const int v = ref.delta[ref.neg + j] + drps;
            if (v < 0 && use[ref.neg + j] && neg < 40) tmp_neg[neg++] = v;
        }
        if (drps < 0 && use[n] && neg < 40) tmp_neg[neg++] = drps;
        for (int j = 0; j < ref.neg; ++j) {
            const int v = ref.delta[j] + drps;
            if (v < 0 && use[j] && neg < 40) tmp_neg[neg++] = v;
        }
        for (int j = ref.neg - 1; j >= 0; --j) {
            const int v = ref.delta[j] + drps;
            if (v > 0 && use[j] && pos < 40) tmp_pos[pos++] = v;
        }
        if (drps > 0 && use[n] && pos < 40) tmp_pos[pos++] = drps;
        for (int j = 0; j < ref.pos; ++j) {
            const int v = ref.delta[ref.neg + j] + drps;
            if (v > 0 && use[ref.neg + j] && pos < 40) tmp_pos[pos++] = v;
        }
        if (neg + pos > 16) return false;
        out.neg = neg;
        out.pos = pos;
        for (int i = 0; i < neg; ++i) out.delta[i] = tmp_neg[i];
        for (int i = 0; i < pos; ++i) out.delta[neg + i] = tmp_pos[i];
    } else {
        const std::uint32_t neg = b.ue();
        const std::uint32_t pos = b.ue();
        if (neg > 16 || pos > 16 || neg + pos > 16) return false;
        int prev = 0;
        for (std::uint32_t i = 0; i < neg; ++i) {
            prev -= static_cast<int>(b.ue()) + 1;
            out.delta[i] = prev;
            b.skip(1);
        }
        prev = 0;
        for (std::uint32_t i = 0; i < pos; ++i) {
            prev += static_cast<int>(b.ue()) + 1;
            out.delta[neg + i] = prev;
            b.skip(1);
        }
        out.neg = static_cast<int>(neg);
        out.pos = static_cast<int>(pos);
    }
    return !b.failed();
}

void skip_hrd(EsBits& b, int max_sub) {
    bool nal = b.u(1) != 0;
    bool vcl = b.u(1) != 0;
    bool sub_pic = false;
    if (nal || vcl) {
        sub_pic = b.u(1) != 0;
        if (sub_pic) b.skip(19);
        b.skip(8);
        if (sub_pic) b.skip(4);
        b.skip(15);
    }
    for (int i = 0; i <= max_sub && !b.failed(); ++i) {
        const bool fixed_general = b.u(1) != 0;
        bool fixed_cvs = true;
        if (!fixed_general) fixed_cvs = b.u(1) != 0;
        bool low_delay = false;
        if (fixed_cvs) {
            b.ue();
        } else {
            low_delay = b.u(1) != 0;
        }
        std::uint32_t cnt = 1;
        if (!low_delay) cnt = b.ue() + 1;
        if (cnt > 32) return;
        for (int k = 0; k < (nal ? 1 : 0) + (vcl ? 1 : 0); ++k) {
            for (std::uint32_t c = 0; c < cnt; ++c) {
                b.ue();
                b.ue();
                if (sub_pic) {
                    b.ue();
                    b.ue();
                }
                b.skip(1);
            }
        }
    }
}

bool parse_sps(const std::uint8_t* p, std::size_t n, EsSps& s, int& id) {
    EsBits b(p, n);
    b.skip(4);
    const int max_sub = static_cast<int>(b.u(3));
    b.skip(1);
    skip_ptl(b, max_sub);
    id = static_cast<int>(b.ue());
    if (id > 15) return false;
    s.chroma = static_cast<int>(b.ue());
    if (s.chroma > 3) return false;
    if (s.chroma == 3) s.separate = b.u(1) != 0;
    s.width = b.ue();
    s.height = b.ue();
    if (b.u(1)) {
        const int sw = (s.chroma == 1 || s.chroma == 2) ? 2 : 1;
        const int sh = s.chroma == 1 ? 2 : 1;
        const std::uint32_t l = b.ue() * sw;
        const std::uint32_t r = b.ue() * sw;
        const std::uint32_t t = b.ue() * sh;
        const std::uint32_t bo = b.ue() * sh;
        if (l + r < s.width) s.width -= l + r;
        if (t + bo < s.height) s.height -= t + bo;
    }
    b.ue();
    b.ue();
    s.log2_poc = static_cast<int>(b.ue()) + 4;
    if (s.log2_poc > 16) return false;
    const bool ordering = b.u(1) != 0;
    for (int i = ordering ? 0 : max_sub; i <= max_sub; ++i) {
        b.ue();
        b.ue();
        b.ue();
    }
    b.ue();
    b.ue();
    b.ue();
    b.ue();
    b.ue();
    b.ue();
    if (b.u(1)) {
        if (b.u(1) && !skip_scaling_list(b)) return false;
    }
    b.skip(2);
    if (b.u(1)) {
        b.skip(8);
        b.ue();
        b.ue();
        b.skip(1);
    }
    const std::uint32_t nsets = b.ue();
    if (nsets > 64) return false;
    std::vector<EsRps> sets;
    for (std::uint32_t i = 0; i < nsets; ++i) {
        EsRps r;
        if (!parse_rps(b, static_cast<int>(i), static_cast<int>(nsets), sets, r)) return false;
        sets.push_back(r);
    }
    if (b.u(1)) {
        const std::uint32_t lt = b.ue();
        if (lt > 32) return false;
        for (std::uint32_t i = 0; i < lt; ++i) b.skip(static_cast<std::size_t>(s.log2_poc) + 1);
    }
    b.skip(2);
    if (b.failed()) return false;
    s.valid = true;
    if (b.u(1)) {
        static const std::uint16_t sar_table[17][2] = {
            {0, 0}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
            {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1},
        };
        if (b.u(1)) {
            const std::uint32_t idc = b.u(8);
            if (idc == 255) {
                s.sar_num = b.u(16);
                s.sar_den = b.u(16);
            } else if (idc > 0 && idc < 17) {
                s.sar_num = sar_table[idc][0];
                s.sar_den = sar_table[idc][1];
            }
            if (!s.sar_num || !s.sar_den) s.sar_num = s.sar_den = 1;
        }
        if (b.u(1)) b.skip(1);
        if (b.u(1)) {
            b.skip(4);
            if (b.u(1)) b.skip(24);
        }
        if (b.u(1)) {
            b.ue();
            b.ue();
        }
        b.skip(1);
        const bool field = b.u(1) != 0;
        b.skip(1);
        if (b.u(1)) {
            b.ue();
            b.ue();
            b.ue();
            b.ue();
        }
        if (b.u(1)) {
            const std::uint32_t tick = b.u(32);
            const std::uint32_t scale = b.u(32);
            if (!b.failed()) {
                s.tick = tick;
                s.scale = scale;
            }
            if (b.u(1)) b.ue();
            if (b.u(1)) skip_hrd(b, max_sub);
        }
        if (!b.failed()) s.field_seq = field;
    }
    return true;
}

bool parse_pps(const std::uint8_t* p, std::size_t n, EsPps& out, int& id) {
    EsBits b(p, n);
    id = static_cast<int>(b.ue());
    out.sps = static_cast<int>(b.ue());
    if (id > 63 || out.sps > 15) return false;
    b.skip(1);
    out.output_flag = b.u(1) != 0;
    out.extra_bits = static_cast<int>(b.u(3));
    out.valid = !b.failed();
    return out.valid;
}

bool is_irap(int t) { return t >= 16 && t <= 23; }
bool is_vcl(int t) { return t <= 9 || (t >= 16 && t <= 21); }

struct AuEntry {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::int64_t order = 0;
    bool key = false;
};

class HevcEsDemux : public Reader {
public:
    bool open(Source& source, std::string* error) override;
    bool read(Packet& out) override;
    bool seek(double seconds, bool before) override;

private:
    void handle_nal(std::uint64_t offset, std::uint64_t size);
    void finish_au(std::uint64_t end);

    Source* m_src = nullptr;
    std::vector<AuEntry> m_aus;
    std::size_t m_next = 0;
    double m_dur = 0.04;
    EsSps m_sps[16];
    EsPps m_pps[64];
    int m_active_sps = -1;
    std::vector<std::uint8_t> m_tmp;
    std::vector<std::uint8_t> m_rbsp;
    bool m_au_open = false;
    bool m_au_vcl = false;
    std::uint64_t m_au_start = 0;
    bool m_au_key = false;
    std::int64_t m_au_order = 0;
    bool m_au_has_poc = false;
    int m_prev_tid0 = 0;
    bool m_first = true;
    bool m_after_eos = true;
    std::int64_t m_cvs_base = 0;
    std::int64_t m_cvs_max = -1;
    int m_cvs_poc0 = 0;
    std::uint64_t m_last_code = 0;
};

void HevcEsDemux::finish_au(std::uint64_t end) {
    if (!m_au_open) return;
    if (m_au_vcl && end > m_au_start && m_aus.size() < (std::size_t(1) << 26)) {
        AuEntry e;
        e.offset = m_au_start;
        e.size = end - m_au_start;
        e.key = m_au_key;
        e.order = m_au_has_poc ? m_au_order : (m_aus.empty() ? 0 : m_aus.back().order + 1);
        m_aus.push_back(e);
    }
    m_au_open = false;
    m_au_vcl = false;
    m_au_key = false;
    m_au_has_poc = false;
}

void HevcEsDemux::handle_nal(std::uint64_t offset, std::uint64_t size) {
    if (size < 2) return;
    std::uint8_t hdr[2];
    if (!m_src->read_exact(offset, hdr, 2)) return;
    const int type = (hdr[0] >> 1) & 63;
    const int layer = ((hdr[0] & 1) << 5) | (hdr[1] >> 3);
    const int tid = (hdr[1] & 7) - 1;
    const bool vcl = is_vcl(type) && layer == 0;
    bool first_slice = false;
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(size - 2, (type == 33 || type == 34) ? 4096 : 64));
    if (vcl || type == 33 || type == 34) {
        m_tmp.resize(want);
        if (want && !m_src->read_exact(offset + 2, m_tmp.data(), want)) return;
        unescape(m_tmp.data(), m_tmp.size(), m_rbsp);
        if (vcl && !m_rbsp.empty()) first_slice = (m_rbsp[0] & 0x80) != 0;
    }
    const bool au_start_type = type == 32 || type == 33 || type == 34 || type == 35 || type == 39 || (type >= 41 && type <= 44) || (type >= 48 && type <= 55);
    if (m_au_open && m_au_vcl && ((vcl && first_slice) || (!vcl && au_start_type && layer == 0))) finish_au(m_last_code);
    if (!m_au_open) {
        m_au_open = true;
        m_au_start = m_last_code;
    }
    if (layer != 0) return;
    if (type == 33) {
        EsSps s;
        int id = 0;
        if (parse_sps(m_rbsp.data(), m_rbsp.size(), s, id)) m_sps[id] = s;
        return;
    }
    if (type == 34) {
        EsPps p;
        int id = 0;
        if (parse_pps(m_rbsp.data(), m_rbsp.size(), p, id)) m_pps[id] = p;
        return;
    }
    if (type == 36 || type == 37) {
        m_after_eos = true;
        return;
    }
    if (!vcl) return;
    m_au_vcl = true;
    if (!first_slice) return;
    if (is_irap(type)) m_au_key = true;
    EsBits b(m_rbsp.data(), m_rbsp.size());
    b.skip(1);
    if (is_irap(type)) b.skip(1);
    const std::uint32_t pid = b.ue();
    if (pid > 63 || !m_pps[pid].valid) return;
    const EsPps& pp = m_pps[pid];
    if (!m_sps[pp.sps].valid) return;
    const EsSps& sp = m_sps[pp.sps];
    m_active_sps = pp.sps;
    b.skip(static_cast<std::size_t>(pp.extra_bits));
    b.ue();
    if (pp.output_flag) b.skip(1);
    if (sp.separate) b.skip(2);
    int lsb = 0;
    if (type != 19 && type != 20) lsb = static_cast<int>(b.u(sp.log2_poc));
    if (b.failed()) return;
    const bool no_rasl = is_irap(type) && (type < 21 || m_first || m_after_eos);
    const int max_lsb = 1 << sp.log2_poc;
    int msb = 0;
    if (!no_rasl) {
        const int prev_lsb = m_prev_tid0 & (max_lsb - 1);
        const int prev_msb = m_prev_tid0 - prev_lsb;
        if (lsb < prev_lsb && prev_lsb - lsb >= max_lsb / 2) {
            msb = prev_msb + max_lsb;
        } else if (lsb > prev_lsb && lsb - prev_lsb > max_lsb / 2) {
            msb = prev_msb - max_lsb;
        } else {
            msb = prev_msb;
        }
    }
    const int poc = msb + lsb;
    const bool sub_non_ref = type <= 14 && (type & 1) == 0;
    if (tid == 0 && !(type >= 6 && type <= 9) && !sub_non_ref) m_prev_tid0 = poc;
    if (no_rasl) {
        if (!m_first) m_cvs_base += m_cvs_max + 1;
        m_cvs_max = -1;
        m_cvs_poc0 = poc;
    }
    m_first = false;
    m_after_eos = false;
    const std::int64_t rel = static_cast<std::int64_t>(poc) - m_cvs_poc0;
    if (rel > m_cvs_max) m_cvs_max = rel;
    m_au_order = m_cvs_base + rel;
    m_au_has_poc = true;
}

bool HevcEsDemux::open(Source& source, std::string* error) {
    m_src = &source;
    const std::uint64_t total = source.size();
    const std::size_t chunk = std::size_t(1) << 20;
    std::vector<std::uint8_t> buf(chunk);
    std::uint64_t pos = 0;
    int zeros = 0;
    bool have = false;
    std::uint64_t nal_start = 0;
    while (pos < total) {
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(chunk, total - pos));
        const std::size_t got = source.read_at(pos, buf.data(), n);
        if (got == 0) break;
        for (std::size_t i = 0; i < got; ++i) {
            const std::uint8_t c = buf[i];
            if (c == 1 && zeros >= 2) {
                const std::uint64_t at = pos + i;
                const std::uint64_t code = at - static_cast<std::uint64_t>(std::min(zeros, 3));
                if (have) {
                    std::uint64_t end = code;
                    handle_nal(nal_start, end - nal_start);
                }
                m_last_code = code;
                nal_start = at + 1;
                have = true;
                zeros = 0;
                continue;
            }
            zeros = c == 0 ? zeros + 1 : 0;
        }
        pos += got;
    }
    if (have) {
        handle_nal(nal_start, total - nal_start);
        finish_au(total);
    }
    if (m_aus.empty()) {
        if (error) *error = "no H.265 access units found";
        return false;
    }
    TrackInfo t;
    t.id = 0;
    t.type = MediaType::video;
    t.codec = "hevc";
    const EsSps* s = m_active_sps >= 0 ? &m_sps[m_active_sps] : nullptr;
    if (!s) {
        for (const EsSps& c : m_sps) {
            if (c.valid) {
                s = &c;
                break;
            }
        }
    }
    double fps = 25.0;
    bool field = false;
    if (s) {
        t.width = s->width;
        t.height = s->height;
        field = s->field_seq;
        if (s->tick && s->scale) {
            const double rate = static_cast<double>(s->scale) / s->tick;
            if (rate > 0.5 && rate < 1000.0) fps = rate;
        }
        if (field) {
            t.height *= 2;
            m_dur = 1.0 / fps;
            fps *= 0.5;
            if (!(s->tick && s->scale)) m_dur = 0.02;
        } else {
            m_dur = 1.0 / fps;
        }
        t.display_width = t.width;
        t.display_height = t.height;
        if (s->sar_num != s->sar_den && s->sar_den) {
            t.display_width = static_cast<std::uint32_t>(static_cast<double>(t.width) * s->sar_num / s->sar_den + 0.5);
        }
    }
    std::int64_t min_order = m_aus.front().order;
    std::int64_t max_order = m_aus.front().order;
    for (const AuEntry& e : m_aus) {
        min_order = std::min(min_order, e.order);
        max_order = std::max(max_order, e.order);
    }
    for (AuEntry& e : m_aus) e.order -= min_order;
    t.frame_rate = fps;
    t.timescale = 90000.0;
    t.frame_count = field ? m_aus.size() / 2 : m_aus.size();
    t.duration = static_cast<double>(max_order - min_order + 1) * m_dur;
    t.is_default = true;
    tracks.push_back(t);
    enabled.assign(1, 1);
    duration = t.duration;
    format = "hevc";
    m_next = 0;
    return true;
}

bool HevcEsDemux::read(Packet& out) {
    while (m_next < m_aus.size()) {
        const AuEntry& e = m_aus[m_next];
        const std::size_t idx = m_next++;
        if (!is_enabled(0)) continue;
        if (e.size > k_max_packet) continue;
        out.track = 0;
        if (!m_src->read_vector(e.offset, static_cast<std::size_t>(e.size), out.data)) return false;
        out.pts = static_cast<double>(e.order) * m_dur;
        out.dts = static_cast<double>(idx) * m_dur;
        out.duration = m_dur;
        out.keyframe = e.key;
        out.position = e.offset;
        out.discard_padding = 0.0;
        return true;
    }
    return false;
}

bool HevcEsDemux::seek(double seconds, bool before) {
    if (m_aus.empty()) return false;
    std::size_t best = 0;
    bool found = false;
    for (std::size_t i = 0; i < m_aus.size(); ++i) {
        if (!m_aus[i].key) continue;
        const double t = static_cast<double>(m_aus[i].order) * m_dur;
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
            m_next = m_aus.size();
            return true;
        }
    }
    m_next = best;
    return true;
}

} // namespace

bool is_hevc_annexb(const void* data, std::size_t size) noexcept {
    if (!data || size < 6) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t i = 0;
    while (i < size && i < 32 && p[i] == 0) ++i;
    if (i < 2 || i >= size || p[i] != 1 || i + 2 >= size) return false;
    const std::uint8_t h0 = p[i + 1];
    const std::uint8_t h1 = p[i + 2];
    if (h0 & 0x80) return false;
    const int type = (h0 >> 1) & 63;
    const int layer = ((h0 & 1) << 5) | (h1 >> 3);
    if (layer != 0 || (h1 & 7) == 0) return false;
    return type == 32 || type == 33 || type == 34 || type == 35 || type == 39;
}

std::unique_ptr<Reader> make_hevc_es_reader() {
    return std::unique_ptr<Reader>(new HevcEsDemux());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
