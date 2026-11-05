#include "av1_internal.hpp"

namespace fizmo {
namespace video {
namespace av1 {

std::uint32_t BitReader::f(int n) noexcept {
    std::uint32_t x = 0;
    for (int i = 0; i < n; ++i) {
        std::uint32_t bit = 0;
        const std::size_t byte = m_pos >> 3;
        if (byte < m_size) bit = (m_data[byte] >> (7 - (m_pos & 7))) & 1u;
        ++m_pos;
        x = (x << 1) | bit;
    }
    return x;
}

int BitReader::su(int n) noexcept {
    int value = static_cast<int>(f(n));
    const int signMask = 1 << (n - 1);
    if (value & signMask) value -= 2 * signMask;
    return value;
}

std::uint32_t BitReader::ns(std::uint32_t n) noexcept {
    if (n <= 1) return 0;
    const int w = floor_log2(n) + 1;
    const std::uint32_t m = (1u << w) - n;
    const std::uint32_t v = f(w - 1);
    if (v < m) return v;
    const std::uint32_t extra_bit = f(1);
    return (v << 1) - m + extra_bit;
}

std::uint32_t BitReader::uvlc() noexcept {
    int leadingZeros = 0;
    while (true) {
        if (overrun()) return 0;
        const std::uint32_t done = f(1);
        if (done) break;
        ++leadingZeros;
    }
    if (leadingZeros >= 32) return 0xFFFFFFFFu;
    const std::uint32_t value = f(leadingZeros);
    return value + ((1u << leadingZeros) - 1u);
}

std::uint64_t BitReader::leb128() noexcept {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        const std::uint32_t b = f(8);
        value |= static_cast<std::uint64_t>(b & 0x7f) << (i * 7);
        if (!(b & 0x80)) break;
    }
    return value;
}

std::uint32_t BitReader::le(int n) noexcept {
    std::uint32_t t = 0;
    for (int i = 0; i < n; ++i) t += f(8) << (i * 8);
    return t;
}

void SymbolDecoder::init(const std::uint8_t* data, std::size_t size, bool disable_update) noexcept {
    m_data = data;
    m_size = size;
    m_bitpos = 0;
    m_disable_update = disable_update;
    const int numBits = static_cast<int>(size * 8 < 15 ? size * 8 : 15);
    const std::uint32_t buf = read_bits(numBits);
    const std::uint32_t paddedBuf = buf << (15 - numBits);
    m_value = ((1u << 15) - 1u) ^ paddedBuf;
    m_range = 1u << 15;
    m_max_bits = static_cast<std::int64_t>(size) * 8 - 15;
}

std::uint32_t SymbolDecoder::read_bits(int n) noexcept {
    if (n <= 0) return 0;
    const std::size_t byte = m_bitpos >> 3;
    std::uint32_t w = 0;
    for (int i = 0; i < 4; ++i) {
        w <<= 8;
        if (byte + i < m_size) w |= m_data[byte + i];
    }
    w <<= (m_bitpos & 7);
    m_bitpos += static_cast<std::size_t>(n);
    return w >> (32 - n);
}

int SymbolDecoder::read_symbol(std::uint16_t* cdf, int n) noexcept {
    std::uint32_t cur = m_range;
    std::uint32_t prev;
    int symbol = -1;
    do {
        ++symbol;
        prev = cur;
        const std::uint32_t f = (1u << 15) - cdf[symbol];
        cur = ((m_range >> 8) * (f >> EC_PROB_SHIFT)) >> (7 - EC_PROB_SHIFT);
        cur += EC_MIN_PROB * static_cast<std::uint32_t>(n - symbol - 1);
    } while (m_value < cur);
    m_range = prev - cur;
    m_value -= cur;
    const int bits = 15 - floor_log2(m_range);
    if (bits > 0) {
        m_range <<= bits;
        std::int64_t avail = m_max_bits > 0 ? m_max_bits : 0;
        const int numBits = bits < avail ? bits : static_cast<int>(avail);
        const std::uint32_t newData = read_bits(numBits);
        const std::uint32_t paddedData = newData << (bits - numBits);
        m_value = paddedData ^ (((m_value + 1u) << bits) - 1u);
        m_max_bits -= bits;
    }
    if (!m_disable_update) {
        const int rate = 3 + (cdf[n] > 15) + (cdf[n] > 31) + imin(floor_log2(static_cast<std::uint32_t>(n)), 2);
        std::uint32_t tmp = 0;
        for (int i = 0; i < n - 1; ++i) {
            if (i == symbol) tmp = 1u << 15;
            if (tmp < cdf[i]) {
                cdf[i] = static_cast<std::uint16_t>(cdf[i] - ((cdf[i] - tmp) >> rate));
            } else {
                cdf[i] = static_cast<std::uint16_t>(cdf[i] + ((tmp - cdf[i]) >> rate));
            }
        }
        if (cdf[n] < 32) ++cdf[n];
    }
    return symbol;
}

int SymbolDecoder::read_bool() noexcept {
    std::uint16_t cdf[3] = {1u << 14, 1u << 15, 0};
    const bool saved = m_disable_update;
    m_disable_update = true;
    const int v = read_symbol(cdf, 2);
    m_disable_update = saved;
    return v;
}

int SymbolDecoder::read_literal(int n) noexcept {
    int x = 0;
    for (int i = 0; i < n; ++i) x = 2 * x + read_bool();
    return x;
}

int SymbolDecoder::read_ns(int n) noexcept {
    if (n <= 1) return 0;
    const int w = floor_log2(static_cast<std::uint32_t>(n)) + 1;
    const int m = (1 << w) - n;
    const int v = read_literal(w - 1);
    if (v < m) return v;
    const int extra_bit = read_literal(1);
    return (v << 1) - m + extra_bit;
}

namespace {

template <typename T, std::size_t N>
void copy_table(T (&dst)[N], const T (&src)[N]) {
    std::memcpy(&dst, &src, sizeof(dst));
}

template <typename A, typename B>
void copy_raw(A& dst, const B& src) {
    static_assert(sizeof(A) == sizeof(B), "cdf table size mismatch");
    std::memcpy(&dst, &src, sizeof(A));
}

void reset_array(std::uint16_t* p, std::size_t total, std::size_t inner) {
    for (std::size_t k = inner - 1; k < total; k += inner) p[k] = 0;
}

template <typename A>
void reset_counts(A& arr, std::size_t inner) {
    reset_array(reinterpret_cast<std::uint16_t*>(&arr), sizeof(A) / sizeof(std::uint16_t), inner);
}

} // namespace

void CdfContext::init_non_coeff() {
    copy_raw(YMode, Default_Y_Mode_Cdf);
    copy_raw(UVModeCflNotAllowed, Default_Uv_Mode_Cfl_Not_Allowed_Cdf);
    copy_raw(UVModeCflAllowed, Default_Uv_Mode_Cfl_Allowed_Cdf);
    copy_raw(AngleDelta, Default_Angle_Delta_Cdf);
    copy_raw(Intrabc, Default_Intrabc_Cdf);
    copy_raw(PartitionW8, Default_Partition_W8_Cdf);
    copy_raw(PartitionW16, Default_Partition_W16_Cdf);
    copy_raw(PartitionW32, Default_Partition_W32_Cdf);
    copy_raw(PartitionW64, Default_Partition_W64_Cdf);
    copy_raw(PartitionW128, Default_Partition_W128_Cdf);
    copy_raw(SegmentId, Default_Segment_Id_Cdf);
    copy_raw(SegmentIdPredicted, Default_Segment_Id_Predicted_Cdf);
    copy_raw(Tx8x8, Default_Tx_8x8_Cdf);
    copy_raw(Tx16x16, Default_Tx_16x16_Cdf);
    copy_raw(Tx32x32, Default_Tx_32x32_Cdf);
    copy_raw(Tx64x64, Default_Tx_64x64_Cdf);
    copy_raw(TxfmSplit, Default_Txfm_Split_Cdf);
    copy_raw(FilterIntraMode, Default_Filter_Intra_Mode_Cdf);
    copy_raw(FilterIntra, Default_Filter_Intra_Cdf);
    copy_raw(InterpFilter, Default_Interp_Filter_Cdf);
    copy_raw(MotionMode, Default_Motion_Mode_Cdf);
    copy_raw(NewMv, Default_New_Mv_Cdf);
    copy_raw(ZeroMv, Default_Zero_Mv_Cdf);
    copy_raw(RefMv, Default_Ref_Mv_Cdf);
    copy_raw(CompoundMode, Default_Compound_Mode_Cdf);
    copy_raw(DrlMode, Default_Drl_Mode_Cdf);
    copy_raw(IsInter, Default_Is_Inter_Cdf);
    copy_raw(CompMode, Default_Comp_Mode_Cdf);
    copy_raw(SkipMode, Default_Skip_Mode_Cdf);
    copy_raw(Skip, Default_Skip_Cdf);
    copy_raw(CompRef, Default_Comp_Ref_Cdf);
    copy_raw(CompBwdRef, Default_Comp_Bwd_Ref_Cdf);
    copy_raw(SingleRef, Default_Single_Ref_Cdf);
    for (int i = 0; i < MV_CONTEXTS; ++i) {
        copy_raw(MvJoint[i], Default_Mv_Joint_Cdf);
        copy_raw(MvClass[i], Default_Mv_Class_Cdf);
        copy_raw(MvFr[i], Default_Mv_Fr_Cdf);
        copy_raw(MvClass0Fr[i], Default_Mv_Class0_Fr_Cdf);
        for (int comp = 0; comp < 2; ++comp) {
            copy_raw(MvClass0Bit[i][comp], Default_Mv_Class0_Bit_Cdf);
            copy_raw(MvClass0Hp[i][comp], Default_Mv_Class0_Hp_Cdf);
            copy_raw(MvSign[i][comp], Default_Mv_Sign_Cdf);
            copy_raw(MvBit[i][comp], Default_Mv_Bit_Cdf);
            copy_raw(MvHp[i][comp], Default_Mv_Hp_Cdf);
        }
    }
    copy_raw(PaletteYMode, Default_Palette_Y_Mode_Cdf);
    copy_raw(PaletteUVMode, Default_Palette_Uv_Mode_Cdf);
    copy_raw(PaletteYSize, Default_Palette_Y_Size_Cdf);
    copy_raw(PaletteUVSize, Default_Palette_Uv_Size_Cdf);
    std::memset(PaletteYColor, 0, sizeof(PaletteYColor));
    std::memset(PaletteUVColor, 0, sizeof(PaletteUVColor));
    for (int ctx = 0; ctx < PALETTE_COLOR_CONTEXTS; ++ctx) {
        std::memcpy(PaletteYColor[0][ctx], Default_Palette_Size_2_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_2_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[1][ctx], Default_Palette_Size_3_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_3_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[2][ctx], Default_Palette_Size_4_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_4_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[3][ctx], Default_Palette_Size_5_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_5_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[4][ctx], Default_Palette_Size_6_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_6_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[5][ctx], Default_Palette_Size_7_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_7_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteYColor[6][ctx], Default_Palette_Size_8_Y_Color_Cdf[ctx], sizeof(Default_Palette_Size_8_Y_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[0][ctx], Default_Palette_Size_2_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_2_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[1][ctx], Default_Palette_Size_3_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_3_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[2][ctx], Default_Palette_Size_4_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_4_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[3][ctx], Default_Palette_Size_5_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_5_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[4][ctx], Default_Palette_Size_6_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_6_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[5][ctx], Default_Palette_Size_7_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_7_Uv_Color_Cdf[ctx]));
        std::memcpy(PaletteUVColor[6][ctx], Default_Palette_Size_8_Uv_Color_Cdf[ctx], sizeof(Default_Palette_Size_8_Uv_Color_Cdf[ctx]));
    }
    copy_raw(DeltaQ, Default_Delta_Q_Cdf);
    copy_raw(DeltaLF, Default_Delta_Lf_Cdf);
    for (int i = 0; i < FRAME_LF_COUNT; ++i) copy_raw(DeltaLFMulti[i], Default_Delta_Lf_Cdf);
    copy_raw(IntraTxTypeSet1, Default_Intra_Tx_Type_Set1_Cdf);
    copy_raw(IntraTxTypeSet2, Default_Intra_Tx_Type_Set2_Cdf);
    copy_raw(InterTxTypeSet1, Default_Inter_Tx_Type_Set1_Cdf);
    copy_raw(InterTxTypeSet2, Default_Inter_Tx_Type_Set2_Cdf);
    copy_raw(InterTxTypeSet3, Default_Inter_Tx_Type_Set3_Cdf);
    copy_raw(UseObmc, Default_Use_Obmc_Cdf);
    copy_raw(InterIntra, Default_Inter_Intra_Cdf);
    copy_raw(CompRefType, Default_Comp_Ref_Type_Cdf);
    copy_raw(CflSign, Default_Cfl_Sign_Cdf);
    copy_raw(UniCompRef, Default_Uni_Comp_Ref_Cdf);
    copy_raw(WedgeInterIntra, Default_Wedge_Inter_Intra_Cdf);
    copy_raw(CompGroupIdx, Default_Comp_Group_Idx_Cdf);
    copy_raw(CompoundIdx, Default_Compound_Idx_Cdf);
    copy_raw(CompoundType, Default_Compound_Type_Cdf);
    copy_raw(InterIntraMode, Default_Inter_Intra_Mode_Cdf);
    copy_raw(WedgeIndex, Default_Wedge_Index_Cdf);
    copy_raw(CflAlpha, Default_Cfl_Alpha_Cdf);
    copy_raw(UseWiener, Default_Use_Wiener_Cdf);
    copy_raw(UseSgrproj, Default_Use_Sgrproj_Cdf);
    copy_raw(RestorationType, Default_Restoration_Type_Cdf);
}

void CdfContext::init_coeff(int base_q_idx) {
    int idx;
    if (base_q_idx <= 20) {
        idx = 0;
    } else if (base_q_idx <= 60) {
        idx = 1;
    } else if (base_q_idx <= 120) {
        idx = 2;
    } else {
        idx = 3;
    }
    copy_raw(TxbSkip, Default_Txb_Skip_Cdf[idx]);
    copy_raw(EobPt16, Default_Eob_Pt_16_Cdf[idx]);
    copy_raw(EobPt32, Default_Eob_Pt_32_Cdf[idx]);
    copy_raw(EobPt64, Default_Eob_Pt_64_Cdf[idx]);
    copy_raw(EobPt128, Default_Eob_Pt_128_Cdf[idx]);
    copy_raw(EobPt256, Default_Eob_Pt_256_Cdf[idx]);
    copy_raw(EobPt512, Default_Eob_Pt_512_Cdf[idx]);
    copy_raw(EobPt1024, Default_Eob_Pt_1024_Cdf[idx]);
    copy_raw(EobExtra, Default_Eob_Extra_Cdf[idx]);
    copy_raw(DcSign, Default_Dc_Sign_Cdf[idx]);
    copy_raw(CoeffBaseEob, Default_Coeff_Base_Eob_Cdf[idx]);
    copy_raw(CoeffBase, Default_Coeff_Base_Cdf[idx]);
    copy_raw(CoeffBr, Default_Coeff_Br_Cdf[idx]);
}

void CdfContext::reset_counters() {
    reset_counts(YMode, INTRA_MODES + 1);
    reset_counts(UVModeCflNotAllowed, UV_INTRA_MODES_CFL_NOT_ALLOWED + 1);
    reset_counts(UVModeCflAllowed, UV_INTRA_MODES_CFL_ALLOWED + 1);
    reset_counts(AngleDelta, 8);
    reset_counts(Intrabc, 3);
    reset_counts(PartitionW8, 5);
    reset_counts(PartitionW16, 11);
    reset_counts(PartitionW32, 11);
    reset_counts(PartitionW64, 11);
    reset_counts(PartitionW128, 9);
    reset_counts(SegmentId, MAX_SEGMENTS + 1);
    reset_counts(SegmentIdPredicted, 3);
    reset_counts(Tx8x8, MAX_TX_DEPTH + 1);
    reset_counts(Tx16x16, MAX_TX_DEPTH + 2);
    reset_counts(Tx32x32, MAX_TX_DEPTH + 2);
    reset_counts(Tx64x64, MAX_TX_DEPTH + 2);
    reset_counts(TxfmSplit, 3);
    reset_counts(FilterIntraMode, 6);
    reset_counts(FilterIntra, 3);
    reset_counts(InterpFilter, INTERP_FILTERS + 1);
    reset_counts(MotionMode, MOTION_MODES + 1);
    reset_counts(NewMv, 3);
    reset_counts(ZeroMv, 3);
    reset_counts(RefMv, 3);
    reset_counts(CompoundMode, COMPOUND_MODES + 1);
    reset_counts(DrlMode, 3);
    reset_counts(IsInter, 3);
    reset_counts(CompMode, 3);
    reset_counts(SkipMode, 3);
    reset_counts(Skip, 3);
    reset_counts(CompRef, 3);
    reset_counts(CompBwdRef, 3);
    reset_counts(SingleRef, 3);
    reset_counts(MvJoint, MV_JOINTS + 1);
    reset_counts(MvClass, MV_CLASSES + 1);
    reset_counts(MvClass0Bit, 3);
    reset_counts(MvFr, MV_JOINTS + 1);
    reset_counts(MvClass0Fr, MV_JOINTS + 1);
    reset_counts(MvClass0Hp, 3);
    reset_counts(MvSign, 3);
    reset_counts(MvBit, 3);
    reset_counts(MvHp, 3);
    reset_counts(PaletteYMode, 3);
    reset_counts(PaletteUVMode, 3);
    reset_counts(PaletteYSize, PALETTE_SIZES + 1);
    reset_counts(PaletteUVSize, PALETTE_SIZES + 1);
    for (int s = 0; s < PALETTE_SIZES; ++s) {
        for (int ctx = 0; ctx < PALETTE_COLOR_CONTEXTS; ++ctx) {
            PaletteYColor[s][ctx][s + 2] = 0;
            PaletteUVColor[s][ctx][s + 2] = 0;
        }
    }
    reset_counts(DeltaQ, DELTA_Q_SMALL + 2);
    reset_counts(DeltaLF, DELTA_LF_SMALL + 2);
    reset_counts(DeltaLFMulti, DELTA_LF_SMALL + 2);
    reset_counts(IntraTxTypeSet1, 8);
    reset_counts(IntraTxTypeSet2, 6);
    reset_counts(InterTxTypeSet1, 17);
    reset_counts(InterTxTypeSet2, 13);
    reset_counts(InterTxTypeSet3, 3);
    reset_counts(UseObmc, 3);
    reset_counts(InterIntra, 3);
    reset_counts(CompRefType, 3);
    reset_counts(CflSign, CFL_JOINT_SIGNS + 1);
    reset_counts(UniCompRef, 3);
    reset_counts(WedgeInterIntra, 3);
    reset_counts(CompGroupIdx, 3);
    reset_counts(CompoundIdx, 3);
    reset_counts(CompoundType, COMPOUND_TYPES + 1);
    reset_counts(InterIntraMode, INTERINTRA_MODES + 1);
    reset_counts(WedgeIndex, 17);
    reset_counts(CflAlpha, CFL_ALPHABET_SIZE + 1);
    reset_counts(UseWiener, 3);
    reset_counts(UseSgrproj, 3);
    reset_counts(RestorationType, RESTORE_SWITCHABLE + 1);
    reset_counts(TxbSkip, 3);
    reset_counts(EobPt16, 6);
    reset_counts(EobPt32, 7);
    reset_counts(EobPt64, 8);
    reset_counts(EobPt128, 9);
    reset_counts(EobPt256, 10);
    reset_counts(EobPt512, 11);
    reset_counts(EobPt1024, 12);
    reset_counts(EobExtra, 3);
    reset_counts(DcSign, 3);
    reset_counts(CoeffBaseEob, 4);
    reset_counts(CoeffBase, 5);
    reset_counts(CoeffBr, BR_CDF_SIZE + 1);
}

} // namespace av1
} // namespace video
} // namespace fizmo
