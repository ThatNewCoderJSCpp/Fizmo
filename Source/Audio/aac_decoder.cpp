#include "aac_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fizmo {
namespace audio {
namespace aac_internal {

namespace {

const double k_pi = 3.14159265358979323846;

enum WindowSequence {
    seq_only_long = 0,
    seq_long_start = 1,
    seq_eight_short = 2,
    seq_long_stop = 3
};

enum Codebook {
    cb_zero = 0,
    cb_escape = 11,
    cb_reserved = 12,
    cb_noise = 13,
    cb_intensity2 = 14,
    cb_intensity = 15
};

enum ChannelBit : std::uint32_t {
    spk_fl = 0x1,
    spk_fr = 0x2,
    spk_fc = 0x4,
    spk_lfe = 0x8,
    spk_bl = 0x10,
    spk_br = 0x20,
    spk_flc = 0x40,
    spk_frc = 0x80,
    spk_bc = 0x100,
    spk_sl = 0x200,
    spk_sr = 0x400,
    spk_tfl = 0x1000,
    spk_tfr = 0x4000
};

double bessel_i0(double x) {
    double sum = 1.0;
    double term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 200; ++k) {
        term *= q / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

std::vector<float> sine_half(int full) {
    std::vector<float> w(static_cast<std::size_t>(full / 2));
    for (int n = 0; n < full / 2; ++n) w[static_cast<std::size_t>(n)] = static_cast<float>(std::sin(k_pi / full * (n + 0.5)));
    return w;
}

std::vector<float> kbd_half(int full, double alpha) {
    const int half = full / 2;
    std::vector<double> kernel(static_cast<std::size_t>(half + 1));
    double total = 0.0;
    const double quarter = full / 4.0;
    for (int n = 0; n <= half; ++n) {
        const double r = (n - quarter) / quarter;
        kernel[static_cast<std::size_t>(n)] = bessel_i0(k_pi * alpha * std::sqrt(std::max(0.0, 1.0 - r * r)));
        total += kernel[static_cast<std::size_t>(n)];
    }
    std::vector<float> w(static_cast<std::size_t>(half));
    double acc = 0.0;
    for (int n = 0; n < half; ++n) {
        acc += kernel[static_cast<std::size_t>(n)];
        w[static_cast<std::size_t>(n)] = static_cast<float>(std::sqrt(acc / total));
    }
    return w;
}

struct WindowSet {
    int length = 0;
    std::vector<float> long_half[2];
    std::vector<float> short_half[2];
    Imdct long_mdct;
    Imdct short_mdct;

    explicit WindowSet(int n) : length(n) {
        long_half[0] = sine_half(2 * n);
        long_half[1] = kbd_half(2 * n, 4.0);
        short_half[0] = sine_half(n / 4);
        short_half[1] = kbd_half(n / 4, 6.0);
        long_mdct.init(static_cast<std::size_t>(n));
        short_mdct.init(static_cast<std::size_t>(n / 8));
    }
};

const float* pow43_table() {
    static const std::vector<float> table = [] {
        std::vector<float> t(8192 + 16);
        for (std::size_t i = 0; i < t.size(); ++i) t[i] = static_cast<float>(std::pow(static_cast<double>(i), 4.0 / 3.0));
        return t;
    }();
    return table.data();
}

float pow43(int q) {
    const int a = q < 0 ? -q : q;
    const float v = a < 8192 + 16 ? pow43_table()[a] : static_cast<float>(std::pow(static_cast<double>(a), 4.0 / 3.0));
    return q < 0 ? -v : v;
}

float round_mantissa(float x, int mode) {
    std::uint32_t u;
    std::memcpy(&u, &x, 4);
    if (mode == 0) {
        u = (u + 0x00008000u) & 0xFFFF0000u;
    } else if (mode == 1) {
        u = (u + 0x00007FFFu + ((u >> 16) & 1u)) & 0xFFFF0000u;
    } else {
        u &= 0xFFFF0000u;
    }
    float r;
    std::memcpy(&r, &u, 4);
    return r;
}

struct Predictor {
    float r0 = 0.0f;
    float r1 = 0.0f;
    float cor0 = 0.0f;
    float cor1 = 0.0f;
    float var0 = 1.0f;
    float var1 = 1.0f;

    void clear() {
        r0 = r1 = cor0 = cor1 = 0.0f;
        var0 = var1 = 1.0f;
    }

    void run(float& value, bool apply) {
        const float a = 0.953125f;
        const float alpha = 0.90625f;
        const float k1 = var0 > 1.0f ? cor0 * round_mantissa(a / var0, 1) : 0.0f;
        const float k2 = var1 > 1.0f ? cor1 * round_mantissa(a / var1, 1) : 0.0f;
        const float estimate = round_mantissa(k1 * r0 + k2 * r1, 0);
        if (apply) value += estimate;
        const float e0 = value;
        const float e1 = e0 - k1 * r0;
        cor1 = round_mantissa(alpha * cor1 + r1 * e1, 2);
        var1 = round_mantissa(alpha * var1 + 0.5f * (r1 * r1 + e1 * e1), 2);
        cor0 = round_mantissa(alpha * cor0 + r0 * e0, 2);
        var0 = round_mantissa(alpha * var0 + 0.5f * (r0 * r0 + e0 * e0), 2);
        r1 = round_mantissa(a * (r0 - k1 * e0), 2);
        r0 = round_mantissa(a * e0, 2);
    }
};

struct IcsInfo {
    int window_sequence = seq_only_long;
    int window_shape = 0;
    int max_sfb = 0;
    int num_windows = 1;
    int num_groups = 1;
    int group_len[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
    int num_swb = 0;
    const std::uint16_t* swb = nullptr;
    bool predictor_present = false;
    int predictor_reset_group = 0;
    bool prediction_used[64] = {};
    bool ltp_present[2] = { false, false };
    int ltp_lag[2] = { 0, 0 };
    int ltp_coef[2] = { 0, 0 };
    bool ltp_used[2][40] = {};
};

struct TnsData {
    int n_filt[8] = {};
    int length[8][4] = {};
    int order[8][4] = {};
    int direction[8][4] = {};
    float lpc[8][4][21] = {};
};

struct ChannelData {
    IcsInfo own_info;
    const IcsInfo* info = &own_info;
    int global_gain = 0;
    std::uint8_t cb[8][64] = {};
    int sf[8][64] = {};
    bool pulse_present = false;
    int pulse_start = 0;
    int pulse_count = 0;
    int pulse_offset[4] = {};
    int pulse_amp[4] = {};
    bool tns_present = false;
    TnsData tns;
    int quant[1024] = {};
    float coef[1024] = {};
    bool common = false;
    int ltp_index = 0;
    int ms_mask = 0;
    std::uint8_t ms_used[8][64] = {};
};

struct Slot {
    int type = elem_sce;
    int tag = 0;
    int out[2] = { -1, -1 };
    bool used = false;
};

struct ChannelHistory {
    std::vector<float> overlap;
    std::vector<float> time;
    int prev_shape = 0;
    std::vector<Predictor> predictors;
    std::vector<float> ltp;

    void init(int n) {
        overlap.assign(static_cast<std::size_t>(n), 0.0f);
        time.assign(static_cast<std::size_t>(n), 0.0f);
        ltp.assign(static_cast<std::size_t>(3 * n), 0.0f);
        prev_shape = 0;
        predictors.assign(1024, Predictor());
    }
};

std::uint32_t lcg_next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}


} // namespace

struct CoreDecoder::State {
    StreamConfig cfg;
    StreamConfig initial;
    bool configured = false;
    int frame = 1024;
    std::unique_ptr<WindowSet> windows;
    std::uint16_t swb_long_storage[64] = {};
    std::uint16_t swb_short_storage[20] = {};
    SwbLayout long_layout;
    SwbLayout short_layout;
    int tns_long = 0;
    int tns_short = 0;
    int pred_max = 0;
    std::vector<Slot> slots;
    std::vector<std::uint32_t> slot_masks;
    bool layout_fixed = false;
    int core_channels = 0;
    std::vector<int> out_order;
    std::uint32_t mask = 0;
    std::vector<ChannelHistory> history;
    std::vector<ChannelData> data;
    ChannelData scratch[2];
    std::uint32_t noise_state = 0x1f2e3d4cu;
    bool sbr_on = false;
    bool sbr_downsampled = false;
    bool ps_on = false;
    std::uint32_t sbr_rate = 0;
    std::vector<std::unique_ptr<SbrElement>> sbr;
    std::vector<bool> sbr_seen;
    std::vector<float> sbr_out;
    int decoded_frames = 0;

