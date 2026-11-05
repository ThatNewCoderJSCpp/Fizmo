#ifndef FIZMO_VIDEO_AV1_INTERNAL_HPP
#define FIZMO_VIDEO_AV1_INTERNAL_HPP

#include "av1.hpp"
#include "av1_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fizmo {
namespace video {
namespace av1 {


enum Av1Stat {
    ST_PALETTE_Y,
    ST_PALETTE_UV,
    ST_INTRABC,
    ST_FILTER_INTRA,
    ST_CFL,
    ST_ANGLE_DELTA,
    ST_EDGE_UPSAMPLE,
    ST_EDGE_FILTER,
    ST_SMOOTH,
    ST_PAETH,
    ST_DIRECTIONAL,
    ST_II_SMOOTH,
    ST_II_WEDGE,
    ST_COMP_WEDGE,
    ST_COMP_DIFFWTD,
    ST_COMP_DIST,
    ST_COMP_AVG,
    ST_OBMC,
    ST_LOCALWARP,
    ST_GLOBALWARP,
    ST_SCALED_REF,
    ST_DUAL_FILTER,
    ST_WHT,
    ST_QM,
    ST_DELTA_Q,
    ST_DELTA_LF,
    ST_SEG,
    ST_LF,
    ST_CDEF,
    ST_LR_WIENER,
    ST_LR_SGR,
    ST_SUPERRES,
    ST_GRAIN,
    ST_TILES,
    ST_SHOW_EXISTING,
    ST_SIZE_CHANGE,
    ST_INTRA_ONLY,
    ST_SWITCH_FRAME,
    ST_SKIP_MODE,
    ST_UNI_COMP,
    ST_SB128,
    ST_REDUCED_STILL,
    ST_ERROR_RES,
    ST_TX64,
    ST_IDTX,
    ST_FLIPADST,
    ST_ADST,
    ST_TX_1D,
    ST_TEMPORAL_MV,
    ST_GM_FRAME,
    ST_INTERP_SMOOTH,
    ST_INTERP_SHARP,
    ST_BILINEAR,
    ST_SEG_PRED,
    ST_RECT_TX,
    ST_VARTX,
    ST_LAYER_DROP,
    ST_FRAME_REFS_SHORT,
    ST_COMPOUND,
    ST_KEY_FRAME,
    ST_INTER_FRAME,
    ST_CHROMA_SUBPRED,
    ST_II_DC,
    ST_II_V,
    ST_II_H,
    ST_DC_PRED_ST,
    ST_V_PRED_ST,
    ST_H_PRED_ST,
    ST_SMOOTH_V,
    ST_SMOOTH_H,
    ST_ADST4,
    ST_ADST8,
    ST_ADST16,
    ST_DCT64,
    ST_IDENT32,
    ST_PALETTE_CACHE,
    ST_SEG_FEATURE_Q,
    ST_SEG_FEATURE_LF,
    ST_SEG_FEATURE_REF,
    ST_SEG_FEATURE_SKIP,
    ST_SEG_FEATURE_GMV,
    ST_LR_SWITCHABLE,
    ST_SCREEN_CONTENT,
    ST_INTEGER_MV,
    ST_FRAME_ID,
    ST_ORDER_HINT_OFF,
    ST_JNT_COMP_FRAME,
    ST_MV_JOINT_HP,
    ST_COUNT
};

#ifdef FIZMO_AV1_STATS
extern std::uint64_t g_av1_stats[ST_COUNT];
extern const char* const g_av1_stat_names[ST_COUNT];
#define AV1_STAT(i) (++::fizmo::video::av1::g_av1_stats[(i)])
#else
#define AV1_STAT(i) ((void)0)
#endif // FIZMO_AV1_STATS

inline int clip3(int lo, int hi, int v) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
inline int imin(int a, int b) noexcept { return a < b ? a : b; }
inline int imax(int a, int b) noexcept { return a > b ? a : b; }
inline int iabs(int a) noexcept { return a < 0 ? -a : a; }
inline int round2(int x, int n) noexcept { return n == 0 ? x : ((x + (1 << (n - 1))) >> n); }
inline std::int64_t round2_64(std::int64_t x, int n) noexcept { return n == 0 ? x : ((x + (std::int64_t(1) << (n - 1))) >> n); }
inline int round2signed(int x, int n) noexcept { return x >= 0 ? round2(x, n) : -round2(-x, n); }
inline std::int64_t round2signed_64(std::int64_t x, int n) noexcept { return x >= 0 ? round2_64(x, n) : -round2_64(-x, n); }
inline int floor_log2(std::uint32_t x) noexcept {
    int s = 0;
    while (x > 1) {
        x >>= 1;
        ++s;
    }
    return s;
}
inline int ceil_log2(std::uint32_t x) noexcept {
    if (x < 2) return 0;
    int i = 1;
    std::uint32_t p = 2;
    while (p < x) {
        ++i;
        p <<= 1;
    }
    return i;
}
inline int block_width(int bs) noexcept { return 4 * Num_4x4_Blocks_Wide[bs]; }
inline int block_height(int bs) noexcept { return 4 * Num_4x4_Blocks_High[bs]; }

struct Mv {
    std::int16_t row = 0;
    std::int16_t col = 0;
    bool operator==(const Mv& o) const noexcept { return row == o.row && col == o.col; }
    bool operator!=(const Mv& o) const noexcept { return !(*this == o); }
};

inline Mv make_mv(int r, int c) noexcept {
    Mv m;
    m.row = static_cast<std::int16_t>(r);
    m.col = static_cast<std::int16_t>(c);
    return m;
}

class BitReader {
public:
    BitReader() = default;
    BitReader(const std::uint8_t* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    std::uint32_t f(int n) noexcept;
    int su(int n) noexcept;
    std::uint32_t ns(std::uint32_t n) noexcept;
    std::uint32_t uvlc() noexcept;
    std::uint64_t leb128() noexcept;
    std::uint32_t le(int n) noexcept;
    void byte_alignment() noexcept { m_pos = (m_pos + 7) & ~std::size_t(7); }
    std::size_t position() const noexcept { return m_pos; }
    void set_position(std::size_t p) noexcept { m_pos = p; }
    bool overrun() const noexcept { return m_pos > m_size * 8; }
    const std::uint8_t* data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_pos = 0;
};

class SymbolDecoder {
public:
    void init(const std::uint8_t* data, std::size_t size, bool disable_update) noexcept;
    int read_symbol(std::uint16_t* cdf, int n) noexcept;
    int read_bool() noexcept;
    int read_literal(int n) noexcept;
    int read_ns(int n) noexcept;
    bool exhausted() const noexcept { return m_max_bits < -14 * 8; }

private:
    std::uint32_t read_bits(int n) noexcept;
    void renormalize() noexcept;

    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_bitpos = 0;
    std::uint32_t m_value = 0;
    std::uint32_t m_range = 0;
    std::int64_t m_max_bits = 0;
    bool m_disable_update = false;
};

struct CdfContext {
    std::uint16_t YMode[BLOCK_SIZE_GROUPS][INTRA_MODES + 1];
    std::uint16_t UVModeCflNotAllowed[INTRA_MODES][UV_INTRA_MODES_CFL_NOT_ALLOWED + 1];
    std::uint16_t UVModeCflAllowed[INTRA_MODES][UV_INTRA_MODES_CFL_ALLOWED + 1];
    std::uint16_t AngleDelta[DIRECTIONAL_MODES][(2 * MAX_ANGLE_DELTA + 1) + 1];
    std::uint16_t Intrabc[3];
    std::uint16_t PartitionW8[PARTITION_CONTEXTS][5];
    std::uint16_t PartitionW16[PARTITION_CONTEXTS][11];
    std::uint16_t PartitionW32[PARTITION_CONTEXTS][11];
    std::uint16_t PartitionW64[PARTITION_CONTEXTS][11];
    std::uint16_t PartitionW128[PARTITION_CONTEXTS][9];
    std::uint16_t SegmentId[SEGMENT_ID_CONTEXTS][MAX_SEGMENTS + 1];
    std::uint16_t SegmentIdPredicted[SEGMENT_ID_PREDICTED_CONTEXTS][3];
    std::uint16_t Tx8x8[TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 1];
    std::uint16_t Tx16x16[TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 2];
    std::uint16_t Tx32x32[TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 2];
    std::uint16_t Tx64x64[TX_SIZE_CONTEXTS][MAX_TX_DEPTH + 2];
    std::uint16_t TxfmSplit[TXFM_PARTITION_CONTEXTS][3];
    std::uint16_t FilterIntraMode[6];
    std::uint16_t FilterIntra[BLOCK_SIZES][3];
    std::uint16_t InterpFilter[INTERP_FILTER_CONTEXTS][INTERP_FILTERS + 1];
    std::uint16_t MotionMode[BLOCK_SIZES][MOTION_MODES + 1];
    std::uint16_t NewMv[NEW_MV_CONTEXTS][3];
    std::uint16_t ZeroMv[ZERO_MV_CONTEXTS][3];
    std::uint16_t RefMv[REF_MV_CONTEXTS][3];
    std::uint16_t CompoundMode[COMPOUND_MODE_CONTEXTS][COMPOUND_MODES + 1];
    std::uint16_t DrlMode[DRL_MODE_CONTEXTS][3];
    std::uint16_t IsInter[IS_INTER_CONTEXTS][3];
    std::uint16_t CompMode[COMP_INTER_CONTEXTS][3];
    std::uint16_t SkipMode[SKIP_MODE_CONTEXTS][3];
    std::uint16_t Skip[SKIP_CONTEXTS][3];
    std::uint16_t CompRef[REF_CONTEXTS][FWD_REFS - 1][3];
    std::uint16_t CompBwdRef[REF_CONTEXTS][BWD_REFS - 1][3];
    std::uint16_t SingleRef[REF_CONTEXTS][SINGLE_REFS - 1][3];
    std::uint16_t MvJoint[MV_CONTEXTS][MV_JOINTS + 1];
    std::uint16_t MvClass[MV_CONTEXTS][2][MV_CLASSES + 1];
    std::uint16_t MvClass0Bit[MV_CONTEXTS][2][3];
    std::uint16_t MvFr[MV_CONTEXTS][2][MV_JOINTS + 1];
    std::uint16_t MvClass0Fr[MV_CONTEXTS][2][CLASS0_SIZE][MV_JOINTS + 1];
    std::uint16_t MvClass0Hp[MV_CONTEXTS][2][3];
    std::uint16_t MvSign[MV_CONTEXTS][2][3];
    std::uint16_t MvBit[MV_CONTEXTS][2][MV_OFFSET_BITS][3];
    std::uint16_t MvHp[MV_CONTEXTS][2][3];
    std::uint16_t PaletteYMode[PALETTE_BLOCK_SIZE_CONTEXTS][PALETTE_Y_MODE_CONTEXTS][3];
    std::uint16_t PaletteUVMode[PALETTE_UV_MODE_CONTEXTS][3];
    std::uint16_t PaletteYSize[PALETTE_BLOCK_SIZE_CONTEXTS][PALETTE_SIZES + 1];
    std::uint16_t PaletteUVSize[PALETTE_BLOCK_SIZE_CONTEXTS][PALETTE_SIZES + 1];
    std::uint16_t PaletteYColor[PALETTE_SIZES][PALETTE_COLOR_CONTEXTS][PALETTE_COLORS + 1];
    std::uint16_t PaletteUVColor[PALETTE_SIZES][PALETTE_COLOR_CONTEXTS][PALETTE_COLORS + 1];
    std::uint16_t DeltaQ[DELTA_Q_SMALL + 2];
    std::uint16_t DeltaLF[DELTA_LF_SMALL + 2];
    std::uint16_t DeltaLFMulti[FRAME_LF_COUNT][DELTA_LF_SMALL + 2];
    std::uint16_t IntraTxTypeSet1[2][INTRA_MODES][8];
    std::uint16_t IntraTxTypeSet2[3][INTRA_MODES][6];
    std::uint16_t InterTxTypeSet1[2][17];
    std::uint16_t InterTxTypeSet2[13];
    std::uint16_t InterTxTypeSet3[4][3];
    std::uint16_t UseObmc[BLOCK_SIZES][3];
    std::uint16_t InterIntra[BLOCK_SIZE_GROUPS - 1][3];
    std::uint16_t CompRefType[COMP_REF_TYPE_CONTEXTS][3];
    std::uint16_t CflSign[CFL_JOINT_SIGNS + 1];
    std::uint16_t UniCompRef[REF_CONTEXTS][UNIDIR_COMP_REFS - 1][3];
    std::uint16_t WedgeInterIntra[BLOCK_SIZES][3];
    std::uint16_t CompGroupIdx[COMP_GROUP_IDX_CONTEXTS][3];
    std::uint16_t CompoundIdx[COMPOUND_IDX_CONTEXTS][3];
    std::uint16_t CompoundType[BLOCK_SIZES][COMPOUND_TYPES + 1];
    std::uint16_t InterIntraMode[BLOCK_SIZE_GROUPS - 1][INTERINTRA_MODES + 1];
    std::uint16_t WedgeIndex[BLOCK_SIZES][16 + 1];
    std::uint16_t CflAlpha[CFL_ALPHA_CONTEXTS][CFL_ALPHABET_SIZE + 1];
    std::uint16_t UseWiener[3];
    std::uint16_t UseSgrproj[3];
    std::uint16_t RestorationType[RESTORE_SWITCHABLE + 1];
    std::uint16_t TxbSkip[TX_SIZES][TXB_SKIP_CONTEXTS][3];
    std::uint16_t EobPt16[PLANE_TYPES][2][6];
    std::uint16_t EobPt32[PLANE_TYPES][2][7];
    std::uint16_t EobPt64[PLANE_TYPES][2][8];
    std::uint16_t EobPt128[PLANE_TYPES][2][9];
    std::uint16_t EobPt256[PLANE_TYPES][2][10];
    std::uint16_t EobPt512[PLANE_TYPES][11];
    std::uint16_t EobPt1024[PLANE_TYPES][12];
    std::uint16_t EobExtra[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][3];
    std::uint16_t DcSign[PLANE_TYPES][DC_SIGN_CONTEXTS][3];
    std::uint16_t CoeffBaseEob[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB][4];
    std::uint16_t CoeffBase[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS][5];
    std::uint16_t CoeffBr[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS][BR_CDF_SIZE + 1];

