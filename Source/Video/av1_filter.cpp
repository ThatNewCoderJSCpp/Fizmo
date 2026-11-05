#include "av1_internal.hpp"

#include <algorithm>

#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define FIZMO_AV1_SSE2 1
#endif // __SSE2__

namespace fizmo {
namespace video {
namespace av1 {

namespace {

struct LfCtx {
    const Decoder* d;
    int BitDepth;
};

int filter_level(const Decoder& d, const MiInfo& m, int plane, int pass) {
    const FrameHeader& fh = d.fh;
    const int segment = m.segment_id;
    const int ref = m.ref_frame[0];
    const int mode = m.y_mode;
    const int modeType = (mode >= NEARESTMV && mode != GLOBALMV && mode != GLOBAL_GLOBALMV) ? 1 : 0;
    int deltaLF;
    if (fh.delta_lf_multi == 0) {
        deltaLF = m.delta_lf[0];
    } else {
        deltaLF = m.delta_lf[plane == 0 ? pass : plane + 1];
    }
    const int i = plane == 0 ? pass : plane + 1;
    const int baseFilterLevel = clip3(0, MAX_LOOP_FILTER, deltaLF + fh.loop_filter_level[i]);
    int lvlSeg = baseFilterLevel;
    const int feature = SEG_LVL_ALT_LF_Y_V + i;
    if (fh.segmentation_enabled && fh.FeatureEnabled[segment][feature]) {
        lvlSeg = fh.FeatureData[segment][feature] + lvlSeg;
        lvlSeg = clip3(0, MAX_LOOP_FILTER, lvlSeg);
    }
    if (fh.loop_filter_delta_enabled == 1) {
        const int nShift = lvlSeg >> 5;
        if (ref <= INTRA_FRAME) {
            lvlSeg = lvlSeg + (fh.loop_filter_ref_deltas[INTRA_FRAME] * (1 << nShift));
        } else {
            lvlSeg = lvlSeg + (fh.loop_filter_ref_deltas[ref] * (1 << nShift)) + (fh.loop_filter_mode_deltas[modeType] * (1 << nShift));
        }
        lvlSeg = clip3(0, MAX_LOOP_FILTER, lvlSeg);
    }
    return lvlSeg;
}

inline int f4clamp(int v, int bd) { return clip3(-(1 << (bd - 1)), (1 << (bd - 1)) - 1, v); }

void sample_filter(std::uint16_t* p, std::ptrdiff_t step, int plane, int limit, int blimit, int thresh, int filterSize, int bd) {
    int filterLen;
    if (filterSize == 4) {
        filterLen = 4;
    } else if (plane != 0) {
        filterLen = 6;
    } else if (filterSize == 8) {
        filterLen = 8;
    } else {
        filterLen = 16;
    }
    const int q0 = p[0];
    const int q1 = p[step];
    const int p0 = p[-step];
    const int p1 = p[-2 * step];
    const int q2 = filterLen >= 6 ? p[2 * step] : 0;
    const int p2 = filterLen >= 6 ? p[-3 * step] : 0;
    const int q3 = filterLen >= 8 ? p[3 * step] : 0;
    const int p3 = filterLen >= 8 ? p[-4 * step] : 0;
    const int sh = bd - 8;
    int hevMask = 0;
    const int threshBd = thresh << sh;
    hevMask |= iabs(p1 - p0) > threshBd;
    hevMask |= iabs(q1 - q0) > threshBd;
    const int limitBd = limit << sh;
    const int blimitBd = blimit << sh;
    int mask = 0;
    mask |= iabs(p1 - p0) > limitBd;
    mask |= iabs(q1 - q0) > limitBd;
    mask |= iabs(p0 - q0) * 2 + iabs(p1 - q1) / 2 > blimitBd;
    if (filterLen >= 6) {
        mask |= iabs(p2 - p1) > limitBd;
        mask |= iabs(q2 - q1) > limitBd;
    }
    if (filterLen >= 8) {
        mask |= iabs(p3 - p2) > limitBd;
        mask |= iabs(q3 - q2) > limitBd;
    }
    if (mask != 0) return;
    const int thresholdBd = 1 << sh;
    int flatMask = 0;
    if (filterSize >= 8) {
        int m = 0;
        m |= iabs(p1 - p0) > thresholdBd;
        m |= iabs(q1 - q0) > thresholdBd;
        m |= iabs(p2 - p0) > thresholdBd;
        m |= iabs(q2 - q0) > thresholdBd;
        if (filterLen >= 8) {
            m |= iabs(p3 - p0) > thresholdBd;
            m |= iabs(q3 - q0) > thresholdBd;
        }
        flatMask = m == 0;
    }
    int flatMask2 = 0;
    if (filterSize >= 16) {
        const int q4 = p[4 * step];
        const int q5 = p[5 * step];
        const int q6 = p[6 * step];
        const int p4 = p[-5 * step];
        const int p5 = p[-6 * step];
        const int p6 = p[-7 * step];
        int m = 0;
        m |= iabs(p6 - p0) > thresholdBd;
        m |= iabs(q6 - q0) > thresholdBd;
        m |= iabs(p5 - p0) > thresholdBd;
        m |= iabs(q5 - q0) > thresholdBd;
        m |= iabs(p4 - p0) > thresholdBd;
        m |= iabs(q4 - q0) > thresholdBd;
        flatMask2 = m == 0;
    }
    if (filterSize == 4 || !flatMask) {
        const int off = 0x80 << sh;
        const int ps1 = p1 - off;
        const int ps0 = p0 - off;
        const int qs0 = q0 - off;
        const int qs1 = q1 - off;
        int filter = hevMask ? f4clamp(ps1 - qs1, bd) : 0;
        filter = f4clamp(filter + 3 * (qs0 - ps0), bd);
        const int filter1 = f4clamp(filter + 4, bd) >> 3;
        const int filter2 = f4clamp(filter + 3, bd) >> 3;
        p[0] = static_cast<std::uint16_t>(f4clamp(qs0 - filter1, bd) + off);
        p[-step] = static_cast<std::uint16_t>(f4clamp(ps0 + filter2, bd) + off);
        if (!hevMask) {
            filter = round2(filter1, 1);
            p[step] = static_cast<std::uint16_t>(f4clamp(qs1 - filter, bd) + off);
            p[-2 * step] = static_cast<std::uint16_t>(f4clamp(ps1 + filter, bd) + off);
        }
        return;
    }
    int log2Size;
    if (filterSize == 8 || !flatMask2) {
        log2Size = 3;
    } else {
        log2Size = 4;
    }
    int n;
    if (log2Size == 4) {
        n = 6;
    } else if (plane == 0) {
        n = 3;
    } else {
        n = 2;
    }
    const int n2 = (log2Size == 3 && plane == 0) ? 0 : 1;
    int ext[32];
    int* e = ext + 16;
    for (int k = -(n + 1); k <= n; ++k) e[k] = p[k * step];
    for (int k = -2 * n - 1; k < -(n + 1); ++k) e[k] = e[-(n + 1)];
    for (int k = n + 1; k <= 2 * n; ++k) e[k] = e[n];
    int b1 = 0;
    for (int k = -2 * n; k <= 0; ++k) b1 += e[k];
    int b2 = 0;
    for (int k = -n - n2; k <= -n + n2; ++k) b2 += e[k];
    int F[12];
    const int add = 1 << (log2Size - 1);
    for (int i = -n; i < n; ++i) {
        F[i + 6] = (b1 + b2 + add) >> log2Size;
        b1 += e[i + n + 1] - e[i - n];
        b2 += e[i + n2 + 1] - e[i - n2];
    }
    for (int i = -n; i < n; ++i) p[i * step] = static_cast<std::uint16_t>(F[i + 6]);
}

} // namespace

void Decoder::loop_filter_rows(int plane, int pass, int rowBegin, int rowEnd) {
    const int bd = seq.BitDepth;
    int shift;
    if (fh.loop_filter_sharpness > 4) {
        shift = 2;
    } else if (fh.loop_filter_sharpness > 0) {
        shift = 1;
    } else {
        shift = 0;
    }
    int limitLut[64];
    for (int lvl = 0; lvl < 64; ++lvl) {
        if (fh.loop_filter_sharpness > 0) {
            limitLut[lvl] = clip3(1, 9 - fh.loop_filter_sharpness, lvl >> shift);
        } else {
            limitLut[lvl] = imax(1, lvl >> shift);
        }
    }
    {
        const int subX = plane > 0 ? seq.subsampling_x : 0;
        const int subY = plane > 0 ? seq.subsampling_y : 0;
        Plane& P = frame.planes[plane];
        const std::uint8_t* lft = lf_tx_size[plane].data();
        {
            std::uint8_t lvlLut[MAX_SEGMENTS][8][2];
            const bool useLut = !fh.delta_lf_present;
            if (useLut) {
                MiInfo probe = MiInfo();
                for (int sg = 0; sg < MAX_SEGMENTS; ++sg) {
                    for (int ref = 0; ref < 8; ++ref) {
                        for (int mt = 0; mt < 2; ++mt) {
                            probe.segment_id = static_cast<std::uint8_t>(sg);
                            probe.ref_frame[0] = static_cast<std::int8_t>(ref);
                            probe.y_mode = static_cast<std::uint8_t>(mt ? NEARESTMV : DC_PRED);
                            probe.delta_lf[0] = probe.delta_lf[1] = probe.delta_lf[2] = probe.delta_lf[3] = 0;
                            lvlLut[sg][ref][mt] = static_cast<std::uint8_t>(filter_level(*this, probe, plane, pass));
                        }
                    }
                }
            }
            auto level_of = [&](const MiInfo& m) {
                if (useLut) {
                    const int mode = m.y_mode;
                    const int mt = (mode >= NEARESTMV && mode != GLOBALMV && mode != GLOBAL_GLOBALMV) ? 1 : 0;
                    const int ref = m.ref_frame[0] < 0 ? 0 : m.ref_frame[0];
                    return static_cast<int>(lvlLut[m.segment_id & 7][ref & 7][mt]);
                }
                return filter_level(*this, m, plane, pass);
            };
            const int rowStep = plane == 0 ? 1 : (1 << subY);
            const int colStep = plane == 0 ? 1 : (1 << subX);
            const int dx = pass == 0 ? 1 : 0;
            const int dy = pass == 0 ? 0 : 1;
            const std::ptrdiff_t step = pass == 0 ? 1 : P.stride;
            for (int row0 = rowBegin; row0 < rowEnd && row0 < fh.MiRows; row0 += rowStep) {
                const int y = row0 * MI_SIZE;
                if (y >= fh.FrameHeight) break;
                if (pass == 1 && y == 0) continue;
                const int row = row0 | subY;
                const int yP = y >> subY;
                const int prevRow = row - (dy << subY);
                const std::uint8_t* lrow = lft + static_cast<std::size_t>(row >> subY) * mi_stride;
                const std::uint8_t* lprow = lft + static_cast<std::size_t>(prevRow >> subY) * mi_stride;
                const MiInfo* mrow = &mi[static_cast<std::size_t>(row) * mi_stride];
                const MiInfo* mprow = &mi[static_cast<std::size_t>(prevRow) * mi_stride];
                for (int col0 = 0; col0 < fh.MiCols; col0 += colStep) {
                    const int x = col0 * MI_SIZE;
                    if (x >= fh.FrameWidth) break;
                    if (pass == 0 && x == 0) continue;
                    const int col = col0 | subX;
                    const int xP = x >> subX;
                    const int txSz = lrow[col >> subX];
                    if (pass == 0) {
                        if (xP & (Tx_Width[txSz] - 1)) continue;
                    } else {
                        if (yP & (Tx_Height[txSz] - 1)) continue;
                    }
                    const int prevCol = col - (dx << subX);
                    const MiInfo& m = mrow[col];
                    const int planeSize = Subsampled_Size[m.mi_size][subX][subY];
                    if (planeSize == BLOCK_INVALID) continue;
                    int isBlockEdge;
                    if (pass == 0) {
                        isBlockEdge = (xP % block_width(planeSize)) == 0;
                    } else {
                        isBlockEdge = (yP % block_height(planeSize)) == 0;
                    }
                    const int isIntra = m.ref_frame[0] <= INTRA_FRAME;
                    if (!(isBlockEdge || !m.skip || isIntra)) continue;
                    const int prevTxSz = pass == 0 ? lrow[prevCol >> subX] : lprow[col >> subX];
                    int baseSize;
                    if (pass == 0) {
                        baseSize = imin(Tx_Width[prevTxSz], Tx_Width[txSz]);
                    } else {
                        baseSize = imin(Tx_Height[prevTxSz], Tx_Height[txSz]);
                    }
                    const int filterSize = plane == 0 ? imin(16, baseSize) : imin(8, baseSize);
                    int lvl = level_of(m);
                    if (lvl == 0) lvl = level_of(pass == 0 ? mrow[prevCol] : mprow[col]);
                    if (lvl == 0) continue;
                    const int limit = limitLut[lvl];
                    const int blimit = 2 * (lvl + 2) + limit;
                    const int thresh = lvl >> 4;
                    std::uint16_t* px = P.row(yP) + xP;
                    const std::ptrdiff_t along = pass == 0 ? P.stride : 1;
                    for (int i = 0; i < MI_SIZE; ++i) sample_filter(px + i * along, step, plane, limit, blimit, thresh, filterSize, bd);
                }
            }
        }
    }
}

void Decoder::loop_filter_frame() {
    const int band = 16;
    const int bands = (fh.MiRows + band - 1) / band;
    std::vector<int> planes;
    for (int plane = 0; plane < seq.NumPlanes; ++plane) {
        if (plane == 0 || fh.loop_filter_level[1 + plane]) planes.push_back(plane);
    }
    const int np = static_cast<int>(planes.size());
    for (int pass = 0; pass < 2; ++pass) {
        parallel(np * bands, [&](int t) {
            const int plane = planes[static_cast<std::size_t>(t / bands)];
            const int b = t % bands;
            loop_filter_rows(plane, pass, b * band, (b + 1) * band);
        });
    }
}

namespace {

void cdef_direction(const Plane& P, int x0, int y0, int bd, int* yDir, int* var) {
    int cost[8] = {};
    int partial[8][15] = {};
    int bestCost = 0;
    *yDir = 0;
    for (int i = 0; i < 8; ++i) {
        const std::uint16_t* r = P.row(y0 + i) + x0;
        for (int j = 0; j < 8; ++j) {
            const int x = (r[j] >> (bd - 8)) - 128;
            partial[0][i + j] += x;
            partial[1][i + j / 2] += x;
            partial[2][i] += x;
            partial[3][3 + i - j / 2] += x;
            partial[4][7 + i - j] += x;
            partial[5][3 - i / 2 + j] += x;
            partial[6][j] += x;
            partial[7][i / 2 + j] += x;
        }
    }
    for (int i = 0; i < 8; ++i) {
        cost[2] += partial[2][i] * partial[2][i];
        cost[6] += partial[6][i] * partial[6][i];
    }
    cost[2] *= Div_Table[8];
    cost[6] *= Div_Table[8];
    for (int i = 0; i < 7; ++i) {
        cost[0] += (partial[0][i] * partial[0][i] + partial[0][14 - i] * partial[0][14 - i]) * Div_Table[i + 1];
        cost[4] += (partial[4][i] * partial[4][i] + partial[4][14 - i] * partial[4][14 - i]) * Div_Table[i + 1];
    }
    cost[0] += partial[0][7] * partial[0][7] * Div_Table[8];
    cost[4] += partial[4][7] * partial[4][7] * Div_Table[8];
    for (int i = 1; i < 8; i += 2) {
        for (int j = 0; j < 4 + 1; ++j) cost[i] += partial[i][3 + j] * partial[i][3 + j];
        cost[i] *= Div_Table[8];
        for (int j = 0; j < 4 - 1; ++j) cost[i] += (partial[i][j] * partial[i][j] + partial[i][10 - j] * partial[i][10 - j]) * Div_Table[2 * j + 2];
    }
    for (int i = 0; i < 8; ++i) {
        if (cost[i] > bestCost) {
            bestCost = cost[i];
            *yDir = i;
        }
    }
    *var = (bestCost - cost[(*yDir + 4) & 7]) >> 10;
}

void cdef_filter(const Plane& src, Plane& dst, int planeW4, int planeH4, int x0, int y0, int w, int h, int priStr, int secStr, int damping, int dir, int coeffShift) {
    const int LARGE = 30000;
    const int WS = 12;
    std::int16_t win[12 * 12];
    const int maxX = planeW4;
    const int maxY = planeH4;
    if (x0 >= 2 && y0 >= 2 && x0 + w + 2 <= maxX && y0 + h + 2 <= maxY) {
        for (int i = 0; i < h + 4; ++i) {
            const std::uint16_t* r = src.row(y0 + i - 2) + x0 - 2;
            std::int16_t* o = win + i * WS;
            for (int j = 0; j < w + 4; ++j) o[j] = static_cast<std::int16_t>(r[j]);
        }
    } else {
        for (int i = -2; i < h + 2; ++i) {
            const int yy = y0 + i;
            const bool rowOk = yy >= 0 && yy < maxY;
            const std::uint16_t* r = rowOk ? src.row(yy) : nullptr;
            std::int16_t* o = win + (i + 2) * WS;
            for (int j = -2; j < w + 2; ++j) {
                const int xx = x0 + j;
                o[j + 2] = (rowOk && xx >= 0 && xx < maxX) ? static_cast<std::int16_t>(r[xx]) : static_cast<std::int16_t>(LARGE);
            }
        }
    }
    const int priTap = (priStr >> coeffShift) & 1;
    const int pt0 = Cdef_Pri_Taps[priTap][0];
    const int pt1 = Cdef_Pri_Taps[priTap][1];
    const int st0 = Cdef_Sec_Taps[priTap][0];
    const int st1 = Cdef_Sec_Taps[priTap][1];
    const int priAdj = priStr ? imax(0, damping - floor_log2(static_cast<std::uint32_t>(priStr))) : 0;
    const int secAdj = secStr ? imax(0, damping - floor_log2(static_cast<std::uint32_t>(secStr))) : 0;
    const int d1 = (dir + 2) & 7;
    const int d2 = (dir + 6) & 7;
    const int po0 = Cdef_Directions[dir][0][0] * WS + Cdef_Directions[dir][0][1];
    const int po1 = Cdef_Directions[dir][1][0] * WS + Cdef_Directions[dir][1][1];
    const int sa0 = Cdef_Directions[d1][0][0] * WS + Cdef_Directions[d1][0][1];
    const int sa1 = Cdef_Directions[d1][1][0] * WS + Cdef_Directions[d1][1][1];
    const int sb0 = Cdef_Directions[d2][0][0] * WS + Cdef_Directions[d2][0][1];
    const int sb1 = Cdef_Directions[d2][1][0] * WS + Cdef_Directions[d2][1][1];
    auto cons = [](int diff, int thr, int adj) {
        const int ad = diff < 0 ? -diff : diff;
        int v = thr - (ad >> adj);
        if (v < 0) v = 0;
        if (v > ad) v = ad;
        return diff < 0 ? -v : v;
    };
#if defined(FIZMO_AV1_SSE2)
    if (w == 8 || w == 4) {
        const __m128i vLarge = _mm_set1_epi16(LARGE);
        const __m128i vZero = _mm_setzero_si128();
        const __m128i vPri = _mm_set1_epi16(static_cast<short>(priStr));
        const __m128i vSec = _mm_set1_epi16(static_cast<short>(secStr));
        const __m128i cPriAdj = _mm_cvtsi32_si128(priAdj);
        const __m128i cSecAdj = _mm_cvtsi32_si128(secAdj);
        const __m128i vPt0 = _mm_set1_epi16(static_cast<short>(pt0));
        const __m128i vPt1 = _mm_set1_epi16(static_cast<short>(pt1));
        const __m128i vSt0 = _mm_set1_epi16(static_cast<short>(st0));
        const __m128i vSt1 = _mm_set1_epi16(static_cast<short>(st1));
        const __m128i v8 = _mm_set1_epi16(8);
        auto load = [&](const std::int16_t* q) {
            if (w == 8) return _mm_loadu_si128(reinterpret_cast<const __m128i*>(q));
            return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q));
        };
        auto vcons = [&](__m128i pv, __m128i xv, __m128i thr, __m128i adj) {
            const __m128i diff = _mm_sub_epi16(pv, xv);
            const __m128i sgn = _mm_srai_epi16(diff, 15);
            const __m128i ad = _mm_sub_epi16(_mm_xor_si128(diff, sgn), sgn);
            __m128i t = _mm_sub_epi16(thr, _mm_srl_epi16(ad, adj));
            t = _mm_max_epi16(t, vZero);
            t = _mm_min_epi16(t, ad);
            return _mm_sub_epi16(_mm_xor_si128(t, sgn), sgn);
        };
        for (int i = 0; i < h; ++i) {
            const std::int16_t* c = win + (i + 2) * WS + 2;
            const __m128i xv = load(c);
            __m128i sum = vZero;
            __m128i mx = xv;
            __m128i mn = xv;
            const int offs[12] = {po0, -po0, po1, -po1, sa0, -sa0, sb0, -sb0, sa1, -sa1, sb1, -sb1};
            for (int k = 0; k < 12; ++k) {
                const __m128i pv = load(c + offs[k]);
                mx = _mm_max_epi16(mx, _mm_andnot_si128(_mm_cmpeq_epi16(pv, vLarge), pv));
                mn = _mm_min_epi16(mn, pv);
                if (k < 4) {
                    if (priStr) sum = _mm_add_epi16(sum, _mm_mullo_epi16(k < 2 ? vPt0 : vPt1, vcons(pv, xv, vPri, cPriAdj)));
                } else {
                    if (secStr) sum = _mm_add_epi16(sum, _mm_mullo_epi16(k < 8 ? vSt0 : vSt1, vcons(pv, xv, vSec, cSecAdj)));
                }
            }
            __m128i y = _mm_add_epi16(sum, v8);
            y = _mm_add_epi16(y, _mm_srai_epi16(sum, 15));
            y = _mm_add_epi16(xv, _mm_srai_epi16(y, 4));
            y = _mm_max_epi16(y, mn);
            y = _mm_min_epi16(y, mx);
            std::uint16_t* o = dst.row(y0 + i) + x0;
            if (w == 8) {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(o), y);
            } else {
                _mm_storel_epi64(reinterpret_cast<__m128i*>(o), y);
            }
        }
        return;
    }
#endif // FIZMO_AV1_SSE2
    for (int i = 0; i < h; ++i) {
        std::uint16_t* o = dst.row(y0 + i) + x0;
        const std::int16_t* c = win + (i + 2) * WS + 2;
        for (int j = 0; j < w; ++j) {
            const std::int16_t* p = c + j;
            const int x = p[0];
            int sum = 0;
            int mx = x;
            int mn = x;
            const int taps[12] = {p[po0], p[-po0], p[po1], p[-po1], p[sa0], p[-sa0], p[sb0], p[-sb0], p[sa1], p[-sa1], p[sb1], p[-sb1]};
            if (priStr) {
                sum += pt0 * (cons(taps[0] - x, priStr, priAdj) + cons(taps[1] - x, priStr, priAdj));
                sum += pt1 * (cons(taps[2] - x, priStr, priAdj) + cons(taps[3] - x, priStr, priAdj));
            }
            if (secStr) {
                sum += st0 * (cons(taps[4] - x, secStr, secAdj) + cons(taps[5] - x, secStr, secAdj) + cons(taps[6] - x, secStr, secAdj) + cons(taps[7] - x, secStr, secAdj));
                sum += st1 * (cons(taps[8] - x, secStr, secAdj) + cons(taps[9] - x, secStr, secAdj) + cons(taps[10] - x, secStr, secAdj) + cons(taps[11] - x, secStr, secAdj));
            }
            for (int k = 0; k < 12; ++k) {
                const int t = taps[k];
                if (t != LARGE) {
                    if (t > mx) mx = t;
                    if (t < mn) mn = t;
                }
            }
            o[j] = static_cast<std::uint16_t>(clip3(mn, mx, x + ((8 + sum - (sum < 0)) >> 4)));
        }
    }
}

} // namespace