    int out_channels() const {
        if (ps_on) return 2;
        return core_channels;
    }
    int out_length() const {
        return sbr_on && !sbr_downsampled ? frame * 2 : frame;
    }
};

namespace {

using State = CoreDecoder::State;

bool fail(std::string* error, const char* msg) {
    if (error && error->empty()) *error = msg;
    return false;
}

void build_swb(State& s) {
    const SwbLayout l = swb_long(s.cfg.sampling_index);
    const SwbLayout sh = swb_short(s.cfg.sampling_index);
    const int n = s.frame;
    int count = 0;
    s.swb_long_storage[0] = 0;
    for (int i = 1; i <= l.count; ++i) {
        const int v = std::min<int>(l.offsets[i], n);
        if (v <= s.swb_long_storage[count]) continue;
        s.swb_long_storage[++count] = static_cast<std::uint16_t>(v);
        if (v == n) break;
    }
    s.long_layout.count = count;
    s.long_layout.offsets = s.swb_long_storage;
    const int ns = n / 8;
    count = 0;
    s.swb_short_storage[0] = 0;
    for (int i = 1; i <= sh.count; ++i) {
        const int v = std::min<int>(sh.offsets[i], ns);
        if (v <= s.swb_short_storage[count]) continue;
        s.swb_short_storage[++count] = static_cast<std::uint16_t>(v);
        if (v == ns) break;
    }
    s.short_layout.count = count;
    s.short_layout.offsets = s.swb_short_storage;
}

void add_slot(State& s, int type, int tag, int a, int b) {
    Slot slot;
    slot.type = type;
    slot.tag = tag;
    slot.out[0] = a;
    slot.out[1] = b;
    s.slots.push_back(slot);
}

void finish_layout(State& s, const std::vector<std::uint32_t>& masks) {
    s.core_channels = static_cast<int>(masks.size());
    s.slot_masks = masks;
    std::vector<int> idx(masks.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
    std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        const std::uint32_t ma = masks[static_cast<std::size_t>(a)];
        const std::uint32_t mb = masks[static_cast<std::size_t>(b)];
        if (ma == 0 || mb == 0) return ma != 0 && mb == 0;
        return ma < mb;
    });
    std::vector<int> position(masks.size());
    for (std::size_t i = 0; i < idx.size(); ++i) position[static_cast<std::size_t>(idx[i])] = static_cast<int>(i);
    for (Slot& slot : s.slots) {
        for (int c = 0; c < 2; ++c) {
            if (slot.out[c] >= 0) slot.out[c] = position[static_cast<std::size_t>(slot.out[c])];
        }
    }
    s.mask = 0;
    for (std::uint32_t m : masks) s.mask |= m;
    s.layout_fixed = true;
}

bool build_config_layout(State& s, int config) {
    s.slots.clear();
    std::vector<std::uint32_t> m;
    switch (config) {
    case 1:
        add_slot(s, elem_sce, 0, 0, -1);
        m = { spk_fc };
        break;
    case 2:
        add_slot(s, elem_cpe, 0, 0, 1);
        m = { spk_fl, spk_fr };
        break;
    case 3:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        m = { spk_fc, spk_fl, spk_fr };
        break;
    case 4:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_sce, 1, 3, -1);
        m = { spk_fc, spk_fl, spk_fr, spk_bc };
        break;
    case 5:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_cpe, 1, 3, 4);
        m = { spk_fc, spk_fl, spk_fr, spk_bl, spk_br };
        break;
    case 6:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_cpe, 1, 3, 4);
        add_slot(s, elem_lfe, 0, 5, -1);
        m = { spk_fc, spk_fl, spk_fr, spk_bl, spk_br, spk_lfe };
        break;
    case 7:
    case 12:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_cpe, 1, 3, 4);
        add_slot(s, elem_cpe, 2, 5, 6);
        add_slot(s, elem_lfe, 0, 7, -1);
        m = { spk_fc, spk_fl, spk_fr, spk_sl, spk_sr, spk_bl, spk_br, spk_lfe };
        break;
    case 11:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_cpe, 1, 3, 4);
        add_slot(s, elem_sce, 1, 5, -1);
        add_slot(s, elem_lfe, 0, 6, -1);
        m = { spk_fc, spk_fl, spk_fr, spk_sl, spk_sr, spk_bc, spk_lfe };
        break;
    case 14:
        add_slot(s, elem_sce, 0, 0, -1);
        add_slot(s, elem_cpe, 0, 1, 2);
        add_slot(s, elem_cpe, 1, 3, 4);
        add_slot(s, elem_lfe, 0, 5, -1);
        add_slot(s, elem_cpe, 2, 6, 7);
        m = { spk_fc, spk_fl, spk_fr, spk_bl, spk_br, spk_lfe, spk_tfl, spk_tfr };
        break;
    default:
        return false;
    }
    finish_layout(s, m);
    return true;
}

void build_pce_layout(State& s, const ProgramConfig& pce) {
    s.slots.clear();
    std::vector<std::uint32_t> m;
    std::uint32_t taken = 0;
    auto claim = [&](std::uint32_t want) -> std::uint32_t {
        if (want && !(taken & want)) {
            taken |= want;
            return want;
        }
        return 0;
    };
    int front_channels = 0;
    for (const ProgramConfig::Entry& e : pce.entries) {
        if (e.position == pce_front) front_channels += e.type == elem_cpe ? 2 : 1;
    }
    bool center_pending = (front_channels & 1) != 0;
    int front_pairs = 0;
    int back_pairs = 0;
    int back_pair_total = 0;
    for (const ProgramConfig::Entry& e : pce.entries) {
        if (e.position == pce_back && e.type == elem_cpe) ++back_pair_total;
    }
    for (const ProgramConfig::Entry& e : pce.entries) {
        if (e.position == pce_cc) continue;
        const int base = static_cast<int>(m.size());
        if (e.type == elem_cpe) {
            std::uint32_t a = 0;
            std::uint32_t b = 0;
            if (e.position == pce_front) {
                if (front_pairs == 0) {
                    a = claim(spk_fl);
                    b = claim(spk_fr);
                } else if (front_pairs == 1) {
                    a = claim(spk_flc);
                    b = claim(spk_frc);
                }
                ++front_pairs;
            } else if (e.position == pce_side) {
                a = claim(spk_sl);
                b = claim(spk_sr);
            } else {
                const bool surround_first = back_pair_total > 1 && back_pairs == 0 && !(taken & spk_sl);
                if (surround_first) {
                    a = claim(spk_sl);
                    b = claim(spk_sr);
                } else {
                    a = claim(spk_bl);
                    b = claim(spk_br);
                    if (!a && !b) {
                        a = claim(spk_sl);
                        b = claim(spk_sr);
                    }
                }
                ++back_pairs;
            }
            m.push_back(a);
            m.push_back(b);
            add_slot(s, elem_cpe, e.tag, base, base + 1);
        } else {
            std::uint32_t a = 0;
            if (e.type == elem_lfe) {
                a = claim(spk_lfe);
            } else if (e.position == pce_front) {
                if (center_pending) {
                    a = claim(spk_fc);
                    center_pending = false;
                } else {
                    a = claim(spk_fc);
                }
            } else if (e.position == pce_back) {
                a = claim(spk_bc);
            }
            m.push_back(a);
            add_slot(s, e.type == elem_lfe ? elem_lfe : elem_sce, e.tag, base, -1);
        }
    }
    finish_layout(s, m);
}

