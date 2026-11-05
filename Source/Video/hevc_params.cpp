#include "hevc_internal.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

bool fail(std::string* error, const char* what) {
    if (error) *error = what;
    return false;
}

void parse_ptl(Bits& b, int max_sub_layers_minus1, ProfileTierLevel& ptl) {
    ptl.profile_space = static_cast<int>(b.u(2));
    ptl.tier = static_cast<int>(b.u(1));
    ptl.profile_idc = static_cast<int>(b.u(5));
    ptl.compat = b.u(32);
    ptl.progressive_source = b.flag();
    ptl.interlaced_source = b.flag();
    b.skip(1);
    ptl.frame_only = b.flag();
    b.skip(44);
    ptl.level_idc = static_cast<int>(b.u(8));
    bool prof[8] = {};
    bool lev[8] = {};
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        prof[i] = b.flag();
        lev[i] = b.flag();
    }
    if (max_sub_layers_minus1 > 0) {
        for (int i = max_sub_layers_minus1; i < 8; ++i) b.skip(2);
    }
    for (int i = 0; i < max_sub_layers_minus1; ++i) {
        if (prof[i]) b.skip(88);
        if (lev[i]) b.skip(8);
    }
}

void parse_sub_layer_hrd(Bits& b, int cpb_cnt, bool sub_pic) {
    for (int k = 0; k < cpb_cnt && !b.error(); ++k) {
        b.ue();
        b.ue();
        if (sub_pic) {
            b.ue();
            b.ue();
        }
        b.skip(1);
    }
}

void parse_hrd(Bits& b, bool common, int max_sub_layers_minus1) {
    bool nal = false;
    bool vcl = false;
    bool sub_pic = false;
    if (common) {
        nal = b.flag();
        vcl = b.flag();
        if (nal || vcl) {
            sub_pic = b.flag();
            if (sub_pic) b.skip(8 + 5 + 1 + 5);
            b.skip(8);
            if (sub_pic) b.skip(4);
            b.skip(15);
        }
    }
    for (int i = 0; i <= max_sub_layers_minus1 && !b.error(); ++i) {
        const bool fixed_general = b.flag();
        bool fixed_cvs = true;
        if (!fixed_general) fixed_cvs = b.flag();
        bool low_delay = false;
        if (fixed_cvs) {
            b.ue();
        } else {
            low_delay = b.flag();
        }
        std::uint32_t cpb_cnt = 1;
        if (!low_delay) cpb_cnt = b.ue() + 1;
        if (cpb_cnt > 32) {
            b.set_error();
            return;
        }
        if (nal) parse_sub_layer_hrd(b, static_cast<int>(cpb_cnt), sub_pic);
        if (vcl) parse_sub_layer_hrd(b, static_cast<int>(cpb_cnt), sub_pic);
    }
}

void parse_vui(Bits& b, Sps& s) {
    static const std::uint16_t sar_table[17][2] = {
        {0, 0}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
        {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1},
    };
    if (b.flag()) {
        const std::uint32_t idc = b.u(8);
        if (idc == 255) {
            s.sar_num = b.u(16);
            s.sar_den = b.u(16);
        } else if (idc > 0 && idc < 17) {
            s.sar_num = sar_table[idc][0];
            s.sar_den = sar_table[idc][1];
        }
        if (s.sar_num == 0 || s.sar_den == 0) {
            s.sar_num = 1;
            s.sar_den = 1;
        }
    }
    if (b.flag()) b.skip(1);
    if (b.flag()) {
        s.video_signal_present = true;
        s.video_format = static_cast<int>(b.u(3));
        s.full_range = b.flag();
        if (b.flag()) {
            s.colour_primaries = static_cast<int>(b.u(8));
            s.transfer = static_cast<int>(b.u(8));
            s.matrix = static_cast<int>(b.u(8));
        }
    }
    if (b.flag()) {
        const std::uint32_t top = b.ue();
        b.ue();
        s.chroma_loc = top < 6 ? static_cast<int>(top) : 0;
    }
    b.skip(1);
    s.field_seq = b.flag();
    s.frame_field_info = b.flag();
    if (b.flag()) {
        b.ue();
        b.ue();
        b.ue();
        b.ue();
    }
    if (b.flag()) {
        s.num_units_in_tick = b.u(32);
        s.time_scale = b.u(32);
        if (b.flag()) b.ue();
        if (b.flag()) parse_hrd(b, true, s.max_sub_layers - 1);
    }
    if (b.flag()) {
        b.skip(3);
        b.ue();
        b.ue();
        b.ue();
        b.ue();
        b.ue();
    }
}

} // namespace

