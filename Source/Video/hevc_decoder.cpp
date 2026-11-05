#include "hevc_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define FIZMO_HEVC_SSE2 1
#endif // __SSE2__

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

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

    void finish(std::uint8_t out[16]) {
        const std::uint64_t bits = m_length * 8;
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (m_used != 56) update(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (8 * i));
        update(len, 8);
        for (int i = 0; i < 4; ++i) {
            for (int b = 0; b < 4; ++b) out[i * 4 + b] = static_cast<std::uint8_t>(m_state[i] >> (8 * b));
        }
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

    std::uint32_t m_state[4];
    std::uint8_t m_buffer[64];
    std::size_t m_used = 0;
    std::uint64_t m_length = 0;
};

ColorMatrix matrix_from_vui(int m) {
    switch (m) {
    case 0: return ColorMatrix::rgb;
    case 1: return ColorMatrix::bt709;
    case 5:
    case 6: return ColorMatrix::bt601;
    case 9:
    case 10: return ColorMatrix::bt2020;
    default: return ColorMatrix::unspecified;
    }
}

bool is_irap(int t) { return t >= 16 && t <= 23; }
bool is_idr(int t) { return t == 19 || t == 20; }
bool is_bla(int t) { return t >= 16 && t <= 18; }
bool is_rasl(int t) { return t == 8 || t == 9; }
bool is_radl(int t) { return t == 6 || t == 7; }
bool is_slnr(int t) { return t <= 14 && (t & 1) == 0; }
bool is_vcl(int t) { return t <= 9 || (t >= 16 && t <= 21); }

std::uint16_t be16(const std::uint8_t* p) { return static_cast<std::uint16_t>((p[0] << 8) | p[1]); }

template <typename F>
void for_each_annexb(const std::uint8_t* data, std::size_t size, F&& fn) {
    std::size_t i = 0;
    std::size_t start = std::string::npos;
    while (i + 2 < size) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            if (start != std::string::npos) {
                std::size_t end = i;
                while (end > start && data[end - 1] == 0) --end;
                if (end > start) fn(data + start, end - start);
            }
            i += 3;
            start = i;
            continue;
        }
        ++i;
    }
    if (start != std::string::npos && start < size) {
        std::size_t end = size;
        while (end > start && data[end - 1] == 0) --end;
        if (end > start) fn(data + start, end - start);
    }
}

bool host_little_endian() {
    const std::uint16_t probe = 1;
    std::uint8_t first = 0;
    std::memcpy(&first, &probe, 1);
    return first == 1;
}

bool looks_annexb(const std::uint8_t* d, std::size_t n) {
    return (n >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 1) || (n >= 5 && d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 1);
}

PixelFormat pick_format(int chroma, int depth) {
    if (depth <= 8) {
        switch (chroma) {
        case 0: return PixelFormat::gray;
        case 2: return PixelFormat::yuv422p;
        case 3: return PixelFormat::yuv444p;
        default: return PixelFormat::yuv420p;
        }
    }
    if (depth <= 10) {
        switch (chroma) {
        case 0: return PixelFormat::gray10;
        case 2: return PixelFormat::yuv422p10;
        case 3: return PixelFormat::yuv444p10;
        default: return PixelFormat::yuv420p10;
        }
    }
    switch (chroma) {
    case 0: return PixelFormat::gray12;
    case 2: return PixelFormat::yuv422p12;
    case 3: return PixelFormat::yuv444p12;
    default: return PixelFormat::yuv420p12;
    }
}

} // namespace

bool Picture::allocate(const Sps& s) {
    chroma_format = s.chroma_array_type;
    const int nplanes = chroma_format ? 3 : 1;
    for (int c = 0; c < 3; ++c) {
        if (c >= nplanes) {
            buf[c].clear();
            plane[c] = nullptr;
            stride[c] = 0;
            width[c] = height[c] = 0;
            continue;
        }
        const int w = c ? s.width / s.sub_width_c : s.width;
        const int h = c ? s.height / s.sub_height_c : s.height;
        const std::ptrdiff_t st = (w + 2 * k_pad + 15) & ~15;
        const std::size_t need = static_cast<std::size_t>(st) * static_cast<std::size_t>(h + 2 * k_pad);
        if (buf[c].size() != need) buf[c].assign(need, 0);
        stride[c] = st;
        width[c] = w;
        height[c] = h;
        plane[c] = buf[c].data() + static_cast<std::size_t>(k_pad) * static_cast<std::size_t>(st) + k_pad;
    }
    return true;
}

void Picture::extend_borders() {
    for (int c = 0; c < 3; ++c) {
        if (!plane[c]) continue;
        const int w = width[c];
        const int h = height[c];
        const std::ptrdiff_t s = stride[c];
        std::uint16_t* base = plane[c];
        for (int y = 0; y < h; ++y) {
            std::uint16_t* row = base + y * s;
            const std::uint16_t l = row[0];
            const std::uint16_t r = row[w - 1];
            for (int i = 1; i <= k_pad; ++i) {
                row[-i] = l;
                row[w - 1 + i] = r;
            }
        }
        const std::size_t bytes = sizeof(std::uint16_t) * static_cast<std::size_t>(w + 2 * k_pad);
        for (int y = 1; y <= k_pad; ++y) {
            std::memcpy(base - y * s - k_pad, base - k_pad, bytes);
            std::memcpy(base + (h - 1 + y) * s - k_pad, base + (h - 1) * s - k_pad, bytes);
        }
    }
}

} // namespace hevc_internal

using namespace hevc_internal;

HevcDecoder::State::State() = default;

void HevcDecoder::State::reset_state(bool keep_params) {
    for (auto& p : pool) {
        p->in_use = false;
        p->ref = 0;
        p->output = false;
    }
    cur = nullptr;
    output.clear();
    have_independent = false;
    first_picture = true;
    after_eos = true;
    skip_rasl = false;
    waiting_irap = true;
    skipping_picture = false;
    prev_tid0_poc = 0;
    pending_pic_struct = -1;
    pending_hash = PictureHash();
    have_field = false;
    field.clear();
    st_before.clear();
    st_after.clear();
    lt_curr.clear();
    if (!keep_params) {
        for (auto& v : vps) v.reset();
        for (auto& s : sps) s.reset();
        for (auto& p : pps) p.reset();
        active_sps.reset();
        active_pps.reset();
        latest_sps.reset();
        layout = Layout();
    }
}

void HevcDecoder::State::update_info(const Sps& s) {
    info.profile_idc = s.ptl.profile_idc;
    info.level_idc = s.ptl.level_idc;
    info.tier = s.ptl.tier;
    info.chroma_format_idc = s.chroma_format_idc;
    info.bit_depth = s.bit_depth_y;
    info.bit_depth_chroma = s.bit_depth_c;
    info.coded_width = static_cast<std::uint32_t>(s.width);
    info.coded_height = static_cast<std::uint32_t>(s.height);
    info.width = static_cast<std::uint32_t>(s.width - s.conf_left - s.conf_right);
    info.height = static_cast<std::uint32_t>(s.height - s.conf_top - s.conf_bottom);
    info.field_coded = s.field_seq;
    info.progressive = !s.field_seq && !(s.ptl.interlaced_source && !s.ptl.progressive_source);
    if (s.field_seq && weave) info.height *= 2;
    info.sar_num = s.sar_num;
    info.sar_den = s.sar_den;
    info.max_num_reorder_pics = s.max_num_reorder[s.max_sub_layers - 1];
    info.dpb_frames = s.max_dec_pic_buffering[s.max_sub_layers - 1];
    info.temporal_layers = s.max_sub_layers;
    info.frame_rate = 0.0;
    if (s.num_units_in_tick && s.time_scale) {
        info.frame_rate = static_cast<double>(s.time_scale) / s.num_units_in_tick;
        if (s.field_seq) info.frame_rate *= 0.5;
    }
    ColorInfo c;
    c.range = s.full_range ? ColorRange::full : ColorRange::limited;
    c.matrix = matrix_from_vui(s.matrix);
    c.primaries = static_cast<std::uint8_t>(s.colour_primaries);
    c.transfer = static_cast<std::uint8_t>(s.transfer);
    c.matrix_coefficients = static_cast<std::uint8_t>(s.matrix);
    c.chroma_location = static_cast<std::uint8_t>(s.chroma_loc);
    info.color = c;
    max_temporal_id = s.max_sub_layers - 1;
}