void setup_buffers(State& s) {
    const int n = s.core_channels;
    s.history.resize(static_cast<std::size_t>(n));
    for (ChannelHistory& h : s.history) h.init(s.frame);
    s.data.resize(static_cast<std::size_t>(n));
}

void configure_sbr(State& s) {
    s.sbr.clear();
    s.sbr_seen.clear();
    if (!s.sbr_on) {
        s.ps_on = false;
        return;
    }
    const bool mono = s.slots.size() == 1 && s.slots[0].type == elem_sce;
    s.ps_on = mono && s.cfg.ps != 0;
    for (const Slot& slot : s.slots) {
        std::unique_ptr<SbrElement> e(new SbrElement());
        e->setup(slot.type == elem_cpe ? 2 : 1, s.sbr_rate, s.sbr_downsampled, s.ps_on);
        s.sbr.push_back(std::move(e));
        s.sbr_seen.push_back(false);
    }
}

void activate_sbr(State& s) {
    s.sbr_on = true;
    s.sbr_rate = 2 * s.cfg.sample_rate;
    s.sbr_downsampled = (s.cfg.ext_sample_rate != 0 && s.cfg.ext_sample_rate == s.cfg.sample_rate) || s.sbr_rate > 96000;
    configure_sbr(s);
}

bool parse_ics_info(State& s, BitReader& br, IcsInfo& info, bool common_window, std::string* error) {
    if (br.bit()) return fail(error, "reserved bit set in ics_info");
    info.window_sequence = static_cast<int>(br.read(2));
    info.window_shape = br.bit();
    info.predictor_present = false;
    info.ltp_present[0] = info.ltp_present[1] = false;
    if (info.window_sequence == seq_eight_short) {
        info.max_sfb = static_cast<int>(br.read(4));
        const int grouping = static_cast<int>(br.read(7));
        info.num_windows = 8;
        info.num_groups = 1;
        info.group_len[0] = 1;
        for (int w = 1; w < 8; ++w) {
            if (grouping & (1 << (7 - w))) {
                ++info.group_len[info.num_groups - 1];
            } else {
                info.group_len[info.num_groups++] = 1;
            }
        }
        info.num_swb = s.short_layout.count;
        info.swb = s.short_layout.offsets;
    } else {
        info.max_sfb = static_cast<int>(br.read(6));
        info.num_windows = 1;
        info.num_groups = 1;
        info.group_len[0] = 1;
        info.num_swb = s.long_layout.count;
        info.swb = s.long_layout.offsets;
        if (br.bit()) {
            if (s.cfg.object_type == aot_main) {
                info.predictor_present = true;
                info.predictor_reset_group = 0;
                if (br.bit()) {
                    info.predictor_reset_group = static_cast<int>(br.read(5));
                    if (info.predictor_reset_group == 0 || info.predictor_reset_group > 30) return fail(error, "invalid predictor reset group");
                }
                const int limit = std::min(info.max_sfb, s.pred_max);
                for (int sfb = 0; sfb < limit; ++sfb) info.prediction_used[sfb] = br.bit() != 0;
                for (int sfb = limit; sfb < 64; ++sfb) info.prediction_used[sfb] = false;
            } else if (s.cfg.object_type == aot_ltp) {
                for (int c = 0; c < (common_window ? 2 : 1); ++c) {
                    info.ltp_present[c] = br.bit() != 0;
                    if (info.ltp_present[c]) {
                        info.ltp_lag[c] = static_cast<int>(br.read(11));
                        info.ltp_coef[c] = static_cast<int>(br.read(3));
                        const int limit = std::min(info.max_sfb, 40);
                        for (int sfb = 0; sfb < 40; ++sfb) info.ltp_used[c][sfb] = sfb < limit && br.bit() != 0;
                        if (info.ltp_lag[c] > 2048) return fail(error, "invalid LTP lag");
                    }
                }
            } else {
                return fail(error, "prediction is not allowed in this object type");
            }
        }
    }
    if (info.max_sfb > info.num_swb) return fail(error, "max_sfb exceeds the band count");
    return !br.overrun();
}

bool parse_section_data(BitReader& br, ChannelData& ch, std::string* error) {
    const IcsInfo& info = *ch.info;
    const int bits = info.window_sequence == seq_eight_short ? 3 : 5;
    const int esc = (1 << bits) - 1;
    for (int g = 0; g < info.num_groups; ++g) {
        int k = 0;
        while (k < info.max_sfb) {
            const int cb = static_cast<int>(br.read(4));
            if (cb == cb_reserved) return fail(error, "reserved codebook");
            int len = 0;
            int incr;
            do {
                incr = static_cast<int>(br.read(bits));
                len += incr;
                if (br.overrun()) return fail(error, "truncated section data");
            } while (incr == esc);
            if (k + len > info.max_sfb) return fail(error, "section exceeds max_sfb");
            for (int sfb = k; sfb < k + len; ++sfb) ch.cb[g][sfb] = static_cast<std::uint8_t>(cb);
            k += len;
            if (len == 0 && br.overrun()) return fail(error, "truncated section data");
        }
        for (int sfb = info.max_sfb; sfb < 64; ++sfb) ch.cb[g][sfb] = cb_zero;
    }
    return !br.overrun();
}

bool parse_scalefactors(BitReader& br, ChannelData& ch, std::string* error) {
    const IcsInfo& info = *ch.info;
    const Huffman& h = scalefactor_huffman();
    int sf = ch.global_gain;
    int is_pos = 0;
    int noise = ch.global_gain - 90;
    bool first_noise = true;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            const int cb = ch.cb[g][sfb];
            if (cb == cb_zero) {
                ch.sf[g][sfb] = 0;
            } else if (cb == cb_intensity || cb == cb_intensity2) {
                const int d = h.decode(br);
                if (d < 0) return fail(error, "bad intensity position code");
                is_pos += d - 60;
                ch.sf[g][sfb] = is_pos;
            } else if (cb == cb_noise) {
                if (first_noise) {
                    first_noise = false;
                    noise += static_cast<int>(br.read(9)) - 256;
                } else {
                    const int d = h.decode(br);
                    if (d < 0) return fail(error, "bad noise energy code");
                    noise += d - 60;
                }
                ch.sf[g][sfb] = noise;
            } else {
                const int d = h.decode(br);
                if (d < 0) return fail(error, "bad scalefactor code");
                sf += d - 60;
                if (sf < 0 || sf > 255) return fail(error, "scalefactor out of range");
                ch.sf[g][sfb] = sf;
            }
        }
    }
    return !br.overrun();
}