bool parse_vps(Bits& b, Vps& vps) {
    vps.id = static_cast<int>(b.u(4));
    b.skip(2);
    b.skip(6);
    vps.max_sub_layers = static_cast<int>(b.u(3)) + 1;
    vps.valid = !b.error();
    return vps.valid;
}

bool parse_scaling_list(Bits& b, ScalingList& sl, bool chroma444) {
    (void)chroma444;
    std::uint8_t list[4][6][64];
    int dc[4][6];
    for (int size_id = 0; size_id < 4; ++size_id) {
        const int coef_num = std::min(64, 1 << (4 + (size_id << 1)));
        for (int m = 0; m < 6; m += (size_id == 3) ? 3 : 1) {
            const bool pred_mode = b.flag();
            if (!pred_mode) {
                std::uint32_t delta = b.ue();
                if (size_id == 3) delta *= 3;
                if (delta > static_cast<std::uint32_t>(m)) return false;
                if (delta == 0) {
                    for (int i = 0; i < coef_num; ++i) {
                        list[size_id][m][i] = size_id == 0 ? 16 : (m < 3 ? k_default_scaling_intra[i] : k_default_scaling_inter[i]);
                    }
                    dc[size_id][m] = 16;
                } else {
                    const int ref = m - static_cast<int>(delta);
                    std::memcpy(list[size_id][m], list[size_id][ref], 64);
                    dc[size_id][m] = dc[size_id][ref];
                }
            } else {
                int next = 8;
                if (size_id > 1) {
                    const int v = b.se();
                    if (v < -7 || v > 247) return false;
                    next = v + 8;
                    dc[size_id][m] = next;
                } else {
                    dc[size_id][m] = 16;
                }
                for (int i = 0; i < coef_num; ++i) {
                    const int d = b.se();
                    if (d < -128 || d > 127) return false;
                    next = (next + d + 256) % 256;
                    list[size_id][m][i] = static_cast<std::uint8_t>(next);
                }
            }
            if (b.error()) return false;
        }
    }
    for (int m = 1; m < 6; ++m) {
        if (m == 3) continue;
        std::memcpy(list[3][m], list[2][m], 64);
        dc[3][m] = dc[2][m];
    }
    const ScanTables& st = scan_tables();
    for (int m = 0; m < 6; ++m) {
        for (int i = 0; i < 16; ++i) {
            const int x = st.pos[2][0][i][0];
            const int y = st.pos[2][0][i][1];
            sl.factor[0][m][y * 4 + x] = list[0][m][i];
        }
        for (int size_id = 1; size_id < 4; ++size_id) {
            const int n = 4 << size_id;
            const int rep = n / 8;
            for (int i = 0; i < 64; ++i) {
                const int x = st.pos[3][0][i][0];
                const int y = st.pos[3][0][i][1];
                for (int dy = 0; dy < rep; ++dy) {
                    for (int dx = 0; dx < rep; ++dx) sl.factor[size_id][m][(y * rep + dy) * n + x * rep + dx] = list[size_id][m][i];
                }
            }
            if (size_id >= 2) sl.factor[size_id][m][0] = static_cast<std::uint8_t>(dc[size_id][m]);
        }
    }
    return true;
}

