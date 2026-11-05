#include "opus_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {
namespace opus_detail {

namespace {

const double k_pi = 3.14159265358979323846;
const int k_bands = 21;
const int k_overlap = 120;
const int k_pf_history = 1100;

const std::uint8_t k_spread_icdf[4] = { 25, 23, 2, 0 };
const std::uint8_t k_trim_icdf[11] = { 126, 124, 119, 109, 87, 41, 19, 9, 4, 2, 0 };
const std::uint8_t k_tapset_icdf[3] = { 2, 1, 0 };
const std::uint8_t k_small_energy_icdf[3] = { 2, 1, 0 };
const int k_tf_table[4][8] = {
    { 0, -1, 0, -1, 0, -1, 0, -1 },
    { 0, -1, 0, -2, 1, 0, 1, -1 },
    { 0, -2, 0, -3, 2, 0, 1, -1 },
    { 0, -2, 0, -3, 3, 0, 1, -1 },
};
const int k_exp2_frac[8] = { 16384, 17866, 19483, 21247, 23170, 25267, 27554, 30048 };
const int k_order2[2] = { 1, 0 };
const int k_order4[4] = { 3, 0, 2, 1 };
const int k_order8[8] = { 7, 0, 4, 3, 6, 1, 5, 2 };
const int k_order16[16] = { 15, 0, 8, 7, 12, 3, 11, 4, 14, 1, 9, 6, 13, 2, 10, 5 };
const float k_alpha[4] = { 29440.0f / 32768.0f, 26112.0f / 32768.0f, 21248.0f / 32768.0f, 16384.0f / 32768.0f };
const float k_beta[4] = { 30147.0f / 32768.0f, 22282.0f / 32768.0f, 12124.0f / 32768.0f, 6554.0f / 32768.0f };
const float k_beta_intra = 4915.0f / 32768.0f;
const float k_taps[3][3] = {
    { 0.3066406250f, 0.2170410156f, 0.1296386719f },
    { 0.4638671875f, 0.2680664062f, 0.0f },
    { 0.7998046875f, 0.1000976562f, 0.0f },
};
const float k_deemph = 0.8500061035f;
const float k_inv_sqrt2 = 0.70710678118654752f;
const float k_state_limit = 128.0f * 32768.0f;

int width_of(int j) noexcept {
    return k_celt_edges[j + 1] - k_celt_edges[j];
}

int fmul(int a, int b) noexcept {
    return (16384 + a * b) >> 15;
}

int bcos(int x) noexcept {
    const int u = (4096 + x * x) >> 13;
    const int v = (32767 - u) + fmul(u, -7651 + fmul(u, 8277 + fmul(-626, u)));
    return 1 + v;
}

int l2tan(int s, int c) noexcept {
    const int ls = ilog(static_cast<std::uint32_t>(s));
    const int lc = ilog(static_cast<std::uint32_t>(c));
    const int sn = s << (15 - ls);
    const int cn = c << (15 - lc);
    return 2048 * (ls - lc) + fmul(sn, fmul(sn, -2597) + 7932) - fmul(cn, fmul(cn, -2597) + 7932);
}

std::uint32_t isqrt32(std::uint32_t x) noexcept {
    std::uint32_t r = static_cast<std::uint32_t>(std::sqrt(static_cast<double>(x)));
    while (static_cast<std::uint64_t>(r) * r > x) --r;
    while (static_cast<std::uint64_t>(r + 1) * (r + 1) <= x) ++r;
    return r;
}

int trunc_div(int a, int b) noexcept {
    return a / b;
}

struct UTable {
    int max_n = 177;
    int max_k = 132;
    std::vector<std::uint64_t> v;
    UTable() {
        v.assign(static_cast<std::size_t>((max_n + 1) * (max_k + 1)), 0);
        at(0, 0) = 1;
        const std::uint64_t cap = std::uint64_t(1) << 40;
        for (int n = 1; n <= max_n; ++n) {
            for (int k = 1; k <= max_k; ++k) {
                const std::uint64_t s = at(n - 1, k) + at(n, k - 1) + at(n - 1, k - 1);
                at(n, k) = s > cap ? cap : s;
            }
        }
    }
    std::uint64_t& at(int n, int k) { return v[static_cast<std::size_t>(n * (max_k + 1) + k)]; }
    std::uint64_t get(int n, int k) const {
        if (n < 0 || k < 0 || n > max_n || k > max_k) return 0;
        return v[static_cast<std::size_t>(n * (max_k + 1) + k)];
    }
};

const UTable& u_table() {
    static const UTable t;
    return t;
}

int laplace_decode(RangeDecoder& rd, int fs0, int decay) noexcept {
    const int f = static_cast<int>(rd.decode_bin(15));
    if (f < fs0) {
        rd.update(0, static_cast<std::uint32_t>(fs0), 32768);
        return 0;
    }
    int k = 1;
    int lo = fs0;
    int w = ((32768 - 32 - fs0) * (16384 - decay) >> 15) + 1;
    while (w > 1 && f >= lo + 2 * w) {
        lo += 2 * w;
        w = (((2 * w - 2) * decay) >> 15) + 1;
        ++k;
    }
    if (w <= 1) {
        w = 1;
        const int jump = (f - lo) / 2;
        k += jump;
        lo += 2 * jump;
    }
    int fl;
    int value;
    if (f < lo + w) {
        fl = lo;
        value = -k;
    } else {
        fl = lo + w;
        value = k;
    }
    const int fh = std::min(fl + w, 32768);
    rd.update(static_cast<std::uint32_t>(fl), static_cast<std::uint32_t>(fh), 32768);
    return value;
}

struct Allocation {
    int shape[k_bands] = {};
    int fine[k_bands] = {};
    int priority[k_bands] = {};
    int coded = 0;
    int intensity = 0;
    bool dual = false;
    int balance = 0;
};

void compute_allocation(RangeDecoder& rd, int start, int end, int C, int LM, bool transient, int storage, const int* cap, const int* offsets, int trim, int& anti_collapse_rsv, Allocation& out) {
    const int M = 1 << LM;
    int B = 64 * storage - static_cast<int>(rd.tell_frac()) - 1;
    const int AC = (transient && LM >= 2 && B >= 8 * (LM + 2)) ? 8 : 0;
    B -= AC;
    if (B < 0) B = 0;
    anti_collapse_rsv = AC;
    const int S = B >= 8 ? 8 : 0;
    B -= S;
    int I = 0;
    int D = 0;
    if (C == 2) {
        I = k_celt_log2t[end - start];
        if (I > B) {
            I = 0;
        } else {
            B -= I;
            D = B >= 8 ? 8 : 0;
            B -= D;
        }
    }
    int thresh[k_bands];
    int trimoff[k_bands];
    for (int j = start; j < end; ++j) {
        const int N = width_of(j) * M;
        thresh[j] = std::max(8 * C, (24 * N) >> 4);
        trimoff[j] = (C * width_of(j) * (trim - 5 - LM) * (end - j - 1) * (1 << (LM + 3))) >> 6;
        if (N == 1) trimoff[j] -= 8 * C;
    }
    auto static_alloc = [&](int v, int j) {
        int s = (C * width_of(j) * k_celt_alloc[v][j] * M) >> 2;
        if (s > 0) s = std::max(0, s + trimoff[j]);
        return s;
    };
    auto trial = [&](const int* x) {
        int sum = 0;
        bool met = false;
        for (int j = end - 1; j >= start; --j) {
            if (!met && x[j] >= thresh[j]) met = true;
            if (met) {
                sum += std::min(x[j], cap[j]);
            } else if (x[j] >= 8 * C) {
                sum += 8 * C;
            }
        }
        return sum;
    };
    int x[k_bands] = {};
    int lo = 1;
    int hi = 10;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        for (int j = start; j < end; ++j) x[j] = static_alloc(mid, j) + offsets[j];
        if (trial(x) > B) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    const int u = lo;
    const int l = lo - 1;
    int b1[k_bands] = {};
    int b2[k_bands] = {};
    int skip_start = start;
    for (int j = start; j < end; ++j) {
        b1[j] = static_alloc(l, j) + (l > 0 ? offsets[j] : 0);
        if (u > 10) {
            int top = cap[j];
            if (top > 0) top = std::max(0, top + trimoff[j]);
            b2[j] = top + offsets[j];
        } else {
            b2[j] = static_alloc(u, j) + offsets[j];
        }
        b2[j] = std::max(0, b2[j] - b1[j]);
        if (offsets[j] > 0) skip_start = j;
    }
    int flo = 0;
    int fhi = 64;
    for (int it = 0; it < 6; ++it) {
        const int mid = (flo + fhi) / 2;
        for (int j = start; j < end; ++j) x[j] = b1[j] + ((mid * b2[j]) >> 6);
        if (trial(x) > B) {
            fhi = mid;
        } else {
            flo = mid;
        }
    }
    int a[k_bands] = {};
    int P = 0;
    {
        bool met = false;
        for (int j = end - 1; j >= start; --j) {
            const int v = b1[j] + ((flo * b2[j]) >> 6);
            if (!met && v >= thresh[j]) met = true;
            int t;
            if (met) {
                t = v;
            } else {
                t = v >= 8 * C ? 8 * C : 0;
            }
            a[j] = std::min(t, cap[j]);
            P += a[j];
        }
    }
    const int alloc_floor = 8 * C;
    int coded = end;
    for (int k = end; k > start; --k) {
        const int j = k - 1;
        if (j <= skip_start) {
            B += S;
            coded = k;
            break;
        }
        int left = B - P;
        const int span = k_celt_edges[k] - k_celt_edges[start];
        const int pc = left / span;
        left -= pc * span;
        const int rem = std::max(left - (k_celt_edges[j] - k_celt_edges[start]), 0);
        int bb = a[j] + pc * (k_celt_edges[k] - k_celt_edges[j]) + rem;
        if (bb >= std::max(thresh[j], alloc_floor + 8)) {
            if (rd.bit_logp(1)) {
                coded = k;
                break;
            }
            P += 8;
            bb -= 8;
        }
        P -= a[j] + I;
        if (I > 0) I = k_celt_log2t[j - start];
        P += I;
        if (bb >= alloc_floor) {
            a[j] = alloc_floor;
            P += alloc_floor;
        } else {
            a[j] = 0;
        }
        coded = j;
    }
    int intensity = 0;
    if (I > 0) intensity = start + static_cast<int>(rd.uint(static_cast<std::uint32_t>(coded + 1 - start)));
    if (intensity <= start) {
        B += D;
        D = 0;
    }
    const bool dual = D > 0 ? rd.bit_logp(1) : false;
    {
        int left = B - P;
        const int span = k_celt_edges[coded] - k_celt_edges[start];
        const int pc = span > 0 ? left / span : 0;
        left -= pc * span;
        for (int j = start; j < coded; ++j) a[j] += pc * width_of(j);
        for (int j = start; j < coded; ++j) {
            const int t = std::min(left, width_of(j));
            a[j] += t;
            left -= t;
        }
    }
    int carry = 0;
    for (int j = start; j < coded; ++j) {
        const int N = width_of(j) * M;
        const int bit = a[j] + carry;
        int excess;
        if (N > 1) {
            excess = std::max(bit - cap[j], 0);
            a[j] = bit - excess;
            int den = C * N;
            if (C == 2 && N > 2 && !dual && j < intensity) ++den;
            const int NL = den * (k_celt_lw[j] + 8 * LM);
            int off = (NL >> 1) - 21 * den;
            if (N == 2) off += 2 * den;
            if (a[j] + off < 16 * den) {
                off += NL >> 2;
            } else if (a[j] + off < 24 * den) {
                off += NL >> 3;
            }
            int F = (std::max(0, a[j] + off + 4 * den) / den) >> 3;
            if (C * F > (a[j] >> 3)) F = (a[j] >> (C - 1)) >> 3;
            F = std::min(F, 8);
            out.priority[j] = F * 8 * den >= a[j] + off ? 1 : 0;
            a[j] -= 8 * C * F;
            out.fine[j] = F;
        } else {
            excess = std::max(0, bit - 8 * C);
            a[j] = bit - excess;
            out.fine[j] = 0;
            out.priority[j] = 1;
        }
        if (excess > 0) {
            const int extra = std::min(excess >> (3 + C - 1), 8 - out.fine[j]);
            out.fine[j] += extra;
            const int eb = 8 * C * extra;
            out.priority[j] = eb >= excess - carry ? 1 : 0;
            excess -= eb;
        }
        carry = excess;
        out.shape[j] = a[j];
    }
    for (int j = coded; j < end; ++j) {
        out.fine[j] = (a[j] >> (C - 1)) >> 3;
        out.shape[j] = 0;
        out.priority[j] = out.fine[j] < 1 ? 1 : 0;
    }
    out.coded = coded;
    out.intensity = intensity;
    out.dual = dual;
    out.balance = carry;
}

struct Angle {
    int itheta = 0;
    int imid = 32767;
    int iside = 0;
    int delta = -16384;
    int qa = 0;
    bool inv = false;
};

struct BandCoder {
    RangeDecoder* rd = nullptr;
    int LM = 0;
    int spread = 2;
    int band = 0;
    int intensity = 0;
    bool allow_inversion = true;
    bool transient = false;
    int remaining = 0;
    std::uint32_t* seed = nullptr;

