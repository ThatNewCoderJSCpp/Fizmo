#include "h264_internal.hpp"

#include <cstdlib>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define FIZMO_H264_SSE2 1
#include <emmintrin.h>
#endif // __SSE2__

namespace fizmo {
namespace video {
namespace h264_internal {

void idct4x4_add(std::uint8_t* dst, int stride, std::int32_t* b) noexcept {
    std::int32_t t[16];
    for (int i = 0; i < 4; ++i) {
        const std::int32_t* d = b + i * 4;
        std::int32_t e = d[0] + d[2];
        std::int32_t f = d[0] - d[2];
        std::int32_t g = (d[1] >> 1) - d[3];
        std::int32_t h = d[1] + (d[3] >> 1);
        t[i * 4 + 0] = e + h;
        t[i * 4 + 1] = f + g;
        t[i * 4 + 2] = f - g;
        t[i * 4 + 3] = e - h;
    }
    for (int j = 0; j < 4; ++j) {
        std::int32_t e = t[j] + t[8 + j];
        std::int32_t f = t[j] - t[8 + j];
        std::int32_t g = (t[4 + j] >> 1) - t[12 + j];
        std::int32_t h = t[4 + j] + (t[12 + j] >> 1);
        dst[j] = clip_pixel(dst[j] + ((e + h + 32) >> 6));
        dst[stride + j] = clip_pixel(dst[stride + j] + ((f + g + 32) >> 6));
        dst[2 * stride + j] = clip_pixel(dst[2 * stride + j] + ((f - g + 32) >> 6));
        dst[3 * stride + j] = clip_pixel(dst[3 * stride + j] + ((e - h + 32) >> 6));
    }
    std::memset(b, 0, 16 * sizeof(std::int32_t));
}

void idct4x4_dc_add(std::uint8_t* dst, int stride, std::int32_t* b) noexcept {
    int dc = (b[0] + 32) >> 6;
    b[0] = 0;
    for (int y = 0; y < 4; ++y) {
        std::uint8_t* row = dst + y * stride;
        for (int x = 0; x < 4; ++x) row[x] = clip_pixel(row[x] + dc);
    }
}

namespace {

inline void idct8_1d(const std::int32_t* d, int s, std::int32_t* f, int fs) noexcept {
    std::int32_t d0 = d[0], d1 = d[s], d2 = d[2 * s], d3 = d[3 * s];
    std::int32_t d4 = d[4 * s], d5 = d[5 * s], d6 = d[6 * s], d7 = d[7 * s];
    std::int32_t a0 = d0 + d4;
    std::int32_t a4 = d0 - d4;
    std::int32_t a2 = (d2 >> 1) - d6;
    std::int32_t a6 = d2 + (d6 >> 1);
    std::int32_t b0 = a0 + a6;
    std::int32_t b2 = a4 + a2;
    std::int32_t b4 = a4 - a2;
    std::int32_t b6 = a0 - a6;
    std::int32_t a1 = -d3 + d5 - d7 - (d7 >> 1);
    std::int32_t a3 = d1 + d7 - d3 - (d3 >> 1);
    std::int32_t a5 = -d1 + d7 + d5 + (d5 >> 1);
    std::int32_t a7 = d3 + d5 + d1 + (d1 >> 1);
    std::int32_t b1 = a1 + (a7 >> 2);
    std::int32_t b7 = a7 - (a1 >> 2);
    std::int32_t b3 = a3 + (a5 >> 2);
    std::int32_t b5 = (a3 >> 2) - a5;
    f[0] = b0 + b7;
    f[fs] = b2 + b5;
    f[2 * fs] = b4 + b3;
    f[3 * fs] = b6 + b1;
    f[4 * fs] = b6 - b1;
    f[5 * fs] = b4 - b3;
    f[6 * fs] = b2 - b5;
    f[7 * fs] = b0 - b7;
}

} // namespace

void idct8x8_add(std::uint8_t* dst, int stride, std::int32_t* b) noexcept {
    std::int32_t t[64];
    std::int32_t r[64];
    for (int i = 0; i < 8; ++i) idct8_1d(b + i * 8, 1, t + i * 8, 1);
    for (int j = 0; j < 8; ++j) idct8_1d(t + j, 8, r + j, 8);
    for (int y = 0; y < 8; ++y) {
        std::uint8_t* row = dst + y * stride;
        for (int x = 0; x < 8; ++x) row[x] = clip_pixel(row[x] + ((r[y * 8 + x] + 32) >> 6));
    }
    std::memset(b, 0, 64 * sizeof(std::int32_t));
}

void idct8x8_dc_add(std::uint8_t* dst, int stride, std::int32_t* b) noexcept {
    int dc = (b[0] + 32) >> 6;
    b[0] = 0;
    for (int y = 0; y < 8; ++y) {
        std::uint8_t* row = dst + y * stride;
        for (int x = 0; x < 8; ++x) row[x] = clip_pixel(row[x] + dc);
    }
}

namespace {

template <int N>
void pred_directional(std::uint8_t* dst, int stride, int mode, const int* tp, const int* lp, bool have_top, bool have_left) {
    auto T = [tp](int x) { return tp[x + 1]; };
    auto L = [lp](int y) { return lp[y + 1]; };
    if (mode == 2) {
        int sum = 0;
        int dc;
        if (have_top && have_left) {
            for (int i = 0; i < N; ++i) sum += T(i) + L(i);
            dc = (sum + N) >> (N == 4 ? 3 : 4);
        } else if (have_left) {
            for (int i = 0; i < N; ++i) sum += L(i);
            dc = (sum + N / 2) >> (N == 4 ? 2 : 3);
        } else if (have_top) {
            for (int i = 0; i < N; ++i) sum += T(i);
            dc = (sum + N / 2) >> (N == 4 ? 2 : 3);
        } else {
            dc = 128;
        }
        for (int y = 0; y < N; ++y) std::memset(dst + y * stride, dc, N);
        return;
    }
    for (int y = 0; y < N; ++y) {
        std::uint8_t* row = dst + y * stride;
        for (int x = 0; x < N; ++x) {
            int v;
            switch (mode) {
            case 0:
                v = T(x);
                break;
            case 1:
                v = L(y);
                break;
            case 3:
                if (x == N - 1 && y == N - 1) {
                    v = (T(2 * N - 2) + 3 * T(2 * N - 1) + 2) >> 2;
                } else {
                    v = (T(x + y) + 2 * T(x + y + 1) + T(x + y + 2) + 2) >> 2;
                }
                break;
            case 4:
                if (x > y) {
                    v = (T(x - y - 2) + 2 * T(x - y - 1) + T(x - y) + 2) >> 2;
                } else if (x < y) {
                    v = (L(y - x - 2) + 2 * L(y - x - 1) + L(y - x) + 2) >> 2;
                } else {
                    v = (T(0) + 2 * T(-1) + L(0) + 2) >> 2;
                }
                break;
            case 5: {
                int z = 2 * x - y;
                if (z >= 0 && !(z & 1)) {
                    v = (T(x - (y >> 1) - 1) + T(x - (y >> 1)) + 1) >> 1;
                } else if (z >= 0) {
                    v = (T(x - (y >> 1) - 2) + 2 * T(x - (y >> 1) - 1) + T(x - (y >> 1)) + 2) >> 2;
                } else if (z == -1) {
                    v = (L(0) + 2 * L(-1) + T(0) + 2) >> 2;
                } else {
                    v = (L(y - 2 * x - 1) + 2 * L(y - 2 * x - 2) + L(y - 2 * x - 3) + 2) >> 2;
                }
                break;
            }
            case 6: {
                int z = 2 * y - x;
                if (z >= 0 && !(z & 1)) {
                    v = (L(y - (x >> 1) - 1) + L(y - (x >> 1)) + 1) >> 1;
                } else if (z >= 0) {
                    v = (L(y - (x >> 1) - 2) + 2 * L(y - (x >> 1) - 1) + L(y - (x >> 1)) + 2) >> 2;
                } else if (z == -1) {
                    v = (L(0) + 2 * L(-1) + T(0) + 2) >> 2;
                } else {
                    v = (T(x - 2 * y - 1) + 2 * T(x - 2 * y - 2) + T(x - 2 * y - 3) + 2) >> 2;
                }
                break;
            }
            case 7:
                if (!(y & 1)) {
                    v = (T(x + (y >> 1)) + T(x + (y >> 1) + 1) + 1) >> 1;
                } else {
                    v = (T(x + (y >> 1)) + 2 * T(x + (y >> 1) + 1) + T(x + (y >> 1) + 2) + 2) >> 2;
                }
                break;
            default: {
                int z = x + 2 * y;
                if (z < 2 * N - 3 && !(z & 1)) {
                    v = (L(y + (x >> 1)) + L(y + (x >> 1) + 1) + 1) >> 1;
                } else if (z < 2 * N - 3) {
                    v = (L(y + (x >> 1)) + 2 * L(y + (x >> 1) + 1) + L(y + (x >> 1) + 2) + 2) >> 2;
                } else if (z == 2 * N - 3) {
                    v = (L(N - 2) + 3 * L(N - 1) + 2) >> 2;
                } else {
                    v = L(N - 1);
                }
                break;
            }
            }
            row[x] = static_cast<std::uint8_t>(v);
        }
    }
}

} // namespace

void pred_intra4x4(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left, bool have_top_right) {
    int t[9];
    int l[5];
    const std::uint8_t* above = dst - stride;
    t[0] = have_top_left ? above[-1] : 128;
    for (int x = 0; x < 4; ++x) t[1 + x] = have_top ? above[x] : 128;
    for (int x = 4; x < 8; ++x) t[1 + x] = (have_top && have_top_right) ? above[x] : t[4];
    l[0] = t[0];
    for (int y = 0; y < 4; ++y) l[1 + y] = have_left ? dst[y * stride - 1] : 128;
    if (mode < 0 || mode > 8) mode = 2;
    pred_directional<4>(dst, stride, mode, t, l, have_top, have_left);
}

void pred_intra8x8(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left, bool have_top_right) {
    int raw_t[16];
    int raw_l[8];
    const std::uint8_t* above = dst - stride;
    int tl = have_top_left ? above[-1] : 128;
    for (int x = 0; x < 8; ++x) raw_t[x] = have_top ? above[x] : 128;
    for (int x = 8; x < 16; ++x) raw_t[x] = (have_top && have_top_right) ? above[x] : raw_t[7];
    for (int y = 0; y < 8; ++y) raw_l[y] = have_left ? dst[y * stride - 1] : 128;
    int t[17];
    int l[9];
    if (have_top) {
        t[1] = have_top_left ? (tl + 2 * raw_t[0] + raw_t[1] + 2) >> 2 : (3 * raw_t[0] + raw_t[1] + 2) >> 2;
        for (int x = 1; x < 15; ++x) t[1 + x] = (raw_t[x - 1] + 2 * raw_t[x] + raw_t[x + 1] + 2) >> 2;
        t[16] = (raw_t[14] + 3 * raw_t[15] + 2) >> 2;
    } else {
        for (int x = 0; x < 16; ++x) t[1 + x] = 128;
    }
    int ftl = tl;
    if (have_top_left) {
        if (have_top && have_left) {
            ftl = (raw_t[0] + 2 * tl + raw_l[0] + 2) >> 2;
        } else if (have_top) {
            ftl = (3 * tl + raw_t[0] + 2) >> 2;
        } else if (have_left) {
            ftl = (3 * tl + raw_l[0] + 2) >> 2;
        }
    }
    t[0] = ftl;
    l[0] = ftl;
    if (have_left) {
        l[1] = have_top_left ? (tl + 2 * raw_l[0] + raw_l[1] + 2) >> 2 : (3 * raw_l[0] + raw_l[1] + 2) >> 2;
        for (int y = 1; y < 7; ++y) l[1 + y] = (raw_l[y - 1] + 2 * raw_l[y] + raw_l[y + 1] + 2) >> 2;
        l[8] = (raw_l[6] + 3 * raw_l[7] + 2) >> 2;
    } else {
        for (int y = 0; y < 8; ++y) l[1 + y] = 128;
    }
    if (mode < 0 || mode > 8) mode = 2;
    pred_directional<8>(dst, stride, mode, t, l, have_top, have_left);
}

void pred_intra16x16(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left) {
    const std::uint8_t* above = dst - stride;
    int top[16];
    int left[16];
    for (int i = 0; i < 16; ++i) {
        top[i] = have_top ? above[i] : 128;
        left[i] = have_left ? dst[i * stride - 1] : 128;
    }
    int tl = have_top_left ? above[-1] : 128;
    switch (mode) {
    case 0:
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = static_cast<std::uint8_t>(top[x]);
        break;
    case 1:
        for (int y = 0; y < 16; ++y) std::memset(dst + y * stride, left[y], 16);
        break;
    case 3: {
        int h = 0;
        int v = 0;
        for (int i = 0; i < 8; ++i) {
            int tp = (6 - i) >= 0 ? top[6 - i] : tl;
            int lp = (6 - i) >= 0 ? left[6 - i] : tl;
            h += (i + 1) * (top[8 + i] - tp);
            v += (i + 1) * (left[8 + i] - lp);
        }
        int a = 16 * (left[15] + top[15]);
        int b = (5 * h + 32) >> 6;
        int c = (5 * v + 32) >> 6;
        for (int y = 0; y < 16; ++y) {
            int base = a + c * (y - 7) + 16;
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = clip_pixel((base + b * (x - 7)) >> 5);
        }
        break;
    }
    default: {
        int sum = 0;
        int dc;
        if (have_top && have_left) {
            for (int i = 0; i < 16; ++i) sum += top[i] + left[i];
            dc = (sum + 16) >> 5;
        } else if (have_left) {
            for (int i = 0; i < 16; ++i) sum += left[i];
            dc = (sum + 8) >> 4;
        } else if (have_top) {
            for (int i = 0; i < 16; ++i) sum += top[i];
            dc = (sum + 8) >> 4;
        } else {
            dc = 128;
        }
        for (int y = 0; y < 16; ++y) std::memset(dst + y * stride, dc, 16);
        break;
    }
    }
}

void pred_chroma(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left) {
    const std::uint8_t* above = dst - stride;
    int top[8];
    int left[8];
    for (int i = 0; i < 8; ++i) {
        top[i] = have_top ? above[i] : 128;
        left[i] = have_left ? dst[i * stride - 1] : 128;
    }
    int tl = have_top_left ? above[-1] : 128;
    switch (mode) {
    case 1:
        for (int y = 0; y < 8; ++y) std::memset(dst + y * stride, left[y], 8);
        break;
    case 2:
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) dst[y * stride + x] = static_cast<std::uint8_t>(top[x]);
        break;
    case 3: {
        int h = 0;
        int v = 0;
        for (int i = 0; i < 4; ++i) {
            int tp = (2 - i) >= 0 ? top[2 - i] : tl;
            int lp = (2 - i) >= 0 ? left[2 - i] : tl;
            h += (i + 1) * (top[4 + i] - tp);
            v += (i + 1) * (left[4 + i] - lp);
        }
        int a = 16 * (left[7] + top[7]);
        int b = (34 * h + 32) >> 6;
        int c = (34 * v + 32) >> 6;
        for (int y = 0; y < 8; ++y) {
            int base = a + c * (y - 3) + 16;
            for (int x = 0; x < 8; ++x) dst[y * stride + x] = clip_pixel((base + b * (x - 3)) >> 5);
        }
        break;
    }
    default:
        for (int by = 0; by < 2; ++by) {
            for (int bx = 0; bx < 2; ++bx) {
                int st = 0;
                int sl = 0;
                for (int i = 0; i < 4; ++i) {
                    st += top[bx * 4 + i];
                    sl += left[by * 4 + i];
                }
                int dc;
                if (bx == by) {
                    if (have_top && have_left) {
                        dc = (st + sl + 4) >> 3;
                    } else if (have_left) {
                        dc = (sl + 2) >> 2;
                    } else if (have_top) {
                        dc = (st + 2) >> 2;
                    } else {
                        dc = 128;
                    }
                } else if (bx == 1) {
                    if (have_top) {
                        dc = (st + 2) >> 2;
                    } else if (have_left) {
                        dc = (sl + 2) >> 2;
                    } else {
                        dc = 128;
                    }
                } else {
                    if (have_left) {
                        dc = (sl + 2) >> 2;
                    } else if (have_top) {
                        dc = (st + 2) >> 2;
                    } else {
                        dc = 128;
                    }
                }
                for (int y = 0; y < 4; ++y) std::memset(dst + (by * 4 + y) * stride + bx * 4, dc, 4);
            }
        }
        break;
    }
}

namespace {

inline int tap6(int a, int b, int c, int d, int e, int f) noexcept {
    return a - 5 * b + 20 * c + 20 * d - 5 * e + f;
}

template <int W>
void put_copy(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int h) {
    for (int y = 0; y < h; ++y) std::memcpy(dst + y * ds, src + y * ss, static_cast<std::size_t>(W));
}

template <int W>
void put_h6(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int h) {
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* s = src + y * ss;
        std::uint8_t* d = dst + y * ds;
        std::uint8_t row[W];
        for (int x = 0; x < W; ++x) row[x] = clip_pixel((tap6(s[x - 2], s[x - 1], s[x], s[x + 1], s[x + 2], s[x + 3]) + 16) >> 5);
        std::memcpy(d, row, W);
    }
}

template <int W>
void put_v6(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int h) {
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* s = src + y * ss;
        std::uint8_t* d = dst + y * ds;
        const std::uint8_t* r0 = s - 2 * ss;
        const std::uint8_t* r1 = s - ss;
        const std::uint8_t* r3 = s + ss;
        const std::uint8_t* r4 = s + 2 * ss;
        const std::uint8_t* r5 = s + 3 * ss;
        std::uint8_t row[W];
        for (int x = 0; x < W; ++x) row[x] = clip_pixel((tap6(r0[x], r1[x], s[x], r3[x], r4[x], r5[x]) + 16) >> 5);
        std::memcpy(d, row, W);
    }
}

template <int W>
void put_hv6(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int h) {
    std::int16_t tmp[(W + 5) * 16];
    const int ts = W + 5;
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* s = src + y * ss - 2;
        std::int16_t* t = tmp + y * ts;
        const std::uint8_t* r0 = s - 2 * ss;
        const std::uint8_t* r1 = s - ss;
        const std::uint8_t* r3 = s + ss;
        const std::uint8_t* r4 = s + 2 * ss;
        const std::uint8_t* r5 = s + 3 * ss;
        for (int x = 0; x < W + 5; ++x) t[x] = static_cast<std::int16_t>(tap6(r0[x], r1[x], s[x], r3[x], r4[x], r5[x]));
    }
    for (int y = 0; y < h; ++y) {
        const std::int16_t* t = tmp + y * ts + 2;
        std::uint8_t* d = dst + y * ds;
        std::uint8_t row[W];
        for (int x = 0; x < W; ++x) row[x] = clip_pixel((tap6(t[x - 2], t[x - 1], t[x], t[x + 1], t[x + 2], t[x + 3]) + 512) >> 10);
        std::memcpy(d, row, W);
    }
}

