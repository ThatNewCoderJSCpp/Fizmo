#include "av1_internal.hpp"

#include <mutex>

#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define FIZMO_AV1_SSE2 1
#endif // __SSE2__

namespace fizmo {
namespace video {
namespace av1 {

namespace {

struct WedgeTable {
    std::vector<std::uint8_t> masks[BLOCK_SIZES];
    WedgeTable() {
        static std::uint8_t MasterMask[6][MASK_MASTER_SIZE][MASK_MASTER_SIZE];
        const int w = MASK_MASTER_SIZE;
        const int h = MASK_MASTER_SIZE;
        for (int j = 0; j < w; ++j) {
            int shift = MASK_MASTER_SIZE / 4;
            for (int i = 0; i < h; i += 2) {
                MasterMask[WEDGE_OBLIQUE63][i][j] = Wedge_Master_Oblique_Even[clip3(0, MASK_MASTER_SIZE - 1, j - shift)];
                shift -= 1;
                MasterMask[WEDGE_OBLIQUE63][i + 1][j] = Wedge_Master_Oblique_Odd[clip3(0, MASK_MASTER_SIZE - 1, j - shift)];
                MasterMask[WEDGE_VERTICAL][i][j] = Wedge_Master_Vertical[j];
                MasterMask[WEDGE_VERTICAL][i + 1][j] = Wedge_Master_Vertical[j];
            }
        }
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) {
                const int msk = MasterMask[WEDGE_OBLIQUE63][i][j];
                MasterMask[WEDGE_OBLIQUE27][j][i] = static_cast<std::uint8_t>(msk);
                MasterMask[WEDGE_OBLIQUE117][i][w - 1 - j] = static_cast<std::uint8_t>(64 - msk);
                MasterMask[WEDGE_OBLIQUE153][w - 1 - j][i] = static_cast<std::uint8_t>(64 - msk);
                MasterMask[WEDGE_HORIZONTAL][j][i] = MasterMask[WEDGE_VERTICAL][i][j];
            }
        }
        for (int bsize = BLOCK_8X8; bsize < BLOCK_SIZES; ++bsize) {
            if (Wedge_Bits[bsize] == 0) continue;
            const int bw = block_width(bsize);
            const int bh = block_height(bsize);
            masks[bsize].assign(static_cast<std::size_t>(2 * WEDGE_TYPES * bw * bh), 0);
            const int w4 = Num_4x4_Blocks_Wide[bsize];
            const int h4 = Num_4x4_Blocks_High[bsize];
            const int shape = h4 > w4 ? 0 : (h4 < w4 ? 1 : 2);
            for (int wedge = 0; wedge < WEDGE_TYPES; ++wedge) {
                const int dir = Wedge_Codebook[shape][wedge][0];
                const int xoff = MASK_MASTER_SIZE / 2 - ((Wedge_Codebook[shape][wedge][1] * bw) >> 3);
                const int yoff = MASK_MASTER_SIZE / 2 - ((Wedge_Codebook[shape][wedge][2] * bh) >> 3);
                int sum = 0;
                for (int i = 0; i < bw; ++i) sum += MasterMask[dir][yoff][xoff + i];
                for (int i = 1; i < bh; ++i) sum += MasterMask[dir][yoff + i][xoff];
                const int avg = (sum + (bw + bh - 1) / 2) / (bw + bh - 1);
                const int flipSign = avg < 32;
                for (int i = 0; i < bh; ++i) {
                    for (int j = 0; j < bw; ++j) {
                        const int v = MasterMask[dir][yoff + i][xoff + j];
                        masks[bsize][((static_cast<std::size_t>(flipSign) * WEDGE_TYPES + wedge) * bh + i) * bw + j] = static_cast<std::uint8_t>(v);
                        masks[bsize][((static_cast<std::size_t>(!flipSign) * WEDGE_TYPES + wedge) * bh + i) * bw + j] = static_cast<std::uint8_t>(64 - v);
                    }
                }
            }
        }
    }
};

const WedgeTable& wedge_table() {
    static const WedgeTable t;
    return t;
}

struct Rounding {
    int r0;
    int r1;
    int post;
};

Rounding rounding_vars(int isCompound, int BitDepth) {
    Rounding r;
    r.r0 = 3;
    r.r1 = isCompound ? 7 : 11;
    if (BitDepth == 12) r.r0 += 2;
    if (BitDepth == 12 && !isCompound) r.r1 -= 2;
    r.post = 2 * FILTER_BITS - (r.r0 + r.r1);
    return r;
}

void resolve_divisor(std::int64_t dd, int* divShift, int* divFactor) {
    const std::int64_t ad = dd < 0 ? -dd : dd;
    int n = 0;
    {
        std::uint64_t t = static_cast<std::uint64_t>(ad);
        while (t > 1) {
            t >>= 1;
            ++n;
        }
    }
    const std::int64_t e = ad - (std::int64_t(1) << n);
    std::int64_t f;
    if (n > DIV_LUT_BITS) {
        f = round2_64(e, n - DIV_LUT_BITS);
    } else {
        f = e << (DIV_LUT_BITS - n);
    }
    *divShift = n + DIV_LUT_PREC_BITS;
    if (dd < 0) {
        *divFactor = -static_cast<int>(Div_Lut[f]);
    } else {
        *divFactor = static_cast<int>(Div_Lut[f]);
    }
}

bool setup_shear(const int* warpParams, int* alpha, int* beta, int* gamma, int* delta) {
    const int alpha0 = clip3(-32768, 32767, warpParams[2] - (1 << WARPEDMODEL_PREC_BITS));
    const int beta0 = clip3(-32768, 32767, warpParams[3]);
    int divShift;
    int divFactor;
    resolve_divisor(warpParams[2], &divShift, &divFactor);
    const std::int64_t v = static_cast<std::int64_t>(warpParams[4]) * (1 << WARPEDMODEL_PREC_BITS);
    const int gamma0 = static_cast<int>(std::max<std::int64_t>(-32768, std::min<std::int64_t>(32767, round2signed_64(v * divFactor, divShift))));
    const std::int64_t w = static_cast<std::int64_t>(warpParams[3]) * warpParams[4];
    const int delta0 = static_cast<int>(std::max<std::int64_t>(-32768, std::min<std::int64_t>(32767, warpParams[5] - round2signed_64(w * divFactor, divShift) - (1 << WARPEDMODEL_PREC_BITS))));
    *alpha = round2signed(alpha0, WARP_PARAM_REDUCE_BITS) * (1 << WARP_PARAM_REDUCE_BITS);
    *beta = round2signed(beta0, WARP_PARAM_REDUCE_BITS) * (1 << WARP_PARAM_REDUCE_BITS);
    *gamma = round2signed(gamma0, WARP_PARAM_REDUCE_BITS) * (1 << WARP_PARAM_REDUCE_BITS);
    *delta = round2signed(delta0, WARP_PARAM_REDUCE_BITS) * (1 << WARP_PARAM_REDUCE_BITS);
    if (4 * iabs(*alpha) + 7 * iabs(*beta) >= (1 << WARPEDMODEL_PREC_BITS)) return false;
    if (4 * iabs(*gamma) + 4 * iabs(*delta) >= (1 << WARPEDMODEL_PREC_BITS)) return false;
    return true;
}

struct RefInfo {
    const Plane* plane;
    int upscaledWidth;
    int frameWidth;
    int frameHeight;
};

void scale_mv(int plane, int subsampling_x, int subsampling_y, int refUpscaledWidth, int refFrameHeight, int FrameWidth, int FrameHeight, int x, int y, Mv mv, int* startX, int* startY, int* stepX, int* stepY) {
    const int xScale = ((refUpscaledWidth << REF_SCALE_SHIFT) + (FrameWidth / 2)) / FrameWidth;
    const int yScale = ((refFrameHeight << REF_SCALE_SHIFT) + (FrameHeight / 2)) / FrameHeight;
    const int subX = plane == 0 ? 0 : subsampling_x;
    const int subY = plane == 0 ? 0 : subsampling_y;
    const int halfSample = 1 << (SUBPEL_BITS - 1);
    const int origX = ((x << SUBPEL_BITS) + ((2 * mv.col) >> subX) + halfSample);
    const int origY = ((y << SUBPEL_BITS) + ((2 * mv.row) >> subY) + halfSample);
    const std::int64_t baseX = static_cast<std::int64_t>(origX) * xScale - (halfSample << REF_SCALE_SHIFT);
    const std::int64_t baseY = static_cast<std::int64_t>(origY) * yScale - (halfSample << REF_SCALE_SHIFT);
    const int off = (1 << (SCALE_SUBPEL_BITS - SUBPEL_BITS)) / 2;
    *startX = static_cast<int>(round2signed_64(baseX, REF_SCALE_SHIFT + SUBPEL_BITS - SCALE_SUBPEL_BITS) + off);
    *startY = static_cast<int>(round2signed_64(baseY, REF_SCALE_SHIFT + SUBPEL_BITS - SCALE_SUBPEL_BITS) + off);
    *stepX = round2signed(xScale, REF_SCALE_SHIFT - SCALE_SUBPEL_BITS);
    *stepY = round2signed(yScale, REF_SCALE_SHIFT - SCALE_SUBPEL_BITS);
}

int adjust_filter(int interpFilter, int size) {
    if (size <= 4) {
        if (interpFilter == EIGHTTAP || interpFilter == EIGHTTAP_SHARP) return 4;
        if (interpFilter == EIGHTTAP_SMOOTH) return 5;
    }
    return interpFilter;
}

void block_inter_prediction(const Plane& ref, int lastX, int lastY, int x, int y, int xStep, int yStep, int w, int h, int filterX, int filterY, const Rounding& rv, std::int32_t* pred, std::int32_t* inter, std::uint16_t* window, std::int16_t* inter16) {
    const int fx = adjust_filter(filterX, w);
    const int fy = adjust_filter(filterY, h);
    if (xStep == 1024 && yStep == 1024) {
        const int ix0 = (x >> 10) - 3;
        const int iy0 = (y >> 10) - 3;
        const int fracX = (x >> 6) & SUBPEL_MASK;
        const int fracY = (y >> 6) & SUBPEL_MASK;
        const int ww = w + 7;
        const int hh = h + 7;
        const std::uint16_t* src;
        std::ptrdiff_t sstride;
        if (ix0 >= 0 && iy0 >= 0 && ix0 + ww - 1 <= lastX && iy0 + hh - 1 <= lastY) {
            src = ref.row(iy0) + ix0;
            sstride = ref.stride;
        } else {
            for (int r = 0; r < hh; ++r) {
                const std::uint16_t* srow = ref.row(clip3(0, lastY, iy0 + r));
                std::uint16_t* drow = window + r * ww;
                for (int c = 0; c < ww; ++c) drow[c] = srow[clip3(0, lastX, ix0 + c)];
            }
            src = window;
            sstride = ww;
        }
        const std::int16_t* kx = Subpel_Filters[fx][fracX];
        const int add0 = (1 << rv.r0) >> 1;
#if defined(FIZMO_AV1_SSE2)
        if ((w & 3) == 0) {
            std::int16_t* i16 = inter16;
            const std::int16_t* ky = Subpel_Filters[fy][fracY];
            const __m128i k01 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(kx[1])) << 16) | static_cast<std::uint16_t>(kx[0])));
            const __m128i k23 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(kx[3])) << 16) | static_cast<std::uint16_t>(kx[2])));
            const __m128i k45 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(kx[5])) << 16) | static_cast<std::uint16_t>(kx[4])));
            const __m128i k67 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(kx[7])) << 16) | static_cast<std::uint16_t>(kx[6])));
            const __m128i vadd0 = _mm_set1_epi32(add0);
            const __m128i sh0 = _mm_cvtsi32_si128(rv.r0);
            const __m128i shx = _mm_cvtsi32_si128(FILTER_BITS - rv.r0);
            for (int r = 0; r < hh; ++r) {
                const std::uint16_t* sr = src + r * sstride;
                std::int16_t* o = i16 + r * w;
                if (fracX == 0) {
                    for (int c = 0; c < w; c += 4) {
                        const __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(sr + c + 3));
                        _mm_storel_epi64(reinterpret_cast<__m128i*>(o + c), _mm_sll_epi16(v, shx));
                    }
                    continue;
                }
                for (int c = 0; c < w; c += 4) {
                    const std::uint16_t* q = sr + c;
                    const __m128i a0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q));
                    const __m128i a1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 1));
                    const __m128i a2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 2));
                    const __m128i a3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 3));
                    const __m128i a4 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 4));
                    const __m128i a5 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 5));
                    const __m128i a6 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 6));
                    const __m128i a7 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 7));
                    __m128i sum = _mm_madd_epi16(_mm_unpacklo_epi16(a0, a1), k01);
                    sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(a2, a3), k23));
                    sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(a4, a5), k45));
                    sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(a6, a7), k67));
                    sum = _mm_sra_epi32(_mm_add_epi32(sum, vadd0), sh0);
                    _mm_storel_epi64(reinterpret_cast<__m128i*>(o + c), _mm_packs_epi32(sum, sum));
                }
            }
            const __m128i add1v = _mm_set1_epi32((1 << rv.r1) >> 1);
            const __m128i sh1 = _mm_cvtsi32_si128(rv.r1);
            if (fracY == 0) {
                for (int r = 0; r < h; ++r) {
                    const std::int16_t* sr = i16 + (r + 3) * w;
                    std::int32_t* o = pred + r * w;
                    for (int c = 0; c < w; c += 4) {
                        const __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(sr + c));
                        const __m128i v32 = _mm_srai_epi32(_mm_unpacklo_epi16(v, v), 16);
                        __m128i t = _mm_slli_epi32(v32, 7);
                        t = _mm_sra_epi32(_mm_add_epi32(t, add1v), sh1);
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(o + c), t);
                    }
                }
            } else {
                const __m128i c01 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(ky[1])) << 16) | static_cast<std::uint16_t>(ky[0])));
                const __m128i c23 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(ky[3])) << 16) | static_cast<std::uint16_t>(ky[2])));
                const __m128i c45 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(ky[5])) << 16) | static_cast<std::uint16_t>(ky[4])));
                const __m128i c67 = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(static_cast<std::uint16_t>(ky[7])) << 16) | static_cast<std::uint16_t>(ky[6])));
                for (int r = 0; r < h; ++r) {
                    const std::int16_t* sr = i16 + r * w;
                    std::int32_t* o = pred + r * w;
                    for (int c = 0; c < w; c += 4) {
                        const std::int16_t* q = sr + c;
                        const __m128i b0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q));
                        const __m128i b1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + w));
                        const __m128i b2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 2 * w));
                        const __m128i b3 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 3 * w));
                        const __m128i b4 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 4 * w));
                        const __m128i b5 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 5 * w));
                        const __m128i b6 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 6 * w));
                        const __m128i b7 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(q + 7 * w));
                        __m128i sum = _mm_madd_epi16(_mm_unpacklo_epi16(b0, b1), c01);
                        sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(b2, b3), c23));
                        sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(b4, b5), c45));
                        sum = _mm_add_epi32(sum, _mm_madd_epi16(_mm_unpacklo_epi16(b6, b7), c67));
                        sum = _mm_sra_epi32(_mm_add_epi32(sum, add1v), sh1);
                        _mm_storeu_si128(reinterpret_cast<__m128i*>(o + c), sum);
                    }
                }
            }
            return;
        }
