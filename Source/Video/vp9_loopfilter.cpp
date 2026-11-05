#include "vp9_internal.hpp"

#include <thread>

namespace fizmo {
namespace video {
namespace vp9_detail {

void build_lf_thresholds(LfThresholds& t, int sharpness) noexcept {
    for (int lvl = 0; lvl <= kMaxLoopFilter; ++lvl) {
        int limit = lvl >> ((sharpness > 0) + (sharpness > 4));
        if (sharpness > 0 && limit > 9 - sharpness) limit = 9 - sharpness;
        if (limit < 1) limit = 1;
        t.lim[lvl] = static_cast<std::uint8_t>(limit);
        t.mblim[lvl] = static_cast<std::uint8_t>(2 * (lvl + 2) + limit);
        t.hev_thr[lvl] = static_cast<std::uint8_t>(lvl >> 4);
    }
}

namespace {

inline int iabs(int v) noexcept { return v < 0 ? -v : v; }

struct EdgeParams {
    int blimit;
    int limit;
    int thresh;
    int flat;
    int shift;
    int smin;
    int smax;
    int offset;
};

inline int sclamp(int v, const EdgeParams& e) noexcept { return v < e.smin ? e.smin : (v > e.smax ? e.smax : v); }

inline void filter4(std::uint16_t* s, std::ptrdiff_t p, bool hev, const EdgeParams& e) noexcept {
    const int ps1 = s[-2 * p] - e.offset;
    const int ps0 = s[-p] - e.offset;
    const int qs0 = s[0] - e.offset;
    const int qs1 = s[p] - e.offset;
    int f = hev ? sclamp(ps1 - qs1, e) : 0;
    f = sclamp(f + 3 * (qs0 - ps0), e);
    const int f1 = sclamp(f + 4, e) >> 3;
    const int f2 = sclamp(f + 3, e) >> 3;
    s[0] = static_cast<std::uint16_t>(sclamp(qs0 - f1, e) + e.offset);
    s[-p] = static_cast<std::uint16_t>(sclamp(ps0 + f2, e) + e.offset);
    if (!hev) {
        const int f3 = (f1 + 1) >> 1;
        s[p] = static_cast<std::uint16_t>(sclamp(qs1 - f3, e) + e.offset);
        s[-2 * p] = static_cast<std::uint16_t>(sclamp(ps1 + f3, e) + e.offset);
    }
}

inline bool filter_mask(const std::uint16_t* s, std::ptrdiff_t p, const EdgeParams& e) noexcept {
    const int p3 = s[-4 * p];
    const int p2 = s[-3 * p];
    const int p1 = s[-2 * p];
    const int p0 = s[-p];
    const int q0 = s[0];
    const int q1 = s[p];
    const int q2 = s[2 * p];
    const int q3 = s[3 * p];
    return iabs(p3 - p2) <= e.limit && iabs(p2 - p1) <= e.limit && iabs(p1 - p0) <= e.limit && iabs(q1 - q0) <= e.limit &&
           iabs(q2 - q1) <= e.limit && iabs(q3 - q2) <= e.limit && iabs(p0 - q0) * 2 + iabs(p1 - q1) / 2 <= e.blimit;
}

inline bool hev_mask(const std::uint16_t* s, std::ptrdiff_t p, const EdgeParams& e) noexcept {
    return iabs(s[-2 * p] - s[-p]) > e.thresh || iabs(s[p] - s[0]) > e.thresh;
}

inline bool flat_mask4(const std::uint16_t* s, std::ptrdiff_t p, const EdgeParams& e) noexcept {
    const int p0 = s[-p];
    const int q0 = s[0];
    return iabs(s[-2 * p] - p0) <= e.flat && iabs(s[p] - q0) <= e.flat && iabs(s[-3 * p] - p0) <= e.flat && iabs(s[2 * p] - q0) <= e.flat &&
           iabs(s[-4 * p] - p0) <= e.flat && iabs(s[3 * p] - q0) <= e.flat;
}

inline bool flat_mask_outer(const std::uint16_t* s, std::ptrdiff_t p, const EdgeParams& e) noexcept {
    const int p0 = s[-p];
    const int q0 = s[0];
    return iabs(s[-5 * p] - p0) <= e.flat && iabs(s[4 * p] - q0) <= e.flat && iabs(s[-6 * p] - p0) <= e.flat && iabs(s[5 * p] - q0) <= e.flat &&
           iabs(s[-7 * p] - p0) <= e.flat && iabs(s[6 * p] - q0) <= e.flat && iabs(s[-8 * p] - p0) <= e.flat && iabs(s[7 * p] - q0) <= e.flat;
}

void lpf4(std::uint16_t* s, std::ptrdiff_t p, std::ptrdiff_t step, const EdgeParams& e) noexcept {
    for (int i = 0; i < 8; ++i, s += step) {
        if (!filter_mask(s, p, e)) continue;
        filter4(s, p, hev_mask(s, p, e), e);
    }
}

inline void filter8_apply(std::uint16_t* s, std::ptrdiff_t p) noexcept {
    const int p3 = s[-4 * p];
    const int p2 = s[-3 * p];
    const int p1 = s[-2 * p];
    const int p0 = s[-p];
    const int q0 = s[0];
    const int q1 = s[p];
    const int q2 = s[2 * p];
    const int q3 = s[3 * p];
    s[-3 * p] = static_cast<std::uint16_t>((p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0 + 4) >> 3);
    s[-2 * p] = static_cast<std::uint16_t>((p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1 + 4) >> 3);
    s[-p] = static_cast<std::uint16_t>((p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2 + 4) >> 3);
    s[0] = static_cast<std::uint16_t>((p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3 + 4) >> 3);
    s[p] = static_cast<std::uint16_t>((p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3 + 4) >> 3);
    s[2 * p] = static_cast<std::uint16_t>((p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3 + 4) >> 3);
}

void lpf8(std::uint16_t* s, std::ptrdiff_t p, std::ptrdiff_t step, const EdgeParams& e) noexcept {
    for (int i = 0; i < 8; ++i, s += step) {
        if (!filter_mask(s, p, e)) continue;
        if (flat_mask4(s, p, e)) filter8_apply(s, p);
        else filter4(s, p, hev_mask(s, p, e), e);
    }
}

void lpf16(std::uint16_t* s, std::ptrdiff_t p, std::ptrdiff_t step, const EdgeParams& e) noexcept {
    for (int i = 0; i < 8; ++i, s += step) {
        if (!filter_mask(s, p, e)) continue;
        if (!flat_mask4(s, p, e)) {
            filter4(s, p, hev_mask(s, p, e), e);
            continue;
        }
        if (!flat_mask_outer(s, p, e)) {
            filter8_apply(s, p);
            continue;
        }
        const int p7 = s[-8 * p], p6 = s[-7 * p], p5 = s[-6 * p], p4 = s[-5 * p];
        const int p3 = s[-4 * p], p2 = s[-3 * p], p1 = s[-2 * p], p0 = s[-p];
        const int q0 = s[0], q1 = s[p], q2 = s[2 * p], q3 = s[3 * p];
        const int q4 = s[4 * p], q5 = s[5 * p], q6 = s[6 * p], q7 = s[7 * p];
        int sum = p7 * 7 + p6 * 2 + p5 + p4 + p3 + p2 + p1 + p0 + q0;
        s[-7 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q1 - p7 - p6 + p5;
        s[-6 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q2 - p7 - p5 + p4;
        s[-5 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q3 - p7 - p4 + p3;
        s[-4 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q4 - p7 - p3 + p2;
        s[-3 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q5 - p7 - p2 + p1;
        s[-2 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q6 - p7 - p1 + p0;
        s[-p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p7 - p0 + q0;
        s[0] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p6 - q0 + q1;
        s[p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p5 - q1 + q2;
        s[2 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p4 - q2 + q3;
        s[3 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p3 - q3 + q4;
        s[4 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p2 - q4 + q5;
        s[5 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
        sum += q7 - p1 - q5 + q6;
        s[6 * p] = static_cast<std::uint16_t>((sum + 8) >> 4);
    }
}

inline int uv_tx(int bsize, int tx_size, int ssx, int ssy) noexcept {
    if (bsize < kBlock8x8) return kTx4x4;
    const int w = (8 * k_num_8x8_w[bsize]) >> ssx;
    const int h = (8 * k_num_8x8_h[bsize]) >> ssy;
    const int m = imin(w, h);
    const int max_tx = m >= 32 ? kTx32x32 : (m >= 16 ? kTx16x16 : (m >= 8 ? kTx8x8 : kTx4x4));
    return imin(tx_size, max_tx);
}

struct PlaneFilter {
    Picture* pic;
    const ModeInfo* mi;
    int mi_stride;
    const std::uint8_t (*lf_level)[4][2];
    const LfThresholds* thr;
    int bd;

    EdgeParams params(int lvl) const noexcept {
        EdgeParams e;
        e.shift = bd - 8;
        e.blimit = thr->mblim[lvl] << e.shift;
        e.limit = thr->lim[lvl] << e.shift;
        e.thresh = thr->hev_thr[lvl] << e.shift;
        e.flat = 1 << e.shift;
        e.smin = -(128 << e.shift);
        e.smax = (128 << e.shift) - 1;
        e.offset = 128 << e.shift;
        return e;
    }

    void superblock(int plane, int mi_row, int mi_col) const noexcept {
        const int ssx = plane ? pic->ssx : 0;
        const int ssy = plane ? pic->ssy : 0;
        const int row_step = 1 << ssy;
        const int col_step = 1 << ssx;
        const int mi_rows = pic->mi_rows;
        const int mi_cols = pic->mi_cols;
        const std::ptrdiff_t stride = pic->stride[plane];
        std::uint16_t* base = pic->plane(plane) + ((mi_row * 8) >> ssy) * stride + ((mi_col * 8) >> ssx);
        unsigned m16[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        unsigned m8[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        unsigned m4[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        unsigned m4i[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        std::uint8_t lfl[8][8];
        std::memset(lfl, 0, sizeof(lfl));
        for (int r = 0; r < 8 && mi_row + r < mi_rows; r += row_step) {
            unsigned m16c = 0;
            unsigned m8c = 0;
            unsigned m4c = 0;
            for (int c = 0; c < 8 && mi_col + c < mi_cols; c += col_step) {
                const ModeInfo& m = mi[(mi_row + r) * mi_stride + mi_col + c];
                const int sb = m.sb_type;
                const bool skip_this = m.skip && m.ref_frame[0] > kIntraFrame;
                const bool edge_left = k_num_4x4_w[sb] > 1 ? !(c & (k_num_8x8_w[sb] - 1)) : true;
                const bool skip_c = skip_this && !edge_left;
                const bool edge_above = k_num_4x4_h[sb] > 1 ? !(r & (k_num_8x8_h[sb] - 1)) : true;
                const bool skip_r = skip_this && !edge_above;
                const int tx = plane ? uv_tx(sb, m.tx_size, ssx, ssy) : m.tx_size;
                const bool border_c = ssx && mi_col + c == mi_cols - 1;
                const bool border_r = ssy && mi_row + r == mi_rows - 1;
                const int ref = m.ref_frame[0] > 0 ? m.ref_frame[0] : 0;
                const int lvl = lf_level[m.segment_id & 7][ref & 3][k_mode_lf_lut[m.y_mode < 14 ? m.y_mode : 0]];
                lfl[r][c >> ssx] = static_cast<std::uint8_t>(lvl);
                if (!lvl) continue;
                const unsigned bit = 1u << (c >> ssx);
                const int cc = c >> ssx;
                const int rr = r >> ssy;
                if (tx == kTx32x32) {
                    if (!skip_c && (cc & 3) == 0) {
                        if (!border_c) m16c |= bit;
                        else m8c |= bit;
                    }
                    if (!skip_r && (rr & 3) == 0) {
                        if (!border_r) m16[r] |= bit;
                        else m8[r] |= bit;
                    }
                } else if (tx == kTx16x16) {
                    if (!skip_c && (cc & 1) == 0) {
                        if (!border_c) m16c |= bit;
                        else m8c |= bit;
                    }
                    if (!skip_r && (rr & 1) == 0) {
                        if (!border_r) m16[r] |= bit;
                        else m8[r] |= bit;
                    }
                } else {
                    if (!skip_c) {
                        if (tx == kTx8x8 || (cc & 3) == 0) m8c |= bit;
                        else m4c |= bit;
                    }
                    if (!skip_r) {
                        if (tx == kTx8x8 || (rr & 3) == 0) m8[r] |= bit;
                        else m4[r] |= bit;
                    }
                    if (!skip_this && tx < kTx8x8 && !border_c) m4i[r] |= bit;
                }
            }
            const unsigned border_mask = mi_col == 0 ? ~1u : ~0u;
            m16c &= border_mask;
            m8c &= border_mask;
            m4c &= border_mask;
            std::uint16_t* row = base + ((r * 8) >> ssy) * stride;
            const unsigned all = m16c | m8c | m4c | m4i[r];
            for (int i = 0; i < 8; ++i) {
                const unsigned bit = 1u << i;
                if (!(all & bit)) continue;
                const EdgeParams e = params(lfl[r][i]);
                std::uint16_t* s = row + i * 8;
                if (m16c & bit) lpf16(s, 1, stride, e);
                else if (m8c & bit) lpf8(s, 1, stride, e);
                else if (m4c & bit) lpf4(s, 1, stride, e);
                if (m4i[r] & bit) lpf4(s + 4, 1, stride, e);
            }
        }
        for (int r = 0; r < 8 && mi_row + r < mi_rows; r += row_step) {
            const bool border_r = ssy && mi_row + r == mi_rows - 1;
            const unsigned mi4 = border_r ? 0u : m4i[r];
            unsigned a16 = m16[r];
            unsigned a8 = m8[r];
            unsigned a4 = m4[r];
            if (mi_row + r == 0) a16 = a8 = a4 = 0;
            std::uint16_t* row = base + ((r * 8) >> ssy) * stride;
            const unsigned all = a16 | a8 | a4 | mi4;
            for (int i = 0; i < 8; ++i) {
                const unsigned bit = 1u << i;
                if (!(all & bit)) continue;
                const EdgeParams e = params(lfl[r][i]);
                std::uint16_t* s = row + i * 8;
                if (a16 & bit) {
                    lpf16(s, stride, 1, e);
                } else if (a8 & bit) {
                    lpf8(s, stride, 1, e);
                    if (mi4 & bit) lpf4(s + 4 * stride, stride, 1, e);
                } else if (a4 & bit) {
                    lpf4(s, stride, 1, e);
                    if (mi4 & bit) lpf4(s + 4 * stride, stride, 1, e);
                } else {
                    lpf4(s + 4 * stride, stride, 1, e);
                }
            }
        }
    }

    void run(int plane) const noexcept {
        for (int r = 0; r < pic->mi_rows; r += 8)
            for (int c = 0; c < pic->mi_cols; c += 8) superblock(plane, r, c);
    }
};

} // namespace

void loop_filter_frame(Picture& pic, const ModeInfo* mi, int mi_stride, const std::uint8_t (*lf_level)[4][2], int sharpness, int threads) {
    LfThresholds thr;
    build_lf_thresholds(thr, sharpness);
    PlaneFilter pf{&pic, mi, mi_stride, lf_level, &thr, pic.bit_depth};
    if (threads > 1) {
        std::thread worker;
        bool started = false;
        try {
            worker = std::thread([&pf]() {
                pf.run(1);
                pf.run(2);
            });
            started = true;
        } catch (...) {
            started = false;
        }
        pf.run(0);
        if (started) {
            worker.join();
        } else {
            pf.run(1);
            pf.run(2);
        }
        return;
    }
    for (int p = 0; p < 3; ++p) pf.run(p);
}

} // namespace vp9_detail
} // namespace video
} // namespace fizmo
