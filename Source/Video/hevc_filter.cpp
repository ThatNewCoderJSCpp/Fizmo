#include "hevc_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

inline int chroma_qp_for_deblock(int qpi, int chroma_array_type) noexcept {
    if (chroma_array_type != 1) return qpi < 51 ? qpi : 51;
    if (qpi < 30) return qpi;
    if (qpi >= 70) return qpi - 6;
    return k_chroma_qp_table[qpi];
}

inline int clipv(int v, int maxv) noexcept {
    return v < 0 ? 0 : (v > maxv ? maxv : v);
}

void filter_luma_segment(std::uint16_t* pix, std::ptrdiff_t xs, std::ptrdiff_t ls, int beta, int tc, int maxv, bool nop, bool noq) {
    auto P = [&](int i, int k) -> int { return pix[k * ls - (i + 1) * xs]; };
    auto Q = [&](int i, int k) -> int { return pix[k * ls + i * xs]; };
    const int dp0 = std::abs(P(2, 0) - 2 * P(1, 0) + P(0, 0));
    const int dp3 = std::abs(P(2, 3) - 2 * P(1, 3) + P(0, 3));
    const int dq0 = std::abs(Q(2, 0) - 2 * Q(1, 0) + Q(0, 0));
    const int dq3 = std::abs(Q(2, 3) - 2 * Q(1, 3) + Q(0, 3));
    const int dpq0 = dp0 + dq0;
    const int dpq3 = dp3 + dq3;
    const int dp = dp0 + dp3;
    const int dq = dq0 + dq3;
    const int d = dpq0 + dpq3;
    if (d >= beta) return;
    auto dsam = [&](int k, int dpq) {
        return 2 * dpq < (beta >> 2) && std::abs(P(3, k) - P(0, k)) + std::abs(Q(0, k) - Q(3, k)) < (beta >> 3) && std::abs(P(0, k) - Q(0, k)) < ((5 * tc + 1) >> 1);
    };
    const bool strong = dsam(0, dpq0) && dsam(3, dpq3);
    const int side = (beta + (beta >> 1)) >> 3;
    const bool dep = dp < side;
    const bool deq = dq < side;
    for (int k = 0; k < 4; ++k) {
        std::uint16_t* q = pix + k * ls;
        const int p0 = q[-xs];
        const int p1 = q[-2 * xs];
        const int p2 = q[-3 * xs];
        const int p3 = q[-4 * xs];
        const int q0 = q[0];
        const int q1 = q[xs];
        const int q2 = q[2 * xs];
        const int q3 = q[3 * xs];
        if (strong) {
            const int t2 = 2 * tc;
            if (!nop) {
                q[-xs] = static_cast<std::uint16_t>(clip3(p0 - t2, p0 + t2, (p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3));
                q[-2 * xs] = static_cast<std::uint16_t>(clip3(p1 - t2, p1 + t2, (p2 + p1 + p0 + q0 + 2) >> 2));
                q[-3 * xs] = static_cast<std::uint16_t>(clip3(p2 - t2, p2 + t2, (2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3));
            }
            if (!noq) {
                q[0] = static_cast<std::uint16_t>(clip3(q0 - t2, q0 + t2, (p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3));
                q[xs] = static_cast<std::uint16_t>(clip3(q1 - t2, q1 + t2, (p0 + q0 + q1 + q2 + 2) >> 2));
                q[2 * xs] = static_cast<std::uint16_t>(clip3(q2 - t2, q2 + t2, (p0 + q0 + q1 + 3 * q2 + 2 * q3 + 4) >> 3));
            }
        } else {
            int delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
            if (std::abs(delta) >= tc * 10) continue;
            delta = clip3(-tc, tc, delta);
            if (!nop) {
                q[-xs] = static_cast<std::uint16_t>(clipv(p0 + delta, maxv));
                if (dep) {
                    const int dl = clip3(-(tc >> 1), tc >> 1, (((p2 + p0 + 1) >> 1) - p1 + delta) >> 1);
                    q[-2 * xs] = static_cast<std::uint16_t>(clipv(p1 + dl, maxv));
                }
            }
            if (!noq) {
                q[0] = static_cast<std::uint16_t>(clipv(q0 - delta, maxv));
                if (deq) {
                    const int dl = clip3(-(tc >> 1), tc >> 1, (((q2 + q0 + 1) >> 1) - q1 - delta) >> 1);
                    q[xs] = static_cast<std::uint16_t>(clipv(q1 + dl, maxv));
                }
            }
        }
    }
}

} // namespace

void PicDecoder::mark_tu_edges(int x0, int y0, int size) {
    const int pw = m_sps->pic_w_ctb;
    if ((x0 & 7) == 0 && x0 > 0) {
        bool ok = true;
        if ((x0 & (m_ctb_size - 1)) == 0) {
            const int nb = m_ctb_rs - 1;
            if (m_ctb_slice_addr[static_cast<std::size_t>(nb)] != m_slice_addr && !m_sh->lf_across_slices) ok = false;
            if (!m_pps->loop_filter_across_tiles &&
                m_lay->tile_id[static_cast<std::size_t>(m_lay->rs_to_ts[static_cast<std::size_t>(nb)])] != m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)]) ok = false;
        }
        if (ok) {
            const int ye = std::min(y0 + size, m_h);
            for (int y = y0; y < ye; y += 4) m_edge_v[idx4(x0, y)] |= 1;
        }
    }
    if ((y0 & 7) == 0 && y0 > 0) {
        bool ok = true;
        if ((y0 & (m_ctb_size - 1)) == 0) {
            const int nb = m_ctb_rs - pw;
            if (m_ctb_slice_addr[static_cast<std::size_t>(nb)] != m_slice_addr && !m_sh->lf_across_slices) ok = false;
            if (!m_pps->loop_filter_across_tiles &&
                m_lay->tile_id[static_cast<std::size_t>(m_lay->rs_to_ts[static_cast<std::size_t>(nb)])] != m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)]) ok = false;
        }
        if (ok) {
            const int xe = std::min(x0 + size, m_w);
            for (int x = x0; x < xe; x += 4) m_edge_h[idx4(x, y0)] |= 1;
        }
    }
}

