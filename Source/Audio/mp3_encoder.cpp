#include "mp3_encoder.hpp"

#include "file_io.hpp"
#include "format.hpp"
#include "mp3_tables.hpp"
#include "resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <cstdlib>
#include <utility>

#ifndef FIZMO_VERSION_STRING
#define FIZMO_VERSION_STRING "0"
#endif // FIZMO_VERSION_STRING

namespace fizmo {
namespace audio {

namespace detail {
namespace mp3enc {

namespace mt = detail::mp3tables;

const double k_pi = 3.14159265358979323846;
const int k_inf = 1 << 20;
const int k_max_ix = 8206;
const int k_e_offset = 128;
const int k_e_count = 512;
const int k_lambda_max = 240;
const std::uint32_t k_encoder_delay = 528;
const std::uint32_t k_decoder_delay = 529;
const float k_abs_threshold_cap = 2.0f;

struct RateInfo {
    std::uint32_t rate;
    int version;
    int index;
};

const RateInfo k_rates[9] = {
    { 44100, 0, 0 }, { 48000, 0, 1 }, { 32000, 0, 2 },
    { 22050, 1, 0 }, { 24000, 1, 1 }, { 16000, 1, 2 },
    { 11025, 2, 0 }, { 12000, 2, 1 }, { 8000, 2, 2 },
};

const int k_bitrates[2][15] = {
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
    { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
};

const int k_sfb_long[9][23] = {
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 52, 62, 74, 90, 110, 134, 162, 196, 238, 288, 342, 418, 576 },
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 42, 50, 60, 72, 88, 106, 128, 156, 190, 230, 276, 330, 384, 576 },
    { 0, 4, 8, 12, 16, 20, 24, 30, 36, 44, 54, 66, 82, 102, 126, 156, 194, 240, 296, 364, 448, 550, 576 },
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 114, 136, 162, 194, 232, 278, 332, 394, 464, 540, 576 },
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },
    { 0, 6, 12, 18, 24, 30, 36, 44, 54, 66, 80, 96, 116, 140, 168, 200, 238, 284, 336, 396, 464, 522, 576 },
    { 0, 12, 24, 36, 48, 60, 72, 88, 108, 132, 160, 192, 232, 280, 336, 400, 476, 566, 568, 570, 572, 574, 576 },
};

const int k_sfb_short[9][14] = {
    { 0, 4, 8, 12, 16, 22, 30, 40, 52, 66, 84, 106, 136, 192 },
    { 0, 4, 8, 12, 16, 22, 28, 38, 50, 64, 80, 100, 126, 192 },
    { 0, 4, 8, 12, 16, 22, 30, 42, 58, 78, 104, 138, 180, 192 },
    { 0, 4, 8, 12, 18, 24, 32, 42, 56, 74, 100, 132, 174, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 136, 180, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192 },
    { 0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192 },
    { 0, 8, 16, 24, 36, 52, 72, 96, 124, 160, 162, 164, 166, 192 },
};

const int k_pretab[22] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0 };
const int k_slen1[16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const int k_slen2[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };
const int k_lsf_parts[3][2][4] = {
    { { 6, 5, 5, 5 }, { 9, 9, 9, 9 } },
    { { 6, 5, 7, 3 }, { 9, 9, 12, 6 } },
    { { 11, 10, 0, 0 }, { 18, 18, 0, 0 } },
};
const double k_alias[8] = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037 };
const double k_alias_leak_db[8] = { -6.8, -25.9, -27.9, -30.7, -34.0, -39.5, -46.6, -55.8 };
const double k_band_cost_base = 0.195;
const double k_band_cost_slope = 2.42;
const double k_band_cost_knee = 11500.0;
const double k_band_cost_order = 5.8;
const double k_band_cost_unpaired = 1.1;
const double k_band_quality_floor = 0.55;
const double k_band_top = 20000.0;
const double k_band_reference_rate = 44100.0;
const double k_vbr_cost_ratio[10] = { 2.43, 2.38, 2.10, 1.74, 1.44, 1.21, 1.00, 0.90, 0.63, 0.55 };
const int k_vbr_lambda[10] = { -11, -5, -1, 2, 4, 6, 8, 9, 12, 13 };

const mt::HuffmanTableSpec& huff(int t) noexcept {
    return mt::k_huffman_tables[t];
}

const int k_region_tables[15] = { 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 24 };
const int k_region_low[15] = { 1, 2, 2, 3, 3, 4, 4, 4, 6, 6, 6, 8, 8, 16, 16 };
const int k_region_cap[15] = { 1, 2, 2, 3, 3, 5, 5, 5, 7, 7, 7, 15, 15, 8206, 8206 };

double band_cost(double lowpass, double rate, double weight) noexcept {
    const int steps = 64;
    const double h = lowpass / steps;
    double area = 0.0;
    for (int i = 0; i <= steps; ++i) {
        const double f = h * i;
        const double w = 1.0 / (1.0 + std::pow(f / k_band_cost_knee, k_band_cost_order));
        area += (i == 0 || i == steps) ? 0.5 * w : w;
    }
    area *= h;
    return weight * (k_band_cost_base + k_band_cost_slope * area / (0.5 * rate));
}

double band_limit(double ratio, double rate, double weight) noexcept {
    const double top = std::min(0.48 * rate, k_band_top);
    if (ratio >= 1.0) return top;
    const double share = std::max(k_band_quality_floor, std::sqrt(std::max(ratio, 0.0)));
    const double target = band_cost(top, rate, weight) * ratio / share;
    double lo = 0.0;
    double hi = top;
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (band_cost(mid, rate, weight) < target) lo = mid;
        else hi = mid;
    }
    return std::max(lo, 0.125 * rate);
}

int popcount4(int v) noexcept {
    return (v & 1) + ((v >> 1) & 1) + ((v >> 2) & 1) + ((v >> 3) & 1);
}

int bits_for(int v) noexcept {
    int n = 0;
    while (v > 0) {
        ++n;
        v >>= 1;
    }
    return n;
}

struct Tables {
    float pow43[k_max_ix + 2];
    float thresh[k_max_ix + 2];
    float q34[k_e_count];
    float step[k_e_count];
    float analysis[512];
    float fold[32][32];
    float mdct_long[4][18][36];
    float mdct_short[6][12];
    float alias_cs[8];
    float alias_ca[8];
    std::uint8_t lens[25][256];
    std::uint8_t pair_bits[256][16];
    std::uint8_t count1[2][16];

    Tables();
};

Tables::Tables() {
    for (int i = 0; i <= k_max_ix + 1; ++i) pow43[i] = static_cast<float>(std::pow(static_cast<double>(i), 4.0 / 3.0));
    for (int i = 0; i <= k_max_ix; ++i) {
        const double a = std::pow(static_cast<double>(i), 4.0 / 3.0);
        const double b = std::pow(static_cast<double>(i + 1), 4.0 / 3.0);
        thresh[i] = static_cast<float>(std::pow(0.5 * (a + b), 0.75));
    }
    thresh[k_max_ix + 1] = std::numeric_limits<float>::max();
    for (int e = 0; e < k_e_count; ++e) {
        const double x = static_cast<double>(e - k_e_offset - 210);
        q34[e] = static_cast<float>(std::pow(2.0, -0.1875 * x));
        step[e] = static_cast<float>(std::pow(2.0, 0.25 * x));
    }
    for (int i = 0; i < 512; ++i) analysis[i] = static_cast<float>(mt::k_analysis_window[i]);
    for (int k = 0; k < 32; ++k) {
        for (int u = 0; u <= 16; ++u) fold[k][u] = static_cast<float>(std::cos((2 * k + 1) * u * k_pi / 64.0));
        for (int v = 1; v <= 15; ++v) fold[k][16 + v] = static_cast<float>(std::cos((2 * k + 1) * (32 + v) * k_pi / 64.0));
    }
    double win[4][36];
    for (int i = 0; i < 36; ++i) {
        const double l = std::sin(k_pi / 36.0 * (i + 0.5));
        win[0][i] = l;
        win[1][i] = i < 18 ? l : (i < 24 ? 1.0 : (i < 30 ? std::sin(k_pi / 12.0 * (i - 18 + 0.5)) : 0.0));
        win[2][i] = 0.0;
        win[3][i] = i < 6 ? 0.0 : (i < 12 ? std::sin(k_pi / 12.0 * (i - 6 + 0.5)) : (i < 18 ? 1.0 : l));
    }
    for (int t = 0; t < 4; ++t) {
        for (int k = 0; k < 18; ++k) {
            for (int i = 0; i < 36; ++i) {
                mdct_long[t][k][i] = static_cast<float>(win[t][i] * std::cos(k_pi / 72.0 * (2 * i + 19) * (2 * k + 1)) / 9.0);
            }
        }
    }
    for (int k = 0; k < 6; ++k) {
        for (int i = 0; i < 12; ++i) {
            mdct_short[k][i] = static_cast<float>(std::sin(k_pi / 12.0 * (i + 0.5)) * std::cos(k_pi / 24.0 * (2 * i + 7) * (2 * k + 1)) / 3.0);
        }
    }
    for (int i = 0; i < 8; ++i) {
        const double s = std::sqrt(1.0 + k_alias[i] * k_alias[i]);
        alias_cs[i] = static_cast<float>(1.0 / s);
        alias_ca[i] = static_cast<float>(k_alias[i] / s);
    }
    std::memset(lens, 0, sizeof(lens));
    for (int t = 1; t < 25; ++t) {
        const mt::HuffmanTableSpec& h = huff(t);
        if (!h.codewords || h.data_table != t) continue;
        for (int x = 0; x < h.xlen; ++x) {
            for (int y = 0; y < h.ylen; ++y) {
                const int idx = x * h.ylen + y;
                lens[t][x * 16 + y] = static_cast<std::uint8_t>(h.codewords[idx].length + (x != 0) + (y != 0));
            }
        }
    }
    for (int i = 0; i < 16; ++i) {
        count1[0][i] = static_cast<std::uint8_t>(huff(32).codewords[i].length + popcount4(i));
        count1[1][i] = static_cast<std::uint8_t>(huff(33).codewords[i].length + popcount4(i));
    }
    for (int idx = 0; idx < 256; ++idx) {
        for (int k = 0; k < 16; ++k) {
            const int t = k < 15 ? k_region_tables[k] : 0;
            pair_bits[idx][k] = t ? lens[huff(t).data_table][idx] : 0;
        }
    }
}

const Tables& tables() {
    static const Tables t;
    return t;
}

class Fft {
public:
    explicit Fft(int n) : m_n(n), m_rev(static_cast<std::size_t>(n)), m_cos(static_cast<std::size_t>(n / 2)), m_sin(static_cast<std::size_t>(n / 2)) {
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i) {
            int r = 0;
            for (int b = 0; b < bits; ++b) {
                if ((i >> b) & 1) r |= 1 << (bits - 1 - b);
            }
            m_rev[static_cast<std::size_t>(i)] = r;
        }
        for (int i = 0; i < n / 2; ++i) {
            m_cos[static_cast<std::size_t>(i)] = static_cast<float>(std::cos(2.0 * k_pi * i / n));
            m_sin[static_cast<std::size_t>(i)] = static_cast<float>(-std::sin(2.0 * k_pi * i / n));
        }
        m_half_cos.resize(static_cast<std::size_t>(n));
        m_half_sin.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            m_half_cos[static_cast<std::size_t>(i)] = static_cast<float>(std::cos(k_pi * i / n));
            m_half_sin[static_cast<std::size_t>(i)] = static_cast<float>(-std::sin(k_pi * i / n));
        }
    }

    void run_real(const float* in, float* re, float* im) const {
        const int h = m_n;
        for (int i = 0; i < h; ++i) {
            re[i] = in[2 * i];
            im[i] = in[2 * i + 1];
        }
        run(re, im);
        const int n = 2 * h;
        m_tr.resize(static_cast<std::size_t>(h + 1));
        m_ti.resize(static_cast<std::size_t>(h + 1));
        for (int k = 0; k <= h; ++k) {
            const int a = k % h;
            const int b = (h - k) % h;
            const float ar = re[a];
            const float ai = im[a];
            const float br = re[b];
            const float bi = -im[b];
            const float er = 0.5f * (ar + br);
            const float ei = 0.5f * (ai + bi);
            const float orr = 0.5f * (ai - bi);
            const float oi = -0.5f * (ar - br);
            const double ang = -2.0 * k_pi * k / n;
            const float wr = k < h ? m_half_cos[static_cast<std::size_t>(k)] : static_cast<float>(std::cos(ang));
            const float wi = k < h ? m_half_sin[static_cast<std::size_t>(k)] : static_cast<float>(std::sin(ang));
            m_tr[static_cast<std::size_t>(k)] = er + wr * orr - wi * oi;
            m_ti[static_cast<std::size_t>(k)] = ei + wr * oi + wi * orr;
        }
        for (int k = 0; k <= h; ++k) {
            re[k] = m_tr[static_cast<std::size_t>(k)];
            im[k] = m_ti[static_cast<std::size_t>(k)];
        }
    }

    void run(float* re, float* im) const {
        const int n = m_n;
        for (int i = 0; i < n; ++i) {
            const int j = m_rev[static_cast<std::size_t>(i)];
            if (j > i) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int len = 2; len <= n; len <<= 1) {
            const int half = len / 2;
            const int stride = n / len;
            for (int i = 0; i < n; i += len) {
                for (int k = 0; k < half; ++k) {
                    const float wr = m_cos[static_cast<std::size_t>(k * stride)];
                    const float wi = m_sin[static_cast<std::size_t>(k * stride)];
                    const int a = i + k;
                    const int b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                }
            }
        }
    }

private:
    int m_n;
    mutable std::vector<float> m_tr;
    mutable std::vector<float> m_ti;
    std::vector<float> m_half_cos;
    std::vector<float> m_half_sin;
    std::vector<int> m_rev;
    std::vector<float> m_cos;
    std::vector<float> m_sin;
};

double bark(double f) noexcept {
    return 13.0 * std::atan(0.00076 * f) + 3.5 * std::atan((f / 7500.0) * (f / 7500.0));
}

double ath_db(double f) noexcept {
    const double k = std::max(f, 20.0) / 1000.0;
    const double v = 3.64 * std::pow(k, -0.8) - 6.5 * std::exp(-0.6 * (k - 3.3) * (k - 3.3)) + 0.001 * k * k * k * k;
    return std::min(v, 140.0);
}

struct PsyLayout {
    int bins = 0;
    int parts = 0;
    std::vector<int> part_of;
    std::vector<int> start;
    std::vector<float> spread;
    std::vector<int> lo;
    std::vector<int> hi;
    std::vector<float> norm;
    std::vector<float> center;
    std::vector<float> inv_width;
    std::vector<std::vector<std::pair<int, float>>> bands;
};

