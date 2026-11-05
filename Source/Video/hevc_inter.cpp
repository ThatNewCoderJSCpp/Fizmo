#include "hevc_internal.hpp"

#include <cstdlib>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define FIZMO_HEVC_SSE2 1
#endif // __SSE2__

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

inline Mv scale_mv(Mv mv, int td, int tb) noexcept {
    td = clip3(-128, 127, td);
    tb = clip3(-128, 127, tb);
    if (td == 0) return mv;
    const int tx = (16384 + (std::abs(td) >> 1)) / td;
    const int dsf = clip3(-4096, 4095, (tb * tx + 32) >> 6);
    Mv r;
    const int px = dsf * mv.x;
    const int py = dsf * mv.y;
    r.x = clip3(-32768, 32767, (px < 0 ? -1 : 1) * ((std::abs(px) + 127) >> 8));
    r.y = clip3(-32768, 32767, (py < 0 ? -1 : 1) * ((std::abs(py) + 127) >> 8));
    return r;
}

inline bool same_motion(const MvField& a, const MvField& b) noexcept {
    if (a.pred != b.pred) return false;
    for (int l = 0; l < 2; ++l) {
        if (!(a.pred & (1 << l))) continue;
        if (a.ref_idx[l] != b.ref_idx[l] || a.mv[l][0] != b.mv[l][0] || a.mv[l][1] != b.mv[l][1]) return false;
    }
    return true;
}

inline PuMotion to_motion(const MvField& f) noexcept {
    PuMotion m;
    m.pred = f.pred;
    for (int l = 0; l < 2; ++l) {
        m.mv[l].x = f.mv[l][0];
        m.mv[l].y = f.mv[l][1];
        m.ref_idx[l] = (f.pred & (1 << l)) ? f.ref_idx[l] : -1;
    }
    return m;
}

inline int wrap16(int v) noexcept {
    const int u = (v + 65536) & 0xFFFF;
    return u >= 32768 ? u - 65536 : u;
}

} // namespace

bool PicDecoder::pb_avail(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int xn, int yn) const {
    const bool same = xcb <= xn && ycb <= yn && xcb + ncbs > xn && ycb + ncbs > yn;
    bool a;
    if (!same) {
        a = avail(xpb, ypb, xn, yn);
    } else if ((w << 1) == ncbs && (h << 1) == ncbs && part_idx == 1 && ycb + h <= yn && xcb + w > xn) {
        a = false;
    } else {
        a = true;
    }
    if (a && (m_cu_flags[idx4(xn, yn)] & CU_INTRA)) a = false;
    return a;
}

void PicDecoder::store_motion(int x, int y, int w, int h, const PuMotion& m) {
    MvField f;
    f.pred = static_cast<std::uint8_t>(m.pred);
    f.pad = 0;
    for (int l = 0; l < 2; ++l) {
        const bool on = (m.pred & (1 << l)) != 0;
        f.mv[l][0] = static_cast<std::int16_t>(on ? m.mv[l].x : 0);
        f.mv[l][1] = static_cast<std::int16_t>(on ? m.mv[l].y : 0);
        f.ref_idx[l] = static_cast<std::int8_t>(on ? m.ref_idx[l] : -1);
    }
    MvField* base = m_pic->mvf.data();
    for (int j = y; j < y + h; j += 4) {
        MvField* row = base + static_cast<std::size_t>(j >> 2) * static_cast<std::size_t>(m_w4);
        for (int i = x; i < x + w; i += 4) row[i >> 2] = f;
    }
}

int PicDecoder::diff_poc_ref(int lx, int idx) const noexcept {
    return m_pic->poc - m_ref_poc[lx][idx];
}

bool PicDecoder::col_mv(const Picture* col, int x, int y, int ref_idx, int lx, Mv& out) {
    if (!col || col->generated || col->mvf.empty() || col->mvf_stride != m_w4) return false;
    if (static_cast<std::size_t>(y >> 2) * static_cast<std::size_t>(m_w4) + static_cast<std::size_t>(x >> 2) >= col->mvf.size()) return false;
    const MvField& f = col->mvf[static_cast<std::size_t>(y >> 2) * static_cast<std::size_t>(m_w4) + static_cast<std::size_t>(x >> 2)];
    if (f.pred == 0) return false;
    int list;
    if (!(f.pred & 1)) {
        list = 1;
    } else if (!(f.pred & 2)) {
        list = 0;
    } else {
        list = m_no_backward_pred ? lx : (m_sh->collocated_from_l0 ? 1 : 0);
    }
    const int ri = f.ref_idx[list];
    if (ri < 0 || ri >= k_max_refs) return false;
    const std::size_t rs = static_cast<std::size_t>(y >> m_log2_ctb) * static_cast<std::size_t>(m_sps->pic_w_ctb) + static_cast<std::size_t>(x >> m_log2_ctb);
    if (rs >= col->ctb_slice.size()) return false;
    const std::size_t si = col->ctb_slice[rs];
    if (si >= col->slices.size()) return false;
    const SliceRefs& sr = col->slices[si];
    const bool col_lt = sr.lt[list][ri] != 0;
    const bool cur_lt = m_ref_lt[lx][ref_idx];
    if (col_lt != cur_lt) return false;
    Mv mv;
    mv.x = f.mv[list][0];
    mv.y = f.mv[list][1];
    const int col_diff = col->poc - sr.poc[list][ri];
    const int cur_diff = diff_poc_ref(lx, ref_idx);
    if (cur_lt || col_diff == cur_diff) {
        out = mv;
    } else {
        out = scale_mv(mv, col_diff, cur_diff);
    }
    return true;
}

