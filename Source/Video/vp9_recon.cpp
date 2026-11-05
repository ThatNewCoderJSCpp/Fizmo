#include "vp9_internal.hpp"

namespace fizmo {
namespace video {
namespace vp9_detail {

namespace {

inline int avg2(int a, int b) noexcept { return (a + b + 1) >> 1; }
inline int avg3(int a, int b, int c) noexcept { return (a + 2 * b + c + 2) >> 2; }

inline std::uint16_t clip_px(int v, int maxv) noexcept {
    return static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
}

} // namespace

void predict_intra(std::uint16_t* dst, std::ptrdiff_t stride, int mode, int tx_size, bool have_left, bool have_top, bool have_right, int x0, int y0, int frame_w, int frame_h, int bd) noexcept {
    const int bs = 4 << tx_size;
    const int base = 1 << (bd - 1);
    const int maxv = (1 << bd) - 1;
    int above_buf[64 + 1] = {};
    int left[32] = {};
    int* above = above_buf + 1;
    if (have_left) {
        for (int i = 0; i < bs; ++i) {
            const int yy = imin(y0 + i, frame_h - 1) - y0;
            left[i] = dst[yy * stride - 1];
        }
    } else {
        for (int i = 0; i < bs; ++i) left[i] = base + 1;
    }
    if (have_top) {
        const std::uint16_t* a = dst - stride;
        for (int i = 0; i < bs; ++i) above[i] = a[imin(x0 + i, frame_w - 1) - x0];
        if (bs == 4 && have_right) {
            for (int i = bs; i < 2 * bs; ++i) above[i] = a[imin(x0 + i, frame_w - 1) - x0];
        } else {
            for (int i = bs; i < 2 * bs; ++i) above[i] = above[bs - 1];
        }
        above[-1] = have_left ? a[-1] : base + 1;
    } else {
        for (int i = -1; i < 2 * bs; ++i) above[i] = base - 1;
    }
    switch (mode) {
    case kDcPred: {
        int v;
        if (have_top && have_left) {
            int s = 0;
            for (int i = 0; i < bs; ++i) s += above[i] + left[i];
            v = (s + bs) / (2 * bs);
        } else if (have_left) {
            int s = 0;
            for (int i = 0; i < bs; ++i) s += left[i];
            v = (s + (bs >> 1)) / bs;
        } else if (have_top) {
            int s = 0;
            for (int i = 0; i < bs; ++i) s += above[i];
            v = (s + (bs >> 1)) / bs;
        } else {
            v = base;
        }
        for (int r = 0; r < bs; ++r)
            for (int c = 0; c < bs; ++c) dst[r * stride + c] = static_cast<std::uint16_t>(v);
        break;
    }
    case kVPred:
        for (int r = 0; r < bs; ++r)
            for (int c = 0; c < bs; ++c) dst[r * stride + c] = static_cast<std::uint16_t>(above[c]);
        break;
    case kHPred:
        for (int r = 0; r < bs; ++r)
            for (int c = 0; c < bs; ++c) dst[r * stride + c] = static_cast<std::uint16_t>(left[r]);
        break;
    case kTmPred:
        for (int r = 0; r < bs; ++r)
            for (int c = 0; c < bs; ++c) dst[r * stride + c] = clip_px(left[r] + above[c] - above[-1], maxv);
        break;
    case kD45Pred:
        for (int r = 0; r < bs; ++r)
            for (int c = 0; c < bs; ++c)
                dst[r * stride + c] = static_cast<std::uint16_t>(r + c + 2 < 2 * bs ? avg3(above[r + c], above[r + c + 1], above[r + c + 2]) : above[2 * bs - 1]);
        break;
    case kD63Pred:
        for (int r = 0; r < bs; ++r) {
            const int o = r >> 1;
            for (int c = 0; c < bs; ++c)
                dst[r * stride + c] = static_cast<std::uint16_t>((r & 1) ? avg3(above[o + c], above[o + c + 1], above[o + c + 2]) : avg2(above[o + c], above[o + c + 1]));
        }
        break;
    case kD117Pred: {
        for (int c = 0; c < bs; ++c) dst[c] = static_cast<std::uint16_t>(avg2(above[c - 1], above[c]));
        dst[stride] = static_cast<std::uint16_t>(avg3(left[0], above[-1], above[0]));
        for (int c = 1; c < bs; ++c) dst[stride + c] = static_cast<std::uint16_t>(avg3(above[c - 2], above[c - 1], above[c]));
        dst[2 * stride] = static_cast<std::uint16_t>(avg3(above[-1], left[0], left[1]));
        for (int r = 3; r < bs; ++r) dst[r * stride] = static_cast<std::uint16_t>(avg3(left[r - 3], left[r - 2], left[r - 1]));
        for (int r = 2; r < bs; ++r)
            for (int c = 1; c < bs; ++c) dst[r * stride + c] = dst[(r - 2) * stride + c - 1];
        break;
    }
    case kD135Pred: {
        dst[0] = static_cast<std::uint16_t>(avg3(left[0], above[-1], above[0]));
        for (int c = 1; c < bs; ++c) dst[c] = static_cast<std::uint16_t>(avg3(above[c - 2], above[c - 1], above[c]));
        dst[stride] = static_cast<std::uint16_t>(avg3(above[-1], left[0], left[1]));
        for (int r = 2; r < bs; ++r) dst[r * stride] = static_cast<std::uint16_t>(avg3(left[r - 2], left[r - 1], left[r]));
        for (int r = 1; r < bs; ++r)
            for (int c = 1; c < bs; ++c) dst[r * stride + c] = dst[(r - 1) * stride + c - 1];
        break;
    }
    case kD153Pred: {
        dst[0] = static_cast<std::uint16_t>(avg2(above[-1], left[0]));
        for (int r = 1; r < bs; ++r) dst[r * stride] = static_cast<std::uint16_t>(avg2(left[r - 1], left[r]));
        dst[1] = static_cast<std::uint16_t>(avg3(left[0], above[-1], above[0]));
        dst[stride + 1] = static_cast<std::uint16_t>(avg3(above[-1], left[0], left[1]));
        for (int r = 2; r < bs; ++r) dst[r * stride + 1] = static_cast<std::uint16_t>(avg3(left[r - 2], left[r - 1], left[r]));
        for (int c = 0; c < bs - 2; ++c) dst[2 + c] = static_cast<std::uint16_t>(avg3(above[c - 1], above[c], above[c + 1]));
        for (int r = 1; r < bs; ++r)
            for (int c = 0; c < bs - 2; ++c) dst[r * stride + 2 + c] = dst[(r - 1) * stride + c];
        break;
    }
    case kD207Pred: {
        for (int r = 0; r < bs - 1; ++r) dst[r * stride] = static_cast<std::uint16_t>(avg2(left[r], left[r + 1]));
        dst[(bs - 1) * stride] = static_cast<std::uint16_t>(left[bs - 1]);
        for (int r = 0; r < bs - 2; ++r) dst[r * stride + 1] = static_cast<std::uint16_t>(avg3(left[r], left[r + 1], left[r + 2]));
        dst[(bs - 2) * stride + 1] = static_cast<std::uint16_t>(avg3(left[bs - 2], left[bs - 1], left[bs - 1]));
        dst[(bs - 1) * stride + 1] = static_cast<std::uint16_t>(left[bs - 1]);
        for (int c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + 2 + c] = static_cast<std::uint16_t>(left[bs - 1]);
        for (int r = bs - 2; r >= 0; --r)
            for (int c = 0; c < bs - 2; ++c) dst[r * stride + 2 + c] = dst[(r + 1) * stride + c];
        break;
    }
    default:
        break;
    }
}

namespace {

typedef std::int64_t th;

const int c1 = 16364, c2 = 16305, c3 = 16207, c4 = 16069, c5 = 15893, c6 = 15679, c7 = 15426, c8 = 15137;
const int c9 = 14811, c10 = 14449, c11 = 14053, c12 = 13623, c13 = 13160, c14 = 12665, c15 = 12140, c16 = 11585;
const int c17 = 11003, c18 = 10394, c19 = 9760, c20 = 9102, c21 = 8423, c22 = 7723, c23 = 7005, c24 = 6270;
const int c25 = 5520, c26 = 4756, c27 = 3981, c28 = 3196, c29 = 2404, c30 = 1606, c31 = 804;
const int s1_9 = 5283, s2_9 = 9929, s3_9 = 13377, s4_9 = 15212;

inline std::int32_t rnd(th x) noexcept { return static_cast<std::int32_t>((x + (th(1) << 13)) >> 14); }

void idct4(const std::int32_t* in, std::int32_t* out) noexcept {
    const std::int32_t s0 = rnd(th(in[0] + in[2]) * c16);
    const std::int32_t s1 = rnd(th(in[0] - in[2]) * c16);
    const std::int32_t s2 = rnd(th(in[1]) * c24 - th(in[3]) * c8);
    const std::int32_t s3 = rnd(th(in[1]) * c8 + th(in[3]) * c24);
    out[0] = s0 + s3;
    out[1] = s1 + s2;
    out[2] = s1 - s2;
    out[3] = s0 - s3;
}

void iadst4(const std::int32_t* in, std::int32_t* out) noexcept {
    const th x0 = in[0];
    const th x1 = in[1];
    const th x2 = in[2];
    const th x3 = in[3];
    if (!(x0 | x1 | x2 | x3)) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    th s0 = s1_9 * x0;
    th s1 = s2_9 * x0;
    th s2 = s3_9 * x1;
    th s3 = s4_9 * x2;
    const th s4 = s1_9 * x2;
    const th s5 = s2_9 * x3;
    const th s6 = s4_9 * x3;
    const th s7 = static_cast<std::int32_t>(x0 - x2 + x3);
    s0 = s0 + s3 + s5;
    s1 = s1 - s4 - s6;
    s3 = s2;
    s2 = s3_9 * s7;
    out[0] = rnd(s0 + s3);
    out[1] = rnd(s1 + s3);
    out[2] = rnd(s2);
    out[3] = rnd(s0 + s1 - s3);
}

void idct8(const std::int32_t* in, std::int32_t* out) noexcept {
    std::int32_t a[8];
    std::int32_t b[8];
    a[0] = in[0];
    a[2] = in[4];
    a[1] = in[2];
    a[3] = in[6];
    a[4] = rnd(th(in[1]) * c28 - th(in[7]) * c4);
    a[7] = rnd(th(in[1]) * c4 + th(in[7]) * c28);
    a[5] = rnd(th(in[5]) * c12 - th(in[3]) * c20);
    a[6] = rnd(th(in[5]) * c20 + th(in[3]) * c12);
    b[0] = rnd(th(a[0] + a[2]) * c16);
    b[1] = rnd(th(a[0] - a[2]) * c16);
    b[2] = rnd(th(a[1]) * c24 - th(a[3]) * c8);
    b[3] = rnd(th(a[1]) * c8 + th(a[3]) * c24);
    b[4] = a[4] + a[5];
    b[5] = a[4] - a[5];
    b[6] = -a[6] + a[7];
    b[7] = a[6] + a[7];
    a[0] = b[0] + b[3];
    a[1] = b[1] + b[2];
    a[2] = b[1] - b[2];
    a[3] = b[0] - b[3];
    a[4] = b[4];
    a[5] = rnd(th(b[6] - b[5]) * c16);
    a[6] = rnd(th(b[5] + b[6]) * c16);
    a[7] = b[7];
    out[0] = a[0] + a[7];
    out[1] = a[1] + a[6];
    out[2] = a[2] + a[5];
    out[3] = a[3] + a[4];
    out[4] = a[3] - a[4];
    out[5] = a[2] - a[5];
    out[6] = a[1] - a[6];
    out[7] = a[0] - a[7];
}

void iadst8(const std::int32_t* in, std::int32_t* out) noexcept {
    th x0 = in[7];
    th x1 = in[0];
    th x2 = in[5];
    th x3 = in[2];
    th x4 = in[3];
    th x5 = in[4];
    th x6 = in[1];
    th x7 = in[6];
    if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7)) {
        for (int i = 0; i < 8; ++i) out[i] = 0;
        return;
    }
    th s0 = c2 * x0 + c30 * x1;
    th s1 = c30 * x0 - c2 * x1;
    th s2 = c10 * x2 + c22 * x3;
    th s3 = c22 * x2 - c10 * x3;
    th s4 = c18 * x4 + c14 * x5;
    th s5 = c14 * x4 - c18 * x5;
    th s6 = c26 * x6 + c6 * x7;
    th s7 = c6 * x6 - c26 * x7;
    x0 = rnd(s0 + s4);
    x1 = rnd(s1 + s5);
    x2 = rnd(s2 + s6);
    x3 = rnd(s3 + s7);
    x4 = rnd(s0 - s4);
    x5 = rnd(s1 - s5);
    x6 = rnd(s2 - s6);
    x7 = rnd(s3 - s7);
    s0 = x0;
    s1 = x1;
    s2 = x2;
    s3 = x3;
    s4 = c8 * x4 + c24 * x5;
    s5 = c24 * x4 - c8 * x5;
    s6 = -c24 * x6 + c8 * x7;
    s7 = c8 * x6 + c24 * x7;
    x0 = s0 + s2;
    x1 = s1 + s3;
    x2 = s0 - s2;
    x3 = s1 - s3;
    x4 = rnd(s4 + s6);
    x5 = rnd(s5 + s7);
    x6 = rnd(s4 - s6);
    x7 = rnd(s5 - s7);
    s2 = c16 * (x2 + x3);
    s3 = c16 * (x2 - x3);
    s6 = c16 * (x6 + x7);
    s7 = c16 * (x6 - x7);
    x2 = rnd(s2);
    x3 = rnd(s3);
    x6 = rnd(s6);
    x7 = rnd(s7);
    out[0] = static_cast<std::int32_t>(x0);
    out[1] = static_cast<std::int32_t>(-x4);
    out[2] = static_cast<std::int32_t>(x6);
    out[3] = static_cast<std::int32_t>(-x2);
    out[4] = static_cast<std::int32_t>(x3);
    out[5] = static_cast<std::int32_t>(-x7);
    out[6] = static_cast<std::int32_t>(x5);
    out[7] = static_cast<std::int32_t>(-x1);
}

void idct16(const std::int32_t* in, std::int32_t* out) noexcept {
    std::int32_t a[16];
    std::int32_t b[16];
    a[0] = in[0];
    a[1] = in[8];
    a[2] = in[4];
    a[3] = in[12];
    a[4] = in[2];
    a[5] = in[10];
    a[6] = in[6];
    a[7] = in[14];
    a[8] = in[1];
    a[9] = in[9];
    a[10] = in[5];
    a[11] = in[13];
    a[12] = in[3];
    a[13] = in[11];
    a[14] = in[7];
    a[15] = in[15];

    for (int i = 0; i < 8; ++i) b[i] = a[i];
    b[8] = rnd(th(a[8]) * c30 - th(a[15]) * c2);
    b[15] = rnd(th(a[8]) * c2 + th(a[15]) * c30);
    b[9] = rnd(th(a[9]) * c14 - th(a[14]) * c18);
    b[14] = rnd(th(a[9]) * c18 + th(a[14]) * c14);
    b[10] = rnd(th(a[10]) * c22 - th(a[13]) * c10);
    b[13] = rnd(th(a[10]) * c10 + th(a[13]) * c22);
    b[11] = rnd(th(a[11]) * c6 - th(a[12]) * c26);
    b[12] = rnd(th(a[11]) * c26 + th(a[12]) * c6);

    a[0] = b[0];
    a[1] = b[1];
    a[2] = b[2];
    a[3] = b[3];
    a[4] = rnd(th(b[4]) * c28 - th(b[7]) * c4);
    a[7] = rnd(th(b[4]) * c4 + th(b[7]) * c28);
    a[5] = rnd(th(b[5]) * c12 - th(b[6]) * c20);
    a[6] = rnd(th(b[5]) * c20 + th(b[6]) * c12);
    a[8] = b[8] + b[9];
    a[9] = b[8] - b[9];
    a[10] = -b[10] + b[11];
    a[11] = b[10] + b[11];
    a[12] = b[12] + b[13];
    a[13] = b[12] - b[13];
    a[14] = -b[14] + b[15];
    a[15] = b[14] + b[15];

    b[0] = rnd(th(a[0] + a[1]) * c16);
    b[1] = rnd(th(a[0] - a[1]) * c16);
    b[2] = rnd(th(a[2]) * c24 - th(a[3]) * c8);
    b[3] = rnd(th(a[2]) * c8 + th(a[3]) * c24);
    b[4] = a[4] + a[5];
    b[5] = a[4] - a[5];
    b[6] = -a[6] + a[7];
    b[7] = a[6] + a[7];
    b[8] = a[8];
    b[15] = a[15];
    b[9] = rnd(-th(a[9]) * c8 + th(a[14]) * c24);
    b[14] = rnd(th(a[9]) * c24 + th(a[14]) * c8);
    b[10] = rnd(-th(a[10]) * c24 - th(a[13]) * c8);
    b[13] = rnd(-th(a[10]) * c8 + th(a[13]) * c24);
    b[11] = a[11];
    b[12] = a[12];

    a[0] = b[0] + b[3];
    a[1] = b[1] + b[2];
    a[2] = b[1] - b[2];
    a[3] = b[0] - b[3];
    a[4] = b[4];
    a[5] = rnd(th(b[6] - b[5]) * c16);
    a[6] = rnd(th(b[5] + b[6]) * c16);
    a[7] = b[7];
    a[8] = b[8] + b[11];
    a[9] = b[9] + b[10];
    a[10] = b[9] - b[10];
    a[11] = b[8] - b[11];
    a[12] = -b[12] + b[15];
    a[13] = -b[13] + b[14];
    a[14] = b[13] + b[14];
    a[15] = b[12] + b[15];

    b[0] = a[0] + a[7];
    b[1] = a[1] + a[6];
    b[2] = a[2] + a[5];
    b[3] = a[3] + a[4];
    b[4] = a[3] - a[4];
    b[5] = a[2] - a[5];
    b[6] = a[1] - a[6];
    b[7] = a[0] - a[7];
    b[8] = a[8];
    b[9] = a[9];
    b[10] = rnd(th(-a[10] + a[13]) * c16);
    b[13] = rnd(th(a[10] + a[13]) * c16);
    b[11] = rnd(th(-a[11] + a[12]) * c16);
    b[12] = rnd(th(a[11] + a[12]) * c16);
    b[14] = a[14];
    b[15] = a[15];

    for (int i = 0; i < 8; ++i) {
        out[i] = b[i] + b[15 - i];
        out[15 - i] = b[i] - b[15 - i];
    }
}

void iadst16(const std::int32_t* in, std::int32_t* out) noexcept {
    th x0 = in[15];
    th x1 = in[0];
    th x2 = in[13];
    th x3 = in[2];
    th x4 = in[11];
    th x5 = in[4];
    th x6 = in[9];
    th x7 = in[6];
    th x8 = in[7];
    th x9 = in[8];
    th x10 = in[5];
    th x11 = in[10];
    th x12 = in[3];
    th x13 = in[12];
    th x14 = in[1];
    th x15 = in[14];
    if (!(x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7 | x8 | x9 | x10 | x11 | x12 | x13 | x14 | x15)) {
        for (int i = 0; i < 16; ++i) out[i] = 0;
        return;
    }
    th s0 = x0 * c1 + x1 * c31;
    th s1 = x0 * c31 - x1 * c1;
    th s2 = x2 * c5 + x3 * c27;
    th s3 = x2 * c27 - x3 * c5;
    th s4 = x4 * c9 + x5 * c23;
    th s5 = x4 * c23 - x5 * c9;
    th s6 = x6 * c13 + x7 * c19;
    th s7 = x6 * c19 - x7 * c13;
    th s8 = x8 * c17 + x9 * c15;
    th s9 = x8 * c15 - x9 * c17;
    th s10 = x10 * c21 + x11 * c11;
    th s11 = x10 * c11 - x11 * c21;
    th s12 = x12 * c25 + x13 * c7;
    th s13 = x12 * c7 - x13 * c25;
    th s14 = x14 * c29 + x15 * c3;
    th s15 = x14 * c3 - x15 * c29;
    x0 = rnd(s0 + s8);
    x1 = rnd(s1 + s9);
    x2 = rnd(s2 + s10);
    x3 = rnd(s3 + s11);
    x4 = rnd(s4 + s12);
    x5 = rnd(s5 + s13);
    x6 = rnd(s6 + s14);
    x7 = rnd(s7 + s15);
    x8 = rnd(s0 - s8);
    x9 = rnd(s1 - s9);
    x10 = rnd(s2 - s10);
    x11 = rnd(s3 - s11);
    x12 = rnd(s4 - s12);
    x13 = rnd(s5 - s13);
    x14 = rnd(s6 - s14);
    x15 = rnd(s7 - s15);
    s0 = x0;
    s1 = x1;
    s2 = x2;
    s3 = x3;
    s4 = x4;
    s5 = x5;
    s6 = x6;
    s7 = x7;
    s8 = x8 * c4 + x9 * c28;
    s9 = x8 * c28 - x9 * c4;
    s10 = x10 * c20 + x11 * c12;
    s11 = x10 * c12 - x11 * c20;
    s12 = -x12 * c28 + x13 * c4;
    s13 = x12 * c4 + x13 * c28;
    s14 = -x14 * c12 + x15 * c20;
    s15 = x14 * c20 + x15 * c12;
    x0 = s0 + s4;
    x1 = s1 + s5;
    x2 = s2 + s6;
    x3 = s3 + s7;
    x4 = s0 - s4;
    x5 = s1 - s5;
    x6 = s2 - s6;
    x7 = s3 - s7;
    x8 = rnd(s8 + s12);
    x9 = rnd(s9 + s13);
    x10 = rnd(s10 + s14);
    x11 = rnd(s11 + s15);
    x12 = rnd(s8 - s12);
    x13 = rnd(s9 - s13);
    x14 = rnd(s10 - s14);
    x15 = rnd(s11 - s15);
    s0 = x0;
    s1 = x1;
    s2 = x2;
    s3 = x3;
    s4 = x4 * c8 + x5 * c24;
    s5 = x4 * c24 - x5 * c8;
    s6 = -x6 * c24 + x7 * c8;
    s7 = x6 * c8 + x7 * c24;
    s8 = x8;
    s9 = x9;
    s10 = x10;
    s11 = x11;
    s12 = x12 * c8 + x13 * c24;
    s13 = x12 * c24 - x13 * c8;
    s14 = -x14 * c24 + x15 * c8;
    s15 = x14 * c8 + x15 * c24;
    x0 = s0 + s2;
    x1 = s1 + s3;
    x2 = s0 - s2;
    x3 = s1 - s3;
    x4 = rnd(s4 + s6);
    x5 = rnd(s5 + s7);
    x6 = rnd(s4 - s6);
    x7 = rnd(s5 - s7);
    x8 = s8 + s10;
    x9 = s9 + s11;
    x10 = s8 - s10;
    x11 = s9 - s11;
    x12 = rnd(s12 + s14);
    x13 = rnd(s13 + s15);
    x14 = rnd(s12 - s14);
    x15 = rnd(s13 - s15);
    s2 = (-c16) * (x2 + x3);
    s3 = c16 * (x2 - x3);
    s6 = c16 * (x6 + x7);
    s7 = c16 * (-x6 + x7);
    s10 = c16 * (x10 + x11);
    s11 = c16 * (-x10 + x11);
    s14 = (-c16) * (x14 + x15);
    s15 = c16 * (x14 - x15);
    x2 = rnd(s2);
    x3 = rnd(s3);
    x6 = rnd(s6);
    x7 = rnd(s7);
    x10 = rnd(s10);
    x11 = rnd(s11);
    x14 = rnd(s14);
    x15 = rnd(s15);
    out[0] = static_cast<std::int32_t>(x0);
    out[1] = static_cast<std::int32_t>(-x8);
    out[2] = static_cast<std::int32_t>(x12);
    out[3] = static_cast<std::int32_t>(-x4);
    out[4] = static_cast<std::int32_t>(x6);
    out[5] = static_cast<std::int32_t>(x14);
    out[6] = static_cast<std::int32_t>(x10);
    out[7] = static_cast<std::int32_t>(x2);
    out[8] = static_cast<std::int32_t>(x3);
    out[9] = static_cast<std::int32_t>(x11);
    out[10] = static_cast<std::int32_t>(x15);
    out[11] = static_cast<std::int32_t>(x7);
    out[12] = static_cast<std::int32_t>(x5);
    out[13] = static_cast<std::int32_t>(-x13);
    out[14] = static_cast<std::int32_t>(x9);
    out[15] = static_cast<std::int32_t>(-x1);
}

void idct32(const std::int32_t* in, std::int32_t* out) noexcept {
    std::int32_t a[32];
    std::int32_t b[32];
    a[0] = in[0];
    a[1] = in[16];
    a[2] = in[8];
    a[3] = in[24];
    a[4] = in[4];
    a[5] = in[20];
    a[6] = in[12];
    a[7] = in[28];
    a[8] = in[2];
    a[9] = in[18];
    a[10] = in[10];
    a[11] = in[26];
    a[12] = in[6];
    a[13] = in[22];
    a[14] = in[14];
    a[15] = in[30];
    a[16] = rnd(th(in[1]) * c31 - th(in[31]) * c1);
    a[31] = rnd(th(in[1]) * c1 + th(in[31]) * c31);
    a[17] = rnd(th(in[17]) * c15 - th(in[15]) * c17);
    a[30] = rnd(th(in[17]) * c17 + th(in[15]) * c15);
    a[18] = rnd(th(in[9]) * c23 - th(in[23]) * c9);
    a[29] = rnd(th(in[9]) * c9 + th(in[23]) * c23);
    a[19] = rnd(th(in[25]) * c7 - th(in[7]) * c25);
    a[28] = rnd(th(in[25]) * c25 + th(in[7]) * c7);
    a[20] = rnd(th(in[5]) * c27 - th(in[27]) * c5);
    a[27] = rnd(th(in[5]) * c5 + th(in[27]) * c27);
    a[21] = rnd(th(in[21]) * c11 - th(in[11]) * c21);
    a[26] = rnd(th(in[21]) * c21 + th(in[11]) * c11);
    a[22] = rnd(th(in[13]) * c19 - th(in[19]) * c13);
    a[25] = rnd(th(in[13]) * c13 + th(in[19]) * c19);
    a[23] = rnd(th(in[29]) * c3 - th(in[3]) * c29);
    a[24] = rnd(th(in[29]) * c29 + th(in[3]) * c3);

    for (int i = 0; i < 8; ++i) b[i] = a[i];
    b[8] = rnd(th(a[8]) * c30 - th(a[15]) * c2);
    b[15] = rnd(th(a[8]) * c2 + th(a[15]) * c30);
    b[9] = rnd(th(a[9]) * c14 - th(a[14]) * c18);
    b[14] = rnd(th(a[9]) * c18 + th(a[14]) * c14);
    b[10] = rnd(th(a[10]) * c22 - th(a[13]) * c10);
    b[13] = rnd(th(a[10]) * c10 + th(a[13]) * c22);
    b[11] = rnd(th(a[11]) * c6 - th(a[12]) * c26);
    b[12] = rnd(th(a[11]) * c26 + th(a[12]) * c6);
    b[16] = a[16] + a[17];
    b[17] = a[16] - a[17];
    b[18] = -a[18] + a[19];
    b[19] = a[18] + a[19];
    b[20] = a[20] + a[21];
    b[21] = a[20] - a[21];
    b[22] = -a[22] + a[23];
    b[23] = a[22] + a[23];
    b[24] = a[24] + a[25];
    b[25] = a[24] - a[25];
    b[26] = -a[26] + a[27];
    b[27] = a[26] + a[27];
    b[28] = a[28] + a[29];
    b[29] = a[28] - a[29];
    b[30] = -a[30] + a[31];
    b[31] = a[30] + a[31];

    a[0] = b[0];
    a[1] = b[1];
    a[2] = b[2];
    a[3] = b[3];
    a[4] = rnd(th(b[4]) * c28 - th(b[7]) * c4);
    a[7] = rnd(th(b[4]) * c4 + th(b[7]) * c28);
    a[5] = rnd(th(b[5]) * c12 - th(b[6]) * c20);
    a[6] = rnd(th(b[5]) * c20 + th(b[6]) * c12);
    a[8] = b[8] + b[9];
    a[9] = b[8] - b[9];
    a[10] = -b[10] + b[11];
    a[11] = b[10] + b[11];
    a[12] = b[12] + b[13];
    a[13] = b[12] - b[13];
    a[14] = -b[14] + b[15];
    a[15] = b[14] + b[15];
    a[16] = b[16];
    a[31] = b[31];
    a[17] = rnd(-th(b[17]) * c4 + th(b[30]) * c28);
    a[30] = rnd(th(b[17]) * c28 + th(b[30]) * c4);
    a[18] = rnd(-th(b[18]) * c28 - th(b[29]) * c4);
    a[29] = rnd(-th(b[18]) * c4 + th(b[29]) * c28);
    a[19] = b[19];
    a[20] = b[20];
    a[21] = rnd(-th(b[21]) * c20 + th(b[26]) * c12);
    a[26] = rnd(th(b[21]) * c12 + th(b[26]) * c20);
    a[22] = rnd(-th(b[22]) * c12 - th(b[25]) * c20);
    a[25] = rnd(-th(b[22]) * c20 + th(b[25]) * c12);
    a[23] = b[23];
    a[24] = b[24];
    a[27] = b[27];
    a[28] = b[28];

    b[0] = rnd(th(a[0] + a[1]) * c16);
    b[1] = rnd(th(a[0] - a[1]) * c16);
    b[2] = rnd(th(a[2]) * c24 - th(a[3]) * c8);
    b[3] = rnd(th(a[2]) * c8 + th(a[3]) * c24);
    b[4] = a[4] + a[5];
    b[5] = a[4] - a[5];
    b[6] = -a[6] + a[7];
    b[7] = a[6] + a[7];
    b[8] = a[8];
    b[15] = a[15];
    b[9] = rnd(-th(a[9]) * c8 + th(a[14]) * c24);
    b[14] = rnd(th(a[9]) * c24 + th(a[14]) * c8);
    b[10] = rnd(-th(a[10]) * c24 - th(a[13]) * c8);
    b[13] = rnd(-th(a[10]) * c8 + th(a[13]) * c24);
    b[11] = a[11];
    b[12] = a[12];
    b[16] = a[16] + a[19];
    b[17] = a[17] + a[18];
    b[18] = a[17] - a[18];
    b[19] = a[16] - a[19];
    b[20] = -a[20] + a[23];
    b[21] = -a[21] + a[22];
    b[22] = a[21] + a[22];
    b[23] = a[20] + a[23];
    b[24] = a[24] + a[27];
    b[25] = a[25] + a[26];
    b[26] = a[25] - a[26];
    b[27] = a[24] - a[27];
    b[28] = -a[28] + a[31];
    b[29] = -a[29] + a[30];
    b[30] = a[29] + a[30];
    b[31] = a[28] + a[31];

    a[0] = b[0] + b[3];
    a[1] = b[1] + b[2];
    a[2] = b[1] - b[2];
    a[3] = b[0] - b[3];
    a[4] = b[4];
    a[5] = rnd(th(b[6] - b[5]) * c16);
    a[6] = rnd(th(b[5] + b[6]) * c16);
    a[7] = b[7];
    a[8] = b[8] + b[11];
    a[9] = b[9] + b[10];
    a[10] = b[9] - b[10];
    a[11] = b[8] - b[11];
    a[12] = -b[12] + b[15];
    a[13] = -b[13] + b[14];
    a[14] = b[13] + b[14];
    a[15] = b[12] + b[15];
    a[16] = b[16];
    a[17] = b[17];
    a[18] = rnd(-th(b[18]) * c8 + th(b[29]) * c24);
    a[29] = rnd(th(b[18]) * c24 + th(b[29]) * c8);
    a[19] = rnd(-th(b[19]) * c8 + th(b[28]) * c24);
    a[28] = rnd(th(b[19]) * c24 + th(b[28]) * c8);
    a[20] = rnd(-th(b[20]) * c24 - th(b[27]) * c8);
    a[27] = rnd(-th(b[20]) * c8 + th(b[27]) * c24);
    a[21] = rnd(-th(b[21]) * c24 - th(b[26]) * c8);
    a[26] = rnd(-th(b[21]) * c8 + th(b[26]) * c24);
    a[22] = b[22];
    a[23] = b[23];
    a[24] = b[24];
    a[25] = b[25];
    a[30] = b[30];
    a[31] = b[31];

    b[0] = a[0] + a[7];
    b[1] = a[1] + a[6];
    b[2] = a[2] + a[5];
    b[3] = a[3] + a[4];
    b[4] = a[3] - a[4];
    b[5] = a[2] - a[5];
    b[6] = a[1] - a[6];
    b[7] = a[0] - a[7];
    b[8] = a[8];
    b[9] = a[9];
    b[10] = rnd(th(-a[10] + a[13]) * c16);
    b[13] = rnd(th(a[10] + a[13]) * c16);
    b[11] = rnd(th(-a[11] + a[12]) * c16);
    b[12] = rnd(th(a[11] + a[12]) * c16);
    b[14] = a[14];
    b[15] = a[15];
    b[16] = a[16] + a[23];
    b[17] = a[17] + a[22];
    b[18] = a[18] + a[21];
    b[19] = a[19] + a[20];
    b[20] = a[19] - a[20];
    b[21] = a[18] - a[21];
    b[22] = a[17] - a[22];
    b[23] = a[16] - a[23];
    b[24] = -a[24] + a[31];
    b[25] = -a[25] + a[30];
    b[26] = -a[26] + a[29];
    b[27] = -a[27] + a[28];
    b[28] = a[27] + a[28];
    b[29] = a[26] + a[29];
    b[30] = a[25] + a[30];
    b[31] = a[24] + a[31];

    for (int i = 0; i < 8; ++i) {
        a[i] = b[i] + b[15 - i];
        a[15 - i] = b[i] - b[15 - i];
    }
    a[16] = b[16];
    a[17] = b[17];
    a[18] = b[18];
    a[19] = b[19];
    a[20] = rnd(th(-b[20] + b[27]) * c16);
    a[27] = rnd(th(b[20] + b[27]) * c16);
    a[21] = rnd(th(-b[21] + b[26]) * c16);
    a[26] = rnd(th(b[21] + b[26]) * c16);
    a[22] = rnd(th(-b[22] + b[25]) * c16);
    a[25] = rnd(th(b[22] + b[25]) * c16);
    a[23] = rnd(th(-b[23] + b[24]) * c16);
    a[24] = rnd(th(b[23] + b[24]) * c16);
    a[28] = b[28];
    a[29] = b[29];
    a[30] = b[30];
    a[31] = b[31];

    for (int i = 0; i < 16; ++i) {
        out[i] = a[i] + a[31 - i];
        out[31 - i] = a[i] - a[31 - i];
    }
}

typedef void (*Tx1d)(const std::int32_t*, std::int32_t*);

void iwht4_add(std::uint16_t* dst, std::ptrdiff_t stride, const std::int32_t* in, int maxv) noexcept {
    std::int32_t tmp[16];
    for (int i = 0; i < 4; ++i) {
        std::int32_t a1 = in[4 * i + 0] >> 2;
        std::int32_t c1v = in[4 * i + 1] >> 2;
        std::int32_t d1 = in[4 * i + 2] >> 2;
        std::int32_t b1 = in[4 * i + 3] >> 2;
        a1 += c1v;
        d1 -= b1;
        const std::int32_t e1 = (a1 - d1) >> 1;
        b1 = e1 - b1;
        c1v = e1 - c1v;
        a1 -= b1;
        d1 += c1v;
        tmp[4 * i + 0] = a1;
        tmp[4 * i + 1] = b1;
        tmp[4 * i + 2] = c1v;
        tmp[4 * i + 3] = d1;
    }
    for (int i = 0; i < 4; ++i) {
        std::int32_t a1 = tmp[i];
        std::int32_t c1v = tmp[4 + i];
        std::int32_t d1 = tmp[8 + i];
        std::int32_t b1 = tmp[12 + i];
        a1 += c1v;
        d1 -= b1;
        const std::int32_t e1 = (a1 - d1) >> 1;
        b1 = e1 - b1;
        c1v = e1 - c1v;
        a1 -= b1;
        d1 += c1v;
        dst[0 * stride + i] = clip_px(dst[0 * stride + i] + a1, maxv);
        dst[1 * stride + i] = clip_px(dst[1 * stride + i] + b1, maxv);
        dst[2 * stride + i] = clip_px(dst[2 * stride + i] + c1v, maxv);
        dst[3 * stride + i] = clip_px(dst[3 * stride + i] + d1, maxv);
    }
}

} // namespace

void inverse_transform_add(std::uint16_t* dst, std::ptrdiff_t stride, std::int32_t* coef, int tx_size, int tx_type, int eob, bool lossless, int bd) noexcept {
    const int maxv = (1 << bd) - 1;
    if (lossless) {
        iwht4_add(dst, stride, coef, maxv);
        return;
    }
    const int n = 4 << tx_size;
    const int shift = tx_size == kTx4x4 ? 4 : (tx_size == kTx8x8 ? 5 : 6);
    if (eob == 1 && tx_type == kDctDct) {
        std::int32_t out = rnd(th(coef[0]) * c16);
        out = rnd(th(out) * c16);
        const int a1 = (out + (1 << (shift - 1))) >> shift;
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c) dst[r * stride + c] = clip_px(dst[r * stride + c] + a1, maxv);
        return;
    }
    Tx1d row_tx;
    Tx1d col_tx;
    switch (tx_size) {
    case kTx4x4:
        row_tx = (tx_type & 2) ? iadst4 : idct4;
        col_tx = (tx_type & 1) ? iadst4 : idct4;
        break;
    case kTx8x8:
        row_tx = (tx_type & 2) ? iadst8 : idct8;
        col_tx = (tx_type & 1) ? iadst8 : idct8;
        break;
    case kTx16x16:
        row_tx = (tx_type & 2) ? iadst16 : idct16;
        col_tx = (tx_type & 1) ? iadst16 : idct16;
        break;
    default:
        row_tx = idct32;
        col_tx = idct32;
        break;
    }
    std::int32_t tmp[32 * 32];
    std::int32_t cin[32];
    std::int32_t cout[32];
    for (int i = 0; i < n; ++i) {
        const std::int32_t* in = coef + i * n;
        bool nz = false;
        for (int j = 0; j < n; ++j) {
            if (in[j]) {
                nz = true;
                break;
            }
        }
        if (nz) {
            row_tx(in, tmp + i * n);
        } else {
            for (int j = 0; j < n; ++j) tmp[i * n + j] = 0;
        }
    }
    const int rnd_add = 1 << (shift - 1);
    for (int c = 0; c < n; ++c) {
        for (int r = 0; r < n; ++r) cin[r] = tmp[r * n + c];
        col_tx(cin, cout);
        for (int r = 0; r < n; ++r) dst[r * stride + c] = clip_px(dst[r * stride + c] + ((cout[r] + rnd_add) >> shift), maxv);
    }
}