    void init_non_coeff();
    void init_coeff(int base_q_idx);
    void reset_counters();
};

struct SequenceHeader {
    int seq_profile = 0;
    int still_picture = 0;
    int reduced_still_picture_header = 0;
    int timing_info_present_flag = 0;
    std::uint32_t num_units_in_display_tick = 0;
    std::uint32_t time_scale = 0;
    int equal_picture_interval = 0;
    std::uint32_t num_ticks_per_picture_minus_1 = 0;
    int decoder_model_info_present_flag = 0;
    int buffer_delay_length_minus_1 = 0;
    int buffer_removal_time_length_minus_1 = 0;
    int frame_presentation_time_length_minus_1 = 0;
    int initial_display_delay_present_flag = 0;
    int operating_points_cnt_minus_1 = 0;
    int operating_point_idc[32] = {};
    int seq_level_idx[32] = {};
    int seq_tier[32] = {};
    int decoder_model_present_for_this_op[32] = {};
    int OperatingPointIdc = 0;
    int frame_width_bits_minus_1 = 0;
    int frame_height_bits_minus_1 = 0;
    int max_frame_width_minus_1 = 0;
    int max_frame_height_minus_1 = 0;
    int frame_id_numbers_present_flag = 0;
    int delta_frame_id_length_minus_2 = 0;
    int additional_frame_id_length_minus_1 = 0;
    int use_128x128_superblock = 0;
    int enable_filter_intra = 0;
    int enable_intra_edge_filter = 0;
    int enable_interintra_compound = 0;
    int enable_masked_compound = 0;
    int enable_warped_motion = 0;
    int enable_dual_filter = 0;
    int enable_order_hint = 0;
    int enable_jnt_comp = 0;
    int enable_ref_frame_mvs = 0;
    int seq_force_screen_content_tools = 0;
    int seq_force_integer_mv = 0;
    int OrderHintBits = 0;
    int enable_superres = 0;
    int enable_cdef = 0;
    int enable_restoration = 0;
    int BitDepth = 8;
    int mono_chrome = 0;
    int NumPlanes = 3;
    int color_description_present_flag = 0;
    int color_primaries = 2;
    int transfer_characteristics = 2;
    int matrix_coefficients = 2;
    int color_range = 0;
    int subsampling_x = 1;
    int subsampling_y = 1;
    int chroma_sample_position = 0;
    int separate_uv_delta_q = 0;
    int film_grain_params_present = 0;
};

struct FilmGrainParams {
    int apply_grain = 0;
    int grain_seed = 0;
    int update_grain = 0;
    int num_y_points = 0;
    int point_y_value[16] = {};
    int point_y_scaling[16] = {};
    int chroma_scaling_from_luma = 0;
    int num_cb_points = 0;
    int point_cb_value[16] = {};
    int point_cb_scaling[16] = {};
    int num_cr_points = 0;
    int point_cr_value[16] = {};
    int point_cr_scaling[16] = {};
    int grain_scaling_minus_8 = 0;
    int ar_coeff_lag = 0;
    int ar_coeffs_y_plus_128[24] = {};
    int ar_coeffs_cb_plus_128[25] = {};
    int ar_coeffs_cr_plus_128[25] = {};
    int ar_coeff_shift_minus_6 = 0;
    int grain_scale_shift = 0;
    int cb_mult = 0;
    int cb_luma_mult = 0;
    int cb_offset = 0;
    int cr_mult = 0;
    int cr_luma_mult = 0;
    int cr_offset = 0;
    int overlap_flag = 0;
    int clip_to_restricted_range = 0;
};

struct Plane {
    std::vector<std::uint16_t> buf;
    int stride = 0;
    int width = 0;
    int height = 0;
    std::uint16_t* row(int y) noexcept { return buf.data() + static_cast<std::ptrdiff_t>(y) * stride; }
    const std::uint16_t* row(int y) const noexcept { return buf.data() + static_cast<std::ptrdiff_t>(y) * stride; }
    void alloc(int w, int h);
};

struct FrameBuffer {
    Plane planes[3];
    int num_planes = 0;
    int subx = 0;
    int suby = 0;
    int width = 0;
    int height = 0;
    void alloc(int w, int h, int np, int sx, int sy);
};

struct SavedFrame {
    FrameBuffer frame;
    int frame_id = 0;
    int upscaled_width = 0;
    int frame_width = 0;
    int frame_height = 0;
    int render_width = 0;
    int render_height = 0;
    int mi_cols = 0;
    int mi_rows = 0;
    int frame_type = 0;
    int subsampling_x = 0;
    int subsampling_y = 0;
    int bit_depth = 8;
    int order_hint = 0;
    int saved_order_hints[8] = {};
    int showable_frame = 0;
    int show_frame = 0;
    std::vector<std::int8_t> mf_ref_frames;
    std::vector<Mv> mf_mvs;
    int gm_params[8][6] = {};
    std::vector<std::uint8_t> segment_ids;
    CdfContext cdfs;
    FilmGrainParams grain;
    int loop_filter_ref_deltas[8] = {};
    int loop_filter_mode_deltas[2] = {};
    int feature_enabled[8][8] = {};
    int feature_data[8][8] = {};
    int temporal_id = 0;
    int spatial_id = 0;
};

struct FrameHeader {
    int show_existing_frame = 0;
    int frame_to_show_map_idx = 0;
    int frame_type = 0;
    int FrameIsIntra = 0;
    int show_frame = 0;
    int showable_frame = 0;
    int error_resilient_mode = 0;
    int disable_cdf_update = 0;
    int allow_screen_content_tools = 0;
    int force_integer_mv = 0;
    int current_frame_id = 0;
    int frame_size_override_flag = 0;
    int OrderHint = 0;
    int primary_ref_frame = PRIMARY_REF_NONE;
    int refresh_frame_flags = 0;
    int ref_order_hint[8] = {};
    int allow_intrabc = 0;
    int frame_refs_short_signaling = 0;
    int last_frame_idx = 0;
    int gold_frame_idx = 0;
    int ref_frame_idx[7] = {};
    int allow_high_precision_mv = 0;
    int is_filter_switchable = 0;
    int interpolation_filter = 0;
    int is_motion_mode_switchable = 0;
    int use_ref_frame_mvs = 0;
    int OrderHints[8] = {};
    int RefFrameSignBias[8] = {};
    int disable_frame_end_update_cdf = 0;
    int FrameWidth = 0;
    int FrameHeight = 0;
    int UpscaledWidth = 0;
    int RenderWidth = 0;
    int RenderHeight = 0;
    int use_superres = 0;
    int SuperresDenom = SUPERRES_NUM;
    int MiCols = 0;
    int MiRows = 0;
    int TileColsLog2 = 0;
    int TileRowsLog2 = 0;
    int TileCols = 0;
    int TileRows = 0;
    int MiColStarts[MAX_TILE_COLS + 1] = {};
    int MiRowStarts[MAX_TILE_ROWS + 1] = {};
    int context_update_tile_id = 0;
    int TileSizeBytes = 4;
    int base_q_idx = 0;
    int DeltaQYDc = 0;
    int DeltaQUDc = 0;
    int DeltaQUAc = 0;
    int DeltaQVDc = 0;
    int DeltaQVAc = 0;
    int using_qmatrix = 0;
    int qm_y = 0;
    int qm_u = 0;
    int qm_v = 0;
    int segmentation_enabled = 0;
    int segmentation_update_map = 0;
    int segmentation_temporal_update = 0;
    int segmentation_update_data = 0;
    int FeatureEnabled[MAX_SEGMENTS][SEG_LVL_MAX] = {};
    int FeatureData[MAX_SEGMENTS][SEG_LVL_MAX] = {};
    int SegIdPreSkip = 0;
    int LastActiveSegId = 0;
    int delta_q_present = 0;
    int delta_q_res = 0;
    int delta_lf_present = 0;
    int delta_lf_res = 0;
    int delta_lf_multi = 0;
    int CodedLossless = 0;
    int AllLossless = 0;
    int LosslessArray[MAX_SEGMENTS] = {};
    int SegQMLevel[3][MAX_SEGMENTS] = {};
    int loop_filter_level[4] = {};
    int loop_filter_sharpness = 0;
    int loop_filter_delta_enabled = 0;
    int loop_filter_delta_update = 0;
    int loop_filter_ref_deltas[TOTAL_REFS_PER_FRAME] = {};
    int loop_filter_mode_deltas[2] = {};
    int cdef_damping = 3;
    int cdef_bits = 0;
    int cdef_y_pri_strength[8] = {};
    int cdef_y_sec_strength[8] = {};
    int cdef_uv_pri_strength[8] = {};
    int cdef_uv_sec_strength[8] = {};
    int FrameRestorationType[3] = {};
    int UsesLr = 0;
    int LoopRestorationSize[3] = {};
    int TxMode = 0;
    int reference_select = 0;
    int skip_mode_present = 0;
    int SkipModeFrame[2] = {};
    int allow_warped_motion = 0;
    int reduced_tx_set = 0;
    int GmType[8] = {};
    int gm_params[8][6] = {};
    int PrevGmParams[8][6] = {};
    FilmGrainParams film_grain;
    int temporal_id = 0;
    int spatial_id = 0;
};

struct MiInfo {
    std::int8_t ref_frame[2];
    Mv mv[2];
    std::uint8_t mi_size;
    std::uint8_t y_mode;
    std::uint8_t is_inter;
    std::uint8_t skip;
    std::uint8_t skip_mode;
    std::uint8_t segment_id;
    std::uint8_t tx_size;
    std::uint8_t inter_tx_size;
    std::uint8_t palette_size[2];
    std::uint8_t comp_group_idx;
    std::uint8_t compound_idx;
    std::uint8_t interp_filter[2];
    std::uint8_t tx_type;
    std::uint8_t written;
    std::int8_t delta_lf[4];
    std::uint8_t motion_mode;
    std::uint8_t uv_mode;
    std::uint8_t pad[2];
};

struct LrUnit {
    std::uint8_t type = 0;
    std::uint8_t sgr_set = 0;
    std::int8_t wiener[2][3] = {};
    std::int8_t sgr_xqd[2] = {};
};

class ThreadPool {
public:
    explicit ThreadPool(int threads);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    void run(int count, const std::function<void(int)>& fn);
    int size() const noexcept { return static_cast<int>(m_threads.size()) + 1; }

private:
    void worker();
    bool take(int* index);

    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_done;
    const std::function<void(int)>* m_job = nullptr;
    int m_next = 0;
    int m_count = 0;
    int m_finished = 0;
    std::uint64_t m_generation = 0;
    bool m_stop = false;
};

struct Decoder;

struct WarpParams {
    int m[6];
    bool valid;
};

struct Tile {
    Decoder* d = nullptr;
    const SequenceHeader* seq = nullptr;
    const FrameHeader* fh = nullptr;
    SymbolDecoder sd;
    CdfContext cdf;
    std::uint16_t intra_frame_y_mode_cdf[INTRA_MODE_CONTEXTS][INTRA_MODE_CONTEXTS][INTRA_MODES + 1];