    std::uint32_t next_seed() {
        *seed = 1664525u * *seed + 1013904223u;
        return *seed;
    }

    Angle angle(int n, int b, int Bk, int base_blocks, int lambda, bool stereo, unsigned& fill) {
        Angle r;
        const int pcap = k_celt_lw[band] + 8 * lambda;
        const int off = (pcap >> 1) - ((stereo && n == 2) ? 16 : 4);
        int n2 = 2 * n - 1;
        if (stereo && n == 2) --n2;
        int qb = trunc_div(b + n2 * off, n2);
        qb = std::min(qb, b - pcap - 32);
        qb = std::min(qb, 64);
        int qn;
        if (qb < 4) {
            qn = 1;
        } else {
            qn = k_exp2_frac[qb & 7] >> (14 - (qb >> 3));
            qn = 2 * ((qn + 1) / 2);
        }
        if (stereo && band >= intensity) qn = 1;
        const int t0 = static_cast<int>(rd->tell_frac());
        int itheta = 0;
        if (qn > 1) {
            int xv;
            if (stereo && n > 2) {
                const int h = qn / 2;
                const int ft = 3 * (h + 1) + h;
                const int f = static_cast<int>(rd->decode(static_cast<std::uint32_t>(ft)));
                if (f < 3 * (h + 1)) {
                    xv = f / 3;
                } else {
                    xv = h + 1 + (f - 3 * (h + 1));
                }
                int fl;
                int fh;
                if (xv <= h) {
                    fl = 3 * xv;
                    fh = 3 * xv + 3;
                } else {
                    fl = (xv - 1 - h) + 3 * (h + 1);
                    fh = (xv - h) + 3 * (h + 1);
                }
                rd->update(static_cast<std::uint32_t>(fl), static_cast<std::uint32_t>(fh), static_cast<std::uint32_t>(ft));
            } else if (base_blocks > 1 || stereo) {
                xv = static_cast<int>(rd->uint(static_cast<std::uint32_t>(qn + 1)));
            } else {
                const int h = qn / 2;
                const int ft = (h + 1) * (h + 1);
                const int f = static_cast<int>(rd->decode(static_cast<std::uint32_t>(ft)));
                int fl;
                int width;
                if (f < h * (h + 1) / 2) {
                    xv = static_cast<int>((isqrt32(8u * static_cast<std::uint32_t>(f) + 1u) - 1u) / 2u);
                    fl = xv * (xv + 1) / 2;
                    width = xv + 1;
                } else {
                    xv = static_cast<int>((2u * static_cast<std::uint32_t>(qn + 1) - isqrt32(8u * static_cast<std::uint32_t>(ft - f - 1) + 1u)) / 2u);
                    width = qn + 1 - xv;
                    fl = ft - (qn + 1 - xv) * (qn + 2 - xv) / 2;
                }
                rd->update(static_cast<std::uint32_t>(fl), static_cast<std::uint32_t>(fl + width), static_cast<std::uint32_t>(ft));
            }
            itheta = static_cast<int>((16384LL * xv) / qn);
        } else if (stereo) {
            bool inv = false;
            if (b > 16 && remaining > 16) inv = rd->bit_logp(2);
            r.inv = allow_inversion ? inv : false;
            itheta = 0;
        }
        r.qa = static_cast<int>(rd->tell_frac()) - t0;
        r.itheta = itheta;
        if (itheta == 0) {
            r.imid = 32767;
            r.iside = 0;
            r.delta = -16384;
            fill &= (1u << Bk) - 1u;
        } else if (itheta == 16384) {
            r.imid = 0;
            r.iside = 32767;
            r.delta = 16384;
            fill &= ((1u << Bk) - 1u) << Bk;
        } else {
            r.imid = bcos(itheta);
            r.iside = bcos(16384 - itheta);
            r.delta = fmul((n - 1) * 128, l2tan(r.iside, r.imid));
        }
        return r;
    }