void build_layout(PsyLayout& p, int fft, double rate, const int* edges, int band_count, double line_to_bin, double up_slope) {
    p.bins = fft / 2 + 1;
    p.part_of.assign(static_cast<std::size_t>(p.bins), 0);
    p.start.clear();
    int k = 0;
    while (k < p.bins) {
        p.start.push_back(k);
        const double z0 = bark(k * rate / fft);
        ++k;
        while (k < p.bins && bark(k * rate / fft) - z0 < 0.33) ++k;
    }
    p.parts = static_cast<int>(p.start.size());
    p.start.push_back(p.bins);
    const std::size_t n = static_cast<std::size_t>(p.parts);
    p.center.assign(n, 0.0f);
    p.inv_width.assign(n, 0.0f);
    for (int q = 0; q < p.parts; ++q) {
        const int s = p.start[static_cast<std::size_t>(q)];
        const int e = p.start[static_cast<std::size_t>(q) + 1];
        for (int b = s; b < e; ++b) p.part_of[static_cast<std::size_t>(b)] = q;
        p.center[static_cast<std::size_t>(q)] = static_cast<float>(bark(0.5 * (s + e - 1) * rate / fft));
        p.inv_width[static_cast<std::size_t>(q)] = 1.0f / static_cast<float>(e - s);
    }
    p.spread.assign(n * n, 0.0f);
    p.lo.assign(n, 0);
    p.hi.assign(n, 0);
    p.norm.assign(n, 1.0f);
    for (int i = 0; i < p.parts; ++i) {
        double sum = 0.0;
        int first = p.parts;
        int last = -1;
        for (int j = 0; j < p.parts; ++j) {
            const double dz = static_cast<double>(p.center[static_cast<std::size_t>(i)] - p.center[static_cast<std::size_t>(j)]);
            const double x = (dz > 0.0 ? dz * up_slope : dz) + 0.474;
            const double v = 15.811389 + 7.5 * x - 17.5 * std::sqrt(1.0 + x * x);
            if (v < -60.0) continue;
            const double s = std::pow(10.0, v / 10.0);
            p.spread[static_cast<std::size_t>(i) * n + static_cast<std::size_t>(j)] = static_cast<float>(s);
            sum += s;
            first = std::min(first, j);
            last = std::max(last, j);
        }
        p.lo[static_cast<std::size_t>(i)] = first;
        p.hi[static_cast<std::size_t>(i)] = last + 1;
        p.norm[static_cast<std::size_t>(i)] = sum > 0.0 ? static_cast<float>(1.0 / sum) : 1.0f;
    }
    p.bands.assign(static_cast<std::size_t>(band_count), std::vector<std::pair<int, float>>());
    for (int b = 0; b < band_count; ++b) {
        const double blo = edges[b] * line_to_bin;
        const double bhi = edges[b + 1] * line_to_bin;
        const int k0 = std::max(0, static_cast<int>(std::floor(blo - 0.5)));
        const int k1 = std::min(p.bins - 1, static_cast<int>(std::ceil(bhi + 0.5)));
        for (int kk = k0; kk <= k1; ++kk) {
            const double ov = std::min(bhi, kk + 0.5) - std::max(blo, kk - 0.5);
            if (ov > 0.0) p.bands[static_cast<std::size_t>(b)].emplace_back(kk, static_cast<float>(ov));
        }
    }
}

struct BandLayout {
    int count = 0;
    int start[39];
    int end[39];
};

struct GranuleInfo {
    int block_type = 0;
    int global_gain = 210;
    int scalefac_compress = 0;
    int scalefac_scale = 0;
    int preflag = 0;
    int subblock_gain[3] = { 0, 0, 0 };
    int table_select[3] = { 0, 0, 0 };
    int region0_count = 0;
    int region1_count = 0;
    int big_values = 0;
    int count1_end = 0;
    int count1table = 0;
    int part2_bits = 0;
    int part3_bits = 0;
    int slen[4] = { 0, 0, 0, 0 };
    int lsf_table = 0;
    int sf[39];
    int ix[576];
};

struct QuantChannel {
    float xr[576];
    float xabs[576];
    float x34[576];
    float weight[576];
    float xmin[39];
    float en[39];
    float x34max[39];
    int elo[39];
    int d[39];
    int eb[39];
    const BandLayout* layout = nullptr;
    int cache_count = 0;
    int cache_lambda[24];
    int cache_d[24][39];
    GranuleInfo gi;
};

struct Slot {
    int block_type[2] = { 0, 0 };
    float xr[2][576];
    float xmin[2][39];
    float en[2][39];
    float weight[2][576];
};

struct Channel {
    std::vector<float> pcm;
    float sb_prev[18][32];
    float sb_cur[18][32];
    std::vector<float> hist_mag[2];
    std::vector<float> hist_re[2];
    std::vector<float> hist_im[2];
    int history = 0;
    float prev_t[22];
    bool prev_long = false;
    int prev_type = 0;
    bool wants_short[2] = { false, false };
};

struct Tuning {
    double tmn = 34.0;
    double nmt = 22.0;
    double short_snr = 16.0;
    double minval_low = 18.0;
    double ath_offset = 0.0;
    double attack_ratio = 10.0;
    double up_slope = 1.5;
    int lambda_vbr = 0;
    int lambda_floor = -16;
    bool full_regions = true;
};

class BitWriter {
public:
    void put(std::uint32_t value, int bits) {
        if (bits <= 0) return;
        m_acc = (m_acc << bits) | (static_cast<std::uint64_t>(value) & ((static_cast<std::uint64_t>(1) << bits) - 1));
        m_bits += bits;
        m_total += static_cast<std::size_t>(bits);
        while (m_bits >= 8) {
            m_bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((m_acc >> m_bits) & 0xFF));
        }
    }
    void align() {
        if (m_bits > 0) put(0, 8 - m_bits);
    }
    std::size_t bit_count() const noexcept { return m_total; }
    void clear() {
        bytes.clear();
        m_acc = 0;
        m_bits = 0;
        m_total = 0;
    }

    std::vector<std::uint8_t> bytes;

private:
    std::uint64_t m_acc = 0;
    int m_bits = 0;
    std::size_t m_total = 0;
};

std::uint16_t crc16_update(std::uint16_t crc, const std::uint8_t* data, std::size_t size) noexcept {
    for (std::size_t i = 0; i < size; ++i) {
        crc = static_cast<std::uint16_t>(crc ^ data[i]);
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? static_cast<std::uint16_t>((crc >> 1) ^ 0xA001) : static_cast<std::uint16_t>(crc >> 1);
    }
    return crc;
}

int nearest_rate_index(std::uint32_t rate) noexcept {
    int best = 0;
    std::int64_t best_diff = std::numeric_limits<std::int64_t>::max();
    for (int i = 0; i < 9; ++i) {
        const std::int64_t diff = std::llabs(static_cast<std::int64_t>(k_rates[i].rate) - static_cast<std::int64_t>(rate));
        if (diff < best_diff || (diff == best_diff && k_rates[i].rate > k_rates[best].rate)) {
            best = i;
            best_diff = diff;
        }
    }
    return best;
}

int nearest_bitrate_index(int row, std::uint32_t kbps) noexcept {
    int best = 1;
    std::int64_t best_diff = std::numeric_limits<std::int64_t>::max();
    for (int i = 1; i < 15; ++i) {
        const std::int64_t diff = std::llabs(static_cast<std::int64_t>(k_bitrates[row][i]) - static_cast<std::int64_t>(kbps));
        if (diff < best_diff) {
            best = i;
            best_diff = diff;
        }
    }
    return best;
}

class Encoder {
public:
    bool setup(std::size_t channels, std::uint32_t rate, std::uint32_t source_rate, const Mp3EncodeOptions& options, std::string* error);
    void push(const float* interleaved, std::size_t frames);
    void finish();
    std::vector<std::uint8_t> info_frame() const;
    std::string smpb_text() const;
    std::size_t info_size() const noexcept { return m_info_size; }
    std::uint32_t delay() const noexcept { return k_encoder_delay; }
    std::size_t channels() const noexcept { return static_cast<std::size_t>(m_nch); }
    std::uint32_t rate() const noexcept { return m_rate; }

    std::vector<std::uint8_t> ready;

private:
    int frame_bytes(int index, int padding) const noexcept;
    void run(std::int64_t end_granule);
    void process_granule();
    bool detect_attack(const Channel& c, std::int64_t g) const;
    void analyze(Channel& c, std::int64_t g);
    void mdct(const Channel& c, int block_type, float* xr) const;
    void psy_long(Channel& c, std::int64_t g, float* ratio);
    void psy_short(Channel& c, std::int64_t g, float* ratio);
    const float* pcm_at(const Channel& c, std::int64_t pos) const noexcept;
    void encode_frame();
    bool decide_ms();
    void alias_weights(const float* xmin, float* weight) const;
    double perceptual_entropy(const float* en, const float* xmin, int type) const;
    void prepare(QuantChannel& q, const float* xr, const float* xmin, const float* en, const float* weight, int type) const;
    bool band_within(const QuantChannel& q, int b, int e, double limit) const;
    int search_step(const QuantChannel& q, int b, double t, int lo_hint, int hi_hint) const;
    int evaluate(QuantChannel& q, int lambda, bool full) const;
    int evaluate_all(QuantChannel* const* qs, int n, int lambda, bool full) const;
    void fit_long(QuantChannel& q) const;
    void fit_short(QuantChannel& q) const;
    void quantize(QuantChannel& q) const;
    int compute_part2(GranuleInfo& gi) const;
    int count_part3(GranuleInfo& gi, bool full) const;
    int range_bits(const int* ix, int a, int b, int t) const;
    int best_table(const int* ix, int a, int b, int& bits) const;
    int divide_long(GranuleInfo& gi, int bend, bool full) const;
    int divide_by_class(GranuleInfo& gi, int bend) const;
    int divide_switch(GranuleInfo& gi, int bend) const;
    int search_lambda(QuantChannel* const* qs, int n, int budget, int lo, int hi, int hint) const;
    int allocate(QuantChannel* const* qs, int n, int budget_min, int budget_max, int hard_max, int lambda_floor, int lambda_start);
    int fix_channel_limit(QuantChannel& q, int lambda) const;
    void write_part2(BitWriter& w, const GranuleInfo& gi) const;
    void write_part3(BitWriter& w, const GranuleInfo& gi, const float* xr) const;
    void write_header(BitWriter& w, int bitrate_index, int padding, bool ms) const;
    void emit_frame(const std::vector<std::uint8_t>& head, const std::vector<std::uint8_t>& main, std::size_t capacity, int resv_max_bytes);
    void flush_pending();

    const Tables* m_t = nullptr;
    int m_nch = 1;
    std::uint32_t m_rate = 44100;
    std::uint32_t m_source_rate = 44100;
    int m_rate_slot = 0;
    int m_version = 0;
    int m_rate_index = 0;
    bool m_lsf = false;
    int m_ngr = 2;
    int m_si_bytes = 32;
    int m_mode_bits = 1;
    bool m_joint = true;
    bool m_vbr = false;
    int m_bitrate_index = 11;
    int m_quality = 2;
    int m_vbr_quality = 2;
    int m_info_index = 0;
    std::size_t m_info_size = 0;
    double m_lowpass = 20000.0;
    Tuning m_tune;
    BandLayout m_long;
    BandLayout m_short;
    PsyLayout m_psy_long;
    PsyLayout m_psy_short;
    std::vector<float> m_hann_long;
    std::vector<float> m_hann_short;
    Fft m_fft_long{ 512 };
    Fft m_fft_short{ 128 };
    float m_ath_long[22];
    float m_ath_short[13];
    float m_lp_long[576];
    float m_lp_short[576];
    float m_minval_long[128];
    float m_alias_gain[8];
    Channel m_ch[2];
    std::int64_t m_origin = 0;
    std::int64_t m_end = 0;
    std::uint64_t m_samples = 0;
    std::int64_t m_granule = 0;
    Slot m_slots[2];
    int m_slot_count = 0;
    QuantChannel m_q[2][2];
    std::vector<std::uint8_t> m_pending;
    std::deque<std::pair<std::size_t, std::size_t>> m_holes;
    std::size_t m_resv_bytes = 0;
    std::int64_t m_slot_lag = 0;
    std::uint64_t m_frames = 0;
    std::uint64_t m_audio_bytes = 0;
    std::uint16_t m_music_crc = 0;
    std::vector<std::uint32_t> m_frame_sizes;
    int m_min_bitrate_used = 0;
    int m_last_lambda = 0;
    std::uint32_t m_padding_samples = 0;
    bool m_finished = false;
    std::vector<float> m_re;
    std::vector<float> m_im;
    std::vector<float> m_energy;
    std::vector<float> m_chaos;
};

int Encoder::frame_bytes(int index, int padding) const noexcept {
    const int kbps = k_bitrates[m_lsf ? 1 : 0][index];
    const std::int64_t num = static_cast<std::int64_t>(m_lsf ? 72000 : 144000) * kbps;
    return static_cast<int>(num / static_cast<std::int64_t>(m_rate)) + padding;
}