    int MiRowStart = 0;
    int MiRowEnd = 0;
    int MiColStart = 0;
    int MiColEnd = 0;
    int CurrentQIndex = 0;
    int DeltaLF[FRAME_LF_COUNT] = {};
    int RefSgrXqd[3][2] = {};
    int RefLrWiener[3][2][3] = {};
    int ReadDeltas = 0;
    bool error = false;

    std::vector<std::uint8_t> AboveLevelContext[3];
    std::vector<std::uint8_t> AboveDcContext[3];
    std::vector<std::uint8_t> AboveSegPredContext;
    std::vector<std::uint8_t> LeftLevelContext[3];
    std::vector<std::uint8_t> LeftDcContext[3];
    std::vector<std::uint8_t> LeftSegPredContext;
    std::vector<std::uint16_t> AbovePaletteColors[2];
    std::vector<std::uint16_t> LeftPaletteColors[2];

    std::uint8_t BlockDecodedBuf[3][34][34];

    int MiRow = 0;
    int MiCol = 0;
    int MiSize = 0;
    int HasChroma = 0;
    int AvailU = 0;
    int AvailL = 0;
    int AvailUChroma = 0;
    int AvailLChroma = 0;
    int skip = 0;
    int skip_mode = 0;
    int segment_id = 0;
    int is_inter = 0;
    int use_intrabc = 0;
    int Lossless = 0;
    int YMode = 0;
    int UVMode = 0;
    int AngleDeltaY = 0;
    int AngleDeltaUV = 0;
    int CflAlphaU = 0;
    int CflAlphaV = 0;
    int use_filter_intra = 0;
    int filter_intra_mode = 0;
    int PaletteSizeY = 0;
    int PaletteSizeUV = 0;
    std::uint16_t palette_colors_y[8] = {};
    std::uint16_t palette_colors_u[8] = {};
    std::uint16_t palette_colors_v[8] = {};
    std::uint8_t ColorMapY[64][64];
    std::uint8_t ColorMapUV[64][64];
    int RefFrame[2] = {0, -1};
    Mv MvVal[2];
    Mv PredMv[2];
    int interp_filter[2] = {};
    int motion_mode = 0;
    int interintra = 0;
    int interintra_mode = 0;
    int wedge_interintra = 0;
    int wedge_index = 0;
    int wedge_sign = 0;
    int mask_type = 0;
    int compound_type = COMPOUND_AVERAGE;
    int comp_group_idx = 0;
    int compound_idx = 0;
    int TxSize = 0;
    int MaxLumaW = 0;
    int MaxLumaH = 0;
    int LeftRefFrame[2] = {};
    int AboveRefFrame[2] = {};
    int LeftIntra = 0;
    int AboveIntra = 0;
    int LeftSingle = 0;
    int AboveSingle = 0;

