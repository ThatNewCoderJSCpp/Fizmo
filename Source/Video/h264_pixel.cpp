#include "h264_internal.hpp"

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace fizmo {
namespace video {
namespace h264_internal {

namespace {

inline int clip_bd(int v, int maxv) noexcept {
    return v < 0 ? 0 : (v > maxv ? maxv : v);
}

template <typename P>
void pred_dir(P* dst, int stride, int n, int mode, const int* tp, const int* lp, bool have_top, bool have_left, int bit_depth) {
    auto T = [tp](int x) { return tp[x + 1]; };
    auto L = [lp](int y) { return lp[y + 1]; };
    if (mode == 2) {
        int sum = 0;
        int dc;
        int sh = n == 4 ? 2 : 3;
        if (have_top && have_left) {
            for (int i = 0; i < n; ++i) sum += T(i) + L(i);
            dc = (sum + n) >> (sh + 1);
        } else if (have_left) {
            for (int i = 0; i < n; ++i) sum += L(i);
            dc = (sum + n / 2) >> sh;
        } else if (have_top) {
            for (int i = 0; i < n; ++i) sum += T(i);
            dc = (sum + n / 2) >> sh;
        } else {
            dc = 1 << (bit_depth - 1);
        }
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) dst[y * stride + x] = static_cast<P>(dc);
        }
        return;
    }
    for (int y = 0; y < n; ++y) {
        P* row = dst + y * stride;
        for (int x = 0; x < n; ++x) {
            int v;
            switch (mode) {
            case 0:
                v = T(x);
                break;
            case 1:
                v = L(y);
                break;
            case 3:
                if (x == n - 1 && y == n - 1) {
                    v = (T(2 * n - 2) + 3 * T(2 * n - 1) + 2) >> 2;
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
                if (z < 2 * n - 3 && !(z & 1)) {
                    v = (L(y + (x >> 1)) + L(y + (x >> 1) + 1) + 1) >> 1;
                } else if (z < 2 * n - 3) {
                    v = (L(y + (x >> 1)) + 2 * L(y + (x >> 1) + 1) + L(y + (x >> 1) + 2) + 2) >> 2;
                } else if (z == 2 * n - 3) {
                    v = (L(n - 2) + 3 * L(n - 1) + 2) >> 2;
                } else {
                    v = L(n - 1);
                }
                break;
            }
            }
            row[x] = static_cast<P>(v);
        }
    }
}

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

inline int tap6(int a, int b, int c, int d, int e, int f) noexcept {
    return a - 5 * b + 20 * c + 20 * d - 5 * e + f;
}

template <typename P>
const P* fetch_block(P* buf, int bw, int bh, const PicView& ref, int plane, int x0, int y0) {
    const P* base = reinterpret_cast<const P*>(ref.plane[plane]);
    int s = ref.stride[plane];
    int pw = plane == 0 ? ref.width : ref.chroma_width;
    int ph = plane == 0 ? ref.height : ref.chroma_height;
    int pad = ref.pad[plane];
    int vlo = ref.vpad ? -pad : 0;
    int vhi = ref.vpad ? ph + pad : ph;
    if (x0 >= -pad && y0 >= vlo && x0 + bw <= pw + pad && y0 + bh <= vhi) return nullptr;
    for (int y = 0; y < bh; ++y) {
        int sy = clip3(0, ph - 1, y0 + y);
        const P* row = base + static_cast<std::ptrdiff_t>(sy) * s;
        P* d = buf + y * bw;
        for (int x = 0; x < bw; ++x) d[x] = row[clip3(0, pw - 1, x0 + x)];
    }
    return buf;
}

} // namespace

template <typename P>
void intra_pred_nxn(P* dst, int stride, int n, int mode, const IntraEdge& e, int bit_depth) {
    int def = 1 << (bit_depth - 1);
    if (mode < 0 || mode > 8) mode = 2;
    if (n == 4) {
        int t[9];
        int l[5];
        t[0] = e.have_top_left ? e.top_left : def;
        for (int x = 0; x < 4; ++x) t[1 + x] = e.have_top ? e.top[x] : def;
        for (int x = 4; x < 8; ++x) t[1 + x] = (e.have_top && e.have_top_right) ? e.top[x] : t[4];
        l[0] = t[0];
        for (int y = 0; y < 4; ++y) l[1 + y] = e.have_left ? e.left[y] : def;
        pred_dir<P>(dst, stride, 4, mode, t, l, e.have_top, e.have_left, bit_depth);
        return;
    }
    int raw_t[16];
    int raw_l[8];
    int tl = e.have_top_left ? e.top_left : def;
    for (int x = 0; x < 8; ++x) raw_t[x] = e.have_top ? e.top[x] : def;
    for (int x = 8; x < 16; ++x) raw_t[x] = (e.have_top && e.have_top_right) ? e.top[x] : raw_t[7];
    for (int y = 0; y < 8; ++y) raw_l[y] = e.have_left ? e.left[y] : def;
    int t[17];
    int l[9];
    if (e.have_top) {
        t[1] = e.have_top_left ? (tl + 2 * raw_t[0] + raw_t[1] + 2) >> 2 : (3 * raw_t[0] + raw_t[1] + 2) >> 2;
        for (int x = 1; x < 15; ++x) t[1 + x] = (raw_t[x - 1] + 2 * raw_t[x] + raw_t[x + 1] + 2) >> 2;
        t[16] = (raw_t[14] + 3 * raw_t[15] + 2) >> 2;
    } else {
        for (int x = 0; x < 16; ++x) t[1 + x] = def;
    }
    int ftl = tl;
    if (e.have_top_left) {
        if (e.have_top && e.have_left) {
            ftl = (raw_t[0] + 2 * tl + raw_l[0] + 2) >> 2;
        } else if (e.have_top) {
            ftl = (3 * tl + raw_t[0] + 2) >> 2;
        } else if (e.have_left) {
            ftl = (3 * tl + raw_l[0] + 2) >> 2;
        }
    }
    t[0] = ftl;
    l[0] = ftl;
    if (e.have_left) {
        l[1] = e.have_top_left ? (tl + 2 * raw_l[0] + raw_l[1] + 2) >> 2 : (3 * raw_l[0] + raw_l[1] + 2) >> 2;
        for (int y = 1; y < 7; ++y) l[1 + y] = (raw_l[y - 1] + 2 * raw_l[y] + raw_l[y + 1] + 2) >> 2;
        l[8] = (raw_l[6] + 3 * raw_l[7] + 2) >> 2;
    } else {
        for (int y = 0; y < 8; ++y) l[1 + y] = def;
    }
    pred_dir<P>(dst, stride, 8, mode, t, l, e.have_top, e.have_left, bit_depth);
}

template <typename P>
void intra_pred_16x16(P* dst, int stride, int mode, const IntraEdge& e, int bit_depth) {
    int def = 1 << (bit_depth - 1);
    int maxv = (1 << bit_depth) - 1;
    int top[16];
    int left[16];
    for (int i = 0; i < 16; ++i) {
        top[i] = e.have_top ? e.top[i] : def;
        left[i] = e.have_left ? e.left[i] : def;
    }
    int tl = e.have_top_left ? e.top_left : def;
    switch (mode) {
    case 0:
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = static_cast<P>(top[x]);
        break;
    case 1:
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = static_cast<P>(left[y]);
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
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = static_cast<P>(clip_bd((base + b * (x - 7)) >> 5, maxv));
        }
        break;
    }
    default: {
        int sum = 0;
        int dc;
        if (e.have_top && e.have_left) {
            for (int i = 0; i < 16; ++i) sum += top[i] + left[i];
            dc = (sum + 16) >> 5;
        } else if (e.have_left) {
            for (int i = 0; i < 16; ++i) sum += left[i];
            dc = (sum + 8) >> 4;
        } else if (e.have_top) {
            for (int i = 0; i < 16; ++i) sum += top[i];
            dc = (sum + 8) >> 4;
        } else {
            dc = def;
        }
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) dst[y * stride + x] = static_cast<P>(dc);
        break;
    }
    }
}

