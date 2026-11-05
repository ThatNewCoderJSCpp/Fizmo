#include "h264_internal.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace video {
namespace h264_internal {

namespace {

const std::uint16_t k_sar_table[17][2] = {
    {0, 1}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
    {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99}, {4, 3}, {3, 2}, {2, 1},
};

void set_error(std::string* error, const char* msg) {
    if (error) *error = msg;
}

bool parse_scaling_list(BitReader& br, int size, std::uint8_t* raster, bool* use_default) {
    int last = 8;
    int next = 8;
    *use_default = false;
    const std::uint8_t* scan = size == 16 ? k_zigzag4x4 : k_zigzag8x8;
    for (int j = 0; j < size; ++j) {
        if (next != 0) {
            std::int32_t delta = br.se();
            if (delta < -128 || delta > 127) return false;
            next = (last + delta + 256) % 256;
            if (j == 0 && next == 0) {
                *use_default = true;
                return true;
            }
        }
        int value = next == 0 ? last : next;
        raster[scan[j]] = static_cast<std::uint8_t>(value);
        last = value;
    }
    return true;
}

bool parse_hrd(BitReader& br, Vui* v) {
    std::uint32_t cpb_cnt = br.ue() + 1;
    if (cpb_cnt > 32) return false;
    br.u(4);
    br.u(4);
    for (std::uint32_t i = 0; i < cpb_cnt; ++i) {
        br.ue();
        br.ue();
        br.u(1);
    }
    br.u(5);
    int cpb = static_cast<int>(br.u(5)) + 1;
    int dpb = static_cast<int>(br.u(5)) + 1;
    int tol = static_cast<int>(br.u(5));
    if (v) {
        v->cpb_removal_delay_length = cpb;
        v->dpb_output_delay_length = dpb;
        v->time_offset_length = tol;
    }
    return !br.error();
}

void parse_vui(BitReader& br, Vui& v) {
    v.aspect_ratio_present = br.flag();
    if (v.aspect_ratio_present) {
        int idc = static_cast<int>(br.u(8));
        if (idc == 255) {
            v.sar_width = br.u(16);
            v.sar_height = br.u(16);
        } else if (idc < 17) {
            v.sar_width = k_sar_table[idc][0];
            v.sar_height = k_sar_table[idc][1];
        }
    }
    if (br.flag()) br.u(1);
    v.video_signal_present = br.flag();
    if (v.video_signal_present) {
        v.video_format = static_cast<int>(br.u(3));
        v.full_range = br.flag();
        v.colour_description_present = br.flag();
        if (v.colour_description_present) {
            v.colour_primaries = static_cast<int>(br.u(8));
            v.transfer_characteristics = static_cast<int>(br.u(8));
            v.matrix_coefficients = static_cast<int>(br.u(8));
        }
    }
    v.chroma_loc_present = br.flag();
    if (v.chroma_loc_present) {
        v.chroma_loc_top = static_cast<int>(std::min<std::uint32_t>(br.ue(), 5));
        v.chroma_loc_bottom = static_cast<int>(std::min<std::uint32_t>(br.ue(), 5));
    }
    v.timing_present = br.flag();
    if (v.timing_present) {
        v.num_units_in_tick = br.u(32);
        v.time_scale = br.u(32);
        v.fixed_frame_rate = br.flag();
    }
    bool nal_hrd = br.flag();
    v.nal_hrd = nal_hrd;
    if (nal_hrd && !parse_hrd(br, &v)) return;
    bool vcl_hrd = br.flag();
    v.vcl_hrd = vcl_hrd;
    if (vcl_hrd && !parse_hrd(br, &v)) return;
    if (nal_hrd || vcl_hrd) br.u(1);
    v.pic_struct_present = br.flag();
    bool restriction = br.flag();
    if (restriction) {
        br.u(1);
        br.ue();
        br.ue();
        br.ue();
        br.ue();
        std::uint32_t reorder = br.ue();
        std::uint32_t dec_buf = br.ue();
        if (!br.error() && reorder <= 16 && dec_buf <= 16) {
            v.bitstream_restriction = true;
            v.max_num_reorder_frames = static_cast<int>(reorder);
            v.max_dec_frame_buffering = static_cast<int>(dec_buf);
        }
    }
}

void fill_flat(std::uint8_t s4[6][16], std::uint8_t s8[6][64]) {
    std::memset(s4, 16, 6 * 16);
    std::memset(s8, 16, 6 * 64);
}

} // namespace

void parse_sps(BitReader& br, Sps& sps, std::string* error, bool* ok) {
    *ok = false;
    sps = Sps();
    sps.profile_idc = static_cast<int>(br.u(8));
    sps.constraint_flags = static_cast<int>(br.u(8));
    sps.level_idc = static_cast<int>(br.u(8));
    std::uint32_t id = br.ue();
    if (id > 31) {
        set_error(error, "SPS id out of range");
        return;
    }
    sps.id = static_cast<int>(id);
    fill_flat(sps.scaling4, sps.scaling8);
    int p = sps.profile_idc;
    if (p == 100 || p == 110 || p == 122 || p == 244 || p == 44 || p == 83 || p == 86 || p == 118 || p == 128 || p == 138 || p == 139 || p == 134 || p == 135) {
        std::uint32_t cf = br.ue();
        if (cf > 3) {
            set_error(error, "bad chroma_format_idc");
            return;
        }
        sps.chroma_format_idc = static_cast<int>(cf);
        if (cf == 3) sps.separate_colour_plane = br.flag();
        std::uint32_t bdl = br.ue();
        std::uint32_t bdc = br.ue();
        if (bdl > 6 || bdc > 6) {
            set_error(error, "bad bit depth");
            return;
        }
        sps.bit_depth_luma = static_cast<int>(bdl) + 8;
        sps.bit_depth_chroma = static_cast<int>(bdc) + 8;
        sps.transform_bypass = br.flag();
        sps.scaling_matrix_present = br.flag();
        if (sps.scaling_matrix_present) {
            int count = cf != 3 ? 8 : 12;
            for (int i = 0; i < count; ++i) {
                bool present = br.flag();
                bool use_default = false;
                if (i < 6) {
                    if (present) {
                        if (!parse_scaling_list(br, 16, sps.scaling4[i], &use_default)) {
                            set_error(error, "bad scaling list");
                            return;
                        }
                    }
                    if (!present) {
                        if (i == 0 || i == 3) {
                            std::memcpy(sps.scaling4[i], k_default_scaling4[i / 3], 16);
                        } else {
                            std::memcpy(sps.scaling4[i], sps.scaling4[i - 1], 16);
                        }
                    } else if (use_default) {
                        std::memcpy(sps.scaling4[i], k_default_scaling4[i / 3], 16);
                    }
                } else {
                    int k = i - 6;
                    if (present) {
                        if (!parse_scaling_list(br, 64, sps.scaling8[k], &use_default)) {
                            set_error(error, "bad scaling list");
                            return;
                        }
                    }
                    if (!present) {
                        if (k < 2) {
                            std::memcpy(sps.scaling8[k], k_default_scaling8[k], 64);
                        } else {
                            std::memcpy(sps.scaling8[k], sps.scaling8[k - 2], 64);
                        }
                    } else if (use_default) {
                        std::memcpy(sps.scaling8[k], k_default_scaling8[k & 1], 64);
                    }
                }
            }
        }
    }
    std::uint32_t lmfn = br.ue();
    if (lmfn > 12) {
        set_error(error, "bad log2_max_frame_num");
        return;
    }
    sps.log2_max_frame_num = static_cast<int>(lmfn) + 4;
    std::uint32_t poc_type = br.ue();
    if (poc_type > 2) {
        set_error(error, "bad pic_order_cnt_type");
        return;
    }
    sps.poc_type = static_cast<int>(poc_type);
    if (poc_type == 0) {
        std::uint32_t l = br.ue();
        if (l > 12) {
            set_error(error, "bad log2_max_pic_order_cnt_lsb");
            return;
        }
        sps.log2_max_poc_lsb = static_cast<int>(l) + 4;
    } else if (poc_type == 1) {
        sps.delta_pic_order_always_zero = br.flag();
        sps.offset_for_non_ref_pic = br.se();
        sps.offset_for_top_to_bottom_field = br.se();
        std::uint32_t n = br.ue();
        if (n > 255) {
            set_error(error, "bad num_ref_frames_in_pic_order_cnt_cycle");
            return;
        }
        sps.num_ref_frames_in_poc_cycle = static_cast<int>(n);
        for (std::uint32_t i = 0; i < n; ++i) sps.offset_for_ref_frame[i] = br.se();
    }
    std::uint32_t refs = br.ue();
    if (refs > 16) {
        set_error(error, "too many reference frames");
        return;
    }
    sps.max_num_ref_frames = static_cast<int>(refs);
    sps.gaps_allowed = br.flag();
    std::uint32_t w = br.ue() + 1;
    std::uint32_t h = br.ue() + 1;
    sps.frame_mbs_only = br.flag();
    if (!sps.frame_mbs_only) sps.mb_adaptive_frame_field = br.flag();
    sps.direct_8x8_inference = br.flag();
    if (w > 1024 || h > 1024) {
        set_error(error, "picture size too large");
        return;
    }
    sps.mb_width = static_cast<int>(w);
    sps.mb_height = static_cast<int>(h) * (sps.frame_mbs_only ? 1 : 2);
    if (static_cast<long>(sps.mb_width) * sps.mb_height > 139264) {
        set_error(error, "picture size too large");
        return;
    }
    if (br.flag()) {
        std::uint32_t l = br.ue();
        std::uint32_t r = br.ue();
        std::uint32_t t = br.ue();
        std::uint32_t b = br.ue();
        int cux = 1;
        int cuy = sps.frame_mbs_only ? 1 : 2;
        if (sps.chroma_format_idc != 0 && !sps.separate_colour_plane) {
            cux = (sps.chroma_format_idc == 3) ? 1 : 2;
            cuy *= (sps.chroma_format_idc == 1) ? 2 : 1;
        }
        long width = sps.mb_width * 16L;
        long height = sps.mb_height * 16L;
        if (static_cast<long>(l + r) * cux < width && static_cast<long>(t + b) * cuy < height && l < 65536 && r < 65536 && t < 65536 && b < 65536) {
            sps.crop_left = static_cast<int>(l) * cux;
            sps.crop_right = static_cast<int>(r) * cux;
            sps.crop_top = static_cast<int>(t) * cuy;
            sps.crop_bottom = static_cast<int>(b) * cuy;
        }
    }
    if (br.error()) {
        set_error(error, "truncated SPS");
        return;
    }
    sps.vui_present = br.flag();
    if (sps.vui_present) {
        parse_vui(br, sps.vui);
        if (br.error()) sps.vui.bitstream_restriction = false;
    }
    sps.valid = true;
    *ok = true;
}

void parse_pps(BitReader& br, Pps& pps, const Sps* sps_table, std::string* error, bool* ok) {
    *ok = false;
    pps = Pps();
    std::uint32_t id = br.ue();
    std::uint32_t sps_id = br.ue();
    if (id > 255 || sps_id > 31) {
        set_error(error, "PPS id out of range");
        return;
    }
    pps.id = static_cast<int>(id);
    pps.sps_id = static_cast<int>(sps_id);
    pps.cabac = br.flag();
    pps.bottom_field_pic_order_present = br.flag();
    std::uint32_t groups = br.ue() + 1;
    if (groups > 8) {
        set_error(error, "bad num_slice_groups");
        return;
    }
    pps.num_slice_groups = static_cast<int>(groups);
    if (groups > 1) {
        std::uint32_t map_type = br.ue();
        if (map_type == 0) {
            for (std::uint32_t i = 0; i < groups; ++i) br.ue();
        } else if (map_type == 2) {
            for (std::uint32_t i = 0; i + 1 < groups; ++i) {
                br.ue();
                br.ue();
            }
        } else if (map_type >= 3 && map_type <= 5) {
            br.u(1);
            br.ue();
        } else if (map_type == 6) {
            std::uint32_t units = br.ue() + 1;
            int bits = 0;
            while ((1u << bits) < groups) ++bits;
            if (units > 139264) {
                set_error(error, "bad slice group map");
                return;
            }
            br.skip(static_cast<std::size_t>(units) * static_cast<std::size_t>(bits));
        }
    }
    std::uint32_t l0 = br.ue() + 1;
    std::uint32_t l1 = br.ue() + 1;
    if (l0 > 32 || l1 > 32) {
        set_error(error, "bad default reference count");
        return;
    }
    pps.num_ref_idx_default[0] = static_cast<int>(l0);
    pps.num_ref_idx_default[1] = static_cast<int>(l1);
    pps.weighted_pred = br.flag();
    pps.weighted_bipred_idc = static_cast<int>(br.u(2));
    pps.pic_init_qp = 26 + br.se();
    pps.pic_init_qs = 26 + br.se();
    std::int32_t cqo = br.se();
    int qp_floor = -36;
    if (sps_table && sps_table[sps_id].valid) qp_floor = -6 * (sps_table[sps_id].bit_depth_luma - 8);
    if (cqo < -12 || cqo > 12 || pps.pic_init_qp < qp_floor || pps.pic_init_qp > 51 || pps.weighted_bipred_idc > 2) {
        set_error(error, "bad PPS values");
        return;
    }
    pps.chroma_qp_index_offset[0] = cqo;
    pps.chroma_qp_index_offset[1] = cqo;
    pps.deblocking_filter_control_present = br.flag();
    pps.constrained_intra_pred = br.flag();
    pps.redundant_pic_cnt_present = br.flag();
    std::memset(pps.list_state, 0, sizeof(pps.list_state));
    fill_flat(pps.scaling4, pps.scaling8);
    if (br.more_rbsp_data()) {
        pps.transform_8x8_mode = br.flag();
        pps.scaling_matrix_present = br.flag();
        if (pps.scaling_matrix_present) {
            int chroma_format = 1;
            if (sps_table && sps_table[sps_id].valid) chroma_format = sps_table[sps_id].chroma_format_idc;
            int count = 6 + ((chroma_format != 3) ? 2 : 6) * (pps.transform_8x8_mode ? 1 : 0);
            for (int i = 0; i < count; ++i) {
                bool present = br.flag();
                if (!present) continue;
                bool use_default = false;
                bool good = i < 6 ? parse_scaling_list(br, 16, pps.scaling4[i], &use_default)
                                  : parse_scaling_list(br, 64, pps.scaling8[i - 6], &use_default);
                if (!good) {
                    set_error(error, "bad scaling list");
                    return;
                }
                pps.list_state[i] = use_default ? 2 : 1;
            }
        }
        std::int32_t second = br.se();
        if (second < -12 || second > 12) {
            set_error(error, "bad second chroma qp offset");
            return;
        }
        pps.chroma_qp_index_offset[1] = second;
    }
    if (br.error()) {
        set_error(error, "truncated PPS");
        return;
    }
    pps.valid = true;
    *ok = true;
}

void resolve_scaling(const Sps& sps, const Pps& pps, std::uint8_t out4[6][16], std::uint8_t out8[6][64]) {
    if (!pps.scaling_matrix_present) {
        std::memcpy(out4, sps.scaling4, 6 * 16);
        std::memcpy(out8, sps.scaling8, 6 * 64);
        return;
    }
    bool rule_b = sps.scaling_matrix_present;
    for (int i = 0; i < 6; ++i) {
        int state = pps.list_state[i];
        if (state == 1) {
            std::memcpy(out4[i], pps.scaling4[i], 16);
        } else if (state == 2) {
            std::memcpy(out4[i], k_default_scaling4[i / 3], 16);
        } else if (i == 0 || i == 3) {
            std::memcpy(out4[i], rule_b ? sps.scaling4[i] : k_default_scaling4[i / 3], 16);
        } else {
            std::memcpy(out4[i], out4[i - 1], 16);
        }
    }
    for (int k = 0; k < 6; ++k) {
        int state = pps.list_state[6 + k];
        if (state == 1) {
            std::memcpy(out8[k], pps.scaling8[k], 64);
        } else if (state == 2) {
            std::memcpy(out8[k], k_default_scaling8[k & 1], 64);
        } else if (k < 2) {
            std::memcpy(out8[k], rule_b ? sps.scaling8[k] : k_default_scaling8[k], 64);
        } else {
            std::memcpy(out8[k], out8[k - 2], 64);
        }
    }
}

bool parse_slice_header(BitReader& br, int nal_unit_type, int nal_ref_idc, const Sps* sps_table, const Pps* pps_table, SliceHeader& sh, std::string* error) {
    sh.nal_unit_type = nal_unit_type;
    sh.nal_ref_idc = nal_ref_idc;
    sh.idr = nal_unit_type == 5;
    std::uint32_t first_mb = br.ue();
    std::uint32_t type = br.ue();
    std::uint32_t pps_id = br.ue();
    if (type > 9 || pps_id > 255 || first_mb > 139264) {
        set_error(error, "bad slice header");
        return false;
    }
    sh.first_mb = static_cast<int>(first_mb);
    sh.raw_slice_type = static_cast<int>(type);
    sh.slice_type = static_cast<int>(type % 5);
    sh.pps_id = static_cast<int>(pps_id);
    const Pps& pps = pps_table[pps_id];
    if (!pps.valid) {
        set_error(error, "slice references a missing PPS");
        return false;
    }
    const Sps& sps = sps_table[pps.sps_id];
    if (!sps.valid) {
        set_error(error, "slice references a missing SPS");
        return false;
    }
    if (sh.idr && sh.slice_type != k_slice_i && sh.slice_type != k_slice_si) {
        set_error(error, "IDR slice is not intra");
        return false;
    }
    sh.colour_plane_id = 0;
    if (sps.separate_colour_plane) sh.colour_plane_id = static_cast<int>(br.u(2));
    if (sh.colour_plane_id > 2) {
        set_error(error, "bad colour_plane_id");
        return false;
    }
    sh.frame_num = static_cast<int>(br.u(sps.log2_max_frame_num));
    sh.field_pic = false;
    sh.bottom_field = false;
    if (!sps.frame_mbs_only) {
        sh.field_pic = br.flag();
        if (sh.field_pic) sh.bottom_field = br.flag();
    }
    sh.mbaff = sps.mb_adaptive_frame_field && !sh.field_pic;
    if (static_cast<long>(sh.first_mb) * (sh.mbaff ? 2 : 1) >= static_cast<long>(sps.mb_width) * sps.mb_height / (sh.field_pic ? 2 : 1)) {
        set_error(error, "first_mb_in_slice out of range");
        return false;
    }
    if (sh.idr) sh.idr_pic_id = static_cast<int>(br.ue());
    sh.poc_lsb = 0;
    sh.delta_poc_bottom = 0;
    sh.delta_poc[0] = sh.delta_poc[1] = 0;
    if (sps.poc_type == 0) {
        sh.poc_lsb = static_cast<int>(br.u(sps.log2_max_poc_lsb));
        if (pps.bottom_field_pic_order_present && !sh.field_pic) sh.delta_poc_bottom = br.se();
    } else if (sps.poc_type == 1 && !sps.delta_pic_order_always_zero) {
        sh.delta_poc[0] = br.se();
        if (pps.bottom_field_pic_order_present && !sh.field_pic) sh.delta_poc[1] = br.se();
    }
    sh.redundant_pic_cnt = 0;
    if (pps.redundant_pic_cnt_present) sh.redundant_pic_cnt = static_cast<int>(br.ue());
    sh.direct_spatial = false;
    if (sh.slice_type == k_slice_b) sh.direct_spatial = br.flag();
    sh.num_ref_idx_active[0] = 0;
    sh.num_ref_idx_active[1] = 0;
    if (sh.slice_type == k_slice_p || sh.slice_type == k_slice_sp || sh.slice_type == k_slice_b) {
        sh.num_ref_idx_active[0] = pps.num_ref_idx_default[0];
        if (sh.slice_type == k_slice_b) sh.num_ref_idx_active[1] = pps.num_ref_idx_default[1];
        if (br.flag()) {
            std::uint32_t a = br.ue() + 1;
            if (a > 32) {
                set_error(error, "too many active references");
                return false;
            }
            sh.num_ref_idx_active[0] = static_cast<int>(a);
            if (sh.slice_type == k_slice_b) {
                std::uint32_t b = br.ue() + 1;
                if (b > 32) {
                    set_error(error, "too many active references");
                    return false;
                }
                sh.num_ref_idx_active[1] = static_cast<int>(b);
            }
        }
        int limit = sh.field_pic ? 32 : 16;
        if (sh.num_ref_idx_active[0] > limit || sh.num_ref_idx_active[1] > limit) {
            set_error(error, "too many active references");
            return false;
        }
    }
    sh.num_modifications[0] = sh.num_modifications[1] = 0;
    if (sh.slice_type != k_slice_i && sh.slice_type != k_slice_si) {
        int lists = sh.slice_type == k_slice_b ? 2 : 1;
        for (int l = 0; l < lists; ++l) {
            if (!br.flag()) continue;
            int n = 0;
            while (true) {
                std::uint32_t idc = br.ue();
                if (idc == 3) break;
                if (idc > 5 || n >= 33 || br.error()) {
                    set_error(error, "bad reference list modification");
                    return false;
                }
                std::uint32_t value = br.ue();
                sh.modifications[l][n].idc = static_cast<int>(idc);
                sh.modifications[l][n].value = value;
                ++n;
            }
            sh.num_modifications[l] = n;
        }
    }
    sh.has_weights = false;
    if ((pps.weighted_pred && (sh.slice_type == k_slice_p || sh.slice_type == k_slice_sp)) || (pps.weighted_bipred_idc == 1 && sh.slice_type == k_slice_b)) {
        sh.has_weights = true;
        std::uint32_t ld = br.ue();
        std::uint32_t cd = 0;
        bool chroma = sps.chroma_format_idc != 0 && !sps.separate_colour_plane;
        if (chroma) cd = br.ue();
        if (ld > 7 || cd > 7) {
            set_error(error, "bad weight denominator");
            return false;
        }
        sh.luma_log2_denom = static_cast<int>(ld);
        sh.chroma_log2_denom = static_cast<int>(cd);
        int lists = sh.slice_type == k_slice_b ? 2 : 1;
        for (int l = 0; l < lists; ++l) {
            for (int i = 0; i < sh.num_ref_idx_active[l]; ++i) {
                sh.luma_weight[l][i] = static_cast<std::int16_t>(1 << ld);
                sh.luma_offset[l][i] = 0;
                sh.luma_weight_flag[l][i] = br.flag();
                if (sh.luma_weight_flag[l][i]) {
                    std::int32_t w = br.se();
                    std::int32_t o = br.se();
                    if (w < -128 || w > 127 || o < -128 || o > 127) {
                        set_error(error, "bad prediction weight");
                        return false;
                    }
                    sh.luma_weight[l][i] = static_cast<std::int16_t>(w);
                    sh.luma_offset[l][i] = static_cast<std::int16_t>(o);
                }
                sh.chroma_weight_flag[l][i] = false;
                for (int c = 0; c < 2; ++c) {
                    sh.chroma_weight[l][i][c] = static_cast<std::int16_t>(1 << cd);
                    sh.chroma_offset[l][i][c] = 0;
                }
                if (chroma) {
                    sh.chroma_weight_flag[l][i] = br.flag();
                    if (sh.chroma_weight_flag[l][i]) {
                        for (int c = 0; c < 2; ++c) {
                            std::int32_t w = br.se();
                            std::int32_t o = br.se();
                            if (w < -128 || w > 127 || o < -128 || o > 127) {
                                set_error(error, "bad prediction weight");
                                return false;
                            }
                            sh.chroma_weight[l][i][c] = static_cast<std::int16_t>(w);
                            sh.chroma_offset[l][i][c] = static_cast<std::int16_t>(o);
                        }
                    }
                }
            }
        }
    }
    sh.no_output_of_prior_pics = false;
    sh.long_term_reference_flag = false;
    sh.adaptive_marking = false;
    sh.num_mmco = 0;
    sh.has_mmco5 = false;
    if (nal_ref_idc != 0) {
        if (sh.idr) {
            sh.no_output_of_prior_pics = br.flag();
            sh.long_term_reference_flag = br.flag();
        } else {
            sh.adaptive_marking = br.flag();
            if (sh.adaptive_marking) {
                while (true) {
                    std::uint32_t op = br.ue();
                    if (op == 0) break;
                    if (op > 6 || sh.num_mmco >= 66 || br.error()) {
                        set_error(error, "bad memory management operation");
                        return false;
                    }
                    Mmco& m = sh.mmco[sh.num_mmco++];
                    m.op = static_cast<int>(op);
                    m.difference_of_pic_nums_minus1 = 0;
                    m.long_term_pic_num = 0;
                    m.long_term_frame_idx = 0;
                    m.max_long_term_frame_idx_plus1 = 0;
                    if (op == 1 || op == 3) m.difference_of_pic_nums_minus1 = br.ue();
                    if (op == 2) m.long_term_pic_num = br.ue();
                    if (op == 3 || op == 6) m.long_term_frame_idx = br.ue();
                    if (op == 4) m.max_long_term_frame_idx_plus1 = br.ue();
                    if (op == 5) sh.has_mmco5 = true;
                }
            }
        }
    }
    sh.cabac_init_idc = 0;
    if (pps.cabac && sh.slice_type != k_slice_i && sh.slice_type != k_slice_si) {
        std::uint32_t idc = br.ue();
        if (idc > 2) {
            set_error(error, "bad cabac_init_idc");
            return false;
        }
        sh.cabac_init_idc = static_cast<int>(idc);
    }
    int qp = pps.pic_init_qp + br.se();
    int qp_min = -6 * (sps.bit_depth_luma - 8);
    if (qp < qp_min || qp > 51) {
        set_error(error, "bad slice QP");
        return false;
    }
    sh.slice_qp = qp;
    if (sh.slice_type == k_slice_sp || sh.slice_type == k_slice_si) {
        if (sh.slice_type == k_slice_sp) br.u(1);
        br.se();
    }
    sh.disable_deblocking = 0;
    sh.alpha_offset = 0;
    sh.beta_offset = 0;
    if (pps.deblocking_filter_control_present) {
        std::uint32_t d = br.ue();
        if (d > 2) {
            set_error(error, "bad disable_deblocking_filter_idc");
            return false;
        }
        sh.disable_deblocking = static_cast<int>(d);
        if (d != 1) {
            std::int32_t a = br.se();
            std::int32_t b = br.se();
            if (a < -6 || a > 6 || b < -6 || b > 6) {
                set_error(error, "bad deblocking offsets");
                return false;
            }
            sh.alpha_offset = a * 2;
            sh.beta_offset = b * 2;
        }
    }
    if (pps.num_slice_groups > 1) {
        set_error(error, "slice groups (FMO) are not supported");
        return false;
    }
    if (br.error()) {
        set_error(error, "truncated slice header");
        return false;
    }
    sh.data_bit_offset = br.position();
    return true;
}

bool parse_sei_pic_struct(BitReader& br, const Sps* sps, int* pic_struct) {
    bool found = false;
    while (br.more_rbsp_data() && !br.error()) {
        int type = 0;
        int size = 0;
        while (true) {
            int b = static_cast<int>(br.u(8));
            type += b;
            if (b != 255 || br.error()) break;
        }
        while (true) {
            int b = static_cast<int>(br.u(8));
            size += b;
            if (b != 255 || br.error()) break;
        }
        if (br.error()) break;
        std::size_t start = br.position();
        if (type == 1 && sps && sps->vui_present) {
            const Vui& v = sps->vui;
            if (v.nal_hrd || v.vcl_hrd) {
                br.u(v.cpb_removal_delay_length);
                br.u(v.dpb_output_delay_length);
            }
            if (v.pic_struct_present) {
                int ps = static_cast<int>(br.u(4));
                if (!br.error() && ps <= 8) {
                    *pic_struct = ps;
                    found = true;
                }
            }
        }
        br.set_position(start + static_cast<std::size_t>(size) * 8);
        if (br.position() > br.size_bits()) break;
    }
    return found;
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