    int NumMvFound = 0;
    int NewMvCount = 0;
    Mv RefStackMv[MAX_REF_MV_STACK_SIZE + 1][2];
    int WeightStack[MAX_REF_MV_STACK_SIZE + 1] = {};
    Mv GlobalMvs[2];
    int DrlCtxStack[MAX_REF_MV_STACK_SIZE + 1] = {};
    int FoundMatch = 0;
    int CloseMatches = 0;
    int TotalMatches = 0;
    int NewMvContext = 0;
    int RefMvContext = 0;
    int ZeroMvContext = 0;
    int RefMvIdx = 0;
    Mv RefIdMvs[2][2];
    Mv RefDiffMvs[2][2];
    int RefIdCount[2] = {};
    int RefDiffCount[2] = {};
    int NumSamples = 0;
    int NumSamplesScanned = 0;
    int CandList[LEAST_SQUARES_SAMPLES_MAX][4] = {};
    WarpParams LocalWarp = {};

    std::int32_t Quant[1024];
    int PlaneTxType = 0;
    std::int32_t Dequant[64 * 64];
    std::int32_t Residual[64 * 64];

    std::vector<std::uint16_t> scratch16;
    std::vector<std::int16_t> scratch_i16;
    std::vector<std::int32_t> scratch32;
    std::vector<std::int32_t> preds[2];
    std::vector<std::uint8_t> mask;
    std::vector<std::uint16_t> obmc_buf;
    std::vector<std::uint16_t> ii_buf;
    std::vector<std::uint16_t> edge_buf;

