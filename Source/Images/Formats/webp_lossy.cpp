#include "fizmo_library.hpp"
#include "webp_internal.hpp"
#include "webp_tables.hpp"

#include <cmath>
#include <functional>

namespace fizmo {
namespace images {
namespace webp_detail {

namespace {

enum { B_DC = 0, B_TM, B_VE, B_HE, B_RD, B_VR, B_LD, B_VL, B_HD, B_HU };
enum { DC_PRED = 0, TM_PRED = 1, V_PRED = 2, H_PRED = 3, DC_NOTOP = 4, DC_NOLEFT = 5, DC_NOTOPLEFT = 6 };

const int8_t kYModesIntra4[18] = { -B_DC, 1, -B_TM, 2, -B_VE, 3, 4, 6, -B_HE, 5, -B_RD, -B_VR, -B_LD, 7, -B_VL, 8, -B_HD, -B_HU };

const std::uint8_t kDcTable[128] = {
    4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17, 18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157
};

const std::uint16_t kAcTable[128] = {
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
    78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
    155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284
};

const std::uint8_t kCat3[] = { 173, 148, 140, 0 };
const std::uint8_t kCat4[] = { 176, 155, 140, 135, 0 };
const std::uint8_t kCat5[] = { 180, 157, 141, 134, 130, 0 };
const std::uint8_t kCat6[] = { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0 };
const std::uint8_t* const kCat3456[] = { kCat3, kCat4, kCat5, kCat6 };
const std::uint8_t kZigzag[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };
const std::uint8_t kBands[17] = { 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 0 };

constexpr int BPS = 32;

inline int clip_int(int v, int lo, int hi) noexcept { return v < lo ? lo : v > hi ? hi : v; }
inline std::uint8_t clip8(int v) noexcept { return static_cast<std::uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); }
inline int mul1(int a) noexcept { return ((a * 20091) >> 16) + a; }
inline int mul2(int a) noexcept { return (a * 35468) >> 16; }

class BoolDecoder {
public:
    void init(const std::uint8_t* p, std::size_t n) {
        m_p = p;
        m_end = p + n;
        m_value = (static_cast<std::uint32_t>(next()) << 8) | next();
        m_range = 255;
        m_count = 0;
    }

    int get(int prob) {
        const std::uint32_t split = 1 + (((m_range - 1) * static_cast<std::uint32_t>(prob)) >> 8);
        const std::uint32_t big = split << 8;
        int bit;

        if (m_value >= big) {
            bit = 1;
            m_range -= split;
            m_value -= big;
        } else {
            bit = 0;
            m_range = split;
        }

        while (m_range < 128) {
            m_value <<= 1;
            m_range <<= 1;
            if (++m_count == 8) {
                m_count = 0;
                m_value |= next();
            }
        }

        return bit;
    }

    std::uint32_t value(int bits) {
        std::uint32_t v = 0;
        while (bits-- > 0) v |= static_cast<std::uint32_t>(get(0x80)) << bits;
        return v;
    }

    int signed_value(int bits) {
        const int v = static_cast<int>(value(bits));
        return get(0x80) ? -v : v;
    }

    int flag() { return get(0x80); }

    bool overrun() const noexcept { return m_phantom > 16; }

private:
    std::uint8_t next() {
        if (m_p < m_end) return *m_p++;
        ++m_phantom;
        return 0;
    }