bool HevcDecoder::State::open_hvcc(const std::uint8_t* data, std::size_t size, std::string* error) {
    if (!data || size == 0) {
        annexb = true;
        open = true;
        return true;
    }
    if (looks_annexb(data, size)) {
        annexb = true;
        open = true;
        for_each_annexb(data, size, [&](const std::uint8_t* p, std::size_t n) { handle_nal(p, n, 0.0, nullptr); });
        return true;
    }
    if (size < 23) {
        if (error) *error = "hvcC configuration is too short";
        return false;
    }
    length_size = (data[21] & 3u) + 1u;
    if (length_size == 3) {
        if (error) *error = "unsupported hvcC NAL length size";
        return false;
    }
    const unsigned arrays = data[22];
    std::size_t pos = 23;
    for (unsigned a = 0; a < arrays; ++a) {
        if (pos + 3 > size) {
            if (error) *error = "truncated hvcC";
            return false;
        }
        const unsigned count = be16(data + pos + 1);
        pos += 3;
        for (unsigned j = 0; j < count; ++j) {
            if (pos + 2 > size) {
                if (error) *error = "truncated hvcC";
                return false;
            }
            const std::size_t len = be16(data + pos);
            pos += 2;
            if (pos + len > size) {
                if (error) *error = "truncated hvcC";
                return false;
            }
            std::string e;
            if (!handle_nal(data + pos, len, 0.0, &e)) {
                if (error) *error = "invalid parameter set in hvcC: " + e;
                return false;
            }
            pos += len;
        }
    }
    annexb = false;
    open = true;
    return true;
}

bool HevcDecoder::State::decode_packet(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (!open) {
        if (error) *error = "decoder is not open";
        return false;
    }
    if (!data || size == 0) return true;
    bool ok = true;
    std::string first_error;
    auto run = [&](const std::uint8_t* p, std::size_t n) {
        std::string e;
        if (!handle_nal(p, n, pts, &e)) {
            ++errors;
            if (ok) first_error = e;
            ok = false;
        }
    };
    bool use_annexb = annexb;
    if (!use_annexb && looks_annexb(data, size)) {
        std::size_t pos = 0;
        bool valid = true;
        while (pos < size) {
            if (pos + length_size > size) {
                valid = false;
                break;
            }
            std::size_t len = 0;
            for (unsigned i = 0; i < length_size; ++i) len = (len << 8) | data[pos + i];
            pos += length_size;
            if (len > size - pos) {
                valid = false;
                break;
            }
            pos += len;
        }
        if (!valid) use_annexb = true;
    }
    if (use_annexb) {
        for_each_annexb(data, size, run);
    } else {
        std::size_t pos = 0;
        while (pos + length_size <= size) {
            std::size_t len = 0;
            for (unsigned i = 0; i < length_size; ++i) len = (len << 8) | data[pos + i];
            pos += length_size;
            if (len > size - pos) {
                ++errors;
                if (ok) first_error = "truncated NAL unit";
                ok = false;
                break;
            }
            if (len) run(data + pos, len);
            pos += len;
        }
    }
    if (cur) finish_picture();
    if (!ok && error) *error = first_error;
    return ok;
}

bool HevcDecoder::State::handle_nal(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (size < 2) return true;
    if (data[0] & 0x80) {
        if (error) *error = "forbidden_zero_bit set";
        return false;
    }
    const int type = (data[0] >> 1) & 63;
    const int layer = ((data[0] & 1) << 5) | (data[1] >> 3);
    const int tid = (data[1] & 7) - 1;
    if (layer != 0 || tid < 0) return true;
    if (type >= 41 && type != 39 && type != 40) return true;
    if (type > 21 && type < 32) return true;
    if (type >= 10 && type <= 15) return true;
    unescape_nal(data + 2, size - 2, rbsp, ep);
    Bits b(rbsp.data(), rbsp.size());
    switch (type) {
    case 32: {
        std::unique_ptr<Vps> v(new Vps());
        if (parse_vps(b, *v) && v->id < k_max_vps) vps[v->id] = std::move(v);
        return true;
    }
    case 33: {
        std::shared_ptr<Sps> s = std::make_shared<Sps>();
        std::string e;
        if (!parse_sps(b, *s, &e)) {
            if (error) *error = e.empty() ? "invalid SPS" : e;
            return false;
        }
        if (s->separate_colour_plane) {
            if (error) *error = "separate colour planes are not supported";
            return false;
        }
        s->generation = ++generation;
        sps[s->id] = s;
        latest_sps = s;
        if (!active_sps) update_info(*s);
        return true;
    }
    case 34: {
        std::shared_ptr<Pps> p = std::make_shared<Pps>();
        std::string e;
        if (!parse_pps(b, *p, &e)) {
            if (error) *error = e.empty() ? "invalid PPS" : e;
            return false;
        }
        p->generation = ++generation;
        pps[p->id] = p;
        return true;
    }
    case 35:
        return true;
    case 36:
    case 37:
        if (cur) finish_picture();
        bump(true);
        flush_field();
        after_eos = true;
        return true;
    case 39:
    case 40:
        parse_sei(b, type == 40);
        return true;
    default:
        break;
    }
    if (!is_vcl(type)) return true;
    cur_temporal_id = tid;
    return handle_slice(type, tid, pts, error);
}

void HevcDecoder::State::parse_sei(Bits& b, bool suffix) {
    while (b.more_rbsp_data() && b.left() >= 16) {
        std::uint32_t type = 0;
        std::uint32_t byte;
        while ((byte = b.u(8)) == 0xFF) {
            type += 255;
            if (b.error()) return;
        }
        type += byte;
        std::uint32_t size = 0;
        while ((byte = b.u(8)) == 0xFF) {
            size += 255;
            if (b.error()) return;
        }
        size += byte;
        if (b.error() || static_cast<std::size_t>(size) * 8 > b.left()) return;
        const std::size_t start = b.position() / 8;
        Bits pb(rbsp.data() + start, size);
        if (!suffix && type == 1) {
            const Sps* s = latest_sps ? latest_sps.get() : active_sps.get();
            if (s && s->frame_field_info) pending_pic_struct = static_cast<int>(pb.u(4));
        } else if (suffix && type == 132) {
            PictureHash h;
            h.type = static_cast<int>(pb.u(8));
            const Sps* s = active_sps.get();
            const int nc = s && s->chroma_format_idc == 0 ? 1 : 3;
            for (int c = 0; c < nc; ++c) {
                if (h.type == 0) {
                    for (int i = 0; i < 16; ++i) h.md5[c][i] = static_cast<std::uint8_t>(pb.u(8));
                } else if (h.type == 1) {
                    h.value[c] = pb.u(16);
                } else if (h.type == 2) {
                    h.value[c] = pb.u(32);
                }
            }
            if (pb.error() || h.type > 2) h.type = -1;
            if (cur) {
                cur->hash = h;
            } else {
                pending_hash = h;
            }
        }
        b.skip(static_cast<std::size_t>(size) * 8);
    }
}