template <int W>
void avg_into(std::uint8_t* dst, int ds, const std::uint8_t* a, int as, const std::uint8_t* b, int bs, int h) {
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* pa = a + y * as;
        const std::uint8_t* pb = b + y * bs;
        std::uint8_t* d = dst + y * ds;
        std::uint8_t row[W];
        for (int x = 0; x < W; ++x) row[x] = static_cast<std::uint8_t>((pa[x] + pb[x] + 1) >> 1);
        std::memcpy(d, row, W);
    }
}

const std::uint8_t* emulate_edge(std::uint8_t* buf, int bw, int bh, const std::uint8_t* plane, int stride, int pw, int ph, int x0, int y0) {
    for (int y = 0; y < bh; ++y) {
        int sy = clip3(0, ph - 1, y0 + y);
        const std::uint8_t* row = plane + sy * stride;
        std::uint8_t* d = buf + y * bw;
        for (int x = 0; x < bw; ++x) d[x] = row[clip3(0, pw - 1, x0 + x)];
    }
    return buf;
}

template <int W>
void mc_luma_w(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int fx, int fy, int h) {
    std::uint8_t t1[16 * 16];
    std::uint8_t t2[16 * 16];
    switch (fy * 4 + fx) {
    case 0:
        put_copy<W>(dst, ds, src, ss, h);
        break;
    case 1:
        put_h6<W>(t1, 16, src, ss, h);
        avg_into<W>(dst, ds, src, ss, t1, 16, h);
        break;
    case 2:
        put_h6<W>(dst, ds, src, ss, h);
        break;
    case 3:
        put_h6<W>(t1, 16, src, ss, h);
        avg_into<W>(dst, ds, src + 1, ss, t1, 16, h);
        break;
    case 4:
        put_v6<W>(t1, 16, src, ss, h);
        avg_into<W>(dst, ds, src, ss, t1, 16, h);
        break;
    case 8:
        put_v6<W>(dst, ds, src, ss, h);
        break;
    case 12:
        put_v6<W>(t1, 16, src, ss, h);
        avg_into<W>(dst, ds, src + ss, ss, t1, 16, h);
        break;
    case 5:
        put_h6<W>(t1, 16, src, ss, h);
        put_v6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 7:
        put_h6<W>(t1, 16, src, ss, h);
        put_v6<W>(t2, 16, src + 1, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 13:
        put_h6<W>(t1, 16, src + ss, ss, h);
        put_v6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 15:
        put_h6<W>(t1, 16, src + ss, ss, h);
        put_v6<W>(t2, 16, src + 1, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 6:
        put_h6<W>(t1, 16, src, ss, h);
        put_hv6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 14:
        put_h6<W>(t1, 16, src + ss, ss, h);
        put_hv6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 9:
        put_v6<W>(t1, 16, src, ss, h);
        put_hv6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    case 11:
        put_v6<W>(t1, 16, src + 1, ss, h);
        put_hv6<W>(t2, 16, src, ss, h);
        avg_into<W>(dst, ds, t1, 16, t2, 16, h);
        break;
    default:
        put_hv6<W>(dst, ds, src, ss, h);
        break;
    }
}

template <int W>
void mc_chroma_w(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int fx, int fy, int h) {
    if (fx == 0 && fy == 0) {
        put_copy<W>(dst, ds, src, ss, h);
        return;
    }
    const int a = (8 - fx) * (8 - fy);
    const int b = fx * (8 - fy);
    const int c = (8 - fx) * fy;
    const int d = fx * fy;
    for (int j = 0; j < h; ++j) {
        const std::uint8_t* s0 = src + j * ss;
        const std::uint8_t* s1 = s0 + ss;
        std::uint8_t* o = dst + j * ds;
        std::uint8_t row[W];
        for (int i = 0; i < W; ++i) row[i] = static_cast<std::uint8_t>((a * s0[i] + b * s0[i + 1] + c * s1[i] + d * s1[i + 1] + 32) >> 6);
        std::memcpy(o, row, W);
    }
}

template <int W>
void weight_uni_w(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int h, int log_wd, int weight, int offset) {
    if (log_wd >= 1) {
        const int round = 1 << (log_wd - 1);
        for (int y = 0; y < h; ++y) {
            const std::uint8_t* s = src + y * ss;
            std::uint8_t* d = dst + y * ds;
            std::uint8_t row[W];
            for (int x = 0; x < W; ++x) row[x] = clip_pixel(((s[x] * weight + round) >> log_wd) + offset);
            std::memcpy(d, row, W);
        }
    } else {
        for (int y = 0; y < h; ++y) {
            const std::uint8_t* s = src + y * ss;
            std::uint8_t* d = dst + y * ds;
            std::uint8_t row[W];
            for (int x = 0; x < W; ++x) row[x] = clip_pixel(s[x] * weight + offset);
            std::memcpy(d, row, W);
        }
    }
}

template <int W>
void weight_bi_w(std::uint8_t* dst, int ds, const std::uint8_t* a, const std::uint8_t* b, int ss, int h, int log_wd, int w0, int w1, int offset) {
    const int round = 1 << log_wd;
    const int shift = log_wd + 1;
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* pa = a + y * ss;
        const std::uint8_t* pb = b + y * ss;
        std::uint8_t* d = dst + y * ds;
        std::uint8_t row[W];
        for (int x = 0; x < W; ++x) row[x] = clip_pixel(((pa[x] * w0 + pb[x] * w1 + round) >> shift) + offset);
        std::memcpy(d, row, W);
    }
}

#if defined(FIZMO_H264_SSE2)

inline __m128i load_px(const std::uint8_t* p, int n) {
    if (n == 8) return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    std::int32_t v;
    std::memcpy(&v, p, 4);
    return _mm_cvtsi32_si128(v);
}

inline void store_px(std::uint8_t* p, __m128i v, int n) {
    if (n == 8) {
        _mm_storel_epi64(reinterpret_cast<__m128i*>(p), v);
    } else {
        std::int32_t x = _mm_cvtsi128_si32(v);
        std::memcpy(p, &x, 4);
    }
}

void bi_sse2(std::uint8_t* dst, int ds, const std::uint8_t* a, const std::uint8_t* b, int ss, int w, int h, int log_wd, int w0, int w1, int offset) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i wpair = _mm_set1_epi32(static_cast<int>((static_cast<std::uint32_t>(w1) << 16) | (static_cast<std::uint32_t>(w0) & 0xFFFFu)));
    const __m128i round = _mm_set1_epi32(1 << log_wd);
    const __m128i shift = _mm_cvtsi32_si128(log_wd + 1);
    const __m128i off = _mm_set1_epi16(static_cast<short>(offset));
    const int step = w >= 8 ? 8 : 4;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += step) {
            __m128i va = _mm_unpacklo_epi8(load_px(a + y * ss + x, step), zero);
            __m128i vb = _mm_unpacklo_epi8(load_px(b + y * ss + x, step), zero);
            __m128i lo = _mm_madd_epi16(_mm_unpacklo_epi16(va, vb), wpair);
            __m128i hi = _mm_madd_epi16(_mm_unpackhi_epi16(va, vb), wpair);
            lo = _mm_sra_epi32(_mm_add_epi32(lo, round), shift);
            hi = _mm_sra_epi32(_mm_add_epi32(hi, round), shift);
            __m128i r = _mm_adds_epi16(_mm_packs_epi32(lo, hi), off);
            store_px(dst + y * ds + x, _mm_packus_epi16(r, r), step);
        }
    }
}