void Decoder::cdef_rows(int fbrBegin, int fbrEnd) {
    const int bd = seq.BitDepth;
    const int coeffShift = bd - 8;
    const int numPlanes = seq.NumPlanes;
    for (int p = 0; p < numPlanes; ++p) {
        const Plane& s = frame.planes[p];
        Plane& t = cdef_frame.planes[p];
        const int sy = p > 0 ? seq.subsampling_y : 0;
        const int y0 = (fbrBegin * 64) >> sy;
        const int y1 = fbrEnd >= (fh.MiRows + 15) / 16 ? s.height : imin(s.height, (fbrEnd * 64) >> sy);
        for (int y = y0; y < y1; ++y) std::memcpy(t.row(y), s.row(y), sizeof(std::uint16_t) * static_cast<std::size_t>(s.width));
    }
    for (int r = fbrBegin * 16; r < fh.MiRows && r < fbrEnd * 16; r += 2) {
        for (int c = 0; c < fh.MiCols; c += 2) {
            const int idx = cdef_idx[static_cast<std::size_t>(r >> 4) * cdef_stride + (c >> 4)];
            if (idx == -1) continue;
            auto skipAt = [&](int rr, int cc) { return mi[static_cast<std::size_t>(rr) * mi_stride + cc].skip; };
            const int skip = skipAt(r, c) && skipAt(r + 1, c) && skipAt(r, c + 1) && skipAt(r + 1, c + 1);
            if (skip) continue;
            int yDir;
            int var;
            AV1_STAT(ST_CDEF);
            cdef_direction(frame.planes[0], c * MI_SIZE, r * MI_SIZE, bd, &yDir, &var);
            {
                int priStr = fh.cdef_y_pri_strength[idx] << coeffShift;
                const int secStr = fh.cdef_y_sec_strength[idx] << coeffShift;
                const int dir = priStr == 0 ? 0 : yDir;
                const int varStr = (var >> 6) ? imin(floor_log2(static_cast<std::uint32_t>(var >> 6)), 12) : 0;
                priStr = var ? (priStr * (4 + varStr) + 8) >> 4 : 0;
                const int damping = fh.cdef_damping + coeffShift;
                if (priStr || secStr) cdef_filter(frame.planes[0], cdef_frame.planes[0], fh.MiCols * MI_SIZE, fh.MiRows * MI_SIZE, c * MI_SIZE, r * MI_SIZE, 8, 8, priStr, secStr, damping, dir, coeffShift);
            }
            if (numPlanes == 1) continue;
            const int priStr = fh.cdef_uv_pri_strength[idx] << coeffShift;
            const int secStr = fh.cdef_uv_sec_strength[idx] << coeffShift;
            const int dir = priStr == 0 ? 0 : Cdef_Uv_Dir[seq.subsampling_x][seq.subsampling_y][yDir];
            const int damping = fh.cdef_damping + coeffShift - 1;
            if (!priStr && !secStr) continue;
            const int sx = seq.subsampling_x;
            const int sy = seq.subsampling_y;
            for (int pl = 1; pl < 3; ++pl) cdef_filter(frame.planes[pl], cdef_frame.planes[pl], (fh.MiCols * MI_SIZE) >> sx, (fh.MiRows * MI_SIZE) >> sy, (c * MI_SIZE) >> sx, (r * MI_SIZE) >> sy, 8 >> sx, 8 >> sy, priStr, secStr, damping, dir, coeffShift);
        }
    }
}