#endif // FIZMO_AV1_SSE2
        if (fracX == 0) {
            const int sh = FILTER_BITS - rv.r0;
            for (int r = 0; r < hh; ++r) {
                const std::uint16_t* s = src + r * sstride + 3;
                std::int32_t* o = inter + r * w;
                for (int c = 0; c < w; ++c) o[c] = static_cast<std::int32_t>(s[c]) << sh;
            }
        } else {
            for (int r = 0; r < hh; ++r) {
                const std::uint16_t* s = src + r * sstride;
                std::int32_t* o = inter + r * w;
                for (int c = 0; c < w; ++c) {
                    const std::uint16_t* p = s + c;
                    const int sum = kx[0] * p[0] + kx[1] * p[1] + kx[2] * p[2] + kx[3] * p[3] + kx[4] * p[4] + kx[5] * p[5] + kx[6] * p[6] + kx[7] * p[7];
                    o[c] = (sum + add0) >> rv.r0;
                }
            }
        }
        const std::int16_t* ky = Subpel_Filters[fy][fracY];
        const int add1 = (1 << rv.r1) >> 1;
        if (fracY == 0) {
            for (int r = 0; r < h; ++r) {
                const std::int32_t* s = inter + (r + 3) * w;
                std::int32_t* o = pred + r * w;
                for (int c = 0; c < w; ++c) o[c] = (s[c] * 128 + add1) >> rv.r1;
            }
        } else {
            for (int r = 0; r < h; ++r) {
                const std::int32_t* s = inter + r * w;
                std::int32_t* o = pred + r * w;
                for (int c = 0; c < w; ++c) {
                    const std::int32_t* p = s + c;
                    const int sum = ky[0] * p[0] + ky[1] * p[w] + ky[2] * p[2 * w] + ky[3] * p[3 * w] + ky[4] * p[4 * w] + ky[5] * p[5 * w] + ky[6] * p[6 * w] + ky[7] * p[7 * w];
                    o[c] = (sum + add1) >> rv.r1;
                }
            }
        }
        return;
    }
    const int intermediateHeight = (((h - 1) * yStep + (1 << SCALE_SUBPEL_BITS) - 1) >> SCALE_SUBPEL_BITS) + 8;
    for (int r = 0; r < intermediateHeight; ++r) {
        const std::uint16_t* srow = ref.row(clip3(0, lastY, (y >> 10) + r - 3));
        for (int c = 0; c < w; ++c) {
            int s = 0;
            const int p = x + xStep * c;
            const std::int16_t* k = Subpel_Filters[fx][(p >> 6) & SUBPEL_MASK];
            for (int t = 0; t < 8; ++t) s += k[t] * srow[clip3(0, lastX, (p >> 10) + t - 3)];
            inter[r * w + c] = round2(s, rv.r0);
        }
    }
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            int s = 0;
            const int p = (y & 1023) + yStep * r;
            const std::int16_t* k = Subpel_Filters[fy][(p >> 6) & SUBPEL_MASK];
            for (int t = 0; t < 8; ++t) s += k[t] * inter[((p >> 10) + t) * w + c];
            pred[r * w + c] = round2(s, rv.r1);
        }
    }
}