    void rotate_pass(float* z, int len, int stride, float c, float s) {
        for (int i = 0; i < len - stride; ++i) {
            const float z1 = z[i];
            const float z2 = z[i + stride];
            z[i + stride] = c * z2 + s * z1;
            z[i] = c * z1 - s * z2;
        }
        for (int i = len - 2 * stride - 1; i >= 0; --i) {
            const float z1 = z[i];
            const float z2 = z[i + stride];
            z[i + stride] = c * z2 + s * z1;
            z[i] = c * z1 - s * z2;
        }
    }

    unsigned leaf(float* X, int n, int b, int Bk, const float* F, float gamma, unsigned fill) {
        const CeltCache& cache = celt_cache();
        const int row = cache.row_for(n);
        const std::vector<int>& cost = cache.cost[static_cast<std::size_t>(row)];
        const int kmax = cache.kmax[static_cast<std::size_t>(row)];
        auto cost_of = [&](int q) { return q == 0 ? 0 : cost[static_cast<std::size_t>(q)] + 1; };
        int q;
        const int t = b - 1;
        if (t > cost[static_cast<std::size_t>(kmax)]) {
            q = kmax;
        } else {
            int h = 1;
            while (h < kmax && cost[static_cast<std::size_t>(h)] < t) ++h;
            const int lq = h - 1;
            q = (t - cost[static_cast<std::size_t>(lq)] <= cost[static_cast<std::size_t>(h)] - t) ? lq : h;
        }
        remaining -= cost_of(q);
        while (remaining < 0 && q > 0) {
            remaining += cost_of(q);
            --q;
            remaining -= cost_of(q);
        }
        if (q > 0) {
            const int K = pulses_of(q);
            const std::uint32_t total = pvq_count(n, K);
            std::uint64_t i = rd->uint(total);
            const UTable& ut = u_table();
            int y[176];
            int nn = n;
            int k = K;
            long long R = 0;
            for (int j = 0; j < n; ++j) {
                bool negative = false;
                const std::uint64_t up = ut.get(nn, k + 1);
                if (i >= up) {
                    negative = true;
                    i -= up;
                }
                int kp = k;
                while (kp > 0 && ut.get(nn, kp) > i) --kp;
                const int mag = k - kp;
                i -= ut.get(nn, kp);
                y[j] = negative ? -mag : mag;
                R += static_cast<long long>(mag) * mag;
                k = kp;
                --nn;
            }
            const float scale = R > 0 ? gamma / std::sqrt(static_cast<float>(R)) : 0.0f;
            for (int j = 0; j < n; ++j) X[j] = scale * static_cast<float>(y[j]);
            if (2 * K < n && spread != 0) {
                static const int factors[4] = { 0, 15, 10, 5 };
                const double g = static_cast<double>(n) / (n + factors[spread] * K);
                const double theta = g * g / 2.0;
                const float c = static_cast<float>(std::cos(theta * k_pi / 2.0));
                const float s = static_cast<float>(std::sin(theta * k_pi / 2.0));
                int t2 = 0;
                if (n >= 8 * Bk) {
                    t2 = 1;
                    while ((t2 * t2 + t2) * Bk + (Bk >> 2) < n) ++t2;
                }
                const int len = n / Bk;
                for (int blk = 0; blk < Bk; ++blk) {
                    float* z = X + blk * len;
                    if (t2) rotate_pass(z, len, t2, s, c);
                    rotate_pass(z, len, 1, c, s);
                }
            }
            if (Bk <= 1) return 1;
            unsigned cm = 0;
            const int len = n / Bk;
            for (int blk = 0; blk < Bk; ++blk) {
                for (int j = 0; j < len; ++j) {
                    if (y[blk * len + j] != 0) {
                        cm |= 1u << blk;
                        break;
                    }
                }
            }
            return cm;
        }
        fill &= (1u << Bk) - 1u;
        if (fill == 0) {
            for (int j = 0; j < n; ++j) X[j] = 0.0f;
            return 0;
        }
        unsigned cm;
        if (!F) {
            for (int j = 0; j < n; ++j) X[j] = static_cast<float>(static_cast<std::int32_t>(next_seed()) >> 20);
            cm = (1u << Bk) - 1u;
        } else {
            for (int j = 0; j < n; ++j) {
                const std::uint32_t s = next_seed();
                X[j] = F[j] + ((s & 0x8000u) ? 1.0f / 256.0f : -1.0f / 256.0f);
            }
            cm = fill;
        }
        double e = 1e-15;
        for (int j = 0; j < n; ++j) e += static_cast<double>(X[j]) * X[j];
        const float g = static_cast<float>(gamma / std::sqrt(e));
        for (int j = 0; j < n; ++j) X[j] *= g;
        return cm;
    }