bool parse_tns(BitReader& br, ChannelData& ch, int object_type, std::string* error) {
    const IcsInfo& info = *ch.info;
    const bool is_short = info.window_sequence == seq_eight_short;
    const int max_order = is_short ? 7 : (object_type == aot_main ? 20 : 12);
    TnsData& t = ch.tns;
    for (int w = 0; w < info.num_windows; ++w) {
        t.n_filt[w] = static_cast<int>(br.read(is_short ? 1 : 2));
        if (!t.n_filt[w]) continue;
        const int coef_res = br.bit();
        for (int f = 0; f < t.n_filt[w]; ++f) {
            t.length[w][f] = static_cast<int>(br.read(is_short ? 4 : 6));
            const int order = static_cast<int>(br.read(is_short ? 3 : 5));
            if (order > max_order) return fail(error, "TNS order too high");
            t.order[w][f] = order;
            if (!order) continue;
            t.direction[w][f] = br.bit();
            const int compress = br.bit();
            const int res_bits = coef_res + 3;
            const int bits = res_bits - compress;
            const double iq = ((1 << (res_bits - 1)) - 0.5) / (k_pi / 2.0);
            const double iqm = ((1 << (res_bits - 1)) + 0.5) / (k_pi / 2.0);
            double parcor[20];
            for (int i = 0; i < order; ++i) {
                int c = static_cast<int>(br.read(bits));
                if (c & (1 << (bits - 1))) c -= 1 << bits;
                parcor[i] = std::sin(c / (c >= 0 ? iq : iqm));
            }
            double a[21] = { 1.0 };
            double b[21];
            for (int m = 1; m <= order; ++m) {
                for (int i = 1; i < m; ++i) b[i] = a[i] + parcor[m - 1] * a[m - i];
                for (int i = 1; i < m; ++i) a[i] = b[i];
                a[m] = parcor[m - 1];
            }
            for (int i = 0; i <= order; ++i) t.lpc[w][f][i] = static_cast<float>(a[i]);
        }
    }
    return !br.overrun();
}

bool parse_spectral(BitReader& br, ChannelData& ch, std::string* error) {
    const IcsInfo& info = *ch.info;
    std::memset(ch.quant, 0, sizeof(ch.quant));
    const int win_len = info.window_sequence == seq_eight_short ? 128 : 1024;
    int win = 0;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            const int cb = ch.cb[g][sfb];
            if (cb == cb_zero || cb >= cb_noise) continue;
            const Huffman& h = spectral_huffman(cb);
            const int start = info.swb[sfb];
            const int end = info.swb[sfb + 1];
            const bool unsigned_cb = cb == 3 || cb == 4 || cb >= 7;
            const int dim = cb < 5 ? 4 : 2;
            for (int w = 0; w < info.group_len[g]; ++w) {
                int* q = ch.quant + (win + w) * win_len;
                for (int k = start; k < end; k += dim) {
                    const int sym = h.decode(br);
                    if (sym < 0) return fail(error, "bad spectral codeword");
                    int v[4];
                    if (dim == 4) {
                        v[0] = sym / 27;
                        v[1] = (sym / 9) % 3;
                        v[2] = (sym / 3) % 3;
                        v[3] = sym % 3;
                        if (!unsigned_cb) {
                            for (int i = 0; i < 4; ++i) v[i] -= 1;
                        }
                    } else {
                        const int mod = cb < 7 ? 9 : (cb < 9 ? 8 : (cb < 11 ? 13 : 17));
                        v[0] = sym / mod;
                        v[1] = sym % mod;
                        if (!unsigned_cb) {
                            v[0] -= 4;
                            v[1] -= 4;
                        }
                    }
                    if (unsigned_cb) {
                        for (int i = 0; i < dim; ++i) {
                            if (v[i] && br.bit()) v[i] = -v[i];
                        }
                    }
                    if (cb == cb_escape) {
                        for (int i = 0; i < 2; ++i) {
                            if (v[i] != 16 && v[i] != -16) continue;
                            int n = 0;
                            while (br.bit()) {
                                if (++n > 8) return fail(error, "bad escape sequence");
                            }
                            const int mag = (1 << (n + 4)) + static_cast<int>(br.read(n + 4));
                            v[i] = v[i] < 0 ? -mag : mag;
                        }
                    }
                    for (int i = 0; i < dim && k + i < end; ++i) q[k + i] = v[i];
                }
            }
        }
        win += info.group_len[g];
    }
    return !br.overrun();
}

bool parse_ics(State& s, BitReader& br, ChannelData& ch, bool common_window, bool scale_flag, std::string* error) {
    ch.global_gain = static_cast<int>(br.read(8));
    if (!common_window && !scale_flag) {
        ch.info = &ch.own_info;
        if (!parse_ics_info(s, br, ch.own_info, false, error)) return false;
    }
    if (!parse_section_data(br, ch, error)) return false;
    if (!parse_scalefactors(br, ch, error)) return false;
    ch.pulse_present = false;
    ch.tns_present = false;
    if (!scale_flag) {
        ch.pulse_present = br.bit() != 0;
        if (ch.pulse_present) {
            if (ch.info->window_sequence == seq_eight_short) return fail(error, "pulse data in a short window");
            ch.pulse_count = static_cast<int>(br.read(2)) + 1;
            ch.pulse_start = static_cast<int>(br.read(6));
            if (ch.pulse_start >= ch.info->num_swb) return fail(error, "pulse start out of range");
            for (int i = 0; i < ch.pulse_count; ++i) {
                ch.pulse_offset[i] = static_cast<int>(br.read(5));
                ch.pulse_amp[i] = static_cast<int>(br.read(4));
            }
        }
        ch.tns_present = br.bit() != 0;
        if (ch.tns_present && !parse_tns(br, ch, s.cfg.object_type, error)) return false;
        if (br.bit()) return fail(error, "gain control is not supported");
    }
    if (!parse_spectral(br, ch, error)) return false;
    return !br.overrun();
}

bool is_intensity(int cb) {
    return cb == cb_intensity || cb == cb_intensity2;
}

void dequantize(State& s, ChannelData& ch) {
    const IcsInfo& info = *ch.info;
    if (ch.pulse_present) {
        int k = info.swb[ch.pulse_start];
        for (int i = 0; i < ch.pulse_count; ++i) {
            k += ch.pulse_offset[i];
            if (k >= s.frame) break;
            if (ch.quant[k] > 0) ch.quant[k] += ch.pulse_amp[i];
            else ch.quant[k] -= ch.pulse_amp[i];
        }
    }
    std::memset(ch.coef, 0, sizeof(ch.coef));
    const int win_len = info.window_sequence == seq_eight_short ? 128 : 1024;
    int win = 0;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            const int cb = ch.cb[g][sfb];
            if (cb == cb_zero || cb >= cb_noise) continue;
            const float gain = static_cast<float>(std::pow(2.0, 0.25 * (ch.sf[g][sfb] - 100)));
            for (int w = 0; w < info.group_len[g]; ++w) {
                const int base = (win + w) * win_len;
                for (int k = info.swb[sfb]; k < info.swb[sfb + 1]; ++k) ch.coef[base + k] = pow43(ch.quant[base + k]) * gain;
            }
        }
        win += info.group_len[g];
    }
}

void fill_noise(State& s, ChannelData& ch, const ChannelData* partner, const std::uint8_t (*ms_used)[64]) {
    const IcsInfo& info = *ch.info;
    const int win_len = info.window_sequence == seq_eight_short ? 128 : 1024;
    int win = 0;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            if (ch.cb[g][sfb] != cb_noise) continue;
            const bool correlated = partner && ms_used && ms_used[g][sfb] && partner->cb[g][sfb] == cb_noise;
            const int start = info.swb[sfb];
            const int end = info.swb[sfb + 1];
            const double scale_db = std::pow(2.0, 0.25 * ch.sf[g][sfb]);
            for (int w = 0; w < info.group_len[g]; ++w) {
                float* c = ch.coef + (win + w) * win_len;
                if (correlated) {
                    for (int k = start; k < end; ++k) lcg_next(s.noise_state);
                    const float* p = partner->coef + (win + w) * win_len;
                    const double ps = std::pow(2.0, 0.25 * partner->sf[g][sfb]);
                    const double ratio = ps > 0.0 ? scale_db / ps : 0.0;
                    for (int k = start; k < end; ++k) c[k] = static_cast<float>(p[k] * ratio);
                    continue;
                }
                double energy = 0.0;
                for (int k = start; k < end; ++k) {
                    const float r = static_cast<float>(static_cast<std::int32_t>(lcg_next(s.noise_state)));
                    c[k] = r;
                    energy += static_cast<double>(r) * r;
                }
                const double scale = energy > 0.0 ? scale_db / std::sqrt(energy) : 0.0;
                for (int k = start; k < end; ++k) c[k] = static_cast<float>(c[k] * scale);
            }
        }
        win += info.group_len[g];
    }
}