void block_warp(const int* warpParams, const Plane& ref, int lastX, int lastY, int subX, int subY, int x, int y, int i8, int j8, int w, int h, const Rounding& rv, std::int32_t* pred, int predStride) {
    const int srcX = (x + j8 * 8 + 4) << subX;
    const int srcY = (y + i8 * 8 + 4) << subY;
    const std::int64_t dstX = static_cast<std::int64_t>(warpParams[2]) * srcX + static_cast<std::int64_t>(warpParams[3]) * srcY + warpParams[0];
    const std::int64_t dstY = static_cast<std::int64_t>(warpParams[4]) * srcX + static_cast<std::int64_t>(warpParams[5]) * srcY + warpParams[1];
    int alpha;
    int beta;
    int gamma;
    int delta;
    setup_shear(warpParams, &alpha, &beta, &gamma, &delta);
    const std::int64_t x4 = dstX >> subX;
    const std::int64_t y4 = dstY >> subY;
    const int ix4 = static_cast<int>(x4 >> WARPEDMODEL_PREC_BITS);
    const int sx4 = static_cast<int>(x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1));
    const int iy4 = static_cast<int>(y4 >> WARPEDMODEL_PREC_BITS);
    const int sy4 = static_cast<int>(y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1));
    int intermediate[15][8];
    for (int i1 = -7; i1 < 8; ++i1) {
        const std::uint16_t* srow = ref.row(clip3(0, lastY, iy4 + i1));
        for (int i2 = -4; i2 < 4; ++i2) {
            const int sx = sx4 + alpha * i2 + beta * i1;
            const int offs = round2(sx, WARPEDDIFF_PREC_BITS) + WARPEDPIXEL_PREC_SHIFTS;
            const std::int8_t* k = Warped_Filters[clip3(0, WARPEDPIXEL_PREC_SHIFTS * 3, offs)];
            int s = 0;
            for (int i3 = 0; i3 < 8; ++i3) s += k[i3] * srow[clip3(0, lastX, ix4 + i2 - 3 + i3)];
            intermediate[i1 + 7][i2 + 4] = round2(s, rv.r0);
        }
    }
    for (int i1 = -4; i1 < imin(4, h - i8 * 8 - 4); ++i1) {
        for (int i2 = -4; i2 < imin(4, w - j8 * 8 - 4); ++i2) {
            const int sy = sy4 + gamma * i2 + delta * i1;
            const int offs = round2(sy, WARPEDDIFF_PREC_BITS) + WARPEDPIXEL_PREC_SHIFTS;
            const std::int8_t* k = Warped_Filters[clip3(0, WARPEDPIXEL_PREC_SHIFTS * 3, offs)];
            int s = 0;
            for (int i3 = 0; i3 < 8; ++i3) s += k[i3] * intermediate[i1 + i3 + 4][i2 + 4];
            pred[(i8 * 8 + i1 + 4) * predStride + j8 * 8 + i2 + 4] = round2(s, rv.r1);
        }
    }
}

} // namespace

