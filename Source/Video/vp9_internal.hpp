#ifndef FIZMO_VIDEO_VP9_INTERNAL_HPP
#define FIZMO_VIDEO_VP9_INTERNAL_HPP

#include "vp9.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {
namespace vp9_detail {

inline int imin(int a, int b) noexcept { return a < b ? a : b; }
inline int imax(int a, int b) noexcept { return a > b ? a : b; }
inline int iclamp(int v, int lo, int hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

enum : int {
    kKeyFrame = 0,
    kNonKeyFrame = 1
};

enum : int {
    kIntraFrame = 0,
    kLastFrame = 1,
    kGoldenFrame = 2,
    kAltrefFrame = 3,
    kNoneFrame = -1
};

enum : int {
    kBlock4x4 = 0,
    kBlock4x8,
    kBlock8x4,
    kBlock8x8,
    kBlock8x16,
    kBlock16x8,
    kBlock16x16,
    kBlock16x32,
    kBlock32x16,
    kBlock32x32,
    kBlock32x64,
    kBlock64x32,
    kBlock64x64,
    kBlockInvalid
};

enum : int {
    kDcPred = 0,
    kVPred,
    kHPred,
    kD45Pred,
    kD135Pred,
    kD117Pred,
    kD153Pred,
    kD207Pred,
    kD63Pred,
    kTmPred,
    kNearestMv,
    kNearMv,
    kZeroMv,
    kNewMv
};

enum : int {
    kTx4x4 = 0,
    kTx8x8,
    kTx16x16,
    kTx32x32
};

enum : int {
    kOnly4x4 = 0,
    kAllow8x8,
    kAllow16x16,
    kAllow32x32,
    kTxModeSelect
};

enum : int {
    kDctDct = 0,
    kAdstDct,
    kDctAdst,
    kAdstAdst
};

enum : int {
    kPartitionNone = 0,
    kPartitionHorz,
    kPartitionVert,
    kPartitionSplit
};

enum : int {
    kSingleReference = 0,
    kCompoundReference,
    kReferenceModeSelect
};

enum : int {
    kFilterRegular = 0,
    kFilterSmooth,
    kFilterSharp,
    kFilterBilinear,
    kFilterSwitchable
};

enum : int {
    kSegLvlAltQ = 0,
    kSegLvlAltL,
    kSegLvlRefFrame,
    kSegLvlSkip
};

enum : int {
    kMvJointZero = 0,
    kMvJointHnzvz,
    kMvJointHzvnz,
    kMvJointHnzvnz
};

const int kMaxSegments = 8;
const int kMaxLoopFilter = 63;
const int kMinTileWidthB64 = 4;
const int kMaxTileWidthB64 = 64;
const int kNumRefFrames = 8;
const int kCompandedMvrefThresh = 8;
const int kMvBorder = 16 << 3;
const int kBestMvMargin = (160 - 4) << 3;

extern const std::uint8_t k_num_4x4_w[13];
extern const std::uint8_t k_num_4x4_h[13];
extern const std::uint8_t k_num_8x8_w[13];
extern const std::uint8_t k_num_8x8_h[13];
extern const std::uint8_t k_mi_width_log2[13];
extern const std::uint8_t k_size_group[13];
extern const std::uint8_t k_max_tx_size[13];
extern const std::uint8_t k_subsize[4][13];
extern const std::uint8_t k_partition_ctx_above[13];
extern const std::uint8_t k_partition_ctx_left[13];
extern const std::uint8_t k_tx_mode_to_biggest[5];
extern const std::uint8_t k_intra_mode_to_tx_type[10];
extern const std::int8_t k_mv_ref_blocks[13][8][2];
extern const std::uint8_t k_mode_2_counter[14];
extern const std::uint8_t k_counter_to_context[19];
extern const std::uint8_t k_idx_n_column_to_subblock[4][2];
extern const std::uint8_t k_mode_lf_lut[14];
extern const std::uint8_t k_seg_feature_bits[4];
extern const std::uint8_t k_seg_feature_signed[4];
extern const std::uint8_t k_literal_to_filter[4];

extern const std::int8_t k_intra_mode_tree[18];
extern const std::int8_t k_segment_tree[14];
extern const std::int8_t k_partition_tree[6];
extern const std::int8_t k_inter_mode_tree[6];
extern const std::int8_t k_interp_filter_tree[4];
extern const std::int8_t k_mv_joint_tree[6];
extern const std::int8_t k_mv_class_tree[20];
extern const std::int8_t k_mv_fr_tree[6];

extern const std::int16_t k_dc_qlookup[3][256];
extern const std::int16_t k_ac_qlookup[3][256];

extern const std::uint8_t k_kf_y_mode_probs[10][10][9];
extern const std::uint8_t k_kf_uv_mode_probs[10][9];
extern const std::uint8_t k_kf_partition_probs[16][3];
extern const std::uint8_t k_cat_probs[6][18];
extern const std::uint8_t k_cat6_prob[18];
extern const std::uint8_t k_band_4x4[16];
extern const std::uint8_t k_energy_class[12];
extern const std::int16_t k_subpel_filters[4][16][8];

const std::uint8_t* pareto_row(int prob) noexcept;
const std::uint8_t* band_8x8_table() noexcept;
const std::uint8_t* inv_map_table() noexcept;

struct NormTable {
    std::uint8_t v[256];
    constexpr NormTable() : v() {
        for (int i = 1; i < 256; ++i) {
            int n = 0;
            int r = i;
            while (r < 128) {
                r <<= 1;
                ++n;
            }
            v[i] = static_cast<std::uint8_t>(n);
        }
    }
};

inline constexpr NormTable k_norm_table{};

extern const std::int16_t k_default_scan_4x4[16];
extern const std::int16_t k_col_scan_4x4[16];
extern const std::int16_t k_row_scan_4x4[16];
extern const std::int16_t k_default_scan_8x8[64];
extern const std::int16_t k_col_scan_8x8[64];
extern const std::int16_t k_row_scan_8x8[64];
extern const std::int16_t k_default_scan_16x16[256];
extern const std::int16_t k_col_scan_16x16[256];
extern const std::int16_t k_row_scan_16x16[256];
extern const std::int16_t k_default_scan_32x32[1024];

struct ScanOrder {
    const std::int16_t* scan;
    const std::int16_t* neighbors;
};

const ScanOrder& scan_order(int tx_size, int tx_type) noexcept;
bool verify_tables() noexcept;

struct ProbContext {
    std::uint8_t tx8[2][1];
    std::uint8_t tx16[2][2];
    std::uint8_t tx32[2][3];
    std::uint8_t coef[4][2][2][6][6][3];
    std::uint8_t skip[3];
    std::uint8_t inter_mode[7][3];
    std::uint8_t interp_filter[4][2];
    std::uint8_t is_inter[4];
    std::uint8_t comp_mode[5];
    std::uint8_t single_ref[5][2];
    std::uint8_t comp_ref[5];
    std::uint8_t y_mode[4][9];
    std::uint8_t uv_mode[10][9];
    std::uint8_t partition[16][3];
    std::uint8_t mv_joint[3];
    std::uint8_t mv_sign[2];
    std::uint8_t mv_class[2][10];
    std::uint8_t mv_class0_bit[2];
    std::uint8_t mv_bits[2][10];
    std::uint8_t mv_class0_fr[2][2][3];
    std::uint8_t mv_fr[2][3];
    std::uint8_t mv_class0_hp[2];
    std::uint8_t mv_hp[2];
};

void default_probs(ProbContext& fc) noexcept;

struct Counts {
    std::uint32_t partition[16][4];
    std::uint32_t skip[3][2];
    std::uint32_t tx8[2][2];
    std::uint32_t tx16[2][3];
    std::uint32_t tx32[2][4];
    std::uint32_t coef[4][2][2][6][6][4];
    std::uint32_t eob_branch[4][2][2][6][6];
    std::uint32_t y_mode[4][10];
    std::uint32_t uv_mode[10][10];
    std::uint32_t is_inter[4][2];
    std::uint32_t comp_mode[5][2];
    std::uint32_t single_ref[5][2][2];
    std::uint32_t comp_ref[5][2];
    std::uint32_t inter_mode[7][4];
    std::uint32_t interp_filter[4][3];
    std::uint32_t mv_joint[4];
    std::uint32_t mv_sign[2][2];
    std::uint32_t mv_class[2][11];
    std::uint32_t mv_class0_bit[2][2];
    std::uint32_t mv_bits[2][10][2];
    std::uint32_t mv_class0_fr[2][2][4];
    std::uint32_t mv_fr[2][4];
    std::uint32_t mv_class0_hp[2][2];
    std::uint32_t mv_hp[2][2];

    void clear() noexcept { std::memset(this, 0, sizeof(*this)); }
    void add(const Counts& o) noexcept;
};

void adapt_coef_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool intra_frame, bool last_was_key) noexcept;
void adapt_mode_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool switchable_interp, bool tx_select) noexcept;
void adapt_mv_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool allow_hp) noexcept;