    std::uint8_t& block_decoded(int plane, int y, int x) noexcept { return BlockDecodedBuf[plane][y + 1][x + 1]; }

    void init(Decoder* dec, int tile_row, int tile_col);
    bool decode(const std::uint8_t* data, std::size_t size);
    void clear_block_decoded_flags(int r, int c, int sbSize4);
    void read_lr(int r, int c, int bSize);
    void read_lr_unit(int plane, int unitRow, int unitCol);
    int decode_signed_subexp_with_ref_bool(int low, int high, int k, int r);
    int decode_unsigned_subexp_with_ref_bool(int mx, int k, int r);
    int decode_subexp_bool(int numSyms, int k);
    void decode_partition(int r, int c, int bSize);
    void decode_block(int r, int c, int subSize);
    bool is_inside(int r, int c) const noexcept { return c >= MiColStart && c < MiColEnd && r >= MiRowStart && r < MiRowEnd; }
    MiInfo& mi(int r, int c) noexcept;
    const MiInfo& mi(int r, int c) const noexcept;

    void intra_frame_mode_info();
    void inter_frame_mode_info();
    void intra_segment_id();
    void read_segment_id();
    void inter_segment_id(int preSkip);
    int get_segment_id();
    void read_skip_mode();
    void read_skip();
    void read_cdef();
    void read_delta_qindex();
    void read_delta_lf();
    void read_tx_size(int allowSelect);
    void read_block_tx_size();
    void read_var_tx_size(int row, int col, int txSz, int depth);
    void read_is_inter();
    void intra_block_mode_info();
    void inter_block_mode_info();
    void filter_intra_mode_info();
    void read_ref_frames();
    void assign_mv(int isCompound);
    void read_mv(int ref);
    int read_mv_component(int ctx, int comp);
    void read_motion_mode(int isCompound);
    void read_interintra_mode(int isCompound);
    void read_compound_type(int isCompound);
    int get_mode(int refList) const noexcept;
    void intra_angle_info_y();
    void intra_angle_info_uv();
    void read_cfl_alphas();
    void palette_mode_info();
    int get_palette_cache(int plane, std::uint16_t* cache);
    void palette_tokens();
    void get_palette_color_context(std::uint8_t (*colorMap)[64], int r, int c, int n, int* order, int* ctx);
    int seg_feature_active(int feature) const noexcept;
    int get_qindex(int ignoreDeltaQ, int segmentId) const noexcept;
    int count_refs(int frameType) const noexcept;
    int needs_interp_filter() const noexcept;
    void reset_block_context(int bw4, int bh4);
    int get_tx_set(int txSz) const noexcept;
    int compute_tx_type(int plane, int txSz, int blockX, int blockY);
    void transform_type(int x4, int y4, int txSz);
    int coeffs(int plane, int startX, int startY, int txSz);
    void residual();
    void transform_block(int plane, int baseX, int baseY, int txSz, int x, int y);
    void transform_tree(int startX, int startY, int w, int h);
    int get_tx_size(int plane, int txSz) const noexcept;
    int get_plane_residual_size(int subsize, int plane) const noexcept;
    int get_above_tx_width(int row, int col);
    int get_left_tx_height(int row, int col);

