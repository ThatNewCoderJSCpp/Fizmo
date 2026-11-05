#ifndef FIZMO_VIDEO_HEVC_INTERNAL_HPP
#define FIZMO_VIDEO_HEVC_INTERNAL_HPP

#include "hevc.hpp"

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
namespace hevc_internal {

const int k_max_vps = 16;
const int k_max_sps = 16;
const int k_max_pps = 64;
const int k_max_refs = 16;
const int k_max_dpb = 16;
const int k_pad = 80;

inline int clz32(std::uint32_t v) noexcept {
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

inline int ceil_log2(std::uint32_t v) noexcept {
    int n = 0;
    while ((1u << n) < v && n < 31) ++n;
    return n;
}

class Bits {
public:
    Bits() = default;
    Bits(const std::uint8_t* data, std::size_t size) noexcept { init(data, size); }
    void init(const std::uint8_t* data, std::size_t size) noexcept;
    std::uint32_t u(int n) noexcept;
    bool flag() noexcept { return u(1) != 0; }
    std::uint32_t ue() noexcept;
    std::int32_t se() noexcept;
    void skip(std::size_t n) noexcept;
    void align() noexcept { m_pos = (m_pos + 7) & ~static_cast<std::size_t>(7); }
    bool byte_aligned() const noexcept { return (m_pos & 7) == 0; }
    bool more_rbsp_data() const noexcept { return m_pos < m_trailing; }
    std::size_t position() const noexcept { return m_pos; }
    std::size_t size_bits() const noexcept { return m_bits; }
    std::size_t left() const noexcept { return m_pos < m_bits ? m_bits - m_pos : 0; }
    bool error() const noexcept { return m_error; }
    void set_error() noexcept { m_error = true; }

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_bits = 0;
    std::size_t m_trailing = 0;
    std::size_t m_pos = 0;
    bool m_error = false;
};

void unescape_nal(const std::uint8_t* src, std::size_t size, std::vector<std::uint8_t>& out, std::vector<std::size_t>& ep);

class CabacOracle {
public:
    virtual ~CabacOracle() = default;
    virtual void begin_substream() = 0;
    virtual int decision(std::uint8_t& ctx, int index) = 0;
    virtual int bypass() = 0;
    virtual int terminate(int kind) = 0;
    virtual void align_bypass() = 0;
    virtual std::uint32_t pcm_bits(int n) = 0;
};

CabacOracle*& cabac_oracle() noexcept;

enum TerminateKind {
    TERM_END_OF_SLICE = 1,
    TERM_END_OF_SUBSET = 2,
    TERM_PCM_FLAG = 3
};

class Cabac {
public:
    void init(const std::uint8_t* data, std::size_t size) noexcept;
    int decode(std::uint8_t& ctx) noexcept;
    int bypass() noexcept;
    std::uint32_t bypass_bits(int n) noexcept;
    int terminate(int kind = 0) noexcept;
    void align_bypass() noexcept;
    std::size_t aligned_position() const noexcept;
    bool overrun() const noexcept { return m_over > 2; }
    void attach(CabacOracle* oracle, const std::uint8_t* base) noexcept {
        m_oracle = oracle;
        m_base = base;
    }
    bool oracle() const noexcept { return m_oracle != nullptr; }
    std::uint32_t pcm_bits(int n) noexcept { return m_oracle ? m_oracle->pcm_bits(n) : 0; }

private:
    void refill() noexcept;

    CabacOracle* m_oracle = nullptr;
    const std::uint8_t* m_base = nullptr;

    const std::uint8_t* m_start = nullptr;
    const std::uint8_t* m_ptr = nullptr;
    const std::uint8_t* m_end = nullptr;
    std::uint32_t m_range = 510;
    std::uint32_t m_value = 0;
    int m_bits = 0;
    int m_over = 0;
};

enum CtxIndex {
    CTX_SAO_MERGE = 0,
    CTX_SAO_TYPE = 1,
    CTX_SPLIT_CU = 2,
    CTX_TRANSQUANT_BYPASS = 5,
    CTX_SKIP = 6,
    CTX_PRED_MODE = 9,
    CTX_PART_MODE = 10,
    CTX_PREV_INTRA = 14,
    CTX_CHROMA_PRED = 15,
    CTX_RQT_ROOT_CBF = 16,
    CTX_MERGE_FLAG = 17,
    CTX_MERGE_IDX = 18,
    CTX_INTER_PRED = 19,
    CTX_REF_IDX = 24,
    CTX_MVP_FLAG = 26,
    CTX_SPLIT_TRANSFORM = 27,
    CTX_CBF_LUMA = 30,
    CTX_CBF_CHROMA = 32,
    CTX_MVD_G0 = 37,
    CTX_MVD_G1 = 38,
    CTX_QP_DELTA = 39,
    CTX_TRANSFORM_SKIP = 41,
    CTX_LAST_X = 43,
    CTX_LAST_Y = 61,
    CTX_CSBF = 79,
    CTX_SIG = 83,
    CTX_GT1 = 127,
    CTX_GT2 = 151,
    CTX_RDPCM = 157,
    CTX_RDPCM_DIR = 159,
    CTX_LOG2_RES_SCALE = 161,
    CTX_RES_SCALE_SIGN = 169,
    CTX_CHROMA_QP_OFFSET_FLAG = 171,
    CTX_CHROMA_QP_OFFSET_IDX = 172,
    CTX_COUNT = 173
};

extern const std::uint8_t k_ctx_init[3][CTX_COUNT];
extern const std::uint8_t k_default_scaling_intra[64];
extern const std::uint8_t k_default_scaling_inter[64];
extern const std::int8_t k_intra_pred_angle[35];
extern const std::int16_t k_inv_angle[35];
extern const std::int8_t k_luma_filter[4][8];
extern const std::int8_t k_chroma_filter[8][4];
extern const std::uint8_t k_beta_table[52];
extern const std::uint8_t k_tc_table[54];
extern const std::uint8_t k_chroma_qp_table[70];
extern const std::uint8_t k_intra_422_map[35];
extern const std::uint8_t k_level_scale[6];
extern const std::uint8_t k_sig_ctx_4x4[16];
extern const std::int8_t k_dst4[4][4];

struct ScanTables {
    std::uint8_t pos[4][3][64][2];
    std::int8_t dct[32][32];
};
const ScanTables& scan_tables();

void init_contexts(std::uint8_t* ctx, int init_type, int qp) noexcept;

struct ShortTermRps {
    int num_negative = 0;
    int num_positive = 0;
    int delta_poc[32] = {};
    std::uint8_t used[32] = {};
    int num_delta() const noexcept { return num_negative + num_positive; }
};

struct ScalingList {
    std::uint8_t factor[4][6][1024];
};

void default_scaling_list(ScalingList& sl);
bool parse_scaling_list(Bits& b, ScalingList& sl, bool chroma444);

struct ProfileTierLevel {
    int profile_space = 0;
    int tier = 0;
    int profile_idc = 0;
    std::uint32_t compat = 0;
    bool progressive_source = false;
    bool interlaced_source = false;
    bool frame_only = false;
    int level_idc = 0;
};

struct Vps {
    int id = 0;
    int max_sub_layers = 1;
    bool valid = false;
};

struct Sps {
    int id = 0;
    int vps_id = 0;
    int max_sub_layers = 1;
    ProfileTierLevel ptl;
    int chroma_format_idc = 1;
    bool separate_colour_plane = false;
    int chroma_array_type = 1;
    int width = 0;
    int height = 0;
    int conf_left = 0;
    int conf_right = 0;
    int conf_top = 0;
    int conf_bottom = 0;
    int bit_depth_y = 8;
    int bit_depth_c = 8;
    int log2_max_poc_lsb = 4;
    int max_dec_pic_buffering[8] = {};
    int max_num_reorder[8] = {};
    int max_latency_increase_plus1[8] = {};
    int log2_min_cb = 3;
    int log2_ctb = 4;
    int log2_min_tb = 2;
    int log2_max_tb = 5;
    int max_th_depth_inter = 0;
    int max_th_depth_intra = 0;
    bool scaling_list_enabled = false;
    std::shared_ptr<ScalingList> scaling;
    bool amp = false;
    bool sao = false;
    bool pcm = false;
    int pcm_bit_depth_y = 8;
    int pcm_bit_depth_c = 8;
    int log2_min_pcm = 3;
    int log2_max_pcm = 3;
    bool pcm_loop_filter_disabled = false;
    std::vector<ShortTermRps> st_rps;
    bool long_term_refs_present = false;
    int num_lt_sps = 0;
    int lt_poc_lsb_sps[33] = {};
    std::uint8_t lt_used_sps[33] = {};
    bool temporal_mvp = false;
    bool strong_intra_smoothing = false;
    bool vui_present = false;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
    bool video_signal_present = false;
    int video_format = 5;
    bool full_range = false;
    int colour_primaries = 2;
    int transfer = 2;
    int matrix = 2;
    int chroma_loc = 0;
    bool field_seq = false;
    bool frame_field_info = false;
    std::uint32_t num_units_in_tick = 0;
    std::uint32_t time_scale = 0;
    bool transform_skip_rotation = false;
    bool transform_skip_context = false;
    bool implicit_rdpcm = false;
    bool explicit_rdpcm = false;
    bool extended_precision = false;
    bool intra_smoothing_disabled = false;
    bool high_precision_offsets = false;
    bool persistent_rice = false;
    bool cabac_bypass_alignment = false;
    int sub_width_c = 2;
    int sub_height_c = 2;
    int ctb_size = 16;
    int pic_w_ctb = 0;
    int pic_h_ctb = 0;
    int qp_bd_offset_y = 0;
    int qp_bd_offset_c = 0;
    std::uint32_t generation = 0;
};

struct Pps {
    int id = 0;
    int sps_id = 0;
    bool dependent_slice_segments = false;
    bool output_flag_present = false;
    int num_extra_slice_header_bits = 0;
    bool sign_data_hiding = false;
    bool cabac_init_present = false;
    int num_ref_idx_default[2] = {1, 1};
    int init_qp = 26;
    bool constrained_intra_pred = false;
    bool transform_skip = false;
    bool cu_qp_delta = false;
    int diff_cu_qp_delta_depth = 0;
    int cb_qp_offset = 0;
    int cr_qp_offset = 0;
    bool slice_chroma_qp_offsets_present = false;
    bool weighted_pred = false;
    bool weighted_bipred = false;
    bool transquant_bypass = false;
    bool tiles = false;
    bool entropy_coding_sync = false;
    int num_tile_cols = 1;
    int num_tile_rows = 1;
    bool uniform_spacing = true;
    std::vector<int> col_width;
    std::vector<int> row_height;
    bool loop_filter_across_tiles = true;
    bool loop_filter_across_slices = false;
    bool deblocking_control_present = false;
    bool deblocking_override_enabled = false;
    bool deblocking_disabled = false;
    int beta_offset_div2 = 0;
    int tc_offset_div2 = 0;
    bool scaling_list_present = false;
    std::shared_ptr<ScalingList> scaling;
    bool lists_modification_present = false;
    int log2_parallel_merge_level = 2;
    bool slice_header_extension_present = false;
    int log2_max_transform_skip_size = 2;
    bool cross_component_prediction = false;
    bool chroma_qp_offset_list_enabled = false;
    int diff_cu_chroma_qp_offset_depth = 0;
    int chroma_qp_offset_list_len = 0;
    int cb_qp_offset_list[6] = {};
    int cr_qp_offset_list[6] = {};
    int log2_sao_offset_scale_luma = 0;
    int log2_sao_offset_scale_chroma = 0;
    std::uint32_t generation = 0;
};

bool parse_vps(Bits& b, Vps& vps);
bool parse_sps(Bits& b, Sps& sps, std::string* error);
bool parse_pps(Bits& b, Pps& pps, std::string* error);
bool parse_st_rps(Bits& b, int idx, int num_sets, const std::vector<ShortTermRps>& sets, ShortTermRps& out);

struct Layout {
    std::uint32_t sps_gen = 0;
    std::uint32_t pps_gen = 0;
    int pps_id = -1;
    int sps_id = -1;
    std::vector<int> col_bd;
    std::vector<int> row_bd;
    std::vector<int> rs_to_ts;
    std::vector<int> ts_to_rs;
    std::vector<int> tile_id;
    std::vector<int> ctb_tile_x0;
    std::vector<int> ctb_tile_y0;
    std::vector<std::uint32_t> zs;
    int w4 = 0;
    int h4 = 0;
};

bool build_layout(const Sps& sps, const Pps& pps, Layout& lay);

enum SliceType { SLICE_B = 0, SLICE_P = 1, SLICE_I = 2 };

struct SliceHeader {
    bool first_slice_in_pic = false;
    bool no_output_of_prior_pics = false;
    int pps_id = 0;
    bool dependent = false;
    int segment_address = 0;
    int slice_type = SLICE_I;
    bool pic_output_flag = true;
    int colour_plane_id = 0;
    int poc_lsb = 0;
    bool st_rps_sps_flag = false;
    int st_rps_idx = 0;
    ShortTermRps st_rps;
    int num_lt = 0;
    int num_lt_sps = 0;
    int poc_lsb_lt[33] = {};
    std::uint8_t used_by_curr_lt[33] = {};
    std::uint8_t delta_poc_msb_present[33] = {};
    int delta_poc_msb_cycle_lt[33] = {};
    bool temporal_mvp = false;
    bool sao_luma = false;
    bool sao_chroma = false;
    int num_ref_idx[2] = {0, 0};
    bool rpl_mod[2] = {false, false};
    int list_entry[2][16] = {};
    bool mvd_l1_zero = false;
    bool cabac_init = false;
    bool collocated_from_l0 = true;
    int collocated_ref_idx = 0;
    int luma_log2_wd = 0;
    int chroma_log2_wd = 0;
    int luma_w[2][16] = {};
    int luma_o[2][16] = {};
    int chroma_w[2][16][2] = {};
    int chroma_o[2][16][2] = {};
    bool luma_w_flag[2][16] = {};
    bool chroma_w_flag[2][16] = {};
    int max_num_merge_cand = 5;
    int slice_qp = 26;
    int cb_qp_offset = 0;
    int cr_qp_offset = 0;
    bool cu_chroma_qp_offset_enabled = false;
    bool deblocking_disabled = false;
    int beta_offset_div2 = 0;
    int tc_offset_div2 = 0;
    bool lf_across_slices = false;
    std::vector<std::uint32_t> entry_points;
    std::size_t data_offset = 0;
    int nal_type = 0;
    int temporal_id = 0;
    int slice_addr_rs = 0;
    int num_pic_total_curr = 0;
};

struct MvField {
    std::int16_t mv[2][2];
    std::int8_t ref_idx[2];
    std::uint8_t pred;
    std::uint8_t pad;
};

struct SliceRefs {
    int poc[2][k_max_refs];
    std::uint8_t lt[2][k_max_refs];
    std::uint32_t uid[2][k_max_refs];
};

struct PictureHash {
    int type = -1;
    std::uint8_t md5[3][16];
    std::uint32_t value[3] = {0, 0, 0};
};

struct Picture {
    std::vector<std::uint16_t> buf[3];
    std::uint16_t* plane[3] = {nullptr, nullptr, nullptr};
    std::ptrdiff_t stride[3] = {0, 0, 0};
    int width[3] = {0, 0, 0};
    int height[3] = {0, 0, 0};
    int chroma_format = 1;
    int poc = 0;
    std::uint32_t uid = 0;
    int ref = 0;
    bool output = false;
    int latency = 0;
    bool generated = false;
    bool in_use = false;
    std::vector<MvField> mvf;
    int mvf_stride = 0;
    std::vector<std::uint16_t> ctb_slice;
    std::vector<SliceRefs> slices;
    double pts = 0.0;
    int pic_struct = 0;
    bool keyframe = false;
    int nal_type = 0;
    PictureHash hash;
    std::uint64_t decode_index = 0;
    std::shared_ptr<const Sps> sps;
    bool output_flag = true;
    bool field = false;

    bool allocate(const Sps& sps);
    void extend_borders();
};

struct SaoParams {
    std::uint8_t type[3];
    std::uint8_t band_pos[3];
    std::uint8_t eo_class[3];
    std::int16_t offset[3][5];
};

struct SliceParams {
    int beta_offset = 0;
    int tc_offset = 0;
    bool deblocking_disabled = false;
    bool lf_across_slices = false;
    bool sao_luma = false;
    bool sao_chroma = false;
    int slice_addr_rs = 0;
};

enum CuFlag {
    CU_INTRA = 1,
    CU_SKIP = 2,
    CU_NOFILTER = 4,
    CU_BYPASS = 8,
    CU_PCM = 16
};

enum PartMode {
    PART_2Nx2N = 0,
    PART_2NxN = 1,
    PART_Nx2N = 2,
    PART_NxN = 3,
    PART_2NxnU = 4,
    PART_2NxnD = 5,
    PART_nLx2N = 6,
    PART_nRx2N = 7
};

struct Mv {
    int x;
    int y;
};

struct PuMotion {
    Mv mv[2];
    int ref_idx[2];
    int pred;
};

class PicDecoder {
public:
    PicDecoder();

    bool begin(const Sps& sps, const Pps& pps, const Layout& lay, Picture* pic);
    bool decode_slice(const SliceHeader& sh, const std::uint8_t* data, std::size_t size, const std::vector<std::size_t>& substreams,
                      Picture* const refs[2][k_max_refs], std::string* error);
    void finish();
    bool had_error() const noexcept { return m_error; }
    int slice_count() const noexcept { return static_cast<int>(m_slices.size()); }

private:
    void sao_syntax(int rx, int ry, int ctb_rs);
    void coding_quadtree(int x0, int y0, int log2, int depth);
    void coding_unit(int x0, int y0, int log2);
    void prediction_unit(int x0, int y0, int w, int h, int part_idx);
    void pcm_sample(int x0, int y0, int log2);
    void fill_map(std::vector<std::uint8_t>& map, int x0, int y0, int w, int h, std::uint8_t v);
    int decode_part_mode(bool intra, int log2);
    void transform_tree(int x0, int y0, int xb, int yb, int log2, int depth, int blk, int max_depth, bool intra_split, int parent_cbf_cb, int parent_cbf_cr);
    void transform_unit(int x0, int y0, int xb, int yb, int log2, int depth, int blk, int cbf_luma, int cbf_cb, int cbf_cr);
    void residual_coding(int x0, int y0, int log2, int cidx, std::int32_t* out);
    void decode_qp_delta();
    void decode_chroma_qp_offset();
    void start_qg(int xq, int yq);
    void update_qp();
    int chroma_qp(int cidx) const;

    bool avail(int xc, int yc, int xn, int yn) const noexcept;
    bool avail_lt(int xn, int yn) const noexcept;
    int intra_mode_luma_at(int x, int y) const noexcept;
    int chroma_mode_at(int x, int y) const noexcept;
    int chroma_syntax_at(int x, int y) const noexcept;

    void intra_predict(int cidx, int x, int y, int log2, int mode);
    void reconstruct(int cidx, int x, int y, int log2, const std::int32_t* res);
    void transform_residual(int cidx, int log2, bool ts, bool intra, int mode, std::int32_t* coeff, int max_x, int max_y, std::int32_t* out);
    void cross_component(int log2, int res_scale, std::int32_t* res_c, const std::int32_t* res_y);

    void derive_merge(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int merge_idx, PuMotion& out);
    void derive_amvp(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int lx, int ref_idx, Mv& out, int mvp_flag);
    bool temporal_mv(int xpb, int ypb, int w, int h, int ref_idx, int lx, Mv& out);
    bool col_mv(const Picture* col, int x, int y, int ref_idx, int lx, Mv& out);
    bool pb_avail(int xcb, int ycb, int ncbs, int xpb, int ypb, int w, int h, int part_idx, int xn, int yn) const;
    const MvField& mvf_at(int x, int y) const noexcept { return m_pic->mvf[static_cast<std::size_t>(y >> 2) * m_pic->mvf_stride + (x >> 2)]; }
    void store_motion(int x, int y, int w, int h, const PuMotion& m);
    void motion_compensate(int x, int y, int w, int h, const PuMotion& m);
    void mc_luma(const Picture* ref, int x, int y, int w, int h, Mv mv, std::int16_t* dst);
    void mc_chroma(const Picture* ref, int cidx, int x, int y, int w, int h, Mv mv, std::int16_t* dst);
    int diff_poc_ref(int lx, int idx) const noexcept;

    void mark_tu_edges(int x0, int y0, int size);
    void mark_pu_edges(int x0, int y0, int w, int h);
    void deblock();
    void deblock_edges(bool vertical);
    int compute_bs(int xq, int yq, int xp, int yp, bool tu_edge) const;
    void sao_apply();
    void sao_ctb(int cidx, int rx, int ry, const std::uint16_t* src, std::ptrdiff_t sstride);

    std::size_t idx4(int x, int y) const noexcept { return static_cast<std::size_t>(y >> 2) * m_w4 + static_cast<std::size_t>(x >> 2); }

    const Sps* m_sps = nullptr;
    const Pps* m_pps = nullptr;
    const Layout* m_lay = nullptr;
    Picture* m_pic = nullptr;
    const SliceHeader* m_sh = nullptr;
    Picture* m_refs[2][k_max_refs];
    int m_ref_poc[2][k_max_refs];
    bool m_ref_lt[2][k_max_refs];
    bool m_no_backward_pred = false;

    int m_w = 0;
    int m_h = 0;
    int m_w4 = 0;
    int m_h4 = 0;
    int m_log2_ctb = 4;
    int m_ctb_size = 16;
    int m_min_cb = 3;
    int m_bd_y = 8;
    int m_bd_c = 8;
    int m_chroma = 1;
    int m_sx = 1;
    int m_sy = 1;
    bool m_error = false;

    std::vector<std::uint8_t> m_cu_flags;
    std::vector<std::uint8_t> m_ct_depth;
    std::vector<std::uint8_t> m_ipm;
    std::vector<std::int8_t> m_qp;
    std::vector<std::uint8_t> m_nz;
    std::vector<std::uint8_t> m_edge_v;
    std::vector<std::uint8_t> m_edge_h;
    std::vector<std::uint8_t> m_bs;
    std::vector<int> m_ctb_slice_addr;
    std::vector<std::uint16_t> m_ctb_slice_idx;
    std::vector<SaoParams> m_sao;
    std::vector<SliceParams> m_slices;
    bool m_any_nofilter = false;
    bool m_any_sao = false;
    bool m_any_deblock = false;

    Cabac m_cabac;
    std::uint8_t m_ctx[CTX_COUNT];
    std::uint8_t m_ctx_wpp[CTX_COUNT];
    std::uint8_t m_ctx_ds[CTX_COUNT];
    int m_stat[4] = {0, 0, 0, 0};
    int m_stat_wpp[4] = {0, 0, 0, 0};
    int m_stat_ds[4] = {0, 0, 0, 0};
    bool m_ds_valid = false;

    int m_ctb_rs = 0;
    int m_ctb_ts = 0;
    int m_slice_addr = 0;
    int m_slice_idx = 0;
    int m_slice_type = SLICE_I;
    int m_init_type = 0;

    int m_qp_y = 26;
    int m_qp_y_pred = 26;
    int m_last_qp = 26;
    int m_cu_qp_delta = 0;
    bool m_qp_delta_coded = false;
    bool m_chroma_qp_offset_coded = false;
    int m_cu_qp_offset_cb = 0;
    int m_cu_qp_offset_cr = 0;
    int m_log2_min_qg = 4;
    int m_log2_min_cqg = 4;

    int m_cur_depth = 0;
    bool m_cu_pcm = false;
    std::size_t m_cur_sub_start = 0;
    std::size_t m_cur_sub_end = 0;
    const std::uint8_t* m_cur_data = nullptr;
    int m_tu_x = 0;
    int m_tu_y = 0;
    int m_cu_x = 0;
    int m_cu_y = 0;
    int m_cu_log2 = 3;
    bool m_cu_intra = false;
    bool m_cu_bypass = false;
    int m_cu_part = PART_2Nx2N;
    int m_luma_mode[4] = {1, 1, 1, 1};
    int m_chroma_mode[4] = {1, 1, 1, 1};
    int m_chroma_syntax[4] = {0, 0, 0, 0};
    bool m_merge_flag = false;
    int m_res_scale = 0;
    bool m_ts_flag = false;
    bool m_rdpcm_explicit = false;
    int m_rdpcm_dir = 0;

    std::int32_t m_coeff[32 * 32];
    std::int32_t m_res[32 * 32];
    std::int32_t m_res_y[32 * 32];
    std::int32_t m_res_c[32 * 32];
    std::int32_t m_tmp[32 * 32];
    std::int16_t m_pred[2][64 * 64];
    std::int16_t m_mc_tmp[(64 + 7) * 64];
    std::vector<std::uint16_t> m_sao_buf[3];
};

} // namespace hevc_internal

struct HevcDecoder::State {
    State();