void apply_intensity(ChannelData& l, ChannelData& r) {
    const IcsInfo& info = *l.info;
    const int win_len = info.window_sequence == seq_eight_short ? 128 : 1024;
    int win = 0;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            const int rcb = r.cb[g][sfb];
            if (!is_intensity(rcb)) continue;
            const int start = info.swb[sfb];
            const int end = info.swb[sfb + 1];
            float scale = static_cast<float>(std::pow(0.5, 0.25 * r.sf[g][sfb]));
            if (rcb == cb_intensity2) scale = -scale;
            if (r.ms_mask == 1 && r.ms_used[g][sfb]) scale = -scale;
            for (int w = 0; w < info.group_len[g]; ++w) {
                const float* lc = l.coef + (win + w) * win_len;
                float* rc = r.coef + (win + w) * win_len;
                for (int k = start; k < end; ++k) rc[k] = lc[k] * scale;
            }
        }
        win += info.group_len[g];
    }
}

void apply_mid_side(ChannelData& l, ChannelData& r, const std::uint8_t (*ms_used)[64]) {
    const IcsInfo& info = *l.info;
    const int win_len = info.window_sequence == seq_eight_short ? 128 : 1024;
    int win = 0;
    for (int g = 0; g < info.num_groups; ++g) {
        for (int sfb = 0; sfb < info.max_sfb; ++sfb) {
            const int rcb = r.cb[g][sfb];
            const int start = info.swb[sfb];
            const int end = info.swb[sfb + 1];
            if (is_intensity(rcb)) continue;
            if (!ms_used[g][sfb]) continue;
            if (l.cb[g][sfb] == cb_noise || rcb == cb_noise) continue;
            for (int w = 0; w < info.group_len[g]; ++w) {
                float* lc = l.coef + (win + w) * win_len;
                float* rc = r.coef + (win + w) * win_len;
                for (int k = start; k < end; ++k) {
                    const float m = lc[k];
                    const float sd = rc[k];
                    lc[k] = m + sd;
                    rc[k] = m - sd;
                }
            }
        }
        win += info.group_len[g];
    }
}

void apply_prediction(State& s, ChannelData& ch, ChannelHistory& h) {
    const IcsInfo& info = *ch.info;
    if (info.window_sequence == seq_eight_short) {
        for (Predictor& p : h.predictors) p.clear();
        return;
    }
    const int limit = std::min(s.pred_max, info.num_swb);
    for (int sfb = 0; sfb < limit; ++sfb) {
        const bool noise = sfb < info.max_sfb && (ch.cb[0][sfb] == cb_noise || is_intensity(ch.cb[0][sfb]));
        const bool apply = info.predictor_present && info.prediction_used[sfb];
        for (int k = info.swb[sfb]; k < info.swb[sfb + 1] && k < 1024; ++k) {
            h.predictors[static_cast<std::size_t>(k)].run(ch.coef[k], apply && !noise);
        }
    }
    if (info.predictor_present && info.predictor_reset_group) {
        for (int k = info.predictor_reset_group - 1; k < 1024; k += 30) h.predictors[static_cast<std::size_t>(k)].clear();
    }
}

void apply_tns_spectrum(State& s, const ChannelData& ch, float* coef, bool analysis) {
    if (!ch.tns_present) return;
    const IcsInfo& info = *ch.info;
    const bool is_short = info.window_sequence == seq_eight_short;
    const int win_len = is_short ? s.frame / 8 : s.frame;
    const int max_bands = is_short ? s.tns_short : s.tns_long;
    const int limit = std::min(max_bands, info.max_sfb);
    const TnsData& t = ch.tns;
    for (int w = 0; w < info.num_windows; ++w) {
        int top = info.num_swb;
        float* spec = coef + w * (is_short ? 128 : 1024);
        for (int f = 0; f < t.n_filt[w]; ++f) {
            const int bottom = std::max(top - t.length[w][f], 0);
            const int order = t.order[w][f];
            const int lo = info.swb[std::min(bottom, limit)];
            const int hi = info.swb[std::min(top, limit)];
            top = bottom;
            if (!order || hi <= lo) continue;
            const int size = std::min(hi, win_len) - lo;
            if (size <= 0) continue;
            int pos = t.direction[w][f] ? std::min(hi, win_len) - 1 : lo;
            const int inc = t.direction[w][f] ? -1 : 1;
            float state[20] = {};
            const float* a = t.lpc[w][f];
            for (int m = 0; m < size; ++m, pos += inc) {
                const float x = spec[pos];
                float y = x;
                if (analysis) {
                    for (int j = 0; j < order; ++j) y += a[j + 1] * state[j];
                    for (int j = order - 1; j > 0; --j) state[j] = state[j - 1];
                    state[0] = x;
                } else {
                    for (int j = 0; j < order; ++j) y -= a[j + 1] * state[j];
                    for (int j = order - 1; j > 0; --j) state[j] = state[j - 1];
                    state[0] = y;
                }
                spec[pos] = y;
            }
        }
    }
}

void apply_tns(State& s, ChannelData& ch) {
    apply_tns_spectrum(s, ch, ch.coef, false);
}

void synthesize(State& s, ChannelData& ch, ChannelHistory& h) {
    const IcsInfo& info = *ch.info;
    WindowSet& ws = *s.windows;
    const int n = s.frame;
    const int ns = n / 8;
    std::vector<float> buf(static_cast<std::size_t>(2 * n), 0.0f);
    const float* lprev = ws.long_half[h.prev_shape].data();
    const float* lcur = ws.long_half[info.window_shape].data();
    const float* sprev = ws.short_half[h.prev_shape].data();
    const float* scur = ws.short_half[info.window_shape].data();
    if (info.window_sequence == seq_eight_short) {
        std::vector<float> tmp(static_cast<std::size_t>(2 * ns));
        const int off = n / 2 - ns / 2;
        for (int w = 0; w < 8; ++w) {
            ws.short_mdct.transform(ch.coef + w * 128, tmp.data());
            const float* rise = w == 0 ? sprev : scur;
            float* dst = buf.data() + off + w * ns;
            for (int i = 0; i < ns; ++i) {
                dst[i] += tmp[static_cast<std::size_t>(i)] * rise[i];
                dst[ns + i] += tmp[static_cast<std::size_t>(ns + i)] * scur[ns - 1 - i];
            }
        }
    } else {
        ws.long_mdct.transform(ch.coef, buf.data());
        float* b = buf.data();
        if (info.window_sequence == seq_long_stop) {
            const int zeros = n / 2 - ns / 2;
            for (int i = 0; i < zeros; ++i) b[i] = 0.0f;
            for (int i = 0; i < ns; ++i) b[zeros + i] *= sprev[i];
        } else {
            for (int i = 0; i < n; ++i) b[i] *= lprev[i];
        }
        if (info.window_sequence == seq_long_start) {
            const int ones = n / 2 - ns / 2;
            for (int i = 0; i < ns; ++i) b[n + ones + i] *= scur[ns - 1 - i];
            for (int i = n + ones + ns; i < 2 * n; ++i) b[i] = 0.0f;
        } else {
            for (int i = 0; i < n; ++i) b[n + i] *= lcur[n - 1 - i];
        }
    }
    for (int i = 0; i < n; ++i) {
        h.time[static_cast<std::size_t>(i)] = buf[static_cast<std::size_t>(i)] + h.overlap[static_cast<std::size_t>(i)];
        h.overlap[static_cast<std::size_t>(i)] = buf[static_cast<std::size_t>(n + i)];
    }
    if (s.cfg.object_type == aot_ltp) {
        float* l = h.ltp.data();
        std::memmove(l, l + n, sizeof(float) * static_cast<std::size_t>(n));
        std::memcpy(l + n, h.time.data(), sizeof(float) * static_cast<std::size_t>(n));
        std::memcpy(l + 2 * n, h.overlap.data(), sizeof(float) * static_cast<std::size_t>(n));
    }
    h.prev_shape = info.window_shape;
}

