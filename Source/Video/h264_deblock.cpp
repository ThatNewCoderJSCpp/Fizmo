#include "h264_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace fizmo {
namespace video {
namespace h264_internal {

namespace {

inline bool mv_far(const Mv& a, const Mv& b, int limit_y) {
    return std::abs(a.x - b.x) >= 4 || std::abs(a.y - b.y) >= limit_y;
}

int motion_bs(const std::int32_t* qr, const Mv* qm, const std::int32_t* pr, const Mv* pm, int ly) {
    int qn = (qr[0] != -1) + (qr[1] != -1);
    int pn = (pr[0] != -1) + (pr[1] != -1);
    if (qn != pn) return 1;
    if (qn == 1) {
        int ql = qr[0] != -1 ? 0 : 1;
        int pl = pr[0] != -1 ? 0 : 1;
        if (qr[ql] != pr[pl]) return 1;
        return mv_far(qm[ql], pm[pl], ly) ? 1 : 0;
    }
    if (qn == 0) return 0;
    if (!((qr[0] == pr[0] && qr[1] == pr[1]) || (qr[0] == pr[1] && qr[1] == pr[0]))) return 1;
    if (qr[0] != qr[1]) {
        if (qr[0] == pr[0]) return (mv_far(qm[0], pm[0], ly) || mv_far(qm[1], pm[1], ly)) ? 1 : 0;
        return (mv_far(qm[0], pm[1], ly) || mv_far(qm[1], pm[0], ly)) ? 1 : 0;
    }
    bool a = mv_far(qm[0], pm[0], ly) || mv_far(qm[1], pm[1], ly);
    bool b = mv_far(qm[0], pm[1], ly) || mv_far(qm[1], pm[0], ly);
    return (a && b) ? 1 : 0;
}

inline unsigned col_bits(unsigned nz, int x) {
    return ((nz >> x) & 1u) | ((nz >> (x + 3)) & 2u) | ((nz >> (x + 6)) & 4u) | ((nz >> (x + 9)) & 8u);
}

struct EdgeLines {
    int bs[16];
    int pmb[16];
};

} // namespace

void DecoderState::deblock_picture() {
    int saved_plane = colour_plane;
    for (int p = 0; p < num_planes; ++p) deblock_plane(p);
    colour_plane = saved_plane;
    switch_plane(0);
}

void DecoderState::deblock_plane(int plane_id) {
    switch_plane(plane_id);
    colour_plane = plane_id;
    motion_offset = cur->motion_base(plane_id, structure);
    cur_pixel_bytes = cur->pixel_bytes;
    for (int addr = 0; addr < pic_mb_count; ++addr) {
        if (cur_pixel_bytes == 2) {
            deblock_mb_t<std::uint16_t>(addr, plane_id);
        } else {
            deblock_mb_t<std::uint8_t>(addr, plane_id);
        }
    }
}

void DecoderState::deblock_mb(int mbx, int mby) {
    deblock_mb_generic(mby * mb_width + mbx);
}

void DecoderState::deblock_mb_generic(int addr) {
    cur_pixel_bytes = cur->pixel_bytes;
    if (cur_pixel_bytes == 2) {
        deblock_mb_t<std::uint16_t>(addr, colour_plane);
    } else {
        deblock_mb_t<std::uint8_t>(addr, colour_plane);
    }
}

void DecoderState::frame_mb_bs(int addr, bool left_ok, bool top_ok, int (*bs)[4][4], bool all_edges) const {
    std::memset(bs, 0, sizeof(int) * 32);
    const MbInfo& q = mbs[static_cast<std::size_t>(addr)];
    bool q_intra = (q.flags & k_mb_intra) != 0;
    if (q_intra) {
        for (int dir = 0; dir < 2; ++dir) {
            bool edge_ok = dir == 0 ? left_ok : top_ok;
            for (int e = 0; e < 4; ++e) {
                if (e == 0 && !edge_ok) continue;
                if ((e & 1) && q.transform8x8 && !all_edges) continue;
                for (int s = 0; s < 4; ++s) bs[dir][e][s] = e == 0 ? 4 : 3;
            }
        }
    } else {
        auto fetch = [&](int maddr, int blk, std::int32_t* r, Mv* m) {
            std::size_t gi = (motion_offset + static_cast<std::size_t>(maddr)) * 16 + static_cast<std::size_t>(blk);
            std::size_t g8 = (motion_offset + static_cast<std::size_t>(maddr)) * 4 + static_cast<std::size_t>(((blk >> 3) << 1) | ((blk & 3) >> 1));
            for (int l = 0; l < 2; ++l) {
                if (cur->ref_idx[l][gi] >= 0) {
                    r[l] = cur->ref_uid[l][g8];
                    m[l] = cur->mv[l][gi];
                } else {
                    r[l] = -1;
                    m[l] = Mv{0, 0};
                }
            }
        };
        for (int dir = 0; dir < 2; ++dir) {
            bool edge_ok = dir == 0 ? left_ok : top_ok;
            for (int e = 0; e < 4; ++e) {
                if (e == 0 && !edge_ok) continue;
                if ((e & 1) && q.transform8x8 && !all_edges) continue;
                int paddr = e == 0 ? (dir == 0 ? addr - 1 : addr - mb_width) : addr;
                const MbInfo& p = mbs[static_cast<std::size_t>(paddr)];
                if (e == 0 && (p.flags & k_mb_intra)) {
                    for (int s = 0; s < 4; ++s) bs[dir][0][s] = 4;
                    continue;
                }
                bool need_motion = e == 0 || q.shape == 3 || (e == 2 && q.shape == (dir == 0 ? 2 : 1));
                unsigned qbits;
                unsigned pbits;
                if (dir == 0) {
                    qbits = col_bits(q.nz, e);
                    pbits = col_bits(p.nz, e == 0 ? 3 : e - 1);
                } else {
                    qbits = (q.nz >> (4 * e)) & 15u;
                    pbits = (p.nz >> (4 * (e == 0 ? 3 : e - 1))) & 15u;
                }
                unsigned nzbits = qbits | pbits;
                if (!need_motion && !nzbits) continue;
                for (int s = 0; s < 4; ++s) {
                    if (nzbits & (1u << s)) {
                        bs[dir][e][s] = 2;
                        continue;
                    }
                    if (!need_motion) continue;
                    int qx = dir == 0 ? e : s;
                    int qy = dir == 0 ? s : e;
                    int px = dir == 0 ? (e == 0 ? 3 : e - 1) : s;
                    int py = dir == 0 ? s : (e == 0 ? 3 : e - 1);
                    std::int32_t qr[2];
                    std::int32_t pr[2];
                    Mv qm[2];
                    Mv pm[2];
                    fetch(addr, qy * 4 + qx, qr, qm);
                    fetch(paddr, py * 4 + px, pr, pm);
                    bs[dir][e][s] = motion_bs(qr, qm, pr, pm, 4);
                }
            }
        }
    }
}

void DecoderState::deblock_mb_fast8(int addr) {
    const MbInfo& q = mbs[static_cast<std::size_t>(addr)];
    const SliceParams& sp = slices[static_cast<std::size_t>(q.slice)];
    mb_addr = addr;
    mb_field = false;
    setup_mb_position();
    auto usable = [&](int a) {
        const MbInfo& m = mbs[static_cast<std::size_t>(a)];
        if (m.slice < 0) return false;
        return sp.disable_deblocking != 2 || m.slice == q.slice;
    };
    bool left_ok = mb_x > 0 && usable(addr - 1);
    bool top_ok = mb_y > 0 && usable(addr - mb_width);
    int bs[2][4][4];
    frame_mb_bs(addr, left_ok, top_ok, bs);
    int ls = mb_pstride[0];
    std::uint8_t* luma = mb_pix[0];
    for (int dir = 0; dir < 2; ++dir) {
        for (int e = 0; e < 4; ++e) {
            const int* b = bs[dir][e];
            if (!(b[0] | b[1] | b[2] | b[3])) continue;
            int qp_p = e == 0 ? mbs[static_cast<std::size_t>(dir == 0 ? addr - 1 : addr - mb_width)].qp : q.qp;
            int qpav = (qp_p + q.qp + 1) >> 1;
            int index_a = clip3(0, 51, qpav + sp.alpha_offset);
            int index_b = clip3(0, 51, qpav + sp.beta_offset);
            int alpha = k_alpha_table[index_a];
            int beta = k_beta_table[index_b];
            if (alpha == 0 || beta == 0) continue;
            int tc0[4];
            for (int s = 0; s < 4; ++s) tc0[s] = (b[s] > 0 && b[s] < 4) ? k_tc0_table[index_a][b[s] - 1] : 0;
            if (dir == 0) {
                filter_luma_edge(luma + e * 4, 1, ls, alpha, beta, b, tc0);
            } else {
                filter_luma_edge(luma + e * 4 * ls, ls, 1, alpha, beta, b, tc0);
            }
        }
    }
    if (chroma_array_type != 1) return;
    for (int c = 0; c < 2; ++c) {
        int cs = mb_pstride[1 + c];
        std::uint8_t* chroma = mb_pix[1 + c];
        int offset = sp.chroma_qp_offset[c];
        int qc = chroma_qp_for(q.qp, c, offset);
        for (int dir = 0; dir < 2; ++dir) {
            for (int ce = 0; ce < 2; ++ce) {
                const int* b = bs[dir][ce * 2];
                if (!(b[0] | b[1] | b[2] | b[3])) continue;
                int pc = qc;
                if (ce == 0) pc = chroma_qp_for(mbs[static_cast<std::size_t>(dir == 0 ? addr - 1 : addr - mb_width)].qp, c, offset);
                int qpav = (pc + qc + 1) >> 1;
                int index_a = clip3(0, 51, qpav + sp.alpha_offset);
                int index_b = clip3(0, 51, qpav + sp.beta_offset);
                int alpha = k_alpha_table[index_a];
                int beta = k_beta_table[index_b];
                if (alpha == 0 || beta == 0) continue;
                int tc0[4];
                for (int s = 0; s < 4; ++s) tc0[s] = (b[s] > 0 && b[s] < 4) ? k_tc0_table[index_a][b[s] - 1] : 0;
                if (dir == 0) {
                    filter_chroma_edge(chroma + ce * 4, 1, cs, alpha, beta, b, tc0);
                } else {
                    filter_chroma_edge(chroma + ce * 4 * cs, cs, 1, alpha, beta, b, tc0);
                }
            }
        }
    }
}

template <typename P>
void DecoderState::deblock_mb_t(int addr, int plane_id) {
    (void)plane_id;
    const MbInfo& q = mbs[static_cast<std::size_t>(addr)];
    if (q.slice < 0) return;
    const SliceParams& sp = slices[static_cast<std::size_t>(q.slice)];
    if (sp.disable_deblocking == 1) return;
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        if (!mbaff && !field_pic && num_planes == 1 && chroma_array_type <= 1) {
            deblock_mb_fast8(addr);
            return;
        }
    }
    mb_addr = addr;
    mb_field = mbaff ? q.field != 0 : field_pic;
    setup_mb_position();
    auto usable = [&](int a) {
        if (a < 0 || a >= pic_mb_count) return false;
        const MbInfo& m = mbs[static_cast<std::size_t>(a)];
        if (m.slice < 0) return false;
        if (sp.disable_deblocking == 2 && m.slice != q.slice) return false;
        return true;
    };
    for (int i = 0; i < 4; ++i) {
        simple_nb[i] = -1;
        pair_nb[i] = -1;
    }
    if (mbaff) {
        int pair = addr >> 1;
        if (pair_x > 0 && usable(2 * (pair - 1))) pair_nb[0] = 2 * (pair - 1);
        if (pair_y > 0 && usable(2 * (pair - mb_width))) pair_nb[1] = 2 * (pair - mb_width);
    } else {
        if (mb_x > 0 && usable(addr - 1)) simple_nb[0] = addr - 1;
        if (mb_y > 0 && usable(addr - mb_width)) simple_nb[1] = addr - mb_width;
    }
    bool qfield = field_pic || mb_field;
    auto is_field_mb = [&](int a) { return field_pic || (mbaff && mbs[static_cast<std::size_t>(a)].field); };
    auto bs_for = [&](int pmb, int pblk, int qblk, bool mb_edge, bool vertical, bool mixed) -> int {
        const MbInfo& p = mbs[static_cast<std::size_t>(pmb)];
        if ((p.flags | q.flags) & k_mb_intra) {
            if (!mb_edge) return 3;
            bool pf = is_field_mb(pmb);
            if (!pf && !qfield) return 4;
            if (vertical) return 4;
            return 3;
        }
        if (((p.nz >> pblk) & 1) || ((q.nz >> qblk) & 1)) return 2;
        if (mixed) return 1;
        std::int32_t qr[2];
        std::int32_t pr[2];
        Mv qm[2];
        Mv pm[2];
        std::size_t qi = (motion_offset + static_cast<std::size_t>(addr)) * 16 + static_cast<std::size_t>(qblk);
        std::size_t pi = (motion_offset + static_cast<std::size_t>(pmb)) * 16 + static_cast<std::size_t>(pblk);
        std::size_t q8 = (motion_offset + static_cast<std::size_t>(addr)) * 4 + static_cast<std::size_t>(((qblk >> 3) << 1) | ((qblk & 3) >> 1));
        std::size_t p8 = (motion_offset + static_cast<std::size_t>(pmb)) * 4 + static_cast<std::size_t>(((pblk >> 3) << 1) | ((pblk & 3) >> 1));
        for (int l = 0; l < 2; ++l) {
            if (cur->ref_idx[l][qi] >= 0) {
                qr[l] = cur->ref_uid[l][q8];
                qm[l] = cur->mv[l][qi];
            } else {
                qr[l] = -1;
                qm[l] = Mv{0, 0};
            }
            if (cur->ref_idx[l][pi] >= 0) {
                pr[l] = cur->ref_uid[l][p8];
                pm[l] = cur->mv[l][pi];
            } else {
                pr[l] = -1;
                pm[l] = Mv{0, 0};
            }
        }
        return motion_bs(qr, qm, pr, pm, qfield ? 2 : 4);
    };
    EdgeLines ev[4];
    EdgeLines eh[4];
    std::memset(ev, 0, sizeof(ev));
    std::memset(eh, 0, sizeof(eh));
    bool left_ok = mbaff ? pair_nb[0] >= 0 : simple_nb[0] >= 0;
    bool fast_bs = !mbaff && !field_pic;
    if (fast_bs) {
        int fb[2][4][4];
        frame_mb_bs(addr, left_ok, simple_nb[1] >= 0, fb, true);
        for (int e = 0; e < 4; ++e) {
            int pv = e == 0 ? (left_ok ? simple_nb[0] : -1) : addr;
            int ph = e == 0 ? (simple_nb[1] >= 0 ? simple_nb[1] : -1) : addr;
            for (int k = 0; k < 16; ++k) {
                ev[e].pmb[k] = pv;
                ev[e].bs[k] = fb[0][e][k >> 2];
                eh[e].pmb[k] = ph;
                eh[e].bs[k] = fb[1][e][k >> 2];
            }
        }
    } else if (left_ok && !mbaff) {
        int pmb = simple_nb[0];
        for (int y = 0; y < 16; y += 4) {
            int b = bs_for(pmb, y + 3, y, true, true, false);
            for (int k = 0; k < 4; ++k) {
                ev[0].pmb[y + k] = pmb;
                ev[0].bs[y + k] = b;
            }
        }
    } else if (left_ok) {
        for (int y = 0; y < 16; ++y) {
            int xw = 0;
            int yw = 0;
            int pmb = nb_loc(-1, y, 16, 16, &xw, &yw);
            if (pmb < 0) {
                ev[0].pmb[y] = -1;
                continue;
            }
            bool mixed = mbaff && (mbs[static_cast<std::size_t>(pmb)].field != 0) != mb_field;
            ev[0].pmb[y] = pmb;
            ev[0].bs[y] = bs_for(pmb, (yw >> 2) * 4 + 3, (y >> 2) * 4, true, true, mixed);
        }
    } else {
        for (int y = 0; y < 16; ++y) ev[0].pmb[y] = -1;
    }
    for (int e = 1; e < 4 && !fast_bs; ++e) {
        for (int y = 0; y < 16; y += 4) {
            int b = bs_for(addr, y + e - 1, y + e, false, true, false);
            for (int k = 0; k < 4; ++k) {
                ev[e].pmb[y + k] = addr;
                ev[e].bs[y + k] = b;
            }
        }
    }
    bool top_ok = false;
    bool top_double = false;
    int top_p[2] = {-1, -1};
    if (!mbaff) {
        top_ok = simple_nb[1] >= 0;
        top_p[0] = simple_nb[1];
    } else if (!mb_field) {
        if (mb_bottom) {
            top_ok = true;
            top_p[0] = addr - 1;
        } else if (pair_nb[1] >= 0) {
            top_ok = true;
            if (mbs[static_cast<std::size_t>(pair_nb[1])].field) {
                top_double = true;
                top_p[0] = pair_nb[1];
                top_p[1] = pair_nb[1] + 1;
            } else {
                top_p[0] = pair_nb[1] + 1;
            }
        }
    } else if (pair_nb[1] >= 0) {
        top_ok = true;
        if (mbs[static_cast<std::size_t>(pair_nb[1])].field) {
            top_p[0] = pair_nb[1] + (mb_bottom ? 1 : 0);
        } else {
            top_p[0] = pair_nb[1] + 1;
        }
    }
    EdgeLines eh2;
    std::memset(&eh2, 0, sizeof(eh2));
    if (top_ok && !fast_bs) {
        int pmb = top_p[0];
        bool mixed = mbaff && (mbs[static_cast<std::size_t>(pmb)].field != 0) != mb_field;
        for (int x = 0; x < 16; x += 4) {
            int b = bs_for(pmb, 12 + (x >> 2), x >> 2, true, false, mixed);
            int b2 = top_double ? bs_for(top_p[1], 12 + (x >> 2), x >> 2, true, false, true) : 0;
            for (int k = 0; k < 4; ++k) {
                eh[0].pmb[x + k] = pmb;
                eh[0].bs[x + k] = b;
                if (top_double) {
                    eh2.pmb[x + k] = top_p[1];
                    eh2.bs[x + k] = b2;
                }
            }
        }
    }
    for (int e = 1; e < 4 && !fast_bs; ++e) {
        for (int x = 0; x < 16; x += 4) {
            int b = bs_for(addr, (e - 1) * 4 + (x >> 2), e * 4 + (x >> 2), false, false, false);
            for (int k = 0; k < 4; ++k) {
                eh[e].pmb[x + k] = addr;
                eh[e].bs[x + k] = b;
            }
        }
    }
    bool t8 = q.transform8x8 != 0;
    auto qp_of = [&](int mb) -> int {
        const MbInfo& m = mbs[static_cast<std::size_t>(mb)];
        return (m.flags & k_mb_pcm) ? 0 : m.qp;
    };
    int qq = qp_of(addr);
    auto filter_lines = [&](P* q0, int xs, int ls, int n, const EdgeLines& el, int line_scale, int qpq, bool chroma_style, int bd, int comp, bool chroma_qp_mode, const int* pmb_lines) {
        int sh = bd - 8;
        int last_pmb = -2;
        int index_a = 0;
        int alpha = 0;
        int beta = 0;
        for (int k = 0; k < n; ++k) {
            int li = k * line_scale;
            int bs = el.bs[li];
            if (bs <= 0) continue;
            int pmb = pmb_lines ? pmb_lines[k] : el.pmb[li];
            if (pmb < 0) continue;
            if (pmb != last_pmb) {
                last_pmb = pmb;
                int qpp = qp_of(pmb);
                if (chroma_qp_mode) {
                    qpp = chroma_qp_for(qpp, comp, sp.chroma_qp_offset[comp]);
                }
                int qpav = (qpp + qpq + 1) >> 1;
                index_a = clip3(0, 51, qpav + sp.alpha_offset);
                int index_b = clip3(0, 51, qpav + sp.beta_offset);
                alpha = k_alpha_table[index_a] << sh;
                beta = k_beta_table[index_b] << sh;
            }
            if (alpha == 0 || beta == 0) continue;
            int tc0 = bs < 4 ? (k_tc0_table[index_a][bs - 1] << sh) : 0;
            deblock_line_luma<P>(q0 + k * ls, xs, bs, alpha, beta, tc0, chroma_style, bd);
        }
    };
    int luma_planes = chroma_array_type == 3 ? 3 : 1;
    for (int pl = 0; pl < luma_planes; ++pl) {
        int s = mb_pstride[pl];
        P* base = reinterpret_cast<P*>(mb_pix[pl]);
        int bd = pl == 0 || num_planes > 1 ? bit_depth_y : bit_depth_c;
        bool cq = pl > 0;
        int comp = pl > 0 ? pl - 1 : 0;
        int qpq = cq ? chroma_qp_for(qq, comp, sp.chroma_qp_offset[comp]) : qq;
        for (int e = 0; e < 4; ++e) {
            if (e == 0 && !left_ok) continue;
            if ((e & 1) && t8) continue;
            filter_lines(base + e * 4, 1, s, 16, ev[e], 1, qpq, false, bd, comp, cq, nullptr);
        }
        for (int e = 0; e < 4; ++e) {
            if (e == 0 && !top_ok) continue;
            if ((e & 1) && t8) continue;
            if (e == 0 && top_double) {
                filter_lines(base, 2 * s, 1, 16, eh[0], 1, qpq, false, bd, comp, cq, nullptr);
                filter_lines(base + s, 2 * s, 1, 16, eh2, 1, qpq, false, bd, comp, cq, nullptr);
                continue;
            }
            filter_lines(base + e * 4 * s, s, 1, 16, eh[e], 1, qpq, false, bd, comp, cq, nullptr);
        }
    }
    if (chroma_array_type != 1 && chroma_array_type != 2) return;
    int bd = bit_depth_c;
    for (int c = 0; c < 2; ++c) {
        int s = mb_pstride[1 + c];
        P* base = reinterpret_cast<P*>(mb_pix[1 + c]);
        int qpq = chroma_qp_for(qq, c, sp.chroma_qp_offset[c]);
        int nrows = mb_hc;
        if (left_ok) {
            int pml[16];
            EdgeLines cl;
            bool mixed_left = mbaff && !mb_field && pair_nb[0] >= 0 && mbs[static_cast<std::size_t>(pair_nb[0])].field && sub_h == 2;
            for (int k = 0; k < nrows; ++k) {
                int xw = 0;
                int yw = 0;
                int li = mixed_left ? 4 * (k >> 1) + (k & 1) : k * sub_h;
                pml[k] = nb_loc(-1, k, mb_wc, mb_hc, &xw, &yw);
                cl.bs[k] = ev[0].bs[li];
                cl.pmb[k] = pml[k];
            }
            filter_lines(base, 1, s, nrows, cl, 1, qpq, true, bd, c, true, pml);
        }
        {
            EdgeLines cl;
            for (int k = 0; k < nrows; ++k) {
                cl.bs[k] = ev[2].bs[k * sub_h];
                cl.pmb[k] = addr;
            }
            filter_lines(base + 4, 1, s, nrows, cl, 1, qpq, true, bd, c, true, nullptr);
        }
        int hedges = mb_hc / 4;
        for (int ce = 0; ce < hedges; ++ce) {
            int le = chroma_array_type == 1 ? ce * 2 : ce;
            if (ce == 0 && !top_ok) continue;
            EdgeLines cl;
            for (int k = 0; k < 8; ++k) {
                cl.bs[k] = eh[le].bs[k * 2];
                cl.pmb[k] = eh[le].pmb[k * 2];
            }
            if (ce == 0 && top_double) {
                EdgeLines cl2;
                for (int k = 0; k < 8; ++k) {
                    cl2.bs[k] = eh2.bs[k * 2];
                    cl2.pmb[k] = eh2.pmb[k * 2];
                }
                filter_lines(base, 2 * s, 1, 8, cl, 1, qpq, true, bd, c, true, nullptr);
                filter_lines(base + s, 2 * s, 1, 8, cl2, 1, qpq, true, bd, c, true, nullptr);
                continue;
            }
            filter_lines(base + ce * 4 * s, s, 1, 8, cl, 1, qpq, true, bd, c, true, nullptr);
        }
    }
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