    void find_mv_stack(int isCompound);
    Mv setup_global_mv(int refList);
    void scan_row(int deltaRow, int isCompound);
    void scan_col(int deltaCol, int isCompound);
    void scan_point(int deltaRow, int deltaCol, int isCompound);
    void temporal_scan(int isCompound);
    void add_tpl_ref_mv(int deltaRow, int deltaCol, int isCompound);
    void add_ref_mv_candidate(int mvRow, int mvCol, int isCompound, int weight);
    void search_stack(int mvRow, int mvCol, int candList, int weight);
    void compound_search_stack(int mvRow, int mvCol, int weight);
    void lower_mv_precision(Mv& mv) const noexcept;
    void sort_stack(int start, int end, int isCompound);
    void extra_search(int isCompound);
    void add_extra_mv_candidate(int mvRow, int mvCol, int isCompound);
    void context_and_clamping(int isCompound, int numNew);
    int clamp_mv_row(int mvec, int border) const noexcept;
    int clamp_mv_col(int mvec, int border) const noexcept;
    int has_overlappable_candidates();
    void find_warp_samples();
    void add_sample(int deltaRow, int deltaCol);
    bool is_scaled(int refFrame) const noexcept;

    void compute_prediction();
    void predict_intra(int plane, int x, int y, int haveLeft, int haveAbove, int haveAboveRight, int haveBelowLeft, int mode, int log2W, int log2H);
    void predict_palette(int plane, int startX, int startY, int x, int y, int txSz);
    void predict_chroma_from_luma(int plane, int startX, int startY, int txSz);
    void predict_inter(int plane, int x, int y, int w, int h, int candRow, int candCol);
    void reconstruct(int plane, int x, int y, int txSz);
    void overlapped_motion_compensation(int plane, int w, int h);
    bool warp_estimation(WarpParams& wp);
};

struct Decoder {
    SequenceHeader seq;
    bool have_seq = false;
    FrameHeader fh;
    int SeenFrameHeader = 0;
    int TileNum = 0;
    bool frame_in_progress = false;
    bool frame_header_valid = false;
    int temporal_id = 0;
    int spatial_id = 0;