bool HevcDecoder::State::parse_slice_header(Bits& b, int nal_type, SliceHeader& sh, std::string* error) {
    auto fail = [&](const char* what) {
        if (error) *error = what;
        return false;
    };
    sh.nal_type = nal_type;
    sh.first_slice_in_pic = b.flag();
    if (is_irap(nal_type)) sh.no_output_of_prior_pics = b.flag();
    const std::uint32_t pid = b.ue();
    if (pid >= static_cast<std::uint32_t>(k_max_pps)) return fail("invalid slice PPS id");
    sh.pps_id = static_cast<int>(pid);
    std::shared_ptr<const Pps> pp;
    std::shared_ptr<const Sps> sp;
    if (sh.first_slice_in_pic) {
        pp = pps[pid];
        if (!pp) return fail("slice refers to a missing PPS");
        sp = sps[pp->sps_id];
        if (!sp) return fail("slice refers to a missing SPS");
    } else {
        pp = active_pps;
        sp = active_sps;
        if (!pp || !sp || pp->id != static_cast<int>(pid)) return fail("slice PPS changed within a picture");
    }
    const Pps& p = *pp;
    const Sps& s = *sp;
    const int nctb = s.pic_w_ctb * s.pic_h_ctb;
    sh.dependent = false;
    sh.segment_address = 0;
    if (!sh.first_slice_in_pic) {
        if (p.dependent_slice_segments) sh.dependent = b.flag();
        sh.segment_address = static_cast<int>(b.u(ceil_log2(static_cast<std::uint32_t>(nctb))));
        if (sh.segment_address >= nctb) return fail("invalid slice segment address");
    }
    if (sh.dependent) {
        if (!have_independent) return fail("dependent slice without an independent slice");
        const bool first = sh.first_slice_in_pic;
        const int addr = sh.segment_address;
        const bool nop = sh.no_output_of_prior_pics;
        sh = last_independent;
        sh.first_slice_in_pic = first;
        sh.dependent = true;
        sh.segment_address = addr;
        sh.no_output_of_prior_pics = nop;
        sh.entry_points.clear();
    } else {
        sh.slice_addr_rs = sh.segment_address;
        b.skip(static_cast<std::size_t>(p.num_extra_slice_header_bits));
        const std::uint32_t st = b.ue();
        if (st > 2) return fail("invalid slice type");
        sh.slice_type = static_cast<int>(st);
        sh.pic_output_flag = p.output_flag_present ? b.flag() : true;
        if (s.separate_colour_plane) sh.colour_plane_id = static_cast<int>(b.u(2));
        sh.poc_lsb = 0;
        sh.st_rps = ShortTermRps();
        sh.num_lt = 0;
        sh.num_lt_sps = 0;
        sh.temporal_mvp = false;
        if (!is_idr(nal_type)) {
            sh.poc_lsb = static_cast<int>(b.u(s.log2_max_poc_lsb));
            sh.st_rps_sps_flag = b.flag();
            const int nsets = static_cast<int>(s.st_rps.size());
            if (!sh.st_rps_sps_flag) {
                if (!parse_st_rps(b, nsets, nsets, s.st_rps, sh.st_rps)) return fail("invalid slice short-term RPS");
            } else {
                if (nsets == 0) return fail("slice uses a missing SPS RPS");
                const int idx = nsets > 1 ? static_cast<int>(b.u(ceil_log2(static_cast<std::uint32_t>(nsets)))) : 0;
                if (idx >= nsets) return fail("invalid short-term RPS index");
                sh.st_rps_idx = idx;
                sh.st_rps = s.st_rps[static_cast<std::size_t>(idx)];
            }
            if (s.long_term_refs_present) {
                std::uint32_t nsps = 0;
                if (s.num_lt_sps > 0) nsps = b.ue();
                const std::uint32_t npics = b.ue();
                if (nsps > static_cast<std::uint32_t>(s.num_lt_sps) || nsps + npics > 32) return fail("invalid long-term reference count");
                sh.num_lt_sps = static_cast<int>(nsps);
                sh.num_lt = static_cast<int>(nsps + npics);
                for (int i = 0; i < sh.num_lt; ++i) {
                    if (i < sh.num_lt_sps) {
                        const int idx = s.num_lt_sps > 1 ? static_cast<int>(b.u(ceil_log2(static_cast<std::uint32_t>(s.num_lt_sps)))) : 0;
                        if (idx >= s.num_lt_sps) return fail("invalid lt_idx_sps");
                        sh.poc_lsb_lt[i] = s.lt_poc_lsb_sps[idx];
                        sh.used_by_curr_lt[i] = s.lt_used_sps[idx];
                    } else {
                        sh.poc_lsb_lt[i] = static_cast<int>(b.u(s.log2_max_poc_lsb));
                        sh.used_by_curr_lt[i] = b.flag() ? 1 : 0;
                    }
                    sh.delta_poc_msb_present[i] = b.flag() ? 1 : 0;
                    int delta = 0;
                    if (sh.delta_poc_msb_present[i]) {
                        const std::uint32_t d = b.ue();
                        if (d > 0xFFFFFF) return fail("invalid delta_poc_msb_cycle_lt");
                        delta = static_cast<int>(d);
                    }
                    if (i == 0 || i == sh.num_lt_sps) {
                        sh.delta_poc_msb_cycle_lt[i] = delta;
                    } else {
                        sh.delta_poc_msb_cycle_lt[i] = delta + sh.delta_poc_msb_cycle_lt[i - 1];
                    }
                }
            }
            if (s.temporal_mvp) sh.temporal_mvp = b.flag();
        }
        sh.sao_luma = false;
        sh.sao_chroma = false;
        if (s.sao) {
            sh.sao_luma = b.flag();
            if (s.chroma_array_type != 0) sh.sao_chroma = b.flag();
        }
        sh.num_ref_idx[0] = sh.num_ref_idx[1] = 0;
        sh.rpl_mod[0] = sh.rpl_mod[1] = false;
        sh.mvd_l1_zero = false;
        sh.cabac_init = false;
        sh.collocated_from_l0 = true;
        sh.collocated_ref_idx = 0;
        sh.max_num_merge_cand = 5;
        int total = 0;
        for (int i = 0; i < sh.st_rps.num_delta(); ++i) total += sh.st_rps.used[i];
        for (int i = 0; i < sh.num_lt; ++i) total += sh.used_by_curr_lt[i];
        sh.num_pic_total_curr = total;
        if (sh.slice_type != SLICE_I) {
            sh.num_ref_idx[0] = p.num_ref_idx_default[0];
            sh.num_ref_idx[1] = sh.slice_type == SLICE_B ? p.num_ref_idx_default[1] : 0;
            if (b.flag()) {
                const std::uint32_t a = b.ue() + 1;
                if (a > 15) return fail("invalid num_ref_idx_l0");
                sh.num_ref_idx[0] = static_cast<int>(a);
                if (sh.slice_type == SLICE_B) {
                    const std::uint32_t c = b.ue() + 1;
                    if (c > 15) return fail("invalid num_ref_idx_l1");
                    sh.num_ref_idx[1] = static_cast<int>(c);
                }
            }
            if (total == 0) return fail("inter slice without reference pictures");
            if (p.lists_modification_present && total > 1) {
                const int bits = ceil_log2(static_cast<std::uint32_t>(total));
                sh.rpl_mod[0] = b.flag();
                if (sh.rpl_mod[0]) {
                    for (int i = 0; i < sh.num_ref_idx[0]; ++i) sh.list_entry[0][i] = static_cast<int>(b.u(bits));
                }
                if (sh.slice_type == SLICE_B) {
                    sh.rpl_mod[1] = b.flag();
                    if (sh.rpl_mod[1]) {
                        for (int i = 0; i < sh.num_ref_idx[1]; ++i) sh.list_entry[1][i] = static_cast<int>(b.u(bits));
                    }
                }
            }
            if (sh.slice_type == SLICE_B) sh.mvd_l1_zero = b.flag();
            if (p.cabac_init_present) sh.cabac_init = b.flag();
            if (sh.temporal_mvp) {
                if (sh.slice_type == SLICE_B) sh.collocated_from_l0 = b.flag();
                const int lst = sh.collocated_from_l0 ? 0 : 1;
                if (sh.num_ref_idx[lst] > 1) {
                    const std::uint32_t c = b.ue();
                    if (c >= static_cast<std::uint32_t>(sh.num_ref_idx[lst])) return fail("invalid collocated_ref_idx");
                    sh.collocated_ref_idx = static_cast<int>(c);
                }
            }
            if ((p.weighted_pred && sh.slice_type == SLICE_P) || (p.weighted_bipred && sh.slice_type == SLICE_B)) {
                const std::uint32_t ld = b.ue();
                if (ld > 7) return fail("invalid luma_log2_weight_denom");
                sh.luma_log2_wd = static_cast<int>(ld);
                sh.chroma_log2_wd = sh.luma_log2_wd;
                if (s.chroma_array_type != 0) {
                    const int d = b.se();
                    if (sh.luma_log2_wd + d < 0 || sh.luma_log2_wd + d > 7) return fail("invalid chroma weight denom");
                    sh.chroma_log2_wd = sh.luma_log2_wd + d;
                }
                const int shift_y = s.high_precision_offsets ? 0 : s.bit_depth_y - 8;
                const int shift_c = s.high_precision_offsets ? 0 : s.bit_depth_c - 8;
                const int half_y = 1 << (s.high_precision_offsets ? s.bit_depth_y - 1 : 7);
                const int half_c = 1 << (s.high_precision_offsets ? s.bit_depth_c - 1 : 7);
                for (int l = 0; l < (sh.slice_type == SLICE_B ? 2 : 1); ++l) {
                    const int n = sh.num_ref_idx[l];
                    for (int i = 0; i < n; ++i) sh.luma_w_flag[l][i] = b.flag();
                    for (int i = 0; i < n; ++i) sh.chroma_w_flag[l][i] = s.chroma_array_type != 0 ? b.flag() : false;
                    for (int i = 0; i < n; ++i) {
                        sh.luma_w[l][i] = 1 << sh.luma_log2_wd;
                        sh.luma_o[l][i] = 0;
                        if (sh.luma_w_flag[l][i]) {
                            const int dw = b.se();
                            const int o = b.se();
                            if (dw < -128 || dw > 127 || o < -half_y || o >= half_y) return fail("invalid luma weight");
                            sh.luma_w[l][i] += dw;
                            sh.luma_o[l][i] = o * (1 << shift_y);
                        }
                        for (int j = 0; j < 2; ++j) {
                            sh.chroma_w[l][i][j] = 1 << sh.chroma_log2_wd;
                            sh.chroma_o[l][i][j] = 0;
                        }
                        if (sh.chroma_w_flag[l][i]) {
                            for (int j = 0; j < 2; ++j) {
                                const int dw = b.se();
                                const int dofs = b.se();
                                if (dw < -128 || dw > 127 || dofs < -4 * half_c || dofs >= 4 * half_c) return fail("invalid chroma weight");
                                const int w = (1 << sh.chroma_log2_wd) + dw;
                                const int o = clip3(-half_c, half_c - 1, (half_c - ((half_c * w) >> sh.chroma_log2_wd)) + dofs);
                                sh.chroma_w[l][i][j] = w;
                                sh.chroma_o[l][i][j] = o * (1 << shift_c);
                            }
                        }
                    }
                }
            }
            const std::uint32_t five = b.ue();
            if (five > 4) return fail("invalid five_minus_max_num_merge_cand");
            sh.max_num_merge_cand = 5 - static_cast<int>(five);
        }
        const int qpd = b.se();
        sh.slice_qp = p.init_qp + qpd;
        if (sh.slice_qp < -s.qp_bd_offset_y || sh.slice_qp > 51) return fail("invalid slice QP");
        sh.cb_qp_offset = 0;
        sh.cr_qp_offset = 0;
        if (p.slice_chroma_qp_offsets_present) {
            sh.cb_qp_offset = b.se();
            sh.cr_qp_offset = b.se();
            if (sh.cb_qp_offset < -12 || sh.cb_qp_offset > 12 || sh.cr_qp_offset < -12 || sh.cr_qp_offset > 12) return fail("invalid slice chroma QP offset");
        }
        sh.cu_chroma_qp_offset_enabled = p.chroma_qp_offset_list_enabled ? b.flag() : false;
        bool override_flag = false;
        if (p.deblocking_override_enabled) override_flag = b.flag();
        sh.deblocking_disabled = p.deblocking_disabled;
        sh.beta_offset_div2 = p.beta_offset_div2;
        sh.tc_offset_div2 = p.tc_offset_div2;
        if (override_flag) {
            sh.deblocking_disabled = b.flag();
            if (!sh.deblocking_disabled) {
                sh.beta_offset_div2 = b.se();
                sh.tc_offset_div2 = b.se();
                if (sh.beta_offset_div2 < -6 || sh.beta_offset_div2 > 6 || sh.tc_offset_div2 < -6 || sh.tc_offset_div2 > 6) return fail("invalid slice deblocking offsets");
            }
        }
        sh.lf_across_slices = p.loop_filter_across_slices;
        if (p.loop_filter_across_slices && (sh.sao_luma || sh.sao_chroma || !sh.deblocking_disabled)) sh.lf_across_slices = b.flag();
    }
    sh.entry_points.clear();
    if (p.tiles || p.entropy_coding_sync) {
        const std::uint32_t n = b.ue();
        if (n > static_cast<std::uint32_t>(nctb)) return fail("invalid entry point count");
        if (n > 0) {
            const std::uint32_t len = b.ue() + 1;
            if (len > 32) return fail("invalid entry point offset length");
            sh.entry_points.reserve(n);
            for (std::uint32_t i = 0; i < n; ++i) sh.entry_points.push_back(b.u(static_cast<int>(len)) + 1);
        }
    }
    if (p.slice_header_extension_present) {
        const std::uint32_t len = b.ue();
        if (len > 256) return fail("invalid slice header extension");
        b.skip(static_cast<std::size_t>(len) * 8);
    }
    if (!b.flag()) return fail("missing slice header alignment bit");
    while (!b.byte_aligned()) b.skip(1);
    if (b.error()) return fail("truncated slice header");
    sh.data_offset = b.position() / 8;
    if (!sh.dependent) {
        last_independent = sh;
        have_independent = true;
    }
    return true;
}