bool Tile::warp_estimation(WarpParams& wp) {
    std::int64_t A[2][2] = {{0, 0}, {0, 0}};
    std::int64_t Bx[2] = {0, 0};
    std::int64_t By[2] = {0, 0};
    const int w4 = Num_4x4_Blocks_Wide[MiSize];
    const int h4 = Num_4x4_Blocks_High[MiSize];
    const int midY = MiRow * 4 + h4 * 2 - 1;
    const int midX = MiCol * 4 + w4 * 2 - 1;
    const int suy = midY * 8;
    const int sux = midX * 8;
    const int duy = suy + MvVal[0].row;
    const int dux = sux + MvVal[0].col;
    auto ls_product = [](std::int64_t a, std::int64_t b) { return ((a * b) >> 2) + (a + b); };
    for (int i = 0; i < NumSamples; ++i) {
        const int sy = CandList[i][0] - suy;
        const int sx = CandList[i][1] - sux;
        const int dy = CandList[i][2] - duy;
        const int dx = CandList[i][3] - dux;
        if (iabs(sx - dx) < LS_MV_MAX && iabs(sy - dy) < LS_MV_MAX) {
            A[0][0] += ls_product(sx, sx) + 8;
            A[0][1] += ls_product(sx, sy) + 4;
            A[1][1] += ls_product(sy, sy) + 8;
            Bx[0] += ls_product(sx, dx) + 8;
            Bx[1] += ls_product(sy, dx) + 4;
            By[0] += ls_product(sx, dy) + 4;
            By[1] += ls_product(sy, dy) + 8;
        }
    }
    const std::int64_t det = A[0][0] * A[1][1] - A[0][1] * A[0][1];
    if (det == 0) {
        wp.valid = false;
        return false;
    }
    wp.valid = true;
    int divShift;
    int divFactor;
    resolve_divisor(det, &divShift, &divFactor);
    divShift -= WARPEDMODEL_PREC_BITS;
    std::int64_t factor = divFactor;
    if (divShift < 0) {
        factor = factor * (std::int64_t(1) << (-divShift));
        divShift = 0;
    }
    auto mulround = [&](std::int64_t v) {
        const std::int64_t prod = v * factor;
        return round2signed_64(prod, divShift);
    };
    auto nondiag = [&](std::int64_t v) {
        return static_cast<int>(std::max<std::int64_t>(-WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1, std::min<std::int64_t>(WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1, mulround(v))));
    };
    auto diag = [&](std::int64_t v) {
        return static_cast<int>(std::max<std::int64_t>((1 << WARPEDMODEL_PREC_BITS) - WARPEDMODEL_NONDIAGAFFINE_CLAMP + 1, std::min<std::int64_t>((1 << WARPEDMODEL_PREC_BITS) + WARPEDMODEL_NONDIAGAFFINE_CLAMP - 1, mulround(v))));
    };
    wp.m[2] = diag(A[1][1] * Bx[0] - A[0][1] * Bx[1]);
    wp.m[3] = nondiag(-A[0][1] * Bx[0] + A[0][0] * Bx[1]);
    wp.m[4] = nondiag(A[1][1] * By[0] - A[0][1] * By[1]);
    wp.m[5] = diag(-A[0][1] * By[0] + A[0][0] * By[1]);
    const std::int64_t mvx = MvVal[0].col;
    const std::int64_t mvy = MvVal[0].row;
    const std::int64_t vx = mvx * (1 << (WARPEDMODEL_PREC_BITS - 3)) - (static_cast<std::int64_t>(midX) * (wp.m[2] - (1 << WARPEDMODEL_PREC_BITS)) + static_cast<std::int64_t>(midY) * wp.m[3]);
    const std::int64_t vy = mvy * (1 << (WARPEDMODEL_PREC_BITS - 3)) - (static_cast<std::int64_t>(midX) * wp.m[4] + static_cast<std::int64_t>(midY) * (wp.m[5] - (1 << WARPEDMODEL_PREC_BITS)));
    wp.m[0] = static_cast<int>(std::max<std::int64_t>(-WARPEDMODEL_TRANS_CLAMP, std::min<std::int64_t>(WARPEDMODEL_TRANS_CLAMP - 1, vx)));
    wp.m[1] = static_cast<int>(std::max<std::int64_t>(-WARPEDMODEL_TRANS_CLAMP, std::min<std::int64_t>(WARPEDMODEL_TRANS_CLAMP - 1, vy)));
    return true;
}