    std::shared_ptr<SavedFrame> refs[NUM_REF_FRAMES];
    int RefValid[NUM_REF_FRAMES] = {};
    int RefFrameId[NUM_REF_FRAMES] = {};
    int RefOrderHint[NUM_REF_FRAMES] = {};
    int RefFrameType[NUM_REF_FRAMES] = {};

    CdfContext cdf;
    CdfContext saved_cdf;
    bool saved_cdf_valid = false;

    std::shared_ptr<SavedFrame> cur;
    FrameBuffer frame;
    std::vector<MiInfo> mi;
    int mi_stride = 0;
    std::vector<std::uint8_t> lf_tx_size[3];
    std::vector<std::int8_t> cdef_idx;
    int cdef_stride = 0;
    std::vector<LrUnit> lr_units[3];
    int lr_unit_rows[3] = {};
    int lr_unit_cols[3] = {};
    std::vector<std::uint8_t> prev_segment_ids;
    std::vector<Mv> motion_field_mvs[8];
    std::vector<std::int16_t> dequant_cache;
    int tile_errors = 0;
    std::uint8_t mi_stamp = 1;

    FrameBuffer cdef_frame;
    FrameBuffer upscaled_cur;
    FrameBuffer upscaled_cdef;
    FrameBuffer lr_frame;
    const FrameBuffer* lr_cdef_src = nullptr;
    const FrameBuffer* lr_cur_src = nullptr;