Picture* HevcDecoder::State::alloc_picture() {
    for (auto& p : pool) {
        if (!p->in_use) {
            p->in_use = true;
            return p.get();
        }
    }
    if (pool.size() >= 40) return nullptr;
    pool.emplace_back(new Picture());
    pool.back()->in_use = true;
    return pool.back().get();
}

Picture* HevcDecoder::State::generate_missing(int poc_value, bool long_term) {
    Picture* p = alloc_picture();
    if (!p) return nullptr;
    const Sps& s = *active_sps;
    p->allocate(s);
    p->sps = active_sps;
    for (int c = 0; c < 3; ++c) {
        if (!p->plane[c]) continue;
        const std::uint16_t v = static_cast<std::uint16_t>(1 << ((c ? s.bit_depth_c : s.bit_depth_y) - 1));
        std::fill(p->buf[c].begin(), p->buf[c].end(), v);
    }
    p->poc = poc_value;
    p->uid = ++uid_counter;
    p->ref = long_term ? 2 : 1;
    p->output = false;
    p->generated = true;
    p->mvf.clear();
    p->slices.clear();
    p->ctb_slice.clear();
    ++stats.missing_refs;
    return p;
}

bool HevcDecoder::State::apply_rps(const SliceHeader& sh) {
    const Sps& s = *active_sps;
    const int max_lsb = 1 << s.log2_max_poc_lsb;
    st_before.clear();
    st_after.clear();
    lt_curr.clear();
    if (is_irap(sh.nal_type) && no_rasl_output) {
        for (auto& p : pool) {
            if (p.get() != cur) p->ref = 0;
        }
    }
    if (is_idr(sh.nal_type)) {
        for (auto& p : pool) {
            if (p.get() != cur) p->ref = 0;
        }
        return true;
    }
    std::vector<Picture*> keep;
    struct LtEntry {
        int value;
        bool msb;
        bool curr;
    };
    std::vector<LtEntry> lts;
    for (int i = 0; i < sh.num_lt; ++i) {
        int v = sh.poc_lsb_lt[i];
        if (sh.delta_poc_msb_present[i]) v += poc - sh.delta_poc_msb_cycle_lt[i] * max_lsb - (poc & (max_lsb - 1));
        lts.push_back(LtEntry{v, sh.delta_poc_msb_present[i] != 0, sh.used_by_curr_lt[i] != 0});
    }
    std::vector<Picture*> lt_found(lts.size(), nullptr);
    for (std::size_t i = 0; i < lts.size(); ++i) {
        for (auto& p : pool) {
            Picture* q = p.get();
            if (!q->in_use || q == cur || q->ref == 0) continue;
            const int pv = lts[i].msb ? q->poc : (q->poc & (max_lsb - 1));
            if (pv == lts[i].value) {
                lt_found[i] = q;
                break;
            }
        }
    }
    const ShortTermRps& r = sh.st_rps;
    std::vector<Picture*> st_found(static_cast<std::size_t>(r.num_delta()), nullptr);
    for (int i = 0; i < r.num_delta(); ++i) {
        const int v = poc + r.delta_poc[i];
        for (auto& p : pool) {
            Picture* q = p.get();
            if (!q->in_use || q == cur || q->ref != 1) continue;
            bool is_lt = false;
            for (Picture* l : lt_found) {
                if (l == q) is_lt = true;
            }
            if (is_lt) continue;
            if (q->poc == v) {
                st_found[static_cast<std::size_t>(i)] = q;
                break;
            }
        }
    }
    for (auto& p : pool) {
        Picture* q = p.get();
        if (!q->in_use || q == cur || q->ref == 0) continue;
        bool inset = false;
        for (Picture* l : lt_found) {
            if (l == q) inset = true;
        }
        for (Picture* l : st_found) {
            if (l == q) inset = true;
        }
        if (!inset) q->ref = 0;
    }
    for (Picture* l : lt_found) {
        if (l) l->ref = 2;
    }
    for (int i = 0; i < r.num_delta(); ++i) {
        Picture* q = st_found[static_cast<std::size_t>(i)];
        if (!r.used[i]) continue;
        if (!q) q = generate_missing(poc + r.delta_poc[i], false);
        if (!q) return false;
        if (i < r.num_negative) {
            st_before.push_back(q);
        } else {
            st_after.push_back(q);
        }
    }
    for (std::size_t i = 0; i < lts.size(); ++i) {
        if (!lts[i].curr) continue;
        Picture* q = lt_found[i];
        if (!q) q = generate_missing(lts[i].value, true);
        if (!q) return false;
        lt_curr.push_back(q);
    }
    return true;
}

