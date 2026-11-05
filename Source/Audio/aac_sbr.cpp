#include "aac_internal.hpp"
#include "aac_ps.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>

namespace fizmo {
namespace audio {
namespace aac_internal {

namespace {

using Cplx = std::complex<float>;

const double k_pi = 3.14159265358979323846;
const int k_slots = 32;
const int k_hf_adj = 2;
const int k_low_slots = 40;

enum FrameClass {
    fc_fixfix = 0,
    fc_fixvar = 1,
    fc_varfix = 2,
    fc_varvar = 3
};

struct QmfTables {
    float window[640];
    float window_ds[320];
    float an_re[32][64];
    float an_im[32][64];
    float sy_re[128][64];
    float sy_im[128][64];
    float sd_re[64][32];
    float sd_im[64][32];

    QmfTables() {
        for (int n = 0; n < 640; ++n) {
            const int m = n <= 320 ? n : 640 - n;
            double v = k_qmf_prototype[m];
            if (n > 320) {
                const int sa = ((n / 128) & 1) ? -1 : 1;
                const int sb = ((m / 128) & 1) ? -1 : 1;
                v *= sa * sb;
            }
            window[n] = static_cast<float>(v);
        }
        for (int n = 0; n < 320; ++n) window_ds[n] = window[2 * n];
        for (int k = 0; k < 32; ++k) {
            for (int n = 0; n < 64; ++n) {
                const double a = k_pi / 64.0 * (k + 0.5) * (2.0 * n - 0.5);
                an_re[k][n] = static_cast<float>(2.0 * std::cos(a));
                an_im[k][n] = static_cast<float>(2.0 * std::sin(a));
            }
        }
        for (int n = 0; n < 128; ++n) {
            for (int k = 0; k < 64; ++k) {
                const double a = k_pi / 128.0 * (k + 0.5) * (2.0 * n - 255.0);
                sy_re[n][k] = static_cast<float>(std::cos(a) / 64.0);
                sy_im[n][k] = static_cast<float>(std::sin(a) / 64.0);
            }
        }
        for (int n = 0; n < 64; ++n) {
            for (int k = 0; k < 32; ++k) {
                const double a = k_pi / 64.0 * (k + 0.5) * (2.0 * n - 127.5);
                sd_re[n][k] = static_cast<float>(std::cos(a) / 64.0);
                sd_im[n][k] = static_cast<float>(std::sin(a) / 64.0);
            }
        }
    }
};

const QmfTables& qmf() {
    static const QmfTables t;
    return t;
}

struct Header {
    int amp_res = 1;
    int start_freq = -1;
    int stop_freq = -1;
    int xover = 0;
    int freq_scale = 2;
    int alter_scale = 1;
    int noise_bands = 2;
    int limiter_bands = 2;
    int limiter_gains = 2;
    int interpol_freq = 1;
    int smoothing_mode = 1;
};

struct Tables {
    bool valid = false;
    int k0 = 0;
    int k2 = 0;
    int kx = 0;
    int m = 0;
    int n_master = 0;
    int n_high = 0;
    int n_low = 0;
    int n_q = 0;
    int n_lim = 0;
    int f_master[66] = {};
    int f_high[66] = {};
    int f_low[66] = {};
    int f_noise[8] = {};
    int f_lim[80] = {};
    int num_patches = 0;
    int patch_num[8] = {};
    int patch_start[8] = {};
};

struct Grid {
    int frame_class = fc_fixfix;
    int num_env = 1;
    int num_noise = 1;
    int t_env[8] = {};
    int t_noise[3] = {};
    int freq_res[8] = {};
    int pointer = 0;
    int l_a = -1;
    int amp_res = 1;
};

struct Channel {
    Grid grid;
    int df_env[8] = {};
    int df_noise[2] = {};
    int invf[8] = {};
    int env[8][64] = {};
    int noise[2][8] = {};
    int add_harmonic_flag = 0;
    int add_harmonic[64] = {};

    int env_prev[64] = {};
    int env_prev_res = 1;
    int noise_prev[8] = {};
    int invf_prev[8] = {};
    float bw_prev[8] = {};
    int s_index_prev[64] = {};
    int la_prev = -1;
    int ne_prev = 0;
    int t_end_prev = 0;
    int kx_prev = 32;
    int m_prev = 0;
    int noise_index = 0;
    int sine_index = 0;
    bool hist_valid = false;
    float g_hist[4][64] = {};
    float q_hist[4][64] = {};

    Cplx x_low[k_low_slots][32];
    Cplx y[k_low_slots][64];
    Cplx y_prev[8][64];
    Cplx xout[k_slots][64];
    Cplx ahead[6][5];
    float analysis[320] = {};
    float synthesis[1280] = {};