bool Encoder::setup(std::size_t channels, std::uint32_t rate, std::uint32_t source_rate, const Mp3EncodeOptions& options, std::string* error) {
    if (channels < 1 || channels > 2) {
        if (error) *error = "the MP3 encoder needs one or two channels";
        return false;
    }
    m_t = &tables();
    m_nch = static_cast<int>(channels);
    m_rate_slot = nearest_rate_index(rate);
    if (k_rates[m_rate_slot].rate != rate) {
        if (error) *error = "unsupported MP3 sample rate";
        return false;
    }
    m_rate = rate;
    m_source_rate = source_rate ? source_rate : rate;
    m_version = k_rates[m_rate_slot].version;
    m_rate_index = k_rates[m_rate_slot].index;
    m_lsf = m_version != 0;
    m_ngr = m_lsf ? 1 : 2;
    m_si_bytes = m_lsf ? (m_nch == 1 ? 9 : 17) : (m_nch == 1 ? 17 : 32);
    if (m_nch == 1) {
        m_mode_bits = 3;
        m_joint = false;
    } else if (options.mode == Mp3Mode::stereo) {
        m_mode_bits = 0;
        m_joint = false;
    } else {
        m_mode_bits = 1;
        m_joint = true;
    }
    m_vbr = options.vbr;
    m_quality = std::min(9, std::max(0, options.quality));
    m_vbr_quality = std::min(9, std::max(0, options.vbr_quality));
    m_bitrate_index = nearest_bitrate_index(m_lsf ? 1 : 0, options.bitrate_kbps);

    m_tune = Tuning();
    m_tune.full_regions = m_quality <= 4;
    m_tune.lambda_vbr = k_vbr_lambda[m_vbr_quality];
    m_tune.lambda_floor = m_quality <= 4 ? -40 : -16;

    const double nyquist = 0.5 * static_cast<double>(m_rate);
    const double rate_hz = static_cast<double>(m_rate);
    const double pairing = (m_nch == 2 && m_joint) ? 1.0 : k_band_cost_unpaired;
    if (m_vbr) {
        const double reference = band_cost(k_band_top, k_band_reference_rate, pairing);
        const double full = band_cost(std::min(0.48 * rate_hz, k_band_top), rate_hz, pairing);
        m_lowpass = band_limit(k_vbr_cost_ratio[m_vbr_quality] * reference / full, rate_hz, pairing);
    } else {
        const double kbps = k_bitrates[m_lsf ? 1 : 0][m_bitrate_index];
        const double per_sample = kbps * 1000.0 / (rate_hz * m_nch);
        const double full = band_cost(std::min(0.48 * rate_hz, k_band_top), rate_hz, pairing);
        m_lowpass = band_limit(per_sample / full, rate_hz, pairing);
    }
    m_lowpass = std::min(m_lowpass, nyquist * 0.96);

    const int* l = k_sfb_long[m_rate_slot];
    const int* s = k_sfb_short[m_rate_slot];
    m_long.count = 22;
    for (int b = 0; b < 22; ++b) {
        m_long.start[b] = l[b];
        m_long.end[b] = l[b + 1];
    }
    m_short.count = 39;
    for (int sfb = 0; sfb < 13; ++sfb) {
        const int width = s[sfb + 1] - s[sfb];
        for (int w = 0; w < 3; ++w) {
            m_short.start[sfb * 3 + w] = 3 * s[sfb] + w * width;
            m_short.end[sfb * 3 + w] = 3 * s[sfb] + (w + 1) * width;
        }
    }
    build_layout(m_psy_long, 1024, m_rate, l, 22, 1024.0 / 1152.0, m_tune.up_slope);
    build_layout(m_psy_short, 256, m_rate, s, 13, 256.0 / 384.0, m_tune.up_slope);
    for (int q = 0; q < m_psy_long.parts && q < 128; ++q) {
        const double z = m_psy_long.center[static_cast<std::size_t>(q)];
        m_minval_long[q] = static_cast<float>(std::max(0.0, m_tune.minval_low - 2.0 * z));
    }
    for (int d = 0; d < 8; ++d) m_alias_gain[d] = static_cast<float>(std::pow(10.0, -k_alias_leak_db[d] / 10.0));
    m_hann_long.resize(1024);
    m_hann_short.resize(256);
    for (int i = 0; i < 1024; ++i) m_hann_long[static_cast<std::size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * k_pi * (i + 0.5) / 1024.0));
    for (int i = 0; i < 256; ++i) m_hann_short[static_cast<std::size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * k_pi * (i + 0.5) / 256.0));

    const double rate_d = static_cast<double>(m_rate);
    for (int b = 0; b < 22; ++b) {
        double mn = 1e9;
        for (int j = l[b]; j < l[b + 1]; ++j) mn = std::min(mn, ath_db((j + 0.5) * rate_d / 1152.0));
        m_ath_long[b] = static_cast<float>(2.0 * 0.5 * std::pow(10.0, (mn - 96.0 + m_tune.ath_offset) / 10.0));
    }
    for (int sfb = 0; sfb < 13; ++sfb) {
        double mn = 1e9;
        for (int j = s[sfb]; j < s[sfb + 1]; ++j) mn = std::min(mn, ath_db((j + 0.5) * rate_d / 384.0));
        m_ath_short[sfb] = static_cast<float>(2.0 * 0.5 * std::pow(10.0, (mn - 96.0 + m_tune.ath_offset) / 10.0));
    }
    const double lp_lo = m_lowpass * 0.97;
    auto lp_gain = [&](double f) {
        if (f <= lp_lo) return 1.0f;
        if (f >= m_lowpass) return 0.0f;
        return static_cast<float>(0.5 + 0.5 * std::cos(k_pi * (f - lp_lo) / (m_lowpass - lp_lo)));
    };
    for (int j = 0; j < 576; ++j) m_lp_long[j] = lp_gain((j + 0.5) * rate_d / 1152.0);
    for (int sfb = 0; sfb < 13; ++sfb) {
        const int width = s[sfb + 1] - s[sfb];
        for (int w = 0; w < 3; ++w) {
            for (int j = 0; j < width; ++j) {
                m_lp_short[3 * s[sfb] + w * width + j] = lp_gain((s[sfb] + j + 0.5) * rate_d / 384.0);
            }
        }
    }

    const int info_need = 4 + m_si_bytes + 120 + 36;
    m_info_index = 0;
    if (!m_vbr && frame_bytes(m_bitrate_index, 0) >= info_need) {
        m_info_index = m_bitrate_index;
    } else {
        for (int i = 1; i < 15; ++i) {
            if (frame_bytes(i, 0) >= info_need) {
                m_info_index = i;
                break;
            }
        }
    }
    m_info_size = options.write_xing && m_info_index ? static_cast<std::size_t>(frame_bytes(m_info_index, 0)) : 0;

    for (int c = 0; c < 2; ++c) {
        Channel& ch = m_ch[c];
        ch.pcm.assign(2048, 0.0f);
        std::memset(ch.sb_prev, 0, sizeof(ch.sb_prev));
        std::memset(ch.sb_cur, 0, sizeof(ch.sb_cur));
        for (int h = 0; h < 2; ++h) {
            ch.hist_mag[h].assign(513, 0.0f);
            ch.hist_re[h].assign(513, 1.0f);
            ch.hist_im[h].assign(513, 0.0f);
        }
        ch.history = 0;
        std::memset(ch.prev_t, 0, sizeof(ch.prev_t));
        ch.prev_long = false;
        ch.prev_type = 0;
        ch.wants_short[0] = false;
        ch.wants_short[1] = false;
    }
    m_origin = -2048;
    m_end = 0;
    m_samples = 0;
    m_granule = 0;
    m_slot_count = 0;
    m_pending.clear();
    m_holes.clear();
    m_resv_bytes = 0;
    m_slot_lag = 0;
    m_frames = 0;
    m_audio_bytes = 0;
    m_music_crc = 0;
    m_frame_sizes.clear();
    m_min_bitrate_used = 0;
    m_last_lambda = 0;
    m_padding_samples = 0;
    m_finished = false;
    m_re.assign(1024, 0.0f);
    m_im.assign(1024, 0.0f);
    m_energy.assign(513, 0.0f);
    m_chaos.assign(513, 0.0f);
    ready.clear();
    return true;
}

const float* Encoder::pcm_at(const Channel& c, std::int64_t pos) const noexcept {
    return c.pcm.data() + (pos - m_origin);
}

void Encoder::push(const float* interleaved, std::size_t frames) {
    if (!interleaved || frames == 0 || m_finished) return;
    for (int c = 0; c < m_nch; ++c) {
        std::vector<float>& p = m_ch[c].pcm;
        const std::size_t base = p.size();
        p.resize(base + frames);
        for (std::size_t i = 0; i < frames; ++i) {
            float v = interleaved[i * static_cast<std::size_t>(m_nch) + static_cast<std::size_t>(c)];
            if (!(v == v)) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            p[base + i] = v;
        }
    }
    m_samples += frames;
    m_end += static_cast<std::int64_t>(frames);
    run(std::numeric_limits<std::int64_t>::max());
}

void Encoder::finish() {
    if (m_finished) return;
    const std::uint64_t needed = m_samples + k_encoder_delay + k_decoder_delay;
    std::uint64_t granules = (needed + 575) / 576;
    if (granules == 0) granules = 1;
    granules = (granules + static_cast<std::uint64_t>(m_ngr) - 1) / static_cast<std::uint64_t>(m_ngr) * static_cast<std::uint64_t>(m_ngr);
    m_padding_samples = static_cast<std::uint32_t>(granules * 576 - k_encoder_delay - m_samples);
    const std::int64_t want_end = static_cast<std::int64_t>(granules) * 576 + 1152;
    if (m_end < want_end) {
        const std::size_t extra = static_cast<std::size_t>(want_end - m_end);
        for (int c = 0; c < m_nch; ++c) m_ch[c].pcm.resize(m_ch[c].pcm.size() + extra, 0.0f);
        m_end = want_end;
    }
    run(static_cast<std::int64_t>(granules));
    m_holes.clear();
    m_resv_bytes = 0;
    flush_pending();
    m_finished = true;
}

void Encoder::run(std::int64_t end_granule) {
    while (m_granule < end_granule && (m_granule + 2) * 576 <= m_end) {
        process_granule();
        const std::int64_t keep_from = m_granule * 576 - 1536;
        if (keep_from - m_origin > 16384) {
            const std::size_t drop = static_cast<std::size_t>(keep_from - m_origin);
            for (int c = 0; c < m_nch; ++c) {
                std::vector<float>& p = m_ch[c].pcm;
                p.erase(p.begin(), p.begin() + static_cast<std::ptrdiff_t>(drop));
            }
            m_origin = keep_from;
        }
    }
}

bool Encoder::detect_attack(const Channel& c, std::int64_t g) const {
    const int history = 12;
    const std::int64_t first = 576 * g - 529 - history * 64;
    double e[history + 9];
    for (int k = 0; k < history + 9; ++k) {
        const float* p = pcm_at(c, first + 64 * k);
        double sum = 0.0;
        for (int i = 0; i < 64; ++i) {
            const double d = static_cast<double>(p[i]) - static_cast<double>(p[i - 1]);
            sum += d * d;
        }
        e[k] = sum;
    }
    const double floor_energy = 64.0 * 1e-7;
    double window = 0.0;
    for (int k = 0; k < history; ++k) window += e[k];
    for (int k = history; k < history + 9; ++k) {
        const double ref = std::max(window / history, floor_energy);
        if (e[k] > m_tune.attack_ratio * ref) return true;
        window += e[k] - e[k - history];
    }
    return false;
}

void Encoder::analyze(Channel& c, std::int64_t g) {
    std::memcpy(c.sb_prev, c.sb_cur, sizeof(c.sb_cur));
    const Tables& t = *m_t;
    float y[64];
    float ab[32];
    for (int i = 0; i < 18; ++i) {
        const std::int64_t newest = 32 * (18 * g + i) + 31;
        const float* p = pcm_at(c, newest);
        for (int j = 0; j < 64; ++j) {
            float acc = 0.0f;
            for (int m = 0; m < 8; ++m) acc += t.analysis[j + 64 * m] * p[-(j + 64 * m)];
            y[j] = acc;
        }
        ab[0] = y[16];
        for (int u = 1; u < 16; ++u) ab[u] = y[16 + u] + y[16 - u];
        ab[16] = y[32] + y[0];
        for (int v = 1; v < 16; ++v) ab[16 + v] = y[48 + v] - y[48 - v];
        for (int k = 0; k < 32; ++k) {
            const float* f = t.fold[k];
            float acc = 0.0f;
            for (int u = 0; u < 32; ++u) acc += f[u] * ab[u];
            c.sb_cur[i][k] = ((k & i) & 1) ? -acc : acc;
        }
    }
}

void Encoder::mdct(const Channel& c, int block_type, float* xr) const {
    const Tables& t = *m_t;
    float in[36];
    if (block_type != 2) {
        for (int sb = 0; sb < 32; ++sb) {
            for (int i = 0; i < 18; ++i) {
                in[i] = c.sb_prev[i][sb];
                in[18 + i] = c.sb_cur[i][sb];
            }
            for (int k = 0; k < 18; ++k) {
                const float* m = t.mdct_long[block_type][k];
                float acc = 0.0f;
                for (int i = 0; i < 36; ++i) acc += m[i] * in[i];
                xr[sb * 18 + k] = acc;
            }
        }
        for (int sb = 1; sb < 32; ++sb) {
            for (int i = 0; i < 8; ++i) {
                const int a = 18 * sb - 1 - i;
                const int b = 18 * sb + i;
                const float bu = xr[a];
                const float bd = xr[b];
                xr[a] = bu * t.alias_cs[i] + bd * t.alias_ca[i];
                xr[b] = bd * t.alias_cs[i] - bu * t.alias_ca[i];
            }
        }
        return;
    }
    float tmp[576];
    for (int sb = 0; sb < 32; ++sb) {
        for (int i = 0; i < 18; ++i) {
            in[i] = c.sb_prev[i][sb];
            in[18 + i] = c.sb_cur[i][sb];
        }
        for (int w = 0; w < 3; ++w) {
            for (int k = 0; k < 6; ++k) {
                const float* m = t.mdct_short[k];
                float acc = 0.0f;
                for (int i = 0; i < 12; ++i) acc += m[i] * in[6 + 6 * w + i];
                tmp[sb * 18 + 3 * k + w] = acc;
            }
        }
    }
    const int* s = k_sfb_short[m_rate_slot];
    for (int sfb = 0; sfb < 13; ++sfb) {
        const int width = s[sfb + 1] - s[sfb];
        for (int w = 0; w < 3; ++w) {
            for (int j = 0; j < width; ++j) xr[3 * s[sfb] + w * width + j] = tmp[3 * (s[sfb] + j) + w];
        }
    }
}