bool HevcDecoder::State::build_ref_lists(const SliceHeader& sh, Picture* refs[2][k_max_refs]) {
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < k_max_refs; ++i) refs[l][i] = nullptr;
    }
    if (sh.slice_type == SLICE_I) return true;
    const int total = static_cast<int>(st_before.size() + st_after.size() + lt_curr.size());
    if (total == 0) return false;
    for (int l = 0; l < (sh.slice_type == SLICE_B ? 2 : 1); ++l) {
        std::vector<Picture*> temp;
        const int n = std::max(sh.num_ref_idx[l], total);
        const std::vector<Picture*>& first = l == 0 ? st_before : st_after;
        const std::vector<Picture*>& second = l == 0 ? st_after : st_before;
        while (static_cast<int>(temp.size()) < n) {
            for (Picture* p : first) {
                if (static_cast<int>(temp.size()) < n) temp.push_back(p);
            }
            for (Picture* p : second) {
                if (static_cast<int>(temp.size()) < n) temp.push_back(p);
            }
            for (Picture* p : lt_curr) {
                if (static_cast<int>(temp.size()) < n) temp.push_back(p);
            }
        }
        for (int i = 0; i < sh.num_ref_idx[l]; ++i) {
            const int idx = sh.rpl_mod[l] ? sh.list_entry[l][i] : i;
            if (idx < 0 || idx >= static_cast<int>(temp.size())) return false;
            refs[l][i] = temp[static_cast<std::size_t>(idx)];
        }
    }
    return true;
}

void HevcDecoder::State::remove_unused() {
    for (auto& p : pool) {
        if (p->in_use && p.get() != cur && p->ref == 0 && !p->output) p->in_use = false;
    }
}

void HevcDecoder::State::bump(bool flush_all) {
    while (true) {
        Picture* best = nullptr;
        for (auto& p : pool) {
            if (!p->in_use || p.get() == cur || !p->output) continue;
            if (!best || p->poc < best->poc) best = p.get();
        }
        if (!best) break;
        output_picture(best);
        best->output = false;
        if (!best->ref) best->in_use = false;
        if (!flush_all) break;
    }
}

void HevcDecoder::State::emit_frame(VideoFrame& f, bool keyframe) {
    (void)keyframe;
    output.push_back(std::move(f));
}

void HevcDecoder::State::flush_field() {
    if (!have_field) return;
    have_field = false;
    VideoFrame out;
    const PixelFormat fmt = field.format;
    if (!out.allocate(field.width, field.height * 2, fmt)) return;
    const int bps = bytes_per_sample(fmt);
    for (int c = 0; c < plane_count(fmt); ++c) {
        const std::uint32_t h = field.plane_height(c);
        const std::size_t row = static_cast<std::size_t>(field.plane_width(c)) * static_cast<std::size_t>(bps);
        for (std::uint32_t y = 0; y < h; ++y) {
            const std::uint8_t* src = field.plane(c) + y * field.stride[c];
            std::memcpy(out.plane(c) + (2 * y) * out.stride[c], src, row);
            std::memcpy(out.plane(c) + (2 * y + 1) * out.stride[c], src, row);
        }
    }
    out.color = field.color;
    out.sar_num = field.sar_num;
    out.sar_den = field.sar_den;
    out.pts = field.pts;
    out.keyframe = field.keyframe;
    out.interlaced = true;
    out.top_field_first = field_parity == 0;
    out.structure = field_parity == 0 ? PictureStructure::top_field : PictureStructure::bottom_field;
    field.clear();
    output.push_back(std::move(out));
    ++frames;
}

