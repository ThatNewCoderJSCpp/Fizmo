#include "hevc_internal.hpp"

#include <cstdlib>
#include <cstring>

namespace fizmo {
namespace video {
namespace hevc_internal {

void PicDecoder::intra_predict(int cidx, int x, int y, int log2, int mode) {
    const int n = 1 << log2;
    const int n2 = n * 2;
    const int sx = cidx ? m_sx : 0;
    const int sy = cidx ? m_sy : 0;
    const int bd = cidx ? m_bd_c : m_bd_y;
    std::uint16_t* plane = m_pic->plane[cidx];
    const std::ptrdiff_t stride = m_pic->stride[cidx];
    const int xl = x << sx;
    const int yl = y << sy;
    const bool cip = m_pps->constrained_intra_pred;
    int left[129];
    int top[129];
    bool lav[129];
    bool tav[129];
    int any = 0;
    const int ux = 4 >> sx;
    const int uy = 4 >> sy;
    auto ok = [&](int xn, int yn) -> bool {
        if (!avail(xl, yl, xn, yn)) return false;
        if (cip && !(m_cu_flags[idx4(xn, yn)] & CU_INTRA)) return false;
        return true;
    };
    {
        const bool a = ok(xl - 1, yl - 1);
        lav[0] = tav[0] = a;
        if (a) {
            left[0] = top[0] = plane[(y - 1) * stride + x - 1];
            ++any;
        }
    }
    for (int i = 0; i < n2; i += uy) {
        const bool a = ok(xl - 1, (y + i) << sy);
        for (int k = 0; k < uy && i + k < n2; ++k) {
            lav[1 + i + k] = a;
            if (a) left[1 + i + k] = plane[(y + i + k) * stride + x - 1];
        }
        if (a) ++any;
    }
    for (int i = 0; i < n2; i += ux) {
        const bool a = ok((x + i) << sx, yl - 1);
        for (int k = 0; k < ux && i + k < n2; ++k) {
            tav[1 + i + k] = a;
            if (a) top[1 + i + k] = plane[(y - 1) * stride + x + i + k];
        }
        if (a) ++any;
    }
    if (!any) {
        const int v = 1 << (bd - 1);
        for (int i = 0; i <= n2; ++i) {
            left[i] = v;
            top[i] = v;
        }
    } else {
        if (!lav[n2]) {
            int v = -1;
            for (int i = n2; i >= 0 && v < 0; --i) {
                if (lav[i]) v = left[i];
            }
            for (int i = 1; i <= n2 && v < 0; ++i) {
                if (tav[i]) v = top[i];
            }
            left[n2] = v;
            lav[n2] = true;
        }
        for (int i = n2 - 1; i >= 0; --i) {
            if (!lav[i]) {
                left[i] = left[i + 1];
                lav[i] = true;
            }
        }
        top[0] = left[0];
        for (int i = 1; i <= n2; ++i) {
            if (!tav[i]) top[i] = top[i - 1];
        }
    }
    bool filter = false;
    if (!m_sps->intra_smoothing_disabled && (cidx == 0 || m_chroma == 3) && mode != 1 && n != 4) {
        const int d1 = std::abs(mode - 26);
        const int d2 = std::abs(mode - 10);
        const int md = d1 < d2 ? d1 : d2;
        const int thr = n == 8 ? 7 : (n == 16 ? 1 : 0);
        filter = md > thr;
    }
    if (filter) {
        int fl[129];
        int ft[129];
        const int thr = 1 << (bd - 5);
        if (m_sps->strong_intra_smoothing && cidx == 0 && n == 32 &&
            std::abs(left[0] + top[n2] - 2 * top[n]) < thr && std::abs(left[0] + left[n2] - 2 * left[n]) < thr) {
            fl[0] = ft[0] = left[0];
            for (int i = 0; i < 63; ++i) {
                fl[1 + i] = ((63 - i) * left[0] + (i + 1) * left[64] + 32) >> 6;
                ft[1 + i] = ((63 - i) * top[0] + (i + 1) * top[64] + 32) >> 6;
            }
            fl[64] = left[64];
            ft[64] = top[64];
        } else {
            fl[0] = ft[0] = (left[1] + 2 * left[0] + top[1] + 2) >> 2;
            for (int i = 1; i < n2; ++i) {
                fl[i] = (left[i + 1] + 2 * left[i] + left[i - 1] + 2) >> 2;
                ft[i] = (top[i + 1] + 2 * top[i] + top[i - 1] + 2) >> 2;
            }
            fl[n2] = left[n2];
            ft[n2] = top[n2];
        }
        std::memcpy(left, fl, sizeof(int) * static_cast<std::size_t>(n2 + 1));
        std::memcpy(top, ft, sizeof(int) * static_cast<std::size_t>(n2 + 1));
    }
    const int maxv = (1 << bd) - 1;
    const bool edge = cidx == 0 && n < 32 && !(m_sps->implicit_rdpcm && m_cu_bypass);
    std::uint16_t* dst = plane + y * stride + x;
    if (mode == 0) {
        const int sh = log2 + 1;
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) {
                dst[j * stride + i] = static_cast<std::uint16_t>(((n - 1 - i) * left[1 + j] + (i + 1) * top[1 + n] + (n - 1 - j) * top[1 + i] + (j + 1) * left[1 + n] + n) >> sh);
            }
        }
        return;
    }
    if (mode == 1) {
        int sum = n;
        for (int i = 0; i < n; ++i) sum += top[1 + i] + left[1 + i];
        const int dc = sum >> (log2 + 1);
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) dst[j * stride + i] = static_cast<std::uint16_t>(dc);
        }
        if (edge) {
            dst[0] = static_cast<std::uint16_t>((left[1] + 2 * dc + top[1] + 2) >> 2);
            for (int i = 1; i < n; ++i) dst[i] = static_cast<std::uint16_t>((top[1 + i] + 3 * dc + 2) >> 2);
            for (int j = 1; j < n; ++j) dst[j * stride] = static_cast<std::uint16_t>((left[1 + j] + 3 * dc + 2) >> 2);
        }
        return;
    }
    const int angle = k_intra_pred_angle[mode];
    int refbuf[3 * 64 + 2];
    int* ref = refbuf + 64;
    if (mode >= 18) {
        for (int i = 0; i <= n; ++i) ref[i] = top[i];
        if (angle < 0) {
            const int last = (n * angle) >> 5;
            if (last < -1) {
                const int inv = k_inv_angle[mode];
                for (int i = last; i <= -1; ++i) ref[i] = left[((i * inv + 128) >> 8)];
            }
        } else {
            for (int i = n + 1; i <= 2 * n; ++i) ref[i] = top[i];
        }
        for (int j = 0; j < n; ++j) {
            const int pos = (j + 1) * angle;
            const int idx = pos >> 5;
            const int fact = pos & 31;
            std::uint16_t* row = dst + j * stride;
            if (fact) {
                for (int i = 0; i < n; ++i) row[i] = static_cast<std::uint16_t>(((32 - fact) * ref[i + idx + 1] + fact * ref[i + idx + 2] + 16) >> 5);
            } else {
                for (int i = 0; i < n; ++i) row[i] = static_cast<std::uint16_t>(ref[i + idx + 1]);
            }
        }
        if (mode == 26 && edge) {
            for (int j = 0; j < n; ++j) {
                const int v = top[1] + ((left[1 + j] - left[0]) >> 1);
                dst[j * stride] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
            }
        }
    } else {
        for (int i = 0; i <= n; ++i) ref[i] = left[i];
        if (angle < 0) {
            const int last = (n * angle) >> 5;
            if (last < -1) {
                const int inv = k_inv_angle[mode];
                for (int i = last; i <= -1; ++i) ref[i] = top[((i * inv + 128) >> 8)];
            }
        } else {
            for (int i = n + 1; i <= 2 * n; ++i) ref[i] = left[i];
        }
        for (int i = 0; i < n; ++i) {
            const int pos = (i + 1) * angle;
            const int idx = pos >> 5;
            const int fact = pos & 31;
            if (fact) {
                for (int j = 0; j < n; ++j) dst[j * stride + i] = static_cast<std::uint16_t>(((32 - fact) * ref[j + idx + 1] + fact * ref[j + idx + 2] + 16) >> 5);
            } else {
                for (int j = 0; j < n; ++j) dst[j * stride + i] = static_cast<std::uint16_t>(ref[j + idx + 1]);
            }
        }
        if (mode == 10 && edge) {
            for (int i = 0; i < n; ++i) {
                const int v = left[1] + ((top[1 + i] - top[0]) >> 1);
                dst[i] = static_cast<std::uint16_t>(v < 0 ? 0 : (v > maxv ? maxv : v));
            }
        }
    }
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