    const std::uint8_t* m_p = nullptr;
    const std::uint8_t* m_end = nullptr;
    std::uint32_t       m_value = 0;
    std::uint32_t       m_range = 255;
    int                 m_count = 0;
    unsigned            m_phantom = 0;
};

struct QuantMatrix {
    int y1[2], y2[2], uv[2];
};

struct FilterInfo {
    int limit = 0;
    int ilevel = 0;
    int hev = 0;
    bool inner = false;
};

using Probas = std::uint8_t[3][11];

void transform_one(const std::int16_t* in, std::uint8_t* dst) {
    int C[16];
    int* tmp = C;

    for (int i = 0; i < 4; ++i) {
        const int a = in[0] + in[8];
        const int b = in[0] - in[8];
        const int c = mul2(in[4]) - mul1(in[12]);
        const int d = mul1(in[4]) + mul2(in[12]);
        tmp[0] = a + d;
        tmp[1] = b + c;
        tmp[2] = b - c;
        tmp[3] = a - d;
        tmp += 4;
        ++in;
    }

    tmp = C;
    for (int i = 0; i < 4; ++i) {
        const int dc = tmp[0] + 4;
        const int a = dc + tmp[8];
        const int b = dc - tmp[8];
        const int c = mul2(tmp[4]) - mul1(tmp[12]);
        const int d = mul1(tmp[4]) + mul2(tmp[12]);
        dst[0] = clip8(dst[0] + ((a + d) >> 3));
        dst[1] = clip8(dst[1] + ((b + c) >> 3));
        dst[2] = clip8(dst[2] + ((b - c) >> 3));
        dst[3] = clip8(dst[3] + ((a - d) >> 3));
        ++tmp;
        dst += BPS;
    }
}

void transform_wht(const std::int16_t* in, std::int16_t* out) {
    int tmp[16];

    for (int i = 0; i < 4; ++i) {
        const int a0 = in[0 + i] + in[12 + i];
        const int a1 = in[4 + i] + in[8 + i];
        const int a2 = in[4 + i] - in[8 + i];
        const int a3 = in[0 + i] - in[12 + i];
        tmp[0 + i] = a0 + a1;
        tmp[8 + i] = a0 - a1;
        tmp[4 + i] = a3 + a2;
        tmp[12 + i] = a3 - a2;
    }

    for (int i = 0; i < 4; ++i) {
        const int dc = tmp[0 + i * 4] + 3;
        const int a0 = dc + tmp[3 + i * 4];
        const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
        const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
        const int a3 = dc - tmp[3 + i * 4];
        out[0] = static_cast<std::int16_t>((a0 + a1) >> 3);
        out[16] = static_cast<std::int16_t>((a3 + a2) >> 3);
        out[32] = static_cast<std::int16_t>((a0 - a1) >> 3);
        out[48] = static_cast<std::int16_t>((a3 - a2) >> 3);
        out += 64;
    }
}

bool any_nonzero(const std::int16_t* c) {
    for (int i = 0; i < 16; ++i) if (c[i]) return true;
    return false;
}

inline std::uint8_t avg3(int a, int b, int c) { return static_cast<std::uint8_t>((a + 2 * b + c + 2) >> 2); }
inline std::uint8_t avg2(int a, int b) { return static_cast<std::uint8_t>((a + b + 1) >> 1); }

void true_motion(std::uint8_t* dst, int size) {
    const std::uint8_t* top = dst - BPS;
    const int tl = top[-1];
    for (int y = 0; y < size; ++y) {
        const int l = dst[-1];
        for (int x = 0; x < size; ++x) dst[x] = clip8(top[x] + l - tl);
        dst += BPS;
    }
}

void fill_block(std::uint8_t* dst, int size, int v) {
    for (int j = 0; j < size; ++j) std::memset(dst + j * BPS, v, static_cast<std::size_t>(size));
}

void pred_luma16(int mode, std::uint8_t* dst) {
    switch (mode) {
        case DC_PRED: {
            int dc = 16;
            for (int j = 0; j < 16; ++j) dc += dst[-1 + j * BPS] + dst[j - BPS];
            fill_block(dst, 16, dc >> 5);
            break;
        }
        case TM_PRED: true_motion(dst, 16); break;
        case V_PRED: for (int j = 0; j < 16; ++j) std::memcpy(dst + j * BPS, dst - BPS, 16); break;
        case H_PRED: for (int j = 0; j < 16; ++j) std::memset(dst + j * BPS, dst[j * BPS - 1], 16); break;
        case DC_NOTOP: {
            int dc = 8;
            for (int j = 0; j < 16; ++j) dc += dst[-1 + j * BPS];
            fill_block(dst, 16, dc >> 4);
            break;
        }
        case DC_NOLEFT: {
            int dc = 8;
            for (int i = 0; i < 16; ++i) dc += dst[i - BPS];
            fill_block(dst, 16, dc >> 4);
            break;
        }
        default: fill_block(dst, 16, 0x80); break;
    }
}

void pred_chroma8(int mode, std::uint8_t* dst) {
    switch (mode) {
        case DC_PRED: {
            int dc = 8;
            for (int i = 0; i < 8; ++i) dc += dst[i - BPS] + dst[-1 + i * BPS];
            fill_block(dst, 8, dc >> 4);
            break;
        }
        case TM_PRED: true_motion(dst, 8); break;
        case V_PRED: for (int j = 0; j < 8; ++j) std::memcpy(dst + j * BPS, dst - BPS, 8); break;
        case H_PRED: for (int j = 0; j < 8; ++j) std::memset(dst + j * BPS, dst[j * BPS - 1], 8); break;
        case DC_NOTOP: {
            int dc = 4;
            for (int i = 0; i < 8; ++i) dc += dst[-1 + i * BPS];
            fill_block(dst, 8, dc >> 3);
            break;
        }
        case DC_NOLEFT: {
            int dc = 4;
            for (int i = 0; i < 8; ++i) dc += dst[i - BPS];
            fill_block(dst, 8, dc >> 3);
            break;
        }
        default: fill_block(dst, 8, 0x80); break;
    }
}

#define DST(x, y) dst[(x) + (y) * BPS]

void pred_luma4(int mode, std::uint8_t* dst) {
    const std::uint8_t* top = dst - BPS;

    switch (mode) {
        case B_DC: {
            unsigned dc = 4;
            for (int i = 0; i < 4; ++i) dc += dst[i - BPS] + dst[-1 + i * BPS];
            fill_block(dst, 4, static_cast<int>(dc >> 3));
            break;
        }
        case B_TM: true_motion(dst, 4); break;
        case B_VE: {
            const std::uint8_t vals[4] = { avg3(top[-1], top[0], top[1]), avg3(top[0], top[1], top[2]), avg3(top[1], top[2], top[3]), avg3(top[2], top[3], top[4]) };
            for (int i = 0; i < 4; ++i) std::memcpy(dst + i * BPS, vals, 4);
            break;
        }
        case B_HE: {
            const int A = dst[-1 - BPS], B = dst[-1], C = dst[-1 + BPS], D = dst[-1 + 2 * BPS], E = dst[-1 + 3 * BPS];
            std::memset(dst + 0 * BPS, avg3(A, B, C), 4);
            std::memset(dst + 1 * BPS, avg3(B, C, D), 4);
            std::memset(dst + 2 * BPS, avg3(C, D, E), 4);
            std::memset(dst + 3 * BPS, avg3(D, E, E), 4);
            break;
        }
        case B_RD: {
            const int I = dst[-1], J = dst[-1 + BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS], X = dst[-1 - BPS];
            const int A = top[0], B = top[1], C = top[2], D = top[3];
            DST(0, 3) = avg3(J, K, L);
            DST(1, 3) = DST(0, 2) = avg3(I, J, K);
            DST(2, 3) = DST(1, 2) = DST(0, 1) = avg3(X, I, J);
            DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = avg3(A, X, I);
            DST(3, 2) = DST(2, 1) = DST(1, 0) = avg3(B, A, X);
            DST(3, 1) = DST(2, 0) = avg3(C, B, A);
            DST(3, 0) = avg3(D, C, B);
            break;
        }
        case B_LD: {
            const int A = top[0], B = top[1], C = top[2], D = top[3], E = top[4], F = top[5], G = top[6], H = top[7];
            DST(0, 0) = avg3(A, B, C);
            DST(1, 0) = DST(0, 1) = avg3(B, C, D);
            DST(2, 0) = DST(1, 1) = DST(0, 2) = avg3(C, D, E);
            DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = avg3(D, E, F);
            DST(3, 1) = DST(2, 2) = DST(1, 3) = avg3(E, F, G);
            DST(3, 2) = DST(2, 3) = avg3(F, G, H);
            DST(3, 3) = avg3(G, H, H);
            break;
        }
        case B_VR: {
            const int I = dst[-1], J = dst[-1 + BPS], K = dst[-1 + 2 * BPS], X = dst[-1 - BPS];
            const int A = top[0], B = top[1], C = top[2], D = top[3];
            DST(0, 0) = DST(1, 2) = avg2(X, A);
            DST(1, 0) = DST(2, 2) = avg2(A, B);
            DST(2, 0) = DST(3, 2) = avg2(B, C);
            DST(3, 0) = avg2(C, D);
            DST(0, 3) = avg3(K, J, I);
            DST(0, 2) = avg3(J, I, X);
            DST(0, 1) = DST(1, 3) = avg3(I, X, A);
            DST(1, 1) = DST(2, 3) = avg3(X, A, B);
            DST(2, 1) = DST(3, 3) = avg3(A, B, C);
            DST(3, 1) = avg3(B, C, D);
            break;
        }
        case B_VL: {
            const int A = top[0], B = top[1], C = top[2], D = top[3], E = top[4], F = top[5], G = top[6], H = top[7];
            DST(0, 0) = avg2(A, B);
            DST(1, 0) = DST(0, 2) = avg2(B, C);
            DST(2, 0) = DST(1, 2) = avg2(C, D);
            DST(3, 0) = DST(2, 2) = avg2(D, E);
            DST(0, 1) = avg3(A, B, C);
            DST(1, 1) = DST(0, 3) = avg3(B, C, D);
            DST(2, 1) = DST(1, 3) = avg3(C, D, E);
            DST(3, 1) = DST(2, 3) = avg3(D, E, F);
            DST(3, 2) = avg3(E, F, G);
            DST(3, 3) = avg3(F, G, H);
            break;
        }
        case B_HU: {
            const int I = dst[-1], J = dst[-1 + BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS];
            DST(0, 0) = avg2(I, J);
            DST(2, 0) = DST(0, 1) = avg2(J, K);
            DST(2, 1) = DST(0, 2) = avg2(K, L);
            DST(1, 0) = avg3(I, J, K);
            DST(3, 0) = DST(1, 1) = avg3(J, K, L);
            DST(3, 1) = DST(1, 2) = avg3(K, L, L);
            DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = static_cast<std::uint8_t>(L);
            break;
        }
        default: {
            const int I = dst[-1], J = dst[-1 + BPS], K = dst[-1 + 2 * BPS], L = dst[-1 + 3 * BPS], X = dst[-1 - BPS];
            const int A = top[0], B = top[1], C = top[2];
            DST(0, 0) = DST(2, 1) = avg2(I, X);
            DST(0, 1) = DST(2, 2) = avg2(J, I);
            DST(0, 2) = DST(2, 3) = avg2(K, J);
            DST(0, 3) = avg2(L, K);
            DST(3, 0) = avg3(A, B, C);
            DST(2, 0) = avg3(X, A, B);
            DST(1, 0) = DST(3, 1) = avg3(I, X, A);
            DST(1, 1) = DST(3, 2) = avg3(J, I, X);
            DST(1, 2) = DST(3, 3) = avg3(K, J, I);
            DST(1, 3) = avg3(L, K, J);
            break;
        }
    }
}

#undef DST

inline int sclip1(int v) { return clip_int(v, -128, 127); }
inline int sclip2(int v) { return clip_int(v, -16, 15); }

void do_filter2(std::uint8_t* p, int step) {
    const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    const int a = 3 * (q0 - p0) + sclip1(p1 - q1);
    const int a1 = sclip2((a + 4) >> 3);
    const int a2 = sclip2((a + 3) >> 3);
    p[-step] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
}

void do_filter4(std::uint8_t* p, int step) {
    const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    const int a = 3 * (q0 - p0);
    const int a1 = sclip2((a + 4) >> 3);
    const int a2 = sclip2((a + 3) >> 3);
    const int a3 = (a1 + 1) >> 1;
    p[-2 * step] = clip8(p1 + a3);
    p[-step] = clip8(p0 + a2);
    p[0] = clip8(q0 - a1);
    p[step] = clip8(q1 - a3);
}

void do_filter6(std::uint8_t* p, int step) {
    const int p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
    const int q0 = p[0], q1 = p[step], q2 = p[2 * step];
    const int a = sclip1(3 * (q0 - p0) + sclip1(p1 - q1));
    const int a1 = (27 * a + 63) >> 7;
    const int a2 = (18 * a + 63) >> 7;
    const int a3 = (9 * a + 63) >> 7;
    p[-3 * step] = clip8(p2 + a3);
    p[-2 * step] = clip8(p1 + a2);
    p[-step] = clip8(p0 + a1);
    p[0] = clip8(q0 - a1);
    p[step] = clip8(q1 - a2);
    p[2 * step] = clip8(q2 - a3);
}

bool hev(const std::uint8_t* p, int step, int thresh) {
    const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    return std::abs(p1 - p0) > thresh || std::abs(q1 - q0) > thresh;
}

bool needs_filter(const std::uint8_t* p, int step, int t) {
    const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
    return 4 * std::abs(p0 - q0) + std::abs(p1 - q1) <= t;
}

bool needs_filter2(const std::uint8_t* p, int step, int t, int it) {
    const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
    const int q0 = p[0], q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
    if (4 * std::abs(p0 - q0) + std::abs(p1 - q1) > t) return false;
    return std::abs(p3 - p2) <= it && std::abs(p2 - p1) <= it && std::abs(p1 - p0) <= it && std::abs(q3 - q2) <= it && std::abs(q2 - q1) <= it && std::abs(q1 - q0) <= it;
}

void simple_filter(std::uint8_t* p, int hstride, int vstride, int size, int thresh) {
    const int t2 = 2 * thresh + 1;
    for (int i = 0; i < size; ++i, p += vstride)
        if (needs_filter(p, hstride, t2)) do_filter2(p, hstride);
}

void filter_loop26(std::uint8_t* p, int hstride, int vstride, int size, int thresh, int ithresh, int hev_t) {
    const int t2 = 2 * thresh + 1;
    while (size-- > 0) {
        if (needs_filter2(p, hstride, t2, ithresh)) {
            if (hev(p, hstride, hev_t)) do_filter2(p, hstride);
            else do_filter6(p, hstride);
        }
        p += vstride;
    }
}

void filter_loop24(std::uint8_t* p, int hstride, int vstride, int size, int thresh, int ithresh, int hev_t) {
    const int t2 = 2 * thresh + 1;
    while (size-- > 0) {
        if (needs_filter2(p, hstride, t2, ithresh)) {
            if (hev(p, hstride, hev_t)) do_filter2(p, hstride);
            else do_filter4(p, hstride);
        }
        p += vstride;
    }
}

int check_mode(int mb_x, int mb_y, int mode) {
    if (mode == B_DC) {
        if (mb_x == 0) return mb_y == 0 ? DC_NOTOPLEFT : DC_NOLEFT;
        return mb_y == 0 ? DC_NOTOP : DC_PRED;
    }
    return mode;
}

struct MbInfo {
    std::uint8_t nz = 0;
    std::uint8_t nz_dc = 0;
};

struct MbData {
    std::int16_t coeffs[384];
    std::uint8_t imodes[16];
    std::uint8_t uvmode;
    bool         is_i4x4;
    std::uint8_t segment;
    bool         skip;
};

class Vp8Decoder {
public:
    bool decode(const std::uint8_t* data, std::size_t size, fmt::Rgba& out, std::string* error) {
        m_error = error;
        if (size < 10) return fail("truncated VP8 header");
        const std::uint32_t bits = data[0] | (data[1] << 8) | (static_cast<std::uint32_t>(data[2]) << 16);
        if (bits & 1) return fail("VP8 frame is not a key frame");
        if (((bits >> 1) & 7) > 3) return fail("bad VP8 profile");
        if (!((bits >> 4) & 1)) return fail("VP8 frame is not displayable");
        const std::uint32_t part_len = bits >> 5;
        if (data[3] != 0x9D || data[4] != 0x01 || data[5] != 0x2A) return fail("bad VP8 start code");
        m_width = static_cast<unsigned>((data[7] << 8 | data[6]) & 0x3FFF);
        m_height = static_cast<unsigned>((data[9] << 8 | data[8]) & 0x3FFF);
        if (!m_width || !m_height) return fail("bad VP8 dimensions");
        m_mbw = (m_width + 15) >> 4;
        m_mbh = (m_height + 15) >> 4;
        const std::uint8_t* buf = data + 10;
        std::size_t left = size - 10;
        if (part_len > left) return fail("bad VP8 partition length");
        m_br.init(buf, part_len);
        buf += part_len;
        left -= part_len;

        m_br.flag();
        m_br.flag();
        parse_segment_header();
        parse_filter_header();
        if (!parse_partitions(buf, left)) return fail("bad VP8 partitions");
        parse_quant();
        m_br.flag();
        parse_proba();
        if (m_br.overrun()) return fail("truncated VP8 header");

        const unsigned yw = m_mbw * 16, yh = m_mbh * 16;
        m_y.assign(static_cast<std::size_t>(yw) * yh, 0);
        m_u.assign(static_cast<std::size_t>(yw / 2) * (yh / 2), 0);
        m_v.assign(m_u.size(), 0);
        m_finfo.assign(static_cast<std::size_t>(m_mbw) * m_mbh, FilterInfo());
        precompute_filter_strengths();

        std::vector<MbInfo> top_info(m_mbw);
        std::vector<std::uint8_t> intra_t(m_mbw * 4, B_DC);
        std::vector<std::uint8_t> top_y(static_cast<std::size_t>(m_mbw) * 16), top_u(static_cast<std::size_t>(m_mbw) * 8), top_v(static_cast<std::size_t>(m_mbw) * 8);
        std::vector<MbData> row(m_mbw);
        std::vector<std::uint8_t> work(static_cast<std::size_t>(BPS) * 17 * 3 + 64, 0);
        std::uint8_t* const y_dst = work.data() + BPS * 1 + 8;
        std::uint8_t* const u_dst = work.data() + BPS * 18 + 8;
        std::uint8_t* const v_dst = work.data() + BPS * 28 + 8;

        for (unsigned mb_y = 0; mb_y < m_mbh; ++mb_y) {
            BoolDecoder& tbr = m_parts[mb_y & (m_num_parts - 1)];
            MbInfo left_info;
            std::uint8_t intra_l[4] = { B_DC, B_DC, B_DC, B_DC };

            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                MbData& block = row[mb_x];
                parse_intra_mode(block, &intra_t[mb_x * 4], intra_l);
                MbInfo& mb = top_info[mb_x];
                bool skip = m_use_skip_proba ? block.skip : false;

                if (!skip) {
                    skip = parse_residuals(block, mb, left_info, tbr);
                } else {
                    left_info.nz = mb.nz = 0;
                    if (!block.is_i4x4) left_info.nz_dc = mb.nz_dc = 0;
                    std::memset(block.coeffs, 0, sizeof(block.coeffs));
                }

                if (m_filter_type > 0) {
                    FilterInfo fi = m_fstrengths[block.segment][block.is_i4x4 ? 1 : 0];
                    fi.inner = fi.inner || !skip;
                    m_finfo[static_cast<std::size_t>(mb_y) * m_mbw + mb_x] = fi;
                }

                if (tbr.overrun() || m_br.overrun()) return fail("truncated VP8 data");
            }

            for (int j = 0; j < 16; ++j) y_dst[j * BPS - 1] = 129;
            for (int j = 0; j < 8; ++j) {
                u_dst[j * BPS - 1] = 129;
                v_dst[j * BPS - 1] = 129;
            }

            if (mb_y > 0) {
                y_dst[-1 - BPS] = u_dst[-1 - BPS] = v_dst[-1 - BPS] = 129;
            } else {
                std::memset(y_dst - BPS - 1, 127, 16 + 4 + 1);
                std::memset(u_dst - BPS - 1, 127, 8 + 1);
                std::memset(v_dst - BPS - 1, 127, 8 + 1);
            }

            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                const MbData& block = row[mb_x];

                if (mb_x > 0) {
                    for (int j = -1; j < 16; ++j) std::memcpy(&y_dst[j * BPS - 4], &y_dst[j * BPS + 12], 4);
                    for (int j = -1; j < 8; ++j) {
                        std::memcpy(&u_dst[j * BPS - 4], &u_dst[j * BPS + 4], 4);
                        std::memcpy(&v_dst[j * BPS - 4], &v_dst[j * BPS + 4], 4);
                    }
                }

                std::uint8_t* ty = &top_y[mb_x * 16];
                if (mb_y > 0) {
                    std::memcpy(y_dst - BPS, ty, 16);
                    std::memcpy(u_dst - BPS, &top_u[mb_x * 8], 8);
                    std::memcpy(v_dst - BPS, &top_v[mb_x * 8], 8);
                }

                if (block.is_i4x4) {
                    std::uint8_t* top_right = y_dst - BPS + 16;
                    if (mb_y > 0) {
                        if (mb_x >= m_mbw - 1) std::memset(top_right, ty[15], 4);
                        else std::memcpy(top_right, &top_y[(mb_x + 1) * 16], 4);
                    }
                    for (int r = 1; r <= 3; ++r) std::memcpy(top_right + r * 4 * BPS, top_right, 4);

                    for (int n = 0; n < 16; ++n) {
                        std::uint8_t* dst = y_dst + (n & 3) * 4 + (n >> 2) * 4 * BPS;
                        pred_luma4(block.imodes[n], dst);
                        if (any_nonzero(block.coeffs + n * 16)) transform_one(block.coeffs + n * 16, dst);
                    }
                } else {
                    pred_luma16(check_mode(static_cast<int>(mb_x), static_cast<int>(mb_y), block.imodes[0]), y_dst);
                    for (int n = 0; n < 16; ++n)
                        if (any_nonzero(block.coeffs + n * 16)) transform_one(block.coeffs + n * 16, y_dst + (n & 3) * 4 + (n >> 2) * 4 * BPS);
                }

                const int uvm = check_mode(static_cast<int>(mb_x), static_cast<int>(mb_y), block.uvmode);
                pred_chroma8(uvm, u_dst);
                pred_chroma8(uvm, v_dst);

                for (int n = 0; n < 4; ++n) {
                    const int off = (n & 1) * 4 + (n >> 1) * 4 * BPS;
                    if (any_nonzero(block.coeffs + 256 + n * 16)) transform_one(block.coeffs + 256 + n * 16, u_dst + off);
                    if (any_nonzero(block.coeffs + 320 + n * 16)) transform_one(block.coeffs + 320 + n * 16, v_dst + off);
                }

                if (mb_y < m_mbh - 1) {
                    std::memcpy(ty, y_dst + 15 * BPS, 16);
                    std::memcpy(&top_u[mb_x * 8], u_dst + 7 * BPS, 8);
                    std::memcpy(&top_v[mb_x * 8], v_dst + 7 * BPS, 8);
                }

                for (int j = 0; j < 16; ++j) std::memcpy(&m_y[(static_cast<std::size_t>(mb_y) * 16 + j) * yw + mb_x * 16], y_dst + j * BPS, 16);
                for (int j = 0; j < 8; ++j) {
                    std::memcpy(&m_u[(static_cast<std::size_t>(mb_y) * 8 + j) * (yw / 2) + mb_x * 8], u_dst + j * BPS, 8);
                    std::memcpy(&m_v[(static_cast<std::size_t>(mb_y) * 8 + j) * (yw / 2) + mb_x * 8], v_dst + j * BPS, 8);
                }
            }
        }