namespace {

template <int W, bool AVG>
void conv_copy(const std::uint16_t* src, std::ptrdiff_t ss, std::uint16_t* dst, std::ptrdiff_t ds, int h) noexcept {
    for (int r = 0; r < h; ++r) {
        const std::uint16_t* s = src + r * ss;
        std::uint16_t* d = dst + r * ds;
        for (int c = 0; c < W; ++c) d[c] = AVG ? static_cast<std::uint16_t>((d[c] + s[c] + 1) >> 1) : s[c];
    }
}

template <int W, bool AVG>
void conv_h(const std::uint16_t* src, std::ptrdiff_t ss, std::uint16_t* dst, std::ptrdiff_t ds, const std::int16_t* k, int h, int maxv) noexcept {
    const int k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5], k6 = k[6], k7 = k[7];
    for (int r = 0; r < h; ++r) {
        const std::uint16_t* s = src + r * ss - 3;
        std::uint16_t* d = dst + r * ds;
        for (int c = 0; c < W; ++c) {
            int v = (s[c] * k0 + s[c + 1] * k1 + s[c + 2] * k2 + s[c + 3] * k3 + s[c + 4] * k4 + s[c + 5] * k5 + s[c + 6] * k6 + s[c + 7] * k7 + 64) >> 7;
            v = v < 0 ? 0 : (v > maxv ? maxv : v);
            d[c] = AVG ? static_cast<std::uint16_t>((d[c] + v + 1) >> 1) : static_cast<std::uint16_t>(v);
        }
    }
}