template <typename P>
void intra_pred_chroma_block(P* dst, int stride, int w, int h, int mode, const IntraEdge& e, int bit_depth) {
    int def = 1 << (bit_depth - 1);
    int maxv = (1 << bit_depth) - 1;
    int top[16];
    int left[16];
    for (int i = 0; i < w; ++i) top[i] = e.have_top ? e.top[i] : def;
    for (int i = 0; i < h; ++i) left[i] = e.have_left ? e.left[i] : def;
    int tl = e.have_top_left ? e.top_left : def;
    switch (mode) {
    case 1:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) dst[y * stride + x] = static_cast<P>(left[y]);
        break;
    case 2:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) dst[y * stride + x] = static_cast<P>(top[x]);
        break;
    case 3: {
        int xcf = w == 16 ? 4 : 0;
        int ycf = h == 16 ? 4 : 0;
        int hs = 0;
        int vs = 0;
        for (int i = 0; i <= 3 + xcf; ++i) {
            int idx = 2 + xcf - i;
            int tp = idx >= 0 ? top[idx] : tl;
            hs += (i + 1) * (top[4 + xcf + i] - tp);
        }
        for (int i = 0; i <= 3 + ycf; ++i) {
            int idx = 2 + ycf - i;
            int lp = idx >= 0 ? left[idx] : tl;
            vs += (i + 1) * (left[4 + ycf + i] - lp);
        }
        int a = 16 * (left[h - 1] + top[w - 1]);
        int b = ((w == 16 ? 5 : 34) * hs + 32) >> 6;
        int c = ((h == 16 ? 5 : 34) * vs + 32) >> 6;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int v = (a + b * (x - 3 - xcf) + c * (y - 3 - ycf) + 16) >> 5;
                dst[y * stride + x] = static_cast<P>(clip_bd(v, maxv));
            }
        }
        break;
    }
    default:
        for (int by = 0; by < h / 4; ++by) {
            for (int bx = 0; bx < w / 4; ++bx) {
                int st = 0;
                int sl = 0;
                for (int i = 0; i < 4; ++i) {
                    st += top[bx * 4 + i];
                    sl += left[by * 4 + i];
                }
                int dc;
                bool both_rule = (bx == 0 && by == 0) || (bx > 0 && by > 0);
                if (both_rule) {
                    if (e.have_top && e.have_left) {
                        dc = (st + sl + 4) >> 3;
                    } else if (e.have_left) {
                        dc = (sl + 2) >> 2;
                    } else if (e.have_top) {
                        dc = (st + 2) >> 2;
                    } else {
                        dc = def;
                    }
                } else if (bx > 0) {
                    if (e.have_top) {
                        dc = (st + 2) >> 2;
                    } else if (e.have_left) {
                        dc = (sl + 2) >> 2;
                    } else {
                        dc = def;
                    }
                } else {
                    if (e.have_left) {
                        dc = (sl + 2) >> 2;
                    } else if (e.have_top) {
                        dc = (st + 2) >> 2;
                    } else {
                        dc = def;
                    }
                }
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 4; ++x) dst[(by * 4 + y) * stride + bx * 4 + x] = static_cast<P>(dc);
            }
        }
        break;
    }
}