void long_window(State& s, int sequence, int prev_shape, int cur_shape, float* w) {
    const WindowSet& ws = *s.windows;
    const int n = s.frame;
    const int ns = n / 8;
    const float* lprev = ws.long_half[prev_shape].data();
    const float* lcur = ws.long_half[cur_shape].data();
    const float* sprev = ws.short_half[prev_shape].data();
    const float* scur = ws.short_half[cur_shape].data();
    const int edge = n / 2 - ns / 2;
    for (int i = 0; i < n; ++i) {
        if (sequence == seq_long_stop) w[i] = i < edge ? 0.0f : (i < edge + ns ? sprev[i - edge] : 1.0f);
        else w[i] = lprev[i];
        if (sequence == seq_long_start) w[n + i] = i < edge ? 1.0f : (i < edge + ns ? scur[ns - 1 - (i - edge)] : 0.0f);
        else w[n + i] = lcur[n - 1 - i];
    }
}

void apply_ltp(State& s, ChannelData& ch, ChannelHistory& h) {
    const IcsInfo& info = *ch.info;
    const int c = ch.ltp_index;
    if (info.window_sequence == seq_eight_short || !info.ltp_present[c]) return;
    static const float coefs[8] = { 0.570829f, 0.696616f, 0.813004f, 0.911304f, 0.984900f, 1.067894f, 1.194601f, 1.369533f };
    const int n = s.frame;
    const float coef = coefs[info.ltp_coef[c] & 7];
    const int lag = info.ltp_lag[c];
    std::vector<float> pred(static_cast<std::size_t>(2 * n));
    std::vector<float> w(static_cast<std::size_t>(2 * n));
    long_window(s, info.window_sequence, h.prev_shape, info.window_shape, w.data());
    for (int i = 0; i < 2 * n; ++i) {
        const int src = i + 2 * n - lag;
        pred[static_cast<std::size_t>(i)] = src >= 0 && src < 3 * n ? coef * h.ltp[static_cast<std::size_t>(src)] * w[static_cast<std::size_t>(i)] : 0.0f;
    }
    std::vector<float> est(static_cast<std::size_t>(n));
    s.windows->long_mdct.forward(pred.data(), est.data());
    if (ch.tns_present) apply_tns_spectrum(s, ch, est.data(), true);
    const int limit = std::min(info.max_sfb, 40);
    for (int sfb = 0; sfb < limit; ++sfb) {
        if (!info.ltp_used[c][sfb]) continue;
        for (int k = info.swb[sfb]; k < info.swb[sfb + 1]; ++k) ch.coef[k] += est[static_cast<std::size_t>(k)];
    }
}

int find_slot(State& s, int type, int tag) {
    for (std::size_t i = 0; i < s.slots.size(); ++i) {
        const Slot& slot = s.slots[i];
        if (!slot.used && slot.type == type && slot.tag == tag) return static_cast<int>(i);
    }
    for (std::size_t i = 0; i < s.slots.size(); ++i) {
        const Slot& slot = s.slots[i];
        if (!slot.used && slot.type == type) return static_cast<int>(i);
    }
    if (!s.layout_fixed && (type == elem_sce || type == elem_cpe || type == elem_lfe)) {
        const int base = s.core_channels;
        add_slot(s, type, tag, base, type == elem_cpe ? base + 1 : -1);
        s.core_channels += type == elem_cpe ? 2 : 1;
        return static_cast<int>(s.slots.size() - 1);
    }
    return -1;
}

void finalize_dynamic_layout(State& s) {
    if (s.layout_fixed) return;
    const std::vector<Slot> before = s.slots;
    std::vector<std::uint32_t> masks(static_cast<std::size_t>(s.core_channels), 0);
    int sce = 0, cpe = 0, lfe = 0;
    for (const Slot& slot : s.slots) {
        if (slot.type == elem_sce) ++sce;
        else if (slot.type == elem_cpe) ++cpe;
        else ++lfe;
    }
    if (sce == 1 && cpe == 0 && lfe == 0) {
        masks[0] = spk_fc;
    } else if (sce == 0 && cpe == 1 && lfe == 0) {
        masks[0] = spk_fl;
        masks[1] = spk_fr;
    } else if (sce == 2 && cpe == 0 && lfe == 0) {
        masks[0] = spk_fl;
        masks[1] = spk_fr;
    } else {
        std::uint32_t taken = 0;
        const std::uint32_t pair_order[3][2] = { { spk_fl, spk_fr }, { spk_bl, spk_br }, { spk_sl, spk_sr } };
        int pairs = 0;
        bool center = false;
        for (const Slot& slot : s.slots) {
            if (slot.type == elem_cpe && pairs < 3) {
                masks[static_cast<std::size_t>(slot.out[0])] = pair_order[pairs][0];
                masks[static_cast<std::size_t>(slot.out[1])] = pair_order[pairs][1];
                taken |= pair_order[pairs][0] | pair_order[pairs][1];
                ++pairs;
            } else if (slot.type == elem_sce && !center) {
                masks[static_cast<std::size_t>(slot.out[0])] = spk_fc;
                center = true;
            } else if (slot.type == elem_lfe && !(taken & spk_lfe)) {
                masks[static_cast<std::size_t>(slot.out[0])] = spk_lfe;
                taken |= spk_lfe;
            }
        }
    }
    std::vector<ChannelData> old = s.data;
    finish_layout(s, masks);
    setup_buffers(s);
    for (std::size_t i = 0; i < s.slots.size(); ++i) {
        for (int c = 0; c < 2; ++c) {
            const int from = before[i].out[c];
            const int to = s.slots[i].out[c];
            if (from >= 0 && to >= 0 && static_cast<std::size_t>(from) < old.size()) {
                s.data[static_cast<std::size_t>(to)] = old[static_cast<std::size_t>(from)];
                s.data[static_cast<std::size_t>(to)].info = &s.data[static_cast<std::size_t>(to)].own_info;
            }
        }
    }
    if (s.sbr_on) configure_sbr(s);
}

bool skip_cce(State& s, BitReader& br, std::string* error) {
    br.skip(4);
    const int ind_sw = br.bit();
    const int num_coupled = static_cast<int>(br.read(3));
    int num_gain_lists = 0;
    for (int c = 0; c <= num_coupled; ++c) {
        ++num_gain_lists;
        const int is_cpe = br.bit();
        br.skip(4);
        if (is_cpe) {
            const int l = br.bit();
            const int r = br.bit();
            if (l && r) ++num_gain_lists;
        }
    }
    br.skip(1);
    br.skip(1);
    br.skip(2);
    ChannelData& ch = s.scratch[0];
    if (!parse_ics(s, br, ch, false, false, error)) return false;
    const Huffman& h = scalefactor_huffman();
    for (int c = 1; c < num_gain_lists; ++c) {
        int cge = 1;
        if (!ind_sw) cge = br.bit();
        if (cge) {
            if (h.decode(br) < 0) return fail(error, "bad coupling gain");
        } else {
            for (int g = 0; g < ch.info->num_groups; ++g) {
                for (int sfb = 0; sfb < ch.info->max_sfb; ++sfb) {
                    if (ch.cb[g][sfb] != cb_zero && h.decode(br) < 0) return fail(error, "bad coupling gain");
                }
            }
        }
    }
    return !br.overrun();
}