        if (m_filter_type > 0) loop_filter();
        to_rgba(out);
        return true;
    }

private:
    bool fail(const char* why) {
        if (m_error) *m_error = why;
        return false;
    }

    void parse_segment_header() {
        m_use_segment = m_br.flag();
        m_update_map = false;
        m_absolute_delta = true;
        std::memset(m_seg_quant, 0, sizeof(m_seg_quant));
        std::memset(m_seg_filter, 0, sizeof(m_seg_filter));
        m_seg_probs[0] = m_seg_probs[1] = m_seg_probs[2] = 255;

        if (m_use_segment) {
            m_update_map = m_br.flag();
            if (m_br.flag()) {
                m_absolute_delta = m_br.flag();
                for (int s = 0; s < 4; ++s) m_seg_quant[s] = m_br.flag() ? m_br.signed_value(7) : 0;
                for (int s = 0; s < 4; ++s) m_seg_filter[s] = m_br.flag() ? m_br.signed_value(6) : 0;
            }
            if (m_update_map)
                for (int s = 0; s < 3; ++s) m_seg_probs[s] = m_br.flag() ? static_cast<std::uint8_t>(m_br.value(8)) : 255;
        }
    }

    void parse_filter_header() {
        m_simple = m_br.flag();
        m_level = static_cast<int>(m_br.value(6));
        m_sharpness = static_cast<int>(m_br.value(3));
        m_use_lf_delta = m_br.flag();
        std::memset(m_ref_lf_delta, 0, sizeof(m_ref_lf_delta));
        std::memset(m_mode_lf_delta, 0, sizeof(m_mode_lf_delta));

        if (m_use_lf_delta && m_br.flag()) {
            for (int i = 0; i < 4; ++i) if (m_br.flag()) m_ref_lf_delta[i] = m_br.signed_value(6);
            for (int i = 0; i < 4; ++i) if (m_br.flag()) m_mode_lf_delta[i] = m_br.signed_value(6);
        }

        m_filter_type = m_level == 0 ? 0 : m_simple ? 1 : 2;
    }

    bool parse_partitions(const std::uint8_t* buf, std::size_t size) {
        m_num_parts = 1u << m_br.value(2);
        const std::size_t last = m_num_parts - 1;
        if (size < 3 * last) return false;
        const std::uint8_t* sz = buf;
        const std::uint8_t* start = buf + last * 3;
        std::size_t left = size - last * 3;

        for (std::size_t p = 0; p < last; ++p) {
            std::size_t psize = sz[0] | (sz[1] << 8) | (static_cast<std::size_t>(sz[2]) << 16);
            if (psize > left) psize = left;
            m_parts[p].init(start, psize);
            start += psize;
            left -= psize;
            sz += 3;
        }

        m_parts[last].init(start, left);
        return left > 0 || last > 0;
    }