void Decoder::cdef_frame_process() {
    const int fbrs = (fh.MiRows + 15) / 16;
    parallel(fbrs, [&](int t) { cdef_rows(t, t + 1); });
}

void Decoder::upscale_rows(const FrameBuffer& in, FrameBuffer& out, int plane, int yBegin, int yEnd) {
    const int maxv = (1 << seq.BitDepth) - 1;
    {
        const int subX = plane > 0 ? seq.subsampling_x : 0;
        const int subY = plane > 0 ? seq.subsampling_y : 0;
        const int downscaledPlaneW = round2(fh.FrameWidth, subX);
        const int upscaledPlaneW = round2(fh.UpscaledWidth, subX);
        const int planeH = round2(fh.FrameHeight, subY);
        const int stepX = ((downscaledPlaneW << SUPERRES_SCALE_BITS) + (upscaledPlaneW / 2)) / upscaledPlaneW;
        const int err = (upscaledPlaneW * stepX) - (downscaledPlaneW << SUPERRES_SCALE_BITS);
        int initialSubpelX = (-((upscaledPlaneW - downscaledPlaneW) << (SUPERRES_SCALE_BITS - 1)) + upscaledPlaneW / 2) / upscaledPlaneW + (1 << (SUPERRES_EXTRA_BITS - 1)) - err / 2;
        initialSubpelX &= SUPERRES_SCALE_MASK;
        const int miW = fh.MiCols >> subX;
        const int minX = 0;
        const int maxX = miW * MI_SIZE - 1;
        const Plane& ip = in.planes[plane];
        Plane& op = out.planes[plane];
        for (int y = yBegin; y < planeH && y < yEnd; ++y) {
            const std::uint16_t* src = ip.row(y);
            std::uint16_t* dst = op.row(y);
            for (int x = 0; x < upscaledPlaneW; ++x) {
                const int srcX = -(1 << SUPERRES_SCALE_BITS) + initialSubpelX + x * stepX;
                const int srcXPx = srcX >> SUPERRES_SCALE_BITS;
                const int srcXSubpel = (srcX & SUPERRES_SCALE_MASK) >> SUPERRES_EXTRA_BITS;
                int sum = 0;
                for (int k = 0; k < SUPERRES_FILTER_TAPS; ++k) {
                    const int sampleX = clip3(minX, maxX, srcXPx + (k - SUPERRES_FILTER_OFFSET));
                    sum += src[sampleX] * Upscale_Filter[srcXSubpel][k];
                }
                dst[x] = static_cast<std::uint16_t>(clip3(0, maxv, round2(sum, FILTER_BITS)));
            }
        }
    }
}