class BoolDecoder {
public:
    bool init(const std::uint8_t* data, std::size_t size) noexcept {
        m_p = data;
        m_end = data + size;
        m_value = 0;
        m_bits = 0;
        m_range = 255;
        m_exhausted = false;
        if (size == 0) return false;
        fill();
        return read(128) == 0;
    }

    inline int read(int prob) noexcept {
        if (m_bits < 8) fill();
        const unsigned split = 1u + (((m_range - 1u) * static_cast<unsigned>(prob)) >> 8);
        const std::uint64_t big = static_cast<std::uint64_t>(split) << 56;
        int bit;
        if (m_value >= big) {
            m_range -= split;
            m_value -= big;
            bit = 1;
        } else {
            m_range = split;
            bit = 0;
        }
        const int shift = k_norm_table.v[m_range];
        m_range <<= shift;
        m_value <<= shift;
        m_bits -= shift;
        return bit;
    }

    inline int read_bit() noexcept { return read(128); }

    inline unsigned read_literal(int n) noexcept {
        unsigned v = 0;
        for (int i = 0; i < n; ++i) v = (v << 1) | static_cast<unsigned>(read(128));
        return v;
    }

    inline int read_tree(const std::int8_t* tree, const std::uint8_t* probs) noexcept {
        int n = 0;
        do {
            n = tree[n + read(probs[n >> 1])];
        } while (n > 0);
        return -n;
    }