void PicDecoder::mark_pu_edges(int x0, int y0, int w, int h) {
    if (w == 0) {
        if (x0 & 7) return;
        const int ye = std::min(y0 + h, m_h);
        for (int y = y0; y < ye; y += 4) m_edge_v[idx4(x0, y)] |= 2;
    } else {
        if (y0 & 7) return;
        const int xe = std::min(x0 + w, m_w);
        for (int x = x0; x < xe; x += 4) m_edge_h[idx4(x, y0)] |= 2;
    }
}

int PicDecoder::compute_bs(int xq, int yq, int xp, int yp, bool tu_edge) const {
    const std::size_t iq = idx4(xq, yq);
    const std::size_t ip = idx4(xp, yp);
    if ((m_cu_flags[iq] | m_cu_flags[ip]) & CU_INTRA) return 2;
    if (tu_edge && (m_nz[iq] || m_nz[ip])) return 1;
    const MvField& fq = m_pic->mvf[iq];
    const MvField& fp = m_pic->mvf[ip];
    const int pw = m_sps->pic_w_ctb;
    const SliceRefs& sq = m_pic->slices[m_ctb_slice_idx[static_cast<std::size_t>((yq >> m_log2_ctb) * pw + (xq >> m_log2_ctb))]];
    const SliceRefs& sp = m_pic->slices[m_ctb_slice_idx[static_cast<std::size_t>((yp >> m_log2_ctb) * pw + (xp >> m_log2_ctb))]];
    const int nq = (fq.pred & 1) + ((fq.pred >> 1) & 1);
    const int np = (fp.pred & 1) + ((fp.pred >> 1) & 1);
    if (nq != np) return 1;
    auto apart = [](const std::int16_t* a, const std::int16_t* b) { return std::abs(a[0] - b[0]) >= 4 || std::abs(a[1] - b[1]) >= 4; };
    if (nq == 1) {
        const int lq = (fq.pred & 1) ? 0 : 1;
        const int lp = (fp.pred & 1) ? 0 : 1;
        if (sq.uid[lq][fq.ref_idx[lq]] != sp.uid[lp][fp.ref_idx[lp]]) return 1;
        return apart(fq.mv[lq], fp.mv[lp]) ? 1 : 0;
    }
    const std::uint32_t q0 = sq.uid[0][fq.ref_idx[0]];
    const std::uint32_t q1 = sq.uid[1][fq.ref_idx[1]];
    const std::uint32_t p0 = sp.uid[0][fp.ref_idx[0]];
    const std::uint32_t p1 = sp.uid[1][fp.ref_idx[1]];
    if (!((q0 == p0 && q1 == p1) || (q0 == p1 && q1 == p0))) return 1;
    if (q0 != q1) {
        if (q0 == p0) return (apart(fq.mv[0], fp.mv[0]) || apart(fq.mv[1], fp.mv[1])) ? 1 : 0;
        return (apart(fq.mv[0], fp.mv[1]) || apart(fq.mv[1], fp.mv[0])) ? 1 : 0;
    }
    const bool a = apart(fq.mv[0], fp.mv[0]) || apart(fq.mv[1], fp.mv[1]);
    const bool b = apart(fq.mv[0], fp.mv[1]) || apart(fq.mv[1], fp.mv[0]);
    return (a && b) ? 1 : 0;
}