void Decoder::upscale(const FrameBuffer& in, FrameBuffer& out) {
    const int band = 32;
    const int bands = (fh.FrameHeight + band - 1) / band;
    parallel(seq.NumPlanes * bands, [&](int t) {
        const int plane = t / bands;
        const int b = t % bands;
        upscale_rows(in, out, plane, b * band, (b + 1) * band);
    });
}

namespace {

struct LrRegion {
    const Plane* cur;
    const Plane* cdef;
    int planeEndX;
    int planeEndY;
    int stripeStartY;
    int stripeEndY;
    int get(int x, int y) const {
        x = imin(planeEndX, x);
        x = imax(0, x);
        y = imin(planeEndY, y);
        y = imax(0, y);
        if (y < stripeStartY) {
            y = imax(stripeStartY - 2, y);
            return cur->row(y)[x];
        } else if (y > stripeEndY) {
            y = imin(stripeEndY + 2, y);
            return cur->row(y)[x];
        }
        return cdef->row(y)[x];
    }
};

void wiener_region(const LrRegion& R, Plane& out, const LrUnit& u, int x, int y, int w, int h, int bd, std::vector<std::int32_t>& tmp) {
    const int r0 = bd == 12 ? 5 : 3;
    const int r1 = bd == 12 ? 9 : 11;
    int vfilter[7];
    int hfilter[7];
    {
        vfilter[3] = 128;
        hfilter[3] = 128;
        for (int i = 0; i < 3; ++i) {
            const int cv = u.wiener[0][i];
            vfilter[i] = cv;
            vfilter[6 - i] = cv;
            vfilter[3] -= 2 * cv;
            const int ch = u.wiener[1][i];
            hfilter[i] = ch;
            hfilter[6 - i] = ch;
            hfilter[3] -= 2 * ch;
        }
    }
    const int offset = 1 << (bd + FILTER_BITS - r0 - 1);
    const int limit = (1 << (bd + 1 + FILTER_BITS - r0)) - 1;
    const int maxv = (1 << bd) - 1;
    tmp.resize(static_cast<std::size_t>(w + 6) * (h + 6) + static_cast<std::size_t>(w) * (h + 6));
    std::int32_t* src = tmp.data();
    std::int32_t* inter = src + static_cast<std::size_t>(w + 6) * (h + 6);
    const int sw = w + 6;
    for (int r = 0; r < h + 6; ++r) {
        for (int c = 0; c < w + 6; ++c) src[r * sw + c] = R.get(x + c - 3, y + r - 3);
    }
    for (int r = 0; r < h + 6; ++r) {
        const std::int32_t* s = src + r * sw;
        for (int c = 0; c < w; ++c) {
            int sum = 0;
            for (int t = 0; t < 7; ++t) sum += hfilter[t] * s[c + t];
            const int v = round2(sum, r0);
            inter[r * w + c] = clip3(-offset, limit - offset, v);
        }
    }
    for (int r = 0; r < h; ++r) {
        std::uint16_t* o = out.row(y + r) + x;
        for (int c = 0; c < w; ++c) {
            int sum = 0;
            for (int t = 0; t < 7; ++t) sum += vfilter[t] * inter[(r + t) * w + c];
            const int v = round2(sum, r1);
            o[c] = static_cast<std::uint16_t>(clip3(0, maxv, v));
        }
    }
}

void box_filter(const LrRegion& R, const std::int32_t* src, int sw, int x, int y, int w, int h, int set, int pass, int bd, std::vector<std::int32_t>& AB, std::int32_t* F) {
    const int r = Sgr_Params[set][pass * 2 + 0];
    if (r == 0) return;
    const int eps = Sgr_Params[set][pass * 2 + 1];
    const int n = (2 * r + 1) * (2 * r + 1);
    const int n2e = n * n * eps;
    const std::int64_t s = (((1 << SGRPROJ_MTABLE_BITS) + n2e / 2) / n2e);
    const int aw = w + 2;
    const int ah = h + 2;
    AB.resize(static_cast<std::size_t>(aw) * ah * 2);
    std::int32_t* A = AB.data();
    std::int32_t* B = A + static_cast<std::size_t>(aw) * ah;
    const int oneOverN = ((1 << SGRPROJ_RECIP_BITS) + (n / 2)) / n;
    for (int i = -1; i < h + 1; ++i) {
        if (pass == 0 && !(i & 1)) continue;
        for (int j = -1; j < w + 1; ++j) {
            std::int64_t a = 0;
            std::int64_t b = 0;
            for (int dy = -r; dy <= r; ++dy) {
                const std::int32_t* row = src + (i + dy + 3) * sw + (j + 3);
                for (int dx = -r; dx <= r; ++dx) {
                    const std::int64_t c = row[dx];
                    a += c * c;
                    b += c;
                }
            }
            a = round2_64(a, 2 * (bd - 8));
            const std::int64_t dd = round2_64(b, bd - 8);
            const std::int64_t p = std::max<std::int64_t>(0, a * n - dd * dd);
            const std::int64_t z = round2_64(p * s, SGRPROJ_MTABLE_BITS);
            std::int64_t a2;
            if (z >= 255) {
                a2 = 256;
            } else if (z == 0) {
                a2 = 1;
            } else {
                a2 = ((z << SGRPROJ_SGR_BITS) + (z / 2)) / (z + 1);
            }
            const std::int64_t b2 = ((1 << SGRPROJ_SGR_BITS) - a2) * b * oneOverN;
            A[(i + 1) * aw + (j + 1)] = static_cast<std::int32_t>(a2);
            B[(i + 1) * aw + (j + 1)] = static_cast<std::int32_t>(round2_64(b2, SGRPROJ_RECIP_BITS));
        }
    }
    for (int i = 0; i < h; ++i) {
        int shift = 5;
        if (pass == 0 && (i & 1)) shift = 4;
        for (int j = 0; j < w; ++j) {
            std::int64_t a = 0;
            std::int64_t b = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int weight;
                    if (pass == 0) {
                        if ((i + dy) & 1) {
                            weight = dx == 0 ? 6 : 5;
                        } else {
                            weight = 0;
                        }
                    } else {
                        weight = (dx == 0 || dy == 0) ? 4 : 3;
                    }
                    if (weight == 0) continue;
                    a += static_cast<std::int64_t>(weight) * A[(i + dy + 1) * aw + (j + dx + 1)];
                    b += static_cast<std::int64_t>(weight) * B[(i + dy + 1) * aw + (j + dx + 1)];
                }
            }
            const std::int64_t v = a * R.cdef->row(y + i)[x + j] + b;
            F[i * w + j] = static_cast<std::int32_t>(round2_64(v, SGRPROJ_SGR_BITS + shift - SGRPROJ_RST_BITS));
        }
    }
    (void)x;
}