bool parse_st_rps(Bits& b, int idx, int num_sets, const std::vector<ShortTermRps>& sets, ShortTermRps& out) {
    out = ShortTermRps();
    bool inter = false;
    if (idx != 0) inter = b.flag();
    if (inter) {
        std::uint32_t delta_idx = 1;
        if (idx == num_sets) delta_idx = b.ue() + 1;
        if (delta_idx > static_cast<std::uint32_t>(idx)) return false;
        const int ref_idx = idx - static_cast<int>(delta_idx);
        if (ref_idx < 0 || ref_idx >= static_cast<int>(sets.size())) return false;
        const ShortTermRps& ref = sets[static_cast<std::size_t>(ref_idx)];
        const int sign = static_cast<int>(b.u(1));
        const std::uint32_t abs_minus1 = b.ue();
        if (abs_minus1 > 32767) return false;
        const int delta_rps = (1 - 2 * sign) * (static_cast<int>(abs_minus1) + 1);
        const int n = ref.num_delta();
        std::uint8_t used[33] = {};
        std::uint8_t use_delta[33] = {};
        for (int j = 0; j <= n; ++j) {
            used[j] = b.flag() ? 1 : 0;
            use_delta[j] = 1;
            if (!used[j]) use_delta[j] = b.flag() ? 1 : 0;
        }
        int i = 0;
        int s0[64];
        std::uint8_t u0[64];
        for (int j = ref.num_positive - 1; j >= 0; --j) {
            const int dpoc = ref.delta_poc[ref.num_negative + j] + delta_rps;
            if (dpoc < 0 && use_delta[ref.num_negative + j]) {
                s0[i] = dpoc;
                u0[i++] = used[ref.num_negative + j];
            }
        }
        if (delta_rps < 0 && use_delta[n]) {
            s0[i] = delta_rps;
            u0[i++] = used[n];
        }
        for (int j = 0; j < ref.num_negative; ++j) {
            const int dpoc = ref.delta_poc[j] + delta_rps;
            if (dpoc < 0 && use_delta[j]) {
                s0[i] = dpoc;
                u0[i++] = used[j];
            }
        }
        if (i > 16) return false;
        const int neg = i;
        i = 0;
        int s1[64];
        std::uint8_t u1[64];
        for (int j = ref.num_negative - 1; j >= 0; --j) {
            const int dpoc = ref.delta_poc[j] + delta_rps;
            if (dpoc > 0 && use_delta[j]) {
                s1[i] = dpoc;
                u1[i++] = used[j];
            }
        }
        if (delta_rps > 0 && use_delta[n]) {
            s1[i] = delta_rps;
            u1[i++] = used[n];
        }
        for (int j = 0; j < ref.num_positive; ++j) {
            const int dpoc = ref.delta_poc[ref.num_negative + j] + delta_rps;
            if (dpoc > 0 && use_delta[ref.num_negative + j]) {
                s1[i] = dpoc;
                u1[i++] = used[ref.num_negative + j];
            }
        }
        if (i > 16 || neg + i > 16) return false;
        out.num_negative = neg;
        out.num_positive = i;
        for (int k = 0; k < neg; ++k) {
            out.delta_poc[k] = s0[k];
            out.used[k] = u0[k];
        }
        for (int k = 0; k < i; ++k) {
            out.delta_poc[neg + k] = s1[k];
            out.used[neg + k] = u1[k];
        }
    } else {
        const std::uint32_t neg = b.ue();
        const std::uint32_t pos = b.ue();
        if (neg > 16 || pos > 16 || neg + pos > 16) return false;
        out.num_negative = static_cast<int>(neg);
        out.num_positive = static_cast<int>(pos);
        int prev = 0;
        for (std::uint32_t i = 0; i < neg; ++i) {
            const std::uint32_t d = b.ue();
            if (d > 32767) return false;
            prev -= static_cast<int>(d) + 1;
            out.delta_poc[i] = prev;
            out.used[i] = b.flag() ? 1 : 0;
        }
        prev = 0;
        for (std::uint32_t i = 0; i < pos; ++i) {
            const std::uint32_t d = b.ue();
            if (d > 32767) return false;
            prev += static_cast<int>(d) + 1;
            out.delta_poc[neg + i] = prev;
            out.used[neg + i] = b.flag() ? 1 : 0;
        }
    }
    return !b.error();
}