    bool open_hvcc(const std::uint8_t* data, std::size_t size, std::string* error);
    bool decode_packet(const std::uint8_t* data, std::size_t size, double pts, std::string* error);
    bool handle_nal(const std::uint8_t* data, std::size_t size, double pts, std::string* error);
    bool handle_slice(int nal_type, int temporal_id, double pts, std::string* error);
    bool parse_slice_header(hevc_internal::Bits& b, int nal_type, hevc_internal::SliceHeader& sh, std::string* error);
    void parse_sei(hevc_internal::Bits& b, bool suffix);
    bool start_picture(const hevc_internal::SliceHeader& sh, double pts, std::string* error);
    void finish_picture();
    bool apply_rps(const hevc_internal::SliceHeader& sh);
    bool build_ref_lists(const hevc_internal::SliceHeader& sh, hevc_internal::Picture* refs[2][hevc_internal::k_max_refs]);
    hevc_internal::Picture* alloc_picture();
    hevc_internal::Picture* generate_missing(int poc, bool long_term);
    void bump(bool flush_all);
    void output_picture(hevc_internal::Picture* pic);
    void emit_frame(VideoFrame& f, bool keyframe);
    void flush_field();
    void remove_unused();
    void verify_hash(hevc_internal::Picture* pic);
    void update_info(const hevc_internal::Sps& sps);
    void reset_state(bool keep_params);