void Encoder::psy_long(Channel& c, std::int64_t g, float* ratio) {
    const float* p = pcm_at(c, 576 * g - 753);
    float* re = m_re.data();
    float* im = m_im.data();
    float windowed[1024];
    for (int i = 0; i < 1024; ++i) windowed[i] = m_hann_long[static_cast<std::size_t>(i)] * p[i];
    m_fft_long.run_real(windowed, re, im);
    const bool predict = c.history >= 2;
    for (int k = 0; k <= 512; ++k) {
        const float e = re[k] * re[k] + im[k] * im[k];
        const float r = std::sqrt(e);
        m_energy[static_cast<std::size_t>(k)] = e;
        float chaos = 1.0f;
        const std::size_t ku = static_cast<std::size_t>(k);
        if (predict) {
            const float r1 = c.hist_mag[0][ku];
            const float r2 = c.hist_mag[1][ku];
            const float a = c.hist_re[0][ku];
            const float b = c.hist_im[0][ku];
            const float cc = c.hist_re[1][ku];
            const float dd = c.hist_im[1][ku];
            const float pr = a * a - b * b;
            const float pi = 2.0f * a * b;
            const float ur = pr * cc + pi * dd;
            const float ui = pi * cc - pr * dd;
            const float rp = 2.0f * r1 - r2;
            const float dr = re[k] - rp * ur;
            const float di = im[k] - rp * ui;
            const float den = r + std::fabs(rp);
            chaos = den > 0.0f ? std::sqrt(dr * dr + di * di) / den : 1.0f;
        }
        m_chaos[ku] = std::min(1.0f, chaos);
        c.hist_mag[1][ku] = c.hist_mag[0][ku];
        c.hist_re[1][ku] = c.hist_re[0][ku];
        c.hist_im[1][ku] = c.hist_im[0][ku];
        c.hist_mag[0][ku] = r;
        if (r > 0.0f) {
            c.hist_re[0][ku] = re[k] / r;
            c.hist_im[0][ku] = im[k] / r;
        } else {
            c.hist_re[0][ku] = 1.0f;
            c.hist_im[0][ku] = 0.0f;
        }
    }
    if (c.history < 2) ++c.history;
    const PsyLayout& L = m_psy_long;
    const std::size_t n = static_cast<std::size_t>(L.parts);
    float eb[128];
    float cb[128];
    float tonal[128];
    float thr[128];
    for (int q = 0; q < L.parts; ++q) {
        float se = 0.0f;
        float sc = 0.0f;
        const int s0 = L.start[static_cast<std::size_t>(q)];
        const int s1 = L.start[static_cast<std::size_t>(q) + 1];
        for (int k = s0; k < s1; ++k) {
            se += m_energy[static_cast<std::size_t>(k)];
            sc += m_energy[static_cast<std::size_t>(k)] * m_chaos[static_cast<std::size_t>(k)];
        }
        eb[q] = se;
        cb[q] = sc;
        const int n0 = std::max(0, s0 - 2);
        const int n1 = std::min(L.bins, s1 + 2);
        double arith = 0.0;
        double logsum = 0.0;
        for (int k = n0; k < n1; ++k) {
            const double e = static_cast<double>(m_energy[static_cast<std::size_t>(k)]) + 1e-12;
            arith += e;
            logsum += std::log(e);
        }
        const double count = static_cast<double>(n1 - n0);
        const double sfm = 10.0 * (logsum / count - std::log(arith / count)) / std::log(10.0);
        tonal[q] = se * static_cast<float>(std::min(1.0, std::max(0.0, (sfm + 3.0) / -12.0)));
    }
    const double tmn = m_tune.tmn;
    const double nmt = m_tune.nmt;
    for (int i = 0; i < L.parts; ++i) {
        const float* row = &L.spread[static_cast<std::size_t>(i) * n];
        float ecb = 0.0f;
        float ct = 0.0f;
        float tt = 0.0f;
        for (int j = L.lo[static_cast<std::size_t>(i)]; j < L.hi[static_cast<std::size_t>(i)]; ++j) {
            ecb += row[j] * eb[j];
            ct += row[j] * cb[j];
            tt += row[j] * tonal[j];
        }
        double tb = 0.0;
        if (ecb > 0.0f) {
            const double cbb = std::max(1e-6, static_cast<double>(ct) / static_cast<double>(ecb));
            tb = std::min(1.0, std::max(0.0, -0.299 - 0.43 * std::log(cbb)));
            tb = std::max(tb, std::min(1.0, static_cast<double>(tt) / static_cast<double>(ecb)));
        }
        const double snr = std::max(static_cast<double>(m_minval_long[std::min(i, 127)]), tb * tmn + (1.0 - tb) * nmt);
        thr[i] = static_cast<float>(ecb * L.norm[static_cast<std::size_t>(i)] * std::pow(10.0, -snr / 10.0)) * L.inv_width[static_cast<std::size_t>(i)];
    }
    for (int b = 0; b < 22; ++b) {
        double ef = 0.0;
        double tf = 0.0;
        for (const auto& kw : L.bands[static_cast<std::size_t>(b)]) {
            ef += static_cast<double>(m_energy[static_cast<std::size_t>(kw.first)]) * kw.second;
            tf += static_cast<double>(thr[L.part_of[static_cast<std::size_t>(kw.first)]]) * kw.second;
        }
        ratio[b] = ef > 0.0 ? static_cast<float>(tf / ef) : 1.0f;
        ratio[22 + b] = static_cast<float>(tf * (2.0 / 576.0) / (384.0 * 1024.0 / 1152.0));
    }
}

void Encoder::psy_short(Channel& c, std::int64_t g, float* ratio) {
    const PsyLayout& L = m_psy_short;
    const std::size_t n = static_cast<std::size_t>(L.parts);
    float* re = m_re.data();
    float* im = m_im.data();
    const double snr = m_tune.short_snr;
    const double atten = std::pow(10.0, -snr / 10.0);
    for (int w = 0; w < 3; ++w) {
        const float* p = pcm_at(c, 576 * g - 561 + 192 * w);
        float windowed[256];
        for (int i = 0; i < 256; ++i) windowed[i] = m_hann_short[static_cast<std::size_t>(i)] * p[i];
        m_fft_short.run_real(windowed, re, im);
        float e[129];
        for (int k = 0; k <= 128; ++k) e[k] = re[k] * re[k] + im[k] * im[k];
        float eb[64];
        float thr[64];
        for (int q = 0; q < L.parts; ++q) {
            float se = 0.0f;
            for (int k = L.start[static_cast<std::size_t>(q)]; k < L.start[static_cast<std::size_t>(q) + 1]; ++k) se += e[k];
            eb[q] = se;
        }
        for (int i = 0; i < L.parts; ++i) {
            const float* row = &L.spread[static_cast<std::size_t>(i) * n];
            float ecb = 0.0f;
            for (int j = L.lo[static_cast<std::size_t>(i)]; j < L.hi[static_cast<std::size_t>(i)]; ++j) ecb += row[j] * eb[j];
            thr[i] = static_cast<float>(ecb * L.norm[static_cast<std::size_t>(i)] * atten) * L.inv_width[static_cast<std::size_t>(i)];
        }
        for (int sfb = 0; sfb < 13; ++sfb) {
            double ef = 0.0;
            double tf = 0.0;
            for (const auto& kw : L.bands[static_cast<std::size_t>(sfb)]) {
                ef += static_cast<double>(e[kw.first]) * kw.second;
                tf += static_cast<double>(thr[L.part_of[static_cast<std::size_t>(kw.first)]]) * kw.second;
            }
            ratio[sfb * 3 + w] = ef > 0.0 ? static_cast<float>(tf / ef) : 1.0f;
            ratio[39 + sfb * 3 + w] = static_cast<float>(tf * (2.0 / 192.0) / (96.0 * 256.0 / 384.0));
        }
    }
}

void Encoder::process_granule() {
    const std::int64_t g = m_granule;
    for (int c = 0; c < m_nch; ++c) {
        Channel& ch = m_ch[c];
        if (g == 0) ch.wants_short[0] = detect_attack(ch, 0);
        ch.wants_short[1] = detect_attack(ch, g + 1);
    }
    if (m_joint && m_nch == 2) {
        for (int k = 0; k < 2; ++k) {
            const bool any = m_ch[0].wants_short[k] || m_ch[1].wants_short[k];
            m_ch[0].wants_short[k] = any;
            m_ch[1].wants_short[k] = any;
        }
    }
    Slot& slot = m_slots[m_slot_count];
    for (int c = 0; c < m_nch; ++c) {
        Channel& ch = m_ch[c];
        int type;
        const bool prev_short = ch.prev_type == 1 || ch.prev_type == 2;
        if (ch.wants_short[0] || ch.prev_type == 1) type = 2;
        else if (ch.wants_short[1]) type = prev_short ? 2 : 1;
        else type = prev_short ? 3 : 0;
        analyze(ch, g);
        float* xr = slot.xr[c];
        mdct(ch, type, xr);
        const float* lp = type == 2 ? m_lp_short : m_lp_long;
        for (int j = 0; j < 576; ++j) xr[j] *= lp[j];
        float ratio_long[44];
        psy_long(ch, g, ratio_long);
        const BandLayout& layout = type == 2 ? m_short : m_long;
        float* en = slot.en[c];
        for (int b = 0; b < layout.count; ++b) {
            double s = 0.0;
            for (int j = layout.start[b]; j < layout.end[b]; ++j) s += static_cast<double>(xr[j]) * xr[j];
            en[b] = static_cast<float>(s);
        }
        float* xmin = slot.xmin[c];
        if (type != 2) {
            for (int b = 0; b < 22; ++b) {
                const float raw = std::min(ratio_long[b] * en[b], ratio_long[22 + b] * k_abs_threshold_cap);
                float t = raw;
                if (ch.prev_long) t = std::min(t, 2.0f * ch.prev_t[b]);
                ch.prev_t[b] = raw;
                xmin[b] = std::max(t, m_ath_long[b]);
            }
            alias_weights(xmin, slot.weight[c]);
            ch.prev_long = true;
        } else {
            float ratio_short[78];
            psy_short(ch, g, ratio_short);
            for (int b = 0; b < 39; ++b) {
                const float raw = std::min(ratio_short[b] * en[b], ratio_short[39 + b] * k_abs_threshold_cap);
                xmin[b] = std::max(raw, m_ath_short[b / 3]);
            }
            ch.prev_long = false;
            for (int j = 0; j < 576; ++j) slot.weight[c][j] = 1.0f;
        }
        slot.block_type[c] = type;
        ch.prev_type = type;
        ch.wants_short[0] = ch.wants_short[1];
    }
    ++m_slot_count;
    ++m_granule;
    if (m_slot_count == m_ngr) {
        encode_frame();
        m_slot_count = 0;
    }
}

void Encoder::alias_weights(const float* xmin, float* weight) const {
    float dens[576];
    for (int b = 0; b < 22; ++b) {
        const float d = xmin[b] / static_cast<float>(m_long.end[b] - m_long.start[b]);
        for (int j = m_long.start[b]; j < m_long.end[b]; ++j) {
            dens[j] = d;
            weight[j] = 1.0f;
        }
    }
    for (int s = 1; s < 32; ++s) {
        const int edge = 18 * s;
        for (int d = 0; d < 8; ++d) {
            const int lo = edge - 1 - d;
            const int hi = edge + d;
            const float wh = dens[hi] / std::max(1e-30f, dens[lo] * m_alias_gain[d]);
            const float wl = dens[lo] / std::max(1e-30f, dens[hi] * m_alias_gain[d]);
            weight[hi] = std::max(weight[hi], wh);
            weight[lo] = std::max(weight[lo], wl);
        }
    }
    for (int j = 0; j < 576; ++j) weight[j] = std::min(weight[j], 1e6f);
}

double Encoder::perceptual_entropy(const float* en, const float* xmin, int type) const {
    const BandLayout& layout = type == 2 ? m_short : m_long;
    double pe = 0.0;
    for (int b = 0; b < layout.count; ++b) {
        const double ratio = static_cast<double>(en[b]) / std::max(1e-30, static_cast<double>(xmin[b]));
        if (ratio > 1.0) pe += (layout.end[b] - layout.start[b]) * 0.5 * std::log2(1.0 + ratio);
    }
    return pe;
}

bool Encoder::decide_ms() {
    double lr = 0.0;
    double ms = 0.0;
    const float h = 0.70710678f;
    for (int gr = 0; gr < m_ngr; ++gr) {
        const Slot& s = m_slots[gr];
        if (s.block_type[0] != s.block_type[1]) return false;
        const int type = s.block_type[0];
        const BandLayout& layout = type == 2 ? m_short : m_long;
        lr += perceptual_entropy(s.en[0], s.xmin[0], type) + perceptual_entropy(s.en[1], s.xmin[1], type);
        float em[39];
        float es[39];
        float xm[39];
        for (int b = 0; b < layout.count; ++b) {
            double sm = 0.0;
            double ss = 0.0;
            for (int j = layout.start[b]; j < layout.end[b]; ++j) {
                const float m = (s.xr[0][j] + s.xr[1][j]) * h;
                const float d = (s.xr[0][j] - s.xr[1][j]) * h;
                sm += static_cast<double>(m) * m;
                ss += static_cast<double>(d) * d;
            }
            em[b] = static_cast<float>(sm);
            es[b] = static_cast<float>(ss);
            xm[b] = std::min(s.xmin[0][b], s.xmin[1][b]);
        }
        ms += perceptual_entropy(em, xm, type) + perceptual_entropy(es, xm, type);
    }
    return ms < lr;
}

void Encoder::prepare(QuantChannel& q, const float* xr, const float* xmin, const float* en, const float* weight, int type) const {
    q.layout = type == 2 ? &m_short : &m_long;
    q.gi.block_type = type;
    q.cache_count = 0;
    std::memcpy(q.xr, xr, sizeof(q.xr));
    std::memcpy(q.weight, weight, sizeof(q.weight));
    for (int i = 0; i < 576; ++i) {
        const float a = std::fabs(xr[i]);
        q.xabs[i] = a;
        q.x34[i] = std::sqrt(a * std::sqrt(a));
    }
    for (int b = 0; b < q.layout->count; ++b) {
        float mx = 0.0f;
        for (int j = q.layout->start[b]; j < q.layout->end[b]; ++j) mx = std::max(mx, q.x34[j]);
        q.x34max[b] = mx;
        q.xmin[b] = xmin[b];
        q.en[b] = en[b];
        if (mx > 0.0f) {
            const double e = 210.0 + (16.0 / 3.0) * std::log2(static_cast<double>(mx) / static_cast<double>(k_max_ix)) + 1.0;
            q.elo[b] = std::max(0, static_cast<int>(std::ceil(e)));
        } else {
            q.elo[b] = 0;
        }
    }
}

bool Encoder::band_within(const QuantChannel& q, int b, int e, double limit) const {
    const Tables& t = *m_t;
    const int ei = std::min(k_e_count - 1, std::max(0, e + k_e_offset));
    const float qs = t.q34[ei];
    const float st = t.step[ei];
    const float lim = static_cast<float>(limit);
    float noise = 0.0f;
    const int start = q.layout->start[b];
    const int end = q.layout->end[b];
    for (int j = start; j < end; j += 4) {
        const int stop = std::min(end, j + 4);
        for (int i = j; i < stop; ++i) {
            const float v = std::min(q.x34[i] * qs, 8207.0f);
            int ix = std::min(static_cast<int>(v), k_max_ix);
            ix += v >= t.thresh[ix] ? 1 : 0;
            ix = std::min(ix, k_max_ix);
            const float d = q.xabs[i] - t.pow43[ix] * st;
            noise += d * d * q.weight[i];
        }
        if (noise > lim) return false;
    }
    return true;
}