    void clear() {
        *this = Channel();
        std::memset(static_cast<void*>(x_low), 0, sizeof(x_low));
        std::memset(static_cast<void*>(y), 0, sizeof(y));
        std::memset(static_cast<void*>(y_prev), 0, sizeof(y_prev));
        std::memset(static_cast<void*>(xout), 0, sizeof(xout));
    }
};

int nint(double x) {
    return static_cast<int>(std::floor(x + 0.5));
}

bool build_master(const Header& h, std::uint32_t fs, Tables& t) {
    static const int off16[16] = { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int off22[16] = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13 };
    static const int off24[16] = { -5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 };
    static const int off32[16] = { -6, -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 };
    static const int off44[16] = { -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20 };
    static const int off88[16] = { -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24 };
    const int* off = fs < 22050 ? off16 : fs < 24000 ? off22 : fs < 32000 ? off24 : fs < 44100 ? off32 : fs <= 64000 ? off44 : off88;
    const double start_hz = fs < 32000 ? 3000.0 : fs < 64000 ? 4000.0 : 5000.0;
    const int start_min = nint(start_hz * 128.0 / fs);
    t.k0 = start_min + off[h.start_freq];
    int k2;
    if (h.stop_freq < 14) {
        const double stop_hz = fs < 32000 ? 6000.0 : fs < 64000 ? 8000.0 : 10000.0;
        const int stop_min = nint(stop_hz * 128.0 / fs);
        int vec[14];
        for (int i = 0; i <= 13; ++i) vec[i] = nint(stop_min * std::pow(64.0 / stop_min, i / 13.0));
        int dk[13];
        for (int i = 0; i < 13; ++i) dk[i] = vec[i + 1] - vec[i];
        std::sort(dk, dk + 13);
        k2 = stop_min;
        for (int i = 0; i < h.stop_freq; ++i) k2 += dk[i];
    } else if (h.stop_freq == 14) {
        k2 = 2 * t.k0;
    } else {
        k2 = 3 * t.k0;
    }
    k2 = std::min(k2, 64);
    t.k2 = k2;
    if (t.k0 <= 0 || k2 <= t.k0 || t.k0 >= 64) return false;
    int n = 0;
    if (h.freq_scale == 0) {
        const int dk = h.alter_scale ? 2 : 1;
        int bands = h.alter_scale ? 2 * nint((k2 - t.k0) / (dk * 2.0)) : 2 * ((k2 - t.k0) / (dk * 2));
        if (bands <= 0 || bands > 64) return false;
        int vdk[64];
        for (int i = 0; i < bands; ++i) vdk[i] = dk;
        int diff = k2 - (t.k0 + bands * dk);
        if (diff) {
            const int incr = diff < 0 ? 1 : -1;
            int k = diff < 0 ? 0 : bands - 1;
            while (diff != 0 && k >= 0 && k < bands) {
                vdk[k] -= incr;
                k += incr;
                diff += incr;
            }
        }
        t.f_master[0] = t.k0;
        for (int i = 1; i <= bands; ++i) t.f_master[i] = t.f_master[i - 1] + vdk[i - 1];
        n = bands;
    } else {
        static const int band_tab[3] = { 12, 10, 8 };
        const int bands = band_tab[h.freq_scale - 1];
        const double warp = h.alter_scale ? 1.3 : 1.0;
        const bool two = static_cast<double>(k2) / t.k0 > 2.2449;
        const int k1 = two ? 2 * t.k0 : k2;
        const int n0 = 2 * nint(bands * std::log(static_cast<double>(k1) / t.k0) / (2.0 * std::log(2.0)));
        if (n0 <= 0 || n0 > 64) return false;
        int dk0[64];
        for (int i = 0; i < n0; ++i) {
            const double r = static_cast<double>(k1) / t.k0;
            dk0[i] = nint(t.k0 * std::pow(r, (i + 1.0) / n0)) - nint(t.k0 * std::pow(r, static_cast<double>(i) / n0));
        }
        std::sort(dk0, dk0 + n0);
        if (dk0[0] <= 0) return false;
        t.f_master[0] = t.k0;
        for (int i = 1; i <= n0; ++i) t.f_master[i] = t.f_master[i - 1] + dk0[i - 1];
        n = n0;
        if (two) {
            const int n1 = 2 * nint(bands * std::log(static_cast<double>(k2) / k1) / (2.0 * std::log(2.0) * warp));
            if (n1 <= 0 || n0 + n1 > 64) return false;
            int dk1[64];
            for (int i = 0; i < n1; ++i) {
                const double r = static_cast<double>(k2) / k1;
                dk1[i] = nint(k1 * std::pow(r, (i + 1.0) / n1)) - nint(k1 * std::pow(r, static_cast<double>(i) / n1));
            }
            std::sort(dk1, dk1 + n1);
            if (dk1[0] < dk0[n0 - 1]) {
                const int change = dk0[n0 - 1] - dk1[0];
                dk1[0] += change;
                dk1[n1 - 1] -= change;
                std::sort(dk1, dk1 + n1);
            }
            if (dk1[0] <= 0) return false;
            for (int i = 1; i <= n1; ++i) t.f_master[n0 + i] = t.f_master[n0 + i - 1] + dk1[i - 1];
            n = n0 + n1;
        }
    }
    t.n_master = n;
    for (int i = 0; i < n; ++i) {
        if (t.f_master[i + 1] <= t.f_master[i]) return false;
    }
    return t.f_master[n] <= 64;
}

bool build_tables(const Header& h, std::uint32_t fs, Tables& t) {
    t = Tables();
    if (!build_master(h, fs, t)) return false;
    if (h.xover >= t.n_master) return false;
    t.n_high = t.n_master - h.xover;
    for (int i = 0; i <= t.n_high; ++i) t.f_high[i] = t.f_master[i + h.xover];
    t.n_low = t.n_high / 2 + (t.n_high & 1);
    t.f_low[0] = t.f_high[0];
    for (int i = 1; i <= t.n_low; ++i) t.f_low[i] = t.f_high[2 * i - (t.n_high & 1)];
    t.kx = t.f_high[0];
    t.m = t.f_high[t.n_high] - t.kx;
    if (t.kx > 32 || t.kx + t.m > 64 || t.m <= 0) return false;
    if (h.noise_bands == 0) {
        t.n_q = 1;
    } else {
        t.n_q = std::max(1, nint(h.noise_bands * std::log(static_cast<double>(t.k2) / t.kx) / std::log(2.0)));
    }
    if (t.n_q > 5) return false;
    t.f_noise[0] = t.f_low[0];
    int ik = 0;
    for (int k = 1; k <= t.n_q; ++k) {
        ik += (t.n_low - ik) / (t.n_q + 1 - k);
        t.f_noise[k] = t.f_low[ik];
    }
    const int goal = nint(2.048e6 / fs);
    int msb = t.k0;
    int usb = t.kx;
    int k = 0;
    if (goal < t.kx + t.m) {
        for (int i = 0; t.f_master[i] < goal; ++i) k = i + 1;
    } else {
        k = t.n_master;
    }
    int sb = 0;
    int guard = 0;
    do {
        int j = k + 1;
        int odd = 0;
        do {
            --j;
            if (j < 0) return false;
            sb = t.f_master[j];
            odd = (sb - 2 + t.k0) & 1;
        } while (sb > t.k0 - 1 + msb - odd);
        if (t.num_patches >= 6) return false;
        t.patch_num[t.num_patches] = std::max(sb - usb, 0);
        t.patch_start[t.num_patches] = t.k0 - odd - t.patch_num[t.num_patches];
        if (t.patch_num[t.num_patches] > 0) {
            usb = sb;
            msb = sb;
            ++t.num_patches;
        } else {
            msb = t.kx;
        }
        if (t.f_master[k] - sb < 3) k = t.n_master;
        if (++guard > 64) return false;
    } while (sb != t.kx + t.m);
    if (t.num_patches > 1 && t.patch_num[t.num_patches - 1] < 3) --t.num_patches;
    if (t.num_patches == 0 || t.num_patches > 5) return false;
    for (int p = 0; p < t.num_patches; ++p) {
        if (t.patch_start[p] < 0 || t.patch_start[p] + t.patch_num[p] > t.kx) return false;
    }
    if (h.limiter_bands == 0) {
        t.f_lim[0] = t.f_low[0];
        t.f_lim[1] = t.f_low[t.n_low];
        t.n_lim = 1;
    } else {
        static const double per_octave[4] = { 0.0, 1.2, 2.0, 3.0 };
        const double lb = per_octave[h.limiter_bands];
        int border[8];
        border[0] = t.kx;
        for (int p = 1; p <= t.num_patches; ++p) border[p] = border[p - 1] + t.patch_num[p - 1];
        int lim[80];
        int count = 0;
        for (int i = 0; i <= t.n_low; ++i) lim[count++] = t.f_low[i];
        for (int p = 1; p < t.num_patches; ++p) lim[count++] = border[p];
        std::sort(lim, lim + count);
        auto is_border = [&](int v) {
            for (int p = 0; p <= t.num_patches; ++p) {
                if (border[p] == v) return true;
            }
            return false;
        };
        int nr = count - 1;
        int kk = 1;
        while (kk <= nr) {
            const double octaves = std::log(static_cast<double>(lim[kk]) / lim[kk - 1]) / std::log(2.0);
            if (octaves * lb < 0.49) {
                if (lim[kk] == lim[kk - 1] || !is_border(lim[kk])) {
                    for (int i = kk; i < nr; ++i) lim[i] = lim[i + 1];
                    --nr;
                } else if (!is_border(lim[kk - 1])) {
                    for (int i = kk - 1; i < nr; ++i) lim[i] = lim[i + 1];
                    --nr;
                } else {
                    ++kk;
                }
            } else {
                ++kk;
            }
        }
        t.n_lim = nr;
        for (int i = 0; i <= nr; ++i) t.f_lim[i] = lim[i];
        if (t.n_lim <= 0) return false;
    }
    t.valid = true;
    return true;
}

int ceil_log2(int v) {
    int bits = 0;
    while ((1 << bits) < v) ++bits;
    return bits;
}

} // namespace

struct SbrElement::State {
    int channels = 1;
    std::uint32_t rate = 44100;
    bool downsampled = false;
    bool allow_ps = false;
    Header header;
    bool header_seen = false;
    Tables tables;
    bool reset_pending = true;
    bool frame_valid = false;
    int coupling = 0;
    Channel ch[2];
    PsDecoder ps;
    bool ps_used = false;
    bool ps_frame = false;