void uni_sse2(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int w, int h, int log_wd, int weight, int offset) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i wv = _mm_set1_epi16(static_cast<short>(weight));
    const __m128i round = _mm_set1_epi16(static_cast<short>(log_wd >= 1 ? 1 << (log_wd - 1) : 0));
    const __m128i shift = _mm_cvtsi32_si128(log_wd);
    const __m128i off = _mm_set1_epi16(static_cast<short>(offset));
    const int step = w >= 8 ? 8 : 4;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += step) {
            __m128i v = _mm_unpacklo_epi8(load_px(src + y * ss + x, step), zero);
            v = _mm_sra_epi16(_mm_add_epi16(_mm_mullo_epi16(v, wv), round), shift);
            v = _mm_adds_epi16(v, off);
            store_px(dst + y * ds + x, _mm_packus_epi16(v, v), step);
        }
    }
}

void avg_sse2(std::uint8_t* dst, int ds, const std::uint8_t* a, const std::uint8_t* b, int ss, int w, int h) {
    for (int y = 0; y < h; ++y) {
        if (w == 16) {
            __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + y * ss));
            __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + y * ss));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + y * ds), _mm_avg_epu8(va, vb));
        } else {
            int step = w >= 8 ? 8 : 4;
            __m128i va = load_px(a + y * ss, step);
            __m128i vb = load_px(b + y * ss, step);
            store_px(dst + y * ds, _mm_avg_epu8(va, vb), step);
        }
    }
}