    bool exhausted() const noexcept { return m_exhausted; }
    bool overrun() const noexcept { return m_exhausted && (0x40000000 - m_bits) > 64; }

private:
    void fill() noexcept {
        while (m_bits <= 56) {
            if (m_p < m_end) {
                m_value |= static_cast<std::uint64_t>(*m_p++) << (56 - m_bits);
                m_bits += 8;
            } else {
                m_exhausted = true;
                m_valid_at_end = m_bits;
                m_bits += 0x40000000;
                break;
            }
        }
    }

    const std::uint8_t* m_p = nullptr;
    const std::uint8_t* m_end = nullptr;
    std::uint64_t m_value = 0;
    int m_bits = 0;
    unsigned m_range = 255;
    bool m_exhausted = false;
    int m_valid_at_end = 0;
};

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

    unsigned bit() noexcept {
        if ((m_pos >> 3) >= m_size) {
            m_fail = true;
            return 0;
        }
        const unsigned v = (m_data[m_pos >> 3] >> (7 - (m_pos & 7))) & 1u;
        ++m_pos;
        return v;
    }

    unsigned bits(int n) noexcept {
        unsigned v = 0;
        for (int i = 0; i < n; ++i) v = (v << 1) | bit();
        return v;
    }

    int signed_bits(int n) noexcept {
        const int v = static_cast<int>(bits(n));
        return bit() ? -v : v;
    }

    std::size_t byte_pos() const noexcept { return (m_pos + 7) >> 3; }
    bool failed() const noexcept { return m_fail; }

private:
    const std::uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_pos = 0;
    bool m_fail = false;
};

struct Mv {
    std::int16_t row = 0;
    std::int16_t col = 0;
};

inline bool mv_equal(const Mv& a, const Mv& b) noexcept { return a.row == b.row && a.col == b.col; }