template <typename P>
void idct4x4_add_t(P* dst, int stride, std::int32_t* b, int bit_depth) noexcept {
    int maxv = (1 << bit_depth) - 1;
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
        dst[j] = static_cast<P>(clip_bd(dst[j] + ((e + h + 32) >> 6), maxv));
        dst[stride + j] = static_cast<P>(clip_bd(dst[stride + j] + ((f + g + 32) >> 6), maxv));
        dst[2 * stride + j] = static_cast<P>(clip_bd(dst[2 * stride + j] + ((f - g + 32) >> 6), maxv));
        dst[3 * stride + j] = static_cast<P>(clip_bd(dst[3 * stride + j] + ((e - h + 32) >> 6), maxv));
    }
    std::memset(b, 0, 16 * sizeof(std::int32_t));
}

template <typename P>
void idct8x8_add_t(P* dst, int stride, std::int32_t* b, int bit_depth) noexcept {
    int maxv = (1 << bit_depth) - 1;
    std::int32_t t[64];
    std::int32_t r[64];
    for (int i = 0; i < 8; ++i) idct8_1d(b + i * 8, 1, t + i * 8, 1);
    for (int j = 0; j < 8; ++j) idct8_1d(t + j, 8, r + j, 8);
    for (int y = 0; y < 8; ++y) {
        P* row = dst + y * stride;
        for (int x = 0; x < 8; ++x) row[x] = static_cast<P>(clip_bd(row[x] + ((r[y * 8 + x] + 32) >> 6), maxv));
    }
    std::memset(b, 0, 64 * sizeof(std::int32_t));
}