#endif // FIZMO_H264_SSE2

} // namespace

void mc_luma(std::uint8_t* dst, int ds, const Picture& ref, int x, int y, int mvx, int mvy, int w, int h) {
    int xi = x + (mvx >> 2);
    int yi = y + (mvy >> 2);
    int fx = mvx & 3;
    int fy = mvy & 3;
    const std::uint8_t* src;
    int ss;
    std::uint8_t edge[22 * 22];
    if (xi - 2 < -k_pad_luma || yi - 2 < -k_pad_luma || xi + w + 3 > ref.width + k_pad_luma || yi + h + 3 > ref.height + k_pad_luma) {
        emulate_edge(edge, 22, h + 6, ref.plane[0], ref.stride[0], ref.width, ref.height, xi - 2, yi - 2);
        src = edge + 2 * 22 + 2;
        ss = 22;
    } else {
        src = ref.plane[0] + yi * ref.stride[0] + xi;
        ss = ref.stride[0];
    }
    if (w == 16) {
        mc_luma_w<16>(dst, ds, src, ss, fx, fy, h);
    } else if (w == 8) {
        mc_luma_w<8>(dst, ds, src, ss, fx, fy, h);
    } else {
        mc_luma_w<4>(dst, ds, src, ss, fx, fy, h);
    }
}