struct ModeInfo {
    Mv mv[2];
    Mv bmv[4][2];
    std::int8_t ref_frame[2] = {0, -1};
    std::uint8_t sb_type = 0;
    std::uint8_t y_mode = 0;
    std::uint8_t sub_modes[4] = {0, 0, 0, 0};
    std::uint8_t uv_mode = 0;
    std::uint8_t skip = 0;
    std::uint8_t tx_size = 0;
    std::uint8_t interp_filter = 0;
    std::uint8_t segment_id = 0;
    std::uint8_t seg_id_predicted = 0;
    std::uint8_t is_inter = 0;
};

struct MvRef {
    Mv mv[2];
    std::int8_t ref_frame[2] = {0, -1};
};

struct Picture {
    int width = 0;
    int height = 0;
    int ssx = 1;
    int ssy = 1;
    int bit_depth = 8;
    int mi_cols = 0;
    int mi_rows = 0;
    int plane_w[3] = {0, 0, 0};
    int plane_h[3] = {0, 0, 0};
    int alloc_w[3] = {0, 0, 0};
    int alloc_h[3] = {0, 0, 0};
    std::ptrdiff_t stride[3] = {0, 0, 0};
    std::size_t offset[3] = {0, 0, 0};
    std::vector<std::uint16_t> data;
    std::vector<MvRef> mvs;
    int color_space = 0;
    int color_range = 0;
    int render_width = 0;
    int render_height = 0;
    bool intra_only = false;

    bool allocate(int w, int h, int sx, int sy, int bd);
    std::uint16_t* plane(int p) noexcept { return data.data() + offset[p]; }
    const std::uint16_t* plane(int p) const noexcept { return data.data() + offset[p]; }
};

struct ScaleFactors {
    int x_scale = 1 << 14;
    int y_scale = 1 << 14;
    int x_step = 16;
    int y_step = 16;
    bool scaled = false;
    bool valid = false;
};

struct SegmentationParams {
    bool enabled = false;
    bool update_map = false;
    bool temporal_update = false;
    bool update_data = false;
    bool abs_delta = false;
    std::uint8_t tree_probs[7] = {255, 255, 255, 255, 255, 255, 255};
    std::uint8_t pred_probs[3] = {255, 255, 255};
    bool feature_enabled[kMaxSegments][4] = {};
    int feature_data[kMaxSegments][4] = {};

    bool active(int seg, int feature) const noexcept { return enabled && feature_enabled[seg][feature]; }
};

struct LoopFilterParams {
    int level = 0;
    int sharpness = 0;
    bool delta_enabled = false;
    int ref_deltas[4] = {1, 0, -1, -1};
    int mode_deltas[2] = {0, 0};
};

struct FrameHeader {
    int profile = 0;
    bool show_existing_frame = false;
    int frame_to_show = 0;
    int frame_type = kKeyFrame;
    bool show_frame = true;
    bool error_resilient_mode = false;
    bool intra_only = false;
    int reset_frame_context = 0;
    int refresh_frame_flags = 0;
    int ref_frame_idx[3] = {0, 0, 0};
    int ref_frame_sign_bias[4] = {0, 0, 0, 0};
    bool allow_high_precision_mv = false;
    int interp_filter = 0;
    bool refresh_frame_context = false;
    bool frame_parallel_decoding_mode = false;
    int frame_context_idx = 0;
    int width = 0;
    int height = 0;
    int render_width = 0;
    int render_height = 0;
    int base_q_idx = 0;
    int delta_q_y_dc = 0;
    int delta_q_uv_dc = 0;
    int delta_q_uv_ac = 0;
    bool lossless = false;
    int tile_cols_log2 = 0;
    int tile_rows_log2 = 0;
    std::size_t header_size = 0;
    std::size_t uncompressed_size = 0;
    int tx_mode = 0;
    int reference_mode = kSingleReference;
    int comp_fixed_ref = kAltrefFrame;
    int comp_var_ref[2] = {kLastFrame, kGoldenFrame};
    bool frame_is_intra = true;
};

class TileDecoder;

struct LfThresholds {
    std::uint8_t mblim[64];
    std::uint8_t lim[64];
    std::uint8_t hev_thr[64];
};

void build_lf_thresholds(LfThresholds& t, int sharpness) noexcept;