    unsigned partition(float* X, int n, int b, int Bk, const float* F, int lambda, float gamma, unsigned fill) {
        const CeltCache& cache = celt_cache();
        const int row = cache.row_for(n);
        const int maxcost = cache.cost[static_cast<std::size_t>(row)][static_cast<std::size_t>(cache.kmax[static_cast<std::size_t>(row)])];
        const int base_blocks = Bk;
        if (lambda >= 0 && b > maxcost + 12 && n > 2) {
            n /= 2;
            --lambda;
            if (Bk == 1) fill = (fill & 1u) | (fill << 1);
            Bk = (Bk + 1) / 2;
            Angle a = angle(n, b, Bk, base_blocks, lambda, false, fill);
            b -= a.qa;
            int delta = a.delta;
            if (base_blocks > 1 && a.itheta != 0 && a.itheta != 16384) {
                if (a.itheta > 8192) {
                    delta -= delta >> (4 - lambda);
                } else {
                    delta = std::min(0, delta + ((8 * n) >> (5 - lambda)));
                }
            }
            int mbits = std::max(0, std::min(b, trunc_div(b - delta, 2)));
            int sbits = b - mbits;
            remaining -= a.qa;
            const float g1 = gamma * static_cast<float>(a.imid) / 32768.0f;
            const float g2 = gamma * static_cast<float>(a.iside) / 32768.0f;
            const float* F1 = F;
            const float* F2 = F ? F + n : nullptr;
            unsigned cm1;
            unsigned cm2;
            if (mbits >= sbits) {
                const int before = remaining;
                cm1 = partition(X, n, mbits, Bk, F1, lambda, g1, fill);
                const int rb = mbits - (before - remaining);
                if (rb > 24 && a.itheta != 0) sbits += rb - 24;
                cm2 = partition(X + n, n, sbits, Bk, F2, lambda, g2, fill >> Bk);
            } else {
                const int before = remaining;
                cm2 = partition(X + n, n, sbits, Bk, F2, lambda, g2, fill >> Bk);
                const int rb = sbits - (before - remaining);
                if (rb > 24 && a.itheta != 16384) mbits += rb - 24;
                cm1 = partition(X, n, mbits, Bk, F1, lambda, g1, fill);
            }
            return cm1 | (cm2 << (base_blocks / 2));
        }
        return leaf(X, n, b, Bk, F, gamma, fill);
    }

    static void butterfly_bit(float* v, int n, int stride) {
        for (int i = 0; i < n; ++i) {
            if (i & stride) continue;
            const float a = v[i];
            const float c = v[i + stride];
            v[i] = (a + c) * k_inv_sqrt2;
            v[i + stride] = (a - c) * k_inv_sqrt2;
        }
    }

    static void reorder(float* v, int N, int G, bool sequency, bool forward) {
        float tmp[176];
        const int per = N / G;
        const int* ord = nullptr;
        if (sequency) {
            if (G == 2) ord = k_order2;
            if (G == 4) ord = k_order4;
            if (G == 8) ord = k_order8;
            if (G == 16) ord = k_order16;
        }
        for (int i = 0; i < G; ++i) {
            const int oi = ord ? ord[i] : i;
            for (int u = 0; u < per; ++u) {
                if (forward) {
                    tmp[oi * per + u] = v[u * G + i];
                } else {
                    tmp[u * G + i] = v[oi * per + u];
                }
            }
        }
        std::memcpy(v, tmp, sizeof(float) * static_cast<std::size_t>(N));
    }

    unsigned mono(float* X, int N, int b, int Bk, const float* source, int tf, float gamma, unsigned fill, float* fold_out) {
        if (N == 1) {
            int s = 0;
            if (remaining >= 8) {
                s = static_cast<int>(rd->bits(1));
                remaining -= 8;
            }
            X[0] = s ? -gamma : gamma;
            if (fold_out) fold_out[0] = X[0];
            return 1;
        }
        float F[176];
        const float* Fp = nullptr;
        if (source) {
            std::memcpy(F, source, sizeof(float) * static_cast<std::size_t>(N));
            Fp = F;
        }
        const int Bk_orig = Bk;
        int recombine = 0;
        int nb = N / Bk;
        if (tf > 0) {
            recombine = tf;
            for (int k = 0; k < recombine; ++k) {
                if (Fp) butterfly_bit(F, N, 1 << k);
                unsigned nf = 0;
                for (int m = 0; m < 4; ++m) {
                    if (fill & (3u << (2 * m))) nf |= 1u << m;
                }
                fill = nf;
            }
            Bk >>= recombine;
            nb <<= recombine;
        }
        int td = 0;
        int tau = tf;
        while ((nb & 1) == 0 && tau < 0) {
            if (Fp) {
                for (int u = 0; u < N / (2 * Bk); ++u) {
                    for (int i = 0; i < Bk; ++i) {
                        const int p0 = Bk * 2 * u + i;
                        const int p1 = Bk * (2 * u + 1) + i;
                        const float a = F[p0];
                        const float c = F[p1];
                        F[p0] = (a + c) * k_inv_sqrt2;
                        F[p1] = (a - c) * k_inv_sqrt2;
                    }
                }
            }
            fill |= fill << Bk;
            Bk <<= 1;
            nb >>= 1;
            ++tau;
            ++td;
        }
        const int base_blocks = Bk;
        const int G = base_blocks << recombine;
        const bool sequency = !transient;
        if (base_blocks > 1 && Fp) reorder(F, N, G, sequency, true);
        unsigned cm = partition(X, N, b, base_blocks, Fp, LM, gamma, fill);
        if (base_blocks > 1) reorder(X, N, G, sequency, false);
        for (int k = 0; k < td; ++k) {
            Bk >>= 1;
            nb <<= 1;
            cm |= cm >> Bk;
            for (int u = 0; u < N / (2 * Bk); ++u) {
                for (int i = 0; i < Bk; ++i) {
                    const int p0 = Bk * 2 * u + i;
                    const int p1 = Bk * (2 * u + 1) + i;
                    const float a = X[p0];
                    const float c = X[p1];
                    X[p0] = (a + c) * k_inv_sqrt2;
                    X[p1] = (a - c) * k_inv_sqrt2;
                }
            }
        }
        for (int k = 0; k < recombine; ++k) {
            unsigned nc = 0;
            for (int m = 0; m < 4; ++m) {
                if (cm & (1u << m)) nc |= 3u << (2 * m);
            }
            cm = nc;
            butterfly_bit(X, N, 1 << k);
        }
        Bk <<= recombine;
        (void)Bk_orig;
        cm &= (1u << Bk) - 1u;
        if (fold_out) {
            const float s = std::sqrt(static_cast<float>(N));
            for (int j = 0; j < N; ++j) fold_out[j] = X[j] * s;
        }
        return cm;
    }