void PicDecoder::deblock_edges(bool vertical) {
    const std::vector<std::uint8_t>& edges = vertical ? m_edge_v : m_edge_h;
    std::vector<std::uint8_t>& bs = m_bs;
    bs.assign(edges.size(), 0);
    const int pw = m_sps->pic_w_ctb;
    std::uint16_t* plane = m_pic->plane[0];
    const std::ptrdiff_t s = m_pic->stride[0];
    const std::ptrdiff_t xs = vertical ? 1 : s;
    const std::ptrdiff_t ls = vertical ? s : 1;
    const int maxv = (1 << m_bd_y) - 1;
    const int bscale = 1 << (m_bd_y - 8);
    for (int y = 0; y < m_h; y += 4) {
        for (int x = 0; x < m_w; x += 4) {
            if (vertical ? (x & 7) : (y & 7)) continue;
            const std::size_t i = idx4(x, y);
            const std::uint8_t e = edges[i];
            if (!e) continue;
            const int xp = vertical ? x - 1 : x;
            const int yp = vertical ? y : y - 1;
            const int b = compute_bs(x, y, xp, yp, (e & 1) != 0);
            bs[i] = static_cast<std::uint8_t>(b);
            if (!b) continue;
            const std::size_t ip = idx4(xp, yp);
            const int qpl = (m_qp[i] + m_qp[ip] + 1) >> 1;
            const SliceParams& sp = m_slices[m_ctb_slice_idx[static_cast<std::size_t>((y >> m_log2_ctb) * pw + (x >> m_log2_ctb))]];
            const int beta = k_beta_table[clip3(0, 51, qpl + sp.beta_offset)] * bscale;
            const int tc = k_tc_table[clip3(0, 53, qpl + 2 * (b - 1) + sp.tc_offset)] * bscale;
            if (tc == 0 || beta == 0) continue;
            const bool nop = (m_cu_flags[ip] & CU_NOFILTER) != 0;
            const bool noq = (m_cu_flags[i] & CU_NOFILTER) != 0;
            filter_luma_segment(plane + y * s + x, xs, ls, beta, tc, maxv, nop, noq);
        }
    }
    if (m_chroma == 0) return;
    const int cw = m_pic->width[1];
    const int ch = m_pic->height[1];
    const int maxc = (1 << m_bd_c) - 1;
    const int cscale = 1 << (m_bd_c - 8);
    for (int c = 1; c < 3; ++c) {
        std::uint16_t* cp = m_pic->plane[c];
        const std::ptrdiff_t cs = m_pic->stride[c];
        const std::ptrdiff_t cxs = vertical ? 1 : cs;
        const int pic_off = c == 1 ? m_pps->cb_qp_offset : m_pps->cr_qp_offset;
        if (vertical) {
            for (int xc = 8; xc < cw; xc += 8) {
                const int xl = xc << m_sx;
                for (int yc = 0; yc < ch; ++yc) {
                    const int yl = (yc & ~3) << m_sy;
                    const std::size_t i = idx4(xl, yl);
                    if (bs[i] != 2) continue;
                    const std::size_t ip = idx4(xl - 1, yl);
                    const int qpi = ((m_qp[i] + m_qp[ip] + 1) >> 1) + pic_off;
                    const int qpc = chroma_qp_for_deblock(qpi, m_chroma);
                    const SliceParams& sp = m_slices[m_ctb_slice_idx[static_cast<std::size_t>((yl >> m_log2_ctb) * pw + (xl >> m_log2_ctb))]];
                    const int tc = k_tc_table[clip3(0, 53, qpc + 2 + sp.tc_offset)] * cscale;
                    if (!tc) continue;
                    std::uint16_t* q = cp + yc * cs + xc;
                    const int p0 = q[-cxs];
                    const int p1 = q[-2 * cxs];
                    const int q0 = q[0];
                    const int q1 = q[cxs];
                    const int delta = clip3(-tc, tc, ((((q0 - p0) * 4) + p1 - q1 + 4) >> 3));
                    if (!(m_cu_flags[ip] & CU_NOFILTER)) q[-cxs] = static_cast<std::uint16_t>(clipv(p0 + delta, maxc));
                    if (!(m_cu_flags[i] & CU_NOFILTER)) q[0] = static_cast<std::uint16_t>(clipv(q0 - delta, maxc));
                }
            }
        } else {
            for (int yc = 8; yc < ch; yc += 8) {
                const int yl = yc << m_sy;
                for (int xc = 0; xc < cw; ++xc) {
                    const int xl = (xc & ~3) << m_sx;
                    const std::size_t i = idx4(xl, yl);
                    if (bs[i] != 2) continue;
                    const std::size_t ip = idx4(xl, yl - 1);
                    const int qpi = ((m_qp[i] + m_qp[ip] + 1) >> 1) + pic_off;
                    const int qpc = chroma_qp_for_deblock(qpi, m_chroma);
                    const SliceParams& sp = m_slices[m_ctb_slice_idx[static_cast<std::size_t>((yl >> m_log2_ctb) * pw + (xl >> m_log2_ctb))]];
                    const int tc = k_tc_table[clip3(0, 53, qpc + 2 + sp.tc_offset)] * cscale;
                    if (!tc) continue;
                    std::uint16_t* q = cp + yc * cs + xc;
                    const int p0 = q[-cxs];
                    const int p1 = q[-2 * cxs];
                    const int q0 = q[0];
                    const int q1 = q[cxs];
                    const int delta = clip3(-tc, tc, ((((q0 - p0) * 4) + p1 - q1 + 4) >> 3));
                    if (!(m_cu_flags[ip] & CU_NOFILTER)) q[-cxs] = static_cast<std::uint16_t>(clipv(p0 + delta, maxc));
                    if (!(m_cu_flags[i] & CU_NOFILTER)) q[0] = static_cast<std::uint16_t>(clipv(q0 - delta, maxc));
                }
            }
        }
    }
}