void mc_chroma(std::uint8_t* dst, int ds, const Picture& ref, int plane, int x, int y, int mvx, int mvy, int w, int h) {
    int xi = x + (mvx >> 3);
    int yi = y + (mvy >> 3);
    int fx = mvx & 7;
    int fy = mvy & 7;
    const std::uint8_t* src;
    int ss;
    std::uint8_t edge[9 * 9];
    if (xi < -k_pad_chroma || yi < -k_pad_chroma || xi + w + 1 > ref.chroma_width + k_pad_chroma || yi + h + 1 > ref.chroma_height + k_pad_chroma) {
        emulate_edge(edge, 9, h + 1, ref.plane[plane], ref.stride[plane], ref.chroma_width, ref.chroma_height, xi, yi);
        src = edge;
        ss = 9;
    } else {
        src = ref.plane[plane] + yi * ref.stride[plane] + xi;
        ss = ref.stride[plane];
    }
    if (w == 8) {
        mc_chroma_w<8>(dst, ds, src, ss, fx, fy, h);
    } else if (w == 4) {
        mc_chroma_w<4>(dst, ds, src, ss, fx, fy, h);
    } else {
        mc_chroma_w<2>(dst, ds, src, ss, fx, fy, h);
    }
}

void mc_luma_block8(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int fx, int fy, int w, int h) {
    if (w == 16) {
        mc_luma_w<16>(dst, ds, src, ss, fx, fy, h);
    } else if (w == 8) {
        mc_luma_w<8>(dst, ds, src, ss, fx, fy, h);
    } else {
        mc_luma_w<4>(dst, ds, src, ss, fx, fy, h);
    }
}

void mc_chroma_block8(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int fx, int fy, int w, int h) {
    if (w == 8) {
        mc_chroma_w<8>(dst, ds, src, ss, fx, fy, h);
    } else if (w == 4) {
        mc_chroma_w<4>(dst, ds, src, ss, fx, fy, h);
    } else {
        mc_chroma_w<2>(dst, ds, src, ss, fx, fy, h);
    }
}

void mc_average(std::uint8_t* dst, int ds, const std::uint8_t* a, const std::uint8_t* b, int ss, int w, int h) {
#if defined(FIZMO_H264_SSE2)
    if (w >= 4) {
        avg_sse2(dst, ds, a, b, ss, w, h);
        return;
    }
#endif // FIZMO_H264_SSE2
    if (w == 16) {
        avg_into<16>(dst, ds, a, ss, b, ss, h);
    } else if (w == 8) {
        avg_into<8>(dst, ds, a, ss, b, ss, h);
    } else if (w == 4) {
        avg_into<4>(dst, ds, a, ss, b, ss, h);
    } else {
        avg_into<2>(dst, ds, a, ss, b, ss, h);
    }
}

void mc_weight_uni(std::uint8_t* dst, int ds, const std::uint8_t* src, int ss, int w, int h, int log_wd, int weight, int offset) {
#if defined(FIZMO_H264_SSE2)
    if (w >= 4) {
        uni_sse2(dst, ds, src, ss, w, h, log_wd, weight, offset);
        return;
    }
#endif // FIZMO_H264_SSE2
    if (w == 16) {
        weight_uni_w<16>(dst, ds, src, ss, h, log_wd, weight, offset);
    } else if (w == 8) {
        weight_uni_w<8>(dst, ds, src, ss, h, log_wd, weight, offset);
    } else if (w == 4) {
        weight_uni_w<4>(dst, ds, src, ss, h, log_wd, weight, offset);
    } else {
        weight_uni_w<2>(dst, ds, src, ss, h, log_wd, weight, offset);
    }
}