    unsigned stereo(float* X, float* Y, int N, int b, int Bk, const float* source, int tf, unsigned fill, float* fold_out) {
        if (N == 1) {
            for (int c = 0; c < 2; ++c) {
                int s = 0;
                if (remaining >= 8) {
                    s = static_cast<int>(rd->bits(1));
                    remaining -= 8;
                }
                (c == 0 ? X : Y)[0] = s ? -1.0f : 1.0f;
            }
            if (fold_out) fold_out[0] = X[0];
            return 1;
        }
        const unsigned fill_orig = fill;
        Angle a = angle(N, b, Bk, Bk, LM, true, fill);
        const float mid = static_cast<float>(a.imid) / 32768.0f;
        const float side = static_cast<float>(a.iside) / 32768.0f;
        b -= a.qa;
        unsigned cm;
        if (N == 2) {
            const int sbits = (a.itheta != 0 && a.itheta != 16384) ? 8 : 0;
            const int mbits = b - sbits;
            remaining -= a.qa + sbits;
            float* P = a.itheta > 8192 ? Y : X;
            float* S = a.itheta > 8192 ? X : Y;
            int sg = 0;
            if (sbits > 0) sg = static_cast<int>(rd->bits(1));
            const float sigma = 1.0f - 2.0f * static_cast<float>(sg);
            cm = mono(P, 2, mbits, Bk, source, tf, 1.0f, fill_orig, fold_out);
            S[0] = -sigma * P[1];
            S[1] = sigma * P[0];
            for (int m = 0; m < 2; ++m) {
                X[m] *= mid;
                Y[m] *= side;
            }
            for (int m = 0; m < 2; ++m) {
                const float l = X[m];
                const float r = Y[m];
                X[m] = l - r;
                Y[m] = l + r;
            }
        } else {
            int mbits = std::max(0, std::min(b, trunc_div(b - a.delta, 2)));
            int sbits = b - mbits;
            remaining -= a.qa;
            unsigned cm_mid;
            unsigned cm_side;
            if (mbits >= sbits) {
                const int before = remaining;
                cm_mid = mono(X, N, mbits, Bk, source, tf, 1.0f, fill, fold_out);
                const int rb = mbits - (before - remaining);
                if (rb > 24 && a.itheta != 0) sbits += rb - 24;
                cm_side = mono(Y, N, sbits, Bk, nullptr, tf, side, fill >> Bk, nullptr);
            } else {
                const int before = remaining;
                cm_side = mono(Y, N, sbits, Bk, nullptr, tf, side, fill >> Bk, nullptr);
                const int rb = sbits - (before - remaining);
                if (rb > 24 && a.itheta != 16384) mbits += rb - 24;
                cm_mid = mono(X, N, mbits, Bk, source, tf, 1.0f, fill, fold_out);
            }
            cm = cm_mid | cm_side;
            double p = 0.0;
            double e = 0.0;
            for (int m = 0; m < N; ++m) {
                p += static_cast<double>(X[m]) * Y[m];
                e += static_cast<double>(Y[m]) * Y[m];
            }
            p *= mid;
            const double mm = static_cast<double>(mid) * mid;
            const double El = mm + e - 2.0 * p;
            const double Er = mm + e + 2.0 * p;
            if (El < 6e-4 || Er < 6e-4) {
                std::memcpy(Y, X, sizeof(float) * static_cast<std::size_t>(N));
            } else {
                const float il = static_cast<float>(1.0 / std::sqrt(El));
                const float ir = static_cast<float>(1.0 / std::sqrt(Er));
                for (int m = 0; m < N; ++m) {
                    const float l = mid * X[m];
                    const float r = Y[m];
                    X[m] = (l - r) * il;
                    Y[m] = (l + r) * ir;
                }
            }
        }
        if (a.inv) {
            for (int m = 0; m < N; ++m) Y[m] = -Y[m];
        }
        return cm;
    }
};

class Fft {
public:
    void setup(int n) {
        m_n = n;
        m_tw.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) m_tw[static_cast<std::size_t>(i)] = std::polar(1.0, 2.0 * k_pi * i / n);
        m_factors.clear();
        int r = n;
        while (r > 1) {
            if (r % 4 == 0) {
                m_factors.push_back(4);
                r /= 4;
            } else if (r % 2 == 0) {
                m_factors.push_back(2);
                r /= 2;
            } else if (r % 3 == 0) {
                m_factors.push_back(3);
                r /= 3;
            } else if (r % 5 == 0) {
                m_factors.push_back(5);
                r /= 5;
            } else {
                m_factors.push_back(r);
                r = 1;
            }
        }
        m_tmp.resize(static_cast<std::size_t>(n));
    }

    void run(const std::complex<double>* in, std::complex<double>* out) {
        rec(in, out, m_n, 1, 0);
    }

private:
    void rec(const std::complex<double>* in, std::complex<double>* out, int n, int stride, std::size_t level) {
        if (n == 1) {
            out[0] = in[0];
            return;
        }
        const int p = m_factors[level];
        const int m = n / p;
        for (int r = 0; r < p; ++r) rec(in + r * stride, out + r * m, m, stride * p, level + 1);
        const int step = m_n / n;
        std::complex<double> t[64];
        for (int k = 0; k < m; ++k) {
            for (int r = 0; r < p; ++r) t[r] = out[r * m + k] * m_tw[static_cast<std::size_t>((r * k * step) % m_n)];
            for (int q = 0; q < p; ++q) {
                std::complex<double> acc = 0.0;
                for (int r = 0; r < p; ++r) acc += t[r] * m_tw[static_cast<std::size_t>(((r * q) % p) * (m_n / p))];
                m_tmp[static_cast<std::size_t>(q * m + k)] = acc;
            }
        }
        for (int i = 0; i < n; ++i) out[i] = m_tmp[static_cast<std::size_t>(i)];
    }

    int m_n = 0;
    std::vector<std::complex<double>> m_tw;
    std::vector<int> m_factors;
    std::vector<std::complex<double>> m_tmp;
};

} // namespace

struct MdctImpl {
    int n = 0;
    Fft fft;
    std::vector<std::complex<double>> pre;
    std::vector<std::complex<double>> post;
    std::vector<std::complex<double>> a;
    std::vector<std::complex<double>> b;
    std::vector<double> c;

    void setup(int size) {
        n = size;
        const int h = n / 2;
        fft.setup(h);
        pre.resize(static_cast<std::size_t>(h));
        post.resize(static_cast<std::size_t>(h));
        for (int p = 0; p < h; ++p) pre[static_cast<std::size_t>(p)] = std::polar(1.0, k_pi * p / n);
        for (int q = 0; q < h; ++q) post[static_cast<std::size_t>(q)] = std::polar(1.0, k_pi * (q + 0.25) / n);
        a.resize(static_cast<std::size_t>(h));
        b.resize(static_cast<std::size_t>(h));
        c.resize(static_cast<std::size_t>(n));
    }

    void inverse(const float* X, float* y) {
        const int h = n / 2;
        for (int p = 0; p < h; ++p) {
            const std::complex<double> v(X[2 * p], -static_cast<double>(X[n - 1 - 2 * p]));
            a[static_cast<std::size_t>(p)] = v * pre[static_cast<std::size_t>(p)];
        }
        fft.run(a.data(), b.data());
        for (int q = 0; q < h; ++q) {
            const std::complex<double> v = b[static_cast<std::size_t>(q)] * post[static_cast<std::size_t>(q)];
            c[static_cast<std::size_t>(2 * q)] = v.real();
            c[static_cast<std::size_t>(n - 1 - 2 * q)] = v.imag();
        }
        for (int i = 0; i < 2 * n; ++i) {
            const int m = i + n / 2;
            double v;
            if (m < n) {
                v = c[static_cast<std::size_t>(m)];
            } else if (m < 2 * n) {
                v = -c[static_cast<std::size_t>(2 * n - 1 - m)];
            } else {
                v = -c[static_cast<std::size_t>(m - 2 * n)];
            }
            y[i] = static_cast<float>(v);
        }
    }
};

struct CeltDecoder::State {
    MdctImpl mdct[4];
};

CeltDecoder::CeltDecoder() : m_state(new State()) {
    for (int i = 0; i < 4; ++i) m_state->mdct[i].setup(120 << i);
    m_window.resize(k_overlap);
    for (int i = 0; i < k_overlap; ++i) {
        const double s = std::sin(0.5 * k_pi * (i + 0.5) / k_overlap);
        m_window[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(0.5 * k_pi * s * s));
    }
    init(1, 1);
}

CeltDecoder::~CeltDecoder() = default;
CeltDecoder::CeltDecoder(CeltDecoder&&) noexcept = default;
CeltDecoder& CeltDecoder::operator=(CeltDecoder&&) noexcept = default;