bool parse_sps(Bits& b, Sps& s, std::string* error) {
    s.vps_id = static_cast<int>(b.u(4));
    s.max_sub_layers = static_cast<int>(b.u(3)) + 1;
    if (s.max_sub_layers > 7) return fail(error, "invalid SPS sub-layer count");
    b.skip(1);
    parse_ptl(b, s.max_sub_layers - 1, s.ptl);
    const std::uint32_t id = b.ue();
    if (id >= static_cast<std::uint32_t>(k_max_sps)) return fail(error, "invalid SPS id");
    s.id = static_cast<int>(id);
    const std::uint32_t cf = b.ue();
    if (cf > 3) return fail(error, "invalid chroma_format_idc");
    s.chroma_format_idc = static_cast<int>(cf);
    if (cf == 3) s.separate_colour_plane = b.flag();
    s.chroma_array_type = s.separate_colour_plane ? 0 : s.chroma_format_idc;
    s.sub_width_c = (cf == 1 || cf == 2) ? 2 : 1;
    s.sub_height_c = cf == 1 ? 2 : 1;
    const std::uint32_t w = b.ue();
    const std::uint32_t h = b.ue();
    if (w == 0 || h == 0 || w > 16384 || h > 16384) return fail(error, "invalid picture size");
    s.width = static_cast<int>(w);
    s.height = static_cast<int>(h);
    if (b.flag()) {
        const std::uint32_t l = b.ue();
        const std::uint32_t r = b.ue();
        const std::uint32_t t = b.ue();
        const std::uint32_t bo = b.ue();
        const std::uint64_t sw = static_cast<std::uint64_t>(s.chroma_array_type ? s.sub_width_c : 1);
        const std::uint64_t sh = static_cast<std::uint64_t>(s.chroma_array_type ? s.sub_height_c : 1);
        if ((static_cast<std::uint64_t>(l) + r) * sw >= w || (static_cast<std::uint64_t>(t) + bo) * sh >= h) return fail(error, "invalid conformance window");
        s.conf_left = static_cast<int>(l * sw);
        s.conf_right = static_cast<int>(r * sw);
        s.conf_top = static_cast<int>(t * sh);
        s.conf_bottom = static_cast<int>(bo * sh);
    }
    const std::uint32_t bdy = b.ue();
    const std::uint32_t bdc = b.ue();
    if (bdy > 4 || bdc > 4) return fail(error, "unsupported HEVC bit depth (above 12 bits)");
    s.bit_depth_y = static_cast<int>(bdy) + 8;
    s.bit_depth_c = static_cast<int>(bdc) + 8;
    const std::uint32_t lsb = b.ue();
    if (lsb > 12) return fail(error, "invalid log2_max_pic_order_cnt_lsb");
    s.log2_max_poc_lsb = static_cast<int>(lsb) + 4;
    const bool ordering = b.flag();
    for (int i = ordering ? 0 : s.max_sub_layers - 1; i < s.max_sub_layers; ++i) {
        const std::uint32_t dpb = b.ue() + 1;
        const std::uint32_t reorder = b.ue();
        const std::uint32_t lat = b.ue();
        if (dpb > 16 || reorder > 16) return fail(error, "invalid SPS DPB size");
        s.max_dec_pic_buffering[i] = static_cast<int>(dpb);
        s.max_num_reorder[i] = static_cast<int>(std::min(reorder, dpb - 1));
        s.max_latency_increase_plus1[i] = static_cast<int>(std::min<std::uint32_t>(lat, 0x7FFFFFFF));
    }
    if (!ordering) {
        for (int i = 0; i < s.max_sub_layers - 1; ++i) {
            s.max_dec_pic_buffering[i] = s.max_dec_pic_buffering[s.max_sub_layers - 1];
            s.max_num_reorder[i] = s.max_num_reorder[s.max_sub_layers - 1];
            s.max_latency_increase_plus1[i] = s.max_latency_increase_plus1[s.max_sub_layers - 1];
        }
    }
    const std::uint32_t min_cb = b.ue() + 3;
    const std::uint32_t diff_cb = b.ue();
    const std::uint32_t min_tb = b.ue() + 2;
    const std::uint32_t diff_tb = b.ue();
    if (min_cb > 6 || diff_cb > 3 || min_cb + diff_cb > 6 || min_cb + diff_cb < 4) return fail(error, "invalid coding block sizes");
    if (min_tb >= min_cb || min_tb + diff_tb > 5 || min_tb + diff_tb > min_cb + diff_cb) return fail(error, "invalid transform block sizes");
    s.log2_min_cb = static_cast<int>(min_cb);
    s.log2_ctb = static_cast<int>(min_cb + diff_cb);
    s.log2_min_tb = static_cast<int>(min_tb);
    s.log2_max_tb = static_cast<int>(min_tb + diff_tb);
    const std::uint32_t dinter = b.ue();
    const std::uint32_t dintra = b.ue();
    if (dinter > static_cast<std::uint32_t>(s.log2_ctb - s.log2_min_tb) || dintra > static_cast<std::uint32_t>(s.log2_ctb - s.log2_min_tb)) return fail(error, "invalid transform hierarchy depth");
    s.max_th_depth_inter = static_cast<int>(dinter);
    s.max_th_depth_intra = static_cast<int>(dintra);
    if ((s.width & ((1 << s.log2_min_cb) - 1)) || (s.height & ((1 << s.log2_min_cb) - 1))) return fail(error, "picture size is not a multiple of the minimum coding block");
    s.scaling_list_enabled = b.flag();
    if (s.scaling_list_enabled) {
        s.scaling = std::make_shared<ScalingList>();
        if (b.flag()) {
            if (!parse_scaling_list(b, *s.scaling, s.chroma_array_type == 3)) return fail(error, "invalid SPS scaling list");
        } else {
            default_scaling_list(*s.scaling);
        }
    }
    s.amp = b.flag();
    s.sao = b.flag();
    s.pcm = b.flag();
    if (s.pcm) {
        s.pcm_bit_depth_y = static_cast<int>(b.u(4)) + 1;
        s.pcm_bit_depth_c = static_cast<int>(b.u(4)) + 1;
        const std::uint32_t pmin = b.ue() + 3;
        const std::uint32_t pdiff = b.ue();
        if (s.pcm_bit_depth_y > s.bit_depth_y || s.pcm_bit_depth_c > s.bit_depth_c) return fail(error, "invalid PCM bit depth");
        if (pmin + pdiff > static_cast<std::uint32_t>(std::min(s.log2_ctb, 5))) return fail(error, "invalid PCM block size");
        s.log2_min_pcm = static_cast<int>(pmin);
        s.log2_max_pcm = static_cast<int>(pmin + pdiff);
        s.pcm_loop_filter_disabled = b.flag();
    }
    const std::uint32_t nsets = b.ue();
    if (nsets > 64) return fail(error, "invalid short-term RPS count");
    s.st_rps.clear();
    for (std::uint32_t i = 0; i < nsets; ++i) {
        ShortTermRps r;
        if (!parse_st_rps(b, static_cast<int>(i), static_cast<int>(nsets), s.st_rps, r)) return fail(error, "invalid short-term RPS");
        s.st_rps.push_back(r);
    }
    s.long_term_refs_present = b.flag();
    if (s.long_term_refs_present) {
        const std::uint32_t n = b.ue();
        if (n > 32) return fail(error, "invalid long-term RPS count");
        s.num_lt_sps = static_cast<int>(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            s.lt_poc_lsb_sps[i] = static_cast<int>(b.u(s.log2_max_poc_lsb));
            s.lt_used_sps[i] = b.flag() ? 1 : 0;
        }
    }
    s.temporal_mvp = b.flag();
    s.strong_intra_smoothing = b.flag();
    if (b.error()) return fail(error, "truncated SPS");
    s.vui_present = b.flag();
    bool ext_ok = true;
    if (s.vui_present) {
        Sps saved = s;
        parse_vui(b, s);
        if (b.error()) {
            const std::uint32_t g = s.generation;
            s = saved;
            s.generation = g;
            ext_ok = false;
        }
    }
    if (ext_ok && b.flag()) {
        const bool range = b.flag();
        b.skip(7);
        if (range) {
            s.transform_skip_rotation = b.flag();
            s.transform_skip_context = b.flag();
            s.implicit_rdpcm = b.flag();
            s.explicit_rdpcm = b.flag();
            s.extended_precision = b.flag();
            s.intra_smoothing_disabled = b.flag();
            s.high_precision_offsets = b.flag();
            s.persistent_rice = b.flag();
            s.cabac_bypass_alignment = b.flag();
            if (!b.error() && s.extended_precision) return fail(error, "extended_precision_processing_flag is not supported");
            if (b.error()) {
                s.transform_skip_rotation = s.transform_skip_context = s.implicit_rdpcm = s.explicit_rdpcm = false;
                s.extended_precision = s.intra_smoothing_disabled = s.high_precision_offsets = false;
                s.persistent_rice = s.cabac_bypass_alignment = false;
            }
        }
    }
    s.ctb_size = 1 << s.log2_ctb;
    s.pic_w_ctb = (s.width + s.ctb_size - 1) >> s.log2_ctb;
    s.pic_h_ctb = (s.height + s.ctb_size - 1) >> s.log2_ctb;
    s.qp_bd_offset_y = 6 * (s.bit_depth_y - 8);
    s.qp_bd_offset_c = 6 * (s.bit_depth_c - 8);
    return true;
}