bool PicDecoder::temporal_mv(int xpb, int ypb, int w, int h, int ref_idx, int lx, Mv& out) {
    if (!m_sh->temporal_mvp) return false;
    const int cl = (m_slice_type == SLICE_B && !m_sh->collocated_from_l0) ? 1 : 0;
    const int ci = m_sh->collocated_ref_idx;
    if (ci < 0 || ci >= m_sh->num_ref_idx[cl]) return false;
    const Picture* col = m_refs[cl][ci];
    if (!col) return false;
    const int xbr = xpb + w;
    const int ybr = ypb + h;
    if ((ypb >> m_log2_ctb) == (ybr >> m_log2_ctb) && ybr < m_h && xbr < m_w) {
        if (col_mv(col, (xbr >> 4) << 4, (ybr >> 4) << 4, ref_idx, lx, out)) return true;
    }
    const int xc = xpb + (w >> 1);
    const int yc = ypb + (h >> 1);
    return col_mv(col, (xc >> 4) << 4, (yc >> 4) << 4, ref_idx, lx, out);
}

void PicDecoder::derive_merge(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int merge_idx, PuMotion& out) {
    const int wo = w;
    const int ho = h;
    const int pml = m_pps->log2_parallel_merge_level;
    if (pml > 2 && ncbs == 8) {
        xpb = xcb;
        ypb = ycb;
        w = ncbs;
        h = ncbs;
        part_idx = 0;
    }
    PuMotion cand[6];
    int count = 0;
    auto region = [&](int xn, int yn) { return (xpb >> pml) == (xn >> pml) && (ypb >> pml) == (yn >> pml); };
    const int part = m_cu_part;
    const int xa1 = xpb - 1;
    const int ya1 = ypb + h - 1;
    const bool a1 = !region(xa1, ya1) && !(part_idx == 1 && (part == PART_Nx2N || part == PART_nLx2N || part == PART_nRx2N)) && pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xa1, ya1);
    const MvField* fa1 = a1 ? &mvf_at(xa1, ya1) : nullptr;
    if (a1) cand[count++] = to_motion(*fa1);
    const int xb1 = xpb + w - 1;
    const int yb1 = ypb - 1;
    const bool b1 = !region(xb1, yb1) && !(part_idx == 1 && (part == PART_2NxN || part == PART_2NxnU || part == PART_2NxnD)) && pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xb1, yb1);
    const MvField* fb1 = b1 ? &mvf_at(xb1, yb1) : nullptr;
    if (b1 && !(a1 && same_motion(*fa1, *fb1))) cand[count++] = to_motion(*fb1);
    if (merge_idx < count) {
        out = cand[merge_idx];
    } else {
        const int xb0 = xpb + w;
        const int yb0 = ypb - 1;
        bool b0 = !region(xb0, yb0) && pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xb0, yb0);
        if (b0 && b1 && same_motion(*fb1, mvf_at(xb0, yb0))) b0 = false;
        if (b0) cand[count++] = to_motion(mvf_at(xb0, yb0));
        const int xa0 = xpb - 1;
        const int ya0 = ypb + h;
        bool a0 = !region(xa0, ya0) && pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xa0, ya0);
        if (a0 && a1 && same_motion(*fa1, mvf_at(xa0, ya0))) a0 = false;
        if (a0) cand[count++] = to_motion(mvf_at(xa0, ya0));
        const int xb2 = xpb - 1;
        const int yb2 = ypb - 1;
        bool b2 = !region(xb2, yb2) && pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xb2, yb2);
        if (b2 && a1 && same_motion(*fa1, mvf_at(xb2, yb2))) b2 = false;
        if (b2 && b1 && same_motion(*fb1, mvf_at(xb2, yb2))) b2 = false;
        if (count == 4) b2 = false;
        if (b2) cand[count++] = to_motion(mvf_at(xb2, yb2));
        const int maxc = m_sh->max_num_merge_cand;
        if (merge_idx >= count && count < maxc) {
            Mv m0 = {0, 0};
            Mv m1 = {0, 0};
            const bool l0 = temporal_mv(xpb, ypb, w, h, 0, 0, m0);
            const bool l1 = m_slice_type == SLICE_B && temporal_mv(xpb, ypb, w, h, 0, 1, m1);
            if (l0 || l1) {
                PuMotion c;
                c.pred = (l0 ? 1 : 0) | (l1 ? 2 : 0);
                c.mv[0] = l0 ? m0 : Mv{0, 0};
                c.mv[1] = l1 ? m1 : Mv{0, 0};
                c.ref_idx[0] = l0 ? 0 : -1;
                c.ref_idx[1] = l1 ? 0 : -1;
                cand[count++] = c;
            }
        }
        if (merge_idx >= count && m_slice_type == SLICE_B && count > 1 && count < maxc) {
            static const int l0i[12] = {0, 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
            static const int l1i[12] = {1, 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
            const int orig = count;
            for (int ci = 0; ci < orig * (orig - 1) && count < maxc; ++ci) {
                const PuMotion& c0 = cand[l0i[ci]];
                const PuMotion& c1 = cand[l1i[ci]];
                if ((c0.pred & 1) && (c1.pred & 2)) {
                    const bool diff_pic = m_ref_poc[0][c0.ref_idx[0]] != m_ref_poc[1][c1.ref_idx[1]] || m_refs[0][c0.ref_idx[0]] != m_refs[1][c1.ref_idx[1]];
                    if (diff_pic || c0.mv[0].x != c1.mv[1].x || c0.mv[0].y != c1.mv[1].y) {
                        PuMotion c;
                        c.pred = 3;
                        c.mv[0] = c0.mv[0];
                        c.mv[1] = c1.mv[1];
                        c.ref_idx[0] = c0.ref_idx[0];
                        c.ref_idx[1] = c1.ref_idx[1];
                        cand[count++] = c;
                    }
                }
            }
        }
        if (merge_idx >= count) {
            const int nref = m_slice_type == SLICE_P ? m_sh->num_ref_idx[0] : (m_sh->num_ref_idx[0] < m_sh->num_ref_idx[1] ? m_sh->num_ref_idx[0] : m_sh->num_ref_idx[1]);
            int zi = 0;
            while (count < maxc && count <= merge_idx) {
                PuMotion c;
                const int r = zi < nref ? zi : 0;
                c.pred = m_slice_type == SLICE_P ? 1 : 3;
                c.mv[0] = Mv{0, 0};
                c.mv[1] = Mv{0, 0};
                c.ref_idx[0] = r;
                c.ref_idx[1] = m_slice_type == SLICE_P ? -1 : r;
                cand[count++] = c;
                ++zi;
            }
        }
        if (merge_idx >= count) merge_idx = count - 1;
        out = cand[merge_idx];
    }
    if (out.pred == 3 && wo + ho == 12) {
        out.pred = 1;
        out.ref_idx[1] = -1;
    }
}