void CeltDecoder::init(int out_channels, int downsample) {
    m_out_channels = out_channels;
    m_down = downsample;
    reset();
}

void CeltDecoder::reset() {
    m_seed = 0;
    for (int c = 0; c < 2; ++c) {
        for (int j = 0; j < k_bands; ++j) {
            m_energy[c][j] = 0.0f;
            m_hist1[c][j] = -28.0f;
            m_hist2[c][j] = -28.0f;
        }
        m_overlap[c].assign(k_overlap, 0.0f);
        m_postfilter_hist[c].assign(k_pf_history, 0.0f);
        m_deemph[c] = 0.0f;
    }
    m_pf_period_old = 0;
    m_pf_period = 0;
    m_pf_gain_old = 0.0f;
    m_pf_gain = 0.0f;
    m_pf_tapset_old = 0;
    m_pf_tapset = 0;
}

void CeltDecoder::decode(RangeDecoder& rd, int frame48, int start, int end, int C, float* out, bool accumulate) {
    int LM = 0;
    while ((120 << LM) < frame48 && LM < 3) ++LM;
    const int M = 1 << LM;
    const int storage = static_cast<int>(rd.storage());
    const int T1 = 8 * storage;
    if (end > k_bands) end = k_bands;
    if (start >= end) start = 0;
    if (C == 1) {
        for (int j = 0; j < k_bands; ++j) m_energy[0][j] = std::max(m_energy[0][j], m_energy[1][j]);
    }
    bool silence = false;
    int t = rd.tell();
    if (t >= T1) {
        silence = true;
    } else if (t == 1) {
        silence = rd.bit_logp(15);
    }
    if (silence) rd.skip_to_end();
    int pf_period = 0;
    float pf_gain = 0.0f;
    int pf_tapset = 0;
    t = rd.tell();
    if (start == 0 && t + 16 <= T1) {
        if (rd.bit_logp(1)) {
            const int octave = static_cast<int>(rd.uint(6));
            pf_period = (16 << octave) + static_cast<int>(rd.bits(static_cast<unsigned>(4 + octave))) - 1;
            const int g = static_cast<int>(rd.bits(3));
            pf_tapset = rd.tell() + 2 <= T1 ? rd.icdf(k_tapset_icdf, 2) : 0;
            pf_gain = 3.0f * static_cast<float>(g + 1) / 32.0f;
        }
    }
    t = rd.tell();
    const bool transient = LM > 0 && t + 3 <= T1 ? rd.bit_logp(3) : false;
    t = rd.tell();
    const bool intra = t + 3 <= T1 ? rd.bit_logp(3) : false;
    float E[2][k_bands];
    for (int c = 0; c < 2; ++c) {
        for (int j = 0; j < k_bands; ++j) E[c][j] = m_energy[c][j];
    }
    {
        const float alpha = intra ? 0.0f : k_alpha[LM];
        const float beta = intra ? k_beta_intra : k_beta[LM];
        float P[2] = { 0.0f, 0.0f };
        for (int j = start; j < end; ++j) {
            for (int c = 0; c < C; ++c) {
                const int r = T1 - rd.tell();
                int q;
                if (r >= 15) {
                    const std::uint8_t* prm = k_celt_laplace[LM][intra ? 1 : 0][std::min(j, 20)];
                    q = laplace_decode(rd, prm[0] * 128, prm[1] * 64);
                } else if (r >= 2) {
                    const int s = rd.icdf(k_small_energy_icdf, 2);
                    q = s == 0 ? 0 : (s == 1 ? -1 : 1);
                } else if (r >= 1) {
                    q = -static_cast<int>(rd.bit_logp(1));
                } else {
                    q = -1;
                }
                const float old = std::max(-9.0f, m_energy[c][j]);
                float v = alpha * old + P[c] + static_cast<float>(q);
                v = std::max(-28.0f, std::min(28.0f, v));
                E[c][j] = v;
                P[c] += (1.0f - beta) * static_cast<float>(q);
            }
        }
    }
    int tf_change[k_bands] = {};
    {
        int budget = T1;
        int tt = rd.tell();
        int L = transient ? 2 : 4;
        const int rsv = (LM > 0 && tt + L + 1 <= budget) ? 1 : 0;
        budget -= rsv;
        int cur = 0;
        int changed = 0;
        int flags[k_bands] = {};
        for (int j = start; j < end; ++j) {
            if (tt + L <= budget) {
                cur ^= rd.bit_logp(static_cast<unsigned>(L)) ? 1 : 0;
                tt = rd.tell();
                changed |= cur;
            }
            flags[j] = cur;
            L = transient ? 4 : 5;
        }
        int tf_select = 0;
        const int tr = transient ? 1 : 0;
        if (rsv && k_tf_table[LM][4 * tr + 0 + changed] != k_tf_table[LM][4 * tr + 2 + changed]) tf_select = rd.bit_logp(1) ? 1 : 0;
        for (int j = start; j < end; ++j) tf_change[j] = k_tf_table[LM][4 * tr + 2 * tf_select + flags[j]];
    }
    const int spread = rd.tell() + 4 <= T1 ? rd.icdf(k_spread_icdf, 5) : 2;
    const CeltCache& cache = celt_cache();
    int cap[k_bands] = {};
    for (int j = 0; j < k_bands; ++j) cap[j] = ((cache.caps[static_cast<std::size_t>(21 * (2 * LM + C - 1) + j)] + 64) * C * width_of(j) * M) >> 2;
    int offsets[k_bands] = {};
    int total_bits = 64 * storage;
    {
        int L = 6;
        int tf = static_cast<int>(rd.tell_frac());
        for (int j = start; j < end; ++j) {
            const int N = width_of(j) * M;
            const int quantum = std::min(8 * C * N, std::max(48, C * N));
            int boost = 0;
            int Lp = L;
            while (tf + 8 * Lp < total_bits && boost < cap[j]) {
                const bool flag = rd.bit_logp(static_cast<unsigned>(Lp));
                tf = static_cast<int>(rd.tell_frac());
                if (!flag) break;
                boost += quantum;
                total_bits -= quantum;
                Lp = 1;
            }
            offsets[j] = boost;
            if (boost > 0) L = std::max(2, L - 1);
        }
    }
    int trim = 5;
    if (static_cast<int>(rd.tell_frac()) + 48 <= total_bits) trim = rd.icdf(k_trim_icdf, 7);
    Allocation alloc;
    int ac_rsv = 0;
    compute_allocation(rd, start, end, C, LM, transient, storage, cap, offsets, trim, ac_rsv, alloc);
    for (int j = start; j < end; ++j) {
        const int F = alloc.fine[j];
        if (F <= 0) continue;
        if (rd.tell() + C * F > T1) continue;
        for (int c = 0; c < C; ++c) {
            const int v = static_cast<int>(rd.bits(static_cast<unsigned>(F)));
            E[c][j] += (static_cast<float>(v) + 0.5f) / static_cast<float>(1 << F) - 0.5f;
        }
    }
    const int nspec = 120 * M;
    std::vector<float> X[2];
    X[0].assign(static_cast<std::size_t>(nspec), 0.0f);
    X[1].assign(static_cast<std::size_t>(nspec), 0.0f);
    std::uint8_t collapse[k_bands][2] = {};
    {
        const int base = M * k_celt_edges[start];
        const int fold_len = M * k_celt_edges[end] - base;
        std::vector<float> fold[2];
        fold[0].assign(static_cast<std::size_t>(std::max(fold_len, 1)), 0.0f);
        fold[1].assign(static_cast<std::size_t>(std::max(fold_len, 1)), 0.0f);
        BandCoder bc;
        bc.rd = &rd;
        bc.LM = LM;
        bc.spread = spread;
        bc.intensity = alloc.intensity;
        bc.allow_inversion = m_allow_inversion && m_out_channels == 2;
        bc.transient = transient;
        bc.seed = &m_seed;
        const int TB = 64 * storage - ac_rsv;
        int balance = alloc.balance;
        int fold_band = 0;
        bool update = true;
        bool dual = alloc.dual;
        const int Bk = transient ? M : 1;
        for (int j = start; j < end; ++j) {
            const int N = width_of(j) * M;
            const int t0 = static_cast<int>(rd.tell_frac());
            if (j != start) balance -= t0;
            bc.remaining = TB - t0 - 1;
            bc.band = j;
            int b = 0;
            if (j < alloc.coded) {
                const int cb = trunc_div(balance, std::min(3, alloc.coded - j));
                b = std::max(0, std::min(16383, std::min(bc.remaining + 1, alloc.shape[j] + cb)));
            }
            if ((M * k_celt_edges[j] - N >= base || j == start + 1) && (update || fold_band == 0)) fold_band = j;
            if (j == start + 1) {
                const int n1 = M * width_of(start);
                const int n2 = M * width_of(start + 1);
                for (int c = 0; c < (dual ? 2 : 1); ++c) {
                    for (int i = 0; i < n2 - n1; ++i) {
                        const int from = 2 * n1 - n2 + i;
                        const int to = n1 + i;
                        if (from >= 0 && to < fold_len) fold[c][static_cast<std::size_t>(to)] = fold[c][static_cast<std::size_t>(from)];
                    }
                }
            }
            const int tf = tf_change[j];
            int src = -1;
            unsigned xmask;
            unsigned ymask;
            if (fold_band != 0 && (spread != 3 || transient || tf < 0)) {
                src = std::max(0, M * k_celt_edges[fold_band] - base - N);
                int fs = fold_band;
                do {
                    --fs;
                } while (fs > start && M * k_celt_edges[fs] > src + base);
                int fe = fold_band;
                while (fe < j && M * k_celt_edges[fe] < src + base + N) ++fe;
                xmask = 0;
                ymask = 0;
                for (int i = fs; i < fe; ++i) {
                    xmask |= collapse[i][0];
                    ymask |= collapse[i][C - 1];
                }
            } else {
                xmask = (1u << Bk) - 1u;
                ymask = xmask;
            }
            if (dual && j == alloc.intensity) {
                dual = false;
                const int upto = M * k_celt_edges[j] - base;
                for (int i = 0; i < upto; ++i) fold[0][static_cast<std::size_t>(i)] = 0.5f * (fold[0][static_cast<std::size_t>(i)] + fold[1][static_cast<std::size_t>(i)]);
            }
            const bool last = j == end - 1;
            const int at = M * k_celt_edges[j] - base;
            float* xs = X[0].data() + M * k_celt_edges[j];
            float* ys = X[1].data() + M * k_celt_edges[j];
            float tmp_fold[176];
            float tmp_fold2[176];
            unsigned xr;
            unsigned yr;
            const float* s0 = src >= 0 ? fold[0].data() + src : nullptr;
            if (dual) {
                const float* s1 = src >= 0 ? fold[1].data() + src : nullptr;
                xr = bc.mono(xs, N, b / 2, Bk, s0, tf, 1.0f, xmask, last ? nullptr : tmp_fold);
                yr = bc.mono(ys, N, b / 2, Bk, s1, tf, 1.0f, ymask, last ? nullptr : tmp_fold2);
                if (!last) {
                    std::memcpy(fold[0].data() + at, tmp_fold, sizeof(float) * static_cast<std::size_t>(N));
                    std::memcpy(fold[1].data() + at, tmp_fold2, sizeof(float) * static_cast<std::size_t>(N));
                }
            } else if (C == 2) {
                xr = bc.stereo(xs, ys, N, b, Bk, s0, tf, xmask | ymask, last ? nullptr : tmp_fold);
                yr = xr;
                if (!last) std::memcpy(fold[0].data() + at, tmp_fold, sizeof(float) * static_cast<std::size_t>(N));
            } else {
                xr = bc.mono(xs, N, b, Bk, s0, tf, 1.0f, xmask | ymask, last ? nullptr : tmp_fold);
                yr = xr;
                if (!last) std::memcpy(fold[0].data() + at, tmp_fold, sizeof(float) * static_cast<std::size_t>(N));
            }
            collapse[j][0] = static_cast<std::uint8_t>(xr & 0xFFu);
            collapse[j][C - 1] = static_cast<std::uint8_t>(yr & 0xFFu);
            update = b > 8 * N;
            balance += alloc.shape[j] + t0;
        }
    }
    bool anti_collapse = false;
    if (ac_rsv > 0) anti_collapse = rd.bits(1) != 0;
    {
        int R = T1 - rd.tell();
        for (int p = 0; p < 2; ++p) {
            for (int j = start; j < end; ++j) {
                if (R < C) break;
                if (alloc.fine[j] >= 8 || alloc.priority[j] != p) continue;
                for (int c = 0; c < C; ++c) {
                    const int v = static_cast<int>(rd.bits(1));
                    E[c][j] += (static_cast<float>(v) - 0.5f) / static_cast<float>(1 << (alloc.fine[j] + 1));
                    --R;
                }
            }
        }
    }
    if (anti_collapse) {
        for (int j = start; j < end; ++j) {
            const int w = width_of(j);
            const int N = w * M;
            const int depth = ((1 + alloc.shape[j]) / w) >> LM;
            const float thr = 0.5f * std::exp2(-static_cast<float>(depth) / 8.0f);
            const float scale = 1.0f / std::sqrt(static_cast<float>(N));
            for (int c = 0; c < C; ++c) {
                float p1 = m_hist1[c][j];
                float p2 = m_hist2[c][j];
                if (C == 1) {
                    p1 = std::max(m_hist1[0][j], m_hist1[1][j]);
                    p2 = std::max(m_hist2[0][j], m_hist2[1][j]);
                }
                const float d = std::max(0.0f, E[c][j] - std::min(p1, p2));
                float r = 2.0f * std::exp2(-d);
                if (LM == 3) r *= 1.41421356f;
                r = std::min(thr, r) * scale;
                float* xb = X[c].data() + M * k_celt_edges[j];
                bool filled = false;
                for (int k = 0; k < M; ++k) {
                    if (collapse[j][c] & (1u << k)) continue;
                    for (int f = 0; f < w; ++f) {
                        const std::uint32_t s = 1664525u * m_seed + 1013904223u;
                        m_seed = s;
                        xb[f * M + k] = (s & 0x8000u) ? r : -r;
                    }
                    filled = true;
                }
                if (filled) {
                    double e = 1e-15;
                    for (int i = 0; i < N; ++i) e += static_cast<double>(xb[i]) * xb[i];
                    const float g = static_cast<float>(1.0 / std::sqrt(e));
                    for (int i = 0; i < N; ++i) xb[i] *= g;
                }
            }
        }
    }
    const int limit = (120 * M) / m_down;
    for (int c = 0; c < C; ++c) {
        for (int j = 0; j < k_bands; ++j) {
            const int lo = M * k_celt_edges[j];
            const int hi = M * k_celt_edges[j + 1];
            float g = 0.0f;
            if (j >= start && j < end && !silence) g = std::exp2(std::min(32.0f, E[c][j] + static_cast<float>(k_celt_means[j]) / 16.0f));
            for (int i = lo; i < hi; ++i) X[c][static_cast<std::size_t>(i)] *= g;
        }
        for (int i = M * k_celt_edges[21]; i < nspec; ++i) X[c][static_cast<std::size_t>(i)] = 0.0f;
        for (int i = limit; i < nspec; ++i) X[c][static_cast<std::size_t>(i)] = 0.0f;
    }
    if (silence) {
        for (int c = 0; c < 2; ++c) {
            for (int j = 0; j < k_bands; ++j) E[c][j] = -28.0f;
        }
    }
    synthesize(X, C, LM, transient, silence, frame48, out, accumulate, pf_period, pf_gain, pf_tapset);
    if (C == 1) {
        for (int j = 0; j < k_bands; ++j) E[1][j] = E[0][j];
    }
    for (int c = 0; c < 2; ++c) {
        for (int j = 0; j < k_bands; ++j) {
            if (!transient) {
                m_hist2[c][j] = m_hist1[c][j];
                m_hist1[c][j] = E[c][j];
            } else {
                m_hist1[c][j] = std::min(m_hist1[c][j], E[c][j]);
            }
            if (j < start || j >= end) {
                E[c][j] = 0.0f;
                m_hist1[c][j] = -28.0f;
                m_hist2[c][j] = -28.0f;
            }
            m_energy[c][j] = E[c][j];
        }
    }
    m_seed = rd.range();
}