int Encoder::search_step(const QuantChannel& q, int b, double t, int lo_hint, int hi_hint) const {
    int lo = std::min(255, std::max(0, q.elo[b]));
    int hi = 256;
    if (lo_hint < k_inf && lo_hint > lo) lo = std::min(255, lo_hint);
    if (hi_hint < k_inf && hi_hint + 1 < hi) hi = std::max(lo + 1, hi_hint + 1);
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (band_within(q, b, mid, t)) lo = mid;
        else hi = mid;
    }
    return lo;
}

void Encoder::fit_long(QuantChannel& q) const {
    GranuleInfo& gi = q.gi;
    const int* d = q.d;
    gi.subblock_gain[0] = gi.subblock_gain[1] = gi.subblock_gain[2] = 0;
    int g = -1;
    for (int b = 0; b < 21; ++b) {
        if (d[b] < k_inf) g = std::max(g, d[b]);
    }
    if (d[21] < k_inf) g = g < 0 ? d[21] : std::min(g, d[21] + 2);
    if (g < 0) {
        gi.global_gain = 210;
        gi.scalefac_scale = 0;
        gi.preflag = 0;
        for (int b = 0; b < 22; ++b) {
            gi.sf[b] = 0;
            q.eb[b] = k_inf;
        }
        return;
    }
    int gfloor = 0;
    for (int b = 0; b < 22; ++b) {
        if (d[b] < k_inf) gfloor = std::max(gfloor, q.elo[b]);
    }
    g = std::min(255, std::max(gfloor, g));
    int coded[21];
    int raw[21];
    int use_scale = 1;
    int use_pre = 0;
    bool done = false;
    for (int attempt = 0; attempt < 300 && !done; ++attempt) {
        for (int scale = 0; scale < 2 && !done; ++scale) {
            const int k = scale ? 4 : 2;
            bool pre_useful = false;
            bool pre_waste = false;
            for (int b = 0; b < 21; ++b) {
                if (d[b] >= k_inf) {
                    raw[b] = 0;
                    continue;
                }
                const int need = g - d[b];
                int r = need > 0 ? (need + k - 1) / k : 0;
                const int room = g - q.elo[b];
                const int cap = room > 0 ? room / k : 0;
                if (r > cap) r = cap;
                raw[b] = r;
                if (b >= 11 && k_pretab[b] > 0) {
                    if (r < k_pretab[b]) pre_waste = true;
                    else pre_useful = true;
                }
            }
            auto attempt_fit = [&](int pre) {
                for (int b = 0; b < 21; ++b) {
                    int c = raw[b] - (pre && b >= 11 ? k_pretab[b] : 0);
                    if (c < 0) c = 0;
                    const int lim = (m_lsf && pre) ? (b < 11 ? 7 : 3) : (b < 11 ? 15 : 7);
                    if (c > lim) return false;
                    coded[b] = c;
                }
                return true;
            };
            const bool prefer_pre = pre_useful && !pre_waste;
            if (attempt_fit(prefer_pre ? 1 : 0)) {
                use_pre = prefer_pre ? 1 : 0;
                done = true;
            } else if (attempt_fit(prefer_pre ? 0 : 1)) {
                use_pre = prefer_pre ? 0 : 1;
                done = true;
            }
            if (done) use_scale = scale;
        }
        if (done) break;
        int gnew = g - 1;
        for (int b = 0; b < 21; ++b) {
            if (d[b] >= k_inf) continue;
            const int cand = d[b] + 4 * (b < 11 ? 15 : 7);
            if (cand < gnew) gnew = cand;
        }
        if (gnew < gfloor) break;
        g = gnew;
    }
    if (!done) {
        use_scale = 1;
        use_pre = 0;
        for (int b = 0; b < 21; ++b) coded[b] = std::min(raw[b], b < 11 ? 15 : 7);
    }
    const int k = use_scale ? 4 : 2;
    gi.global_gain = g;
    gi.scalefac_scale = use_scale;
    gi.preflag = use_pre;
    for (int b = 0; b < 21; ++b) {
        gi.sf[b] = coded[b];
        q.eb[b] = d[b] >= k_inf ? k_inf : g - k * (coded[b] + use_pre * k_pretab[b]);
    }
    gi.sf[21] = 0;
    q.eb[21] = d[21] >= k_inf ? k_inf : g;
}

void Encoder::fit_short(QuantChannel& q) const {
    GranuleInfo& gi = q.gi;
    const int* d = q.d;
    gi.preflag = 0;
    int g = -1;
    for (int b = 0; b < 36; ++b) {
        if (d[b] < k_inf) g = std::max(g, d[b]);
    }
    for (int w = 0; w < 3; ++w) {
        if (d[36 + w] < k_inf && g < 0) g = std::max(g, d[36 + w]);
    }
    if (g < 0) {
        gi.global_gain = 210;
        gi.scalefac_scale = 0;
        for (int w = 0; w < 3; ++w) gi.subblock_gain[w] = 0;
        for (int b = 0; b < 39; ++b) {
            gi.sf[b] = 0;
            q.eb[b] = k_inf;
        }
        return;
    }
    int gfloor = 0;
    for (int b = 0; b < 39; ++b) {
        if (d[b] < k_inf) gfloor = std::max(gfloor, q.elo[b]);
    }
    g = std::min(255, std::max(gfloor, g));
    int coded[36];
    int sbg[3] = { 0, 0, 0 };
    int lowest_target = k_inf;
    auto compute = [&](int gain, int k, bool force) {
        lowest_target = k_inf;
        for (int w = 0; w < 3; ++w) {
            int target = -1;
            int wfloor = 0;
            for (int sfb = 0; sfb < 12; ++sfb) {
                if (d[sfb * 3 + w] < k_inf) target = std::max(target, d[sfb * 3 + w]);
            }
            if (target < 0) target = gain;
            for (int sfb = 0; sfb < 12; ++sfb) {
                const int dv = d[sfb * 3 + w];
                if (dv < k_inf) target = std::min(target, dv + k * (sfb < 6 ? 15 : 7));
            }
            if (d[36 + w] < k_inf) target = std::min(target, d[36 + w] + 2);
            for (int sfb = 0; sfb < 13; ++sfb) {
                if (d[sfb * 3 + w] < k_inf) wfloor = std::max(wfloor, q.elo[sfb * 3 + w]);
            }
            target = std::max(target, wfloor);
            lowest_target = std::min(lowest_target, target);
            int sg = gain > target ? (gain - target + 7) / 8 : 0;
            if (sg > 7) {
                if (!force) return false;
                sg = 7;
            }
            sbg[w] = sg;
            const int base = gain - 8 * sg;
            for (int sfb = 0; sfb < 12; ++sfb) {
                const int b = sfb * 3 + w;
                coded[b] = 0;
                if (d[b] >= k_inf) continue;
                const int need = base - d[b];
                int r = need > 0 ? (need + k - 1) / k : 0;
                const int room = base - q.elo[b];
                const int cap = room > 0 ? room / k : 0;
                if (r > cap) r = cap;
                const int lim = sfb < 6 ? 15 : 7;
                if (r > lim) {
                    if (!force) return false;
                    r = lim;
                }
                coded[b] = r;
            }
        }
        return true;
    };
    int use_scale = 1;
    bool done = false;
    for (int attempt = 0; attempt < 300 && !done; ++attempt) {
        if (compute(g, 2, false)) {
            use_scale = 0;
            done = true;
            break;
        }
        if (compute(g, 4, false)) {
            use_scale = 1;
            done = true;
            break;
        }
        int gnew = g - 1;
        if (lowest_target < k_inf && lowest_target + 56 < gnew) gnew = lowest_target + 56;
        if (gnew < gfloor) break;
        g = gnew;
    }
    if (!done) {
        use_scale = 1;
        compute(g, 4, true);
    }
    const int k = use_scale ? 4 : 2;
    gi.global_gain = g;
    gi.scalefac_scale = use_scale;
    for (int w = 0; w < 3; ++w) {
        gi.subblock_gain[w] = sbg[w];
        const int base = g - 8 * sbg[w];
        for (int sfb = 0; sfb < 12; ++sfb) {
            const int b = sfb * 3 + w;
            gi.sf[b] = coded[b];
            q.eb[b] = d[b] >= k_inf ? k_inf : base - k * coded[b];
        }
        gi.sf[36 + w] = 0;
        q.eb[36 + w] = d[36 + w] >= k_inf ? k_inf : base;
    }
}

void Encoder::quantize(QuantChannel& q) const {
    const Tables& t = *m_t;
    int* ix = q.gi.ix;
    for (int b = 0; b < q.layout->count; ++b) {
        const int s = q.layout->start[b];
        const int e = q.layout->end[b];
        if (q.eb[b] >= k_inf) {
            for (int j = s; j < e; ++j) ix[j] = 0;
            continue;
        }
        const int ei = std::min(k_e_count - 1, std::max(0, q.eb[b] + k_e_offset));
        const float qs = t.q34[ei];
        for (int j = s; j < e; ++j) {
            const float v = std::min(q.x34[j] * qs, 8207.0f);
            int x = std::min(static_cast<int>(v), k_max_ix);
            x += v >= t.thresh[x] ? 1 : 0;
            ix[j] = std::min(x, k_max_ix);
        }
    }
}

int Encoder::compute_part2(GranuleInfo& gi) const {
    const bool shortb = gi.block_type == 2;
    if (!m_lsf) {
        int m1 = 0;
        int m2 = 0;
        if (shortb) {
            for (int b = 0; b < 18; ++b) m1 = std::max(m1, gi.sf[b]);
            for (int b = 18; b < 36; ++b) m2 = std::max(m2, gi.sf[b]);
        } else {
            for (int b = 0; b < 11; ++b) m1 = std::max(m1, gi.sf[b]);
            for (int b = 11; b < 21; ++b) m2 = std::max(m2, gi.sf[b]);
        }
        int best = 15;
        int best_bits = std::numeric_limits<int>::max();
        for (int c = 0; c < 16; ++c) {
            if ((1 << k_slen1[c]) <= m1 || (1 << k_slen2[c]) <= m2) continue;
            const int bits = shortb ? 18 * (k_slen1[c] + k_slen2[c]) : 11 * k_slen1[c] + 10 * k_slen2[c];
            if (bits < best_bits) {
                best_bits = bits;
                best = c;
            }
        }
        gi.scalefac_compress = best;
        gi.part2_bits = best_bits;
        return best_bits;
    }
    const int table = (!shortb && gi.preflag) ? 2 : 0;
    const int* parts = k_lsf_parts[table][shortb ? 1 : 0];
    int idx = 0;
    int bits = 0;
    for (int p = 0; p < 4; ++p) {
        int mx = 0;
        for (int n = 0; n < parts[p]; ++n) mx = std::max(mx, gi.sf[idx++]);
        gi.slen[p] = bits_for(mx);
        bits += parts[p] * gi.slen[p];
    }
    gi.lsf_table = table;
    if (table == 0) gi.scalefac_compress = (((gi.slen[0] * 5) + gi.slen[1]) << 4) + (gi.slen[2] << 2) + gi.slen[3];
    else gi.scalefac_compress = 500 + gi.slen[0] * 3 + gi.slen[1];
    gi.part2_bits = bits;
    return bits;
}

int Encoder::range_bits(const int* ix, int a, int b, int t) const {
    if (t == 0) return 0;
    const mt::HuffmanTableSpec& h = huff(t);
    const std::uint8_t* len = m_t->lens[h.data_table];
    int bits = 0;
    if (h.linbits == 0) {
        for (int i = a; i < b; i += 2) bits += len[ix[i] * 16 + ix[i + 1]];
    } else {
        for (int i = a; i < b; i += 2) {
            int x = ix[i];
            int y = ix[i + 1];
            if (x > 14) {
                x = 15;
                bits += h.linbits;
            }
            if (y > 14) {
                y = 15;
                bits += h.linbits;
            }
            bits += len[x * 16 + y];
        }
    }
    return bits;
}

int Encoder::best_table(const int* ix, int a, int b, int& bits) const {
    bits = 0;
    if (a >= b) return 0;
    int mx = 0;
    for (int i = a; i < b; ++i) mx = std::max(mx, ix[i]);
    if (mx == 0) return 0;
    int cand[3];
    int nc = 0;
    if (mx < 16) {
        if (mx == 1) {
            cand[nc++] = 1;
        } else if (mx == 2) {
            cand[nc++] = 2;
            cand[nc++] = 3;
        } else if (mx == 3) {
            cand[nc++] = 5;
            cand[nc++] = 6;
        } else if (mx <= 5) {
            cand[nc++] = 7;
            cand[nc++] = 8;
            cand[nc++] = 9;
        } else if (mx <= 7) {
            cand[nc++] = 10;
            cand[nc++] = 11;
            cand[nc++] = 12;
        } else {
            cand[nc++] = 13;
            cand[nc++] = 15;
        }
    } else {
        const int need = mx - 15;
        int t16 = 16;
        while (t16 < 23 && (1 << huff(t16).linbits) - 1 < need) ++t16;
        int t24 = 24;
        while (t24 < 31 && (1 << huff(t24).linbits) - 1 < need) ++t24;
        cand[nc++] = t16;
        cand[nc++] = t24;
    }
    int best = cand[0];
    int best_bits = range_bits(ix, a, b, cand[0]);
    for (int i = 1; i < nc; ++i) {
        const int v = range_bits(ix, a, b, cand[i]);
        if (v < best_bits) {
            best_bits = v;
            best = cand[i];
        }
    }
    bits = best_bits;
    return best;
}