template <int W, bool AVG>
void conv_v(const std::uint16_t* src, std::ptrdiff_t ss, std::uint16_t* dst, std::ptrdiff_t ds, const std::int16_t* k, int h, int maxv) noexcept {
    const int k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3], k4 = k[4], k5 = k[5], k6 = k[6], k7 = k[7];
    for (int r = 0; r < h; ++r) {
        const std::uint16_t* s0 = src + (r - 3) * ss;
        const std::uint16_t* s1 = s0 + ss;
        const std::uint16_t* s2 = s1 + ss;
        const std::uint16_t* s3 = s2 + ss;
        const std::uint16_t* s4 = s3 + ss;
        const std::uint16_t* s5 = s4 + ss;
        const std::uint16_t* s6 = s5 + ss;
        const std::uint16_t* s7 = s6 + ss;
        std::uint16_t* d = dst + r * ds;
        for (int c = 0; c < W; ++c) {
            int v = (s0[c] * k0 + s1[c] * k1 + s2[c] * k2 + s3[c] * k3 + s4[c] * k4 + s5[c] * k5 + s6[c] * k6 + s7[c] * k7 + 64) >> 7;
            v = v < 0 ? 0 : (v > maxv ? maxv : v);
            d[c] = AVG ? static_cast<std::uint16_t>((d[c] + v + 1) >> 1) : static_cast<std::uint16_t>(v);
        }
    }
}