bool skip_dse(BitReader& br, std::size_t align_ref) {
    br.skip(4);
    const int align = br.bit();
    std::size_t count = br.read(8);
    if (count == 255) count += br.read(8);
    if (align) {
        const std::size_t rel = (br.pos() - align_ref) & 7;
        if (rel) br.skip(8 - rel);
    }
    br.skip(count * 8);
    return !br.overrun();
}

void decode_channel_tail(State& s, int out, ChannelData& ch, bool predict) {
    ChannelHistory& h = s.history[static_cast<std::size_t>(out)];
    if (predict && s.cfg.object_type == aot_main) apply_prediction(s, ch, h);
    if (s.cfg.object_type == aot_ltp) apply_ltp(s, ch, h);
    apply_tns(s, ch);
    synthesize(s, ch, h);
}

} // namespace

CoreDecoder::CoreDecoder() : m_state(new State()) {}
CoreDecoder::~CoreDecoder() = default;

bool CoreDecoder::configure(const StreamConfig& cfg, std::string* error) {
    State& s = *m_state;
    s = State();
    s.cfg = cfg;
    s.initial = cfg;
    if (cfg.object_type != aot_main && cfg.object_type != aot_lc && cfg.object_type != aot_ltp) return fail(error, "unsupported audio object type");
    if (cfg.sampling_index < 0 || cfg.sampling_index > 12) return fail(error, "invalid sampling index");
    if (cfg.sample_rate == 0) return fail(error, "invalid sample rate");
    s.frame = cfg.frame_length_short ? 960 : 1024;
    if (cfg.frame_length_short) {
        s.cfg.sbr = 0;
        s.cfg.ps = 0;
    }
    static const int tns_long[13] = { 31, 31, 34, 40, 42, 51, 46, 46, 42, 42, 42, 39, 39 };
    static const int tns_short[13] = { 9, 9, 10, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14 };
    s.tns_long = tns_long[cfg.sampling_index];
    s.tns_short = tns_short[cfg.sampling_index];
    s.pred_max = pred_sfb_max(cfg.sampling_index);
    build_swb(s);
    s.windows.reset(new WindowSet(s.frame));
    if (cfg.has_pce) {
        build_pce_layout(s, cfg.pce);
        if (s.core_channels == 0) return fail(error, "program config element has no channels");
    } else if (cfg.channel_config != 0) {
        if (!build_config_layout(s, cfg.channel_config)) return fail(error, "unsupported channel configuration");
    }
    if (s.layout_fixed) setup_buffers(s);
    if (s.cfg.sbr == 1) activate_sbr(s);
    s.configured = true;
    return true;
}

void CoreDecoder::reset() {
    State& s = *m_state;
    if (!s.configured) return;
    const StreamConfig original = s.initial;
    std::string err;
    configure(original, &err);
}

const StreamConfig& CoreDecoder::stream() const noexcept {
    return m_state->cfg;
}

bool CoreDecoder::sbr_active() const noexcept {
    return m_state->sbr_on;
}

bool CoreDecoder::ps_active() const noexcept {
    return m_state->ps_on;
}

int CoreDecoder::output_channels() const noexcept {
    return m_state->out_channels();
}

std::uint32_t CoreDecoder::output_rate() const noexcept {
    const State& s = *m_state;
    if (s.sbr_on && !s.sbr_downsampled) return s.sbr_rate;
    return s.cfg.sample_rate;
}

std::uint32_t CoreDecoder::output_frame_length() const noexcept {
    return static_cast<std::uint32_t>(m_state->out_length());
}

std::uint32_t CoreDecoder::extension_rate() const noexcept {
    const State& s = *m_state;
    if (s.sbr_on) return s.sbr_downsampled ? s.cfg.sample_rate : s.sbr_rate;
    return s.cfg.ext_sample_rate;
}

std::uint32_t CoreDecoder::channel_mask() const noexcept {
    const State& s = *m_state;
    if (s.ps_on) return spk_fl | spk_fr;
    return s.mask;
}

std::uint32_t CoreDecoder::delay() const noexcept {
    return m_state->sbr_on ? 962u : 0u;
}

void CoreDecoder::append_silence(std::vector<float>& out) const {
    const State& s = *m_state;
    const std::size_t n = static_cast<std::size_t>(s.out_length()) * static_cast<std::size_t>(s.out_channels());
    out.insert(out.end(), n, 0.0f);
}