int Encoder::divide_long(GranuleInfo& gi, int bend, bool full) const {
    const int* l = k_sfb_long[m_rate_slot];
    gi.table_select[0] = gi.table_select[1] = gi.table_select[2] = 0;
    gi.region0_count = 0;
    gi.region1_count = 0;
    if (bend == 0) return 0;
    if (!full) return divide_by_class(gi, bend);
    const int* ix = gi.ix;
    int nb = 0;
    while (nb < 22 && l[nb] < bend) ++nb;
    int sums[23][16];
    int escapes[23];
    int peak[22];
    for (int k = 0; k < 16; ++k) sums[0][k] = 0;
    escapes[0] = 0;
    for (int b = 0; b < nb; ++b) {
        const int from = std::min(l[b], bend);
        const int to = b + 1 == nb ? bend : std::min(l[b + 1], bend);
        std::uint16_t acc[16];
        for (int k = 0; k < 16; ++k) acc[k] = 0;
        int esc = 0;
        int m = 0;
        for (int i = from; i < to; i += 2) {
            int x = ix[i];
            int y = ix[i + 1];
            m = std::max(m, std::max(x, y));
            if (x > 14) {
                x = 15;
                ++esc;
            }
            if (y > 14) {
                y = 15;
                ++esc;
            }
            const std::uint8_t* row = m_t->pair_bits[x * 16 + y];
            for (int k = 0; k < 16; ++k) acc[k] = static_cast<std::uint16_t>(acc[k] + row[k]);
        }
        peak[b] = m;
        for (int k = 0; k < 16; ++k) sums[b + 1][k] = sums[b][k] + acc[k];
        escapes[b + 1] = escapes[b] + esc;
    }
    int memo_cost[23][23];
    int memo_table[23][23];
    int span_max[23][23];
    for (int a = 0; a < nb; ++a) {
        int m = 0;
        for (int b = a; b < nb; ++b) {
            m = std::max(m, peak[b]);
            span_max[a][b + 1] = m;
            memo_cost[a][b + 1] = -1;
        }
    }
    auto region = [&](int a, int b, int& table) {
        table = 0;
        if (a >= b) return 0;
        if (memo_cost[a][b] >= 0) {
            table = memo_table[a][b];
            return memo_cost[a][b];
        }
        const int mx = span_max[a][b];
        int best = 0;
        if (mx > 0) {
            best = std::numeric_limits<int>::max();
            const int need = mx > 15 ? bits_for(mx - 15) : 0;
            const int esc = escapes[b] - escapes[a];
            for (int k = 0; k < 15; ++k) {
                if (mx > k_region_cap[k]) continue;
                if (!full && mx < k_region_low[k]) continue;
                int t = k_region_tables[k];
                int v = sums[b][k] - sums[a][k];
                if (k >= 13) {
                    const int last = t + 7;
                    while (t < last && huff(t).linbits < need) ++t;
                    if (huff(t).linbits < need) continue;
                    v += esc * huff(t).linbits;
                }
                if (v < best) {
                    best = v;
                    table = t;
                }
            }
        }
        memo_cost[a][b] = best;
        memo_table[a][b] = table;
        return best;
    };
    int best = std::numeric_limits<int>::max();
    for (int r0 = 0; r0 < 16; ++r0) {
        const int a1 = std::min(r0 + 1, nb);
        int t0 = 0;
        const int b0 = region(0, a1, t0);
        if (b0 >= best) break;
        for (int r1 = 0; r1 < 8 && r0 + r1 + 2 <= 22; ++r1) {
            const int a2 = std::min(r0 + r1 + 2, nb);
            int t1 = 0;
            int t2 = 0;
            const int b1 = region(a1, a2, t1);
            const int b2 = region(a2, nb, t2);
            const int total = b0 + b1 + b2;
            if (total < best) {
                best = total;
                gi.region0_count = r0;
                gi.region1_count = r1;
                gi.table_select[0] = t0;
                gi.table_select[1] = t1;
                gi.table_select[2] = t2;
            }
            if (a2 >= nb) break;
        }
        if (a1 >= nb) break;
    }
    return best;
}

int value_class(int v) noexcept {
    if (v <= 3) return v;
    if (v <= 5) return 4;
    if (v <= 7) return 5;
    if (v <= 15) return 6;
    return 7;
}

int Encoder::divide_by_class(GranuleInfo& gi, int bend) const {
    const int* l = k_sfb_long[m_rate_slot];
    const int* ix = gi.ix;
    int nb = 0;
    while (nb < 22 && l[nb] < bend) ++nb;
    int cls[22];
    int pairs[23];
    pairs[nb] = 0;
    for (int b = 0; b < nb; ++b) {
        const int from = std::min(l[b], bend);
        const int to = b + 1 == nb ? bend : std::min(l[b + 1], bend);
        int m = 0;
        for (int i = from; i < to; ++i) m = std::max(m, ix[i]);
        cls[b] = value_class(m);
    }
    int tail[23];
    tail[nb] = 0;
    for (int b = nb - 1; b >= 0; --b) {
        tail[b] = std::max(tail[b + 1], cls[b]);
        pairs[b] = pairs[b + 1] + ((b + 1 == nb ? bend : std::min(l[b + 1], bend)) - std::min(l[b], bend)) / 2;
    }
    auto split = [&](int start, int first, int last) {
        int pick = last;
        int gain = 0;
        int head = 0;
        for (int j = start; j < last; ++j) {
            head = std::max(head, cls[j]);
            if (j + 1 < first) continue;
            const int g = pairs[j + 1] * (head - tail[j + 1]);
            if (g > gain) {
                gain = g;
                pick = j + 1;
            }
        }
        return pick;
    };
    const int j0 = split(0, 1, std::min(16, nb));
    const int j1 = j0 < nb ? split(j0, j0 + 1, std::min(j0 + 8, nb)) : nb;
    gi.region0_count = std::max(0, j0 - 1);
    gi.region1_count = std::max(0, std::min(7, j1 - j0 - 1));
    const int a1 = std::min(l[gi.region0_count + 1], bend);
    const int a2 = std::min(l[std::min(gi.region0_count + gi.region1_count + 2, 22)], bend);
    int b0 = 0;
    int b1 = 0;
    int b2 = 0;
    gi.table_select[0] = best_table(ix, 0, a1, b0);
    gi.table_select[1] = best_table(ix, a1, a2, b1);
    gi.table_select[2] = best_table(ix, a2, bend, b2);
    return b0 + b1 + b2;
}

int Encoder::divide_switch(GranuleInfo& gi, int bend) const {
    const int* l = k_sfb_long[m_rate_slot];
    const int* s = k_sfb_short[m_rate_slot];
    gi.table_select[2] = 0;
    gi.region0_count = 0;
    gi.region1_count = 0;
    if (bend == 0) {
        gi.table_select[0] = gi.table_select[1] = 0;
        return 0;
    }
    if (gi.block_type == 2 && m_rate == 8000) {
        int bits = 0;
        const int t = best_table(gi.ix, 0, bend, bits);
        gi.table_select[0] = gi.table_select[1] = t;
        return bits;
    }
    const int boundary = std::min(bend, gi.block_type == 2 ? 3 * s[3] : l[8]);
    int b0 = 0;
    int b1 = 0;
    gi.table_select[0] = best_table(gi.ix, 0, boundary, b0);
    gi.table_select[1] = best_table(gi.ix, boundary, bend, b1);
    return b0 + b1;
}

int Encoder::count_part3(GranuleInfo& gi, bool full) const {
    const int* ix = gi.ix;
    int i = 576;
    while (i > 1 && ix[i - 1] == 0 && ix[i - 2] == 0) i -= 2;
    gi.count1_end = i;
    int ca = 0;
    int cb = 0;
    while (i > 3) {
        const int a = ix[i - 4];
        const int b = ix[i - 3];
        const int c = ix[i - 2];
        const int d = ix[i - 1];
        if ((a | b | c | d) > 1) break;
        const int idx = a * 8 + b * 4 + c * 2 + d;
        ca += m_t->count1[0][idx];
        cb += m_t->count1[1][idx];
        i -= 4;
    }
    gi.big_values = i / 2;
    gi.count1table = cb < ca ? 1 : 0;
    int bits = std::min(ca, cb);
    if (gi.block_type == 0) bits += divide_long(gi, i, full);
    else bits += divide_switch(gi, i);
    gi.part3_bits = bits;
    return bits;
}

int Encoder::evaluate(QuantChannel& q, int lambda, bool full) const {
    const double scale = std::pow(2.0, 0.5 * lambda);
    int below = -1;
    int above = -1;
    int exact = -1;
    for (int i = 0; i < q.cache_count; ++i) {
        const int l = q.cache_lambda[i];
        if (l == lambda) exact = i;
        else if (l < lambda && (below < 0 || l > q.cache_lambda[below])) below = i;
        else if (l > lambda && (above < 0 || l < q.cache_lambda[above])) above = i;
    }
    if (exact >= 0) {
        std::memcpy(q.d, q.cache_d[exact], sizeof(q.d));
    } else {
        for (int b = 0; b < q.layout->count; ++b) {
            const double t = static_cast<double>(q.xmin[b]) * scale;
            if (static_cast<double>(q.en[b]) <= t) {
                q.d[b] = k_inf;
            } else {
                const int lo = below >= 0 ? q.cache_d[below][b] : k_inf;
                const int hi = above >= 0 ? q.cache_d[above][b] : k_inf;
                q.d[b] = search_step(q, b, t, lo, hi);
            }
        }
        const int slot = q.cache_count < 24 ? q.cache_count++ : 23;
        q.cache_lambda[slot] = lambda;
        std::memcpy(q.cache_d[slot], q.d, sizeof(q.d));
    }
    if (q.gi.block_type == 2) fit_short(q);
    else fit_long(q);
    quantize(q);
    const int p2 = compute_part2(q.gi);
    const int p3 = count_part3(q.gi, full);
    return p2 + p3;
}

int Encoder::evaluate_all(QuantChannel* const* qs, int n, int lambda, bool full) const {
    int total = 0;
    for (int i = 0; i < n; ++i) total += evaluate(*qs[i], lambda, full);
    return total;
}

int Encoder::fix_channel_limit(QuantChannel& q, int lambda) const {
    int bits = q.gi.part2_bits + q.gi.part3_bits;
    int lam = lambda;
    while (bits > 4095 && lam < k_lambda_max) {
        lam += 1;
        bits = evaluate(q, lam, m_tune.full_regions);
    }
    return lam;
}

int Encoder::search_lambda(QuantChannel* const* qs, int n, int budget, int lo, int hi, int hint) const {
    if (hi - lo <= 1) return hi;
    int h = std::min(hi - 1, std::max(lo + 1, hint));
    if (evaluate_all(qs, n, h, false) <= budget) {
        hi = h;
        int step = 1;
        while (hi - lo > 1) {
            const int probe = std::max(lo + 1, hi - step);
            if (probe >= hi) break;
            if (evaluate_all(qs, n, probe, false) <= budget) {
                hi = probe;
                step *= 2;
            } else {
                lo = probe;
                break;
            }
        }
    } else {
        lo = h;
        int step = 1;
        while (hi - lo > 1) {
            const int probe = std::min(hi - 1, lo + step);
            if (probe <= lo) break;
            if (evaluate_all(qs, n, probe, false) <= budget) {
                hi = probe;
                break;
            }
            lo = probe;
            step *= 2;
        }
    }
    while (hi - lo > 1) {
        const int mid = lo + (hi - lo) / 2;
        if (evaluate_all(qs, n, mid, false) <= budget) hi = mid;
        else lo = mid;
    }
    return hi;
}

int Encoder::allocate(QuantChannel* const* qs, int n, int budget_min, int budget_max, int hard_max, int lambda_floor, int lambda_start) {
    const int b0 = evaluate_all(qs, n, lambda_start, false);
    int budget = std::max(budget_min, std::min(b0, budget_max));
    budget = std::min(budget, hard_max);
    int lambda = lambda_start;
    if (b0 <= budget) {
        if (lambda_floor < lambda_start) lambda = search_lambda(qs, n, budget, lambda_floor - 1, lambda_start, m_last_lambda);
    } else {
        lambda = search_lambda(qs, n, budget, lambda_start, k_lambda_max, m_last_lambda);
    }
    m_last_lambda = lambda;
    int total = evaluate_all(qs, n, lambda, m_tune.full_regions);
    while (total > hard_max && lambda < k_lambda_max) {
        ++lambda;
        total = evaluate_all(qs, n, lambda, m_tune.full_regions);
    }
    total = 0;
    int lams[2] = { lambda, lambda };
    for (int i = 0; i < n; ++i) {
        lams[i] = fix_channel_limit(*qs[i], lambda);
        total += qs[i]->gi.part2_bits + qs[i]->gi.part3_bits;
    }
    if (m_quality <= 1) {
        for (int i = 0; i < n; ++i) {
            const int before = qs[i]->gi.part2_bits + qs[i]->gi.part3_bits;
            const int trial = evaluate(*qs[i], lams[i] - 1, true);
            if (trial <= 4095 && total - before + trial <= budget) total += trial - before;
            else evaluate(*qs[i], lams[i], true);
        }
    }
    return total;
}

void Encoder::write_header(BitWriter& w, int bitrate_index, int padding, bool ms) const {
    w.put(0x7FF, 11);
    w.put(static_cast<std::uint32_t>(m_version == 0 ? 3 : (m_version == 1 ? 2 : 0)), 2);
    w.put(1, 2);
    w.put(1, 1);
    w.put(static_cast<std::uint32_t>(bitrate_index), 4);
    w.put(static_cast<std::uint32_t>(m_rate_index), 2);
    w.put(static_cast<std::uint32_t>(padding), 1);
    w.put(0, 1);
    w.put(static_cast<std::uint32_t>(m_mode_bits), 2);
    w.put(ms ? 2u : 0u, 2);
    w.put(0, 1);
    w.put(1, 1);
    w.put(0, 2);
}

void Encoder::write_part2(BitWriter& w, const GranuleInfo& gi) const {
    const bool shortb = gi.block_type == 2;
    if (!m_lsf) {
        const int s1 = k_slen1[gi.scalefac_compress];
        const int s2 = k_slen2[gi.scalefac_compress];
        if (shortb) {
            for (int b = 0; b < 36; ++b) w.put(static_cast<std::uint32_t>(gi.sf[b]), b < 18 ? s1 : s2);
        } else {
            for (int b = 0; b < 21; ++b) w.put(static_cast<std::uint32_t>(gi.sf[b]), b < 11 ? s1 : s2);
        }
        return;
    }
    const int* parts = k_lsf_parts[gi.lsf_table][shortb ? 1 : 0];
    int idx = 0;
    for (int p = 0; p < 4; ++p) {
        for (int n = 0; n < parts[p]; ++n) w.put(static_cast<std::uint32_t>(gi.sf[idx++]), gi.slen[p]);
    }
}

void Encoder::write_part3(BitWriter& w, const GranuleInfo& gi, const float* xr) const {
    const int* l = k_sfb_long[m_rate_slot];
    const int* s = k_sfb_short[m_rate_slot];
    const int* ix = gi.ix;
    const int bend = gi.big_values * 2;
    int r1;
    int r2;
    if (gi.block_type == 0) {
        r1 = l[gi.region0_count + 1];
        r2 = l[std::min(gi.region0_count + gi.region1_count + 2, 22)];
    } else {
        r1 = gi.block_type == 2 ? 3 * s[3] : l[8];
        r2 = 576;
    }
    for (int i = 0; i < bend; i += 2) {
        const int t = i < r1 ? gi.table_select[0] : (i < r2 ? gi.table_select[1] : gi.table_select[2]);
        if (t == 0) continue;
        const mt::HuffmanTableSpec& h = huff(t);
        int x = ix[i];
        int y = ix[i + 1];
        int xc = x;
        int yc = y;
        if (h.linbits) {
            if (xc > 14) xc = 15;
            if (yc > 14) yc = 15;
        }
        const mt::HuffmanCodeword& code = h.codewords[xc * h.ylen + yc];
        w.put(code.code, code.length);
        if (h.linbits && x > 14) w.put(static_cast<std::uint32_t>(x - 15), h.linbits);
        if (x) w.put(xr[i] < 0.0f ? 1u : 0u, 1);
        if (h.linbits && y > 14) w.put(static_cast<std::uint32_t>(y - 15), h.linbits);
        if (y) w.put(xr[i + 1] < 0.0f ? 1u : 0u, 1);
    }
    const mt::HuffmanCodeword* c1 = huff(32 + gi.count1table).codewords;
    for (int i = bend; i < gi.count1_end; i += 4) {
        const int idx = ix[i] * 8 + ix[i + 1] * 4 + ix[i + 2] * 2 + ix[i + 3];
        const mt::HuffmanCodeword& code = c1[idx];
        w.put(code.code, code.length);
        for (int k = 0; k < 4; ++k) {
            if (ix[i + k]) w.put(xr[i + k] < 0.0f ? 1u : 0u, 1);
        }
    }
}