template <int W, bool AVG>
void conv_unscaled(const std::uint16_t* src, std::ptrdiff_t ss, std::uint16_t* dst, std::ptrdiff_t ds, const std::int16_t (*kernel)[8], int fx, int fy, int h, int maxv, std::uint16_t* tmp) noexcept {
    if (!fx && !fy) {
        conv_copy<W, AVG>(src, ss, dst, ds, h);
    } else if (!fy) {
        conv_h<W, AVG>(src, ss, dst, ds, kernel[fx], h, maxv);
    } else if (!fx) {
        conv_v<W, AVG>(src, ss, dst, ds, kernel[fy], h, maxv);
    } else {
        conv_h<W, false>(src - 3 * ss, ss, tmp, 64, kernel[fx], h + 7, maxv);
        conv_v<W, AVG>(tmp + 3 * 64, 64, dst, ds, kernel[fy], h, maxv);
    }
}

template <bool AVG>
void conv_dispatch(const std::uint16_t* src, std::ptrdiff_t ss, std::uint16_t* dst, std::ptrdiff_t ds, const std::int16_t (*kernel)[8], int fx, int fy, int w, int h, int maxv, std::uint16_t* tmp) noexcept {
    switch (w) {
    case 4: conv_unscaled<4, AVG>(src, ss, dst, ds, kernel, fx, fy, h, maxv, tmp); break;
    case 8: conv_unscaled<8, AVG>(src, ss, dst, ds, kernel, fx, fy, h, maxv, tmp); break;
    case 16: conv_unscaled<16, AVG>(src, ss, dst, ds, kernel, fx, fy, h, maxv, tmp); break;
    case 32: conv_unscaled<32, AVG>(src, ss, dst, ds, kernel, fx, fy, h, maxv, tmp); break;
    default: conv_unscaled<64, AVG>(src, ss, dst, ds, kernel, fx, fy, h, maxv, tmp); break;
    }
}

} // namespace