void Tile::compute_prediction() {
    const int sbMask = seq->use_128x128_superblock ? 31 : 15;
    const int subBlockMiRow = MiRow & sbMask;
    const int subBlockMiCol = MiCol & sbMask;
    for (int plane = 0; plane < 1 + HasChroma * 2; ++plane) {
        const int planeSz = get_plane_residual_size(MiSize, plane);
        const int num4x4W = Num_4x4_Blocks_Wide[planeSz];
        const int num4x4H = Num_4x4_Blocks_High[planeSz];
        const int log2W = MI_SIZE_LOG2 + Mi_Width_Log2[planeSz];
        const int log2H = MI_SIZE_LOG2 + Mi_Height_Log2[planeSz];
        const int subX = plane > 0 ? seq->subsampling_x : 0;
        const int subY = plane > 0 ? seq->subsampling_y : 0;
        const int baseX = (MiCol >> subX) * MI_SIZE;
        const int baseY = (MiRow >> subY) * MI_SIZE;
        int candRow = (MiRow >> subY) << subY;
        int candCol = (MiCol >> subX) << subX;
        const int IsInterIntra = is_inter && RefFrame[1] == INTRA_FRAME;
        if (IsInterIntra) {
            if (wedge_interintra) {
                AV1_STAT(ST_II_WEDGE);
            } else {
                AV1_STAT(ST_II_SMOOTH);
            }
            if (interintra_mode == II_DC_PRED) AV1_STAT(ST_II_DC);
            if (interintra_mode == II_V_PRED) AV1_STAT(ST_II_V);
            if (interintra_mode == II_H_PRED) AV1_STAT(ST_II_H);
            int mode;
            if (interintra_mode == II_DC_PRED) {
                mode = DC_PRED;
            } else if (interintra_mode == II_V_PRED) {
                mode = V_PRED;
            } else if (interintra_mode == II_H_PRED) {
                mode = H_PRED;
            } else {
                mode = SMOOTH_PRED;
            }
            predict_intra(plane, baseX, baseY, plane == 0 ? AvailL : AvailLChroma, plane == 0 ? AvailU : AvailUChroma,
                          block_decoded(plane, (subBlockMiRow >> subY) - 1, (subBlockMiCol >> subX) + num4x4W),
                          block_decoded(plane, (subBlockMiRow >> subY) + num4x4H, (subBlockMiCol >> subX) - 1), mode, log2W, log2H);
        }
        if (is_inter) {
            int predW = block_width(MiSize) >> subX;
            int predH = block_height(MiSize) >> subY;
            int someUseIntra = 0;
            for (int r = 0; r < (num4x4H << subY); ++r) {
                for (int c = 0; c < (num4x4W << subX); ++c) {
                    if (mi(candRow + r, candCol + c).ref_frame[0] == INTRA_FRAME) someUseIntra = 1;
                }
            }
            if (!someUseIntra && (predW < num4x4W * 4 || predH < num4x4H * 4)) AV1_STAT(ST_CHROMA_SUBPRED);
            if (someUseIntra) {
                predW = num4x4W * 4;
                predH = num4x4H * 4;
                candRow = MiRow;
                candCol = MiCol;
            }
            int r = 0;
            for (int y = 0; y < num4x4H * 4; y += predH) {
                int c = 0;
                for (int x = 0; x < num4x4W * 4; x += predW) {
                    predict_inter(plane, baseX + x, baseY + y, predW, predH, candRow + r, candCol + c);
                    ++c;
                }
                ++r;
            }
        }
    }
}