void PicDecoder::derive_amvp(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int lx, int ref_idx, Mv& out, int mvp_flag) {
    const int ly = 1 - lx;
    const int target_poc = m_ref_poc[lx][ref_idx];
    const bool target_lt = m_ref_lt[lx][ref_idx];
    const int cur_poc = m_pic->poc;
    const int xa[2] = {xpb - 1, xpb - 1};
    const int ya[2] = {ypb + h, ypb + h - 1};
    bool av_a[2];
    for (int k = 0; k < 2; ++k) av_a[k] = pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xa[k], ya[k]);
    const bool scaled_flag = av_a[0] || av_a[1];
    bool fa = false;
    Mv mva = {0, 0};
    for (int k = 0; k < 2 && !fa; ++k) {
        if (!av_a[k]) continue;
        const MvField& f = mvf_at(xa[k], ya[k]);
        if ((f.pred & (1 << lx)) && m_ref_poc[lx][f.ref_idx[lx]] == target_poc) {
            fa = true;
            mva = Mv{f.mv[lx][0], f.mv[lx][1]};
        } else if ((f.pred & (1 << ly)) && m_ref_poc[ly][f.ref_idx[ly]] == target_poc) {
            fa = true;
            mva = Mv{f.mv[ly][0], f.mv[ly][1]};
        }
    }
    for (int k = 0; k < 2 && !fa; ++k) {
        if (!av_a[k]) continue;
        const MvField& f = mvf_at(xa[k], ya[k]);
        int rl = -1;
        if ((f.pred & (1 << lx)) && m_ref_lt[lx][f.ref_idx[lx]] == target_lt) {
            rl = lx;
        } else if ((f.pred & (1 << ly)) && m_ref_lt[ly][f.ref_idx[ly]] == target_lt) {
            rl = ly;
        }
        if (rl >= 0) {
            fa = true;
            mva = Mv{f.mv[rl][0], f.mv[rl][1]};
            const int rpoc = m_ref_poc[rl][f.ref_idx[rl]];
            if (!m_ref_lt[rl][f.ref_idx[rl]] && !target_lt) mva = scale_mv(mva, cur_poc - rpoc, cur_poc - target_poc);
        }
    }
    const int xb[3] = {xpb + w, xpb + w - 1, xpb - 1};
    const int yb[3] = {ypb - 1, ypb - 1, ypb - 1};
    bool av_b[3];
    for (int k = 0; k < 3; ++k) av_b[k] = pb_avail(xcb, ycb, ncbs, xpb, ypb, w, h, part_idx, xb[k], yb[k]);
    bool fb = false;
    Mv mvb = {0, 0};
    for (int k = 0; k < 3 && !fb; ++k) {
        if (!av_b[k]) continue;
        const MvField& f = mvf_at(xb[k], yb[k]);
        if ((f.pred & (1 << lx)) && m_ref_poc[lx][f.ref_idx[lx]] == target_poc) {
            fb = true;
            mvb = Mv{f.mv[lx][0], f.mv[lx][1]};
        } else if ((f.pred & (1 << ly)) && m_ref_poc[ly][f.ref_idx[ly]] == target_poc) {
            fb = true;
            mvb = Mv{f.mv[ly][0], f.mv[ly][1]};
        }
    }
    if (!scaled_flag && fb) {
        fa = true;
        mva = mvb;
    }
    if (!scaled_flag) {
        fb = false;
        for (int k = 0; k < 3 && !fb; ++k) {
            if (!av_b[k]) continue;
            const MvField& f = mvf_at(xb[k], yb[k]);
            int rl = -1;
            if ((f.pred & (1 << lx)) && m_ref_lt[lx][f.ref_idx[lx]] == target_lt) {
                rl = lx;
            } else if ((f.pred & (1 << ly)) && m_ref_lt[ly][f.ref_idx[ly]] == target_lt) {
                rl = ly;
            }
            if (rl >= 0) {
                fb = true;
                mvb = Mv{f.mv[rl][0], f.mv[rl][1]};
                const int rpoc = m_ref_poc[rl][f.ref_idx[rl]];
                if (!m_ref_lt[rl][f.ref_idx[rl]] && !target_lt) mvb = scale_mv(mvb, cur_poc - rpoc, cur_poc - target_poc);
            }
        }
    }
    Mv list[2] = {{0, 0}, {0, 0}};
    int n = 0;
    if (fa) list[n++] = mva;
    if (fb && !(fa && mva.x == mvb.x && mva.y == mvb.y)) list[n++] = mvb;
    if (n < 2 && mvp_flag >= n) {
        Mv col;
        if (temporal_mv(xpb, ypb, w, h, ref_idx, lx, col)) list[n++] = col;
    }
    out = list[mvp_flag ? 1 : 0];
}