    void parse_quant() {
        const int base_q0 = static_cast<int>(m_br.value(7));
        const int dqy1_dc = m_br.flag() ? m_br.signed_value(4) : 0;
        const int dqy2_dc = m_br.flag() ? m_br.signed_value(4) : 0;
        const int dqy2_ac = m_br.flag() ? m_br.signed_value(4) : 0;
        const int dquv_dc = m_br.flag() ? m_br.signed_value(4) : 0;
        const int dquv_ac = m_br.flag() ? m_br.signed_value(4) : 0;

        for (int i = 0; i < 4; ++i) {
            int q;
            if (m_use_segment) {
                q = m_seg_quant[i];
                if (!m_absolute_delta) q += base_q0;
            } else {
                if (i > 0) {
                    m_dqm[i] = m_dqm[0];
                    continue;
                }
                q = base_q0;
            }

            QuantMatrix& m = m_dqm[i];
            m.y1[0] = kDcTable[clip_int(q + dqy1_dc, 0, 127)];
            m.y1[1] = kAcTable[clip_int(q, 0, 127)];
            m.y2[0] = kDcTable[clip_int(q + dqy2_dc, 0, 127)] * 2;
            m.y2[1] = (kAcTable[clip_int(q + dqy2_ac, 0, 127)] * 101581) >> 16;
            if (m.y2[1] < 8) m.y2[1] = 8;
            m.uv[0] = kDcTable[clip_int(q + dquv_dc, 0, 117)];
            m.uv[1] = kAcTable[clip_int(q + dquv_ac, 0, 127)];
        }
    }

    void parse_proba() {
        for (int t = 0; t < 4; ++t)
            for (int b = 0; b < 8; ++b)
                for (int c = 0; c < 3; ++c)
                    for (int p = 0; p < 11; ++p)
                        m_proba[t][b][c][p] = m_br.get(kCoeffsUpdateProba[t][b][c][p]) ? static_cast<std::uint8_t>(m_br.value(8)) : kCoeffsProba0[t][b][c][p];

        m_use_skip_proba = m_br.flag();
        if (m_use_skip_proba) m_skip_p = static_cast<int>(m_br.value(8));
    }

    void precompute_filter_strengths() {
        if (m_filter_type == 0) return;

        for (int s = 0; s < 4; ++s) {
            int base_level;
            if (m_use_segment) {
                base_level = m_seg_filter[s];
                if (!m_absolute_delta) base_level += m_level;
            } else {
                base_level = m_level;
            }

            for (int i4 = 0; i4 <= 1; ++i4) {
                FilterInfo& info = m_fstrengths[s][i4];
                int level = base_level;
                if (m_use_lf_delta) {
                    level += m_ref_lf_delta[0];
                    if (i4) level += m_mode_lf_delta[0];
                }
                level = clip_int(level, 0, 63);

                if (level > 0) {
                    int ilevel = level;
                    if (m_sharpness > 0) {
                        ilevel >>= m_sharpness > 4 ? 2 : 1;
                        if (ilevel > 9 - m_sharpness) ilevel = 9 - m_sharpness;
                    }
                    if (ilevel < 1) ilevel = 1;
                    info.ilevel = ilevel;
                    info.limit = 2 * level + ilevel;
                    info.hev = level >= 40 ? 2 : level >= 15 ? 1 : 0;
                } else {
                    info.limit = 0;
                }

                info.inner = i4 != 0;
            }
        }
    }

    void parse_intra_mode(MbData& block, std::uint8_t* top, std::uint8_t* left) {
        if (m_update_map) block.segment = static_cast<std::uint8_t>(!m_br.get(m_seg_probs[0]) ? m_br.get(m_seg_probs[1]) : m_br.get(m_seg_probs[2]) + 2);
        else block.segment = 0;
        block.skip = m_use_skip_proba ? m_br.get(m_skip_p) != 0 : false;
        block.is_i4x4 = !m_br.get(145);

        if (!block.is_i4x4) {
            const int ymode = m_br.get(156) ? (m_br.get(128) ? TM_PRED : H_PRED) : (m_br.get(163) ? V_PRED : DC_PRED);
            block.imodes[0] = static_cast<std::uint8_t>(ymode);
            std::memset(top, ymode, 4);
            std::memset(left, ymode, 4);
        } else {
            std::uint8_t* modes = block.imodes;
            for (int y = 0; y < 4; ++y) {
                int ymode = left[y];
                for (int x = 0; x < 4; ++x) {
                    const std::uint8_t* prob = kBModesProba[top[x]][ymode];
                    int i = kYModesIntra4[m_br.get(prob[0])];
                    while (i > 0) i = kYModesIntra4[2 * i + m_br.get(prob[i])];
                    ymode = -i;
                    top[x] = static_cast<std::uint8_t>(ymode);
                }
                std::memcpy(modes, top, 4);
                modes += 4;
                left[y] = static_cast<std::uint8_t>(ymode);
            }
        }

        block.uvmode = static_cast<std::uint8_t>(!m_br.get(142) ? DC_PRED : !m_br.get(114) ? V_PRED : m_br.get(183) ? TM_PRED : H_PRED);
    }

    static int get_large_value(BoolDecoder& br, const std::uint8_t* p) {
        int v;
        if (!br.get(p[3])) {
            if (!br.get(p[4])) v = 2;
            else v = 3 + br.get(p[5]);
        } else {
            if (!br.get(p[6])) {
                if (!br.get(p[7])) {
                    v = 5 + br.get(159);
                } else {
                    v = 7 + 2 * br.get(165);
                    v += br.get(145);
                }
            } else {
                const int bit1 = br.get(p[8]);
                const int bit0 = br.get(p[9 + bit1]);
                const int cat = 2 * bit1 + bit0;
                v = 0;
                for (const std::uint8_t* tab = kCat3456[cat]; *tab; ++tab) v += v + br.get(*tab);
                v += 3 + (8 << cat);
            }
        }
        return v;
    }

    int get_coeffs(BoolDecoder& br, int type, int ctx, const int* dq, int n, std::int16_t* out) {
        const std::uint8_t* p = m_proba[type][kBands[n]][ctx];

        for (; n < 16; ++n) {
            if (!br.get(p[0])) return n;
            while (!br.get(p[1])) {
                p = m_proba[type][kBands[++n]][0];
                if (n == 16) return 16;
            }
            int v;
            if (!br.get(p[2])) {
                v = 1;
                p = m_proba[type][kBands[n + 1]][1];
            } else {
                v = get_large_value(br, p);
                p = m_proba[type][kBands[n + 1]][2];
            }
            out[kZigzag[n]] = static_cast<std::int16_t>((br.get(0x80) ? -v : v) * dq[n > 0 ? 1 : 0]);
        }

        return 16;
    }

    bool parse_residuals(MbData& block, MbInfo& mb, MbInfo& left_mb, BoolDecoder& br) {
        const QuantMatrix& q = m_dqm[block.segment];
        std::int16_t* dst = block.coeffs;
        std::memset(dst, 0, sizeof(block.coeffs));
        int first;
        int ac_type;
        bool any = false;

        if (!block.is_i4x4) {
            std::int16_t dc[16] = {};
            const int ctx = mb.nz_dc + left_mb.nz_dc;
            const int nz = get_coeffs(br, 1, ctx, q.y2, 0, dc);
            mb.nz_dc = left_mb.nz_dc = nz > 0;
            if (nz > 1) {
                transform_wht(dc, dst);
            } else {
                const int dc0 = (dc[0] + 3) >> 3;
                for (int i = 0; i < 256; i += 16) dst[i] = static_cast<std::int16_t>(dc0);
            }
            if (nz > 0) any = true;
            first = 1;
            ac_type = 0;
        } else {
            first = 0;
            ac_type = 3;
        }

        std::uint32_t tnz = mb.nz & 0x0F;
        std::uint32_t lnz = left_mb.nz & 0x0F;

        for (int y = 0; y < 4; ++y) {
            std::uint32_t l = lnz & 1;
            for (int x = 0; x < 4; ++x) {
                const int ctx = static_cast<int>(l + (tnz & 1));
                const int nz = get_coeffs(br, ac_type, ctx, q.y1, first, dst);
                l = nz > first;
                if (nz > first) any = true;
                tnz = (tnz >> 1) | (l << 7);
                dst += 16;
            }
            tnz >>= 4;
            lnz = (lnz >> 1) | (l << 7);
        }

        std::uint32_t out_t_nz = tnz;
        std::uint32_t out_l_nz = lnz >> 4;

        for (int ch = 0; ch < 4; ch += 2) {
            tnz = static_cast<std::uint32_t>(mb.nz >> (4 + ch));
            lnz = static_cast<std::uint32_t>(left_mb.nz >> (4 + ch));
            for (int y = 0; y < 2; ++y) {
                std::uint32_t l = lnz & 1;
                for (int x = 0; x < 2; ++x) {
                    const int ctx = static_cast<int>(l + (tnz & 1));
                    const int nz = get_coeffs(br, 2, ctx, q.uv, 0, dst);
                    l = nz > 0;
                    if (nz > 0) any = true;
                    tnz = (tnz >> 1) | (l << 3);
                    dst += 16;
                }
                tnz >>= 2;
                lnz = (lnz >> 1) | (l << 5);
            }
            out_t_nz |= (tnz << 4) << ch;
            out_l_nz |= (lnz & 0xF0) << ch;
        }

        mb.nz = static_cast<std::uint8_t>(out_t_nz);
        left_mb.nz = static_cast<std::uint8_t>(out_l_nz);
        bool nonzero = false;
        for (int i = 0; i < 384 && !nonzero; ++i) nonzero = block.coeffs[i] != 0;
        (void)any;
        return !nonzero;
    }

    void loop_filter() {
        const int ys = static_cast<int>(m_mbw * 16), uvs = static_cast<int>(m_mbw * 8);

        for (unsigned mb_y = 0; mb_y < m_mbh; ++mb_y)
            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                const FilterInfo& f = m_finfo[static_cast<std::size_t>(mb_y) * m_mbw + mb_x];
                const int limit = f.limit;
                if (limit == 0) continue;
                std::uint8_t* y = &m_y[static_cast<std::size_t>(mb_y) * 16 * ys + mb_x * 16];

                if (m_filter_type == 1) {
                    if (mb_x > 0) simple_filter(y, 1, ys, 16, limit + 4);
                    if (f.inner) for (int k = 1; k <= 3; ++k) simple_filter(y + 4 * k, 1, ys, 16, limit);
                    if (mb_y > 0) simple_filter(y, ys, 1, 16, limit + 4);
                    if (f.inner) for (int k = 1; k <= 3; ++k) simple_filter(y + 4 * k * ys, ys, 1, 16, limit);
                    continue;
                }

                std::uint8_t* u = &m_u[static_cast<std::size_t>(mb_y) * 8 * uvs + mb_x * 8];
                std::uint8_t* v = &m_v[static_cast<std::size_t>(mb_y) * 8 * uvs + mb_x * 8];
                const int il = f.ilevel, ht = f.hev;

                if (mb_x > 0) {
                    filter_loop26(y, 1, ys, 16, limit + 4, il, ht);
                    filter_loop26(u, 1, uvs, 8, limit + 4, il, ht);
                    filter_loop26(v, 1, uvs, 8, limit + 4, il, ht);
                }
                if (f.inner) {
                    for (int k = 1; k <= 3; ++k) filter_loop24(y + 4 * k, 1, ys, 16, limit, il, ht);
                    filter_loop24(u + 4, 1, uvs, 8, limit, il, ht);
                    filter_loop24(v + 4, 1, uvs, 8, limit, il, ht);
                }
                if (mb_y > 0) {
                    filter_loop26(y, ys, 1, 16, limit + 4, il, ht);
                    filter_loop26(u, uvs, 1, 8, limit + 4, il, ht);
                    filter_loop26(v, uvs, 1, 8, limit + 4, il, ht);
                }
                if (f.inner) {
                    for (int k = 1; k <= 3; ++k) filter_loop24(y + 4 * k * ys, ys, 1, 16, limit, il, ht);
                    filter_loop24(u + 4 * uvs, uvs, 1, 8, limit, il, ht);
                    filter_loop24(v + 4 * uvs, uvs, 1, 8, limit, il, ht);
                }
            }
    }