template <typename P>
void residual_add_t(P* dst, int stride, const std::int32_t* res, int res_stride, int w, int h, int bit_depth) noexcept {
    int maxv = (1 << bit_depth) - 1;
    for (int y = 0; y < h; ++y) {
        P* row = dst + y * stride;
        const std::int32_t* r = res + y * res_stride;
        for (int x = 0; x < w; ++x) row[x] = static_cast<P>(clip_bd(row[x] + r[x], maxv));
    }
}

template <typename P>
void mc_luma_t(P* dst, int ds, const PicView& ref, int plane, int x, int y, int mvx, int mvy, int w, int h, int bit_depth) {
    int maxv = (1 << bit_depth) - 1;
    int xi = x + (mvx >> 2);
    int yi = y + (mvy >> 2);
    int fx = mvx & 3;
    int fy = mvy & 3;
    P edge[21 * 21];
    const P* src;
    int ss;
    const P* em = fetch_block<P>(edge, w + 5, h + 5, ref, plane, xi - 2, yi - 2);
    if (em) {
        src = em + 2 * (w + 5) + 2;
        ss = w + 5;
    } else {
        src = reinterpret_cast<const P*>(ref.plane[plane]) + static_cast<std::ptrdiff_t>(yi) * ref.stride[plane] + xi;
        ss = ref.stride[plane];
    }
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        (void)maxv;
        mc_luma_block8(dst, ds, src, ss, fx, fy, w, h);
        return;
    }
    if (fx == 0 && fy == 0) {
        for (int j = 0; j < h; ++j) std::memcpy(dst + j * ds, src + j * ss, static_cast<std::size_t>(w) * sizeof(P));
        return;
    }
    int bh[16 * 17];
    int bv[17 * 16];
    int jj[16 * 16];
    auto G = [&](int i, int j) -> int { return src[j * ss + i]; };
    auto hb = [&](int i, int j) -> int { return tap6(G(i - 2, j), G(i - 1, j), G(i, j), G(i + 1, j), G(i + 2, j), G(i + 3, j)); };
    auto vb = [&](int i, int j) -> int { return tap6(G(i, j - 2), G(i, j - 1), G(i, j), G(i, j + 1), G(i, j + 2), G(i, j + 3)); };
    if (fx != 0 && fy != 2) {
        for (int j = 0; j < h + 1; ++j)
            for (int i = 0; i < w; ++i) bh[j * 16 + i] = hb(i, j);
    }
    if (fy != 0 && fx != 2) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w + 1; ++i) bv[j * 17 + i] = vb(i, j);
    }
    bool center = (fx == 2 && fy != 0) || (fy == 2 && fx != 0);
    if (center) {
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                int t[6];
                for (int k = 0; k < 6; ++k) t[k] = hb(i, j - 2 + k);
                jj[j * 16 + i] = clip_bd((tap6(t[0], t[1], t[2], t[3], t[4], t[5]) + 512) >> 10, maxv);
            }
        }
    }
    auto b_at = [&](int i, int j) { return clip_bd((bh[j * 16 + i] + 16) >> 5, maxv); };
    auto h_at = [&](int i, int j) { return clip_bd((bv[j * 17 + i] + 16) >> 5, maxv); };
    for (int j = 0; j < h; ++j) {
        P* d = dst + j * ds;
        for (int i = 0; i < w; ++i) {
            int v;
            switch (fy * 4 + fx) {
            case 1: v = (G(i, j) + b_at(i, j) + 1) >> 1; break;
            case 2: v = b_at(i, j); break;
            case 3: v = (G(i + 1, j) + b_at(i, j) + 1) >> 1; break;
            case 4: v = (G(i, j) + h_at(i, j) + 1) >> 1; break;
            case 8: v = h_at(i, j); break;
            case 12: v = (G(i, j + 1) + h_at(i, j) + 1) >> 1; break;
            case 5: v = (b_at(i, j) + h_at(i, j) + 1) >> 1; break;
            case 7: v = (b_at(i, j) + h_at(i + 1, j) + 1) >> 1; break;
            case 13: v = (b_at(i, j + 1) + h_at(i, j) + 1) >> 1; break;
            case 15: v = (b_at(i, j + 1) + h_at(i + 1, j) + 1) >> 1; break;
            case 6: v = (b_at(i, j) + jj[j * 16 + i] + 1) >> 1; break;
            case 14: v = (b_at(i, j + 1) + jj[j * 16 + i] + 1) >> 1; break;
            case 9: v = (h_at(i, j) + jj[j * 16 + i] + 1) >> 1; break;
            case 11: v = (h_at(i + 1, j) + jj[j * 16 + i] + 1) >> 1; break;
            default: v = jj[j * 16 + i]; break;
            }
            d[i] = static_cast<P>(v);
        }
    }
}