void Tile::predict_inter(int plane, int x, int y, int w, int h, int candRow, int candCol) {
    const MiInfo& cand = mi(candRow, candCol);
    const int isCompound = cand.ref_frame[1] > INTRA_FRAME;
    const int BitDepth = seq->BitDepth;
    const int maxv = (1 << BitDepth) - 1;
    const Rounding rv = rounding_vars(isCompound, BitDepth);
    const int subX = plane == 0 ? 0 : seq->subsampling_x;
    const int subY = plane == 0 ? 0 : seq->subsampling_y;
    if (plane == 0 && motion_mode == LOCALWARP) {
        warp_estimation(LocalWarp);
        if (LocalWarp.valid) {
            int a;
            int b;
            int g;
            int dd;
            LocalWarp.valid = setup_shear(LocalWarp.m, &a, &b, &g, &dd);
        }
    }
    const std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (int k = 0; k < 2; ++k) {
        if (preds[k].size() < 128 * 128) preds[k].resize(128 * 128);
    }
    if (scratch32.size() < 270 * 128) scratch32.resize(270 * 128);
    if (scratch16.size() < 140 * 140) scratch16.resize(140 * 140);
    if (scratch_i16.size() < 140 * 128) scratch_i16.resize(140 * 128);
    for (int refList = 0; refList < 1 + isCompound; ++refList) {
        const int refFrame = cand.ref_frame[refList];
        int globalValid = 0;
        if ((YMode == GLOBALMV || YMode == GLOBAL_GLOBALMV) && fh->GmType[refFrame > 0 ? refFrame : 0] > TRANSLATION) {
            int a;
            int b;
            int g;
            int dd;
            globalValid = setup_shear(fh->gm_params[refFrame], &a, &b, &g, &dd);
        }
        int useWarp;
        if (w < 8 || h < 8) {
            useWarp = 0;
        } else if (fh->force_integer_mv == 1) {
            useWarp = 0;
        } else if (motion_mode == LOCALWARP && LocalWarp.valid) {
            useWarp = 1;
        } else if ((YMode == GLOBALMV || YMode == GLOBAL_GLOBALMV) && fh->GmType[refFrame > 0 ? refFrame : 0] > TRANSLATION && !is_scaled(refFrame) && globalValid) {
            useWarp = 2;
        } else {
            useWarp = 0;
        }
        const Mv mv = cand.mv[refList];
        const Plane* refPlane;
        int refUpscaledWidth;
        int refFrameHeight;
        int lastX;
        int lastY;
        if (!use_intrabc) {
            const SavedFrame* rf = d->refs[fh->ref_frame_idx[refFrame - LAST_FRAME]].get();
            refPlane = &rf->frame.planes[plane];
            refUpscaledWidth = rf->upscaled_width;
            refFrameHeight = rf->frame_height;
            lastX = ((refUpscaledWidth + subX) >> subX) - 1;
            lastY = ((refFrameHeight + subY) >> subY) - 1;
        } else {
            refPlane = &d->frame.planes[plane];
            refUpscaledWidth = fh->UpscaledWidth;
            refFrameHeight = fh->FrameHeight;
            lastX = ((fh->MiCols * MI_SIZE + subX) >> subX) - 1;
            lastY = ((fh->MiRows * MI_SIZE + subY) >> subY) - 1;
        }
        std::int32_t* pred = preds[refList].data();
        if (useWarp == 1) AV1_STAT(ST_LOCALWARP);
        if (useWarp == 2) AV1_STAT(ST_GLOBALWARP);
        if (cand.interp_filter[0] == BILINEAR) AV1_STAT(ST_BILINEAR);
        if (useWarp != 0) {
            const int* params = useWarp == 1 ? LocalWarp.m : fh->gm_params[refFrame];
            for (int i8 = 0; i8 <= ((h - 1) >> 3); ++i8) {
                for (int j8 = 0; j8 <= ((w - 1) >> 3); ++j8) block_warp(params, *refPlane, lastX, lastY, subX, subY, x, y, i8, j8, w, h, rv, pred, w);
            }
        } else {
            int startX;
            int startY;
            int stepX;
            int stepY;
            scale_mv(plane, seq->subsampling_x, seq->subsampling_y, refUpscaledWidth, refFrameHeight, fh->FrameWidth, fh->FrameHeight, x, y, mv, &startX, &startY, &stepX, &stepY);
            if (stepX != 1024 || stepY != 1024) AV1_STAT(ST_SCALED_REF);
            block_inter_prediction(*refPlane, lastX, lastY, startX, startY, stepX, stepY, w, h, cand.interp_filter[1], cand.interp_filter[0], rv, pred, scratch32.data(), scratch16.data(), scratch_i16.data());
        }
    }
    if (plane == 0 && isCompound) {
        if (compound_type == COMPOUND_WEDGE) AV1_STAT(ST_COMP_WEDGE);
        if (compound_type == COMPOUND_DIFFWTD) AV1_STAT(ST_COMP_DIFFWTD);
        if (compound_type == COMPOUND_DISTANCE) AV1_STAT(ST_COMP_DIST);
        if (compound_type == COMPOUND_AVERAGE) AV1_STAT(ST_COMP_AVG);
    }
    if (plane == 0 && motion_mode == OBMC) AV1_STAT(ST_OBMC);
    (void)need;
    Plane& P = d->frame.planes[plane];
    const int IsInterIntra = is_inter && RefFrame[1] == INTRA_FRAME;
    if (mask.size() < 128 * 128) mask.resize(128 * 128);
    if (compound_type == COMPOUND_WEDGE && plane == 0) {
        const WedgeTable& wt = wedge_table();
        const int bw = block_width(MiSize);
        const int bh = block_height(MiSize);
        const std::uint8_t* src = wt.masks[MiSize].data() + (static_cast<std::size_t>(wedge_sign) * WEDGE_TYPES + wedge_index) * bw * bh;
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) mask[i * 128 + j] = src[i * bw + j];
        }
    } else if (compound_type == COMPOUND_INTRA) {
        const int sizeScale = MAX_SB_SIZE / imax(h, w);
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) {
                int m;
                if (interintra_mode == II_V_PRED) {
                    m = Ii_Weights_1d[i * sizeScale];
                } else if (interintra_mode == II_H_PRED) {
                    m = Ii_Weights_1d[j * sizeScale];
                } else if (interintra_mode == II_SMOOTH_PRED) {
                    m = Ii_Weights_1d[imin(i, j) * sizeScale];
                } else {
                    m = 32;
                }
                mask[i * 128 + j] = static_cast<std::uint8_t>(m);
            }
        }
    } else if (compound_type == COMPOUND_DIFFWTD && plane == 0) {
        const std::int32_t* p0 = preds[0].data();
        const std::int32_t* p1 = preds[1].data();
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) {
                int diff = iabs(p0[i * w + j] - p1[i * w + j]);
                diff = round2(diff, (BitDepth - 8) + rv.post);
                const int m = clip3(0, 64, 38 + diff / 16);
                mask[i * 128 + j] = static_cast<std::uint8_t>(mask_type ? 64 - m : m);
            }
        }
    }
    int FwdWeight = 0;
    int BckWeight = 0;
    if (compound_type == COMPOUND_DISTANCE) {
        int dist[2];
        for (int refList = 0; refList < 2; ++refList) {
            const int hh = fh->OrderHints[cand.ref_frame[refList]];
            dist[refList] = clip3(0, MAX_FRAME_DISTANCE, iabs(d->get_relative_dist(hh, fh->OrderHint)));
        }
        const int d0 = dist[1];
        const int d1 = dist[0];
        const int order = d0 <= d1;
        if (d0 == 0 || d1 == 0) {
            FwdWeight = Quant_Dist_Lookup[3][order];
            BckWeight = Quant_Dist_Lookup[3][1 - order];
        } else {
            int i;
            for (i = 0; i < 3; ++i) {
                const int c0 = Quant_Dist_Weight[i][order];
                const int c1 = Quant_Dist_Weight[i][1 - order];
                if (order) {
                    if (d0 * c0 > d1 * c1) break;
                } else {
                    if (d0 * c0 < d1 * c1) break;
                }
            }
            FwdWeight = Quant_Dist_Lookup[i][order];
            BckWeight = Quant_Dist_Lookup[i][1 - order];
        }
    }
    const std::int32_t* p0 = preds[0].data();
    const std::int32_t* p1 = preds[1].data();
    if (!isCompound && !IsInterIntra) {
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = P.row(y + i) + x;
            const std::int32_t* s = p0 + i * w;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(clip3(0, maxv, s[j]));
        }
    } else if (compound_type == COMPOUND_AVERAGE) {
        const int sh = 1 + rv.post;
        const int add = 1 << (sh - 1);
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = P.row(y + i) + x;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(clip3(0, maxv, (p0[i * w + j] + p1[i * w + j] + add) >> sh));
        }
    } else if (compound_type == COMPOUND_DISTANCE) {
        const int sh = 4 + rv.post;
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = P.row(y + i) + x;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(clip3(0, maxv, round2(FwdWeight * p0[i * w + j] + BckWeight * p1[i * w + j], sh)));
        }
    } else {
        for (int yy = 0; yy < h; ++yy) {
            std::uint16_t* o = P.row(y + yy) + x;
            for (int xx = 0; xx < w; ++xx) {
                int m;
                if ((!subX && !subY) || (interintra && !wedge_interintra)) {
                    m = mask[yy * 128 + xx];
                } else if (subX && !subY) {
                    m = round2(mask[yy * 128 + 2 * xx] + mask[yy * 128 + 2 * xx + 1], 1);
                } else {
                    m = round2(mask[(2 * yy) * 128 + 2 * xx] + mask[(2 * yy) * 128 + 2 * xx + 1] + mask[(2 * yy + 1) * 128 + 2 * xx] + mask[(2 * yy + 1) * 128 + 2 * xx + 1], 2);
                }
                if (interintra) {
                    const int pred0 = clip3(0, maxv, round2(p0[yy * w + xx], rv.post));
                    const int pred1 = o[xx];
                    o[xx] = static_cast<std::uint16_t>(round2(m * pred1 + (64 - m) * pred0, 6));
                } else {
                    const int pred0 = p0[yy * w + xx];
                    const int pred1 = p1[yy * w + xx];
                    o[xx] = static_cast<std::uint16_t>(clip3(0, maxv, round2(m * pred0 + (64 - m) * pred1, 6 + rv.post)));
                }
            }
        }
    }
    if (motion_mode == OBMC) overlapped_motion_compensation(plane, w, h);
}