void mc_weight_bi(std::uint8_t* dst, int ds, const std::uint8_t* a, const std::uint8_t* b, int ss, int w, int h, int log_wd, int w0, int w1, int offset) {
    if (log_wd == 5 && w0 == 32 && w1 == 32 && offset == 0) {
        mc_average(dst, ds, a, b, ss, w, h);
        return;
    }
#if defined(FIZMO_H264_SSE2)
    if (w >= 4) {
        bi_sse2(dst, ds, a, b, ss, w, h, log_wd, w0, w1, offset);
        return;
    }
#endif // FIZMO_H264_SSE2
    if (w == 16) {
        weight_bi_w<16>(dst, ds, a, b, ss, h, log_wd, w0, w1, offset);
    } else if (w == 8) {
        weight_bi_w<8>(dst, ds, a, b, ss, h, log_wd, w0, w1, offset);
    } else if (w == 4) {
        weight_bi_w<4>(dst, ds, a, b, ss, h, log_wd, w0, w1, offset);
    } else {
        weight_bi_w<2>(dst, ds, a, b, ss, h, log_wd, w0, w1, offset);
    }
}

namespace {

#if defined(FIZMO_H264_SSE2)

inline __m128i abs_diff16(__m128i a, __m128i b) {
    return _mm_max_epi16(_mm_sub_epi16(a, b), _mm_sub_epi16(b, a));
}

inline __m128i select16(__m128i mask, __m128i a, __m128i b) {
    return _mm_or_si128(_mm_and_si128(mask, a), _mm_andnot_si128(mask, b));
}

inline __m128i clamp16(__m128i v, __m128i lo, __m128i hi) {
    return _mm_min_epi16(_mm_max_epi16(v, lo), hi);
}

void luma_core(__m128i p2, __m128i& p1, __m128i& p0, __m128i& q0, __m128i& q1, __m128i q2, __m128i alpha, __m128i beta, __m128i tc0, __m128i on) {
    __m128i f = _mm_and_si128(on, _mm_cmplt_epi16(abs_diff16(p0, q0), alpha));
    f = _mm_and_si128(f, _mm_cmplt_epi16(abs_diff16(p1, p0), beta));
    f = _mm_and_si128(f, _mm_cmplt_epi16(abs_diff16(q1, q0), beta));
    __m128i ap = _mm_cmplt_epi16(abs_diff16(p2, p0), beta);
    __m128i aq = _mm_cmplt_epi16(abs_diff16(q2, q0), beta);
    __m128i tc = _mm_sub_epi16(_mm_sub_epi16(tc0, ap), aq);
    __m128i ntc = _mm_sub_epi16(_mm_setzero_si128(), tc);
    __m128i delta = _mm_add_epi16(_mm_slli_epi16(_mm_sub_epi16(q0, p0), 2), _mm_sub_epi16(p1, q1));
    delta = _mm_srai_epi16(_mm_add_epi16(delta, _mm_set1_epi16(4)), 3);
    delta = clamp16(delta, ntc, tc);
    __m128i avg = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(p0, q0), _mm_set1_epi16(1)), 1);
    __m128i ntc0 = _mm_sub_epi16(_mm_setzero_si128(), tc0);
    __m128i dp1 = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(p2, avg), _mm_slli_epi16(p1, 1)), 1);
    __m128i dq1 = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(q2, avg), _mm_slli_epi16(q1, 1)), 1);
    dp1 = clamp16(dp1, ntc0, tc0);
    dq1 = clamp16(dq1, ntc0, tc0);
    __m128i np1 = select16(_mm_and_si128(f, ap), _mm_add_epi16(p1, dp1), p1);
    __m128i nq1 = select16(_mm_and_si128(f, aq), _mm_add_epi16(q1, dq1), q1);
    p0 = select16(f, _mm_add_epi16(p0, delta), p0);
    q0 = select16(f, _mm_sub_epi16(q0, delta), q0);
    p1 = np1;
    q1 = nq1;
}

void luma_normal_regs(__m128i r[6], __m128i out[4], int alpha, int beta, const int* bs, const int* tc0) {
    const __m128i zero = _mm_setzero_si128();
    __m128i a = _mm_set1_epi16(static_cast<short>(alpha));
    __m128i b = _mm_set1_epi16(static_cast<short>(beta));
    __m128i res[4][2];
    for (int half = 0; half < 2; ++half) {
        short t0 = static_cast<short>(tc0[half * 2]);
        short t1 = static_cast<short>(tc0[half * 2 + 1]);
        short o0 = static_cast<short>(bs[half * 2] ? -1 : 0);
        short o1 = static_cast<short>(bs[half * 2 + 1] ? -1 : 0);
        __m128i tc = _mm_set_epi16(t1, t1, t1, t1, t0, t0, t0, t0);
        __m128i on = _mm_set_epi16(o1, o1, o1, o1, o0, o0, o0, o0);
        __m128i v[6];
        for (int i = 0; i < 6; ++i) v[i] = half == 0 ? _mm_unpacklo_epi8(r[i], zero) : _mm_unpackhi_epi8(r[i], zero);
        luma_core(v[0], v[1], v[2], v[3], v[4], v[5], a, b, tc, on);
        for (int i = 0; i < 4; ++i) res[i][half] = v[i + 1];
    }
    for (int i = 0; i < 4; ++i) out[i] = _mm_packus_epi16(res[i][0], res[i][1]);
}

void luma_normal_16(std::uint8_t* rows[6], int alpha, int beta, const int* bs, const int* tc0) {
    __m128i r[6];
    for (int i = 0; i < 6; ++i) r[i] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(rows[i]));
    __m128i out[4];
    luma_normal_regs(r, out, alpha, beta, bs, tc0);
    for (int i = 0; i < 4; ++i) _mm_storeu_si128(reinterpret_cast<__m128i*>(rows[i + 1]), out[i]);
}