void HevcDecoder::State::output_picture(Picture* pic) {
    if (!pic->sps) return;
    const Sps& s = *pic->sps;
    const int depth = std::max(s.bit_depth_y, s.bit_depth_c);
    const PixelFormat fmt = pick_format(s.chroma_array_type, depth);
    const int target = bit_depth(fmt);
    const std::uint32_t w = static_cast<std::uint32_t>(s.width - s.conf_left - s.conf_right);
    const std::uint32_t h = static_cast<std::uint32_t>(s.height - s.conf_top - s.conf_bottom);
    VideoFrame f;
    if (!f.allocate(w, h, fmt)) return;
    for (int c = 0; c < plane_count(fmt); ++c) {
        const int bd = c ? s.bit_depth_c : s.bit_depth_y;
        const int shift = target - bd;
        const int ox = c ? s.conf_left / s.sub_width_c : s.conf_left;
        const int oy = c ? s.conf_top / s.sub_height_c : s.conf_top;
        const std::uint32_t pwid = f.plane_width(c);
        const std::uint32_t phei = f.plane_height(c);
        for (std::uint32_t y = 0; y < phei; ++y) {
            const std::uint16_t* src = pic->plane[c] + (static_cast<int>(y) + oy) * pic->stride[c] + ox;
            std::uint8_t* dst = f.plane(c) + y * f.stride[c];
            if (target == 8) {
                std::uint32_t x = 0;
#if defined(FIZMO_HEVC_SSE2)
                if (shift == 0) {
                    for (; x + 16 <= pwid; x += 16) {
                        const __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + x));
                        const __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + x + 8));
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + x), _mm_packus_epi16(a, b));
                    }
                }
#endif // FIZMO_HEVC_SSE2
                for (; x < pwid; ++x) dst[x] = static_cast<std::uint8_t>(src[x] << shift);
            } else if (shift == 0 && host_little_endian()) {
                std::memcpy(dst, src, static_cast<std::size_t>(pwid) * 2);
            } else {
                for (std::uint32_t x = 0; x < pwid; ++x) {
                    const std::uint16_t v = static_cast<std::uint16_t>(src[x] << shift);
                    dst[2 * x] = static_cast<std::uint8_t>(v & 0xFF);
                    dst[2 * x + 1] = static_cast<std::uint8_t>(v >> 8);
                }
            }
        }
    }
    f.color = info.color;
    f.color.range = s.full_range ? ColorRange::full : ColorRange::limited;
    f.color.matrix = matrix_from_vui(s.matrix);
    f.color.primaries = static_cast<std::uint8_t>(s.colour_primaries);
    f.color.transfer = static_cast<std::uint8_t>(s.transfer);
    f.color.matrix_coefficients = static_cast<std::uint8_t>(s.matrix);
    f.color.chroma_location = static_cast<std::uint8_t>(s.chroma_loc);
    f.sar_num = s.sar_num;
    f.sar_den = s.sar_den;
    f.pts = pic->pts;
    f.keyframe = pic->keyframe;
    if (s.field_seq && !weave) {
        f.interlaced = true;
        if (pic->pic_struct == 1 || pic->pic_struct == 9 || pic->pic_struct == 11) f.structure = PictureStructure::top_field;
        if (pic->pic_struct == 2 || pic->pic_struct == 10 || pic->pic_struct == 12) f.structure = PictureStructure::bottom_field;
    }
    if (!(s.field_seq && weave)) {
        output.push_back(std::move(f));
        ++frames;
        return;
    }
    int parity = -1;
    if (pic->pic_struct == 1 || pic->pic_struct == 9 || pic->pic_struct == 11) parity = 0;
    if (pic->pic_struct == 2 || pic->pic_struct == 10 || pic->pic_struct == 12) parity = 1;
    if (!have_field) {
        field = std::move(f);
        field_parity = parity < 0 ? 0 : parity;
        have_field = true;
        return;
    }
    const int np = parity < 0 ? 1 - field_parity : parity;
    if (np == field_parity || field.width != f.width || field.height != f.height || field.format != f.format) {
        flush_field();
        field = std::move(f);
        field_parity = parity < 0 ? 0 : parity;
        have_field = true;
        return;
    }
    VideoFrame out;
    if (!out.allocate(f.width, f.height * 2, fmt)) {
        have_field = false;
        return;
    }
    const VideoFrame& top = field_parity == 0 ? field : f;
    const VideoFrame& bot = field_parity == 0 ? f : field;
    const int bps = bytes_per_sample(fmt);
    for (int c = 0; c < plane_count(fmt); ++c) {
        const std::uint32_t ph = top.plane_height(c);
        const std::size_t row = static_cast<std::size_t>(top.plane_width(c)) * static_cast<std::size_t>(bps);
        for (std::uint32_t y = 0; y < ph; ++y) {
            std::memcpy(out.plane(c) + (2 * y) * out.stride[c], top.plane(c) + y * top.stride[c], row);
            std::memcpy(out.plane(c) + (2 * y + 1) * out.stride[c], bot.plane(c) + y * bot.stride[c], row);
        }
    }
    out.color = field.color;
    out.sar_num = field.sar_num;
    out.sar_den = field.sar_den;
    out.pts = field.pts;
    out.keyframe = field.keyframe || f.keyframe;
    out.interlaced = true;
    out.top_field_first = field_parity == 0;
    out.structure = PictureStructure::field_pair;
    have_field = false;
    field.clear();
    output.push_back(std::move(out));
    ++frames;
    ++stats.fields_woven;
}

void HevcDecoder::State::verify_hash(Picture* pic) {
    const Sps& s = *pic->sps;
    const int nc = s.chroma_array_type == 0 ? 1 : 3;
    bool ok = true;
    for (int c = 0; c < nc; ++c) {
        const int bd = c ? s.bit_depth_c : s.bit_depth_y;
        const int w = pic->width[c];
        const int h = pic->height[c];
        if (pic->hash.type == 0) {
            Md5 md5;
            std::vector<std::uint8_t> row(static_cast<std::size_t>(w) * 2);
            for (int y = 0; y < h; ++y) {
                const std::uint16_t* src = pic->plane[c] + y * pic->stride[c];
                std::size_t n = 0;
                for (int x = 0; x < w; ++x) {
                    row[n++] = static_cast<std::uint8_t>(src[x] & 0xFF);
                    if (bd > 8) row[n++] = static_cast<std::uint8_t>(src[x] >> 8);
                }
                md5.update(row.data(), n);
            }
            std::uint8_t out[16];
            md5.finish(out);
            if (std::memcmp(out, pic->hash.md5[c], 16) != 0) ok = false;
        } else if (pic->hash.type == 1) {
            std::uint32_t crc = 0xFFFF;
            const int len = bd > 8 ? 16 : 8;
            for (int y = 0; y < h; ++y) {
                const std::uint16_t* src = pic->plane[c] + y * pic->stride[c];
                for (int x = 0; x < w; ++x) {
                    const std::uint32_t v = src[x];
                    for (int i = 0; i < len; ++i) {
                        const std::uint32_t msb = (crc >> 15) & 1;
                        const std::uint32_t bit = (v >> (len - 1 - i)) & 1;
                        crc = (((crc << 1) + bit) & 0xFFFF) ^ (msb * 0x1021);
                    }
                }
            }
            for (int i = 0; i < 16; ++i) {
                const std::uint32_t msb = (crc >> 15) & 1;
                crc = ((crc << 1) & 0xFFFF) ^ (msb * 0x1021);
            }
            if (crc != pic->hash.value[c]) ok = false;
        } else if (pic->hash.type == 2) {
            std::uint32_t sum = 0;
            for (int y = 0; y < h; ++y) {
                const std::uint16_t* src = pic->plane[c] + y * pic->stride[c];
                for (int x = 0; x < w; ++x) {
                    const std::uint32_t mask = (static_cast<std::uint32_t>(x) & 0xFF) ^ (static_cast<std::uint32_t>(y) & 0xFF) ^ (static_cast<std::uint32_t>(x) >> 8) ^ (static_cast<std::uint32_t>(y) >> 8);
                    sum += (src[x] & 0xFFu) ^ mask;
                    if (bd > 8) sum += (static_cast<std::uint32_t>(src[x]) >> 8) ^ mask;
                }
            }
            if (sum != pic->hash.value[c]) ok = false;
        }
    }
    ++stats.hash_checked;
    if (!ok) ++stats.hash_mismatches;
}