void PicDecoder::prediction_unit(int x0, int y0, int w, int h, int part_idx) {
    if (m_error) return;
    Cabac& cb = m_cabac;
    PuMotion m;
    m.pred = 0;
    m.ref_idx[0] = m.ref_idx[1] = -1;
    m.mv[0] = m.mv[1] = Mv{0, 0};
    const int ncbs = 1 << m_cu_log2;
    const bool skip = (m_cu_flags[idx4(x0, y0)] & CU_SKIP) != 0;
    auto merge_idx = [&]() {
        int idx = 0;
        const int cmax = m_sh->max_num_merge_cand - 1;
        if (cmax > 0) {
            if (cb.decode(m_ctx[CTX_MERGE_IDX])) {
                idx = 1;
                while (idx < cmax && cb.bypass()) ++idx;
            }
        }
        return idx;
    };
    bool merge = skip;
    if (!skip) merge = cb.decode(m_ctx[CTX_MERGE_FLAG]) != 0;
    if (part_idx == 0) m_merge_flag = merge;
    if (merge) {
        const int idx = merge_idx();
        derive_merge(m_cu_x, m_cu_y, ncbs, x0, y0, w, h, part_idx, idx, m);
    } else {
        int idc = 0;
        if (m_slice_type == SLICE_B) {
            if (w + h != 12 && cb.decode(m_ctx[CTX_INTER_PRED + m_cur_depth])) {
                idc = 2;
            } else {
                idc = cb.decode(m_ctx[CTX_INTER_PRED + 4]);
            }
        }
        int mvd[2][2] = {{0, 0}, {0, 0}};
        int mvp[2] = {0, 0};
        int ref[2] = {-1, -1};
        auto ref_idx = [&](int l) {
            const int cmax = m_sh->num_ref_idx[l] - 1;
            int i = 0;
            while (i < cmax) {
                const int b = i < 2 ? cb.decode(m_ctx[CTX_REF_IDX + i]) : cb.bypass();
                if (!b) break;
                ++i;
            }
            return i;
        };
        auto mvd_coding = [&](int* out) {
            const int g0x = cb.decode(m_ctx[CTX_MVD_G0]);
            const int g0y = cb.decode(m_ctx[CTX_MVD_G0]);
            const int g1x = g0x ? cb.decode(m_ctx[CTX_MVD_G1]) : 0;
            const int g1y = g0y ? cb.decode(m_ctx[CTX_MVD_G1]) : 0;
            const int g0[2] = {g0x, g0y};
            const int g1[2] = {g1x, g1y};
            for (int c = 0; c < 2; ++c) {
                out[c] = 0;
                if (!g0[c]) continue;
                int v = 1;
                if (g1[c]) {
                    int k = 1;
                    int add = 0;
                    while (cb.bypass()) {
                        add += 1 << k;
                        if (++k > 18) {
                            m_error = true;
                            return;
                        }
                    }
                    add += static_cast<int>(cb.bypass_bits(k));
                    v = 2 + add;
                }
                if (cb.bypass()) v = -v;
                out[c] = v;
            }
        };
        if (idc != 1) {
            ref[0] = m_sh->num_ref_idx[0] > 1 ? ref_idx(0) : 0;
            mvd_coding(mvd[0]);
            mvp[0] = cb.decode(m_ctx[CTX_MVP_FLAG]);
        }
        if (idc != 0) {
            ref[1] = m_sh->num_ref_idx[1] > 1 ? ref_idx(1) : 0;
            if (m_sh->mvd_l1_zero && idc == 2) {
                mvd[1][0] = mvd[1][1] = 0;
            } else {
                mvd_coding(mvd[1]);
            }
            mvp[1] = cb.decode(m_ctx[CTX_MVP_FLAG]);
        }
        if (m_error) return;
        m.pred = idc == 0 ? 1 : (idc == 1 ? 2 : 3);
        for (int l = 0; l < 2; ++l) {
            if (!(m.pred & (1 << l))) continue;
            if (ref[l] >= m_sh->num_ref_idx[l] || !m_refs[l][ref[l]]) {
                m_error = true;
                return;
            }
            Mv p;
            derive_amvp(m_cu_x, m_cu_y, ncbs, x0, y0, w, h, part_idx, l, ref[l], p, mvp[l]);
            m.mv[l].x = wrap16(p.x + mvd[l][0]);
            m.mv[l].y = wrap16(p.y + mvd[l][1]);
            m.ref_idx[l] = ref[l];
        }
    }
    for (int l = 0; l < 2; ++l) {
        if ((m.pred & (1 << l)) && (m.ref_idx[l] < 0 || m.ref_idx[l] >= m_sh->num_ref_idx[l] || !m_refs[l][m.ref_idx[l]])) {
            m_error = true;
            return;
        }
    }
    store_motion(x0, y0, w, h, m);
    motion_compensate(x0, y0, w, h, m);
}

