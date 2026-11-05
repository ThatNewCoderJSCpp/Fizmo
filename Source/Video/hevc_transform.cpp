#include "hevc_internal.hpp"

#include <cstring>

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

inline std::int32_t clip16(std::int64_t v) noexcept {
    return static_cast<std::int32_t>(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
}

struct DctTables {
    std::int32_t m4[2][2];
    std::int32_t m8[4][4];
    std::int32_t m16[8][8];
    std::int32_t m32[16][16];
};

const DctTables& dct_tables() {
    static const DctTables t = [] {
        DctTables d;
        const ScanTables& st = scan_tables();
        for (int k = 0; k < 2; ++k) {
            for (int i = 0; i < 2; ++i) d.m4[k][i] = st.dct[(2 * k + 1) * 8][i];
        }
        for (int k = 0; k < 4; ++k) {
            for (int i = 0; i < 4; ++i) d.m8[k][i] = st.dct[(2 * k + 1) * 4][i];
        }
        for (int k = 0; k < 8; ++k) {
            for (int i = 0; i < 8; ++i) d.m16[k][i] = st.dct[(2 * k + 1) * 2][i];
        }
        for (int k = 0; k < 16; ++k) {
            for (int i = 0; i < 16; ++i) d.m32[k][i] = st.dct[2 * k + 1][i];
        }
        return d;
    }();
    return t;
}

template <int N>
struct Idct {
    static void run(const std::int32_t* c, std::ptrdiff_t cs, std::int32_t* x, int nz) {
        const int h = N / 2;
        std::int32_t ce[N / 2];
        for (int m = 0; m < h; ++m) ce[m] = 2 * m < nz ? c[2 * m * cs] : 0;
        std::int32_t e[N / 2];
        Idct<N / 2>::run(ce, 1, e, (nz + 1) / 2);
        std::int32_t o[N / 2];
        for (int i = 0; i < h; ++i) o[i] = 0;
        const DctTables& t = dct_tables();
        const std::int32_t (*m)[N / 2] = matrix(t);
        for (int k = 0; 2 * k + 1 < nz; ++k) {
            const std::int32_t v = c[(2 * k + 1) * cs];
            if (!v) continue;
            const std::int32_t* row = m[k];
            for (int i = 0; i < h; ++i) o[i] += v * row[i];
        }
        for (int i = 0; i < h; ++i) {
            x[i] = e[i] + o[i];
            x[N - 1 - i] = e[i] - o[i];
        }
    }
    static const std::int32_t (*matrix(const DctTables& t))[N / 2];
};

template <>
const std::int32_t (*Idct<8>::matrix(const DctTables& t))[4] { return t.m8; }
template <>
const std::int32_t (*Idct<16>::matrix(const DctTables& t))[8] { return t.m16; }
template <>
const std::int32_t (*Idct<32>::matrix(const DctTables& t))[16] { return t.m32; }

template <>
struct Idct<4> {
    static void run(const std::int32_t* c, std::ptrdiff_t cs, std::int32_t* x, int nz) {
        const std::int32_t c0 = c[0];
        const std::int32_t c1 = nz > 1 ? c[cs] : 0;
        const std::int32_t c2 = nz > 2 ? c[2 * cs] : 0;
        const std::int32_t c3 = nz > 3 ? c[3 * cs] : 0;
        const std::int32_t e0 = 64 * (c0 + c2);
        const std::int32_t e1 = 64 * (c0 - c2);
        const std::int32_t o0 = 83 * c1 + 36 * c3;
        const std::int32_t o1 = 36 * c1 - 83 * c3;
        x[0] = e0 + o0;
        x[1] = e1 + o1;
        x[2] = e1 - o1;
        x[3] = e0 - o0;
    }
};

template <>
struct Idct<2> {
    static void run(const std::int32_t* c, std::ptrdiff_t cs, std::int32_t* x, int nz) {
        (void)c;
        (void)cs;
        (void)x;
        (void)nz;
    }
};

template <int N>
void idct2d(std::int32_t* coeff, std::int32_t* tmp, std::int32_t* out, int max_x, int max_y, int shift2) {
    std::int32_t col[N];
    for (int x = 0; x <= max_x; ++x) {
        Idct<N>::run(coeff + x, N, col, max_y + 1);
        for (int i = 0; i < N; ++i) {
            const std::int32_t v = (col[i] + 64) >> 7;
            tmp[i * N + x] = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        }
    }
    const std::int32_t rnd = 1 << (shift2 - 1);
    for (int y = 0; y < N; ++y) {
        std::int32_t* o = out + y * N;
        Idct<N>::run(tmp + y * N, 1, o, max_x + 1);
        for (int i = 0; i < N; ++i) o[i] = (o[i] + rnd) >> shift2;
    }
}

} // namespace

namespace {

void inv_dst_1d(const std::int32_t* src, std::ptrdiff_t ss, std::int32_t* dst, std::ptrdiff_t ds, int shift, bool clip) {
    const int add = 1 << (shift - 1);
    for (int i = 0; i < 4; ++i) {
        std::int64_t s = 0;
        for (int k = 0; k < 4; ++k) s += static_cast<std::int64_t>(k_dst4[k][i]) * src[k * ss];
        const std::int64_t v = (s + add) >> shift;
        dst[i * ds] = clip ? clip16(v) : static_cast<std::int32_t>(v);
    }
}

void rdpcm(std::int32_t* r, int n, int dir) {
    if (dir == 0) {
        for (int y = 0; y < n; ++y) {
            for (int x = 1; x < n; ++x) r[y * n + x] += r[y * n + x - 1];
        }
    } else {
        for (int y = 1; y < n; ++y) {
            for (int x = 0; x < n; ++x) r[y * n + x] += r[(y - 1) * n + x];
        }
    }
}

} // namespace