    Av1Options options;
    std::unique_ptr<ThreadPool> pool;
    std::vector<std::unique_ptr<Tile>> tile_pool;
    int thread_count() ;
    void parallel(int count, const std::function<void(int)>& fn);
    void loop_filter_rows(int plane, int pass, int rowBegin, int rowEnd);
    void cdef_rows(int fbrBegin, int fbrEnd);
    void upscale_rows(const FrameBuffer& in, FrameBuffer& out, int plane, int yBegin, int yEnd);
    void lr_stripe(int plane, int stripeNum, std::vector<std::int32_t>& tmp, std::vector<std::int32_t>& AB);
    std::uint64_t frames_decoded = 0;
    std::uint64_t errors = 0;
    std::deque<VideoFrame> output;
    std::vector<VideoFrame> tu_frames;
    double current_pts = 0.0;
    Av1Info info;
    bool opened = false;

    int get_relative_dist(int a, int b) const noexcept;
    bool decode_temporal_unit(const std::uint8_t* data, std::size_t size, std::string* error);
    bool disposable(const std::uint8_t* data, std::size_t size) const noexcept;
    bool decode_obu(const std::uint8_t* data, std::size_t size, std::size_t* consumed, std::string* error);
    bool parse_sequence_header(BitReader& br, std::string* error);
    bool parse_color_config(BitReader& br);
    bool frame_header_obu(BitReader& br, std::string* error);
    bool uncompressed_header(BitReader& br, std::string* error);
    void mark_ref_frames(int idLen);
    bool frame_size(BitReader& br);
    void superres_params(BitReader& br);
    void compute_image_size();
    void render_size(BitReader& br);
    bool frame_size_with_refs(BitReader& br);
    void set_frame_refs();
    bool tile_info(BitReader& br);
    void quantization_params(BitReader& br);
    void segmentation_params(BitReader& br);
    void delta_q_params(BitReader& br);
    void delta_lf_params(BitReader& br);
    void loop_filter_params(BitReader& br);
    void cdef_params(BitReader& br);
    void lr_params(BitReader& br);
    void read_tx_mode(BitReader& br);
    void skip_mode_params(BitReader& br);
    void global_motion_params(BitReader& br);
    void read_global_param(BitReader& br, int type, int ref, int idx);
    int decode_signed_subexp_with_ref(BitReader& br, int low, int high, int r);
    int decode_unsigned_subexp_with_ref(BitReader& br, int mx, int r);
    int decode_subexp(BitReader& br, int numSyms);
    void film_grain_params(BitReader& br);
    void load_grain_params(int idx);
    void setup_past_independence();
    void load_previous();
    void load_previous_segment_ids();
    int get_qindex(int ignoreDeltaQ, int segmentId, int currentQIndex) const noexcept;
    bool tile_group_obu(BitReader& br, const std::uint8_t* data, std::size_t size, std::string* error);
    bool setup_frame();
    void motion_field_estimation();
    bool projection(int src, int dstSign);
    void decode_frame_wrapup();
    void show_existing_frame();
    void reference_frame_update();
    void output_frame(const SavedFrame& f, const FilmGrainParams& grain);
    void loop_filter_frame();
    void cdef_frame_process();
    void upscale(const FrameBuffer& in, FrameBuffer& out);
    void loop_restoration();
    void reset();
    bool refs_size_changed() const noexcept;
    std::shared_ptr<SavedFrame> ref_for(int refFrame) const { return refs[fh.ref_frame_idx[refFrame - LAST_FRAME]]; }
};

void add_film_grain(const FilmGrainParams& p, int bit_depth, int mono, int subx, int suby, int matrix_coefficients, const FrameBuffer& in, FrameBuffer& out);
void fast_idct4(std::int32_t* T, int r);
void fast_idct8(std::int32_t* T, int r);
void fast_idct16(std::int32_t* T, int r);
void fast_idct32(std::int32_t* T, int r);
void fast_idct64(std::int32_t* T, int r);
void fast_iadst8(std::int32_t* T, int r);
void fast_iadst16(std::int32_t* T, int r);
void inverse_transform_2d(std::int32_t* coeffs, int txSz, int txType, int lossless, int bitDepth, int eobRowBound, int eobColBound, std::int32_t* residual);

} // namespace av1
} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_AV1_INTERNAL_HPP