void Encoder::flush_pending() {
    const std::size_t final_bytes = m_holes.empty() ? m_pending.size() : m_holes.front().first;
    if (final_bytes == 0) return;
    m_music_crc = crc16_update(m_music_crc, m_pending.data(), final_bytes);
    ready.insert(ready.end(), m_pending.begin(), m_pending.begin() + static_cast<std::ptrdiff_t>(final_bytes));
    m_pending.erase(m_pending.begin(), m_pending.begin() + static_cast<std::ptrdiff_t>(final_bytes));
    for (auto& h : m_holes) h.first -= final_bytes;
}

void Encoder::emit_frame(const std::vector<std::uint8_t>& head, const std::vector<std::uint8_t>& main, std::size_t capacity, int resv_max_bytes) {
    const std::size_t pos = m_pending.size();
    m_pending.insert(m_pending.end(), head.begin(), head.end());
    m_pending.resize(pos + head.size() + capacity, 0);
    if (capacity) m_holes.emplace_back(pos + head.size(), capacity);
    std::size_t k = 0;
    while (k < main.size() && !m_holes.empty()) {
        auto& h = m_holes.front();
        const std::size_t n = std::min(h.second, main.size() - k);
        std::memcpy(&m_pending[h.first], &main[k], n);
        h.first += n;
        h.second -= n;
        k += n;
        if (h.second == 0) m_holes.pop_front();
    }
    std::size_t resv = 0;
    for (const auto& h : m_holes) resv += h.second;
    const std::size_t limit = static_cast<std::size_t>(std::max(0, resv_max_bytes));
    while (resv > limit && !m_holes.empty()) {
        auto& h = m_holes.front();
        const std::size_t n = std::min(h.second, resv - limit);
        h.first += n;
        h.second -= n;
        resv -= n;
        if (h.second == 0) m_holes.pop_front();
    }
    m_resv_bytes = resv;
    const std::size_t size = head.size() + capacity;
    m_frame_sizes.push_back(static_cast<std::uint32_t>(size));
    m_audio_bytes += size;
    ++m_frames;
    flush_pending();
}

void Encoder::encode_frame() {
    const int nch = m_nch;
    bool ms = false;
    if (m_joint && nch == 2) ms = decide_ms();
    for (int gr = 0; gr < m_ngr; ++gr) {
        Slot& s = m_slots[gr];
        if (ms) {
            const float h = 0.70710678f;
            for (int j = 0; j < 576; ++j) {
                const float a = s.xr[0][j];
                const float b = s.xr[1][j];
                s.xr[0][j] = (a + b) * h;
                s.xr[1][j] = (a - b) * h;
            }
            for (int j = 0; j < 576; ++j) {
                const float w = std::max(s.weight[0][j], s.weight[1][j]);
                s.weight[0][j] = w;
                s.weight[1][j] = w;
            }
            const BandLayout& layout = s.block_type[0] == 2 ? m_short : m_long;
            for (int b = 0; b < layout.count; ++b) {
                const float xm = std::min(s.xmin[0][b], s.xmin[1][b]);
                s.xmin[0][b] = xm;
                s.xmin[1][b] = xm;
                for (int c = 0; c < 2; ++c) {
                    double e = 0.0;
                    for (int j = layout.start[b]; j < layout.end[b]; ++j) e += static_cast<double>(s.xr[c][j]) * s.xr[c][j];
                    s.en[c][b] = static_cast<float>(e);
                }
            }
        }
        for (int c = 0; c < nch; ++c) prepare(m_q[gr][c], s.xr[c], s.xmin[c], s.en[c], s.weight[c], s.block_type[c]);
    }

    int bitrate_index = m_bitrate_index;
    int padding = 0;
    const int resv_limit_bits = m_lsf ? 255 * 8 : 511 * 8;
    int total_bits = 0;
    QuantChannel* qs[2];
    if (!m_vbr) {
        const std::int64_t num = static_cast<std::int64_t>(m_lsf ? 72000 : 144000) * k_bitrates[m_lsf ? 1 : 0][bitrate_index];
        const std::int64_t frac = num % static_cast<std::int64_t>(m_rate);
        m_slot_lag -= frac;
        if (m_slot_lag < 0) {
            m_slot_lag += static_cast<std::int64_t>(m_rate);
            padding = frac ? 1 : 0;
        }
        const int flen = frame_bytes(bitrate_index, padding);
        const int cap_bits = (flen - 4 - m_si_bytes) * 8;
        const int resv_max = std::max(0, std::min(resv_limit_bits, 7680 - flen * 8));
        int bank = static_cast<int>(m_resv_bytes) * 8;
        for (int gr = 0; gr < m_ngr; ++gr) {
            const int mean = gr + 1 < m_ngr ? cap_bits / m_ngr : cap_bits - (cap_bits / m_ngr) * (m_ngr - 1);
            const int resv_now = bank;
            const int over = std::max(0, resv_now - (resv_max * 9) / 10);
            const int bmin = mean + over;
            const int bmax = std::max(bmin, mean + std::min(resv_now, (resv_max * 6) / 10 + over));
            const int hard = std::min(mean + resv_now, 4095 * nch);
            for (int c = 0; c < nch; ++c) qs[c] = &m_q[gr][c];
            const int used = allocate(qs, nch, std::min(bmin, hard), std::min(bmax, hard), hard, m_tune.lambda_floor, 0);
            bank = bank + mean - used;
            total_bits += used;
        }
    } else {
        int needed = 0;
        for (int gr = 0; gr < m_ngr; ++gr) {
            for (int c = 0; c < nch; ++c) {
                evaluate(m_q[gr][c], m_tune.lambda_vbr, m_tune.full_regions);
                fix_channel_limit(m_q[gr][c], m_tune.lambda_vbr);
                needed += m_q[gr][c].gi.part2_bits + m_q[gr][c].gi.part3_bits;
            }
        }
        const int need_bytes = (needed + 7) / 8;
        bitrate_index = 14;
        bool fits = false;
        for (int i = 1; i < 15; ++i) {
            const int cap = frame_bytes(i, 0) - 4 - m_si_bytes;
            if (static_cast<int>(m_resv_bytes) + cap >= need_bytes) {
                bitrate_index = i;
                fits = true;
                break;
            }
        }
        total_bits = needed;
        if (!fits) {
            const int cap_bits = (frame_bytes(14, 0) - 4 - m_si_bytes) * 8;
            int bank = static_cast<int>(m_resv_bytes) * 8 + cap_bits;
            total_bits = 0;
            int remaining_need = needed;
            for (int gr = 0; gr < m_ngr; ++gr) {
                int gr_need = 0;
                for (int c = 0; c < nch; ++c) gr_need += m_q[gr][c].gi.part2_bits + m_q[gr][c].gi.part3_bits;
                const int share = remaining_need > 0 ? static_cast<int>(static_cast<std::int64_t>(bank) * gr_need / remaining_need) : bank;
                for (int c = 0; c < nch; ++c) qs[c] = &m_q[gr][c];
                const int hard = std::min(bank, 4095 * nch);
                const int target = std::min(share, hard);
                const int used = allocate(qs, nch, 0, target, hard, m_tune.lambda_vbr, m_tune.lambda_vbr);
                bank -= used;
                remaining_need -= gr_need;
                total_bits += used;
            }
        }
    }
    const int flen = frame_bytes(bitrate_index, padding);
    const int capacity = flen - 4 - m_si_bytes;
    const int resv_max_bits = std::max(0, std::min(resv_limit_bits, 7680 - flen * 8));

    BitWriter head;
    write_header(head, bitrate_index, padding, ms);
    const int main_data_begin = static_cast<int>(m_resv_bytes);
    if (!m_lsf) {
        head.put(static_cast<std::uint32_t>(main_data_begin), 9);
        head.put(0, nch == 1 ? 5 : 3);
        for (int c = 0; c < nch; ++c) head.put(0, 4);
    } else {
        head.put(static_cast<std::uint32_t>(main_data_begin), 8);
        head.put(0, nch == 1 ? 1 : 2);
    }
    BitWriter body;
    for (int gr = 0; gr < m_ngr; ++gr) {
        for (int c = 0; c < nch; ++c) {
            const GranuleInfo& gi = m_q[gr][c].gi;
            const std::size_t before = body.bit_count();
            write_part2(body, gi);
            write_part3(body, gi, m_q[gr][c].xr);
            const int length = static_cast<int>(body.bit_count() - before);
            head.put(static_cast<std::uint32_t>(length), 12);
            head.put(static_cast<std::uint32_t>(gi.big_values), 9);
            head.put(static_cast<std::uint32_t>(gi.global_gain), 8);
            head.put(static_cast<std::uint32_t>(gi.scalefac_compress), m_lsf ? 9 : 4);
            if (gi.block_type != 0) {
                head.put(1, 1);
                head.put(static_cast<std::uint32_t>(gi.block_type), 2);
                head.put(0, 1);
                head.put(static_cast<std::uint32_t>(gi.table_select[0]), 5);
                head.put(static_cast<std::uint32_t>(gi.table_select[1]), 5);
                for (int w = 0; w < 3; ++w) head.put(static_cast<std::uint32_t>(gi.subblock_gain[w]), 3);
            } else {
                head.put(0, 1);
                for (int r = 0; r < 3; ++r) head.put(static_cast<std::uint32_t>(gi.table_select[r]), 5);
                head.put(static_cast<std::uint32_t>(gi.region0_count), 4);
                head.put(static_cast<std::uint32_t>(gi.region1_count), 3);
            }
            if (!m_lsf) head.put(static_cast<std::uint32_t>(gi.preflag), 1);
            head.put(static_cast<std::uint32_t>(gi.scalefac_scale), 1);
            head.put(static_cast<std::uint32_t>(gi.count1table), 1);
        }
    }
    body.align();
    (void)total_bits;
    head.align();
    const int kbps = k_bitrates[m_lsf ? 1 : 0][bitrate_index];
    if (m_min_bitrate_used == 0 || kbps < m_min_bitrate_used) m_min_bitrate_used = kbps;
    emit_frame(head.bytes, body.bytes, static_cast<std::size_t>(capacity), resv_max_bits / 8);
}

std::string encoder_version_tag() {
    std::string v = FIZMO_VERSION_STRING;
    if (v.size() > 4) v.resize(4);
    while (!v.empty() && (v.back() == '.' || v.back() == ' ')) v.pop_back();
    std::string tag = "Fizmo" + v;
    tag.resize(9, ' ');
    return tag;
}

std::vector<std::uint8_t> Encoder::info_frame() const {
    std::vector<std::uint8_t> out;
    if (m_info_size == 0) return out;
    BitWriter head;
    write_header(head, m_info_index, 0, false);
    out = head.bytes;
    out.resize(m_info_size, 0);
    std::size_t p = 4 + static_cast<std::size_t>(m_si_bytes);
    const char* id = m_vbr ? "Xing" : "Info";
    std::memcpy(&out[p], id, 4);
    auto put_be32 = [&](std::size_t at, std::uint32_t v) {
        out[at] = static_cast<std::uint8_t>(v >> 24);
        out[at + 1] = static_cast<std::uint8_t>(v >> 16);
        out[at + 2] = static_cast<std::uint8_t>(v >> 8);
        out[at + 3] = static_cast<std::uint8_t>(v);
    };
    const std::uint64_t total = static_cast<std::uint64_t>(m_info_size) + m_audio_bytes;
    const std::uint32_t total32 = static_cast<std::uint32_t>(std::min<std::uint64_t>(total, 0xFFFFFFFFu));
    put_be32(p + 4, 0x0000000Fu);
    put_be32(p + 8, static_cast<std::uint32_t>(m_frames));
    put_be32(p + 12, total32);
    std::vector<std::uint64_t> offsets;
    offsets.reserve(m_frame_sizes.size() + 1);
    std::uint64_t acc = m_info_size;
    for (std::uint32_t s : m_frame_sizes) {
        offsets.push_back(acc);
        acc += s;
    }
    for (int i = 0; i < 100; ++i) {
        std::uint32_t v = static_cast<std::uint32_t>(i * 256 / 100);
        if (!offsets.empty() && total > 0) {
            std::size_t fi = static_cast<std::size_t>(static_cast<double>(i) / 100.0 * static_cast<double>(offsets.size()));
            fi = std::min(fi, offsets.size() - 1);
            v = static_cast<std::uint32_t>(std::min<std::uint64_t>(255, offsets[fi] * 256 / total));
        }
        out[p + 16 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v);
    }
    const int qv = m_vbr ? std::max(0, 100 - 10 * m_vbr_quality - m_quality) : std::max(0, 100 - m_quality);
    put_be32(p + 116, static_cast<std::uint32_t>(qv));
    const std::size_t l = p + 120;
    const std::string version = encoder_version_tag();
    std::memcpy(&out[l], version.data(), 9);
    out[l + 9] = static_cast<std::uint8_t>(m_vbr ? 4 : 1);
    out[l + 10] = static_cast<std::uint8_t>(std::min(255.0, std::floor(m_lowpass / 100.0 + 0.5)));
    const int kbps = m_vbr ? m_min_bitrate_used : k_bitrates[m_lsf ? 1 : 0][m_bitrate_index];
    out[l + 20] = static_cast<std::uint8_t>(std::min(255, std::max(0, kbps)));
    const std::uint32_t delay = std::min<std::uint32_t>(k_encoder_delay, 4095);
    const std::uint32_t pad = std::min<std::uint32_t>(m_padding_samples, 4095);
    out[l + 21] = static_cast<std::uint8_t>(delay >> 4);
    out[l + 22] = static_cast<std::uint8_t>(((delay & 15) << 4) | (pad >> 8));
    out[l + 23] = static_cast<std::uint8_t>(pad & 255);
    int src = 1;
    if (m_source_rate <= 32000) src = 0;
    else if (m_source_rate == 44100) src = 1;
    else if (m_source_rate == 48000) src = 2;
    else if (m_source_rate > 48000) src = 3;
    const int stereo_mode = m_nch == 1 ? 0 : (m_joint ? 3 : 1);
    out[l + 24] = static_cast<std::uint8_t>((src << 6) | (stereo_mode << 2));
    put_be32(l + 28, total32);
    out[l + 32] = static_cast<std::uint8_t>(m_music_crc >> 8);
    out[l + 33] = static_cast<std::uint8_t>(m_music_crc & 255);
    const std::uint16_t crc = crc16_update(0, out.data(), l + 34);
    out[l + 34] = static_cast<std::uint8_t>(crc >> 8);
    out[l + 35] = static_cast<std::uint8_t>(crc & 255);
    return out;
}