void luma_normal_cols(std::uint8_t* pix, int stride, int alpha, int beta, const int* bs, const int* tc0) {
    const std::uint8_t* base = pix - 4;
    __m128i a[8];
    for (int i = 0; i < 8; ++i) {
        __m128i r0 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(base + (2 * i) * stride));
        __m128i r1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(base + (2 * i + 1) * stride));
        a[i] = _mm_unpacklo_epi8(r0, r1);
    }
    __m128i b0 = _mm_unpacklo_epi16(a[0], a[1]);
    __m128i b1 = _mm_unpackhi_epi16(a[0], a[1]);
    __m128i b2 = _mm_unpacklo_epi16(a[2], a[3]);
    __m128i b3 = _mm_unpackhi_epi16(a[2], a[3]);
    __m128i b4 = _mm_unpacklo_epi16(a[4], a[5]);
    __m128i b5 = _mm_unpackhi_epi16(a[4], a[5]);
    __m128i b6 = _mm_unpacklo_epi16(a[6], a[7]);
    __m128i b7 = _mm_unpackhi_epi16(a[6], a[7]);
    __m128i c0 = _mm_unpacklo_epi32(b0, b2);
    __m128i c1 = _mm_unpackhi_epi32(b0, b2);
    __m128i c2 = _mm_unpacklo_epi32(b1, b3);
    __m128i c3 = _mm_unpackhi_epi32(b1, b3);
    __m128i c4 = _mm_unpacklo_epi32(b4, b6);
    __m128i c5 = _mm_unpackhi_epi32(b4, b6);
    __m128i c6 = _mm_unpacklo_epi32(b5, b7);
    __m128i c7 = _mm_unpackhi_epi32(b5, b7);
    __m128i r[6];
    r[0] = _mm_unpackhi_epi64(c0, c4);
    r[1] = _mm_unpacklo_epi64(c1, c5);
    r[2] = _mm_unpackhi_epi64(c1, c5);
    r[3] = _mm_unpacklo_epi64(c2, c6);
    r[4] = _mm_unpackhi_epi64(c2, c6);
    r[5] = _mm_unpacklo_epi64(c3, c7);
    __m128i out[4];
    luma_normal_regs(r, out, alpha, beta, bs, tc0);
    __m128i x0 = _mm_unpacklo_epi8(out[0], out[1]);
    __m128i x1 = _mm_unpackhi_epi8(out[0], out[1]);
    __m128i y0 = _mm_unpacklo_epi8(out[2], out[3]);
    __m128i y1 = _mm_unpackhi_epi8(out[2], out[3]);
    __m128i z[4] = {_mm_unpacklo_epi16(x0, y0), _mm_unpackhi_epi16(x0, y0), _mm_unpacklo_epi16(x1, y1), _mm_unpackhi_epi16(x1, y1)};
    std::uint8_t* dst = pix - 2;
    for (int g = 0; g < 4; ++g) {
        __m128i v = z[g];
        for (int k = 0; k < 4; ++k) {
            std::int32_t word = _mm_cvtsi128_si32(v);
            std::memcpy(dst + (g * 4 + k) * stride, &word, 4);
            v = _mm_srli_si128(v, 4);
        }
    }
}

void chroma_normal_8(std::uint8_t* rows[4], int alpha, int beta, const int* bs, const int* tc0) {
    const __m128i zero = _mm_setzero_si128();
    __m128i a = _mm_set1_epi16(static_cast<short>(alpha));
    __m128i b = _mm_set1_epi16(static_cast<short>(beta));
    __m128i v[4];
    for (int i = 0; i < 4; ++i) v[i] = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(rows[i])), zero);
    __m128i tc = _mm_set_epi16(static_cast<short>(tc0[3] + 1), static_cast<short>(tc0[3] + 1), static_cast<short>(tc0[2] + 1), static_cast<short>(tc0[2] + 1),
                               static_cast<short>(tc0[1] + 1), static_cast<short>(tc0[1] + 1), static_cast<short>(tc0[0] + 1), static_cast<short>(tc0[0] + 1));
    __m128i on = _mm_set_epi16(static_cast<short>(bs[3] ? -1 : 0), static_cast<short>(bs[3] ? -1 : 0), static_cast<short>(bs[2] ? -1 : 0), static_cast<short>(bs[2] ? -1 : 0),
                               static_cast<short>(bs[1] ? -1 : 0), static_cast<short>(bs[1] ? -1 : 0), static_cast<short>(bs[0] ? -1 : 0), static_cast<short>(bs[0] ? -1 : 0));
    __m128i p1 = v[0], p0 = v[1], q0 = v[2], q1 = v[3];
    __m128i f = _mm_and_si128(on, _mm_cmplt_epi16(abs_diff16(p0, q0), a));
    f = _mm_and_si128(f, _mm_cmplt_epi16(abs_diff16(p1, p0), b));
    f = _mm_and_si128(f, _mm_cmplt_epi16(abs_diff16(q1, q0), b));
    __m128i ntc = _mm_sub_epi16(zero, tc);
    __m128i delta = _mm_add_epi16(_mm_slli_epi16(_mm_sub_epi16(q0, p0), 2), _mm_sub_epi16(p1, q1));
    delta = _mm_srai_epi16(_mm_add_epi16(delta, _mm_set1_epi16(4)), 3);
    delta = clamp16(delta, ntc, tc);
    p0 = select16(f, _mm_add_epi16(p0, delta), p0);
    q0 = select16(f, _mm_sub_epi16(q0, delta), q0);
    __m128i packed = _mm_packus_epi16(p0, q0);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(rows[1]), packed);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(rows[2]), _mm_srli_si128(packed, 8));
}

#endif // FIZMO_H264_SSE2

void filter_luma_rows(std::uint8_t* pix, int stride, int alpha, int beta, const int* bs, const int* tc0) {
#if defined(FIZMO_H264_SSE2)
    std::uint8_t* rows[6] = {pix - 3 * stride, pix - 2 * stride, pix - stride, pix, pix + stride, pix + 2 * stride};
    luma_normal_16(rows, alpha, beta, bs, tc0);
#else
    std::uint8_t* rp2 = pix - 3 * stride;
    std::uint8_t* rp1 = pix - 2 * stride;
    std::uint8_t* rp0 = pix - stride;
    std::uint8_t* rq0 = pix;
    std::uint8_t* rq1 = pix + stride;
    std::uint8_t* rq2 = pix + 2 * stride;
    for (int i = 0; i < 16; ++i) {
        int c0 = tc0[i >> 2];
        int p2 = rp2[i];
        int p1 = rp1[i];
        int p0 = rp0[i];
        int q0 = rq0[i];
        int q1 = rq1[i];
        int q2 = rq2[i];
        if (!bs[i >> 2] || std::abs(p0 - q0) >= alpha || std::abs(p1 - p0) >= beta || std::abs(q1 - q0) >= beta) continue;
        bool ap = std::abs(p2 - p0) < beta;
        bool aq = std::abs(q2 - q0) < beta;
        int tc = c0 + (ap ? 1 : 0) + (aq ? 1 : 0);
        int delta = clip3(-tc, tc, ((q0 - p0) * 4 + (p1 - q1) + 4) >> 3);
        rp0[i] = clip_pixel(p0 + delta);
        rq0[i] = clip_pixel(q0 - delta);
        if (ap) rp1[i] = static_cast<std::uint8_t>(p1 + clip3(-c0, c0, (p2 + ((p0 + q0 + 1) >> 1) - p1 * 2) >> 1));
        if (aq) rq1[i] = static_cast<std::uint8_t>(q1 + clip3(-c0, c0, (q2 + ((p0 + q0 + 1) >> 1) - q1 * 2) >> 1));
    }
#endif // FIZMO_H264_SSE2
}