bool CoreDecoder::decode_block(BitReader& br, std::size_t align_ref, std::vector<float>& out, std::string* error) {
    State& s = *m_state;
    if (!s.configured) {
        if (error) *error = "decoder is not configured";
        return false;
    }
    for (Slot& slot : s.slots) slot.used = false;
    for (std::size_t i = 0; i < s.sbr_seen.size(); ++i) s.sbr_seen[i] = false;
    std::vector<int> decoded_slots;
    int last_slot = -1;
    int last_type = -1;
    bool ok = true;
    std::string err;
    bool ended = false;
    std::vector<int> pending_sbr_slots;
    while (ok && !ended) {
        if (br.left() < 3) {
            ok = fail(&err, "raw data block is truncated");
            break;
        }
        const int id = static_cast<int>(br.read(3));
        switch (id) {
        case elem_sce:
        case elem_lfe: {
            const int tag = static_cast<int>(br.read(4));
            const int si = find_slot(s, id, tag);
            ChannelData& ch = s.scratch[0];
            ch.info = &ch.own_info;
            if (!parse_ics(s, br, ch, false, false, &err)) {
                ok = false;
                break;
            }
            dequantize(s, ch);
            fill_noise(s, ch, nullptr, nullptr);
            if (si < 0) {
                last_slot = -1;
                last_type = id;
                break;
            }
            Slot& slot = s.slots[static_cast<std::size_t>(si)];
            slot.used = true;
            if (static_cast<int>(s.data.size()) < s.core_channels) s.data.resize(static_cast<std::size_t>(s.core_channels));
            ChannelData& dst = s.data[static_cast<std::size_t>(slot.out[0])];
            dst = ch;
            dst.info = &dst.own_info;
            dst.ltp_index = 0;
            decoded_slots.push_back(si);
            last_slot = si;
            last_type = id;
            break;
        }
        case elem_cpe: {
            const int tag = static_cast<int>(br.read(4));
            const int si = find_slot(s, elem_cpe, tag);
            ChannelData& l = s.scratch[0];
            ChannelData& r = s.scratch[1];
            const bool common = br.bit() != 0;
            int ms_mask = 0;
            std::uint8_t ms_used[8][64] = {};
            if (common) {
                l.info = &l.own_info;
                if (!parse_ics_info(s, br, l.own_info, true, &err)) {
                    ok = false;
                    break;
                }
                r.own_info = l.own_info;
                r.info = &r.own_info;
                ms_mask = static_cast<int>(br.read(2));
                if (ms_mask == 3) {
                    ok = fail(&err, "reserved ms_mask_present");
                    break;
                }
                if (ms_mask == 1) {
                    for (int g = 0; g < l.own_info.num_groups; ++g) {
                        for (int sfb = 0; sfb < l.own_info.max_sfb; ++sfb) ms_used[g][sfb] = static_cast<std::uint8_t>(br.bit());
                    }
                } else if (ms_mask == 2) {
                    for (int g = 0; g < 8; ++g) {
                        for (int sfb = 0; sfb < 64; ++sfb) ms_used[g][sfb] = 1;
                    }
                }
            }
            if (!parse_ics(s, br, l, common, false, &err) || !parse_ics(s, br, r, common, false, &err)) {
                ok = false;
                break;
            }
            if (common) {
                l.info = &l.own_info;
                r.info = &r.own_info;
            }
            dequantize(s, l);
            dequantize(s, r);
            fill_noise(s, l, nullptr, nullptr);
            fill_noise(s, r, common ? &l : nullptr, common ? ms_used : nullptr);
            l.common = r.common = common;
            r.ms_mask = ms_mask;
            std::memcpy(r.ms_used, ms_used, sizeof(ms_used));
            if (common) apply_mid_side(l, r, ms_used);
            last_type = elem_cpe;
            if (si < 0) {
                last_slot = -1;
                break;
            }
            Slot& slot = s.slots[static_cast<std::size_t>(si)];
            slot.used = true;
            if (static_cast<int>(s.data.size()) < s.core_channels) s.data.resize(static_cast<std::size_t>(s.core_channels));
            ChannelData& dl = s.data[static_cast<std::size_t>(slot.out[0])];
            ChannelData& dr = s.data[static_cast<std::size_t>(slot.out[1])];
            dl = l;
            dl.info = &dl.own_info;
            dl.ltp_index = 0;
            dr = r;
            dr.info = &dr.own_info;
            dr.ltp_index = common ? 1 : 0;
            decoded_slots.push_back(-1 - si);
            last_slot = si;
            break;
        }
        case elem_cce:
            ok = skip_cce(s, br, &err);
            last_slot = -1;
            last_type = elem_cce;
            break;
        case elem_dse:
            ok = skip_dse(br, align_ref);
            break;
        case elem_pce: {
            ProgramConfig pce;
            if (!parse_program_config(br, align_ref, pce)) {
                ok = fail(&err, "invalid program config element");
                break;
            }
            if (!s.cfg.has_pce && s.cfg.channel_config == 0 && s.decoded_frames == 0 && decoded_slots.empty()) {
                s.cfg.has_pce = true;
                s.cfg.pce = pce;
                build_pce_layout(s, pce);
                setup_buffers(s);
                if (s.sbr_on) configure_sbr(s);
            }
            break;
        }
        case elem_fil: {
            std::size_t count = br.read(4);
            if (count == 15) count += br.read(8) - 1;
            const std::size_t end = br.pos() + count * 8;
            if (static_cast<long long>(count * 8) > br.left()) {
                ok = fail(&err, "fill element exceeds the block");
                break;
            }
            if (count > 0) {
                const int type = static_cast<int>(br.read(4));
                if ((type == 13 || type == 14) && s.cfg.sbr != 0 && last_slot >= 0 && last_type != elem_lfe && last_type != elem_cce) {
                    if (!s.sbr_on) {
                        if (!s.layout_fixed) {
                            br.seek(end);
                            pending_sbr_slots.push_back(last_slot);
                            break;
                        }
                        activate_sbr(s);
                    }
                    if (last_slot < static_cast<int>(s.sbr.size())) {
                        SbrElement& e = *s.sbr[static_cast<std::size_t>(last_slot)];
                        if (e.parse(br, end, type == 14, last_type == elem_cpe)) s.sbr_seen[static_cast<std::size_t>(last_slot)] = true;
                    }
                }
            }
            br.seek(end);
            break;
        }
        case elem_end:
            ended = true;
            break;
        default:
            break;
        }
        if (ok && br.overrun()) ok = fail(&err, "raw data block is truncated");
    }
    if (!s.layout_fixed) {
        if (s.core_channels == 0) {
            if (error) *error = err.empty() ? "no audio elements" : err;
            return false;
        }
        finalize_dynamic_layout(s);
        if (ok && !pending_sbr_slots.empty() && s.cfg.sbr != 0) activate_sbr(s);
    }
    if (!ok) {
        for (ChannelHistory& h : s.history) {
            std::fill(h.overlap.begin(), h.overlap.end(), 0.0f);
            std::fill(h.time.begin(), h.time.end(), 0.0f);
        }
        if (error) *error = err.empty() ? "decode error" : err;
    } else {
        for (int code : decoded_slots) {
            const int si = code < 0 ? -1 - code : code;
            const Slot& slot = s.slots[static_cast<std::size_t>(si)];
            if (slot.type == elem_cpe) {
                ChannelData& l = s.data[static_cast<std::size_t>(slot.out[0])];
                ChannelData& r = s.data[static_cast<std::size_t>(slot.out[1])];
                if (s.cfg.object_type == aot_main) {
                    apply_prediction(s, l, s.history[static_cast<std::size_t>(slot.out[0])]);
                    apply_prediction(s, r, s.history[static_cast<std::size_t>(slot.out[1])]);
                }
                if (r.common) apply_intensity(l, r);
                decode_channel_tail(s, slot.out[0], l, false);
                decode_channel_tail(s, slot.out[1], r, false);
            } else {
                decode_channel_tail(s, slot.out[0], s.data[static_cast<std::size_t>(slot.out[0])], true);
            }
        }
        for (const Slot& slot : s.slots) {
            if (slot.used) continue;
            for (int c = 0; c < 2; ++c) {
                if (slot.out[c] < 0) continue;
                ChannelHistory& h = s.history[static_cast<std::size_t>(slot.out[c])];
                for (int i = 0; i < s.frame; ++i) {
                    h.time[static_cast<std::size_t>(i)] = h.overlap[static_cast<std::size_t>(i)];
                    h.overlap[static_cast<std::size_t>(i)] = 0.0f;
                }
            }
        }
    }
    const int len = s.out_length();
    const int chans = s.out_channels();
    const std::size_t base = out.size();
    out.resize(base + static_cast<std::size_t>(len) * static_cast<std::size_t>(chans), 0.0f);
    float* dst = out.data() + base;
    const float scale = 1.0f / 32768.0f;
    if (!s.sbr_on) {
        if (ok) {
            for (int c = 0; c < chans; ++c) {
                const float* src = s.history[static_cast<std::size_t>(c)].time.data();
                for (int i = 0; i < len; ++i) dst[static_cast<std::size_t>(i) * chans + c] = src[i] * scale;
            }
        }
    } else {
        s.sbr_out.assign(static_cast<std::size_t>(len) * 2, 0.0f);
        std::vector<float> zero(static_cast<std::size_t>(s.frame), 0.0f);
        for (std::size_t si = 0; si < s.slots.size() && si < s.sbr.size(); ++si) {
            const Slot& slot = s.slots[si];
            SbrElement& e = *s.sbr[si];
            if (!s.sbr_seen[si]) e.no_data();
            const float* in[2] = { zero.data(), zero.data() };
            for (int c = 0; c < 2; ++c) {
                if (slot.out[c] >= 0 && ok) in[c] = s.history[static_cast<std::size_t>(slot.out[c])].time.data();
            }
            float* o[2] = { s.sbr_out.data(), s.sbr_out.data() + len };
            e.process(in, o);
            if (s.ps_on) {
                for (int i = 0; i < len; ++i) {
                    dst[static_cast<std::size_t>(i) * 2] = o[0][i] * scale;
                    dst[static_cast<std::size_t>(i) * 2 + 1] = o[1][i] * scale;
                }
            } else {
                for (int c = 0; c < 2; ++c) {
                    if (slot.out[c] < 0) continue;
                    for (int i = 0; i < len; ++i) dst[static_cast<std::size_t>(i) * chans + slot.out[c]] = o[c][i] * scale;
                }
            }
        }
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(len) * static_cast<std::size_t>(chans); ++i) {
        if (!std::isfinite(dst[i])) dst[i] = 0.0f;
    }
    ++s.decoded_frames;
    return ok;
}

} // namespace aac_internal
} // namespace audio
} // namespace fizmo