template <typename P>
void mc_chroma_t(P* dst, int ds, const PicView& ref, int plane, int x, int y, int mvx8, int mvy8, int w, int h) {
    int xi = x + (mvx8 >> 3);
    int yi = y + (mvy8 >> 3);
    int fx = mvx8 & 7;
    int fy = mvy8 & 7;
    P edge[17 * 17];
    const P* src;
    int ss;
    const P* em = fetch_block<P>(edge, w + 1, h + 1, ref, plane, xi, yi);
    if (em) {
        src = em;
        ss = w + 1;
    } else {
        src = reinterpret_cast<const P*>(ref.plane[plane]) + static_cast<std::ptrdiff_t>(yi) * ref.stride[plane] + xi;
        ss = ref.stride[plane];
    }
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        mc_chroma_block8(dst, ds, src, ss, fx, fy, w, h);
        return;
    }
    if (fx == 0 && fy == 0) {
        for (int j = 0; j < h; ++j) std::memcpy(dst + j * ds, src + j * ss, static_cast<std::size_t>(w) * sizeof(P));
        return;
    }
    const int a = (8 - fx) * (8 - fy);
    const int b = fx * (8 - fy);
    const int c = (8 - fx) * fy;
    const int d = fx * fy;
    for (int j = 0; j < h; ++j) {
        const P* s0 = src + j * ss;
        const P* s1 = s0 + ss;
        P* o = dst + j * ds;
        for (int i = 0; i < w; ++i) o[i] = static_cast<P>((a * s0[i] + b * s0[i + 1] + c * s1[i] + d * s1[i + 1] + 32) >> 6);
    }
}

template <typename P>
void mc_avg_t(P* dst, int ds, const P* a, const P* b, int ss, int w, int h) {
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        mc_average(dst, ds, a, b, ss, w, h);
        return;
    }
    for (int y = 0; y < h; ++y) {
        const P* pa = a + y * ss;
        const P* pb = b + y * ss;
        P* d = dst + y * ds;
        for (int x = 0; x < w; ++x) d[x] = static_cast<P>((pa[x] + pb[x] + 1) >> 1);
    }
}

template <typename P>
void mc_weight_uni_t(P* dst, int ds, const P* src, int ss, int w, int h, int log_wd, int weight, int offset, int bit_depth) {
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        if (bit_depth == 8) {
            mc_weight_uni(dst, ds, src, ss, w, h, log_wd, weight, offset);
            return;
        }
    }
    int maxv = (1 << bit_depth) - 1;
    for (int y = 0; y < h; ++y) {
        const P* s = src + y * ss;
        P* d = dst + y * ds;
        if (log_wd >= 1) {
            int round = 1 << (log_wd - 1);
            for (int x = 0; x < w; ++x) d[x] = static_cast<P>(clip_bd(((s[x] * weight + round) >> log_wd) + offset, maxv));
        } else {
            for (int x = 0; x < w; ++x) d[x] = static_cast<P>(clip_bd(s[x] * weight + offset, maxv));
        }
    }
}

template <typename P>
void mc_weight_bi_t(P* dst, int ds, const P* a, const P* b, int ss, int w, int h, int log_wd, int w0, int w1, int offset, int bit_depth) {
    if (log_wd == 5 && w0 == 32 && w1 == 32 && offset == 0) {
        mc_avg_t<P>(dst, ds, a, b, ss, w, h);
        return;
    }
    if constexpr (std::is_same<P, std::uint8_t>::value) {
        if (bit_depth == 8) {
            mc_weight_bi(dst, ds, a, b, ss, w, h, log_wd, w0, w1, offset);
            return;
        }
    }
    int maxv = (1 << bit_depth) - 1;
    int round = 1 << log_wd;
    int shift = log_wd + 1;
    for (int y = 0; y < h; ++y) {
        const P* pa = a + y * ss;
        const P* pb = b + y * ss;
        P* d = dst + y * ds;
        for (int x = 0; x < w; ++x) d[x] = static_cast<P>(clip_bd(((pa[x] * w0 + pb[x] * w1 + round) >> shift) + offset, maxv));
    }
}