void sgr_region(const LrRegion& R, Plane& out, const LrUnit& u, int x, int y, int w, int h, int bd, std::vector<std::int32_t>& tmp, std::vector<std::int32_t>& AB) {
    const int set = u.sgr_set;
    const int sw = w + 6;
    tmp.resize(static_cast<std::size_t>(sw) * (h + 6) + 2 * static_cast<std::size_t>(w) * h);
    std::int32_t* src = tmp.data();
    std::int32_t* flt0 = src + static_cast<std::size_t>(sw) * (h + 6);
    std::int32_t* flt1 = flt0 + static_cast<std::size_t>(w) * h;
    for (int r = 0; r < h + 6; ++r) {
        for (int c = 0; c < sw; ++c) src[r * sw + c] = R.get(x + c - 3, y + r - 3);
    }
    box_filter(R, src, sw, x, y, w, h, set, 0, bd, AB, flt0);
    box_filter(R, src, sw, x, y, w, h, set, 1, bd, AB, flt1);
    const int w0 = u.sgr_xqd[0];
    const int w1 = u.sgr_xqd[1];
    const int w2 = (1 << SGRPROJ_PRJ_BITS) - w0 - w1;
    const int r0 = Sgr_Params[set][0];
    const int r1 = Sgr_Params[set][2];
    const int maxv = (1 << bd) - 1;
    for (int i = 0; i < h; ++i) {
        std::uint16_t* o = out.row(y + i) + x;
        const std::uint16_t* c = R.cdef->row(y + i) + x;
        for (int j = 0; j < w; ++j) {
            const std::int64_t uu = static_cast<std::int64_t>(c[j]) << SGRPROJ_RST_BITS;
            std::int64_t v = w1 * uu;
            if (r0) {
                v += static_cast<std::int64_t>(w0) * flt0[i * w + j];
            } else {
                v += w0 * uu;
            }
            if (r1) {
                v += static_cast<std::int64_t>(w2) * flt1[i * w + j];
            } else {
                v += w2 * uu;
            }
            const int s = static_cast<int>(round2_64(v, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS));
            o[j] = static_cast<std::uint16_t>(clip3(0, maxv, s));
        }
    }
}

} // namespace