namespace {

template <int F>
struct LumaTaps {
    static const int t0 = F == 1 ? -1 : (F == 2 ? -1 : 0);
    static const int t1 = F == 1 ? 4 : (F == 2 ? 4 : 1);
    static const int t2 = F == 1 ? -10 : (F == 2 ? -11 : -5);
    static const int t3 = F == 1 ? 58 : (F == 2 ? 40 : 17);
    static const int t4 = F == 1 ? 17 : (F == 2 ? 40 : 58);
    static const int t5 = F == 1 ? -5 : (F == 2 ? -11 : -10);
    static const int t6 = F == 1 ? 1 : (F == 2 ? 4 : 4);
    static const int t7 = F == 1 ? 0 : (F == 2 ? -1 : -1);
};

template <int F, typename T>
inline int luma_tap(const T* p, std::ptrdiff_t s) noexcept {
    typedef LumaTaps<F> L;
    return L::t0 * p[0] + L::t1 * p[s] + L::t2 * p[2 * s] + L::t3 * p[3 * s] + L::t4 * p[4 * s] + L::t5 * p[5 * s] + L::t6 * p[6 * s] + L::t7 * p[7 * s];
}

template <int F>
void luma_h(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + j * ss - 3;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(luma_tap<F>(r + i, 1) >> shift);
    }
}

template <int F>
void luma_v(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + (j - 3) * ss;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(luma_tap<F>(r + i, ss) >> shift);
    }
}

template <int F>
void luma_v16(const std::int16_t* src, std::int16_t* dst, int w, int h) {
    for (int j = 0; j < h; ++j) {
        const std::int16_t* r = src + j * w;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(luma_tap<F>(r + i, w) >> 6);
    }
}

template <int F>
struct ChromaTaps {
    static const int t0 = F == 1 ? -2 : F == 2 ? -4 : F == 3 ? -6 : F == 4 ? -4 : F == 5 ? -4 : F == 6 ? -2 : -2;
    static const int t1 = F == 1 ? 58 : F == 2 ? 54 : F == 3 ? 46 : F == 4 ? 36 : F == 5 ? 28 : F == 6 ? 16 : 10;
    static const int t2 = F == 1 ? 10 : F == 2 ? 16 : F == 3 ? 28 : F == 4 ? 36 : F == 5 ? 46 : F == 6 ? 54 : 58;
    static const int t3 = F == 1 ? -2 : F == 2 ? -2 : F == 3 ? -4 : F == 4 ? -4 : F == 5 ? -6 : F == 6 ? -4 : -2;
};

template <int F, typename T>
inline int chroma_tap(const T* p, std::ptrdiff_t s) noexcept {
    typedef ChromaTaps<F> C;
    return C::t0 * p[0] + C::t1 * p[s] + C::t2 * p[2 * s] + C::t3 * p[3 * s];
}

template <int F>
void chroma_h(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + j * ss - 1;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(chroma_tap<F>(r + i, 1) >> shift);
    }
}

template <int F>
void chroma_v(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + (j - 1) * ss;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(chroma_tap<F>(r + i, ss) >> shift);
    }
}

template <int F>
void chroma_v16(const std::int16_t* src, std::int16_t* dst, int w, int h) {
    for (int j = 0; j < h; ++j) {
        const std::int16_t* r = src + j * w;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(chroma_tap<F>(r + i, w) >> 6);
    }
}

void copy_shift(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + j * ss;
        std::int16_t* d = dst + j * w;
        for (int i = 0; i < w; ++i) d[i] = static_cast<std::int16_t>(r[i] << shift);
    }
}

#if defined(FIZMO_HEVC_SSE2)

inline __m128i pair_coef(int a, int b) {
    return _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(b) << 16) | (static_cast<std::uint32_t>(a) & 0xFFFFu)));
}

template <int F>
void luma_h_sse2(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    typedef LumaTaps<F> L;
    const __m128i c01 = pair_coef(L::t0, L::t1);
    const __m128i c23 = pair_coef(L::t2, L::t3);
    const __m128i c45 = pair_coef(L::t4, L::t5);
    const __m128i c67 = pair_coef(L::t6, L::t7);
    const __m128i sh = _mm_cvtsi32_si128(shift);
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + j * ss - 3;
        std::int16_t* d = dst + j * w;
        int i = 0;
        for (; i + 8 <= w; i += 8) {
            const __m128i p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i));
            const __m128i p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 1));
            const __m128i p2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 2));
            const __m128i p3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 3));
            const __m128i p4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 4));
            const __m128i p5 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 5));
            const __m128i p6 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 6));
            const __m128i p7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 7));
            __m128i lo = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpacklo_epi16(p2, p3), c23)),
                                       _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p4, p5), c45), _mm_madd_epi16(_mm_unpacklo_epi16(p6, p7), c67)));
            __m128i hi = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpackhi_epi16(p2, p3), c23)),
                                       _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p4, p5), c45), _mm_madd_epi16(_mm_unpackhi_epi16(p6, p7), c67)));
            lo = _mm_sra_epi32(lo, sh);
            hi = _mm_sra_epi32(hi, sh);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packs_epi32(lo, hi));
        }
        for (; i < w; ++i) d[i] = static_cast<std::int16_t>(luma_tap<F>(r + i, 1) >> shift);
    }
}

