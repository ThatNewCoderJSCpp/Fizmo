#include "aac_ps.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {
namespace aac_internal {

namespace {

using Cplx = std::complex<float>;

const double k_pi = 3.14159265358979323846;
const int k_slots = 32;
const int k_max_env = 5;
const int k_max_bands = 91;
const int k_max_par = 34;
const int k_links = 3;

const float k_icc_values[8] = { 1.0f, 0.937f, 0.84118f, 0.60092f, 0.36764f, 0.0f, -0.589f, -1.0f };
const float k_iid_coarse[15] = { -25, -18, -14, -10, -7, -4, -2, 0, 2, 4, 7, 10, 14, 18, 25 };
const float k_iid_fine[31] = { -50, -45, -40, -35, -30, -25, -22, -19, -16, -13, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 13, 16, 19, 22, 25, 30, 35, 40, 45, 50 };

const double k_g8_20[7] = { 0.00746082949812, 0.02270420949825, 0.04546865930473, 0.07266113929591, 0.09885108575264, 0.11793710567217, 0.125 };
const double k_g2[7] = { 0.0, 0.01899487526049, 0.0, -0.07293139167538, 0.0, 0.30596630545168, 0.5 };
const double k_g12_34[7] = { 0.04081179924692, 0.03812810994926, 0.05144908135699, 0.06399831151592, 0.07428313801106, 0.08100347892914, 0.08333333333333 };
const double k_g8_34[7] = { 0.01565675600122, 0.03752716391991, 0.05417891378782, 0.08417044116767, 0.10307344158036, 0.12222452249753, 0.125 };
const double k_g4_34[7] = { -0.05908211155639, -0.04871498374946, 0.0, 0.07778723915851, 0.16486303567403, 0.23279856662996, 0.25 };

const int k_num_env_fix[4] = { 0, 1, 2, 4 };
const int k_nr_par[6] = { 10, 20, 34, 10, 20, 34 };
const int k_nr_ipdopd[6] = { 5, 11, 17, 5, 11, 17 };

const float k_link_q[k_links] = { 0.43f, 0.75f, 0.347f };
const float k_link_a[k_links] = { 0.65143905753106f, 0.56471812200776f, 0.48954165955695f };
const int k_link_delay[k_links] = { 3, 4, 5 };

struct FilterBank {
    Cplx f8_20[8][13];
    Cplx f12_34[12][13];
    Cplx f8_34[8][13];
    Cplx f4_34[4][13];

    static void make(Cplx (*dst)[13], const double* proto, int q_count) {
        for (int q = 0; q < q_count; ++q) {
            for (int m = 0; m < 13; ++m) {
                const double g = proto[m <= 6 ? m : 12 - m];
                const double a = 2.0 * k_pi / q_count * (q + 0.5) * (m - 6);
                dst[q][m] = Cplx(static_cast<float>(g * std::cos(a)), static_cast<float>(g * std::sin(a)));
            }
        }
    }