template <typename P>
void deblock_line_luma(P* pix, int xs, int bs, int alpha, int beta, int tc0, bool chroma_style, int bit_depth) noexcept {
    if (bs <= 0) return;
    int p0 = pix[-xs];
    int q0 = pix[0];
    int p1 = pix[-2 * xs];
    int q1 = pix[xs];
    if (std::abs(p0 - q0) >= alpha || std::abs(p1 - p0) >= beta || std::abs(q1 - q0) >= beta) return;
    int maxv = (1 << bit_depth) - 1;
    if (chroma_style) {
        if (bs < 4) {
            int tc = tc0 + 1;
            int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
            pix[-xs] = static_cast<P>(clip_bd(p0 + delta, maxv));
            pix[0] = static_cast<P>(clip_bd(q0 - delta, maxv));
        } else {
            pix[-xs] = static_cast<P>((2 * p1 + p0 + q1 + 2) >> 2);
            pix[0] = static_cast<P>((2 * q1 + q0 + p1 + 2) >> 2);
        }
        return;
    }
    int p2 = pix[-3 * xs];
    int q2 = pix[2 * xs];
    int ap = std::abs(p2 - p0);
    int aq = std::abs(q2 - q0);
    if (bs < 4) {
        int tc = tc0 + (ap < beta ? 1 : 0) + (aq < beta ? 1 : 0);
        int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
        pix[-xs] = static_cast<P>(clip_bd(p0 + delta, maxv));
        pix[0] = static_cast<P>(clip_bd(q0 - delta, maxv));
        if (ap < beta) pix[-2 * xs] = static_cast<P>(p1 + clip3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 * 2)) >> 1));
        if (aq < beta) pix[xs] = static_cast<P>(q1 + clip3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 * 2)) >> 1));
        return;
    }
    int p3 = pix[-4 * xs];
    int q3 = pix[3 * xs];
    bool strong = std::abs(p0 - q0) < ((alpha >> 2) + 2);
    if (ap < beta && strong) {
        pix[-xs] = static_cast<P>((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
        pix[-2 * xs] = static_cast<P>((p2 + p1 + p0 + q0 + 2) >> 2);
        pix[-3 * xs] = static_cast<P>((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
    } else {
        pix[-xs] = static_cast<P>((2 * p1 + p0 + q1 + 2) >> 2);
    }
    if (aq < beta && strong) {
        pix[0] = static_cast<P>((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3);
        pix[xs] = static_cast<P>((p0 + q0 + q1 + q2 + 2) >> 2);
        pix[2 * xs] = static_cast<P>((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
    } else {
        pix[0] = static_cast<P>((2 * q1 + q0 + p1 + 2) >> 2);
    }
}

#define FIZMO_H264_INSTANTIATE(P) \
    template void intra_pred_nxn<P>(P*, int, int, int, const IntraEdge&, int); \
    template void intra_pred_16x16<P>(P*, int, int, const IntraEdge&, int); \
    template void intra_pred_chroma_block<P>(P*, int, int, int, int, const IntraEdge&, int); \
    template void idct4x4_add_t<P>(P*, int, std::int32_t*, int) noexcept; \
    template void idct8x8_add_t<P>(P*, int, std::int32_t*, int) noexcept; \
    template void residual_add_t<P>(P*, int, const std::int32_t*, int, int, int, int) noexcept; \
    template void mc_luma_t<P>(P*, int, const PicView&, int, int, int, int, int, int, int, int); \
    template void mc_chroma_t<P>(P*, int, const PicView&, int, int, int, int, int, int, int); \
    template void mc_avg_t<P>(P*, int, const P*, const P*, int, int, int); \
    template void mc_weight_uni_t<P>(P*, int, const P*, int, int, int, int, int, int, int); \
    template void mc_weight_bi_t<P>(P*, int, const P*, const P*, int, int, int, int, int, int, int, int); \
    template void deblock_line_luma<P>(P*, int, int, int, int, int, bool, int) noexcept;

FIZMO_H264_INSTANTIATE(std::uint8_t)
FIZMO_H264_INSTANTIATE(std::uint16_t)

#undef FIZMO_H264_INSTANTIATE

} // namespace h264_internal
} // namespace video
} // namespace fizmo