void CeltDecoder::synthesize(const std::vector<float> spectrum[2], int C, int LM, bool transient, bool silence, int frame48, float* out, bool accumulate, int pf_period, float pf_gain, int pf_tapset) {
    (void)silence;
    const int M = 1 << LM;
    const int N = 120 * M;
    (void)frame48;
    std::vector<float> spec[2];
    const int outc = m_out_channels;
    if (outc == 1 && C == 2) {
        spec[0].resize(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i) spec[0][static_cast<std::size_t>(i)] = 0.5f * (spectrum[0][static_cast<std::size_t>(i)] + spectrum[1][static_cast<std::size_t>(i)]);
    } else {
        spec[0] = spectrum[0];
        spec[1] = C == 2 ? spectrum[1] : spectrum[0];
    }
    std::vector<float> time(static_cast<std::size_t>(N + k_overlap));
    std::vector<float> y(static_cast<std::size_t>(2 * N));
    std::vector<float> sb(120);
    for (int c = 0; c < outc; ++c) {
        std::fill(time.begin(), time.end(), 0.0f);
        const std::vector<float>& S = spec[c];
        if (transient) {
            MdctImpl& md = m_state->mdct[0];
            for (int blk = 0; blk < M; ++blk) {
                for (int f = 0; f < 120; ++f) sb[static_cast<std::size_t>(f)] = S[static_cast<std::size_t>(f * M + blk)];
                md.inverse(sb.data(), y.data());
                for (int i = 0; i < 240; ++i) {
                    float w;
                    if (i < k_overlap) {
                        w = m_window[static_cast<std::size_t>(i)];
                    } else {
                        w = m_window[static_cast<std::size_t>(239 - i)];
                    }
                    time[static_cast<std::size_t>(blk * 120 + i)] += y[static_cast<std::size_t>(i)] * w;
                }
            }
        } else {
            MdctImpl& md = m_state->mdct[LM];
            md.inverse(S.data(), y.data());
            const int zero = (N - k_overlap) / 2;
            for (int i = 0; i < N + k_overlap; ++i) {
                const int n = i + zero;
                float w;
                if (i < k_overlap) {
                    w = m_window[static_cast<std::size_t>(i)];
                } else if (i >= N) {
                    w = m_window[static_cast<std::size_t>(N + k_overlap - 1 - i)];
                } else {
                    w = 1.0f;
                }
                time[static_cast<std::size_t>(i)] = y[static_cast<std::size_t>(n)] * w;
            }
        }
        std::vector<float>& ov = m_overlap[c];
        for (int i = 0; i < k_overlap; ++i) time[static_cast<std::size_t>(i)] += ov[static_cast<std::size_t>(i)];
        for (int i = 0; i < k_overlap; ++i) ov[static_cast<std::size_t>(i)] = std::max(-k_state_limit, std::min(k_state_limit, time[static_cast<std::size_t>(N + i)]));
        std::vector<float>& hist = m_postfilter_hist[c];
        std::vector<float> buf(static_cast<std::size_t>(k_pf_history + N));
        std::copy(hist.begin(), hist.end(), buf.begin());
        for (int i = 0; i < N; ++i) buf[static_cast<std::size_t>(k_pf_history + i)] = time[static_cast<std::size_t>(i)] * m_scale;
        float* yb = buf.data() + k_pf_history;
        auto filter_segment = [&](int from, int count, int T0, float G0, int tap0, int T1, float G1, int tap1) {
            if (G0 == 0.0f && G1 == 0.0f) return;
            T0 = std::max(T0, 15);
            T1 = std::max(T1, 15);
            const float* a0 = k_taps[tap0];
            const float* a1 = k_taps[tap1];
            for (int i = 0; i < count; ++i) {
                const int n = from + i;
                const float* p0 = yb + n - T0;
                const float* p1 = yb + n - T1;
                const float old_term = G0 * (a0[0] * p0[0] + a0[1] * (p0[1] + p0[-1]) + a0[2] * (p0[2] + p0[-2]));
                const float new_term = G1 * (a1[0] * p1[0] + a1[1] * (p1[1] + p1[-1]) + a1[2] * (p1[2] + p1[-2]));
                float v;
                if (i < k_overlap) {
                    const float w = m_window[static_cast<std::size_t>(i)] * m_window[static_cast<std::size_t>(i)];
                    v = (1.0f - w) * old_term + w * new_term;
                } else {
                    v = new_term;
                }
                yb[n] += v;
            }
        };
        filter_segment(0, std::min(120, N), m_pf_period_old, m_pf_gain_old, m_pf_tapset_old, m_pf_period, m_pf_gain, m_pf_tapset);
        if (LM > 0) filter_segment(120, N - 120, m_pf_period, m_pf_gain, m_pf_tapset, pf_period, pf_gain, pf_tapset);
        for (int i = 0; i < N; ++i) yb[i] = std::max(-k_state_limit, std::min(k_state_limit, yb[i]));
        std::copy(buf.end() - k_pf_history, buf.end(), hist.begin());
        float mem = m_deemph[c];
        int o = 0;
        for (int i = 0; i < N; ++i) {
            mem = std::max(-k_state_limit, std::min(k_state_limit, yb[i] + k_deemph * mem));
            if (i % m_down == 0) {
                const float v = mem / 32768.0f;
                float& dst = out[static_cast<std::size_t>(o * outc + c)];
                dst = accumulate ? dst + v : v;
                ++o;
            }
        }
        m_deemph[c] = mem;
    }
    if (LM > 0) {
        m_pf_period_old = pf_period;
        m_pf_gain_old = pf_gain;
        m_pf_tapset_old = pf_tapset;
        m_pf_period = pf_period;
        m_pf_gain = pf_gain;
        m_pf_tapset = pf_tapset;
    } else {
        m_pf_period_old = m_pf_period;
        m_pf_gain_old = m_pf_gain;
        m_pf_tapset_old = m_pf_tapset;
        m_pf_period = pf_period;
        m_pf_gain = pf_gain;
        m_pf_tapset = pf_tapset;
    }
}

} // namespace opus_detail
} // namespace audio
} // namespace fizmo
