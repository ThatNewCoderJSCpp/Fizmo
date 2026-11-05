#ifndef FIZMO_VIDEO_H264_INTERNAL_HPP
#define FIZMO_VIDEO_H264_INTERNAL_HPP

#include "h264.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif // _MSC_VER

namespace fizmo {
namespace video {
namespace h264_internal {

extern const std::uint8_t k_cabac_range_lps[64][4];
extern const std::uint8_t k_cabac_trans_lps[64];
extern const std::int8_t k_cabac_init_i[1024][2];
extern const std::int8_t k_cabac_init_pb[3][1024][2];
extern const std::uint8_t k_coeff_token_len[4][68];
extern const std::uint8_t k_coeff_token_bits[4][68];
extern const std::uint8_t k_chroma_dc_coeff_token_len[20];
extern const std::uint8_t k_chroma_dc_coeff_token_bits[20];
extern const std::uint8_t k_chroma422_dc_coeff_token_len[36];
extern const std::uint8_t k_chroma422_dc_coeff_token_bits[36];
extern const std::uint8_t k_total_zeros_len[16][16];
extern const std::uint8_t k_total_zeros_bits[16][16];
extern const std::uint8_t k_chroma_dc_total_zeros_len[3][4];
extern const std::uint8_t k_chroma_dc_total_zeros_bits[3][4];
extern const std::uint8_t k_chroma422_dc_total_zeros_len[7][8];
extern const std::uint8_t k_chroma422_dc_total_zeros_bits[7][8];
extern const std::uint8_t k_run_len[7][16];
extern const std::uint8_t k_run_bits[7][16];
extern const std::uint8_t k_golomb_to_intra4x4_cbp[48];
extern const std::uint8_t k_golomb_to_inter_cbp[48];
extern const std::uint8_t k_golomb_to_intra4x4_cbp_gray[16];
extern const std::uint8_t k_golomb_to_inter_cbp_gray[16];
extern const std::uint8_t k_sig_8x8_offset[2][63];
extern const std::uint8_t k_last_8x8_offset[63];
extern const std::uint8_t k_default_scaling4[2][16];
extern const std::uint8_t k_default_scaling8[2][64];
extern const std::uint8_t k_zigzag4x4[16];
extern const std::uint8_t k_zigzag8x8[64];
extern const std::uint8_t k_field_scan4x4[16];
extern const std::uint8_t k_field_scan8x8[64];
extern const std::uint8_t k_chroma422_dc_scan[8];
extern const std::uint8_t k_blk_x[16];
extern const std::uint8_t k_blk_y[16];
extern const std::uint8_t k_blk_index[4][4];
extern const std::uint8_t k_alpha_table[52];
extern const std::uint8_t k_beta_table[52];
extern const std::uint8_t k_tc0_table[52][3];
extern const std::uint8_t k_chroma_qp_table[52];
extern const std::uint8_t k_norm_adjust4[6][3];
extern const std::uint8_t k_norm_adjust8[6][6];

inline int count_leading_zeros32(std::uint32_t v) noexcept {
    if (v == 0) return 32;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clz(v);
#elif defined(_MSC_VER)
    unsigned long index = 0;
    _BitScanReverse(&index, v);
    return 31 - static_cast<int>(index);
#else
    int n = 0;
    while (!(v & 0x80000000u)) {
        v <<= 1;
        ++n;
    }
    return n;
#endif // __GNUC__
}

inline int clip3(int lo, int hi, int v) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline std::uint8_t clip_pixel(int v) noexcept {
    return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

class BitReader {
public:
    void init(const std::uint8_t* data, std::size_t size) noexcept;
    std::uint32_t peek32() const noexcept;
    std::uint32_t u(int n) noexcept;
    bool flag() noexcept { return u(1) != 0; }
    std::uint32_t ue() noexcept;
    std::int32_t se() noexcept;
    void skip(std::size_t n) noexcept { m_pos += n; }
    void align() noexcept { m_pos = (m_pos + 7) & ~static_cast<std::size_t>(7); }
    bool byte_aligned() const noexcept { return (m_pos & 7) == 0; }
    bool more_rbsp_data() const noexcept { return m_pos < m_trailing; }
    std::size_t position() const noexcept { return m_pos; }
    void set_position(std::size_t pos) noexcept { m_pos = pos; }
    std::size_t size_bits() const noexcept { return m_size_bits; }
    bool overrun() const noexcept { return m_pos > m_size_bits || m_error; }
    bool error() const noexcept { return m_error || m_pos > m_size_bits; }
    void set_error() noexcept { m_error = true; }
    const std::uint8_t* data() const noexcept { return m_data; }
    std::size_t size_bytes() const noexcept { return m_size_bits >> 3; }

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_size_bits = 0;
    std::size_t m_trailing = 0;
    std::size_t m_pos = 0;
    bool m_error = false;
};

struct VlcEntry {
    std::int16_t symbol;
    std::int8_t length;
};

class Vlc {
public:
    void build(const std::uint8_t* lengths, const std::uint8_t* codes, const std::int16_t* symbols, int count);
    int read(BitReader& br) const noexcept;

private:
    static const int k_bits = 8;
    std::vector<VlcEntry> m_table;
};

class Cabac {
public:
    void init(const std::uint8_t* data, std::size_t size) noexcept;
    inline int decision(std::uint8_t& state) noexcept;
    inline int bypass() noexcept;
    int terminate() noexcept;
    std::size_t pcm_start() const noexcept;
    bool overrun() const noexcept { return m_over > 2; }

private:
    void refill() noexcept;

    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_pos = 0;
    std::uint64_t m_value = 0;
    int m_bits = 0;
    std::uint32_t m_range = 0;
    int m_over = 0;
};

inline int Cabac::decision(std::uint8_t& state) noexcept {
    unsigned s = state >> 1;
    unsigned mps = state & 1u;
    std::uint32_t lps = k_cabac_range_lps[s][(m_range >> 6) & 3];
    m_range -= lps;
    std::uint64_t scaled = static_cast<std::uint64_t>(m_range) << m_bits;
    int bin;
    if (m_value < scaled) {
        bin = static_cast<int>(mps);
        if (s < 62) state = static_cast<std::uint8_t>(((s + 1) << 1) | mps);
        if (m_range < 256) {
            m_range <<= 1;
            --m_bits;
        }
    } else {
        m_value -= scaled;
        bin = static_cast<int>(mps ^ 1u);
        if (s == 0) mps ^= 1u;
        state = static_cast<std::uint8_t>((k_cabac_trans_lps[s] << 1) | mps);
        int shift = count_leading_zeros32(lps) - 23;
        m_range = lps << shift;
        m_bits -= shift;
    }
    if (m_bits < 16) refill();
    return bin;
}

inline int Cabac::bypass() noexcept {
    --m_bits;
    std::uint64_t scaled = static_cast<std::uint64_t>(m_range) << m_bits;
    int bin = 0;
    if (m_value >= scaled) {
        m_value -= scaled;
        bin = 1;
    }
    if (m_bits < 16) refill();
    return bin;
}

struct Vui {
    bool aspect_ratio_present = false;
    std::uint32_t sar_width = 0;
    std::uint32_t sar_height = 0;
    bool video_signal_present = false;
    int video_format = 5;
    bool full_range = false;
    bool colour_description_present = false;
    int colour_primaries = 2;
    int transfer_characteristics = 2;
    int matrix_coefficients = 2;
    bool chroma_loc_present = false;
    int chroma_loc_top = 0;
    int chroma_loc_bottom = 0;
    bool timing_present = false;
    std::uint32_t num_units_in_tick = 0;
    std::uint32_t time_scale = 0;
    bool fixed_frame_rate = false;
    bool pic_struct_present = false;
    bool bitstream_restriction = false;
    int max_num_reorder_frames = 0;
    int max_dec_frame_buffering = 0;
    bool nal_hrd = false;
    bool vcl_hrd = false;
    int cpb_removal_delay_length = 24;
    int dpb_output_delay_length = 24;
    int time_offset_length = 24;
};

struct Sps {
    bool valid = false;
    int id = 0;
    int profile_idc = 0;
    int constraint_flags = 0;
    int level_idc = 0;
    int chroma_format_idc = 1;
    bool separate_colour_plane = false;
    int bit_depth_luma = 8;
    int bit_depth_chroma = 8;
    bool transform_bypass = false;
    bool scaling_matrix_present = false;
    std::uint8_t scaling4[6][16];
    std::uint8_t scaling8[6][64];
    int log2_max_frame_num = 4;
    int poc_type = 0;
    int log2_max_poc_lsb = 4;
    bool delta_pic_order_always_zero = false;
    int offset_for_non_ref_pic = 0;
    int offset_for_top_to_bottom_field = 0;
    int num_ref_frames_in_poc_cycle = 0;
    std::int32_t offset_for_ref_frame[256];
    int max_num_ref_frames = 0;
    bool gaps_allowed = false;
    int mb_width = 0;
    int mb_height = 0;
    bool frame_mbs_only = true;
    bool mb_adaptive_frame_field = false;
    bool direct_8x8_inference = false;
    int crop_left = 0;
    int crop_right = 0;
    int crop_top = 0;
    int crop_bottom = 0;
    bool vui_present = false;
    Vui vui;
};

struct Pps {
    bool valid = false;
    int id = 0;
    int sps_id = 0;
    bool cabac = false;
    bool bottom_field_pic_order_present = false;
    int num_slice_groups = 1;
    int num_ref_idx_default[2] = {1, 1};
    bool weighted_pred = false;
    int weighted_bipred_idc = 0;
    int pic_init_qp = 26;
    int pic_init_qs = 26;
    int chroma_qp_index_offset[2] = {0, 0};
    bool deblocking_filter_control_present = false;
    bool constrained_intra_pred = false;
    bool redundant_pic_cnt_present = false;
    bool transform_8x8_mode = false;
    bool scaling_matrix_present = false;
    std::uint8_t list_state[12];
    std::uint8_t scaling4[6][16];
    std::uint8_t scaling8[6][64];
};

struct RefModification {
    int idc;
    std::uint32_t value;
};

struct Mmco {
    int op;
    std::uint32_t difference_of_pic_nums_minus1;
    std::uint32_t long_term_pic_num;
    std::uint32_t long_term_frame_idx;
    std::uint32_t max_long_term_frame_idx_plus1;
};

enum SliceType {
    k_slice_p = 0,
    k_slice_b = 1,
    k_slice_i = 2,
    k_slice_sp = 3,
    k_slice_si = 4
};

struct SliceHeader {
    int nal_unit_type = 0;
    int nal_ref_idc = 0;
    bool idr = false;
    int first_mb = 0;
    int slice_type = 0;
    int raw_slice_type = 0;
    int pps_id = 0;
    int frame_num = 0;
    bool field_pic = false;
    bool bottom_field = false;
    int idr_pic_id = 0;
    int poc_lsb = 0;
    int delta_poc_bottom = 0;
    int delta_poc[2] = {0, 0};
    int redundant_pic_cnt = 0;
    bool direct_spatial = false;
    int num_ref_idx_active[2] = {0, 0};
    int num_modifications[2] = {0, 0};
    RefModification modifications[2][34];
    int luma_log2_denom = 0;
    int chroma_log2_denom = 0;
    bool has_weights = false;
    std::int16_t luma_weight[2][32];
    std::int16_t luma_offset[2][32];
    std::int16_t chroma_weight[2][32][2];
    std::int16_t chroma_offset[2][32][2];
    bool luma_weight_flag[2][32];
    bool chroma_weight_flag[2][32];
    bool no_output_of_prior_pics = false;
    bool long_term_reference_flag = false;
    bool adaptive_marking = false;
    int num_mmco = 0;
    Mmco mmco[72];
    bool has_mmco5 = false;
    int cabac_init_idc = 0;
    int slice_qp = 26;
    int disable_deblocking = 0;
    int alpha_offset = 0;
    int beta_offset = 0;
    std::size_t data_bit_offset = 0;
    int colour_plane_id = 0;
    bool mbaff = false;
};

struct Mv {
    std::int16_t x;
    std::int16_t y;
};

struct PicView {
    std::uint8_t* plane[3] = {nullptr, nullptr, nullptr};
    int stride[3] = {0, 0, 0};
    int width = 0;
    int height = 0;
    int chroma_width = 0;
    int chroma_height = 0;
    int pad[3] = {0, 0, 0};
    bool vpad = false;
};

enum PicStructure {
    k_struct_top = 1,
    k_struct_bottom = 2,
    k_struct_frame = 3
};

enum PicCoding {
    k_coding_frame = 0,
    k_coding_mbaff = 1,
    k_coding_fields = 2
};

struct Picture {
    std::vector<std::uint8_t> buffer;
    std::uint8_t* plane[3] = {nullptr, nullptr, nullptr};
    int stride[3] = {0, 0, 0};
    int width = 0;
    int height = 0;
    int chroma_width = 0;
    int chroma_height = 0;
    int mb_width = 0;
    int mb_height = 0;
    int chroma_format = 1;
    std::vector<Mv> mv[2];
    std::vector<std::int8_t> ref_idx[2];
    std::vector<std::int32_t> ref_uid[2];
    std::vector<std::uint8_t> mb_intra;
    std::int32_t poc = 0;
    std::int32_t top_poc = 0;
    std::int32_t bottom_poc = 0;
    int frame_num = 0;
    int frame_num_wrap = 0;
    int long_term_frame_idx = 0;
    bool short_ref = false;
    bool long_ref = false;
    bool needs_output = false;
    bool non_existing = false;
    bool idr = false;
    bool mmco5 = false;
    bool corrupt = false;
    std::int32_t uid = 0;
    double pts = 0.0;
    bool keyframe = false;
    int crop_left = 0;
    int crop_right = 0;
    int crop_top = 0;
    int crop_bottom = 0;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
    ColorInfo color;
    std::uint64_t decode_order = 0;

    int pixel_bytes = 1;
    int bit_depth_luma = 8;
    int bit_depth_chroma = 8;
    int pad[3] = {0, 0, 0};
    int planes = 1;
    int coding = k_coding_frame;
    std::vector<std::uint8_t> mb_field;
    std::uint8_t field_ref[2] = {0, 0};
    bool field_done[2] = {false, false};
    bool field_pic_coded = false;
    bool awaiting_field = false;
    int first_field_parity = 0;
    bool interlaced_hint = false;
    int pic_struct = -1;
    double field_pts[2] = {0.0, 0.0};

    bool is_ref() const noexcept { return short_ref || long_ref; }
    bool allocate(int mbw, int mbh, int chroma_format_idc);
    bool allocate(int mbw, int mbh, int chroma_format_idc, int bytes, int motion_planes);
    void extend_borders();
    void sync_ref() noexcept;
    bool field_is_short(int parity) const noexcept { return field_ref[parity] == 1; }
    bool field_is_long(int parity) const noexcept { return field_ref[parity] == 2; }
    bool frame_is_short() const noexcept { return field_ref[0] == 1 && field_ref[1] == 1; }
    bool frame_is_long() const noexcept { return field_ref[0] == 2 && field_ref[1] == 2; }
    std::int32_t structure_poc(int structure) const noexcept;
    PicView view(int structure) const noexcept;
    std::size_t motion_base(int plane, int structure) const noexcept;
};

const int k_pad_luma = 48;
const int k_pad_chroma = 24;

enum MbFlags {
    k_mb_intra = 1,
    k_mb_i4x4 = 2,
    k_mb_i8x8 = 4,
    k_mb_i16x16 = 8,
    k_mb_pcm = 16,
    k_mb_skip = 32,
    k_mb_direct = 64,
    k_mb_inter = 128,
    k_mb_field = 256,
    k_mb_bypass = 512
};

struct MbInfo {
    std::uint16_t flags = 0;
    std::uint8_t cbp = 0;
    std::int8_t qp = 0;
    std::uint8_t transform8x8 = 0;
    std::uint8_t chroma_pred_mode = 0;
    std::uint8_t direct_mask = 0;
    std::uint8_t cbf_dc = 0;
    std::uint8_t shape = 3;
    std::uint16_t nz = 0;
    std::int32_t slice = -1;
    std::uint16_t nz_c[2] = {0, 0};
    std::uint8_t field = 0;
    std::uint8_t i16_mode = 0;
};

struct SliceParams {
    int disable_deblocking = 0;
    int alpha_offset = 0;
    int beta_offset = 0;
    int chroma_qp_offset[2] = {0, 0};
    int slice_type = 0;
};

enum MbKind {
    k_kind_i4x4,
    k_kind_i8x8,
    k_kind_i16x16,
    k_kind_pcm,
    k_kind_p_skip,
    k_kind_b_skip,
    k_kind_b_direct,
    k_kind_16x16,
    k_kind_16x8,
    k_kind_8x16,
    k_kind_8x8
};

struct MbParse {
    int kind = 0;
    int i16_mode = 0;
    int i16_cbp_chroma = 0;
    int i16_cbp_luma = 0;
    bool ref0 = false;
    int part_pred[2] = {0, 0};
    int sub_type[4] = {0, 0, 0, 0};
    int sub_pred[4] = {0, 0, 0, 0};
    int sub_shape[4] = {0, 0, 0, 0};
    int ref_idx[2][4];
    Mv mvd[2][16];
    std::int8_t prev_flag[16];
    std::int8_t rem_mode[16];
    int chroma_mode = 0;
    int cbp = 0;
    bool transform8x8 = false;
    int qp_delta = 0;
    std::uint8_t pcm[384];
    std::uint16_t pcm16[768];
};

struct Residual {
    std::int32_t luma[16][16];
    std::int32_t luma8[4][64];
    std::int32_t chroma[2][4][16];
    std::uint32_t luma_mask;
    std::uint32_t luma8_mask;
    std::uint32_t chroma_mask[2];
    std::uint32_t chroma_dc_only[2];
    std::uint32_t luma_dc_only;
    std::int32_t c4[3][16][16];
    std::int32_t c8[3][4][64];
    std::uint32_t mask4[3];
    std::uint32_t mask8[3];
    std::uint32_t dc_only4[3];
};

struct Neighbor {
    bool available;
    int ref;
    Mv mv;
};

struct NbMotion {
    bool available;
    int ref;
    Mv mv;
    int mb;
    int blk;
};

struct PlaneStore {
    std::vector<MbInfo> mbs;
    std::vector<std::uint8_t> nnz;
    std::vector<std::int8_t> intra_modes;
    std::vector<std::uint8_t> mvd[2];
    std::vector<SliceParams> slices;
    std::vector<std::uint8_t> mb_done;
};

struct RefEntry {
    Picture* pic = nullptr;
    int structure = k_struct_frame;
};

void parse_sps(BitReader& br, Sps& sps, std::string* error, bool* ok);
void parse_pps(BitReader& br, Pps& pps, const Sps* sps_table, std::string* error, bool* ok);
void resolve_scaling(const Sps& sps, const Pps& pps, std::uint8_t out4[6][16], std::uint8_t out8[6][64]);
bool parse_slice_header(BitReader& br, int nal_unit_type, int nal_ref_idc, const Sps* sps_table, const Pps* pps_table, SliceHeader& sh, std::string* error);
std::size_t unescape_nal(const std::uint8_t* src, std::size_t size, std::vector<std::uint8_t>& out);
bool parse_sei_pic_struct(BitReader& br, const Sps* sps, int* pic_struct);

void idct4x4_add(std::uint8_t* dst, int stride, std::int32_t* block) noexcept;
void idct4x4_dc_add(std::uint8_t* dst, int stride, std::int32_t* block) noexcept;
void idct8x8_add(std::uint8_t* dst, int stride, std::int32_t* block) noexcept;
void idct8x8_dc_add(std::uint8_t* dst, int stride, std::int32_t* block) noexcept;

void pred_intra4x4(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left, bool have_top_right);
void pred_intra8x8(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left, bool have_top_right);
void pred_intra16x16(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left);
void pred_chroma(std::uint8_t* dst, int stride, int mode, bool have_top, bool have_left, bool have_top_left);

void mc_luma(std::uint8_t* dst, int dst_stride, const Picture& ref, int x, int y, int mvx, int mvy, int w, int h);
void mc_chroma(std::uint8_t* dst, int dst_stride, const Picture& ref, int plane, int x, int y, int mvx, int mvy, int w, int h);
void mc_average(std::uint8_t* dst, int dst_stride, const std::uint8_t* a, const std::uint8_t* b, int src_stride, int w, int h);
void mc_luma_block8(std::uint8_t* dst, int dst_stride, const std::uint8_t* src, int src_stride, int fx, int fy, int w, int h);
void mc_chroma_block8(std::uint8_t* dst, int dst_stride, const std::uint8_t* src, int src_stride, int fx, int fy, int w, int h);
void mc_weight_uni(std::uint8_t* dst, int dst_stride, const std::uint8_t* src, int src_stride, int w, int h, int log_wd, int weight, int offset);
void mc_weight_bi(std::uint8_t* dst, int dst_stride, const std::uint8_t* a, const std::uint8_t* b, int src_stride, int w, int h, int log_wd, int w0, int w1, int offset);

void filter_luma_edge(std::uint8_t* p, int xstep, int ystep, int alpha, int beta, const int* bs, const int* tc0);
void filter_chroma_edge(std::uint8_t* p, int xstep, int ystep, int alpha, int beta, const int* bs, const int* tc0);

struct IntraEdge {
    int top[33];
    int left[17];
    int top_left;
    bool have_top;
    bool have_left;
    bool have_top_left;
    bool have_top_right;
};

template <typename P>
void intra_pred_nxn(P* dst, int stride, int n, int mode, const IntraEdge& e, int bit_depth);
template <typename P>
void intra_pred_16x16(P* dst, int stride, int mode, const IntraEdge& e, int bit_depth);
template <typename P>
void intra_pred_chroma_block(P* dst, int stride, int w, int h, int mode, const IntraEdge& e, int bit_depth);
template <typename P>
void idct4x4_add_t(P* dst, int stride, std::int32_t* block, int bit_depth) noexcept;
template <typename P>
void idct8x8_add_t(P* dst, int stride, std::int32_t* block, int bit_depth) noexcept;
template <typename P>
void residual_add_t(P* dst, int stride, const std::int32_t* res, int res_stride, int w, int h, int bit_depth) noexcept;
template <typename P>
void mc_luma_t(P* dst, int dst_stride, const PicView& ref, int plane, int x, int y, int mvx, int mvy, int w, int h, int bit_depth);
template <typename P>
void mc_chroma_t(P* dst, int dst_stride, const PicView& ref, int plane, int x, int y, int mvx8, int mvy8, int w, int h);
template <typename P>
void mc_avg_t(P* dst, int dst_stride, const P* a, const P* b, int src_stride, int w, int h);
template <typename P>
void mc_weight_uni_t(P* dst, int dst_stride, const P* src, int src_stride, int w, int h, int log_wd, int weight, int offset, int bit_depth);
template <typename P>
void mc_weight_bi_t(P* dst, int dst_stride, const P* a, const P* b, int src_stride, int w, int h, int log_wd, int w0, int w1, int offset, int bit_depth);
template <typename P>
void deblock_line_luma(P* pix, int xstep, int bs, int alpha, int beta, int tc0, bool chroma_style, int bit_depth) noexcept;

struct DecoderState {
    DecoderState();

    bool opened = false;
    bool avcc = false;
    int nal_length_size = 4;
    H264Info info;
    std::uint64_t frames_decoded = 0;
    std::uint64_t errors = 0;
    std::string last_error;

    Sps sps_table[32];
    Pps pps_table[256];
    const Sps* sps = nullptr;
    const Pps* pps = nullptr;
    int active_sps_id = -1;

    std::vector<std::unique_ptr<Picture>> pool;
    std::vector<Picture*> dpb;
    std::deque<VideoFrame> output;
    Picture* cur = nullptr;
    bool cur_has_slices = false;
    int cur_mbs_decoded = 0;
    SliceHeader cur_first_header;
    std::int32_t next_uid = 1;
    std::uint64_t decode_counter = 0;
    int dpb_size = 16;
    int reorder_frames = 16;
    int max_long_term_frame_idx = -1;
    int prev_ref_frame_num = 0;
    int prev_frame_num = 0;
    int prev_frame_num_offset = 0;
    int prev_poc_msb = 0;
    int prev_poc_lsb = 0;
    bool prev_has_mmco5 = false;
    bool prev_ref_has_mmco5 = false;
    int prev_ref_top_poc = 0;
    int frame_num_offset = 0;

    std::vector<std::uint8_t> rbsp;

    int mb_width = 0;
    int mb_height = 0;
    int mb_count = 0;
    int b4_stride = 0;
    std::vector<MbInfo> mbs;
    std::vector<std::uint8_t> nnz;
    std::vector<std::uint8_t> nnz_c[2];
    std::vector<std::int8_t> intra_modes;
    std::vector<std::uint8_t> mvd[2];
    std::vector<SliceParams> slices;
    std::vector<std::uint8_t> mb_done;

    SliceHeader sh;
    int slice_num = 0;
    Picture* ref_list[2][32];
    int ref_count[2] = {0, 0};
    int implicit_w[32][32];
    std::int32_t dq4[6][92][16];
    std::int32_t dq8[6][92][64];
    bool dq_valid = false;
    std::uint8_t dq_lists4[6][16];
    std::uint8_t dq_lists8[6][64];
    std::unique_ptr<Picture> gray_pic;
    bool cur_geometry_valid = false;
    int cur_chroma_format = -1;
    int cur_poc_msb = 0;
    bool seen_reference = false;
    std::size_t cabac_data_offset = 0;
    std::uint8_t cabac_state[1024];
    Cabac cabac;
    BitReader br;
    int qp = 0;
    int last_qp_delta = 0;
    int chroma_qp[2] = {0, 0};
    int mb_x = 0;
    int mb_y = 0;
    int mb_addr = 0;
    bool mb_error = false;
    MbParse mp;
    Residual res;
    std::uint8_t mc_buf[4][16 * 16];
    bool col_short = true;

    int ref_struct[2][32];
    int implicit_wf[2][64][64];
    int chroma_array_type = 1;
    int sub_w = 2;
    int sub_h = 2;
    int mb_wc = 8;
    int mb_hc = 8;
    int bit_depth_y = 8;
    int bit_depth_c = 8;
    int qp_bd_y = 0;
    int qp_bd_c = 0;
    int pixel_bytes = 1;
    int cur_pixel_bytes = 1;
    int cur_bit_depth[2] = {8, 8};
    bool field_pic = false;
    bool bottom_field = false;
    bool mbaff = false;
    int structure = k_struct_frame;
    int pic_mb_height = 0;
    int pic_mb_count = 0;
    PicView cv;
    bool mb_field = false;
    bool mb_bottom = false;
    bool mb_bypass = false;
    int pair_x = 0;
    int pair_y = 0;
    int pair_nb[4] = {-1, -1, -1, -1};
    int simple_nb[4] = {-1, -1, -1, -1};
    std::uint8_t* mb_pix[3] = {nullptr, nullptr, nullptr};
    int mb_pstride[3] = {0, 0, 0};
    std::size_t motion_offset = 0;
    int colour_plane = 0;
    int num_planes = 1;
    PlaneStore plane_store[3];
    int active_plane = 0;
    int mbs_per_plane_done[3] = {0, 0, 0};
    Picture* pending_field = nullptr;
    bool second_field = false;
    int cur_pic_struct = -1;
    bool prev_ref_bottom_field = false;
    std::int32_t res_mb[3][256];
    bool res_mb_used[3] = {false, false, false};
    alignas(16) std::uint8_t mc_tmp[4][16 * 16 * 2];
    int prev_top_skip = 0;
    bool pair_flag_known = false;
    bool pair_field_flag = false;
    bool pending_skip_bottom = false;
    int pending_skip_value = 0;
    int max_long_term_idx_frame = -1;

    bool open_avcc(const std::uint8_t* data, std::size_t size, std::string* error);
    bool decode_packet(const std::uint8_t* data, std::size_t size, double pts, std::string* error);
    bool handle_nal(const std::uint8_t* data, std::size_t size, double pts, std::string* error);
    bool handle_slice(int nal_type, int nal_ref_idc, double pts, std::string* error);
    bool start_picture(const SliceHeader& h, double pts, std::string* error);
    void finish_picture();
    void conceal_picture();
    bool setup_geometry(const Sps& s);
    Picture* alloc_picture();
    void release_unused();
    void compute_poc(const SliceHeader& h, Picture* pic);
    void handle_frame_num_gap(const SliceHeader& h);
    void mark_references(Picture* pic, const SliceHeader& h);
    void sliding_window();
    void bump_one();
    void bump_all();
    void output_picture(Picture* pic);
    void store_picture(Picture* pic);
    bool build_ref_lists(std::string* error);
    void compute_implicit_weights();
    void update_dequant();
    void init_cabac_contexts();
    void flush_all();
    void reset_state(bool keep_params);

    bool decode_slice_data(std::string* error);
    void decode_skip_mb(bool begin);
    bool setup_mb_type(int raw);
    bool parse_mb(bool skip_known_false);
    bool parse_residual();
    bool read_block(int cat, int comp, int bx, int by, int max_coeff, int* pos, std::int32_t* lev, int* count);
    bool read_mvd(int list, int x4, int y4, int w4, int h4);
    int read_cavlc_coeffs(int nc, int max_coeff, int* pos, std::int32_t* lev);
    int read_cabac_coeffs(int cat, int max_coeff, int* pos, std::int32_t* lev);
    int cavlc_nc_luma(int bx, int by) const;
    int cavlc_nc_chroma(int comp, int cx, int cy) const;
    int cabac_mb_skip();
    int cabac_mb_type_i(int ctx_base, bool in_i_slice);
    int cabac_ref_idx(int list, int x4, int y4);
    int cabac_mvd(int list, int comp, int bx, int by);
    int cabac_cbf_ctx(int cat, int comp, int bx, int by);

    bool mb_available(int mbx, int mby) const noexcept {
        if (mbx < 0 || mby < 0 || mbx >= mb_width || mby >= mb_height) return false;
        return mbs[static_cast<std::size_t>(mby * mb_width + mbx)].slice == slice_num;
    }
    bool mb_available_intra(int mbx, int mby) const noexcept;
    Neighbor neighbor(int list, int x4, int y4, int cur_blk) const noexcept;
    void mv_pred(int list, int ref, int x4, int y4, int w4, int h4, int shape, Mv& out) const noexcept;
    void set_motion(int x4, int y4, int w4, int h4, int list, int ref, Mv mv);
    void pskip_motion();
    void direct_motion(int sub_mask);
    void direct_spatial(int sub_mask);
    void direct_temporal(int sub_mask);
    void compute_motion();
    void resolve_intra_modes();
    void begin_mb_info();
    void store_mb_info(int flags);
    void reconstruct_mb();
    void reconstruct_intra_luma();
    void reconstruct_intra_chroma();
    void reconstruct_inter();
    void mc_partition(int x4, int y4, int w4, int h4);
    void add_luma_residual();
    void add_chroma_residual();
    void write_pcm();
    void deblock_picture();
    void deblock_mb(int mbx, int mby);
    int chroma_qp_for(int qp, int comp, int offset) const noexcept;

    bool mb_is_available(int addr) const noexcept {
        return addr >= 0 && addr < pic_mb_count && mbs[static_cast<std::size_t>(addr)].slice == slice_num;
    }
    int nb_loc(int xn, int yn, int maxw, int maxh, int* xw, int* yw) const noexcept;
    int nb_mb_a() const noexcept;
    int nb_mb_b() const noexcept;
    void setup_mb_position();
    void setup_mb_neighbors();
    void infer_pair_field();
    int cabac_field_flag();
    int cabac_skip_ctx_for(int addr_a, int addr_b);
    NbMotion nb_motion(int list, int xn, int yn, int cur_order) const noexcept;
    void mv_pred_part(int list, int ref, int x, int y, int w, int h, int shape, int order, Mv& out) const noexcept;
    RefEntry ref_entry(int list, int idx) const noexcept;
    int ref_count_mb(int list) const noexcept;
    std::int32_t ref_entry_id(const RefEntry& e) const noexcept;
    std::int32_t ref_entry_poc(const RefEntry& e) const noexcept;
    bool ref_entry_long(const RefEntry& e) const noexcept;
    std::int32_t cur_poc_for_mb() const noexcept;
    int nnz_at(int addr, int plane, int blk) const noexcept {
        return nnz[static_cast<std::size_t>(addr) * 48 + static_cast<std::size_t>(plane * 16 + blk)];
    }
    void set_nnz(int plane, int blk, int v) noexcept {
        nnz[static_cast<std::size_t>(mb_addr) * 48 + static_cast<std::size_t>(plane * 16 + blk)] = static_cast<std::uint8_t>(v);
    }
    int luma_nc(int plane, int blk) const noexcept;
    int chroma_nc(int comp, int blk) const noexcept;
    int cbf_ctx_inc(int cat_class, int plane, int blk) const noexcept;
    bool read_residual_block(int cat, int plane, int blk, int start, int end, int max_coeff, int* pos, std::int32_t* lev, int* count);
    int read_cabac_block(int cat, int plane, int max_coeff, int* pos, std::int32_t* lev);
    bool parse_residual_luma(int plane, int list_intra4, int list_inter4);
    bool parse_residual_chroma();
    void switch_plane(int p);
    void plane_sizes_for(int s_chroma_format, int s_separate);
    void finish_field_or_frame();
    void complete_frame(Picture* pic);
    void finalize_pending_field();
    void build_frame_lists(std::vector<Picture*>* init, int lists);
    void build_field_lists(std::vector<RefEntry>* init, int lists);
    void apply_modifications(int list, std::vector<RefEntry>& init, int n, bool* missing);
    void mark_field_references(Picture* pic, const SliceHeader& h);
    void deblock_plane(int plane_id);
    void deblock_mb_generic(int addr);
    void deblock_mb_fast8(int addr);
    void frame_mb_bs(int addr, bool left_ok, bool top_ok, int (*bs)[4][4], bool all_edges = false) const;
    std::int32_t col_ref_lookup(int list, std::int32_t id, int mode) const noexcept;
    void compute_implicit_field_weights();
    void reconstruct_plane_intra(int plane);
    void gather_intra_edge(int plane, int bx, int by, int n, bool inner_tr, IntraEdge& e) const;
    void gather_intra_edge2(int plane, int bx, int by, int n, int nh, bool inner_tr, IntraEdge& e) const;
    template <typename P> void reconstruct_mb_t();
    template <typename P> void reconstruct_intra_t(int plane);
    template <typename P> void reconstruct_chroma_t();
    template <typename P> void reconstruct_inter_t();
    template <typename P> void mc_partition_t(int x4, int y4, int w4, int h4);
    template <typename P> void write_pcm_t();
    template <typename P> void add_residual_plane_t(int plane, bool intra_blocks);
    template <typename P> void deblock_mb_t(int addr, int plane_id);
    template <typename P> void conceal_t();
    template <typename P> void output_t(Picture* pic, VideoFrame& f);
};

} // namespace h264_internal
} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_H264_INTERNAL_HPP