    void to_rgba(fmt::Rgba& out) const {
        const unsigned W = m_width, H = m_height;
        const unsigned ys = m_mbw * 16, cs = m_mbw * 8;
        const unsigned cw = (W + 1) / 2, ch = (H + 1) / 2;
        out = fmt::Rgba(W, H);

        auto chroma16 = [&](const std::vector<std::uint8_t>& plane, unsigned x, unsigned y) -> int {
            const int cx0 = static_cast<int>(x >> 1), cy0 = static_cast<int>(y >> 1);
            const int cx1 = clip_int((x & 1) ? cx0 + 1 : cx0 - 1, 0, static_cast<int>(cw) - 1);
            const int cy1 = clip_int((y & 1) ? cy0 + 1 : cy0 - 1, 0, static_cast<int>(ch) - 1);
            const int a = plane[static_cast<std::size_t>(cy0) * cs + cx0], b = plane[static_cast<std::size_t>(cy0) * cs + cx1];
            const int c = plane[static_cast<std::size_t>(cy1) * cs + cx0], d = plane[static_cast<std::size_t>(cy1) * cs + cx1];
            return 9 * a + 3 * b + 3 * c + d;
        };

        for (unsigned y = 0; y < H; ++y)
            for (unsigned x = 0; x < W; ++x) {
                const double Y = m_y[static_cast<std::size_t>(y) * ys + x];
                const double U = chroma16(m_u, x, y) / 16.0 - 128.0;
                const double V = chroma16(m_v, x, y) / 16.0 - 128.0;
                const double yy = (Y - 16.0) * (255.0 / 219.0);
                const double kr = 0.299, kb = 0.114, kg = 1.0 - kr - kb, cscale = 255.0 / 224.0;
                const double r = yy + 2.0 * (1.0 - kr) * cscale * V;
                const double g = yy - (2.0 * kb * (1.0 - kb) / kg) * cscale * U - (2.0 * kr * (1.0 - kr) / kg) * cscale * V;
                const double b = yy + 2.0 * (1.0 - kb) * cscale * U;
                std::uint8_t* d = out.at(x, y);
                d[0] = clip8(static_cast<int>(std::lround(r)));
                d[1] = clip8(static_cast<int>(std::lround(g)));
                d[2] = clip8(static_cast<int>(std::lround(b)));
                d[3] = 255;
            }
    }

    std::string* m_error = nullptr;
    unsigned     m_width = 0, m_height = 0, m_mbw = 0, m_mbh = 0;
    BoolDecoder  m_br;
    BoolDecoder  m_parts[8];
    unsigned     m_num_parts = 1;
    bool         m_use_segment = false, m_update_map = false, m_absolute_delta = true;
    int          m_seg_quant[4] = {}, m_seg_filter[4] = {};
    std::uint8_t m_seg_probs[3] = { 255, 255, 255 };
    int          m_simple = 0, m_level = 0, m_sharpness = 0, m_use_lf_delta = 0, m_filter_type = 0;
    int          m_ref_lf_delta[4] = {}, m_mode_lf_delta[4] = {};
    QuantMatrix  m_dqm[4] = {};
    std::uint8_t m_proba[4][8][3][11] = {};
    int          m_use_skip_proba = 0, m_skip_p = 0;
    FilterInfo   m_fstrengths[4][2];
    std::vector<FilterInfo>   m_finfo;
    std::vector<std::uint8_t> m_y, m_u, m_v;
};


class BoolEncoder {
public:
    void put(int prob, int bit) {
        const std::uint32_t split = 1 + (((m_range - 1) * static_cast<std::uint32_t>(prob)) >> 8);
        if (bit) {
            m_bottom += split;
            m_range -= split;
        } else {
            m_range = split;
        }

        while (m_range < 128) {
            m_range <<= 1;
            if (m_bottom & (1u << 31)) add_one();
            m_bottom <<= 1;
            if (!--m_bit_count) {
                m_out.push_back(static_cast<std::uint8_t>(m_bottom >> 24));
                m_bottom &= (1u << 24) - 1;
                m_bit_count = 8;
            }
        }
    }

    void put_value(std::uint32_t v, int bits) {
        while (bits-- > 0) put(0x80, static_cast<int>((v >> bits) & 1u));
    }

    void put_signed(int v, int bits) {
        put_value(static_cast<std::uint32_t>(v < 0 ? -v : v), bits);
        put(0x80, v < 0 ? 1 : 0);
    }

    std::vector<std::uint8_t> finish() {
        int c = m_bit_count;
        std::uint32_t v = m_bottom;
        if (v & (1u << (32 - c))) add_one();
        v <<= c & 7;
        c >>= 3;
        while (--c >= 0) v <<= 8;
        c = 4;
        while (--c >= 0) {
            m_out.push_back(static_cast<std::uint8_t>(v >> 24));
            v <<= 8;
        }
        return std::move(m_out);
    }

private:
    void add_one() {
        std::size_t i = m_out.size();
        while (i > 0 && m_out[i - 1] == 255) {
            m_out[i - 1] = 0;
            --i;
        }
        if (i > 0) ++m_out[i - 1];
    }

    std::vector<std::uint8_t> m_out;
    std::uint32_t             m_range = 255;
    std::uint32_t             m_bottom = 0;
    int                       m_bit_count = 24;
};

struct EncMb {
    bool         is_i4x4 = false;
    bool         skip = false;
    std::uint8_t ymode = DC_PRED;
    std::uint8_t imodes[16] = {};
    std::uint8_t uvmode = DC_PRED;
    std::int16_t y2[16] = {};
    std::int16_t y[16][16] = {};
    std::int16_t uv[8][16] = {};
};

double bit_cost(int prob, int bit) {
    const double p = bit ? (256 - prob) / 256.0 : prob / 256.0;
    return -std::log2(p);
}

void fdct(const std::uint8_t* src, int src_stride, const std::uint8_t* ref, std::int16_t* out) {
    int tmp[16];
    for (int i = 0; i < 4; ++i, src += src_stride, ref += BPS) {
        const int d0 = src[0] - ref[0], d1 = src[1] - ref[1], d2 = src[2] - ref[2], d3 = src[3] - ref[3];
        const int a0 = d0 + d3, a1 = d1 + d2, a2 = d1 - d2, a3 = d0 - d3;
        tmp[0 + i * 4] = (a0 + a1) * 8;
        tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 1812) >> 9;
        tmp[2 + i * 4] = (a0 - a1) * 8;
        tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 + 937) >> 9;
    }
    for (int i = 0; i < 4; ++i) {
        const int a0 = tmp[0 + i] + tmp[12 + i], a1 = tmp[4 + i] + tmp[8 + i];
        const int a2 = tmp[4 + i] - tmp[8 + i], a3 = tmp[0 + i] - tmp[12 + i];
        out[0 + i] = static_cast<std::int16_t>((a0 + a1 + 7) >> 4);
        out[4 + i] = static_cast<std::int16_t>(((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0));
        out[8 + i] = static_cast<std::int16_t>((a0 - a1 + 7) >> 4);
        out[12 + i] = static_cast<std::int16_t>((a3 * 2217 - a2 * 5352 + 51000) >> 16);
    }
}

void fwht(const std::int16_t* in, std::int16_t* out) {
    int tmp[16];
    for (int i = 0; i < 4; ++i, in += 64) {
        const int a0 = in[0 * 16] + in[2 * 16], a1 = in[1 * 16] + in[3 * 16];
        const int a2 = in[1 * 16] - in[3 * 16], a3 = in[0 * 16] - in[2 * 16];
        tmp[0 + i * 4] = a0 + a1;
        tmp[1 + i * 4] = a3 + a2;
        tmp[2 + i * 4] = a3 - a2;
        tmp[3 + i * 4] = a0 - a1;
    }
    for (int i = 0; i < 4; ++i) {
        const int a0 = tmp[0 + i] + tmp[8 + i], a1 = tmp[4 + i] + tmp[12 + i];
        const int a2 = tmp[4 + i] - tmp[12 + i], a3 = tmp[0 + i] - tmp[8 + i];
        out[0 + i] = static_cast<std::int16_t>((a0 + a1) >> 1);
        out[4 + i] = static_cast<std::int16_t>((a3 + a2) >> 1);
        out[8 + i] = static_cast<std::int16_t>((a3 - a2) >> 1);
        out[12 + i] = static_cast<std::int16_t>((a0 - a1) >> 1);
    }
}

int quantize_coef(int c, int step, double rounding) {
    const int a = c < 0 ? -c : c;
    int l = static_cast<int>((a + rounding * step) / step);
    if (l > 2048) l = 2048;
    return c < 0 ? -l : l;
}

double level_bits(const std::int16_t* levels, int first) {
    int last = -1;
    for (int n = 15; n >= first; --n) if (levels[kZigzag[n]]) { last = n; break; }
    if (last < 0) return 1.0;
    double bits = 1.0;
    for (int n = first; n <= last; ++n) {
        const int a = std::abs(levels[kZigzag[n]]);
        if (!a) bits += 1.0;
        else if (a == 1) bits += 3.0;
        else bits += 4.0 + 2.0 * std::log2(static_cast<double>(a));
    }
    return bits;
}