bool parse_pps(Bits& b, Pps& p, std::string* error) {
    const std::uint32_t id = b.ue();
    const std::uint32_t sid = b.ue();
    if (id >= static_cast<std::uint32_t>(k_max_pps) || sid >= static_cast<std::uint32_t>(k_max_sps)) return fail(error, "invalid PPS id");
    p.id = static_cast<int>(id);
    p.sps_id = static_cast<int>(sid);
    p.dependent_slice_segments = b.flag();
    p.output_flag_present = b.flag();
    p.num_extra_slice_header_bits = static_cast<int>(b.u(3));
    p.sign_data_hiding = b.flag();
    p.cabac_init_present = b.flag();
    const std::uint32_t r0 = b.ue() + 1;
    const std::uint32_t r1 = b.ue() + 1;
    if (r0 > 15 || r1 > 15) return fail(error, "invalid PPS reference count");
    p.num_ref_idx_default[0] = static_cast<int>(r0);
    p.num_ref_idx_default[1] = static_cast<int>(r1);
    const int qp = b.se();
    if (qp < -26 - 48 || qp > 25) return fail(error, "invalid PPS init_qp");
    p.init_qp = 26 + qp;
    p.constrained_intra_pred = b.flag();
    p.transform_skip = b.flag();
    p.cu_qp_delta = b.flag();
    if (p.cu_qp_delta) {
        const std::uint32_t d = b.ue();
        if (d > 3) return fail(error, "invalid diff_cu_qp_delta_depth");
        p.diff_cu_qp_delta_depth = static_cast<int>(d);
    }
    p.cb_qp_offset = b.se();
    p.cr_qp_offset = b.se();
    if (p.cb_qp_offset < -12 || p.cb_qp_offset > 12 || p.cr_qp_offset < -12 || p.cr_qp_offset > 12) return fail(error, "invalid PPS chroma QP offset");
    p.slice_chroma_qp_offsets_present = b.flag();
    p.weighted_pred = b.flag();
    p.weighted_bipred = b.flag();
    p.transquant_bypass = b.flag();
    p.tiles = b.flag();
    p.entropy_coding_sync = b.flag();
    p.col_width.clear();
    p.row_height.clear();
    if (p.tiles) {
        const std::uint32_t c = b.ue() + 1;
        const std::uint32_t r = b.ue() + 1;
        if (c > 64 || r > 64) return fail(error, "invalid tile count");
        p.num_tile_cols = static_cast<int>(c);
        p.num_tile_rows = static_cast<int>(r);
        p.uniform_spacing = b.flag();
        if (!p.uniform_spacing) {
            for (std::uint32_t i = 0; i + 1 < c; ++i) {
                const std::uint32_t v = b.ue();
                if (v > 1024) return fail(error, "invalid tile width");
                p.col_width.push_back(static_cast<int>(v) + 1);
            }
            for (std::uint32_t i = 0; i + 1 < r; ++i) {
                const std::uint32_t v = b.ue();
                if (v > 1024) return fail(error, "invalid tile height");
                p.row_height.push_back(static_cast<int>(v) + 1);
            }
        }
        p.loop_filter_across_tiles = b.flag();
    }
    p.loop_filter_across_slices = b.flag();
    p.deblocking_control_present = b.flag();
    if (p.deblocking_control_present) {
        p.deblocking_override_enabled = b.flag();
        p.deblocking_disabled = b.flag();
        if (!p.deblocking_disabled) {
            p.beta_offset_div2 = b.se();
            p.tc_offset_div2 = b.se();
            if (p.beta_offset_div2 < -6 || p.beta_offset_div2 > 6 || p.tc_offset_div2 < -6 || p.tc_offset_div2 > 6) return fail(error, "invalid deblocking offsets");
        }
    }
    p.scaling_list_present = b.flag();
    if (p.scaling_list_present) {
        p.scaling = std::make_shared<ScalingList>();
        if (!parse_scaling_list(b, *p.scaling, true)) return fail(error, "invalid PPS scaling list");
    }
    p.lists_modification_present = b.flag();
    const std::uint32_t pml = b.ue() + 2;
    if (pml > 6) return fail(error, "invalid parallel merge level");
    p.log2_parallel_merge_level = static_cast<int>(pml);
    p.slice_header_extension_present = b.flag();
    if (b.error()) return fail(error, "truncated PPS");
    if (b.flag()) {
        const bool range = b.flag();
        b.skip(7);
        if (range) {
            if (p.transform_skip) {
                const std::uint32_t v = b.ue();
                if (v > 3) return fail(error, "invalid transform skip size");
                p.log2_max_transform_skip_size = static_cast<int>(v) + 2;
            }
            p.cross_component_prediction = b.flag();
            p.chroma_qp_offset_list_enabled = b.flag();
            if (p.chroma_qp_offset_list_enabled) {
                const std::uint32_t d = b.ue();
                const std::uint32_t len = b.ue() + 1;
                if (d > 3 || len > 6) return fail(error, "invalid chroma QP offset list");
                p.diff_cu_chroma_qp_offset_depth = static_cast<int>(d);
                p.chroma_qp_offset_list_len = static_cast<int>(len);
                for (std::uint32_t i = 0; i < len; ++i) {
                    p.cb_qp_offset_list[i] = b.se();
                    p.cr_qp_offset_list[i] = b.se();
                    if (p.cb_qp_offset_list[i] < -12 || p.cb_qp_offset_list[i] > 12 || p.cr_qp_offset_list[i] < -12 || p.cr_qp_offset_list[i] > 12) return fail(error, "invalid chroma QP offset list");
                }
            }
            const std::uint32_t sl = b.ue();
            const std::uint32_t sc = b.ue();
            if (sl > 6 || sc > 6) return fail(error, "invalid SAO offset scale");
            p.log2_sao_offset_scale_luma = static_cast<int>(sl);
            p.log2_sao_offset_scale_chroma = static_cast<int>(sc);
            if (b.error()) return fail(error, "truncated PPS range extension");
        }
    }
    return true;
}