void PicDecoder::transform_residual(int cidx, int log2, bool ts, bool intra, int mode, std::int32_t* coeff, int max_x, int max_y, std::int32_t* out) {
    const int n = 1 << log2;
    const int nn = n * n;
    const int bd = cidx ? m_bd_c : m_bd_y;
    const bool rotate = m_sps->transform_skip_rotation && n == 4 && intra;
    int dir = -1;
    if (intra && m_sps->implicit_rdpcm && (ts || m_cu_bypass) && (mode == 10 || mode == 26)) dir = mode == 26 ? 1 : 0;
    if (!intra && m_rdpcm_explicit) dir = m_rdpcm_dir;
    if (m_cu_bypass) {
        for (int i = 0; i < nn; ++i) out[i] = rotate ? coeff[nn - 1 - i] : coeff[i];
        if (dir >= 0) rdpcm(out, n, dir);
        return;
    }
    const int qp = cidx == 0 ? m_qp_y + m_sps->qp_bd_offset_y : chroma_qp(cidx);
    const std::uint8_t* factor = nullptr;
    if (m_sps->scaling_list_enabled && !(ts && n > 4)) {
        const ScalingList* sl = m_pps->scaling_list_present && m_pps->scaling ? m_pps->scaling.get() : m_sps->scaling.get();
        if (sl) factor = sl->factor[log2 - 2][(intra ? 0 : 3) + cidx];
    }
    const int bd_shift = bd + log2 - 5;
    const std::int64_t scale = static_cast<std::int64_t>(k_level_scale[qp % 6]) << (qp / 6);
    const std::int64_t add = static_cast<std::int64_t>(1) << (bd_shift - 1);
    for (int y = 0; y <= max_y; ++y) {
        std::int32_t* row = coeff + y * n;
        for (int x = 0; x <= max_x; ++x) {
            const std::int32_t c = row[x];
            if (!c) continue;
            const std::int64_t m = factor ? factor[y * n + x] : 16;
            row[x] = clip16((c * m * scale + add) >> bd_shift);
        }
    }
    const int shift2 = 20 - bd;
    if (ts) {
        const int ts_shift = 5 + log2;
        const std::int64_t rnd = static_cast<std::int64_t>(1) << (shift2 - 1);
        for (int i = 0; i < nn; ++i) {
            const std::int64_t d = rotate ? coeff[nn - 1 - i] : coeff[i];
            out[i] = static_cast<std::int32_t>((d * (static_cast<std::int64_t>(1) << ts_shift) + rnd) >> shift2);
        }
        if (dir >= 0) rdpcm(out, n, dir);
        return;
    }
    std::int32_t* tmp = m_tmp;
    if (intra && n == 4 && cidx == 0) {
        for (int x = 0; x < 4; ++x) inv_dst_1d(coeff + x, 4, tmp + x, 4, 7, true);
        for (int y = 0; y < 4; ++y) inv_dst_1d(tmp + y * 4, 1, out + y * 4, 1, shift2, false);
        return;
    }
    if (max_x == 0 && max_y == 0) {
        const std::int32_t g = clip16((static_cast<std::int64_t>(64) * coeff[0] + 64) >> 7);
        const std::int32_t v = static_cast<std::int32_t>((static_cast<std::int64_t>(64) * g + (static_cast<std::int64_t>(1) << (shift2 - 1))) >> shift2);
        for (int i = 0; i < nn; ++i) out[i] = v;
        return;
    }
    switch (n) {
    case 4: idct2d<4>(coeff, tmp, out, max_x, max_y, shift2); break;
    case 8: idct2d<8>(coeff, tmp, out, max_x, max_y, shift2); break;
    case 16: idct2d<16>(coeff, tmp, out, max_x, max_y, shift2); break;
    default: idct2d<32>(coeff, tmp, out, max_x, max_y, shift2); break;
    }
}

void PicDecoder::reconstruct(int cidx, int x, int y, int log2, const std::int32_t* res) {
    const int n = 1 << log2;
    const int bd = cidx ? m_bd_c : m_bd_y;
    const int maxv = (1 << bd) - 1;
    std::uint16_t* p = m_pic->plane[cidx] + y * m_pic->stride[cidx] + x;
    const std::ptrdiff_t s = m_pic->stride[cidx];
    for (int j = 0; j < n; ++j) {
        std::uint16_t* row = p + j * s;
        const std::int32_t* r = res + j * n;
        for (int i = 0; i < n; ++i) {
            const int v = row[i] + r[i];
            row[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
        }
    }
}

void PicDecoder::cross_component(int log2, int res_scale, std::int32_t* res_c, const std::int32_t* res_y) {
    const int nn = 1 << (2 * log2);
    for (int i = 0; i < nn; ++i) {
        const std::int64_t ry = (static_cast<std::int64_t>(res_y[i]) * (static_cast<std::int64_t>(1) << m_bd_c)) >> m_bd_y;
        res_c[i] += static_cast<std::int32_t>((res_scale * ry) >> 3);
    }
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
