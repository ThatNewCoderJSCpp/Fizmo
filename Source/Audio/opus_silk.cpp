#include "opus_internal.hpp"
#include "opus_tables.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {
namespace opus_detail {

namespace {

const int k_history = 320;
const int k_max_frame = 320;

int sign_of(int v) noexcept {
    return v > 0 ? 1 : (v < 0 ? -1 : 0);
}

int clampi(int lo, int v, int hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

float clampf(float v) noexcept {
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

std::int32_t log2lin(std::int32_t in_log_q7) noexcept {
    if (in_log_q7 < 0) return 0;
    if (in_log_q7 >= 3967) return 0x7FFFFFFF;
    const std::int32_t i = in_log_q7 >> 7;
    const std::int32_t f = in_log_q7 & 127;
    const std::int32_t base = std::int32_t(1) << i;
    return base + ((((-174 * f * (128 - f)) >> 16) + f) * (base >> 7));
}

void stabilize(std::int32_t* nlsf, const std::int16_t* delta_min, int d) noexcept {
    for (int round = 0; round < 20; ++round) {
        int best = 0;
        std::int32_t best_v = 0x7FFFFFFF;
        for (int i = 0; i <= d; ++i) {
            const std::int32_t lo = i == 0 ? 0 : nlsf[i - 1];
            const std::int32_t hi = i == d ? 32768 : nlsf[i];
            const std::int32_t v = hi - lo - delta_min[i];
            if (v < best_v) {
                best_v = v;
                best = i;
            }
        }
        if (best_v >= 0) return;
        if (best == 0) {
            nlsf[0] = delta_min[0];
        } else if (best == d) {
            nlsf[d - 1] = 32768 - delta_min[d];
        } else {
            std::int32_t min_center = delta_min[best] >> 1;
            for (int k = 0; k < best; ++k) min_center += delta_min[k];
            std::int32_t max_center = 32768 - (delta_min[best] >> 1);
            for (int k = best + 1; k <= d; ++k) max_center -= delta_min[k];
            std::int32_t center = (nlsf[best - 1] + nlsf[best] + 1) >> 1;
            center = center < min_center ? min_center : (center > max_center ? max_center : center);
            nlsf[best - 1] = center - (delta_min[best] >> 1);
            nlsf[best] = nlsf[best - 1] + delta_min[best];
        }
    }
    std::sort(nlsf, nlsf + d);
    for (int k = 0; k < d; ++k) {
        const std::int32_t prev = k == 0 ? 0 : nlsf[k - 1];
        std::int32_t lim = prev + delta_min[k];
        if (lim > 32767) lim = 32767;
        if (nlsf[k] < lim) nlsf[k] = lim;
    }
    for (int k = d - 1; k >= 0; --k) {
        const std::int32_t next = k == d - 1 ? 32768 : nlsf[k + 1];
        const std::int32_t lim = next - delta_min[k + 1];
        if (nlsf[k] > lim) nlsf[k] = lim;
    }
}

void bandwidth_expand(std::int64_t* a, int d, std::int64_t sc0) noexcept {
    std::int64_t sc = sc0;
    for (int k = 0; k < d; ++k) {
        a[k] = (a[k] * sc) >> 16;
        sc = (sc0 * sc + 32768) >> 16;
    }
}

bool fits32(std::int64_t v) noexcept {
    return v >= -2147483648LL && v <= 2147483647LL;
}

bool lpc_stable(const std::int32_t* a_q12, int d) noexcept {
    std::int64_t dc = 0;
    for (int n = 0; n < d; ++n) dc += a_q12[n];
    if (dc > 4096) return false;
    std::int64_t a[16];
    std::int64_t b[16];
    for (int n = 0; n < d; ++n) a[n] = static_cast<std::int64_t>(a_q12[n]) * 4096;
    std::int64_t inv_gain = std::int64_t(1) << 30;
    for (int k = d - 1; k >= 0; --k) {
        if (a[k] > 16773022 || a[k] < -16773022) return false;
        const std::int64_t rc = -a[k] * 128;
        const std::int64_t div = (std::int64_t(1) << 30) - ((rc * rc) >> 32);
        inv_gain = ((inv_gain * div) >> 32) * 4;
        if (inv_gain < 107374) return false;
        if (k > 0) {
            const int b1 = ilog(static_cast<std::uint32_t>(div));
            const int b2 = b1 - 16;
            const std::int64_t inv = ((std::int64_t(1) << 29) - 1) / (div >> (b2 + 1));
            const std::int64_t err = (std::int64_t(1) << 29) - (((div << (15 - b2)) * inv) >> 16);
            const std::int64_t gain = (inv << 16) + ((err * inv) >> 13);
            for (int n = 0; n < k; ++n) {
                const std::int64_t num = a[n] - ((a[k - n - 1] * rc + (std::int64_t(1) << 30)) >> 31);
                if (!fits32(num)) return false;
                const std::int64_t v = (num * gain + (std::int64_t(1) << (b1 - 1))) >> b1;
                if (!fits32(v)) return false;
                b[n] = v;
            }
            for (int n = 0; n < k; ++n) a[n] = b[n];
        }
    }
    return true;
}

void nlsf_to_lpc(const std::int32_t* nlsf, int d, bool wb, float* out) noexcept {
    const std::uint8_t* order = wb ? k_silk_lsf_order_wb : k_silk_lsf_order_nb;
    std::int64_t c[16] = {};
    for (int k = 0; k < d; ++k) {
        const std::int32_t i = nlsf[k] >> 8;
        const std::int32_t f = nlsf[k] & 255;
        c[order[k]] = (static_cast<std::int64_t>(k_silk_cos_q12[i]) * 256 + (k_silk_cos_q12[i + 1] - k_silk_cos_q12[i]) * f + 4) >> 3;
    }
    const int d2 = d / 2;
    std::int64_t p[10];
    std::int64_t q[10];
    std::int64_t pn[10];
    std::int64_t qn[10];
    p[0] = 65536;
    q[0] = 65536;
    p[1] = -c[0];
    q[1] = -c[1];
    for (int k = 1; k < d2; ++k) {
        for (int j = 0; j <= k + 1; ++j) {
            const std::int64_t pj = j <= k ? p[j] : p[k - 1];
            const std::int64_t qj = j <= k ? q[j] : q[k - 1];
            const std::int64_t pj2 = j >= 2 ? p[j - 2] : 0;
            const std::int64_t qj2 = j >= 2 ? q[j - 2] : 0;
            const std::int64_t pj1 = j >= 1 ? p[j - 1] : 0;
            const std::int64_t qj1 = j >= 1 ? q[j - 1] : 0;
            pn[j] = pj + pj2 - ((c[2 * k] * pj1 + 32768) >> 16);
            qn[j] = qj + qj2 - ((c[2 * k + 1] * qj1 + 32768) >> 16);
        }
        for (int j = 0; j <= k + 1; ++j) {
            p[j] = pn[j];
            q[j] = qn[j];
        }
    }
    std::int64_t a[16];
    for (int k = 0; k < d2; ++k) {
        const std::int64_t qd = q[k + 1] - q[k];
        const std::int64_t ps = p[k + 1] + p[k];
        a[k] = -qd - ps;
        a[d - k - 1] = qd - ps;
    }
    bool saturate = true;
    for (int round = 0; round < 10; ++round) {
        int best = 0;
        std::int64_t maxabs = -1;
        for (int k = 0; k < d; ++k) {
            const std::int64_t v = a[k] < 0 ? -a[k] : a[k];
            if (v > maxabs) {
                maxabs = v;
                best = k;
            }
        }
        std::int64_t m12 = (maxabs + 16) >> 5;
        if (m12 > 163838) m12 = 163838;
        if (m12 <= 32767) {
            saturate = false;
            break;
        }
        const std::int64_t sc0 = 65470 - ((m12 - 32767) << 14) / ((m12 * (best + 1)) >> 2);
        bandwidth_expand(a, d, sc0);
    }
    if (saturate) {
        for (int k = 0; k < d; ++k) {
            std::int64_t v = (a[k] + 16) >> 5;
            v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
            a[k] = v * 32;
        }
    }
    std::int32_t a12[16];
    for (int round = 0; round < 16; ++round) {
        for (int k = 0; k < d; ++k) a12[k] = static_cast<std::int32_t>((a[k] + 16) >> 5);
        if (lpc_stable(a12, d)) break;
        bandwidth_expand(a, d, 65536 - (std::int64_t(2) << round));
        if (round == 15) {
            for (int k = 0; k < d; ++k) a12[k] = static_cast<std::int32_t>((a[k] + 16) >> 5);
        }
    }
    for (int k = 0; k < d; ++k) out[k] = static_cast<float>(a12[k]) / 4096.0f;
}

void shell_split(RangeDecoder& rd, int size, int count, int* out) noexcept {
    if (count == 0) {
        for (int i = 0; i < size; ++i) out[i] = 0;
        return;
    }
    if (size == 1) {
        out[0] = count;
        return;
    }
    const std::uint8_t* table;
    switch (size) {
        case 16: table = k_silk_shell_16[count - 1]; break;
        case 8: table = k_silk_shell_8[count - 1]; break;
        case 4: table = k_silk_shell_4[count - 1]; break;
        default: table = k_silk_shell_2[count - 1]; break;
    }
    const int left = rd.pdf(table, count + 1, 8);
    shell_split(rd, size / 2, left, out);
    shell_split(rd, size / 2, count - left, out + size / 2);
}

} // namespace

struct SilkDecoder::Frame {
    int signal_type = 0;
    int qoff = 0;
    int log_gain[4] = {};
    std::int32_t gain_q16[4] = {};
    int I1 = 0;
    int I2[16] = {};
    int interp = 4;
    int lag = 0;
    int pitch[4] = {};
    int periodicity = 0;
    int ltp_index[4] = {};
    int ltp_scale = 15565;
    int seed = 0;
    int subframes = 4;
    int sub_len = 40;
    int order = 10;
    bool wb = false;
    bool mid_only = false;
    int w0 = 0;
    int w1 = 0;
    std::int32_t e_q23[k_max_frame] = {};
};

SilkDecoder::SilkDecoder() {
    m_ch.resize(2);
}

void SilkDecoder::reset_channel(int c) {
    m_ch[static_cast<std::size_t>(c)] = Channel();
}

void SilkDecoder::reset() {
    reset_channel(0);
    reset_channel(1);
    m_prev_w[0] = 0;
    m_prev_w[1] = 0;
    m_mid_hist[0] = 0.0f;
    m_mid_hist[1] = 0.0f;
    m_side_hist = 0.0f;
    m_prev_stereo = false;
    m_rate = 0;
}

void SilkDecoder::parse_frame(RangeDecoder& rd, int c, int index, bool lbrr_frame, bool vad, bool independent, bool lbrr_prev_coded, Frame& f, GainContext& gc, int& lag_ctx) {
    (void)c;
    const bool wb = m_rate == 16000;
    const bool nb = m_rate == 8000;
    f.wb = wb;
    f.order = wb ? 16 : 10;
    const bool active = lbrr_frame || vad;
    const int type = active ? rd.pdf(k_silk_frame_type[1], 6, 8) : rd.pdf(k_silk_frame_type[0], 6, 8);
    f.signal_type = type >> 1;
    f.qoff = type & 1;
    for (int s = 0; s < f.subframes; ++s) {
        if (s == 0 && independent) {
            const int msb = rd.pdf(k_silk_gain_msb[f.signal_type], 8, 8);
            const int lsb = rd.pdf(k_silk_gain_lsb, 8, 8);
            const int idx = (msb << 3) | lsb;
            gc.value = gc.have ? std::max(idx, gc.value - 16) : idx;
        } else {
            const int delta = rd.pdf(k_silk_gain_delta, 41, 8);
            gc.value = clampi(0, std::max(2 * delta - 16, gc.value + delta - 4), 63);
        }
        gc.have = true;
        f.log_gain[s] = gc.value;
        f.gain_q16[s] = log2lin(((0x1D1C71 * gc.value) >> 16) + 2090);
    }
    f.I1 = rd.pdf(k_silk_lsf_stage1[(wb ? 2 : 0) + (f.signal_type == 2 ? 1 : 0)], 32, 8);
    for (int k = 0; k < f.order; ++k) {
        const int cb = wb ? k_silk_lsf_cb_wb[f.I1][k] : k_silk_lsf_cb_nb[f.I1][k];
        int v = rd.pdf(k_silk_lsf_stage2[cb], 9, 8) - 4;
        if (v == -4) {
            v -= rd.pdf(k_silk_lsf_ext, 7, 8);
        } else if (v == 4) {
            v += rd.pdf(k_silk_lsf_ext, 7, 8);
        }
        f.I2[k] = v;
    }
    f.interp = 4;
    if (f.subframes == 4) f.interp = rd.pdf(k_silk_lsf_interp, 5, 8);
    if (f.signal_type == 2) {
        int min_lag;
        int max_lag;
        int scale;
        const std::uint8_t* low;
        int low_n;
        if (nb) {
            min_lag = 16;
            max_lag = 144;
            scale = 4;
            low = k_silk_lag_low_nb;
            low_n = 4;
        } else if (wb) {
            min_lag = 32;
            max_lag = 288;
            scale = 8;
            low = k_silk_lag_low_wb;
            low_n = 8;
        } else {
            min_lag = 24;
            max_lag = 216;
            scale = 6;
            low = k_silk_lag_low_mb;
            low_n = 6;
        }
        bool absolute = true;
        if (lag_ctx >= 0) {
            const int delta = rd.pdf(k_silk_lag_delta, 21, 8);
            if (delta != 0) {
                f.lag = lag_ctx + delta - 9;
                absolute = false;
            }
        }
        if (absolute) {
            const int high = rd.pdf(k_silk_lag_high, 32, 8);
            const int lo = rd.pdf(low, low_n, 8);
            f.lag = high * scale + lo + min_lag;
        }
        lag_ctx = f.lag;
        const std::int8_t* cb;
        int cb_stride;
        if (nb) {
            if (f.subframes == 2) {
                const int idx = rd.pdf(k_silk_contour_pdf_nb10, 3, 8);
                cb = k_silk_contour_nb10[idx];
                cb_stride = 2;
            } else {
                const int idx = rd.pdf(k_silk_contour_pdf_nb20, 11, 8);
                cb = k_silk_contour_nb20[idx];
                cb_stride = 4;
            }
        } else {
            if (f.subframes == 2) {
                const int idx = rd.pdf(k_silk_contour_pdf_wb10, 12, 8);
                cb = k_silk_contour_wb10[idx];
                cb_stride = 2;
            } else {
                const int idx = rd.pdf(k_silk_contour_pdf_wb20, 34, 8);
                cb = k_silk_contour_wb20[idx];
                cb_stride = 4;
            }
        }
        (void)cb_stride;
        for (int s = 0; s < f.subframes; ++s) f.pitch[s] = clampi(min_lag, f.lag + cb[s], max_lag);
        f.periodicity = rd.pdf(k_silk_periodicity, 3, 8);
        for (int s = 0; s < f.subframes; ++s) {
            if (f.periodicity == 0) {
                f.ltp_index[s] = rd.pdf(k_silk_ltp_pdf0, 8, 8);
            } else if (f.periodicity == 1) {
                f.ltp_index[s] = rd.pdf(k_silk_ltp_pdf1, 16, 8);
            } else {
                f.ltp_index[s] = rd.pdf(k_silk_ltp_pdf2, 32, 8);
            }
        }
        f.ltp_scale = 15565;
        if (index == 0 || (lbrr_frame && !lbrr_prev_coded)) {
            static const int scales[3] = { 15565, 12288, 8192 };
            f.ltp_scale = scales[rd.pdf(k_silk_ltp_scale, 3, 8)];
        }
    } else {
        lag_ctx = -1;
    }
    f.seed = rd.pdf(k_silk_seed, 4, 8);
    int blocks;
    if (nb) {
        blocks = f.subframes == 2 ? 5 : 10;
    } else if (wb) {
        blocks = f.subframes == 2 ? 10 : 20;
    } else {
        blocks = f.subframes == 2 ? 8 : 15;
    }
    const int rate_level = rd.pdf(k_silk_rate_level[f.signal_type == 2 ? 1 : 0], 9, 8);
    int counts[20];
    int lsbs[20];
    for (int b = 0; b < blocks; ++b) {
        int level = rate_level;
        int extra = 0;
        int v = rd.pdf(k_silk_pulse_count[level], 18, 8);
        while (v == 17) {
            ++extra;
            level = extra >= 10 ? 10 : 9;
            v = rd.pdf(k_silk_pulse_count[level], 18, 8);
        }
        counts[b] = v;
        lsbs[b] = extra;
    }
    int mag[k_max_frame];
    for (int b = 0; b < blocks; ++b) shell_split(rd, 16, counts[b], mag + 16 * b);
    for (int b = 0; b < blocks; ++b) {
        if (lsbs[b] == 0) continue;
        for (int i = 0; i < 16; ++i) {
            int m = mag[16 * b + i];
            for (int l = 0; l < lsbs[b]; ++l) m = 2 * m + rd.pdf(k_silk_lsb, 2, 8);
            mag[16 * b + i] = m;
        }
    }
    for (int b = 0; b < blocks; ++b) {
        const std::uint8_t* table = k_silk_sign[(f.signal_type * 2 + f.qoff) * 7 + std::min(counts[b], 6)];
        for (int i = 0; i < 16; ++i) {
            int& m = mag[16 * b + i];
            if (m != 0 && rd.pdf(table, 2, 8) == 0) m = -m;
        }
    }
    static const int offsets[3][2] = { { 25, 60 }, { 25, 60 }, { 8, 25 } };
    const int offset = offsets[f.signal_type][f.qoff];
    std::uint32_t seed = static_cast<std::uint32_t>(f.seed);
    const int total = f.subframes * f.sub_len;
    for (int i = 0; i < total; ++i) {
        const int raw = mag[i];
        std::int32_t e = raw * 256 - sign_of(raw) * 20 + offset;
        seed = 196314165u * seed + 907633515u;
        if (seed & 0x80000000u) e = -e;
        seed += static_cast<std::uint32_t>(raw);
        f.e_q23[i] = e;
    }
}

void SilkDecoder::synthesize(int c, const Frame& f, float* out) {
    Channel& ch = m_ch[static_cast<std::size_t>(c)];
    const int d = f.order;
    const std::uint8_t* cb1 = f.wb ? k_silk_lsf_cb1_wb[f.I1] : k_silk_lsf_cb1_nb[f.I1];
    const std::uint8_t* sel = f.wb ? k_silk_lsf_pred_sel_wb[f.I1] : k_silk_lsf_pred_sel_nb[f.I1];
    const int qstep = f.wb ? 9830 : 11796;
    std::int32_t res[16];
    for (int k = d - 1; k >= 0; --k) {
        std::int32_t pred = 0;
        if (k + 1 < d) {
            const int w = f.wb ? k_silk_lsf_pred_wb[sel[k]][k] : k_silk_lsf_pred_nb[sel[k]][k];
            pred = (res[k + 1] * w) >> 8;
        }
        res[k] = pred + ((((f.I2[k] * 1024) - sign_of(f.I2[k]) * 102) * qstep) >> 16);
    }
    std::int32_t nlsf[16];
    for (int k = 0; k < d; ++k) {
        const int lo = k == 0 ? 0 : cb1[k - 1];
        const int hi = k + 1 == d ? 256 : cb1[k + 1];
        const std::int32_t w2 = (1024 / (cb1[k] - lo) + 1024 / (hi - cb1[k])) << 16;
        const int i = ilog(static_cast<std::uint32_t>(w2));
        const std::int32_t fr = (w2 >> (i - 8)) & 127;
        const std::int32_t y = ((i & 1) ? 32768 : 46214) >> ((32 - i) >> 1);
        const std::int32_t wq9 = y + ((213 * fr * y) >> 16);
        const std::int32_t v = (static_cast<std::int32_t>(cb1[k]) << 7) + (res[k] * 16384) / wq9;
        nlsf[k] = clampi(0, v, 32767);
    }
    stabilize(nlsf, f.wb ? k_silk_lsf_min_wb : k_silk_lsf_min_nb, d);
    float a2[16];
    float a1[16];
    nlsf_to_lpc(nlsf, d, f.wb, a2);
    const bool interpolate = f.subframes == 4 && f.interp < 4 && ch.have_prev_lsf;
    if (interpolate) {
        std::int32_t n1[16] = {};
        for (int k = 0; k < d; ++k) n1[k] = ch.prev_nlsf[k] + ((f.interp * (nlsf[k] - ch.prev_nlsf[k])) >> 2);
        nlsf_to_lpc(n1, d, f.wb, a1);
    }
    for (int k = 0; k < d; ++k) ch.prev_nlsf[k] = nlsf[k];
    ch.have_prev_lsf = true;
    const int n = f.sub_len;
    const int total = n * f.subframes;
    float outbuf[k_history + k_max_frame];
    float lpcbuf[16 + k_max_frame];
    float resbuf[k_history + k_max_frame];
    std::memcpy(outbuf, ch.out_hist, sizeof(ch.out_hist));
    std::memcpy(lpcbuf, ch.lpc_hist, sizeof(ch.lpc_hist));
    float* o = outbuf + k_history;
    float* l = lpcbuf + 16;
    float* r = resbuf + k_history;
    for (int s = 0; s < f.subframes; ++s) {
        const float* a = (s < 2 && interpolate) ? a1 : a2;
        const int j = s * n;
        const float gain = static_cast<float>(f.gain_q16[s]) / 65536.0f;
        if (f.signal_type == 2) {
            const int lag = f.pitch[s];
            int out_end;
            float ltp_scale;
            if (s >= 2 && f.subframes == 4 && interpolate) {
                out_end = j - (s - 2) * n;
                ltp_scale = 16384.0f;
            } else {
                out_end = j - s * n;
                ltp_scale = static_cast<float>(f.ltp_scale);
            }
            const float inv_gain = f.gain_q16[s] > 0 ? 65536.0f / static_cast<float>(f.gain_q16[s]) : 0.0f;
            const int begin = j - lag - 2;
            for (int i = begin; i < out_end; ++i) {
                float acc = o[i];
                for (int k = 0; k < d; ++k) acc -= o[i - k - 1] * a[k];
                r[i] = 4.0f * ltp_scale / 65536.0f * inv_gain * clampf(acc);
            }
            for (int i = out_end; i < j; ++i) {
                float acc = l[i];
                for (int k = 0; k < d; ++k) acc -= l[i - k - 1] * a[k];
                r[i] = inv_gain * acc;
            }
            const std::int8_t* taps;
            if (f.periodicity == 0) {
                taps = k_silk_ltp_taps0[f.ltp_index[s]];
            } else if (f.periodicity == 1) {
                taps = k_silk_ltp_taps1[f.ltp_index[s]];
            } else {
                taps = k_silk_ltp_taps2[f.ltp_index[s]];
            }
            for (int i = j; i < j + n; ++i) {
                float acc = static_cast<float>(f.e_q23[i]) / 8388608.0f;
                for (int k = 0; k < 5; ++k) acc += r[i - lag + 2 - k] * static_cast<float>(taps[k]) / 128.0f;
                r[i] = acc;
            }
        } else {
            for (int i = j; i < j + n; ++i) r[i] = static_cast<float>(f.e_q23[i]) / 8388608.0f;
        }
        for (int i = j; i < j + n; ++i) {
            float acc = gain * r[i];
            for (int k = 0; k < d; ++k) acc += l[i - k - 1] * a[k];
            l[i] = acc;
            o[i] = clampf(acc);
        }
    }
    std::memcpy(out, o, sizeof(float) * static_cast<std::size_t>(total));
    std::memcpy(ch.out_hist, outbuf + total, sizeof(ch.out_hist));
    std::memcpy(ch.lpc_hist, lpcbuf + total, sizeof(ch.lpc_hist));
}

void SilkDecoder::unmix(const float* mid, const float* side, int n, int w0q, int w1q, int out_channels, bool stereo, std::vector<float> out[2], int offset) {
    const int n1 = m_rate / 125;
    const float pw0 = static_cast<float>(m_prev_w[0]) / 8192.0f;
    const float pw1 = static_cast<float>(m_prev_w[1]) / 8192.0f;
    const float dw0 = static_cast<float>(w0q - m_prev_w[0]) / (8192.0f * static_cast<float>(n1));
    const float dw1 = static_cast<float>(w1q - m_prev_w[1]) / (8192.0f * static_cast<float>(n1));
    for (int i = 0; i < n; ++i) {
        const float m0 = mid[i];
        const float m1 = i >= 1 ? mid[i - 1] : m_mid_hist[1];
        const float m2 = i >= 2 ? mid[i - 2] : (i == 1 ? m_mid_hist[1] : m_mid_hist[0]);
        const float s1 = i >= 1 ? side[i - 1] : m_side_hist;
        if (out_channels == 1) {
            out[0][static_cast<std::size_t>(offset + i)] = m1;
            continue;
        }
        if (!stereo) {
            out[0][static_cast<std::size_t>(offset + i)] = m1;
            out[1][static_cast<std::size_t>(offset + i)] = m1;
            continue;
        }
        const int t = std::min(i, n1);
        const float w0 = pw0 + static_cast<float>(t) * dw0;
        const float w1 = pw1 + static_cast<float>(t) * dw1;
        const float p0 = (m2 + 2.0f * m1 + m0) * 0.25f;
        out[0][static_cast<std::size_t>(offset + i)] = clampf((1.0f + w1) * m1 + s1 + w0 * p0);
        out[1][static_cast<std::size_t>(offset + i)] = clampf((1.0f - w1) * m1 - s1 - w0 * p0);
    }
    if (n >= 2) {
        m_mid_hist[0] = mid[n - 2];
        m_mid_hist[1] = mid[n - 1];
    } else if (n == 1) {
        m_mid_hist[0] = m_mid_hist[1];
        m_mid_hist[1] = mid[0];
    }
    m_side_hist = n >= 1 ? side[n - 1] : m_side_hist;
    m_prev_w[0] = w0q;
    m_prev_w[1] = w1q;
}

bool SilkDecoder::decode(RangeDecoder& rd, int internal_rate, int duration_ms, bool stereo, int out_channels, bool lbrr, std::vector<float> out[2], bool* lbrr_complete) {
    if (internal_rate != m_rate) {
        reset();
        m_rate = internal_rate;
    }
    if (stereo && !m_prev_stereo) {
        reset_channel(1);
        m_prev_w[0] = 0;
        m_prev_w[1] = 0;
        m_side_hist = 0.0f;
    }
    m_prev_stereo = stereo;
    const int frames = duration_ms <= 20 ? 1 : duration_ms / 20;
    const int subframes = duration_ms == 10 ? 2 : 4;
    const int sub_len = internal_rate / 200;
    const int frame_len = subframes * sub_len;
    const int nch = stereo ? 2 : 1;
    bool vad[2][3] = { { false, false, false }, { false, false, false } };
    bool lbrr_flag[2] = { false, false };
    for (int c = 0; c < nch; ++c) {
        for (int i = 0; i < frames; ++i) vad[c][i] = rd.bit_logp(1);
        lbrr_flag[c] = rd.bit_logp(1);
    }
    bool lbrr_frames[2][3] = { { false, false, false }, { false, false, false } };
    for (int c = 0; c < nch; ++c) {
        if (!lbrr_flag[c]) continue;
        if (frames == 1) {
            lbrr_frames[c][0] = true;
        } else {
            const int v = frames == 2 ? rd.pdf(k_silk_lbrr2, 4, 8) : rd.pdf(k_silk_lbrr3, 8, 8);
            for (int i = 0; i < frames; ++i) lbrr_frames[c][i] = ((v >> i) & 1) != 0;
        }
    }
    const std::size_t total = static_cast<std::size_t>(frames * frame_len);
    for (int c = 0; c < out_channels; ++c) out[c].assign(total, 0.0f);
    if (lbrr) {
        bool complete = true;
        for (int i = 0; i < frames; ++i) complete = complete && lbrr_frames[0][i];
        if (lbrr_complete) *lbrr_complete = complete;
        if (!complete) return true;
    }
    float mid[k_max_frame];
    float side[k_max_frame];
    Frame f;
    f.subframes = subframes;
    f.sub_len = sub_len;
    for (int pass = 0; pass < 2; ++pass) {
        const bool lbrr_pass = pass == 0;
        if (!lbrr_pass && lbrr) break;
        const bool apply = lbrr_pass == lbrr;
        GainContext gc[2];
        int lag_ctx[2] = { -1, -1 };
        bool prev_coded[2] = { false, false };
        for (int c = 0; c < 2; ++c) {
            gc[c].have = m_ch[static_cast<std::size_t>(c)].have_prev_gain;
            gc[c].value = m_ch[static_cast<std::size_t>(c)].prev_log_gain;
        }
        for (int i = 0; i < frames; ++i) {
            const bool mid_coded = lbrr_pass ? lbrr_frames[0][i] : true;
            bool side_coded = stereo && lbrr_pass && lbrr_frames[1][i];
            int w0 = m_prev_w[0];
            int w1 = m_prev_w[1];
            for (int c = 0; c < nch; ++c) {
                const bool coded = c == 0 ? mid_coded : side_coded;
                if (!coded) {
                    prev_coded[c] = false;
                    lag_ctx[c] = -1;
                    continue;
                }
                if (stereo && c == 0) {
                    const int n = rd.pdf(k_silk_stereo_stage1, 25, 8);
                    const int i0 = rd.pdf(k_silk_stereo_stage2, 3, 8);
                    const int i1 = rd.pdf(k_silk_stereo_stage3, 5, 8);
                    const int i2 = rd.pdf(k_silk_stereo_stage2, 3, 8);
                    const int i3 = rd.pdf(k_silk_stereo_stage3, 5, 8);
                    const int wi0 = i0 + 3 * (n / 5);
                    const int wi1 = i2 + 3 * (n % 5);
                    w1 = k_silk_stereo_weights[wi1] + (((k_silk_stereo_weights[wi1 + 1] - k_silk_stereo_weights[wi1]) * 6554) >> 16) * (2 * i3 + 1);
                    w0 = k_silk_stereo_weights[wi0] + (((k_silk_stereo_weights[wi0 + 1] - k_silk_stereo_weights[wi0]) * 6554) >> 16) * (2 * i1 + 1) - w1;
                    if (lbrr_pass) {
                        if (!lbrr_frames[1][i]) rd.pdf(k_silk_mid_only, 2, 8);
                    } else {
                        side_coded = vad[1][i] ? true : rd.pdf(k_silk_mid_only, 2, 8) == 0;
                    }
                }
                const bool independent = !prev_coded[c];
                const bool prev_lbrr_coded = i > 0 && lbrr_frames[c][i - 1];
                GainContext scratch = gc[c];
                GainContext& g = apply ? gc[c] : scratch;
                if (apply && c == 1 && !m_side_was_coded) g.have = false;
                parse_frame(rd, c, i, lbrr_pass, lbrr_pass ? true : vad[c][i], independent, prev_lbrr_coded, f, g, lag_ctx[c]);
                if (!apply) gc[c] = scratch;
                prev_coded[c] = true;
                if (apply) {
                    if (c == 1 && !m_side_was_coded) {
                        Channel& side_ch = m_ch[1];
                        std::memset(side_ch.out_hist, 0, sizeof(side_ch.out_hist));
                        std::memset(side_ch.lpc_hist, 0, sizeof(side_ch.lpc_hist));
                        side_ch.have_prev_lsf = false;
                    }
                    synthesize(c, f, c == 0 ? mid : side);
                    m_ch[static_cast<std::size_t>(c)].have_prev_gain = true;
                    m_ch[static_cast<std::size_t>(c)].prev_log_gain = gc[c].value;
                    if (c == 1) m_side_was_coded = true;
                }
            }
            if (apply) {
                if (!stereo || !side_coded) {
                    std::memset(side, 0, sizeof(float) * static_cast<std::size_t>(frame_len));
                    if (stereo) m_side_was_coded = false;
                }
                unmix(mid, side, frame_len, stereo ? w0 : 0, stereo ? w1 : 0, out_channels, stereo, out, i * frame_len);
            }
        }
    }
    if (lbrr_complete && !lbrr) *lbrr_complete = false;
    return true;
}

} // namespace opus_detail
} // namespace audio
} // namespace fizmo