    FilterBank() {
        make(f8_20, k_g8_20, 8);
        make(f12_34, k_g12_34, 12);
        make(f8_34, k_g8_34, 8);
        make(f4_34, k_g4_34, 4);
    }
};

const FilterBank& filters() {
    static const FilterBank f;
    return f;
}

struct Layout {
    int bands = 71;
    int hybrid = 10;
    int qmf_split = 3;
    int k_to_i[k_max_bands] = {};
    int qmf_of[k_max_bands] = {};
    float f_center[k_max_bands] = {};
    int allpass = 30;
    int short_delay = 42;
    int decay_cutoff = 3;
};

Layout make_layout(bool is34) {
    Layout l;
    if (!is34) {
        l.bands = 71;
        l.hybrid = 10;
        l.qmf_split = 3;
        static const int first[10] = { 1, 0, 0, 1, 2, 3, 4, 5, 6, 7 };
        static const float fc[10] = { -3, -1, 1, 3, 5, 7, 10, 14, 18, 22 };
        static const int qmf_first[10] = { 0, 0, 0, 0, 0, 0, 1, 1, 2, 2 };
        for (int k = 0; k < 10; ++k) {
            l.k_to_i[k] = first[k];
            l.f_center[k] = fc[k] / 8.0f;
            l.qmf_of[k] = qmf_first[k];
        }
        static const int borders[13] = { 3, 4, 5, 6, 7, 8, 9, 11, 14, 18, 23, 35, 64 };
        for (int q = 3; q < 64; ++q) {
            int b = 0;
            while (q >= borders[b + 1]) ++b;
            const int k = q + 7;
            l.k_to_i[k] = 8 + b;
            l.f_center[k] = static_cast<float>(q) + 0.5f;
            l.qmf_of[k] = q;
        }
        l.allpass = 30;
        l.short_delay = 42;
        l.decay_cutoff = 3;
    } else {
        l.bands = 91;
        l.hybrid = 32;
        l.qmf_split = 5;
        static const int first[32] = { 0, 1, 2, 3, 4, 5, 6, 6, 7, 2, 1, 0, 10, 10, 4, 5, 6, 7, 8, 9, 10, 11, 12, 9, 14, 11, 12, 13, 14, 15, 16, 13 };
        static const float fc[32] = { 2, 6, 10, 14, 18, 22, 26, 30, 34, -10, -6, -2, 51, 57, 15, 21, 27, 33, 39, 45, 54, 66, 78, 42, 102, 66, 78, 90, 102, 114, 126, 90 };
        static const int qmf_first[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4 };
        for (int k = 0; k < 32; ++k) {
            l.k_to_i[k] = first[k];
            l.f_center[k] = fc[k] / 24.0f;
            l.qmf_of[k] = qmf_first[k];
        }
        static const int borders[] = { 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 21, 24, 27, 30, 33, 37, 41, 64 };
        for (int q = 5; q < 64; ++q) {
            int b = 0;
            while (q >= borders[b + 1]) ++b;
            const int k = q + 27;
            l.k_to_i[k] = 16 + b;
            l.f_center[k] = static_cast<float>(q) + 0.5f;
            l.qmf_of[k] = q;
        }
        l.allpass = 50;
        l.short_delay = 62;
        l.decay_cutoff = 5;
    }
    return l;
}

const Layout& layout(bool is34) {
    static const Layout l20 = make_layout(false);
    static const Layout l34 = make_layout(true);
    return is34 ? l34 : l20;
}

} // namespace

struct PsDecoder::State {
    bool header_seen = false;
    int enable_iid = 0;
    int iid_mode = 0;
    int enable_icc = 0;
    int icc_mode = 0;
    int enable_ext = 0;
    int enable_ipdopd = 0;

    int frame_class = 0;
    int num_env = 0;
    int border[k_max_env + 2] = {};
    int iid[k_max_env + 1][k_max_par] = {};
    int icc[k_max_env + 1][k_max_par] = {};
    int ipd[k_max_env + 1][k_max_par] = {};
    int opd[k_max_env + 1][k_max_par] = {};
    int iid_prev[k_max_par] = {};
    int icc_prev[k_max_par] = {};
    int ipd_prev[k_max_par] = {};
    int opd_prev[k_max_par] = {};
    int iid_prev_count = 0;
    int icc_prev_count = 0;
    int ipd_prev_count = 0;
    bool frame_ok = false;

    bool is34 = false;
    bool is34_prev = false;
    Cplx qmf_hist[5][12];
    Cplx ap_pre[k_max_bands][2];
    Cplx ap_link[k_max_bands][k_links][5];
    Cplx plain_delay[k_max_bands][14];
    int ap_pos = 0;
    float peak_decay[k_max_par] = {};
    float power_smooth[k_max_par] = {};
    float peak_diff_smooth[k_max_par] = {};
    float h_prev[k_max_par][4][2] = {};
    int ipd_hist[2][k_max_par] = {};
    int opd_hist[2][k_max_par] = {};