void Decoder::lr_stripe(int plane, int stripeNum, std::vector<std::int32_t>& tmp, std::vector<std::int32_t>& AB) {
    const FrameBuffer& cdefIn = *lr_cdef_src;
    const FrameBuffer& curIn = *lr_cur_src;
    const int bd = seq.BitDepth;
    const int subX = plane > 0 ? seq.subsampling_x : 0;
    const int subY = plane > 0 ? seq.subsampling_y : 0;
    const int unitSize = fh.LoopRestorationSize[plane];
    const int unitRows = lr_unit_rows[plane];
    const int unitCols = lr_unit_cols[plane];
    LrRegion R;
    R.cur = &curIn.planes[plane];
    R.cdef = &cdefIn.planes[plane];
    R.planeEndX = round2(fh.UpscaledWidth, subX) - 1;
    R.planeEndY = round2(fh.FrameHeight, subY) - 1;
    Plane& out = lr_frame.planes[plane];
    R.stripeStartY = (-8 + stripeNum * 64) >> subY;
    R.stripeEndY = R.stripeStartY + (64 >> subY) - 1;
    const int y0 = imax(0, R.stripeStartY);
    const int y1 = imin(R.planeEndY, R.stripeEndY);
    if (y0 > y1) return;
    const int lumaY = (y0 << subY);
    const int unitRow = imin(unitRows - 1, ((((lumaY >> 2) * MI_SIZE) + 8) >> subY) / unitSize);
    for (int unitCol = 0; unitCol < unitCols; ++unitCol) {
        const int x0 = unitCol * unitSize;
        const int x1 = unitCol == unitCols - 1 ? R.planeEndX : imin(R.planeEndX, (unitCol + 1) * unitSize - 1);
        if (x0 > x1) continue;
        const LrUnit& u = lr_units[plane][static_cast<std::size_t>(unitRow) * unitCols + unitCol];
        if (u.type == RESTORE_WIENER) {
            AV1_STAT(ST_LR_WIENER);
            wiener_region(R, out, u, x0, y0, x1 - x0 + 1, y1 - y0 + 1, bd, tmp);
        } else if (u.type == RESTORE_SGRPROJ) {
            AV1_STAT(ST_LR_SGR);
            sgr_region(R, out, u, x0, y0, x1 - x0 + 1, y1 - y0 + 1, bd, tmp, AB);
        }
    }
}

void Decoder::loop_restoration() {
    const int numStripes = (fh.FrameHeight + 8 + 63) / 64;
    std::vector<int> planes;
    for (int plane = 0; plane < seq.NumPlanes; ++plane) {
        if (fh.FrameRestorationType[plane] != RESTORE_NONE) planes.push_back(plane);
    }
    const int np = static_cast<int>(planes.size());
    parallel(np * numStripes, [&](int t) {
        thread_local std::vector<std::int32_t> tmp;
        thread_local std::vector<std::int32_t> AB;
        lr_stripe(planes[static_cast<std::size_t>(t / numStripes)], t % numStripes, tmp, AB);
    });
}

} // namespace av1
} // namespace video
} // namespace fizmo