bool HevcDecoder::State::handle_slice(int nal_type, int temporal_id, double pts, std::string* error) {
    Bits b(rbsp.data(), rbsp.size());
    SliceHeader sh;
    sh.temporal_id = temporal_id;
    const bool first = rbsp.empty() ? false : (rbsp[0] & 0x80) != 0;
    if (first && cur) finish_picture();
    if (!first && (skipping_picture || !cur)) return true;
    if (!parse_slice_header(b, nal_type, sh, error)) {
        if (first) skipping_picture = true;
        return false;
    }
    sh.temporal_id = temporal_id;
    if (first) {
        skipping_picture = false;
        const bool irap = is_irap(nal_type);
        if (waiting_irap && !irap) {
            skipping_picture = true;
            return true;
        }
        if (irap) {
            no_rasl_output = is_idr(nal_type) || is_bla(nal_type) || first_picture || after_eos || handle_cra_as_bla;
            skip_rasl = no_rasl_output;
            waiting_irap = false;
            handle_cra_as_bla = false;
        }
        if (is_rasl(nal_type) && skip_rasl) {
            ++stats.skipped_rasl;
            skipping_picture = true;
            return true;
        }
        if (!start_picture(sh, pts, error)) {
            skipping_picture = true;
            if (cur) {
                cur->in_use = false;
                cur = nullptr;
            }
            return false;
        }
    }
    if (!cur) return true;
    Picture* refs[2][k_max_refs];
    if (!build_ref_lists(sh, refs)) {
        if (error) *error = "invalid reference picture lists";
        return false;
    }
    std::vector<std::size_t> subs;
    subs.push_back(sh.data_offset);
    if (!sh.entry_points.empty()) {
        std::size_t raw = sh.data_offset;
        std::size_t k = 0;
        while (k < ep.size() && ep[k] <= sh.data_offset) ++k;
        raw = sh.data_offset + k;
        for (std::uint32_t off : sh.entry_points) {
            raw += off;
            std::size_t cnt = 0;
            while (cnt < ep.size() && ep[cnt] + cnt < raw) ++cnt;
            subs.push_back(raw - cnt);
        }
    }
    std::string e;
    if (!pic_decoder.decode_slice(sh, rbsp.data(), rbsp.size(), subs, refs, &e)) {
        if (error) *error = e;
        return false;
    }
    return true;
}

bool HevcDecoder::State::start_picture(const SliceHeader& sh, double pts, std::string* error) {
    std::shared_ptr<const Pps> pp = pps[sh.pps_id];
    std::shared_ptr<const Sps> sp = pp ? sps[pp->sps_id] : nullptr;
    if (!pp || !sp) {
        if (error) *error = "missing parameter sets";
        return false;
    }
    const bool irap = is_irap(sh.nal_type);
    if (active_sps != sp) {
        const bool resized = !active_sps || active_sps->width != sp->width || active_sps->height != sp->height ||
                             active_sps->chroma_array_type != sp->chroma_array_type || active_sps->bit_depth_y != sp->bit_depth_y ||
                             active_sps->bit_depth_c != sp->bit_depth_c || active_sps->log2_ctb != sp->log2_ctb;
        if (resized && !irap) {
            if (error) *error = "SPS changed outside an IRAP picture";
            return false;
        }
        if (resized) {
            bump(true);
            flush_field();
            for (auto& p : pool) {
                p->ref = 0;
                p->output = false;
                p->in_use = false;
            }
        }
        active_sps = sp;
        update_info(*sp);
    }
    active_pps = pp;
    const Sps& s = *sp;
    const Pps& p = *pp;
    if (layout.sps_gen != s.generation || layout.pps_gen != p.generation || layout.sps_id != s.id || layout.pps_id != p.id) {
        if (!build_layout(s, p, layout)) {
            layout = Layout();
            if (error) *error = "invalid tile layout";
            return false;
        }
    }
    const int max_lsb = 1 << s.log2_max_poc_lsb;
    int msb;
    if (irap && no_rasl_output) {
        msb = 0;
    } else {
        const int prev_lsb = prev_tid0_poc & (max_lsb - 1);
        const int prev_msb = prev_tid0_poc - prev_lsb;
        if (sh.poc_lsb < prev_lsb && prev_lsb - sh.poc_lsb >= max_lsb / 2) {
            msb = prev_msb + max_lsb;
        } else if (sh.poc_lsb > prev_lsb && sh.poc_lsb - prev_lsb > max_lsb / 2) {
            msb = prev_msb - max_lsb;
        } else {
            msb = prev_msb;
        }
    }
    poc = msb + sh.poc_lsb;
    if (sh.temporal_id == 0 && !is_rasl(sh.nal_type) && !is_radl(sh.nal_type) && !is_slnr(sh.nal_type)) prev_tid0_poc = poc;
    cur = nullptr;
    if (!apply_rps(sh)) {
        if (error) *error = "reference picture set could not be built";
        return false;
    }
    const int htid = s.max_sub_layers - 1;
    if (irap && no_rasl_output && !first_picture) {
        if (sh.no_output_of_prior_pics) {
            for (auto& q : pool) {
                if (q->in_use) {
                    q->output = false;
                    q->ref = 0;
                    q->in_use = false;
                }
            }
        } else {
            bump(true);
            for (auto& q : pool) {
                if (q->in_use && !q->ref) q->in_use = false;
            }
        }
    }
    remove_unused();
    while (true) {
        int waiting = 0;
        int fullness = 0;
        bool latency = false;
        for (auto& q : pool) {
            if (!q->in_use) continue;
            ++fullness;
            if (q->output) {
                ++waiting;
                if (s.max_latency_increase_plus1[htid] && q->latency >= s.max_num_reorder[htid] + s.max_latency_increase_plus1[htid] - 1) latency = true;
            }
        }
        if (!(waiting > s.max_num_reorder[htid] || latency || fullness >= s.max_dec_pic_buffering[htid])) break;
        if (waiting == 0) break;
        bump(false);
    }
    Picture* pic = alloc_picture();
    if (!pic) {
        bump(true);
        for (auto& q : pool) {
            q->in_use = false;
            q->ref = 0;
            q->output = false;
        }
        pic = alloc_picture();
        if (!pic) {
            if (error) *error = "out of picture buffers";
            return false;
        }
    }
    pic->allocate(s);
    pic->sps = sp;
    pic->poc = poc;
    pic->uid = ++uid_counter;
    pic->ref = 0;
    pic->output = false;
    pic->latency = 0;
    pic->generated = false;
    pic->pts = pts;
    pic->keyframe = irap;
    pic->nal_type = sh.nal_type;
    pic->pic_struct = pending_pic_struct;
    pic->hash = pending_hash;
    pic->decode_index = decode_counter++;
    pic->output_flag = !(is_rasl(sh.nal_type) && no_rasl_output) && sh.pic_output_flag;
    pic->field = s.field_seq;
    pending_pic_struct = -1;
    pending_hash = PictureHash();
    cur = pic;
    pic_decoder.begin(s, p, layout, pic);
    first_picture = false;
    after_eos = false;
    return true;
}

void HevcDecoder::State::finish_picture() {
    Picture* pic = cur;
    if (!pic) return;
    pic_decoder.finish();
    pic->extend_borders();
    if (verify && pic->hash.type >= 0) verify_hash(pic);
    pic->ref = 1;
    const Sps& s = *pic->sps;
    const int htid = s.max_sub_layers - 1;
    for (auto& q : pool) {
        if (q->in_use && q.get() != pic && q->output) ++q->latency;
    }
    pic->output = pic->output_flag;
    pic->latency = 0;
    cur = nullptr;
    ++stats.pictures;
    while (true) {
        int waiting = 0;
        bool latency = false;
        for (auto& q : pool) {
            if (!q->in_use || !q->output) continue;
            ++waiting;
            if (s.max_latency_increase_plus1[htid] && q->latency >= s.max_num_reorder[htid] + s.max_latency_increase_plus1[htid] - 1) latency = true;
        }
        if (!(waiting > s.max_num_reorder[htid] || latency) || waiting == 0) break;
        bump(false);
    }
}