    void clear_all() {
        ch[0].clear();
        ch[1].clear();
        header = Header();
        header_seen = false;
        tables = Tables();
        reset_pending = true;
        frame_valid = false;
        coupling = 0;
        ps.reset();
        ps_used = false;
        ps_frame = false;
    }
};

namespace {

using SbrState = SbrElement::State;

bool parse_header(SbrState& s, BitReader& br) {
    Header h;
    h.amp_res = br.bit();
    h.start_freq = static_cast<int>(br.read(4));
    h.stop_freq = static_cast<int>(br.read(4));
    h.xover = static_cast<int>(br.read(3));
    br.skip(2);
    const int extra1 = br.bit();
    const int extra2 = br.bit();
    if (extra1) {
        h.freq_scale = static_cast<int>(br.read(2));
        h.alter_scale = br.bit();
        h.noise_bands = static_cast<int>(br.read(2));
    }
    if (extra2) {
        h.limiter_bands = static_cast<int>(br.read(2));
        h.limiter_gains = static_cast<int>(br.read(2));
        h.interpol_freq = br.bit();
        h.smoothing_mode = br.bit();
    }
    if (br.overrun()) return false;
    const Header& o = s.header;
    const bool changed = !s.header_seen || o.start_freq != h.start_freq || o.stop_freq != h.stop_freq || o.freq_scale != h.freq_scale || o.alter_scale != h.alter_scale || o.xover != h.xover || o.noise_bands != h.noise_bands;
    const bool limiter_changed = o.limiter_bands != h.limiter_bands;
    s.header = h;
    s.header_seen = true;
    if (changed || limiter_changed || !s.tables.valid) {
        build_tables(h, s.rate, s.tables);
        if (changed) s.reset_pending = true;
    }
    return true;
}

bool parse_grid(BitReader& br, Grid& g, int amp_res) {
    g.amp_res = amp_res;
    g.frame_class = static_cast<int>(br.read(2));
    int abs_lead = 0;
    int abs_trail = k_slots / 2;
    int n_rel_lead = 0;
    int rel_lead[8] = {};
    int rel_trail[8] = {};
    const int slots = k_slots / 2;
    switch (g.frame_class) {
    case fc_fixfix: {
        const int tmp = static_cast<int>(br.read(2));
        g.num_env = 1 << tmp;
        if (g.num_env > 4) return false;
        if (g.num_env == 1) g.amp_res = 0;
        g.freq_res[0] = br.bit();
        for (int e = 1; e < g.num_env; ++e) g.freq_res[e] = g.freq_res[0];
        abs_lead = 0;
        abs_trail = slots;
        n_rel_lead = g.num_env - 1;
        for (int i = 0; i < n_rel_lead; ++i) rel_lead[i] = nint(static_cast<double>(slots) / g.num_env);
        g.pointer = 0;
        break;
    }
    case fc_fixvar: {
        const int var1 = static_cast<int>(br.read(2));
        const int nrel = static_cast<int>(br.read(2));
        g.num_env = nrel + 1;
        for (int i = 0; i < nrel; ++i) rel_trail[i] = 2 * static_cast<int>(br.read(2)) + 2;
        g.pointer = static_cast<int>(br.read(ceil_log2(g.num_env + 1)));
        for (int e = 0; e < g.num_env; ++e) g.freq_res[g.num_env - 1 - e] = br.bit();
        abs_lead = 0;
        abs_trail = var1 + slots;
        break;
    }
    case fc_varfix: {
        const int var0 = static_cast<int>(br.read(2));
        const int nrel = static_cast<int>(br.read(2));
        g.num_env = nrel + 1;
        for (int i = 0; i < nrel; ++i) rel_lead[i] = 2 * static_cast<int>(br.read(2)) + 2;
        g.pointer = static_cast<int>(br.read(ceil_log2(g.num_env + 1)));
        for (int e = 0; e < g.num_env; ++e) g.freq_res[e] = br.bit();
        abs_lead = var0;
        abs_trail = slots;
        n_rel_lead = nrel;
        break;
    }
    default: {
        const int var0 = static_cast<int>(br.read(2));
        const int var1 = static_cast<int>(br.read(2));
        const int nrel0 = static_cast<int>(br.read(2));
        const int nrel1 = static_cast<int>(br.read(2));
        g.num_env = nrel0 + nrel1 + 1;
        if (g.num_env > 5) return false;
        for (int i = 0; i < nrel0; ++i) rel_lead[i] = 2 * static_cast<int>(br.read(2)) + 2;
        for (int i = 0; i < nrel1; ++i) rel_trail[i] = 2 * static_cast<int>(br.read(2)) + 2;
        g.pointer = static_cast<int>(br.read(ceil_log2(g.num_env + 1)));
        for (int e = 0; e < g.num_env; ++e) g.freq_res[e] = br.bit();
        abs_lead = var0;
        abs_trail = var1 + slots;
        n_rel_lead = nrel0;
        break;
    }
    }
    const int ne = g.num_env;
    if (g.pointer > ne + 1) return false;
    g.t_env[0] = abs_lead;
    g.t_env[ne] = abs_trail;
    for (int l = 1; l <= n_rel_lead && l < ne; ++l) g.t_env[l] = g.t_env[l - 1] + rel_lead[l - 1];
    for (int l = n_rel_lead + 1; l < ne; ++l) {
        int sum = 0;
        for (int i = 0; i < ne - l; ++i) sum += rel_trail[i];
        g.t_env[l] = abs_trail - sum;
    }
    for (int l = 0; l < ne; ++l) {
        if (g.t_env[l + 1] <= g.t_env[l]) return false;
    }
    if (g.t_env[ne] > slots + 3) return false;
    g.num_noise = ne > 1 ? 2 : 1;
    int middle = 0;
    if (g.frame_class == fc_fixfix) {
        middle = ne / 2;
    } else if (g.frame_class == fc_varfix) {
        middle = g.pointer == 0 ? 1 : (g.pointer == 1 ? ne - 1 : g.pointer - 1);
    } else {
        middle = g.pointer > 1 ? ne + 1 - g.pointer : ne - 1;
    }
    g.t_noise[0] = g.t_env[0];
    if (ne == 1) {
        g.t_noise[1] = g.t_env[1];
    } else {
        middle = std::max(1, std::min(middle, ne - 1));
        g.t_noise[1] = g.t_env[middle];
        g.t_noise[2] = g.t_env[ne];
    }
    if (g.frame_class == fc_fixfix || g.pointer == 0) {
        g.l_a = -1;
    } else if (g.frame_class == fc_varfix) {
        g.l_a = g.pointer - 1;
    } else {
        g.l_a = ne + 1 - g.pointer;
    }
    return !br.overrun();
}

void parse_dtdf(BitReader& br, Channel& c) {
    for (int e = 0; e < c.grid.num_env; ++e) c.df_env[e] = br.bit();
    for (int n = 0; n < c.grid.num_noise; ++n) c.df_noise[n] = br.bit();
}

void parse_invf(BitReader& br, Channel& c, int nq) {
    for (int i = 0; i < nq; ++i) c.invf[i] = static_cast<int>(br.read(2));
}

int band_count(const Tables& t, int res) {
    return res ? t.n_high : t.n_low;
}

int map_prev_band(const Tables& t, int cur_res, int prev_res, int k) {
    if (cur_res == prev_res) return k;
    if (cur_res == 1) {
        const int f = t.f_high[k];
        for (int i = 0; i < t.n_low; ++i) {
            if (t.f_low[i] <= f && f < t.f_low[i + 1]) return i;
        }
        return t.n_low - 1;
    }
    const int f = t.f_low[k];
    for (int i = 0; i <= t.n_high; ++i) {
        if (t.f_high[i] == f) return std::min(i, t.n_high - 1);
    }
    return 0;
}

bool parse_envelope(BitReader& br, Channel& c, const Tables& t, bool balance) {
    const Grid& g = c.grid;
    int start_bits;
    int t_book;
    int f_book;
    int lav;
    if (balance) {
        if (g.amp_res) {
            start_bits = 5;
            t_book = 6;
            f_book = 7;
            lav = 12;
        } else {
            start_bits = 6;
            t_book = 2;
            f_book = 3;
            lav = 24;
        }
    } else {
        if (g.amp_res) {
            start_bits = 6;
            t_book = 4;
            f_book = 5;
            lav = 31;
        } else {
            start_bits = 7;
            t_book = 0;
            f_book = 1;
            lav = 60;
        }
    }
    const Huffman& th = sbr_huffman(t_book);
    const Huffman& fh = sbr_huffman(f_book);
    const int step = balance ? 2 : 1;
    for (int e = 0; e < g.num_env; ++e) {
        const int res = g.freq_res[e];
        const int nb = band_count(t, res);
        if (c.df_env[e] == 0) {
            c.env[e][0] = step * static_cast<int>(br.read(start_bits));
            for (int k = 1; k < nb; ++k) {
                const int d = fh.decode(br);
                if (d < 0) return false;
                c.env[e][k] = c.env[e][k - 1] + step * (d - lav);
            }
        } else {
            const int* prev = e == 0 ? c.env_prev : c.env[e - 1];
            const int prev_res = e == 0 ? c.env_prev_res : g.freq_res[e - 1];
            for (int k = 0; k < nb; ++k) {
                const int d = th.decode(br);
                if (d < 0) return false;
                c.env[e][k] = prev[map_prev_band(t, res, prev_res, k)] + step * (d - lav);
            }
        }
        for (int k = 0; k < nb; ++k) c.env[e][k] = std::max(-200, std::min(c.env[e][k], 255));
    }
    return !br.overrun();
}

bool parse_noise(BitReader& br, Channel& c, const Tables& t, bool balance) {
    const Huffman& th = sbr_huffman(balance ? 9 : 8);
    const Huffman& fh = sbr_huffman(balance ? 7 : 5);
    const int lav = balance ? 12 : 31;
    const int step = balance ? 2 : 1;
    for (int n = 0; n < c.grid.num_noise; ++n) {
        if (c.df_noise[n] == 0) {
            c.noise[n][0] = step * static_cast<int>(br.read(5));
            for (int k = 1; k < t.n_q; ++k) {
                const int d = fh.decode(br);
                if (d < 0) return false;
                c.noise[n][k] = c.noise[n][k - 1] + step * (d - lav);
            }
        } else {
            const int* prev = n == 0 ? c.noise_prev : c.noise[n - 1];
            for (int k = 0; k < t.n_q; ++k) {
                const int d = th.decode(br);
                if (d < 0) return false;
                c.noise[n][k] = prev[k] + step * (d - lav);
            }
        }
        for (int k = 0; k < t.n_q; ++k) c.noise[n][k] = std::max(-100, std::min(c.noise[n][k], 100));
    }
    return !br.overrun();
}

void parse_harmonics(BitReader& br, Channel& c, const Tables& t) {
    c.add_harmonic_flag = br.bit();
    if (c.add_harmonic_flag) {
        for (int i = 0; i < t.n_high; ++i) c.add_harmonic[i] = br.bit();
    } else {
        for (int i = 0; i < 64; ++i) c.add_harmonic[i] = 0;
    }
}

bool parse_extended(SbrState& s, BitReader& br, std::size_t end) {
    if (!br.bit()) return true;
    std::size_t count = br.read(4);
    if (count == 15) count += br.read(8);
    long long bits = static_cast<long long>(count) * 8;
    if (static_cast<long long>(br.pos()) + bits > static_cast<long long>(end)) return false;
    const std::size_t stop = br.pos() + static_cast<std::size_t>(bits);
    while (bits > 7) {
        const int id = static_cast<int>(br.read(2));
        bits -= 2;
        if (id == 2 && s.allow_ps && !s.ps_frame) {
            const int used = s.ps.parse(br, static_cast<int>(bits));
            if (used < 0) {
                br.seek(stop);
                return true;
            }
            s.ps_frame = true;
            s.ps_used = true;
            bits -= used;
        } else {
            br.seek(stop);
            bits = 0;
        }
    }
    br.seek(stop);
    return true;
}

void dequantize(const SbrState& s, const Channel& c, int channel, const Channel& other, float (*e_orig)[64], float (*q_orig)[8]) {
    const Tables& t = s.tables;
    const Grid& g = c.grid;
    const bool cpl = s.coupling != 0;
    const double a = g.amp_res ? 1.0 : 2.0;
    for (int e = 0; e < g.num_env; ++e) {
        const int nb = band_count(t, g.freq_res[e]);
        for (int k = 0; k < nb; ++k) {
            double v;
            if (!cpl) {
                v = 64.0 * std::pow(2.0, c.env[e][k] / a);
            } else {
                const Channel& c0 = channel == 0 ? c : other;
                const Channel& c1 = channel == 0 ? other : c;
                const double pan = g.amp_res ? 12.0 : 24.0;
                const double base = 64.0 * std::pow(2.0, c0.env[e][k] / a + 1.0);
                const double ratio = std::pow(2.0, (pan - c1.env[e][k]) / a);
                v = channel == 0 ? base / (1.0 + ratio) : base / (1.0 + 1.0 / ratio);
            }
            e_orig[e][k] = static_cast<float>(std::min(v, 1e30));
        }
    }
    for (int n = 0; n < g.num_noise; ++n) {
        for (int k = 0; k < t.n_q; ++k) {
            double v;
            if (!cpl) {
                v = std::pow(2.0, 6.0 - c.noise[n][k]);
            } else {
                const Channel& c0 = channel == 0 ? c : other;
                const Channel& c1 = channel == 0 ? other : c;
                const double base = std::pow(2.0, 6.0 - c0.noise[n][k] + 1.0);
                const double ratio = std::pow(2.0, 12.0 - c1.noise[n][k]);
                v = channel == 0 ? base / (1.0 + ratio) : base / (1.0 + 1.0 / ratio);
            }
            q_orig[n][k] = static_cast<float>(std::min(v, 1e30));
        }
    }
}

void analysis(Channel& c, const float* in) {
    const QmfTables& q = qmf();
    float* x = c.analysis;
    for (int l = 0; l < k_slots; ++l) {
        std::memmove(x + 32, x, sizeof(float) * 288);
        for (int n = 0; n < 32; ++n) x[n] = in[l * 32 + 31 - n];
        float u[64];
        for (int n = 0; n < 64; ++n) {
            float acc = 0.0f;
            for (int j = 0; j < 5; ++j) acc += x[n + 64 * j] * q.window[2 * (n + 64 * j)];
            u[n] = acc;
        }
        Cplx* dst = c.x_low[8 + l];
        for (int k = 0; k < 32; ++k) {
            float re = 0.0f;
            float im = 0.0f;
            for (int n = 0; n < 64; ++n) {
                re += u[n] * q.an_re[k][n];
                im += u[n] * q.an_im[k][n];
            }
            dst[k] = Cplx(re, im);
        }
    }
}

void synthesis(float* state, const Cplx (*xin)[64], float* out, bool downsampled) {
    const QmfTables& q = qmf();
    if (!downsampled) {
        for (int l = 0; l < k_slots; ++l) {
            std::memmove(state + 128, state, sizeof(float) * 1152);
            const Cplx* x = xin[l];
            for (int n = 0; n < 128; ++n) {
                float acc = 0.0f;
                for (int k = 0; k < 64; ++k) acc += x[k].real() * q.sy_re[n][k] - x[k].imag() * q.sy_im[n][k];
                state[n] = acc;
            }
            float* o = out + l * 64;
            for (int k = 0; k < 64; ++k) {
                float acc = 0.0f;
                for (int j = 0; j < 5; ++j) {
                    acc += state[256 * j + k] * q.window[128 * j + k];
                    acc += state[256 * j + 192 + k] * q.window[128 * j + 64 + k];
                }
                o[k] = acc;
            }
        }
    } else {
        for (int l = 0; l < k_slots; ++l) {
            std::memmove(state + 64, state, sizeof(float) * 576);
            const Cplx* x = xin[l];
            for (int n = 0; n < 64; ++n) {
                float acc = 0.0f;
                for (int k = 0; k < 32; ++k) acc += x[k].real() * q.sd_re[n][k] - x[k].imag() * q.sd_im[n][k];
                state[n] = acc;
            }
            float* o = out + l * 32;
            for (int k = 0; k < 32; ++k) {
                float acc = 0.0f;
                for (int j = 0; j < 5; ++j) {
                    acc += state[128 * j + k] * q.window_ds[64 * j + k];
                    acc += state[128 * j + 96 + k] * q.window_ds[64 * j + 32 + k];
                }
                o[k] = acc;
            }
        }
    }
}

void hf_generate(const SbrState& s, Channel& c, Cplx (*xhigh)[64]) {
    const Tables& t = s.tables;
    const Grid& g = c.grid;
    float a0r[32] = {}, a0i[32] = {}, a1r[32] = {}, a1i[32] = {};
    for (int k = 0; k < t.kx; ++k) {
        std::complex<double> phi[3][3];
        for (int i = 0; i < 3; ++i) {
            for (int j = i; j < 3; ++j) {
                std::complex<double> acc(0.0, 0.0);
                for (int n = 0; n < k_slots + 6; ++n) {
                    const Cplx a = c.x_low[n - i + k_hf_adj][k];
                    const Cplx b = c.x_low[n - j + k_hf_adj][k];
                    acc += std::complex<double>(a.real(), a.imag()) * std::conj(std::complex<double>(b.real(), b.imag()));
                }
                phi[i][j] = acc;
            }
        }
        const double d = std::real(phi[2][2]) * std::real(phi[1][1]) - std::norm(phi[1][2]) / (1.0 + 1e-6);
        std::complex<double> alpha1(0.0, 0.0);
        std::complex<double> alpha0(0.0, 0.0);
        if (d != 0.0) alpha1 = (phi[0][1] * phi[1][2] - phi[0][2] * phi[1][1]) / d;
        if (std::real(phi[1][1]) != 0.0) alpha0 = -(phi[0][1] + alpha1 * std::conj(phi[1][2])) / std::real(phi[1][1]);
        if (std::abs(alpha0) >= 4.0 || std::abs(alpha1) >= 4.0) {
            alpha0 = 0.0;
            alpha1 = 0.0;
        }
        a0r[k] = static_cast<float>(alpha0.real());
        a0i[k] = static_cast<float>(alpha0.imag());
        a1r[k] = static_cast<float>(alpha1.real());
        a1i[k] = static_cast<float>(alpha1.imag());
    }
    float bw[8];
    for (int i = 0; i < t.n_q; ++i) {
        const int mode = c.invf[i];
        const int prev = c.invf_prev[i];
        double nb;
        if (mode == 0) nb = prev == 1 ? 0.6 : 0.0;
        else if (mode == 1) nb = prev == 0 ? 0.6 : 0.75;
        else if (mode == 2) nb = 0.9;
        else nb = 0.98;
        double b = nb < c.bw_prev[i] ? 0.75 * nb + 0.25 * c.bw_prev[i] : 0.90625 * nb + 0.09375 * c.bw_prev[i];
        if (b < 0.015625) b = 0.0;
        if (b > 0.99609375) b = 0.99609375;
        bw[i] = static_cast<float>(b);
    }
    const int l0 = 2 * g.t_env[0];
    const int l1 = 2 * g.t_env[g.num_env];
    int k = t.kx;
    for (int p = 0; p < t.num_patches; ++p) {
        for (int q = 0; q < t.patch_num[p]; ++q, ++k) {
            const int src = t.patch_start[p] + q;
            int gi = 0;
            while (gi + 1 < t.n_q && k >= t.f_noise[gi + 1]) ++gi;
            const float b = bw[gi];
            const Cplx al0(a0r[src] * b, a0i[src] * b);
            const Cplx al1(a1r[src] * b * b, a1i[src] * b * b);
            for (int l = l0; l < l1; ++l) {
                xhigh[l][k] = c.x_low[l + k_hf_adj][src] + al0 * c.x_low[l + k_hf_adj - 1][src] + al1 * c.x_low[l + k_hf_adj - 2][src];
            }
        }
    }
    for (int i = 0; i < t.n_q; ++i) {
        c.bw_prev[i] = bw[i];
        c.invf_prev[i] = c.invf[i];
    }
}

void hf_adjust(SbrState& s, Channel& c, const Cplx (*xhigh)[64], const float (*e_orig)[64], const float (*q_orig)[8], bool reset) {
    const Tables& t = s.tables;
    const Grid& g = c.grid;
    const Header& h = s.header;
    const int kx = t.kx;
    const int m_count = t.m;
    const int ne = g.num_env;
    const int la_prev = c.la_prev == c.ne_prev ? 0 : -1;
    static const float lim_gain[4] = { 0.70795f, 1.0f, 1.41254f, 1e10f };
    static const float smooth[5] = { 0.33333333333333f, 0.30150283239582f, 0.21816949906249f, 0.11516383427084f, 0.03183050093751f };
    const double eps = 1.0;
    const double eps0 = 1e-12;
    float g_lim[8][64];
    float q_lim[8][64];
    float s_m[8][64];
    int s_index[64];
    for (int i = 0; i < 64; ++i) s_index[i] = c.s_index_prev[i];
    for (int l = 0; l < ne; ++l) {
        const int res = g.freq_res[l];
        const int* F = res ? t.f_high : t.f_low;
        const int nb = band_count(t, res);
        double eo[64], qo[64], ec[64];
        int sim[64] = {};
        int smap[64] = {};
        for (int i = 0; i < nb; ++i) {
            for (int m = F[i]; m < F[i + 1]; ++m) eo[m - kx] = e_orig[l][i];
        }
        int qi = 0;
        while (qi + 1 < g.num_noise && g.t_noise[qi + 1] <= g.t_env[l]) ++qi;
        for (int i = 0; i < t.n_q; ++i) {
            for (int m = t.f_noise[i]; m < t.f_noise[i + 1]; ++m) qo[m - kx] = q_orig[qi][i];
        }
        if (c.add_harmonic_flag) {
            for (int i = 0; i < t.n_high; ++i) {
                if (!c.add_harmonic[i]) continue;
                const int mm = ((t.f_high[i + 1] + t.f_high[i]) >> 1) - kx;
                if (mm < 0 || mm >= m_count) continue;
                if (l >= g.l_a || c.s_index_prev[mm]) sim[mm] = 1;
            }
        }
        for (int i = 0; i < nb; ++i) {
            int any = 0;
            for (int m = F[i]; m < F[i + 1]; ++m) any |= sim[m - kx];
            for (int m = F[i]; m < F[i + 1]; ++m) smap[m - kx] = any;
        }
        const int i0 = 2 * g.t_env[l];
        const int i1 = 2 * g.t_env[l + 1];
        const double slots = static_cast<double>(i1 - i0);
        if (h.interpol_freq) {
            for (int m = 0; m < m_count; ++m) {
                double acc = 0.0;
                for (int i = i0; i < i1; ++i) acc += std::norm(xhigh[i][m + kx]);
                ec[m] = acc / slots;
            }
        } else {
            for (int b = 0; b < nb; ++b) {
                double acc = 0.0;
                for (int m = F[b]; m < F[b + 1]; ++m) {
                    for (int i = i0; i < i1; ++i) acc += std::norm(xhigh[i][m]);
                }
                acc /= slots * (F[b + 1] - F[b]);
                for (int m = F[b]; m < F[b + 1]; ++m) ec[m - kx] = acc;
            }
        }
        const bool transient = l == g.l_a || l == la_prev;
        const double delta = transient ? 0.0 : 1.0;
        double gain[64], qm[64], sm[64];
        for (int m = 0; m < m_count; ++m) {
            qm[m] = std::sqrt(eo[m] * qo[m] / (1.0 + qo[m]));
            sm[m] = std::sqrt(eo[m] * sim[m] / (1.0 + qo[m]));
            if (!smap[m]) gain[m] = std::sqrt(eo[m] / ((eps + ec[m]) * (1.0 + delta * qo[m])));
            else gain[m] = std::sqrt(eo[m] * qo[m] / ((eps + ec[m]) * (1.0 + qo[m])));
        }
        for (int b = 0; b < t.n_lim; ++b) {
            const int lo = t.f_lim[b] - kx;
            const int hi = t.f_lim[b + 1] - kx;
            double so = 0.0, sc = 0.0;
            for (int m = lo; m < hi; ++m) {
                so += eo[m];
                sc += ec[m];
            }
            double gmax = lim_gain[h.limiter_gains] * std::sqrt((eps0 + so) / (eps0 + sc));
            gmax = std::min(gmax, 1e5);
            for (int m = lo; m < hi; ++m) {
                const double qmax = gain[m] > 0.0 ? qm[m] * gmax / gain[m] : qm[m];
                qm[m] = std::min(qm[m], qmax);
                gain[m] = std::min(gain[m], gmax);
            }
            double den = 0.0;
            for (int m = lo; m < hi; ++m) {
                den += ec[m] * gain[m] * gain[m] + sm[m] * sm[m];
                if (sm[m] == 0.0 && !transient) den += qm[m] * qm[m];
            }
            double boost = std::sqrt((eps0 + so) / (eps0 + den));
            boost = std::min(boost, 1.584893192);
            for (int m = lo; m < hi; ++m) {
                gain[m] *= boost;
                qm[m] *= boost;
                sm[m] *= boost;
            }
        }
        for (int m = 0; m < m_count; ++m) {
            g_lim[l][m] = static_cast<float>(gain[m]);
            q_lim[l][m] = static_cast<float>(qm[m]);
            s_m[l][m] = static_cast<float>(sm[m]);
        }
        for (int m = 0; m < 64; ++m) s_index[m] = m < m_count ? sim[m] : 0;
    }
    const int hsl = h.smoothing_mode ? 0 : 4;
    if (reset || !c.hist_valid) {
        for (int j = 0; j < 4; ++j) {
            for (int m = 0; m < 64; ++m) {
                c.g_hist[j][m] = m < m_count ? g_lim[0][m] : 0.0f;
                c.q_hist[j][m] = m < m_count ? q_lim[0][m] : 0.0f;
            }
        }
        c.hist_valid = true;
    }
    static const float phi_re[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
    static const float phi_im[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
    for (int l = 0; l < ne; ++l) {
        const bool transient = l == g.l_a || l == la_prev;
        for (int i = 2 * g.t_env[l]; i < 2 * g.t_env[l + 1]; ++i) {
            const int sidx = c.sine_index;
            for (int m = 0; m < m_count; ++m) {
                float gf;
                float qf;
                if (hsl && !transient) {
                    gf = smooth[0] * g_lim[l][m];
                    qf = smooth[0] * q_lim[l][m];
                    for (int j = 1; j <= 4; ++j) {
                        gf += smooth[j] * c.g_hist[4 - j][m];
                        qf += smooth[j] * c.q_hist[4 - j][m];
                    }
                } else {
                    gf = g_lim[l][m];
                    qf = q_lim[l][m];
                }
                if (s_m[l][m] != 0.0f || transient) qf = 0.0f;
                const int k = m + kx;
                const int ni = (c.noise_index + m + 1) & 511;
                Cplx v = xhigh[i][k] * gf;
                v += Cplx(static_cast<float>(k_sbr_noise[ni][0]) * qf, static_cast<float>(k_sbr_noise[ni][1]) * qf);
                if (s_m[l][m] != 0.0f) {
                    const float sign = (k & 1) ? -1.0f : 1.0f;
                    v += Cplx(s_m[l][m] * phi_re[sidx], s_m[l][m] * phi_im[sidx] * sign);
                }
                c.y[i][k] = v;
            }
            for (int j = 0; j < 3; ++j) {
                std::memcpy(c.g_hist[j], c.g_hist[j + 1], sizeof(c.g_hist[j]));
                std::memcpy(c.q_hist[j], c.q_hist[j + 1], sizeof(c.q_hist[j]));
            }
            for (int m = 0; m < 64; ++m) {
                c.g_hist[3][m] = m < m_count ? g_lim[l][m] : 0.0f;
                c.q_hist[3][m] = m < m_count ? q_lim[l][m] : 0.0f;
            }
            c.noise_index = (c.noise_index + m_count) & 511;
            c.sine_index = (c.sine_index + 1) & 3;
        }
    }
    for (int i = 0; i < 64; ++i) c.s_index_prev[i] = s_index[i];
}

bool parse_single(SbrState& s, BitReader& br, std::size_t end) {
    Channel& c = s.ch[0];
    const Tables& t = s.tables;
    if (br.bit()) br.skip(4);
    if (!parse_grid(br, c.grid, s.header.amp_res)) return false;
    parse_dtdf(br, c);
    parse_invf(br, c, t.n_q);
    if (!parse_envelope(br, c, t, false)) return false;
    if (!parse_noise(br, c, t, false)) return false;
    parse_harmonics(br, c, t);
    s.coupling = 0;
    if (!parse_extended(s, br, end)) return false;
    return !br.overrun() && br.pos() <= end;
}

bool parse_pair(SbrState& s, BitReader& br, std::size_t end) {
    Channel& a = s.ch[0];
    Channel& b = s.ch[1];
    const Tables& t = s.tables;
    if (br.bit()) br.skip(8);
    s.coupling = br.bit();
    if (s.coupling) {
        if (!parse_grid(br, a.grid, s.header.amp_res)) return false;
        b.grid = a.grid;
        parse_dtdf(br, a);
        parse_dtdf(br, b);
        parse_invf(br, a, t.n_q);
        for (int i = 0; i < t.n_q; ++i) b.invf[i] = a.invf[i];
        if (!parse_envelope(br, a, t, false)) return false;
        if (!parse_noise(br, a, t, false)) return false;
        if (!parse_envelope(br, b, t, true)) return false;
        if (!parse_noise(br, b, t, true)) return false;
    } else {
        if (!parse_grid(br, a.grid, s.header.amp_res)) return false;
        if (!parse_grid(br, b.grid, s.header.amp_res)) return false;
        parse_dtdf(br, a);
        parse_dtdf(br, b);
        parse_invf(br, a, t.n_q);
        parse_invf(br, b, t.n_q);
        if (!parse_envelope(br, a, t, false)) return false;
        if (!parse_envelope(br, b, t, false)) return false;
        if (!parse_noise(br, a, t, false)) return false;
        if (!parse_noise(br, b, t, false)) return false;
    }
    parse_harmonics(br, a, t);
    parse_harmonics(br, b, t);
    if (!parse_extended(s, br, end)) return false;
    return !br.overrun() && br.pos() <= end;
}

} // namespace

SbrElement::SbrElement() : m_state(new State()) {
    m_state->clear_all();
}

SbrElement::~SbrElement() = default;

void SbrElement::setup(int channels, std::uint32_t sbr_rate, bool downsampled, bool allow_ps) {
    State& s = *m_state;
    s.channels = channels == 2 ? 2 : 1;
    s.rate = sbr_rate;
    s.downsampled = downsampled;
    s.allow_ps = allow_ps && s.channels == 1;
    s.clear_all();
}

void SbrElement::reset() {
    m_state->clear_all();
}

bool SbrElement::parse(BitReader& br, std::size_t end_bit, bool crc, bool pair) {
    State& s = *m_state;
    s.frame_valid = false;
    s.ps_frame = false;
    if ((pair ? 2 : 1) != s.channels) return false;
    if (crc) br.skip(10);
    if (br.bit()) {
        if (!parse_header(s, br)) return false;
    }
    if (!s.header_seen || !s.tables.valid) return false;
    const bool ok = pair ? parse_pair(s, br, end_bit) : parse_single(s, br, end_bit);
    if (!ok) return false;
    s.frame_valid = true;
    return true;
}

void SbrElement::no_data() {
    m_state->frame_valid = false;
    m_state->ps_frame = false;
}

void SbrElement::process(const float* const* core, float* const* out) {
    State& s = *m_state;
    const Tables& t = s.tables;
    const bool active = s.frame_valid && t.valid;
    const bool reset = s.reset_pending;
    for (int ci = 0; ci < s.channels; ++ci) {
        Channel& c = s.ch[ci];
        analysis(c, core[ci]);
        std::memset(static_cast<void*>(c.y), 0, sizeof(c.y));
        int l_temp = std::max(0, 2 * c.t_end_prev - k_slots);
        if (active) {
            float e_orig[8][64];
            float q_orig[2][8];
            dequantize(s, c, ci, s.ch[1 - ci], e_orig, q_orig);
            static thread_local Cplx xhigh[k_low_slots][64];
            std::memset(static_cast<void*>(xhigh), 0, sizeof(xhigh));
            if (reset) {
                for (int i = 0; i < 8; ++i) {
                    c.bw_prev[i] = 0.0f;
                    c.invf_prev[i] = 0;
                }
            }
            hf_generate(s, c, xhigh);
            hf_adjust(s, c, xhigh, e_orig, q_orig, reset);
        }
        const int kx_new = active ? t.kx : 32;
        const int m_new = active ? t.m : 0;
        for (int l = 0; l < k_slots; ++l) {
            Cplx* x = c.xout[l];
            const bool old = l < l_temp;
            const int kx = old ? c.kx_prev : kx_new;
            const int mm = old ? c.m_prev : m_new;
            for (int k = 0; k < 64; ++k) x[k] = Cplx();
            for (int k = 0; k < kx && k < 32; ++k) x[k] = c.x_low[l + k_hf_adj][k];
            for (int k = kx; k < kx + mm && k < 64; ++k) x[k] = old ? c.y_prev[l][k] : c.y[l][k];
        }
        for (int i = 0; i < 6; ++i) {
            for (int k = 0; k < 5; ++k) c.ahead[i][k] = c.x_low[k_slots + i + k_hf_adj][k];
        }
        for (int i = 0; i < 8; ++i) std::memcpy(c.y_prev[i], c.y[k_slots + i], sizeof(c.y_prev[i]));
        for (int i = 0; i < 8; ++i) std::memcpy(c.x_low[i], c.x_low[k_slots + i], sizeof(c.x_low[i]));
        if (active) {
            const Grid& g = c.grid;
            const int nb = band_count(t, g.freq_res[g.num_env - 1]);
            for (int k = 0; k < nb; ++k) c.env_prev[k] = c.env[g.num_env - 1][k];
            c.env_prev_res = g.freq_res[g.num_env - 1];
            for (int k = 0; k < t.n_q; ++k) c.noise_prev[k] = c.noise[g.num_noise - 1][k];
            c.la_prev = g.l_a;
            c.ne_prev = g.num_env;
            c.t_end_prev = g.t_env[g.num_env];
        } else {
            c.la_prev = -1;
            c.ne_prev = 0;
            c.t_end_prev = 0;
            c.hist_valid = false;
        }
        c.kx_prev = kx_new;
        c.m_prev = m_new;
    }
    if (active) s.reset_pending = false;
    const int len = s.downsampled ? 1024 : 2048;
    if (s.allow_ps) {
        Channel& l = s.ch[0];
        Channel& r = s.ch[1];
        if (s.ps_used) {
            s.ps.process(l.xout, r.xout, l.ahead, s.ps_frame);
        } else {
            std::memcpy(static_cast<void*>(r.xout), l.xout, sizeof(r.xout));
        }
        synthesis(l.synthesis, l.xout, out[0], s.downsampled);
        synthesis(r.synthesis, r.xout, out[1], s.downsampled);
    } else {
        for (int ci = 0; ci < s.channels; ++ci) synthesis(s.ch[ci].synthesis, s.ch[ci].xout, out[ci], s.downsampled);
        if (s.channels == 1) std::memcpy(out[1], out[0], sizeof(float) * static_cast<std::size_t>(len));
    }
    s.frame_valid = false;
    s.ps_frame = false;
}

bool SbrElement::ps_seen() const noexcept {
    return m_state->ps_used;
}

int SbrElement::channels() const noexcept {
    return m_state->channels;
}

} // namespace aac_internal
} // namespace audio
} // namespace fizmo