void convolve(const std::uint16_t* src, std::ptrdiff_t src_stride, std::uint16_t* dst, std::ptrdiff_t dst_stride, const std::int16_t (*kernel)[8], int fx, int xs, int fy, int ys, int w, int h, int bd, bool average, std::int16_t* tmp_buf) noexcept {
    const int maxv = (1 << bd) - 1;
    std::uint16_t* tmp = reinterpret_cast<std::uint16_t*>(tmp_buf);
    if (xs == 16 && ys == 16 && (w == 4 || w == 8 || w == 16 || w == 32 || w == 64)) {
        if (average) conv_dispatch<true>(src, src_stride, dst, dst_stride, kernel, fx, fy, w, h, maxv, tmp);
        else conv_dispatch<false>(src, src_stride, dst, dst_stride, kernel, fx, fy, w, h, maxv, tmp);
        return;
    }
    const int ih = (((h - 1) * ys + fy) >> 4) + 8;
    for (int r = 0; r < ih; ++r) {
        const std::uint16_t* s = src + (r - 3) * src_stride;
        std::uint16_t* t = tmp + r * 64;
        int p = fx;
        for (int c = 0; c < w; ++c) {
            const std::uint16_t* sx = s + (p >> 4) - 3;
            const std::int16_t* k = kernel[p & 15];
            int sum = 0;
            for (int q = 0; q < 8; ++q) sum += sx[q] * k[q];
            t[c] = clip_px((sum + 64) >> 7, maxv);
            p += xs;
        }
    }
    int p = fy;
    for (int r = 0; r < h; ++r) {
        const std::uint16_t* t = tmp + (p >> 4) * 64;
        const std::int16_t* k = kernel[p & 15];
        std::uint16_t* d = dst + r * dst_stride;
        for (int c = 0; c < w; ++c) {
            int sum = 0;
            for (int q = 0; q < 8; ++q) sum += t[c + q * 64] * k[q];
            const int v = clip_px((sum + 64) >> 7, maxv);
            d[c] = static_cast<std::uint16_t>(average ? ((d[c] + v + 1) >> 1) : v);
        }
        p += ys;
    }
}

} // namespace vp9_detail
} // namespace video
} // namespace fizmo