HevcDecoder::HevcDecoder() : m_state(new State()) {}
HevcDecoder::~HevcDecoder() = default;
HevcDecoder::HevcDecoder(HevcDecoder&&) noexcept = default;
HevcDecoder& HevcDecoder::operator=(HevcDecoder&&) noexcept = default;

bool HevcDecoder::open(const std::uint8_t* hvcc, std::size_t size, std::string* error) {
    if (!m_state) m_state.reset(new State());
    m_state->reset_state(false);
    m_state->open = false;
    return m_state->open_hvcc(hvcc, size, error);
}

bool HevcDecoder::open_annexb() {
    if (!m_state) m_state.reset(new State());
    m_state->reset_state(false);
    m_state->annexb = true;
    m_state->open = true;
    return true;
}

bool HevcDecoder::decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (!m_state) {
        if (error) *error = "decoder is not open";
        return false;
    }
    return m_state->decode_packet(data, size, pts, error);
}

bool HevcDecoder::receive(VideoFrame& out) {
    if (!m_state || m_state->output.empty()) return false;
    out = std::move(m_state->output.front());
    m_state->output.pop_front();
    return true;
}

void HevcDecoder::flush() {
    if (!m_state) return;
    if (m_state->cur) m_state->finish_picture();
    m_state->bump(true);
    m_state->flush_field();
    for (auto& p : m_state->pool) {
        p->in_use = false;
        p->ref = 0;
        p->output = false;
    }
    m_state->after_eos = true;
    m_state->waiting_irap = true;
    m_state->have_independent = false;
}

void HevcDecoder::reset() {
    if (!m_state) return;
    m_state->reset_state(true);
    m_state->handle_cra_as_bla = true;
}

void HevcDecoder::close() {
    if (!m_state) return;
    m_state->reset_state(false);
    m_state->open = false;
    m_state->pool.clear();
}

bool HevcDecoder::is_open() const noexcept {
    return m_state && m_state->open;
}

bool HevcDecoder::is_disposable(const std::uint8_t* data, std::size_t size) const noexcept {
    if (!m_state || !data || size < 3) return false;
    if (m_state->active_sps && m_state->active_sps->field_seq) return false;
    bool any = false;
    bool ok = true;
    const int max_tid = m_state->max_temporal_id;
    auto check = [&](const std::uint8_t* p, std::size_t n) {
        if (n < 2) return;
        const int t = (p[0] >> 1) & 63;
        if (!is_vcl(t)) return;
        const int tid = (p[1] & 7) - 1;
        if (!is_slnr(t) || tid != max_tid) ok = false;
        any = true;
    };
    if (m_state->annexb || looks_annexb(data, size)) {
        for_each_annexb(data, size, check);
    } else {
        const unsigned ls = m_state->length_size;
        std::size_t pos = 0;
        while (pos + ls <= size) {
            std::size_t len = 0;
            for (unsigned i = 0; i < ls; ++i) len = (len << 8) | data[pos + i];
            pos += ls;
            if (len > size - pos) return false;
            check(data + pos, len);
            pos += len;
        }
    }
    return any && ok;
}

void HevcDecoder::set_verify_hash(bool on) noexcept {
    if (m_state) m_state->verify = on;
}

void HevcDecoder::set_weave_fields(bool on) noexcept {
    if (m_state) m_state->weave = on;
}

const HevcInfo& HevcDecoder::info() const noexcept {
    static const HevcInfo none;
    return m_state ? m_state->info : none;
}

const HevcStats& HevcDecoder::stats() const noexcept {
    static const HevcStats none;
    return m_state ? m_state->stats : none;
}

std::uint64_t HevcDecoder::frames_decoded() const noexcept {
    return m_state ? m_state->frames : 0;
}

std::uint64_t HevcDecoder::error_count() const noexcept {
    return m_state ? m_state->errors : 0;
}

bool is_annexb_hevc(const void* data, std::size_t size) noexcept {
    if (!data || size < 6) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t i = 0;
    while (i < size && i < 64 && p[i] == 0) ++i;
    if (i < 2 || i >= size || p[i] != 1 || i + 2 >= size) return false;
    const std::uint8_t h0 = p[i + 1];
    const std::uint8_t h1 = p[i + 2];
    if (h0 & 0x80) return false;
    const int type = (h0 >> 1) & 63;
    const int layer = ((h0 & 1) << 5) | (h1 >> 3);
    const int tid1 = h1 & 7;
    if (layer != 0 || tid1 == 0) return false;
    return type == 32 || type == 33 || type == 34 || type == 35 || type == 39 || (type >= 16 && type <= 21);
}

bool HevcFileReader::open(const std::string& path, std::string* error) {
    close();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "can't open " + path;
        return false;
    }
    std::vector<std::uint8_t> buf;
    std::uint8_t chunk[65536];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) buf.insert(buf.end(), chunk, chunk + n);
    std::fclose(f);
    if (!open(buf.data(), buf.size())) {
        if (error) *error = "not an H.265 Annex B stream";
        return false;
    }
    return true;
}

bool HevcFileReader::open(const void* data, std::size_t size) {
    close();
    if (!is_annexb_hevc(data, size)) return false;
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    m_data.assign(p, p + size);
    std::size_t cs = 0;
    m_first = next_start(0, &cs);
    if (m_first == std::string::npos) return false;
    m_pos = m_first;
    m_open = true;
    return true;
}

void HevcFileReader::close() {
    m_data.clear();
    m_pos = 0;
    m_first = 0;
    m_open = false;
}

std::size_t HevcFileReader::next_start(std::size_t from, std::size_t* code_size) const noexcept {
    for (std::size_t i = from; i + 2 < m_data.size(); ++i) {
        if (m_data[i] == 0 && m_data[i + 1] == 0 && m_data[i + 2] == 1) {
            if (i > from && m_data[i - 1] == 0) {
                *code_size = 4;
                return i - 1;
            }
            *code_size = 3;
            return i;
        }
    }
    return std::string::npos;
}

bool HevcFileReader::read(std::vector<std::uint8_t>& access_unit) {
    access_unit.clear();
    if (!m_open || m_pos >= m_data.size()) return false;
    std::size_t pos = m_pos;
    bool have_vcl = false;
    while (pos < m_data.size()) {
        std::size_t cs = 0;
        const std::size_t s = next_start(pos, &cs);
        if (s == std::string::npos) break;
        const std::size_t hdr = s + cs;
        if (hdr + 2 >= m_data.size()) break;
        const int type = (m_data[hdr] >> 1) & 63;
        const bool vcl = type <= 31;
        const bool starts_au = (vcl && (m_data[hdr + 2] & 0x80)) || (!vcl && (type == 32 || type == 33 || type == 34 || type == 35 || type == 39 || (type >= 41 && type <= 44) || (type >= 48 && type <= 55)));
        if (have_vcl && starts_au) {
            access_unit.assign(m_data.begin() + static_cast<std::ptrdiff_t>(m_pos), m_data.begin() + static_cast<std::ptrdiff_t>(s));
            m_pos = s;
            return true;
        }
        if (vcl) have_vcl = true;
        pos = hdr + 1;
    }
    access_unit.assign(m_data.begin() + static_cast<std::ptrdiff_t>(m_pos), m_data.end());
    m_pos = m_data.size();
    return !access_unit.empty();
}

} // namespace video
} // namespace fizmo