bool build_layout(const Sps& sps, const Pps& pps, Layout& lay) {
    const int w = sps.pic_w_ctb;
    const int h = sps.pic_h_ctb;
    const int cols = pps.tiles ? pps.num_tile_cols : 1;
    const int rows = pps.tiles ? pps.num_tile_rows : 1;
    if (cols > w || rows > h) return false;
    std::vector<int> cw(static_cast<std::size_t>(cols));
    std::vector<int> rh(static_cast<std::size_t>(rows));
    if (!pps.tiles || pps.uniform_spacing) {
        for (int i = 0; i < cols; ++i) cw[static_cast<std::size_t>(i)] = ((i + 1) * w) / cols - (i * w) / cols;
        for (int j = 0; j < rows; ++j) rh[static_cast<std::size_t>(j)] = ((j + 1) * h) / rows - (j * h) / rows;
    } else {
        if (static_cast<int>(pps.col_width.size()) != cols - 1 || static_cast<int>(pps.row_height.size()) != rows - 1) return false;
        int sum = 0;
        for (int i = 0; i < cols - 1; ++i) {
            cw[static_cast<std::size_t>(i)] = pps.col_width[static_cast<std::size_t>(i)];
            sum += cw[static_cast<std::size_t>(i)];
        }
        if (sum >= w) return false;
        cw[static_cast<std::size_t>(cols - 1)] = w - sum;
        sum = 0;
        for (int j = 0; j < rows - 1; ++j) {
            rh[static_cast<std::size_t>(j)] = pps.row_height[static_cast<std::size_t>(j)];
            sum += rh[static_cast<std::size_t>(j)];
        }
        if (sum >= h) return false;
        rh[static_cast<std::size_t>(rows - 1)] = h - sum;
    }
    lay.col_bd.assign(static_cast<std::size_t>(cols + 1), 0);
    lay.row_bd.assign(static_cast<std::size_t>(rows + 1), 0);
    for (int i = 0; i < cols; ++i) lay.col_bd[static_cast<std::size_t>(i + 1)] = lay.col_bd[static_cast<std::size_t>(i)] + cw[static_cast<std::size_t>(i)];
    for (int j = 0; j < rows; ++j) lay.row_bd[static_cast<std::size_t>(j + 1)] = lay.row_bd[static_cast<std::size_t>(j)] + rh[static_cast<std::size_t>(j)];
    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    lay.rs_to_ts.assign(n, 0);
    lay.ts_to_rs.assign(n, 0);
    lay.tile_id.assign(n, 0);
    lay.ctb_tile_x0.assign(n, 0);
    lay.ctb_tile_y0.assign(n, 0);
    for (int rs = 0; rs < static_cast<int>(n); ++rs) {
        const int tbx = rs % w;
        const int tby = rs / w;
        int tx = 0;
        int ty = 0;
        for (int i = 0; i < cols; ++i) {
            if (tbx >= lay.col_bd[static_cast<std::size_t>(i)]) tx = i;
        }
        for (int j = 0; j < rows; ++j) {
            if (tby >= lay.row_bd[static_cast<std::size_t>(j)]) ty = j;
        }
        int v = 0;
        for (int i = 0; i < tx; ++i) v += rh[static_cast<std::size_t>(ty)] * cw[static_cast<std::size_t>(i)];
        for (int j = 0; j < ty; ++j) v += w * rh[static_cast<std::size_t>(j)];
        v += (tby - lay.row_bd[static_cast<std::size_t>(ty)]) * cw[static_cast<std::size_t>(tx)] + tbx - lay.col_bd[static_cast<std::size_t>(tx)];
        lay.rs_to_ts[static_cast<std::size_t>(rs)] = v;
        lay.ts_to_rs[static_cast<std::size_t>(v)] = rs;
        lay.ctb_tile_x0[static_cast<std::size_t>(rs)] = lay.col_bd[static_cast<std::size_t>(tx)];
        lay.ctb_tile_y0[static_cast<std::size_t>(rs)] = lay.row_bd[static_cast<std::size_t>(ty)];
    }
    int tid = 0;
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < cols; ++i, ++tid) {
            for (int y = lay.row_bd[static_cast<std::size_t>(j)]; y < lay.row_bd[static_cast<std::size_t>(j + 1)]; ++y) {
                for (int x = lay.col_bd[static_cast<std::size_t>(i)]; x < lay.col_bd[static_cast<std::size_t>(i + 1)]; ++x) {
                    lay.tile_id[static_cast<std::size_t>(lay.rs_to_ts[static_cast<std::size_t>(y * w + x)])] = tid;
                }
            }
        }
    }
    lay.w4 = (w << sps.log2_ctb) >> 2;
    lay.h4 = (h << sps.log2_ctb) >> 2;
    lay.zs.assign(static_cast<std::size_t>(lay.w4) * static_cast<std::size_t>(lay.h4), 0);
    const int shift = sps.log2_ctb - 2;
    for (int y = 0; y < lay.h4; ++y) {
        for (int x = 0; x < lay.w4; ++x) {
            const int tbx = (x << 2) >> sps.log2_ctb;
            const int tby = (y << 2) >> sps.log2_ctb;
            const int rs = w * tby + tbx;
            std::uint32_t v = static_cast<std::uint32_t>(lay.rs_to_ts[static_cast<std::size_t>(rs)]) << (shift * 2);
            std::uint32_t p = 0;
            for (int i = 0; i < shift; ++i) {
                const std::uint32_t m = 1u << i;
                p += (m & static_cast<std::uint32_t>(x) ? m * m : 0) + (m & static_cast<std::uint32_t>(y) ? 2 * m * m : 0);
            }
            lay.zs[static_cast<std::size_t>(y) * static_cast<std::size_t>(lay.w4) + static_cast<std::size_t>(x)] = v + p;
        }
    }
    lay.sps_id = sps.id;
    lay.pps_id = pps.id;
    lay.sps_gen = sps.generation;
    lay.pps_gen = pps.generation;
    return true;
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