template <int F, typename T>
void luma_v_sse2(const T* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    typedef LumaTaps<F> L;
    const __m128i c01 = pair_coef(L::t0, L::t1);
    const __m128i c23 = pair_coef(L::t2, L::t3);
    const __m128i c45 = pair_coef(L::t4, L::t5);
    const __m128i c67 = pair_coef(L::t6, L::t7);
    const __m128i sh = _mm_cvtsi32_si128(shift);
    for (int j = 0; j < h; ++j) {
        const T* r = src + j * ss;
        std::int16_t* d = dst + j * w;
        int i = 0;
        for (; i + 8 <= w; i += 8) {
            const __m128i p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i));
            const __m128i p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + ss));
            const __m128i p2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 2 * ss));
            const __m128i p3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 3 * ss));
            const __m128i p4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 4 * ss));
            const __m128i p5 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 5 * ss));
            const __m128i p6 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 6 * ss));
            const __m128i p7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 7 * ss));
            __m128i lo = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpacklo_epi16(p2, p3), c23)),
                                       _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p4, p5), c45), _mm_madd_epi16(_mm_unpacklo_epi16(p6, p7), c67)));
            __m128i hi = _mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpackhi_epi16(p2, p3), c23)),
                                       _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p4, p5), c45), _mm_madd_epi16(_mm_unpackhi_epi16(p6, p7), c67)));
            lo = _mm_sra_epi32(lo, sh);
            hi = _mm_sra_epi32(hi, sh);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packs_epi32(lo, hi));
        }
        for (; i < w; ++i) d[i] = static_cast<std::int16_t>(luma_tap<F>(r + i, ss) >> shift);
    }
}

template <int F>
void chroma_h_sse2(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    typedef ChromaTaps<F> C;
    const __m128i c01 = pair_coef(C::t0, C::t1);
    const __m128i c23 = pair_coef(C::t2, C::t3);
    const __m128i sh = _mm_cvtsi32_si128(shift);
    for (int j = 0; j < h; ++j) {
        const std::uint16_t* r = src + j * ss - 1;
        std::int16_t* d = dst + j * w;
        int i = 0;
        for (; i + 8 <= w; i += 8) {
            const __m128i p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i));
            const __m128i p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 1));
            const __m128i p2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 2));
            const __m128i p3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 3));
            __m128i lo = _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpacklo_epi16(p2, p3), c23));
            __m128i hi = _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpackhi_epi16(p2, p3), c23));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packs_epi32(_mm_sra_epi32(lo, sh), _mm_sra_epi32(hi, sh)));
        }
        for (; i < w; ++i) d[i] = static_cast<std::int16_t>(chroma_tap<F>(r + i, 1) >> shift);
    }
}

template <int F, typename T>
void chroma_v_sse2(const T* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    typedef ChromaTaps<F> C;
    const __m128i c01 = pair_coef(C::t0, C::t1);
    const __m128i c23 = pair_coef(C::t2, C::t3);
    const __m128i sh = _mm_cvtsi32_si128(shift);
    for (int j = 0; j < h; ++j) {
        const T* r = src + j * ss;
        std::int16_t* d = dst + j * w;
        int i = 0;
        for (; i + 8 <= w; i += 8) {
            const __m128i p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i));
            const __m128i p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + ss));
            const __m128i p2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 2 * ss));
            const __m128i p3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i + 3 * ss));
            __m128i lo = _mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpacklo_epi16(p2, p3), c23));
            __m128i hi = _mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(p0, p1), c01), _mm_madd_epi16(_mm_unpackhi_epi16(p2, p3), c23));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i), _mm_packs_epi32(_mm_sra_epi32(lo, sh), _mm_sra_epi32(hi, sh)));
        }
        for (; i < w; ++i) d[i] = static_cast<std::int16_t>(chroma_tap<F>(r + i, ss) >> shift);
    }
}

template <int F>
void luma_vp_sse2(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    luma_v_sse2<F, std::uint16_t>(src - 3 * ss, ss, dst, w, h, shift);
}

template <int F>
void luma_v16_sse2(const std::int16_t* src, std::int16_t* dst, int w, int h) {
    luma_v_sse2<F, std::int16_t>(src, w, dst, w, h, 6);
}

template <int F>
void chroma_vp_sse2(const std::uint16_t* src, std::ptrdiff_t ss, std::int16_t* dst, int w, int h, int shift) {
    chroma_v_sse2<F, std::uint16_t>(src - ss, ss, dst, w, h, shift);
}

template <int F>
void chroma_v16_sse2(const std::int16_t* src, std::int16_t* dst, int w, int h) {
    chroma_v_sse2<F, std::int16_t>(src, w, dst, w, h, 6);
}

#endif // FIZMO_HEVC_SSE2

typedef void (*FiltFn)(const std::uint16_t*, std::ptrdiff_t, std::int16_t*, int, int, int);
typedef void (*Filt16Fn)(const std::int16_t*, std::int16_t*, int, int);