void Tile::overlapped_motion_compensation(int plane, int w, int h) {
    const int subX = plane == 0 ? 0 : seq->subsampling_x;
    const int subY = plane == 0 ? 0 : seq->subsampling_y;
    const int BitDepth = seq->BitDepth;
    const int maxv = (1 << BitDepth) - 1;
    const Rounding rv = rounding_vars(0, BitDepth);
    Plane& P = d->frame.planes[plane];
    if (obmc_buf.size() < 128 * 128) obmc_buf.resize(128 * 128);
    std::int32_t* opred = preds[1].data();
    auto get_mask = [](int length) -> const std::uint8_t* {
        if (length == 2) return Obmc_Mask_2;
        if (length == 4) return Obmc_Mask_4;
        if (length == 8) return Obmc_Mask_8;
        if (length == 16) return Obmc_Mask_16;
        return Obmc_Mask_32;
    };
    auto predict_overlap = [&](int candRow, int candCol, int x4, int y4, int predW, int predH, int pass, const std::uint8_t* msk) {
        const MiInfo& cand = mi(candRow, candCol);
        const Mv mv = cand.mv[0];
        const SavedFrame* rf = d->refs[fh->ref_frame_idx[cand.ref_frame[0] - LAST_FRAME]].get();
        const int predX = (x4 * 4) >> subX;
        const int predY = (y4 * 4) >> subY;
        int startX;
        int startY;
        int stepX;
        int stepY;
        scale_mv(plane, seq->subsampling_x, seq->subsampling_y, rf->upscaled_width, rf->frame_height, fh->FrameWidth, fh->FrameHeight, predX, predY, mv, &startX, &startY, &stepX, &stepY);
        const int lastX = ((rf->upscaled_width + subX) >> subX) - 1;
        const int lastY = ((rf->frame_height + subY) >> subY) - 1;
        block_inter_prediction(rf->frame.planes[plane], lastX, lastY, startX, startY, stepX, stepY, predW, predH, cand.interp_filter[1], cand.interp_filter[0], rv, opred, scratch32.data(), scratch16.data(), scratch_i16.data());
        for (int i = 0; i < predH; ++i) {
            std::uint16_t* o = P.row(predY + i) + predX;
            for (int j = 0; j < predW; ++j) {
                const int ob = clip3(0, maxv, opred[i * predW + j]);
                const int m = pass == 0 ? msk[i] : msk[j];
                o[j] = static_cast<std::uint16_t>(round2(m * o[j] + (64 - m) * ob, 6));
            }
        }
    };
    if (AvailU) {
        if (get_plane_residual_size(MiSize, plane) >= BLOCK_8X8) {
            const int w4 = Num_4x4_Blocks_Wide[MiSize];
            int x4 = MiCol;
            const int y4 = MiRow;
            int nCount = 0;
            const int nLimit = imin(4, Mi_Width_Log2[MiSize]);
            while (nCount < nLimit && x4 < imin(fh->MiCols, MiCol + w4)) {
                const int candRow = MiRow - 1;
                const int candCol = x4 | 1;
                const int candSz = mi(candRow, candCol).mi_size;
                const int step4 = clip3(2, 16, Num_4x4_Blocks_Wide[candSz]);
                if (mi(candRow, candCol).ref_frame[0] > INTRA_FRAME) {
                    nCount += 1;
                    const int predW = imin(w, (step4 * MI_SIZE) >> subX);
                    const int predH = imin(h >> 1, 32 >> subY);
                    predict_overlap(candRow, candCol, x4, y4, predW, predH, 0, get_mask(predH));
                }
                x4 += step4;
            }
        }
    }
    if (AvailL) {
        const int h4 = Num_4x4_Blocks_High[MiSize];
        const int x4 = MiCol;
        int y4 = MiRow;
        int nCount = 0;
        const int nLimit = imin(4, Mi_Height_Log2[MiSize]);
        while (nCount < nLimit && y4 < imin(fh->MiRows, MiRow + h4)) {
            const int candCol = MiCol - 1;
            const int candRow = y4 | 1;
            const int candSz = mi(candRow, candCol).mi_size;
            const int step4 = clip3(2, 16, Num_4x4_Blocks_High[candSz]);
            if (mi(candRow, candCol).ref_frame[0] > INTRA_FRAME) {
                nCount += 1;
                const int predW = imin(w >> 1, 32 >> subX);
                const int predH = imin(h, (step4 * MI_SIZE) >> subY);
                predict_overlap(candRow, candCol, x4, y4, predW, predH, 1, get_mask(predW));
            }
            y4 += step4;
        }
    }
}

} // namespace av1
} // namespace video
} // namespace fizmo