struct FrameDecodeContext {
    FrameHeader* hdr = nullptr;
    const SegmentationParams* seg = nullptr;
    const LoopFilterParams* lf = nullptr;
    const ProbContext* fc = nullptr;
    Picture* cur = nullptr;
    const Picture* refs[3] = {nullptr, nullptr, nullptr};
    ScaleFactors scale[3];
    const MvRef* prev_mvs = nullptr;
    const std::uint8_t* prev_seg = nullptr;
    std::uint8_t* cur_seg = nullptr;
    ModeInfo* mi = nullptr;
    int mi_stride = 0;
    int mi_cols = 0;
    int mi_rows = 0;
    int ssx = 1;
    int ssy = 1;
    int bit_depth = 8;
    std::uint8_t* above_nonzero[3] = {nullptr, nullptr, nullptr};
    std::uint8_t* above_partition = nullptr;
    std::int16_t dequant[kMaxSegments][2][2];
    std::uint8_t lf_level[kMaxSegments][4][2];
    bool counting = false;
};

class TileDecoder {
public:
    TileDecoder();

    bool decode(FrameDecodeContext& ctx, const std::uint8_t* data, std::size_t size, int mi_row_start, int mi_row_end, int mi_col_start, int mi_col_end);
    Counts counts;
    bool corrupted = false;

private:
    struct Block;

    void decode_partition(int mi_row, int mi_col, int bsize);
    void decode_block(int mi_row, int mi_col, int bsize);
    void intra_frame_mode_info(Block& b, ModeInfo& mi);
    void inter_frame_mode_info(Block& b, ModeInfo& mi);
    void intra_block_mode_info(Block& b, ModeInfo& mi);
    void inter_block_mode_info(Block& b, ModeInfo& mi);
    int read_intra_segment_id(Block& b);
    int read_inter_segment_id(Block& b, ModeInfo& mi);
    int read_skip(Block& b, int segment_id);
    int read_tx_size(Block& b, bool allow_select);
    void read_ref_frames(Block& b, ModeInfo& mi);
    int read_mv_component(int comp, bool use_hp);
    void read_mv(Mv& out, const Mv& ref, bool allow_hp);
    void find_mv_refs(Block& b, int ref_frame, int block, Mv* list, int* mode_context);
    void append_sub8x8_mvs(Block& b, ModeInfo& mi, int block, int ref, Mv& nearest, Mv& near_mv);
    int decode_coefs(int plane, int tx_size, int tx_type, int ctx, const std::int16_t* dq, bool is_inter);
    void reconstruct_intra(Block& b, ModeInfo& mi);
    void reconstruct_inter(Block& b, ModeInfo& mi);
    void predict_inter(Block& b, ModeInfo& mi, int plane, int ref, int x, int y, int w, int h, Mv mv, std::uint16_t* dst, std::ptrdiff_t stride, int pw, int ph);

    FrameDecodeContext* m_ctx = nullptr;
    BoolDecoder m_bd;
    int m_mi_col_start = 0;
    int m_mi_col_end = 0;
    std::uint8_t m_left_nonzero[3][16];
    std::uint8_t m_left_partition[8];
    std::vector<std::int32_t> m_coef;
    std::uint8_t m_token_cache[1024];
    std::vector<std::uint16_t> m_pred;
    std::vector<std::uint16_t> m_mc_buf;
    std::vector<std::int16_t> m_mc_tmp;
};

void predict_intra(std::uint16_t* dst, std::ptrdiff_t stride, int mode, int tx_size, bool have_left, bool have_top, bool have_right, int x0, int y0, int frame_w, int frame_h, int bd) noexcept;
void inverse_transform_add(std::uint16_t* dst, std::ptrdiff_t stride, std::int32_t* coef, int tx_size, int tx_type, int eob, bool lossless, int bd) noexcept;
void convolve(const std::uint16_t* src, std::ptrdiff_t src_stride, std::uint16_t* dst, std::ptrdiff_t dst_stride, const std::int16_t (*kernel)[8], int fx, int xs, int fy, int ys, int w, int h, int bd, bool average, std::int16_t* tmp) noexcept;

void loop_filter_frame(Picture& pic, const ModeInfo* mi, int mi_stride, const std::uint8_t (*lf_level)[4][2], int sharpness, int threads);

} // namespace vp9_detail
} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_VP9_INTERNAL_HPP