    std::unique_ptr<hevc_internal::Vps> vps[hevc_internal::k_max_vps];
    std::shared_ptr<hevc_internal::Sps> sps[hevc_internal::k_max_sps];
    std::shared_ptr<hevc_internal::Pps> pps[hevc_internal::k_max_pps];
    std::shared_ptr<const hevc_internal::Sps> active_sps;
    std::shared_ptr<const hevc_internal::Pps> active_pps;
    hevc_internal::Layout layout;
    std::vector<std::unique_ptr<hevc_internal::Picture>> pool;
    hevc_internal::Picture* cur = nullptr;
    std::vector<hevc_internal::Picture*> st_before;
    std::vector<hevc_internal::Picture*> st_after;
    std::vector<hevc_internal::Picture*> lt_curr;
    std::shared_ptr<const hevc_internal::Sps> latest_sps;
    bool no_rasl_output = false;
    bool skipping_picture = false;
    bool handle_cra_as_bla = false;
    int cur_temporal_id = 0;
    hevc_internal::PicDecoder pic_decoder;
    hevc_internal::SliceHeader last_independent;
    bool have_independent = false;
    std::vector<std::uint8_t> rbsp;
    std::vector<std::size_t> ep;
    std::deque<VideoFrame> output;
    bool annexb = false;
    unsigned length_size = 4;
    bool open = false;
    bool first_picture = true;
    bool after_eos = true;
    bool skip_rasl = false;
    bool waiting_irap = true;
    int prev_tid0_poc = 0;
    int poc = 0;
    std::uint32_t uid_counter = 0;
    std::uint32_t generation = 0;
    std::uint64_t decode_counter = 0;
    int pending_pic_struct = -1;
    hevc_internal::PictureHash pending_hash;
    bool verify = false;
    bool weave = true;
    bool have_field = false;
    VideoFrame field;
    int field_parity = 0;
    std::uint64_t frames = 0;
    std::uint64_t errors = 0;
    HevcInfo info;
    HevcStats stats;
    int max_temporal_id = 0;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_HEVC_INTERNAL_HPP