struct TokenCounter {
    std::uint32_t counts[4][8][3][11][2] = {};
    void adaptive(int t, int b, int c, int i, const std::uint8_t*, int bit) { ++counts[t][b][c][i][bit]; }
    void fixed(int, int) {}
};

struct TokenWriter {
    BoolEncoder* enc;
    void adaptive(int, int, int, int i, const std::uint8_t* p, int bit) { enc->put(p[i], bit); }
    void fixed(int prob, int bit) { enc->put(prob, bit); }
};

template <class Sink>
int put_coeffs(Sink& sink, const std::uint8_t (&proba)[4][8][3][11], int type, int ctx, const std::int16_t* levels, int first) {
    int last = -1;
    for (int n = 15; n >= first; --n) if (levels[kZigzag[n]]) { last = n; break; }
    int n = first;
    int band = kBands[n];
    const std::uint8_t* p = proba[type][band][ctx];
    int c = ctx;

    while (n < 16) {
        if (last < n) {
            sink.adaptive(type, band, c, 0, p, 0);
            return n;
        }
        sink.adaptive(type, band, c, 0, p, 1);

        while (levels[kZigzag[n]] == 0) {
            sink.adaptive(type, band, c, 1, p, 0);
            ++n;
            band = kBands[n];
            c = 0;
            p = proba[type][band][0];
        }

        sink.adaptive(type, band, c, 1, p, 1);
        const int v = std::abs(levels[kZigzag[n]]);

        if (v == 1) {
            sink.adaptive(type, band, c, 2, p, 0);
        } else {
            sink.adaptive(type, band, c, 2, p, 1);
            if (v <= 4) {
                sink.adaptive(type, band, c, 3, p, 0);
                if (v == 2) {
                    sink.adaptive(type, band, c, 4, p, 0);
                } else {
                    sink.adaptive(type, band, c, 4, p, 1);
                    sink.adaptive(type, band, c, 5, p, v - 3);
                }
            } else {
                sink.adaptive(type, band, c, 3, p, 1);
                if (v <= 10) {
                    sink.adaptive(type, band, c, 6, p, 0);
                    if (v <= 6) {
                        sink.adaptive(type, band, c, 7, p, 0);
                        sink.fixed(159, v - 5);
                    } else {
                        sink.adaptive(type, band, c, 7, p, 1);
                        sink.fixed(165, (v - 7) >> 1);
                        sink.fixed(145, (v - 7) & 1);
                    }
                } else {
                    sink.adaptive(type, band, c, 6, p, 1);
                    int cat = v < 19 ? 0 : v < 35 ? 1 : v < 67 ? 2 : 3;
                    const int bit1 = cat >> 1, bit0 = cat & 1;
                    sink.adaptive(type, band, c, 8, p, bit1);
                    sink.adaptive(type, band, c, 9 + bit1, p, bit0);
                    const int extra = v - (3 + (8 << cat));
                    const std::uint8_t* tab = kCat3456[cat];
                    int nbits = 0;
                    while (tab[nbits]) ++nbits;
                    for (int k = 0; k < nbits; ++k) sink.fixed(tab[k], (extra >> (nbits - 1 - k)) & 1);
                }
            }
        }

        sink.fixed(0x80, levels[kZigzag[n]] < 0 ? 1 : 0);
        ++n;
        if (n == 16) return 16;
        band = kBands[n];
        c = v == 1 ? 1 : 2;
        p = proba[type][band][c];
    }

    return 16;
}

class Vp8Encoder {
public:
    bool encode(const fmt::Rgba& img, int quality, std::vector<std::uint8_t>& out) {
        m_w = img.width;
        m_h = img.height;
        if (!m_w || !m_h || m_w > 16383 || m_h > 16383) return false;
        m_mbw = (m_w + 15) / 16;
        m_mbh = (m_h + 15) / 16;
        quality = clip_int(quality, 0, 100);
        m_q = clip_int(static_cast<int>(std::lround(127.0 * std::pow(1.0 - quality / 100.0, 1.25))), 0, 127);
        setup_quant();
        init_mode_costs();
        to_yuv(img);
        m_mbs.assign(static_cast<std::size_t>(m_mbw) * m_mbh, EncMb());
        analyse();
        out = write_frame();
        return true;
    }

private:
    void setup_quant() {
        m_y1[0] = kDcTable[m_q];
        m_y1[1] = kAcTable[m_q];
        m_y2[0] = kDcTable[m_q] * 2;
        m_y2[1] = (kAcTable[m_q] * 101581) >> 16;
        if (m_y2[1] < 8) m_y2[1] = 8;
        m_uv[0] = kDcTable[std::min(m_q, 117)];
        m_uv[1] = kAcTable[m_q];
        m_lambda = 0.12 * m_y1[1] * m_y1[1];
        m_filter_level = clip_int(static_cast<int>(std::lround(m_y1[1] * 0.4)), 0, 63);
    }

    void to_yuv(const fmt::Rgba& img) {
        const unsigned yw = m_mbw * 16, yh = m_mbh * 16, cw = m_mbw * 8, ch = m_mbh * 8;
        m_sy.assign(static_cast<std::size_t>(yw) * yh, 0);
        m_su.assign(static_cast<std::size_t>(cw) * ch, 128);
        m_sv.assign(m_su.size(), 128);
        const double kr = 0.299, kb = 0.114, kg = 1.0 - kr - kb;

        for (unsigned y = 0; y < yh; ++y)
            for (unsigned x = 0; x < yw; ++x) {
                const std::uint8_t* p = img.at(std::min(x, m_w - 1), std::min(y, m_h - 1));
                const double l = kr * p[0] + kg * p[1] + kb * p[2];
                m_sy[static_cast<std::size_t>(y) * yw + x] = clip8(static_cast<int>(std::lround(16.0 + l * 219.0 / 255.0)));
            }

        for (unsigned y = 0; y < ch; ++y)
            for (unsigned x = 0; x < cw; ++x) {
                double r = 0, g = 0, b = 0;
                for (unsigned dy = 0; dy < 2; ++dy)
                    for (unsigned dx = 0; dx < 2; ++dx) {
                        const std::uint8_t* p = img.at(std::min(2 * x + dx, m_w - 1), std::min(2 * y + dy, m_h - 1));
                        r += p[0];
                        g += p[1];
                        b += p[2];
                    }
                r /= 4; g /= 4; b /= 4;
                const double l = kr * r + kg * g + kb * b;
                const double u = (b - l) / (2.0 * (1.0 - kb)) * 224.0 / 255.0 + 128.0;
                const double v = (r - l) / (2.0 * (1.0 - kr)) * 224.0 / 255.0 + 128.0;
                m_su[static_cast<std::size_t>(y) * cw + x] = clip8(static_cast<int>(std::lround(u)));
                m_sv[static_cast<std::size_t>(y) * cw + x] = clip8(static_cast<int>(std::lround(v)));
            }
    }

    void init_mode_costs() {
        static const int8_t tree[18] = { -B_DC, 1, -B_TM, 2, -B_VE, 3, 4, 6, -B_HE, 5, -B_RD, -B_VR, -B_LD, 7, -B_VL, 8, -B_HD, -B_HU };
        for (int t = 0; t < 10; ++t)
            for (int l = 0; l < 10; ++l) {
                const std::uint8_t* prob = kBModesProba[t][l];
                std::function<void(int, double)> walk = [&](int node, double acc) {
                    for (int b = 0; b < 2; ++b) {
                        const int next = tree[2 * node + b];
                        const double c = acc + bit_cost(prob[node], b);
                        if (next <= 0) m_mode4_bits[t][l][-next] = c;
                        else walk(next, c);
                    }
                };
                walk(0, 0.0);
            }
    }

    double mode4_cost(int top, int left, int mode) const { return m_mode4_bits[top][left][mode]; }