void PicDecoder::deblock() {
    if (!m_any_deblock) return;
    deblock_edges(true);
    deblock_edges(false);
}

void PicDecoder::sao_ctb(int cidx, int rx, int ry, const std::uint16_t* src, std::ptrdiff_t sstride) {
    const int rs = ry * m_sps->pic_w_ctb + rx;
    const SaoParams& p = m_sao[static_cast<std::size_t>(rs)];
    const int type = p.type[cidx];
    if (!type) return;
    const SliceParams& slp = m_slices[m_ctb_slice_idx[static_cast<std::size_t>(rs)]];
    if (cidx == 0 ? !slp.sao_luma : !slp.sao_chroma) return;
    const int sx = cidx ? m_sx : 0;
    const int sy = cidx ? m_sy : 0;
    const int pw = m_pic->width[cidx];
    const int ph = m_pic->height[cidx];
    const int cwid = m_ctb_size >> sx;
    const int chei = m_ctb_size >> sy;
    const int x0 = rx * cwid;
    const int y0 = ry * chei;
    const int w = std::min(cwid, pw - x0);
    const int h = std::min(chei, ph - y0);
    if (w <= 0 || h <= 0) return;
    const int bd = cidx ? m_bd_c : m_bd_y;
    const int maxv = (1 << bd) - 1;
    std::uint16_t* dst = m_pic->plane[cidx];
    const std::ptrdiff_t ds = m_pic->stride[cidx];
    const std::int16_t* off = p.offset[cidx];
    const bool check_nf = m_any_nofilter;
    auto nofilter = [&](int x, int y) { return (m_cu_flags[idx4(x << sx, y << sy)] & CU_NOFILTER) != 0; };
    if (type == 1) {
        int table[32];
        std::memset(table, 0, sizeof(table));
        for (int k = 0; k < 4; ++k) table[(k + p.band_pos[cidx]) & 31] = k + 1;
        const int shift = bd - 5;
        for (int y = y0; y < y0 + h; ++y) {
            const std::uint16_t* sr = src + y * sstride;
            std::uint16_t* dr = dst + y * ds;
            for (int x = x0; x < x0 + w; ++x) {
                if (check_nf && nofilter(x, y)) continue;
                const int v = sr[x];
                dr[x] = static_cast<std::uint16_t>(clipv(v + off[table[v >> shift]], maxv));
            }
        }
        return;
    }
    static const int dxs[4][2] = {{-1, 1}, {0, 0}, {-1, 1}, {1, -1}};
    static const int dys[4][2] = {{0, 0}, {-1, 1}, {-1, 1}, {-1, 1}};
    const int cls = p.eo_class[cidx];
    const int ctbw = m_sps->pic_w_ctb;
    const int ctbh = m_sps->pic_h_ctb;
    bool nb_ok[3][3];
    const int cur_ts = m_lay->rs_to_ts[static_cast<std::size_t>(rs)];
    const int cur_tile = m_lay->tile_id[static_cast<std::size_t>(cur_ts)];
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            bool ok = true;
            const int nx = rx + dx;
            const int ny = ry + dy;
            if (nx < 0 || ny < 0 || nx >= ctbw || ny >= ctbh) {
                ok = false;
            } else if (dx || dy) {
                const int nrs = ny * ctbw + nx;
                const int nts = m_lay->rs_to_ts[static_cast<std::size_t>(nrs)];
                if (m_ctb_slice_addr[static_cast<std::size_t>(nrs)] != m_ctb_slice_addr[static_cast<std::size_t>(rs)]) {
                    const bool flag = nts < cur_ts ? slp.lf_across_slices : m_slices[m_ctb_slice_idx[static_cast<std::size_t>(nrs)]].lf_across_slices;
                    if (!flag) ok = false;
                }
                if (!m_pps->loop_filter_across_tiles && m_lay->tile_id[static_cast<std::size_t>(nts)] != cur_tile) ok = false;
            }
            nb_ok[dy + 1][dx + 1] = ok;
        }
    }
    const int ax = dxs[cls][0];
    const int ay = dys[cls][0];
    const int bx = dxs[cls][1];
    const int by = dys[cls][1];
    int omap[5];
    omap[0] = off[1];
    omap[1] = off[2];
    omap[2] = 0;
    omap[3] = off[3];
    omap[4] = off[4];
    auto one = [&](const std::uint16_t* sr, const std::uint16_t* sa, const std::uint16_t* sb, std::uint16_t* dr, int x, int cya, int cyb) {
        const int xa = x + ax;
        const int xb = x + bx;
        if (xa < 0 || xb < 0 || xa >= pw || xb >= pw) return;
        const int cxa = xa < x0 ? 0 : (xa >= x0 + cwid ? 2 : 1);
        const int cxb = xb < x0 ? 0 : (xb >= x0 + cwid ? 2 : 1);
        if (!nb_ok[cya][cxa] || !nb_ok[cyb][cxb]) return;
        const int v = sr[x];
        const int a = sa[xa];
        const int b = sb[xb];
        const int e = 2 + ((v > a) - (v < a)) + ((v > b) - (v < b));
        dr[x] = static_cast<std::uint16_t>(clipv(v + omap[e], maxv));
    };
    for (int y = y0; y < y0 + h; ++y) {
        const int yay = y + ay;
        const int yby = y + by;
        if (yay < 0 || yby < 0 || yay >= ph || yby >= ph) continue;
        const int cya = yay < y0 ? 0 : (yay >= y0 + chei ? 2 : 1);
        const int cyb = yby < y0 ? 0 : (yby >= y0 + chei ? 2 : 1);
        const std::uint16_t* sr = src + y * sstride;
        const std::uint16_t* sa = src + yay * sstride;
        const std::uint16_t* sb = src + yby * sstride;
        std::uint16_t* dr = dst + y * ds;
        const int xs = (ax != 0 || bx != 0) ? x0 + 1 : x0;
        const int xe = (ax != 0 || bx != 0) ? x0 + w - 1 : x0 + w;
        if (xs > x0) {
            if (!(check_nf && nofilter(x0, y))) one(sr, sa, sb, dr, x0, cya, cyb);
        }
        if (nb_ok[cya][1] && nb_ok[cyb][1] && xe > xs) {
            const std::uint16_t* pa = sa + ax;
            const std::uint16_t* pb = sb + bx;
            if (check_nf) {
                for (int x = xs; x < xe; ++x) {
                    if (nofilter(x, y)) continue;
                    const int v = sr[x];
                    const int e = 2 + ((v > pa[x]) - (v < pa[x])) + ((v > pb[x]) - (v < pb[x]));
                    dr[x] = static_cast<std::uint16_t>(clipv(v + omap[e], maxv));
                }
            } else {
                for (int x = xs; x < xe; ++x) {
                    const int v = sr[x];
                    const int e = 2 + ((v > pa[x]) - (v < pa[x])) + ((v > pb[x]) - (v < pb[x]));
                    dr[x] = static_cast<std::uint16_t>(clipv(v + omap[e], maxv));
                }
            }
        }
        if (xe < x0 + w && w > 1) {
            if (!(check_nf && nofilter(x0 + w - 1, y))) one(sr, sa, sb, dr, x0 + w - 1, cya, cyb);
        }
    }
}

void PicDecoder::sao_apply() {
    if (!m_any_sao) return;
    const int ncomp = m_chroma != 0 ? 3 : 1;
    const int ctbw = m_sps->pic_w_ctb;
    const int ctbh = m_sps->pic_h_ctb;
    for (int c = 0; c < ncomp; ++c) {
        bool any = false;
        for (std::size_t i = 0; i < m_sao.size() && !any; ++i) {
            if (m_sao[i].type[c]) any = true;
        }
        if (!any) continue;
        const int pw = m_pic->width[c];
        const int ph = m_pic->height[c];
        std::vector<std::uint16_t>& buf = m_sao_buf[c];
        buf.resize(static_cast<std::size_t>(pw) * static_cast<std::size_t>(ph));
        for (int y = 0; y < ph; ++y) std::memcpy(buf.data() + static_cast<std::size_t>(y) * pw, m_pic->plane[c] + y * m_pic->stride[c], sizeof(std::uint16_t) * static_cast<std::size_t>(pw));
        for (int ry = 0; ry < ctbh; ++ry) {
            for (int rx = 0; rx < ctbw; ++rx) sao_ctb(c, rx, ry, buf.data(), pw);
        }
    }
}

void PicDecoder::finish() {
    deblock();
    sao_apply();
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