void filter_luma_cols(std::uint8_t* pix, int stride, int alpha, int beta, const int* bs, const int* tc0) {
#if defined(FIZMO_H264_SSE2)
    luma_normal_cols(pix, stride, alpha, beta, bs, tc0);
#else
    std::uint8_t cols[6][16];
    for (int y = 0; y < 16; ++y) {
        const std::uint8_t* row = pix + y * stride - 3;
        for (int k = 0; k < 6; ++k) cols[k][y] = row[k];
    }
    std::uint8_t* rows[6] = {cols[0], cols[1], cols[2], cols[3], cols[4], cols[5]};
    filter_luma_rows(rows[3], 16, alpha, beta, bs, tc0);
    for (int y = 0; y < 16; ++y) {
        std::uint8_t* row = pix + y * stride - 2;
        for (int k = 0; k < 4; ++k) row[k] = cols[k + 1][y];
    }
#endif // FIZMO_H264_SSE2
}

} // namespace

void filter_luma_edge(std::uint8_t* pix, int xstep, int ystep, int alpha, int beta, const int* bs, const int* tc0) {
    if (bs[0] < 4 && bs[1] < 4 && bs[2] < 4 && bs[3] < 4) {
        if (ystep == 1) {
            filter_luma_rows(pix, xstep, alpha, beta, bs, tc0);
        } else {
            filter_luma_cols(pix, ystep, alpha, beta, bs, tc0);
        }
        return;
    }
    for (int seg = 0; seg < 4; ++seg) {
        int strength = bs[seg];
        if (strength == 0) {
            pix += 4 * ystep;
            continue;
        }
        for (int k = 0; k < 4; ++k, pix += ystep) {
            int p0 = pix[-xstep];
            int q0 = pix[0];
            int p1 = pix[-2 * xstep];
            int q1 = pix[xstep];
            if (std::abs(p0 - q0) >= alpha || std::abs(p1 - p0) >= beta || std::abs(q1 - q0) >= beta) continue;
            int p2 = pix[-3 * xstep];
            int q2 = pix[2 * xstep];
            int ap = std::abs(p2 - p0);
            int aq = std::abs(q2 - q0);
            if (strength < 4) {
                int c0 = tc0[seg];
                int tc = c0 + (ap < beta ? 1 : 0) + (aq < beta ? 1 : 0);
                int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
                pix[-xstep] = clip_pixel(p0 + delta);
                pix[0] = clip_pixel(q0 - delta);
                if (ap < beta) pix[-2 * xstep] = static_cast<std::uint8_t>(p1 + clip3(-c0, c0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 * 2)) >> 1));
                if (aq < beta) pix[xstep] = static_cast<std::uint8_t>(q1 + clip3(-c0, c0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 * 2)) >> 1));
            } else {
                int p3 = pix[-4 * xstep];
                int q3 = pix[3 * xstep];
                bool strong = std::abs(p0 - q0) < ((alpha >> 2) + 2);
                if (ap < beta && strong) {
                    pix[-xstep] = static_cast<std::uint8_t>((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
                    pix[-2 * xstep] = static_cast<std::uint8_t>((p2 + p1 + p0 + q0 + 2) >> 2);
                    pix[-3 * xstep] = static_cast<std::uint8_t>((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
                } else {
                    pix[-xstep] = static_cast<std::uint8_t>((2 * p1 + p0 + q1 + 2) >> 2);
                }
                if (aq < beta && strong) {
                    pix[0] = static_cast<std::uint8_t>((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3);
                    pix[xstep] = static_cast<std::uint8_t>((p0 + q0 + q1 + q2 + 2) >> 2);
                    pix[2 * xstep] = static_cast<std::uint8_t>((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
                } else {
                    pix[0] = static_cast<std::uint8_t>((2 * q1 + q0 + p1 + 2) >> 2);
                }
            }
        }
    }
}

void filter_chroma_edge(std::uint8_t* pix, int xstep, int ystep, int alpha, int beta, const int* bs, const int* tc0) {
#if defined(FIZMO_H264_SSE2)
    if (bs[0] < 4 && bs[1] < 4 && bs[2] < 4 && bs[3] < 4) {
        if (ystep == 1) {
            std::uint8_t* rows[4] = {pix - 2 * xstep, pix - xstep, pix, pix + xstep};
            chroma_normal_8(rows, alpha, beta, bs, tc0);
        } else {
            std::uint8_t cols[4][8];
            for (int y = 0; y < 8; ++y) {
                const std::uint8_t* row = pix + y * ystep - 2;
                for (int k = 0; k < 4; ++k) cols[k][y] = row[k];
            }
            std::uint8_t* rows[4] = {cols[0], cols[1], cols[2], cols[3]};
            chroma_normal_8(rows, alpha, beta, bs, tc0);
            for (int y = 0; y < 8; ++y) {
                std::uint8_t* row = pix + y * ystep - 1;
                row[0] = cols[1][y];
                row[1] = cols[2][y];
            }
        }
        return;
    }
#endif // FIZMO_H264_SSE2
    for (int seg = 0; seg < 4; ++seg) {
        int strength = bs[seg];
        if (strength == 0) {
            pix += 2 * ystep;
            continue;
        }
        for (int k = 0; k < 2; ++k, pix += ystep) {
            int p0 = pix[-xstep];
            int q0 = pix[0];
            int p1 = pix[-2 * xstep];
            int q1 = pix[xstep];
            if (std::abs(p0 - q0) >= alpha || std::abs(p1 - p0) >= beta || std::abs(q1 - q0) >= beta) continue;
            if (strength < 4) {
                int tc = tc0[seg] + 1;
                int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
                pix[-xstep] = clip_pixel(p0 + delta);
                pix[0] = clip_pixel(q0 - delta);
            } else {
                pix[-xstep] = static_cast<std::uint8_t>((2 * p1 + p0 + q1 + 2) >> 2);
                pix[0] = static_cast<std::uint8_t>((2 * q1 + q0 + p1 + 2) >> 2);
            }
        }
    }
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