    static int sse_block(const std::uint8_t* a, int as, const std::uint8_t* b, int bs, int w, int h) {
        int s = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int d = a[y * as + x] - b[y * bs + x];
                s += d * d;
            }
        return s;
    }

    void analyse() {
        const unsigned yw = m_mbw * 16, cw = m_mbw * 8;
        std::vector<std::uint8_t> top_y(static_cast<std::size_t>(m_mbw) * 16), top_u(static_cast<std::size_t>(m_mbw) * 8), top_v(static_cast<std::size_t>(m_mbw) * 8);
        std::vector<std::uint8_t> intra_t(m_mbw * 4, B_DC);
        std::vector<std::uint8_t> work(static_cast<std::size_t>(BPS) * 17 * 3 + 64, 0), trial(work.size());
        std::uint8_t* const y_dst = work.data() + BPS * 1 + 8;
        std::uint8_t* const u_dst = work.data() + BPS * 18 + 8;
        std::uint8_t* const v_dst = work.data() + BPS * 28 + 8;

        for (unsigned mb_y = 0; mb_y < m_mbh; ++mb_y) {
            std::uint8_t intra_l[4] = { B_DC, B_DC, B_DC, B_DC };
            for (int j = 0; j < 16; ++j) y_dst[j * BPS - 1] = 129;
            for (int j = 0; j < 8; ++j) {
                u_dst[j * BPS - 1] = 129;
                v_dst[j * BPS - 1] = 129;
            }
            if (mb_y > 0) {
                y_dst[-1 - BPS] = u_dst[-1 - BPS] = v_dst[-1 - BPS] = 129;
            } else {
                std::memset(y_dst - BPS - 1, 127, 16 + 4 + 1);
                std::memset(u_dst - BPS - 1, 127, 8 + 1);
                std::memset(v_dst - BPS - 1, 127, 8 + 1);
            }

            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                EncMb& mb = m_mbs[static_cast<std::size_t>(mb_y) * m_mbw + mb_x];

                if (mb_x > 0) {
                    for (int j = -1; j < 16; ++j) std::memcpy(&y_dst[j * BPS - 4], &y_dst[j * BPS + 12], 4);
                    for (int j = -1; j < 8; ++j) {
                        std::memcpy(&u_dst[j * BPS - 4], &u_dst[j * BPS + 4], 4);
                        std::memcpy(&v_dst[j * BPS - 4], &v_dst[j * BPS + 4], 4);
                    }
                }

                std::uint8_t* ty = &top_y[mb_x * 16];
                if (mb_y > 0) {
                    std::memcpy(y_dst - BPS, ty, 16);
                    std::memcpy(u_dst - BPS, &top_u[mb_x * 8], 8);
                    std::memcpy(v_dst - BPS, &top_v[mb_x * 8], 8);
                }

                const std::uint8_t* src_y = &m_sy[static_cast<std::size_t>(mb_y) * 16 * yw + mb_x * 16];
                const std::uint8_t* src_u = &m_su[static_cast<std::size_t>(mb_y) * 8 * cw + mb_x * 8];
                const std::uint8_t* src_v = &m_sv[static_cast<std::size_t>(mb_y) * 8 * cw + mb_x * 8];

                double best16 = 0;
                EncMb cand16 = mb;
                std::vector<std::uint8_t> recon16(work.size());
                bool have16 = false;

                for (int mode = 0; mode < 4; ++mode) {
                    trial = work;
                    std::uint8_t* t_y = trial.data() + BPS * 1 + 8;
                    EncMb m = mb;
                    m.is_i4x4 = false;
                    m.ymode = static_cast<std::uint8_t>(mode);
                    pred_luma16(check_mode(static_cast<int>(mb_x), static_cast<int>(mb_y), mode), t_y);
                    double rate = encode_luma16(m, src_y, static_cast<int>(yw), t_y);
                    const int dist = sse_block(src_y, static_cast<int>(yw), t_y, BPS, 16, 16);
                    rate += mode == DC_PRED ? bit_cost(145, 1) + bit_cost(156, 0) + bit_cost(163, 0)
                          : mode == V_PRED ? bit_cost(145, 1) + bit_cost(156, 0) + bit_cost(163, 1)
                          : mode == H_PRED ? bit_cost(145, 1) + bit_cost(156, 1) + bit_cost(128, 0)
                                           : bit_cost(145, 1) + bit_cost(156, 1) + bit_cost(128, 1);
                    const double cost = dist + m_lambda * rate;
                    if (!have16 || cost < best16) {
                        best16 = cost;
                        cand16 = m;
                        recon16 = trial;
                        have16 = true;
                    }
                }

                trial = work;
                std::uint8_t* t_y = trial.data() + BPS * 1 + 8;
                EncMb m4 = mb;
                m4.is_i4x4 = true;
                double cost4 = m_lambda * bit_cost(145, 0);
                std::uint8_t top_ctx[4], left_ctx[4];
                std::memcpy(top_ctx, &intra_t[mb_x * 4], 4);
                std::memcpy(left_ctx, intra_l, 4);
                std::uint8_t* top_right = t_y - BPS + 16;
                if (mb_y > 0) {
                    if (mb_x >= m_mbw - 1) std::memset(top_right, ty[15], 4);
                    else std::memcpy(top_right, &top_y[(mb_x + 1) * 16], 4);
                }
                for (int r = 1; r <= 3; ++r) std::memcpy(top_right + r * 4 * BPS, top_right, 4);

                for (int n = 0; n < 16 && cost4 < best16; ++n) {
                    const int bx = n & 3, by = n >> 2;
                    std::uint8_t* dst = t_y + bx * 4 + by * 4 * BPS;
                    const std::uint8_t* src = src_y + bx * 4 + by * 4 * static_cast<int>(yw);
                    double best_b = 0;
                    int best_mode = 0;
                    std::uint8_t best_pix[16];
                    std::int16_t best_lv[16];

                    for (int mode = 0; mode < 10; ++mode) {
                        pred_luma4(mode, dst);
                        std::int16_t coef[16], lv[16];
                        fdct(src, static_cast<int>(yw), dst, coef);
                        for (int k = 0; k < 16; ++k) lv[k] = static_cast<std::int16_t>(quantize_coef(coef[k], k == 0 ? m_y1[0] : m_y1[1], k == 0 ? 0.5 : 0.36));
                        std::int16_t dq[16];
                        for (int k = 0; k < 16; ++k) dq[k] = static_cast<std::int16_t>(lv[k] * (k == 0 ? m_y1[0] : m_y1[1]));
                        if (any_nonzero(dq)) transform_one(dq, dst);
                        const int dist = sse_block(src, static_cast<int>(yw), dst, BPS, 4, 4);
                        const double rate = level_bits(lv, 0) + mode4_cost(top_ctx[bx], left_ctx[by], mode);
                        const double cost = dist + m_lambda * rate;
                        if (mode == 0 || cost < best_b) {
                            best_b = cost;
                            best_mode = mode;
                            for (int r = 0; r < 4; ++r) std::memcpy(best_pix + r * 4, dst + r * BPS, 4);
                            std::memcpy(best_lv, lv, sizeof(lv));
                        }
                    }

                    for (int r = 0; r < 4; ++r) std::memcpy(dst + r * BPS, best_pix + r * 4, 4);
                    std::memcpy(m4.y[n], best_lv, sizeof(best_lv));
                    m4.imodes[n] = static_cast<std::uint8_t>(best_mode);
                    top_ctx[bx] = static_cast<std::uint8_t>(best_mode);
                    left_ctx[by] = static_cast<std::uint8_t>(best_mode);
                    cost4 += best_b;
                }

                if (cost4 < best16) {
                    mb = m4;
                    work = trial;
                    std::memcpy(&intra_t[mb_x * 4], top_ctx, 4);
                    std::memcpy(intra_l, left_ctx, 4);
                } else {
                    mb = cand16;
                    for (int j = 0; j < 16; ++j) std::memcpy(y_dst + j * BPS, recon16.data() + BPS * (1 + j) + 8, 16);
                    std::memset(&intra_t[mb_x * 4], mb.ymode, 4);
                    std::memset(intra_l, mb.ymode, 4);
                }

                double best_uv = 0;
                EncMb best_mb = mb;
                std::uint8_t best_u[64], best_v[64];

                for (int mode = 0; mode < 4; ++mode) {
                    EncMb m = mb;
                    m.uvmode = static_cast<std::uint8_t>(mode);
                    const int pm = check_mode(static_cast<int>(mb_x), static_cast<int>(mb_y), mode);
                    pred_chroma8(pm, u_dst);
                    pred_chroma8(pm, v_dst);
                    double rate = encode_chroma(m, 0, src_u, static_cast<int>(cw), u_dst) + encode_chroma(m, 4, src_v, static_cast<int>(cw), v_dst);
                    rate += mode == DC_PRED ? bit_cost(142, 0) : mode == V_PRED ? bit_cost(142, 1) + bit_cost(114, 0) : mode == TM_PRED ? bit_cost(142, 1) + bit_cost(114, 1) + bit_cost(183, 1) : bit_cost(142, 1) + bit_cost(114, 1) + bit_cost(183, 0);
                    const int dist = sse_block(src_u, static_cast<int>(cw), u_dst, BPS, 8, 8) + sse_block(src_v, static_cast<int>(cw), v_dst, BPS, 8, 8);
                    const double cost = dist + m_lambda * rate;
                    if (mode == 0 || cost < best_uv) {
                        best_uv = cost;
                        best_mb = m;
                        for (int r = 0; r < 8; ++r) {
                            std::memcpy(best_u + r * 8, u_dst + r * BPS, 8);
                            std::memcpy(best_v + r * 8, v_dst + r * BPS, 8);
                        }
                    }
                }

                mb = best_mb;
                for (int r = 0; r < 8; ++r) {
                    std::memcpy(u_dst + r * BPS, best_u + r * 8, 8);
                    std::memcpy(v_dst + r * BPS, best_v + r * 8, 8);
                }

                bool all_zero = true;
                for (int k = 0; k < 16 && all_zero; ++k) all_zero = mb.y2[k] == 0 || mb.is_i4x4;
                for (int b = 0; b < 16 && all_zero; ++b) for (int k = 0; k < 16 && all_zero; ++k) all_zero = mb.y[b][k] == 0;
                for (int b = 0; b < 8 && all_zero; ++b) for (int k = 0; k < 16 && all_zero; ++k) all_zero = mb.uv[b][k] == 0;
                mb.skip = all_zero;

                if (mb_y < m_mbh - 1) {
                    std::memcpy(ty, y_dst + 15 * BPS, 16);
                    std::memcpy(&top_u[mb_x * 8], u_dst + 7 * BPS, 8);
                    std::memcpy(&top_v[mb_x * 8], v_dst + 7 * BPS, 8);
                }
            }
        }
    }

    double encode_luma16(EncMb& m, const std::uint8_t* src, int stride, std::uint8_t* pred) {
        std::int16_t coef[16][16];
        std::int16_t all[256];
        for (int n = 0; n < 16; ++n) {
            fdct(src + (n & 3) * 4 + (n >> 2) * 4 * stride, stride, pred + (n & 3) * 4 + (n >> 2) * 4 * BPS, coef[n]);
            std::memcpy(all + n * 16, coef[n], sizeof(coef[n]));
        }
        std::int16_t dc[16];
        fwht(all, dc);
        double bits = 0;
        for (int k = 0; k < 16; ++k) m.y2[k] = static_cast<std::int16_t>(quantize_coef(dc[k], k == 0 ? m_y2[0] : m_y2[1], 0.5));
        bits += level_bits(m.y2, 0);
        std::int16_t y2dq[16];
        for (int k = 0; k < 16; ++k) y2dq[k] = static_cast<std::int16_t>(m.y2[k] * (k == 0 ? m_y2[0] : m_y2[1]));
        std::int16_t recon[256];
        std::memset(recon, 0, sizeof(recon));
        transform_wht(y2dq, recon);

        for (int n = 0; n < 16; ++n) {
            m.y[n][0] = 0;
            for (int k = 1; k < 16; ++k) m.y[n][k] = static_cast<std::int16_t>(quantize_coef(coef[n][k], m_y1[1], 0.36));
            bits += level_bits(m.y[n], 1);
            std::int16_t* b = recon + n * 16;
            for (int k = 1; k < 16; ++k) b[k] = static_cast<std::int16_t>(m.y[n][k] * m_y1[1]);
            if (any_nonzero(b)) transform_one(b, pred + (n & 3) * 4 + (n >> 2) * 4 * BPS);
        }

        return bits;
    }

    double encode_chroma(EncMb& m, int base, const std::uint8_t* src, int stride, std::uint8_t* pred) {
        double bits = 0;
        for (int n = 0; n < 4; ++n) {
            std::int16_t coef[16];
            const int so = (n & 1) * 4 + (n >> 1) * 4 * stride, po = (n & 1) * 4 + (n >> 1) * 4 * BPS;
            fdct(src + so, stride, pred + po, coef);
            std::int16_t* lv = m.uv[base + n];
            for (int k = 0; k < 16; ++k) lv[k] = static_cast<std::int16_t>(quantize_coef(coef[k], k == 0 ? m_uv[0] : m_uv[1], k == 0 ? 0.5 : 0.36));
            bits += level_bits(lv, 0);
            std::int16_t dq[16];
            for (int k = 0; k < 16; ++k) dq[k] = static_cast<std::int16_t>(lv[k] * (k == 0 ? m_uv[0] : m_uv[1]));
            if (any_nonzero(dq)) transform_one(dq, pred + po);
        }
        return bits;
    }

    template <class Sink>
    void put_tokens(Sink& sink, const std::uint8_t (&proba)[4][8][3][11]) {
        std::vector<std::uint8_t> top_nz(static_cast<std::size_t>(m_mbw) * 9, 0);

        for (unsigned mb_y = 0; mb_y < m_mbh; ++mb_y) {
            std::uint8_t left_nz[9] = {};
            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                const EncMb& mb = m_mbs[static_cast<std::size_t>(mb_y) * m_mbw + mb_x];
                std::uint8_t* tnz = &top_nz[mb_x * 9];

                if (mb.skip) {
                    for (int k = 0; k < 8; ++k) tnz[k] = left_nz[k] = 0;
                    if (!mb.is_i4x4) tnz[8] = left_nz[8] = 0;
                    continue;
                }

                int first = 0, ytype = 3;
                if (!mb.is_i4x4) {
                    const int nz = put_coeffs(sink, proba, 1, tnz[8] + left_nz[8], mb.y2, 0);
                    tnz[8] = left_nz[8] = nz > 0;
                    first = 1;
                    ytype = 0;
                }

                for (int by = 0; by < 4; ++by)
                    for (int bx = 0; bx < 4; ++bx) {
                        const int nz = put_coeffs(sink, proba, ytype, tnz[bx] + left_nz[by], mb.y[by * 4 + bx], first);
                        tnz[bx] = left_nz[by] = nz > first;
                    }

                for (int ch = 0; ch < 2; ++ch)
                    for (int by = 0; by < 2; ++by)
                        for (int bx = 0; bx < 2; ++bx) {
                            const int nz = put_coeffs(sink, proba, 2, tnz[4 + ch * 2 + bx] + left_nz[4 + ch * 2 + by], mb.uv[ch * 4 + by * 2 + bx], 0);
                            tnz[4 + ch * 2 + bx] = left_nz[4 + ch * 2 + by] = nz > 0;
                        }
            }
        }
    }

    std::vector<std::uint8_t> write_frame() {
        std::uint8_t proba[4][8][3][11];
        std::memcpy(proba, kCoeffsProba0, sizeof(proba));
        TokenCounter counter;
        put_tokens(counter, proba);
        bool update[4][8][3][11] = {};

        for (int t = 0; t < 4; ++t)
            for (int b = 0; b < 8; ++b)
                for (int c = 0; c < 3; ++c)
                    for (int i = 0; i < 11; ++i) {
                        const std::uint32_t n0 = counter.counts[t][b][c][i][0], n1 = counter.counts[t][b][c][i][1];
                        if (n0 + n1 == 0) continue;
                        const int np = clip_int(static_cast<int>(std::lround(256.0 * n0 / (n0 + n1))), 1, 255);
                        const int op = kCoeffsProba0[t][b][c][i];
                        const double old_cost = n0 * bit_cost(op, 0) + n1 * bit_cost(op, 1) + bit_cost(kCoeffsUpdateProba[t][b][c][i], 0);
                        const double new_cost = n0 * bit_cost(np, 0) + n1 * bit_cost(np, 1) + bit_cost(kCoeffsUpdateProba[t][b][c][i], 1) + 8;
                        if (new_cost < old_cost) {
                            update[t][b][c][i] = true;
                            proba[t][b][c][i] = static_cast<std::uint8_t>(np);
                        }
                    }

        std::size_t skipped = 0;
        for (const auto& mb : m_mbs) skipped += mb.skip;
        const int skip_p = clip_int(static_cast<int>(std::lround(256.0 * (m_mbs.size() - skipped) / m_mbs.size())), 1, 255);
        const bool use_skip = skipped > 0;

        BoolEncoder hdr;
        hdr.put_value(0, 1);
        hdr.put_value(0, 1);
        hdr.put_value(0, 1);
        hdr.put_value(0, 1);
        hdr.put_value(static_cast<std::uint32_t>(m_filter_level), 6);
        hdr.put_value(0, 3);
        hdr.put_value(0, 1);
        hdr.put_value(0, 2);
        hdr.put_value(static_cast<std::uint32_t>(m_q), 7);
        for (int k = 0; k < 5; ++k) hdr.put_value(0, 1);
        hdr.put_value(0, 1);

        for (int t = 0; t < 4; ++t)
            for (int b = 0; b < 8; ++b)
                for (int c = 0; c < 3; ++c)
                    for (int i = 0; i < 11; ++i) {
                        hdr.put(kCoeffsUpdateProba[t][b][c][i], update[t][b][c][i] ? 1 : 0);
                        if (update[t][b][c][i]) hdr.put_value(proba[t][b][c][i], 8);
                    }

        hdr.put_value(use_skip ? 1 : 0, 1);
        if (use_skip) hdr.put_value(static_cast<std::uint32_t>(skip_p), 8);
        std::vector<std::uint8_t> intra_t(m_mbw * 4, B_DC);

        for (unsigned mb_y = 0; mb_y < m_mbh; ++mb_y) {
            std::uint8_t intra_l[4] = { B_DC, B_DC, B_DC, B_DC };
            for (unsigned mb_x = 0; mb_x < m_mbw; ++mb_x) {
                const EncMb& mb = m_mbs[static_cast<std::size_t>(mb_y) * m_mbw + mb_x];
                std::uint8_t* top = &intra_t[mb_x * 4];
                if (use_skip) hdr.put(skip_p, mb.skip ? 1 : 0);
                hdr.put(145, mb.is_i4x4 ? 0 : 1);

                if (!mb.is_i4x4) {
                    const int ym = mb.ymode;
                    if (ym == DC_PRED || ym == V_PRED) {
                        hdr.put(156, 0);
                        hdr.put(163, ym == V_PRED ? 1 : 0);
                    } else {
                        hdr.put(156, 1);
                        hdr.put(128, ym == TM_PRED ? 1 : 0);
                    }
                    std::memset(top, ym, 4);
                    std::memset(intra_l, ym, 4);
                } else {
                    for (int y = 0; y < 4; ++y) {
                        int left = intra_l[y];
                        for (int x = 0; x < 4; ++x) {
                            const int mode = mb.imodes[y * 4 + x];
                            write_bmode(hdr, kBModesProba[top[x]][left], mode);
                            top[x] = static_cast<std::uint8_t>(mode);
                            left = mode;
                        }
                        intra_l[y] = static_cast<std::uint8_t>(left);
                    }
                }

                const int uv = mb.uvmode;
                if (uv == DC_PRED) {
                    hdr.put(142, 0);
                } else {
                    hdr.put(142, 1);
                    if (uv == V_PRED) {
                        hdr.put(114, 0);
                    } else {
                        hdr.put(114, 1);
                        hdr.put(183, uv == TM_PRED ? 1 : 0);
                    }
                }
            }
        }

        std::vector<std::uint8_t> part0 = hdr.finish();
        BoolEncoder tok;
        TokenWriter writer{ &tok };
        put_tokens(writer, proba);
        std::vector<std::uint8_t> part1 = tok.finish();

        std::vector<std::uint8_t> out;
        const std::uint32_t tag = 0u | (0u << 1) | (1u << 4) | (static_cast<std::uint32_t>(part0.size()) << 5);
        out.push_back(static_cast<std::uint8_t>(tag));
        out.push_back(static_cast<std::uint8_t>(tag >> 8));
        out.push_back(static_cast<std::uint8_t>(tag >> 16));
        out.push_back(0x9D);
        out.push_back(0x01);
        out.push_back(0x2A);
        out.push_back(static_cast<std::uint8_t>(m_w));
        out.push_back(static_cast<std::uint8_t>(m_w >> 8));
        out.push_back(static_cast<std::uint8_t>(m_h));
        out.push_back(static_cast<std::uint8_t>(m_h >> 8));
        out.insert(out.end(), part0.begin(), part0.end());
        out.insert(out.end(), part1.begin(), part1.end());
        return out;
    }

    static void write_bmode(BoolEncoder& e, const std::uint8_t* prob, int mode) {
        static const int8_t tree[18] = { -B_DC, 1, -B_TM, 2, -B_VE, 3, 4, 6, -B_HE, 5, -B_RD, -B_VR, -B_LD, 7, -B_VL, 8, -B_HD, -B_HU };
        std::vector<int> bits;
        std::function<bool(int)> find = [&](int node) -> bool {
            for (int b = 0; b < 2; ++b) {
                bits.push_back(b);
                const int next = tree[2 * node + b];
                if (next <= 0) {
                    if (-next == mode) return true;
                } else if (find(next)) {
                    return true;
                }
                bits.pop_back();
            }
            return false;
        };
        find(0);
        int node = 0;
        for (int b : bits) {
            e.put(prob[node], b);
            node = tree[2 * node + b];
        }
    }

    unsigned m_w = 0, m_h = 0, m_mbw = 0, m_mbh = 0;
    int      m_q = 0;
    int      m_y1[2] = {}, m_y2[2] = {}, m_uv[2] = {};
    double   m_lambda = 0;
    int      m_filter_level = 0;
    double   m_mode4_bits[10][10][10] = {};
    std::vector<std::uint8_t> m_sy, m_su, m_sv;
    std::vector<EncMb>        m_mbs;
};

} // namespace

bool vp8_decode(const std::uint8_t* data, std::size_t size, fmt::Rgba& out, std::string* error) {
    auto dec = std::make_unique<Vp8Decoder>();
    return dec->decode(data, size, out, error);
}

bool vp8_encode(const fmt::Rgba& img, int quality, std::vector<std::uint8_t>& out) {
    auto enc = std::make_unique<Vp8Encoder>();
    return enc->encode(img, quality, out);
}

} // namespace webp_detail
} // namespace images
} // namespace fizmo