#if defined(FIZMO_HEVC_SSE2)
const FiltFn k_luma_h[4] = {nullptr, luma_h_sse2<1>, luma_h_sse2<2>, luma_h_sse2<3>};
const FiltFn k_luma_v[4] = {nullptr, luma_vp_sse2<1>, luma_vp_sse2<2>, luma_vp_sse2<3>};
const Filt16Fn k_luma_v16[4] = {nullptr, luma_v16_sse2<1>, luma_v16_sse2<2>, luma_v16_sse2<3>};
const FiltFn k_chroma_h[8] = {nullptr, chroma_h_sse2<1>, chroma_h_sse2<2>, chroma_h_sse2<3>, chroma_h_sse2<4>, chroma_h_sse2<5>, chroma_h_sse2<6>, chroma_h_sse2<7>};
const FiltFn k_chroma_v[8] = {nullptr, chroma_vp_sse2<1>, chroma_vp_sse2<2>, chroma_vp_sse2<3>, chroma_vp_sse2<4>, chroma_vp_sse2<5>, chroma_vp_sse2<6>, chroma_vp_sse2<7>};
const Filt16Fn k_chroma_v16[8] = {nullptr, chroma_v16_sse2<1>, chroma_v16_sse2<2>, chroma_v16_sse2<3>, chroma_v16_sse2<4>, chroma_v16_sse2<5>, chroma_v16_sse2<6>, chroma_v16_sse2<7>};
#else
const FiltFn k_luma_h[4] = {nullptr, luma_h<1>, luma_h<2>, luma_h<3>};
const FiltFn k_luma_v[4] = {nullptr, luma_v<1>, luma_v<2>, luma_v<3>};
const Filt16Fn k_luma_v16[4] = {nullptr, luma_v16<1>, luma_v16<2>, luma_v16<3>};
const FiltFn k_chroma_h[8] = {nullptr, chroma_h<1>, chroma_h<2>, chroma_h<3>, chroma_h<4>, chroma_h<5>, chroma_h<6>, chroma_h<7>};
const FiltFn k_chroma_v[8] = {nullptr, chroma_v<1>, chroma_v<2>, chroma_v<3>, chroma_v<4>, chroma_v<5>, chroma_v<6>, chroma_v<7>};
const Filt16Fn k_chroma_v16[8] = {nullptr, chroma_v16<1>, chroma_v16<2>, chroma_v16<3>, chroma_v16<4>, chroma_v16<5>, chroma_v16<6>, chroma_v16<7>};
#endif // FIZMO_HEVC_SSE2

} // namespace

void PicDecoder::mc_luma(const Picture* ref, int x, int y, int w, int h, Mv mv, std::int16_t* dst) {
    int xi = x + (mv.x >> 2);
    int yi = y + (mv.y >> 2);
    const int fx = mv.x & 3;
    const int fy = mv.y & 3;
    const int pw = ref->width[0];
    const int ph = ref->height[0];
    if (xi < -(w + 3)) xi = -(w + 3);
    if (xi > pw + 2) xi = pw + 2;
    if (yi < -(h + 3)) yi = -(h + 3);
    if (yi > ph + 2) yi = ph + 2;
    const std::ptrdiff_t s = ref->stride[0];
    const std::uint16_t* src = ref->plane[0] + yi * s + xi;
    const int bd = m_bd_y;
    const int shift1 = bd - 8 < 4 ? bd - 8 : 4;
    if (!fx && !fy) {
        copy_shift(src, s, dst, w, h, 14 - bd);
    } else if (!fy) {
        k_luma_h[fx](src, s, dst, w, h, shift1);
    } else if (!fx) {
        k_luma_v[fy](src, s, dst, w, h, shift1);
    } else {
        k_luma_h[fx](src - 3 * s, s, m_mc_tmp, w, h + 7, shift1);
        k_luma_v16[fy](m_mc_tmp, dst, w, h);
    }
}

void PicDecoder::mc_chroma(const Picture* ref, int cidx, int x, int y, int w, int h, Mv mv, std::int16_t* dst) {
    const int mvx = mv.x * 2 / m_sps->sub_width_c;
    const int mvy = mv.y * 2 / m_sps->sub_height_c;
    int xi = x + (mvx >> 3);
    int yi = y + (mvy >> 3);
    const int fx = mvx & 7;
    const int fy = mvy & 7;
    const int pw = ref->width[cidx];
    const int ph = ref->height[cidx];
    if (xi < -(w + 2)) xi = -(w + 2);
    if (xi > pw + 1) xi = pw + 1;
    if (yi < -(h + 2)) yi = -(h + 2);
    if (yi > ph + 1) yi = ph + 1;
    const std::ptrdiff_t s = ref->stride[cidx];
    const std::uint16_t* src = ref->plane[cidx] + yi * s + xi;
    const int bd = m_bd_c;
    const int shift1 = bd - 8 < 4 ? bd - 8 : 4;
    if (!fx && !fy) {
        copy_shift(src, s, dst, w, h, 14 - bd);
    } else if (!fy) {
        k_chroma_h[fx](src, s, dst, w, h, shift1);
    } else if (!fx) {
        k_chroma_v[fy](src, s, dst, w, h, shift1);
    } else {
        k_chroma_h[fx](src - s, s, m_mc_tmp, w, h + 3, shift1);
        k_chroma_v16[fy](m_mc_tmp, dst, w, h);
    }
}