    void clear_dsp() {
        std::memset(static_cast<void*>(qmf_hist), 0, sizeof(qmf_hist));
        std::memset(static_cast<void*>(ap_pre), 0, sizeof(ap_pre));
        std::memset(static_cast<void*>(ap_link), 0, sizeof(ap_link));
        std::memset(static_cast<void*>(plain_delay), 0, sizeof(plain_delay));
        std::memset(peak_decay, 0, sizeof(peak_decay));
        std::memset(power_smooth, 0, sizeof(power_smooth));
        std::memset(peak_diff_smooth, 0, sizeof(peak_diff_smooth));
        for (int b = 0; b < k_max_par; ++b) {
            for (int i = 0; i < 4; ++i) h_prev[b][i][0] = h_prev[b][i][1] = 0.0f;
            h_prev[b][0][0] = 1.0f;
            h_prev[b][1][0] = 1.0f;
        }
        std::memset(ipd_hist, 0, sizeof(ipd_hist));
        std::memset(opd_hist, 0, sizeof(opd_hist));
        ap_pos = 0;
    }
};

namespace {

using PsState = PsDecoder::State;

bool read_params(BitReader& br, int (*dst)[k_max_par], int* prev, int& prev_count, int e, int count, int dt, const Huffman& h, int offset, int lo, int hi, bool wrap) {
    if (dt) {
        const int* ref = e == 0 ? prev : dst[e - 1];
        const int ref_count = e == 0 ? prev_count : count;
        for (int b = 0; b < count; ++b) {
            const int d = h.decode(br);
            if (d < 0) return false;
            int rb = b;
            if (ref_count && ref_count != count) rb = std::min(ref_count - 1, b * ref_count / count);
            int v = (ref_count ? ref[rb] : 0) + d - offset;
            if (wrap) v &= 7;
            dst[e][b] = v;
        }
    } else {
        int acc = 0;
        for (int b = 0; b < count; ++b) {
            const int d = h.decode(br);
            if (d < 0) return false;
            acc += d - offset;
            if (wrap) acc &= 7;
            dst[e][b] = acc;
        }
    }
    for (int b = 0; b < count; ++b) {
        if (!wrap) dst[e][b] = std::max(lo, std::min(dst[e][b], hi));
    }
    return true;
}

} // namespace

PsDecoder::PsDecoder() : m_state(new State()) {
    m_state->clear_dsp();
}

PsDecoder::~PsDecoder() = default;

void PsDecoder::reset() {
    State& s = *m_state;
    s = State();
    s.clear_dsp();
}

int PsDecoder::parse(BitReader& br, int bits_available) {
    State& s = *m_state;
    const std::size_t start = br.pos();
    const std::size_t limit = start + static_cast<std::size_t>(std::max(bits_available, 0));
    s.frame_ok = false;
    if (br.bit()) {
        s.header_seen = true;
        s.enable_iid = br.bit();
        if (s.enable_iid) s.iid_mode = static_cast<int>(br.read(3));
        s.enable_icc = br.bit();
        if (s.enable_icc) s.icc_mode = static_cast<int>(br.read(3));
        s.enable_ext = br.bit();
    }
    s.frame_class = br.bit();
    const int env_idx = static_cast<int>(br.read(2));
    s.num_env = s.frame_class ? env_idx + 1 : k_num_env_fix[env_idx];
    if (s.frame_class) {
        for (int e = 0; e < s.num_env; ++e) s.border[e + 1] = static_cast<int>(br.read(5)) + 1;
    }
    const bool modes_ok = s.iid_mode <= 5 && s.icc_mode <= 5;
    const int nr_iid = k_nr_par[s.iid_mode % 6];
    const int nr_icc = k_nr_par[s.icc_mode % 6];
    const int nr_ipd = k_nr_ipdopd[s.iid_mode % 6];
    bool ok = s.header_seen && modes_ok;
    if (s.enable_iid) {
        const bool fine = s.iid_mode >= 3;
        for (int e = 0; e < s.num_env && ok; ++e) {
            const int dt = br.bit();
            const Huffman& h = ps_huffman(fine ? (dt ? 1 : 0) : (dt ? 3 : 2));
            const int range = fine ? 15 : 7;
            ok = read_params(br, s.iid, s.iid_prev, s.iid_prev_count, e, nr_iid, dt, h, fine ? 30 : 14, -range, range, false);
        }
    }
    if (s.enable_icc) {
        for (int e = 0; e < s.num_env && ok; ++e) {
            const int dt = br.bit();
            ok = read_params(br, s.icc, s.icc_prev, s.icc_prev_count, e, nr_icc, dt, ps_huffman(dt ? 5 : 4), 7, 0, 7, false);
        }
    }
    s.enable_ipdopd = 0;
    if (s.enable_ext && ok) {
        std::size_t count = br.read(4);
        if (count == 15) count += br.read(8);
        long long bits = static_cast<long long>(count) * 8;
        const std::size_t stop = br.pos() + static_cast<std::size_t>(bits);
        while (bits > 7 && ok) {
            const int id = static_cast<int>(br.read(2));
            bits -= 2;
            if (id == 0) {
                const std::size_t before = br.pos();
                s.enable_ipdopd = br.bit();
                if (s.enable_ipdopd) {
                    for (int e = 0; e < s.num_env && ok; ++e) {
                        int dt = br.bit();
                        ok = read_params(br, s.ipd, s.ipd_prev, s.ipd_prev_count, e, nr_ipd, dt, ps_huffman(dt ? 7 : 6), 0, 0, 7, true);
                        if (!ok) break;
                        dt = br.bit();
                        int dummy = s.ipd_prev_count;
                        ok = read_params(br, s.opd, s.opd_prev, dummy, e, nr_ipd, dt, ps_huffman(dt ? 9 : 8), 0, 0, 7, true);
                    }
                }
                br.skip(1);
                bits -= static_cast<long long>(br.pos() - before);
            } else {
                br.skip(static_cast<std::size_t>(std::max<long long>(bits, 0)));
                bits = 0;
            }
        }
        br.seek(stop);
    }
    if (br.overrun() || br.pos() > limit) ok = false;
    if (ok) {
        if (s.num_env > 0) {
            const int last = s.num_env - 1;
            for (int b = 0; b < k_max_par; ++b) {
                s.iid_prev[b] = s.enable_iid ? s.iid[last][b] : 0;
                s.icc_prev[b] = s.enable_icc ? s.icc[last][b] : 0;
                s.ipd_prev[b] = s.enable_ipdopd ? s.ipd[last][b] : 0;
                s.opd_prev[b] = s.enable_ipdopd ? s.opd[last][b] : 0;
            }
            s.iid_prev_count = s.enable_iid ? nr_iid : 0;
            s.icc_prev_count = s.enable_icc ? nr_icc : 0;
            s.ipd_prev_count = s.enable_ipdopd ? nr_ipd : 0;
        }
        s.frame_ok = true;
    }
    return static_cast<int>(br.pos() - start);
}

namespace {

void hybrid_complex(const Cplx* x, const Cplx (*f)[13], int q_count, Cplx* out) {
    for (int q = 0; q < q_count; ++q) {
        Cplx acc(0.0f, 0.0f);
        for (int m = 0; m < 13; ++m) acc += f[q][m] * x[12 - m];
        out[q] = acc;
    }
}

void hybrid_real2(const Cplx* x, Cplx& low, Cplx& high) {
    Cplx odd(0.0f, 0.0f);
    for (int m = 1; m < 13; m += 2) odd += static_cast<float>(k_g2[m <= 6 ? m : 12 - m]) * x[12 - m];
    const Cplx center = 0.5f * x[6];
    low = center + odd;
    high = center - odd;
}

void mixing_matrix(const PsState& s, int iid, int icc, float* h) {
    const bool fine = s.iid_mode >= 3;
    const float db = fine ? k_iid_fine[std::max(-15, std::min(iid, 15)) + 15] : k_iid_coarse[std::max(-7, std::min(iid, 7)) + 7];
    const double c = std::pow(10.0, db / 20.0);
    const double rho = k_icc_values[std::max(0, std::min(icc, 7))];
    if (s.icc_mode < 3) {
        const double c1 = std::sqrt(2.0 / (1.0 + c * c));
        const double c2 = std::sqrt(2.0 * c * c / (1.0 + c * c));
        const double alpha = 0.5 * std::acos(rho);
        const double beta = alpha * (c1 - c2) / std::sqrt(2.0);
        h[0] = static_cast<float>(c2 * std::cos(beta + alpha));
        h[1] = static_cast<float>(c1 * std::cos(beta - alpha));
        h[2] = static_cast<float>(c2 * std::sin(beta + alpha));
        h[3] = static_cast<float>(c1 * std::sin(beta - alpha));
    } else {
        const double r = std::max(rho, 0.05);
        double alpha = 0.5 * std::atan2(2.0 * c * r, c * c - 1.0);
        double mu = c + 1.0 / c;
        mu = std::sqrt(std::max(0.0, 1.0 + (4.0 * r * r - 4.0) / (mu * mu)));
        const double gamma = std::atan(std::sqrt((1.0 - mu) / (1.0 + mu)));
        if (alpha < 0.0) alpha += k_pi / 2.0;
        const double s2 = std::sqrt(2.0);
        h[0] = static_cast<float>(s2 * std::cos(alpha) * std::cos(gamma));
        h[1] = static_cast<float>(s2 * std::sin(alpha) * std::cos(gamma));
        h[2] = static_cast<float>(-s2 * std::sin(alpha) * std::sin(gamma));
        h[3] = static_cast<float>(s2 * std::cos(alpha) * std::sin(gamma));
    }
}

} // namespace

void PsDecoder::process(std::complex<float> (*left)[64], std::complex<float> (*right)[64], const std::complex<float> (*ahead)[5], bool new_data) {
    State& s = *m_state;
    const bool use_new = new_data && s.frame_ok;
    const bool is34 = s.iid_mode % 3 == 2 || s.icc_mode % 3 == 2;
    if (is34 != s.is34_prev) {
        s.clear_dsp();
        s.is34_prev = is34;
    }
    s.is34 = is34;
    const Layout& lay = layout(is34);
    const FilterBank& fb = filters();
    const int nb = lay.bands;
    static thread_local Cplx sig[k_max_bands][k_slots];
    static thread_local Cplx dec[k_max_bands][k_slots];
    const int split = lay.qmf_split;
    for (int n = 0; n < k_slots; ++n) {
        Cplx win[5][13];
        for (int q = 0; q < split; ++q) {
            for (int m = 0; m < 12; ++m) win[q][m] = s.qmf_hist[q][m];
            const Cplx incoming = n + 6 < k_slots ? left[n + 6][q] : ahead[n + 6 - k_slots][q];
            win[q][12] = incoming;
            for (int m = 0; m < 11; ++m) s.qmf_hist[q][m] = s.qmf_hist[q][m + 1];
            s.qmf_hist[q][11] = incoming;
        }
        if (!is34) {
            Cplx t[8];
            hybrid_complex(win[0], fb.f8_20, 8, t);
            sig[0][n] = t[6];
            sig[1][n] = t[7];
            sig[2][n] = t[0];
            sig[3][n] = t[1];
            sig[4][n] = t[2] + t[5];
            sig[5][n] = t[3] + t[4];
            Cplx lo, hi;
            hybrid_real2(win[1], lo, hi);
            sig[6][n] = hi;
            sig[7][n] = lo;
            hybrid_real2(win[2], lo, hi);
            sig[8][n] = lo;
            sig[9][n] = hi;
        } else {
            Cplx t[12];
            hybrid_complex(win[0], fb.f12_34, 12, t);
            for (int i = 0; i < 12; ++i) sig[i][n] = t[i];
            hybrid_complex(win[1], fb.f8_34, 8, t);
            for (int i = 0; i < 8; ++i) sig[12 + i][n] = t[i];
            hybrid_complex(win[2], fb.f4_34, 4, t);
            for (int i = 0; i < 4; ++i) sig[20 + i][n] = t[i];
            hybrid_complex(win[3], fb.f4_34, 4, t);
            for (int i = 0; i < 4; ++i) sig[24 + i][n] = t[i];
            hybrid_complex(win[4], fb.f4_34, 4, t);
            for (int i = 0; i < 4; ++i) sig[28 + i][n] = t[i];
        }
        for (int q = split; q < 64; ++q) sig[q + (lay.hybrid - split)][n] = left[n][q];
    }
    const int npar = is34 ? 34 : 20;
    float power[k_max_par][k_slots];
    for (int b = 0; b < npar; ++b) {
        for (int n = 0; n < k_slots; ++n) power[b][n] = 0.0f;
    }
    for (int k = 0; k < nb; ++k) {
        const int b = lay.k_to_i[k];
        for (int n = 0; n < k_slots; ++n) power[b][n] += std::norm(sig[k][n]);
    }
    float gain[k_max_par][k_slots];
    const float peak_factor = 0.76592833836465f;
    const float smooth = 0.25f;
    const float impact = 1.5f;
    for (int b = 0; b < npar; ++b) {
        for (int n = 0; n < k_slots; ++n) {
            s.peak_decay[b] = std::max(s.peak_decay[b] * peak_factor, power[b][n]);
            s.power_smooth[b] += smooth * (power[b][n] - s.power_smooth[b]);
            s.peak_diff_smooth[b] += smooth * (s.peak_decay[b] - power[b][n] - s.peak_diff_smooth[b]);
            const float denom = impact * s.peak_diff_smooth[b];
            gain[b][n] = denom > s.power_smooth[b] ? s.power_smooth[b] / denom : 1.0f;
        }
    }
    for (int k = 0; k < nb; ++k) {
        const int b = lay.k_to_i[k];
        if (k < lay.allpass) {
            const float fc = lay.f_center[k];
            const Cplx phi = std::polar(1.0f, static_cast<float>(-k_pi * 0.39 * fc));
            Cplx qf[k_links];
            for (int m = 0; m < k_links; ++m) qf[m] = std::polar(1.0f, static_cast<float>(-k_pi * k_link_q[m] * fc));
            const int qmf = lay.qmf_of[k];
            float decay = 1.0f;
            if (qmf > lay.decay_cutoff) decay = std::max(0.0f, 1.0f - 0.05f * static_cast<float>(qmf - lay.decay_cutoff));
            for (int n = 0; n < k_slots; ++n) {
                Cplx in = s.ap_pre[k][0] * phi;
                s.ap_pre[k][0] = s.ap_pre[k][1];
                s.ap_pre[k][1] = sig[k][n];
                for (int m = 0; m < k_links; ++m) {
                    const float g = k_link_a[m] * decay;
                    Cplx* buf = s.ap_link[k][m];
                    const int d = k_link_delay[m];
                    const Cplx old = buf[0];
                    const Cplx v = in + g * qf[m] * old;
                    const Cplx y = -g * v + qf[m] * old;
                    for (int i = 0; i < d - 1; ++i) buf[i] = buf[i + 1];
                    buf[d - 1] = v;
                    in = y;
                }
                dec[k][n] = in * gain[b][n];
            }
        } else {
            const int d = k < lay.short_delay ? 14 : 1;
            Cplx* buf = s.plain_delay[k];
            for (int n = 0; n < k_slots; ++n) {
                const Cplx out = buf[0];
                for (int i = 0; i < d - 1; ++i) buf[i] = buf[i + 1];
                buf[d - 1] = sig[k][n];
                dec[k][n] = out * gain[b][n];
            }
        }
    }
    int env_count = 1;
    int borders[k_max_env + 2] = { 0, k_slots };
    int iid_e[k_max_env + 1][k_max_par] = {};
    int icc_e[k_max_env + 1][k_max_par] = {};
    int ipd_e[k_max_env + 1][k_max_par] = {};
    int opd_e[k_max_env + 1][k_max_par] = {};
    bool hold = true;
    if (use_new && s.num_env > 0) {
        hold = false;
        env_count = s.num_env;
        if (s.frame_class == 0) {
            for (int e = 0; e <= env_count; ++e) borders[e] = (k_slots * e) / env_count;
        } else {
            borders[0] = 0;
            for (int e = 1; e <= env_count; ++e) {
                borders[e] = std::max(std::min(s.border[e], k_slots), borders[e - 1] + 1);
                borders[e] = std::min(borders[e], k_slots - (env_count - e));
            }
        }
        const int nr_iid = k_nr_par[s.iid_mode];
        const int nr_icc = k_nr_par[s.icc_mode];
        const int nr_ipd = k_nr_ipdopd[s.iid_mode];
        for (int e = 0; e < env_count; ++e) {
            for (int b = 0; b < npar; ++b) {
                auto mapped = [&](const int* src, int count, int b20) {
                    if (count == npar) return src[b20];
                    if (!is34) return src[std::min(count - 1, b20 / 2)];
                    if (count == 34) return src[std::min(33, b20)];
                    static const int map20to34[34] = { 0, 0, 1, 2, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19 };
                    const int v = map20to34[b20];
                    return count == 20 ? src[v] : src[std::min(count - 1, v / 2)];
                };
                iid_e[e][b] = s.enable_iid ? mapped(s.iid[e], nr_iid, b) : 0;
                icc_e[e][b] = s.enable_icc ? mapped(s.icc[e], nr_icc, b) : 0;
                if (s.enable_ipdopd) {
                    const int lim = is34 ? 17 : 11;
                    ipd_e[e][b] = b < lim ? mapped(s.ipd[e], nr_ipd, b) : 0;
                    opd_e[e][b] = b < lim ? mapped(s.opd[e], nr_ipd, b) : 0;
                }
            }
        }
        if (borders[env_count] < k_slots) {
            for (int b = 0; b < npar; ++b) {
                iid_e[env_count][b] = iid_e[env_count - 1][b];
                icc_e[env_count][b] = icc_e[env_count - 1][b];
                ipd_e[env_count][b] = ipd_e[env_count - 1][b];
                opd_e[env_count][b] = opd_e[env_count - 1][b];
            }
            ++env_count;
            borders[env_count] = k_slots;
        }
    }
    float h_env[k_max_env + 1][k_max_par][4][2];
    for (int e = 0; e < env_count; ++e) {
        for (int b = 0; b < npar; ++b) {
            if (hold) {
                for (int i = 0; i < 4; ++i) {
                    h_env[e][b][i][0] = s.h_prev[b][i][0];
                    h_env[e][b][i][1] = s.h_prev[b][i][1];
                }
                continue;
            }
            float h[4];
            mixing_matrix(s, iid_e[e][b], icc_e[e][b], h);
            float ph1 = 0.0f;
            float ph2 = 0.0f;
            if (s.enable_ipdopd && b < (is34 ? 17 : 11)) {
                const double step = k_pi / 4.0;
                auto smoothed = [&](int cur, int p1, int p2) {
                    const double re = std::cos(cur * step) + 0.5 * std::cos(p1 * step) + 0.25 * std::cos(p2 * step);
                    const double im = std::sin(cur * step) + 0.5 * std::sin(p1 * step) + 0.25 * std::sin(p2 * step);
                    return std::atan2(im, re);
                };
                const double opd = smoothed(opd_e[e][b], s.opd_hist[0][b], s.opd_hist[1][b]);
                const double ipd = smoothed(ipd_e[e][b], s.ipd_hist[0][b], s.ipd_hist[1][b]);
                s.opd_hist[1][b] = s.opd_hist[0][b];
                s.opd_hist[0][b] = opd_e[e][b];
                s.ipd_hist[1][b] = s.ipd_hist[0][b];
                s.ipd_hist[0][b] = ipd_e[e][b];
                ph1 = static_cast<float>(opd);
                ph2 = static_cast<float>(opd - ipd);
            }
            const float ph[4] = { ph1, ph2, ph1, ph2 };
            for (int i = 0; i < 4; ++i) {
                h_env[e][b][i][0] = h[i] * std::cos(ph[i]);
                h_env[e][b][i][1] = h[i] * std::sin(ph[i]);
            }
        }
    }
    static thread_local Cplx outl[k_max_bands][k_slots];
    static thread_local Cplx outr[k_max_bands][k_slots];
    for (int e = 0; e < env_count; ++e) {
        const int a = borders[e];
        const int z = borders[e + 1];
        const float width = z > a ? 1.0f / static_cast<float>(z - a) : 1.0f;
        for (int b = 0; b < npar; ++b) {
            float step[4][2];
            float cur[4][2];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 2; ++j) {
                    cur[i][j] = s.h_prev[b][i][j];
                    step[i][j] = (h_env[e][b][i][j] - s.h_prev[b][i][j]) * width;
                }
            }
            for (int n = a; n < z; ++n) {
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 2; ++j) cur[i][j] += step[i][j];
                }
                const Cplx h11(cur[0][0], cur[0][1]);
                const Cplx h12(cur[1][0], cur[1][1]);
                const Cplx h21(cur[2][0], cur[2][1]);
                const Cplx h22(cur[3][0], cur[3][1]);
                for (int k = 0; k < nb; ++k) {
                    if (lay.k_to_i[k] != b) continue;
                    outl[k][n] = h11 * sig[k][n] + h21 * dec[k][n];
                    outr[k][n] = h12 * sig[k][n] + h22 * dec[k][n];
                }
            }
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 2; ++j) s.h_prev[b][i][j] = h_env[e][b][i][j];
            }
        }
    }
    for (int n = 0; n < k_slots; ++n) {
        for (int q = 0; q < 64; ++q) {
            left[n][q] = Cplx();
            right[n][q] = Cplx();
        }
        for (int k = 0; k < lay.hybrid; ++k) {
            const int q = lay.qmf_of[k];
            left[n][q] += outl[k][n];
            right[n][q] += outr[k][n];
        }
        for (int k = lay.hybrid; k < nb; ++k) {
            const int q = lay.qmf_of[k];
            left[n][q] = outl[k][n];
            right[n][q] = outr[k][n];
        }
    }
}

} // namespace aac_internal
} // namespace audio
} // namespace fizmo