std::string Encoder::smpb_text() const {
    auto hex = [](std::uint64_t v, int digits) {
        static const char k_digits[] = "0123456789ABCDEF";
        std::string h(static_cast<std::size_t>(digits), '0');
        for (int i = digits - 1; i >= 0; --i) {
            h[static_cast<std::size_t>(i)] = k_digits[v & 15u];
            v >>= 4;
        }
        return h;
    };
    std::string t = " " + hex(0, 8) + " " + hex(k_encoder_delay, 8) + " " + hex(m_padding_samples, 8) + " " + hex(m_samples, 16);
    for (int i = 0; i < 8; ++i) t += " " + hex(0, 8);
    return t;
}

void append_syncsafe(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 21) & 0x7F));
    out.push_back(static_cast<std::uint8_t>((v >> 14) & 0x7F));
    out.push_back(static_cast<std::uint8_t>((v >> 7) & 0x7F));
    out.push_back(static_cast<std::uint8_t>(v & 0x7F));
}

std::string lower_ascii(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return r;
}

std::vector<std::uint8_t> build_id3(const std::vector<std::pair<std::string, std::string>>& tags, const std::string& smpb, std::size_t& smpb_at) {
    struct Map {
        const char* key;
        const char* id;
    };
    static const Map names[] = {
        { "title", "TIT2" }, { "artist", "TPE1" }, { "album", "TALB" }, { "track", "TRCK" }, { "tracknumber", "TRCK" },
        { "date", "TDRC" }, { "year", "TDRC" }, { "genre", "TCON" }, { "comment", "COMM" }, { "album_artist", "TPE2" },
        { "albumartist", "TPE2" }, { "composer", "TCOM" }, { "copyright", "TCOP" }, { "software", "TSSE" }, { "encoder", "TSSE" },
        { "disc", "TPOS" }, { "discnumber", "TPOS" }, { "bpm", "TBPM" },
    };
    std::vector<std::uint8_t> frames;
    for (const auto& tag : tags) {
        if (tag.second.empty() || tag.first.empty()) continue;
        const std::string key = lower_ascii(tag.first);
        const char* id = "TXXX";
        for (const Map& m : names) {
            if (key == m.key) {
                id = m.id;
                break;
            }
        }
        std::vector<std::uint8_t> body;
        body.push_back(3);
        if (std::strcmp(id, "COMM") == 0) {
            body.push_back('e');
            body.push_back('n');
            body.push_back('g');
            body.push_back(0);
        } else if (std::strcmp(id, "TXXX") == 0) {
            body.insert(body.end(), tag.first.begin(), tag.first.end());
            body.push_back(0);
        }
        body.insert(body.end(), tag.second.begin(), tag.second.end());
        if (body.size() >= (1u << 28)) continue;
        frames.insert(frames.end(), id, id + 4);
        append_syncsafe(frames, static_cast<std::uint32_t>(body.size()));
        frames.push_back(0);
        frames.push_back(0);
        frames.insert(frames.end(), body.begin(), body.end());
    }
    std::size_t smpb_frame_at = 0;
    if (!smpb.empty()) {
        static const char k_name[] = "iTunSMPB";
        std::vector<std::uint8_t> body;
        body.push_back(0);
        body.push_back('e');
        body.push_back('n');
        body.push_back('g');
        body.insert(body.end(), k_name, k_name + 8);
        body.push_back(0);
        smpb_frame_at = frames.size() + 10 + body.size();
        body.insert(body.end(), smpb.begin(), smpb.end());
        frames.insert(frames.end(), { 'C', 'O', 'M', 'M' });
        append_syncsafe(frames, static_cast<std::uint32_t>(body.size()));
        frames.push_back(0);
        frames.push_back(0);
        frames.insert(frames.end(), body.begin(), body.end());
    }
    std::vector<std::uint8_t> out;
    if (frames.empty() || frames.size() >= (1u << 28)) return out;
    smpb_at = smpb.empty() ? 0 : 10 + smpb_frame_at;
    out.push_back('I');
    out.push_back('D');
    out.push_back('3');
    out.push_back(4);
    out.push_back(0);
    out.push_back(0);
    append_syncsafe(out, static_cast<std::uint32_t>(frames.size()));
    out.insert(out.end(), frames.begin(), frames.end());
    return out;
}

std::vector<std::uint8_t> build_id3(const std::vector<std::pair<std::string, std::string>>& tags) {
    std::size_t unused = 0;
    return build_id3(tags, std::string(), unused);
}

} // namespace mp3enc
} // namespace detail

struct Mp3Writer::Impl {
    detail::File file;
    bool to_memory = false;
    std::vector<std::uint8_t> memory;
    detail::mp3enc::Encoder encoder;
    Resampler resampler;
    bool resampling = false;
    std::size_t in_channels = 0;
    std::uint32_t in_rate = 0;
    std::vector<float> mixed;
    std::vector<float> converted;
    std::uint64_t frames_in = 0;
    std::uint64_t info_offset = 0;
    std::size_t smpb_offset = 0;
    bool write_info = true;
    bool failed = false;
    bool open = false;
    std::string error;

    bool emit(const std::uint8_t* data, std::size_t size) {
        if (size == 0) return true;
        if (to_memory) {
            memory.insert(memory.end(), data, data + size);
            return true;
        }
        if (!file.write(data, size)) {
            failed = true;
            error = "couldn't write to the MP3 file";
            return false;
        }
        return true;
    }

    bool drain() {
        if (encoder.ready.empty()) return !failed;
        const bool ok = emit(encoder.ready.data(), encoder.ready.size());
        encoder.ready.clear();
        return ok;
    }

    bool start(const std::string* path, std::size_t channels, std::uint32_t sample_rate, const Mp3EncodeOptions& options, std::string* err) {
        auto fail = [&](const std::string& message) {
            if (err) *err = message;
            file.close();
            open = false;
            return false;
        };
        if (channels == 0) return fail("the audio has no channels");
        if (sample_rate == 0) return fail("the audio has no sample rate");
        in_channels = channels;
        in_rate = sample_rate;
        const std::size_t enc_channels = (channels == 1 || options.mode == Mp3Mode::mono) ? 1 : 2;
        const std::uint32_t out_rate = mp3_encoder_sample_rate(sample_rate);
        std::string setup_error;
        if (!encoder.setup(enc_channels, out_rate, sample_rate, options, &setup_error)) return fail(setup_error);
        resampling = out_rate != sample_rate;
        if (resampling) resampler.configure(enc_channels, static_cast<double>(sample_rate), static_cast<double>(out_rate), ResampleQuality::high);
        frames_in = 0;
        failed = false;
        memory.clear();
        if (path) {
            to_memory = false;
            if (!file.open(*path, "wb")) return fail("couldn't create the MP3 file " + *path);
        } else {
            to_memory = true;
        }
        open = true;
        write_info = options.write_xing && encoder.info_size() > 0;
        smpb_offset = 0;
        const std::vector<std::uint8_t> id3 = detail::mp3enc::build_id3(options.tags, write_info ? encoder.smpb_text() : std::string(), smpb_offset);
        if (!emit(id3.data(), id3.size())) return fail(error);
        info_offset = id3.size();
        if (write_info) {
            const std::vector<std::uint8_t> info = encoder.info_frame();
            if (!emit(info.data(), info.size())) return fail(error);
        }
        return true;
    }

    bool feed(const float* interleaved, std::size_t frames) {
        if (!open || failed) return false;
        if (!interleaved || frames == 0) return true;
        const std::size_t ec = encoder.channels();
        const std::size_t chunk = 4096;
        for (std::size_t done = 0; done < frames; done += chunk) {
            const std::size_t n = std::min(chunk, frames - done);
            const float* src = interleaved + done * in_channels;
            std::vector<float> clean(n * in_channels);
            for (std::size_t i = 0; i < n * in_channels; ++i) {
                float v = src[i];
                if (!(v == v)) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                clean[i] = v;
            }
            mixed.resize(n * ec);
            if (in_channels == ec) {
                std::memcpy(mixed.data(), clean.data(), n * ec * sizeof(float));
            } else {
                remix(clean.data(), in_channels, mixed.data(), ec, n);
            }
            if (resampling) {
                converted.clear();
                resampler.process(mixed.data(), n, converted);
                encoder.push(converted.data(), converted.size() / ec);
            } else {
                encoder.push(mixed.data(), n);
            }
            frames_in += n;
            if (!drain()) return false;
        }
        return true;
    }

    bool finish(std::string* err) {
        if (!open) {
            if (err) *err = "the MP3 writer isn't open";
            return false;
        }
        if (resampling) {
            converted.clear();
            resampler.flush(converted);
            encoder.push(converted.data(), converted.size() / encoder.channels());
        }
        encoder.finish();
        drain();
        if (write_info && !failed) {
            const std::vector<std::uint8_t> info = encoder.info_frame();
            const std::string smpb = encoder.smpb_text();
            if (to_memory) {
                if (info_offset + info.size() <= memory.size()) std::memcpy(&memory[static_cast<std::size_t>(info_offset)], info.data(), info.size());
                if (smpb_offset && smpb_offset + smpb.size() <= memory.size()) std::memcpy(&memory[smpb_offset], smpb.data(), smpb.size());
            } else {
                bool ok = file.seek(static_cast<std::int64_t>(info_offset)) && file.write(info.data(), info.size());
                if (ok && smpb_offset) ok = file.seek(static_cast<std::int64_t>(smpb_offset)) && file.write(smpb.data(), smpb.size());
                if (!ok || !file.seek(0, SEEK_END)) {
                    failed = true;
                    error = "couldn't update the MP3 header";
                }
            }
        }
        if (!to_memory) {
            if (!file.flush()) {
                failed = true;
                if (error.empty()) error = "couldn't write to the MP3 file";
            }
            file.close();
        }
        open = false;
        if (failed && err) *err = error;
        return !failed;
    }
};

std::uint32_t mp3_encoder_sample_rate(std::uint32_t input_rate) noexcept {
    return detail::mp3enc::k_rates[detail::mp3enc::nearest_rate_index(input_rate)].rate;
}

std::uint32_t mp3_encoder_bitrate(std::uint32_t sample_rate, std::uint32_t requested_kbps) noexcept {
    const int slot = detail::mp3enc::nearest_rate_index(sample_rate);
    const int row = detail::mp3enc::k_rates[slot].version == 0 ? 0 : 1;
    return static_cast<std::uint32_t>(detail::mp3enc::k_bitrates[row][detail::mp3enc::nearest_bitrate_index(row, requested_kbps)]);
}

Mp3Writer::Mp3Writer() = default;

Mp3Writer::~Mp3Writer() {
    if (m_impl && m_impl->open) m_impl->finish(nullptr);
}

Mp3Writer::Mp3Writer(Mp3Writer&&) noexcept = default;

Mp3Writer& Mp3Writer::operator=(Mp3Writer&& other) noexcept {
    if (this != &other) {
        if (m_impl && m_impl->open) m_impl->finish(nullptr);
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool Mp3Writer::open(const std::string& path, std::size_t channels, std::uint32_t sample_rate, const Mp3EncodeOptions& options, std::string* error) {
    if (m_impl && m_impl->open) m_impl->finish(nullptr);
    m_impl.reset(new Impl());
    if (!m_impl->start(&path, channels, sample_rate, options, error)) {
        m_impl.reset();
        return false;
    }
    return true;
}

bool Mp3Writer::write(const float* interleaved, std::size_t frames) {
    return m_impl && m_impl->feed(interleaved, frames);
}

bool Mp3Writer::write(const AudioBuffer& buffer) {
    if (!m_impl) return false;
    if (buffer.empty()) return true;
    if (buffer.channels() != m_impl->in_channels) return false;
    if (buffer.sample_rate() != m_impl->in_rate && buffer.sample_rate() != 0) {
        const AudioBuffer converted = buffer.resampled(m_impl->in_rate);
        return m_impl->feed(converted.data(), converted.frames());
    }
    return m_impl->feed(buffer.data(), buffer.frames());
}

bool Mp3Writer::close(std::string* error) {
    if (!m_impl) {
        if (error) *error = "the MP3 writer isn't open";
        return false;
    }
    const bool ok = m_impl->finish(error);
    m_impl.reset();
    return ok;
}

bool Mp3Writer::is_open() const noexcept {
    return m_impl && m_impl->open;
}

std::uint64_t Mp3Writer::frames_written() const noexcept {
    return m_impl ? m_impl->frames_in : 0;
}

std::uint32_t Mp3Writer::output_sample_rate() const noexcept {
    return m_impl ? m_impl->encoder.rate() : 0;
}

std::size_t Mp3Writer::output_channels() const noexcept {
    return m_impl ? m_impl->encoder.channels() : 0;
}

std::uint32_t Mp3Writer::encoder_delay() const noexcept {
    return m_impl ? m_impl->encoder.delay() : 0;
}

bool write_mp3(const std::string& path, const AudioBuffer& buffer, const Mp3EncodeOptions& options, std::string* error) {
    Mp3Writer writer;
    if (!writer.open(path, buffer.channels(), buffer.sample_rate(), options, error)) return false;
    if (!writer.write(buffer)) {
        writer.close(nullptr);
        if (error) *error = "couldn't encode the audio";
        return false;
    }
    return writer.close(error);
}

std::vector<std::uint8_t> encode_mp3(const AudioBuffer& buffer, const Mp3EncodeOptions& options) {
    std::unique_ptr<Mp3Writer::Impl> impl(new Mp3Writer::Impl());
    if (!impl->start(nullptr, buffer.channels(), buffer.sample_rate(), options, nullptr)) return {};
    if (!buffer.empty() && !impl->feed(buffer.data(), buffer.frames())) return {};
    if (!impl->finish(nullptr)) return {};
    return std::move(impl->memory);
}

} // namespace audio
} // namespace fizmo