void PicDecoder::motion_compensate(int x, int y, int w, int h, const PuMotion& m) {
    const bool weighted = (m_slice_type == SLICE_P && m_pps->weighted_pred) || (m_slice_type == SLICE_B && m_pps->weighted_bipred);
    const int ncomp = m_chroma != 0 ? 3 : 1;
    const bool bi = m.pred == 3;
    const int l_uni = (m.pred & 1) ? 0 : 1;
    for (int c = 0; c < ncomp; ++c) {
        const int cx = c ? x >> m_sx : x;
        const int cy = c ? y >> m_sy : y;
        const int cw = c ? w >> m_sx : w;
        const int chh = c ? h >> m_sy : h;
        for (int l = 0; l < 2; ++l) {
            if (!(m.pred & (1 << l))) continue;
            const Picture* ref = m_refs[l][m.ref_idx[l]];
            if (c == 0) {
                mc_luma(ref, cx, cy, cw, chh, m.mv[l], m_pred[l]);
            } else {
                mc_chroma(ref, c, cx, cy, cw, chh, m.mv[l], m_pred[l]);
            }
        }
        const int bd = c ? m_bd_c : m_bd_y;
        const int maxv = (1 << bd) - 1;
        std::uint16_t* dst = m_pic->plane[c] + cy * m_pic->stride[c] + cx;
        const std::ptrdiff_t ds = m_pic->stride[c];
        const int shift1 = 14 - bd;
        if (!weighted) {
            if (bi) {
                const int shift2 = 15 - bd;
                const int off2 = 1 << (shift2 - 1);
                for (int j = 0; j < chh; ++j) {
                    const std::int16_t* a = m_pred[0] + j * cw;
                    const std::int16_t* b = m_pred[1] + j * cw;
                    std::uint16_t* d = dst + j * ds;
                    int i0 = 0;
#if defined(FIZMO_HEVC_SSE2)
                    const __m128i one = _mm_set1_epi16(1);
                    const __m128i offv = _mm_set1_epi32(off2);
                    const __m128i sh = _mm_cvtsi32_si128(shift2);
                    const __m128i vmax = _mm_set1_epi16(static_cast<short>(maxv));
                    const __m128i zero = _mm_setzero_si128();
                    for (; i0 + 8 <= cw; i0 += 8) {
                        const __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i0));
                        const __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i0));
                        const __m128i lo = _mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpacklo_epi16(va, vb), one), offv), sh);
                        const __m128i hi = _mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(_mm_unpackhi_epi16(va, vb), one), offv), sh);
                        const __m128i r = _mm_min_epi16(_mm_max_epi16(_mm_packs_epi32(lo, hi), zero), vmax);
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i0), r);
                    }
#endif // FIZMO_HEVC_SSE2
                    for (int i = i0; i < cw; ++i) {
                        const int v = (a[i] + b[i] + off2) >> shift2;
                        d[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
                    }
                }
            } else {
                const int off1 = 1 << (shift1 - 1);
                for (int j = 0; j < chh; ++j) {
                    const std::int16_t* a = m_pred[l_uni] + j * cw;
                    std::uint16_t* d = dst + j * ds;
                    int i0 = 0;
#if defined(FIZMO_HEVC_SSE2)
                    const __m128i offv = _mm_set1_epi16(static_cast<short>(off1));
                    const __m128i sh = _mm_cvtsi32_si128(shift1);
                    const __m128i vmax = _mm_set1_epi16(static_cast<short>(maxv));
                    const __m128i zero = _mm_setzero_si128();
                    for (; i0 + 8 <= cw; i0 += 8) {
                        const __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i0));
                        const __m128i r = _mm_min_epi16(_mm_max_epi16(_mm_sra_epi16(_mm_adds_epi16(va, offv), sh), zero), vmax);
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(d + i0), r);
                    }
#endif // FIZMO_HEVC_SSE2
                    for (int i = i0; i < cw; ++i) {
                        const int v = (a[i] + off1) >> shift1;
                        d[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
                    }
                }
            }
            continue;
        }
        int wt[2];
        int of[2];
        const int log2wd = (c == 0 ? m_sh->luma_log2_wd : m_sh->chroma_log2_wd) + shift1;
        for (int l = 0; l < 2; ++l) {
            if (!(m.pred & (1 << l))) {
                wt[l] = 0;
                of[l] = 0;
                continue;
            }
            const int r = m.ref_idx[l];
            if (c == 0) {
                wt[l] = m_sh->luma_w[l][r];
                of[l] = m_sh->luma_o[l][r];
            } else {
                wt[l] = m_sh->chroma_w[l][r][c - 1];
                of[l] = m_sh->chroma_o[l][r][c - 1];
            }
        }
        if (bi) {
            const std::int64_t rnd = static_cast<std::int64_t>(of[0] + of[1] + 1) * (static_cast<std::int64_t>(1) << log2wd);
            for (int j = 0; j < chh; ++j) {
                const std::int16_t* a = m_pred[0] + j * cw;
                const std::int16_t* b = m_pred[1] + j * cw;
                std::uint16_t* d = dst + j * ds;
                for (int i = 0; i < cw; ++i) {
                    const std::int64_t v = (static_cast<std::int64_t>(a[i]) * wt[0] + static_cast<std::int64_t>(b[i]) * wt[1] + rnd) >> (log2wd + 1);
                    d[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
                }
            }
        } else {
            const int w0 = wt[l_uni];
            const int o0 = of[l_uni];
            for (int j = 0; j < chh; ++j) {
                const std::int16_t* a = m_pred[l_uni] + j * cw;
                std::uint16_t* d = dst + j * ds;
                for (int i = 0; i < cw; ++i) {
                    std::int64_t v;
                    if (log2wd >= 1) {
                        v = ((static_cast<std::int64_t>(a[i]) * w0 + (static_cast<std::int64_t>(1) << (log2wd - 1))) >> log2wd) + o0;
                    } else {
                        v = static_cast<std::int64_t>(a[i]) * w0 + o0;
                    }
                    d[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
                }
            }
        }
    }
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
