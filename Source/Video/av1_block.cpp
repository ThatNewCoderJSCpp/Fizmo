#include "av1_internal.hpp"

#include <algorithm>

namespace fizmo {
namespace video {
namespace av1 {

MiInfo& Tile::mi(int r, int c) noexcept {
    return d->mi[static_cast<std::size_t>(r) * static_cast<std::size_t>(d->mi_stride) + static_cast<std::size_t>(c)];
}

const MiInfo& Tile::mi(int r, int c) const noexcept {
    return d->mi[static_cast<std::size_t>(r) * static_cast<std::size_t>(d->mi_stride) + static_cast<std::size_t>(c)];
}

void Tile::init(Decoder* dec, int tileRow, int tileCol) {
    d = dec;
    seq = &dec->seq;
    fh = &dec->fh;
    MiRowStart = fh->MiRowStarts[tileRow];
    MiRowEnd = fh->MiRowStarts[tileRow + 1];
    MiColStart = fh->MiColStarts[tileCol];
    MiColEnd = fh->MiColStarts[tileCol + 1];
    CurrentQIndex = fh->base_q_idx;
    cdf = dec->cdf;
    std::memcpy(intra_frame_y_mode_cdf, Default_Intra_Frame_Y_Mode_Cdf, sizeof(intra_frame_y_mode_cdf));
    const std::size_t cols = static_cast<std::size_t>(fh->MiCols) + 64;
    const std::size_t rows = static_cast<std::size_t>(fh->MiRows) + 64;
    for (int p = 0; p < 3; ++p) {
        AboveLevelContext[p].assign(cols, 0);
        AboveDcContext[p].assign(cols, 0);
        LeftLevelContext[p].assign(rows, 0);
        LeftDcContext[p].assign(rows, 0);
    }
    AboveSegPredContext.assign(cols, 0);
    LeftSegPredContext.assign(rows, 0);
    for (int p = 0; p < 2; ++p) {
        AbovePaletteColors[p].assign(cols * 8, 0);
        LeftPaletteColors[p].assign(rows * 8, 0);
    }
}

bool Tile::decode(const std::uint8_t* data, std::size_t size) {
    sd.init(data, size, fh->disable_cdf_update != 0);
    for (int i = 0; i < FRAME_LF_COUNT; ++i) DeltaLF[i] = 0;
    for (int plane = 0; plane < seq->NumPlanes; ++plane) {
        for (int pass = 0; pass < 2; ++pass) {
            RefSgrXqd[plane][pass] = Sgrproj_Xqd_Mid[pass];
            for (int i = 0; i < WIENER_COEFFS; ++i) RefLrWiener[plane][pass][i] = Wiener_Taps_Mid[i];
        }
    }
    const int sbSize = seq->use_128x128_superblock ? BLOCK_128X128 : BLOCK_64X64;
    const int sbSize4 = Num_4x4_Blocks_Wide[sbSize];
    for (int r = MiRowStart; r < MiRowEnd; r += sbSize4) {
        for (int p = 0; p < 3; ++p) {
            std::fill(LeftLevelContext[p].begin(), LeftLevelContext[p].end(), 0);
            std::fill(LeftDcContext[p].begin(), LeftDcContext[p].end(), 0);
        }
        std::fill(LeftSegPredContext.begin(), LeftSegPredContext.end(), 0);
        for (int c = MiColStart; c < MiColEnd; c += sbSize4) {
            ReadDeltas = fh->delta_q_present;
            const int cs = d->cdef_stride;
            d->cdef_idx[static_cast<std::size_t>(r >> 4) * cs + (c >> 4)] = -1;
            if (seq->use_128x128_superblock) {
                d->cdef_idx[static_cast<std::size_t>(r >> 4) * cs + (c >> 4) + 1] = -1;
                d->cdef_idx[static_cast<std::size_t>((r >> 4) + 1) * cs + (c >> 4)] = -1;
                d->cdef_idx[static_cast<std::size_t>((r >> 4) + 1) * cs + (c >> 4) + 1] = -1;
            }
            clear_block_decoded_flags(r, c, sbSize4);
            read_lr(r, c, sbSize);
            decode_partition(r, c, sbSize);
            if (error || sd.exhausted()) return false;
        }
    }
    return true;
}

void Tile::clear_block_decoded_flags(int r, int c, int sbSize4) {
    for (int plane = 0; plane < seq->NumPlanes; ++plane) {
        const int subX = plane > 0 ? seq->subsampling_x : 0;
        const int subY = plane > 0 ? seq->subsampling_y : 0;
        const int sbWidth4 = (MiColEnd - c) >> subX;
        const int sbHeight4 = (MiRowEnd - r) >> subY;
        for (int y = -1; y <= (sbSize4 >> subY); ++y) {
            for (int x = -1; x <= (sbSize4 >> subX); ++x) {
                std::uint8_t v;
                if (y < 0 && x < sbWidth4) {
                    v = 1;
                } else if (x < 0 && y < sbHeight4) {
                    v = 1;
                } else {
                    v = 0;
                }
                block_decoded(plane, y, x) = v;
            }
        }
        block_decoded(plane, sbSize4 >> subY, -1) = 0;
    }
}

void Tile::read_lr(int r, int c, int bSize) {
    if (fh->allow_intrabc) return;
    const int w = Num_4x4_Blocks_Wide[bSize];
    const int h = Num_4x4_Blocks_High[bSize];
    for (int plane = 0; plane < seq->NumPlanes; ++plane) {
        if (fh->FrameRestorationType[plane] != RESTORE_NONE) {
            const int subX = plane == 0 ? 0 : seq->subsampling_x;
            const int subY = plane == 0 ? 0 : seq->subsampling_y;
            const int unitSize = fh->LoopRestorationSize[plane];
            const int unitRows = d->lr_unit_rows[plane];
            const int unitCols = d->lr_unit_cols[plane];
            const int unitRowStart = (r * (MI_SIZE >> subY) + unitSize - 1) / unitSize;
            const int unitRowEnd = imin(unitRows, ((r + h) * (MI_SIZE >> subY) + unitSize - 1) / unitSize);
            int numerator;
            int denominator;
            if (fh->use_superres) {
                numerator = (MI_SIZE >> subX) * fh->SuperresDenom;
                denominator = unitSize * SUPERRES_NUM;
            } else {
                numerator = MI_SIZE >> subX;
                denominator = unitSize;
            }
            const int unitColStart = (c * numerator + denominator - 1) / denominator;
            const int unitColEnd = imin(unitCols, ((c + w) * numerator + denominator - 1) / denominator);
            for (int unitRow = unitRowStart; unitRow < unitRowEnd; ++unitRow) {
                for (int unitCol = unitColStart; unitCol < unitColEnd; ++unitCol) read_lr_unit(plane, unitRow, unitCol);
            }
        }
    }
}

namespace {

int inverse_recenter(int r, int v) {
    if (v > 2 * r) return v;
    if (v & 1) return r - ((v + 1) >> 1);
    return r + (v >> 1);
}

} // namespace

int Tile::decode_subexp_bool(int numSyms, int k) {
    int i = 0;
    int mk = 0;
    while (true) {
        const int b2 = i ? k + i - 1 : k;
        const int a = 1 << b2;
        if (numSyms <= mk + 3 * a) {
            return sd.read_ns(numSyms - mk) + mk;
        } else {
            if (sd.read_literal(1)) {
                ++i;
                mk += a;
            } else {
                return sd.read_literal(b2) + mk;
            }
        }
    }
}

int Tile::decode_unsigned_subexp_with_ref_bool(int mx, int k, int r) {
    const int v = decode_subexp_bool(mx, k);
    if ((r << 1) <= mx) return inverse_recenter(r, v);
    return mx - 1 - inverse_recenter(mx - 1 - r, v);
}

int Tile::decode_signed_subexp_with_ref_bool(int low, int high, int k, int r) {
    const int x = decode_unsigned_subexp_with_ref_bool(high - low, k, r - low);
    return x + low;
}

void Tile::read_lr_unit(int plane, int unitRow, int unitCol) {
    LrUnit& u = d->lr_units[plane][static_cast<std::size_t>(unitRow) * d->lr_unit_cols[plane] + unitCol];
    int restoration_type;
    if (fh->FrameRestorationType[plane] == RESTORE_WIENER) {
        restoration_type = sd.read_symbol(cdf.UseWiener, 2) ? RESTORE_WIENER : RESTORE_NONE;
    } else if (fh->FrameRestorationType[plane] == RESTORE_SGRPROJ) {
        restoration_type = sd.read_symbol(cdf.UseSgrproj, 2) ? RESTORE_SGRPROJ : RESTORE_NONE;
    } else {
        restoration_type = sd.read_symbol(cdf.RestorationType, 3);
    }
    u.type = static_cast<std::uint8_t>(restoration_type);
    if (restoration_type == RESTORE_WIENER) {
        for (int pass = 0; pass < 2; ++pass) {
            int firstCoeff;
            if (plane) {
                firstCoeff = 1;
                u.wiener[pass][0] = 0;
            } else {
                firstCoeff = 0;
            }
            for (int j = firstCoeff; j < 3; ++j) {
                const int mn = Wiener_Taps_Min[j];
                const int mx = Wiener_Taps_Max[j];
                const int k = Wiener_Taps_K[j];
                const int v = decode_signed_subexp_with_ref_bool(mn, mx + 1, k, RefLrWiener[plane][pass][j]);
                u.wiener[pass][j] = static_cast<std::int8_t>(v);
                RefLrWiener[plane][pass][j] = v;
            }
        }
    } else if (restoration_type == RESTORE_SGRPROJ) {
        const int lr_sgr_set = sd.read_literal(SGRPROJ_PARAMS_BITS);
        u.sgr_set = static_cast<std::uint8_t>(lr_sgr_set);
        for (int i = 0; i < 2; ++i) {
            const int radius = Sgr_Params[lr_sgr_set][i * 2];
            const int mn = Sgrproj_Xqd_Min[i];
            const int mx = Sgrproj_Xqd_Max[i];
            int v;
            if (radius) {
                v = decode_signed_subexp_with_ref_bool(mn, mx + 1, SGRPROJ_PRJ_SUBEXP_K, RefSgrXqd[plane][i]);
            } else {
                v = 0;
                if (i == 1) v = clip3(mn, mx, (1 << SGRPROJ_PRJ_BITS) - RefSgrXqd[plane][0]);
            }
            u.sgr_xqd[i] = static_cast<std::int8_t>(v);
            RefSgrXqd[plane][i] = v;
        }
    }
}

void Tile::decode_partition(int r, int c, int bSize) {
    if (r >= fh->MiRows || c >= fh->MiCols) return;
    AvailU = is_inside(r - 1, c);
    AvailL = is_inside(r, c - 1);
    const int num4x4 = Num_4x4_Blocks_Wide[bSize];
    const int halfBlock4x4 = num4x4 >> 1;
    const int quarterBlock4x4 = halfBlock4x4 >> 1;
    const int hasRows = (r + halfBlock4x4) < fh->MiRows;
    const int hasCols = (c + halfBlock4x4) < fh->MiCols;
    int partition;
    if (bSize < BLOCK_8X8) {
        partition = PARTITION_NONE;
    } else {
        const int bsl = Mi_Width_Log2[bSize];
        const int above = AvailU && (Mi_Width_Log2[mi(r - 1, c).mi_size] < bsl);
        const int left = AvailL && (Mi_Height_Log2[mi(r, c - 1).mi_size] < bsl);
        const int ctx = left * 2 + above;
        std::uint16_t* pcdf;
        int n;
        if (bsl == 1) {
            pcdf = cdf.PartitionW8[ctx];
            n = 4;
        } else if (bsl == 2) {
            pcdf = cdf.PartitionW16[ctx];
            n = 10;
        } else if (bsl == 3) {
            pcdf = cdf.PartitionW32[ctx];
            n = 10;
        } else if (bsl == 4) {
            pcdf = cdf.PartitionW64[ctx];
            n = 10;
        } else {
            pcdf = cdf.PartitionW128[ctx];
            n = 8;
        }
        if (hasRows && hasCols) {
            partition = sd.read_symbol(pcdf, n);
        } else if (hasCols || hasRows) {
            auto p = [&](int k) { return static_cast<int>(pcdf[k]) - (k > 0 ? static_cast<int>(pcdf[k - 1]) : 0); };
            int psum;
            if (hasCols) {
                psum = p(PARTITION_VERT) + p(PARTITION_SPLIT) + p(PARTITION_HORZ_A) + p(PARTITION_VERT_A) + p(PARTITION_VERT_B);
                if (bSize != BLOCK_128X128) psum += p(PARTITION_VERT_4);
            } else {
                psum = p(PARTITION_HORZ) + p(PARTITION_SPLIT) + p(PARTITION_HORZ_A) + p(PARTITION_HORZ_B) + p(PARTITION_VERT_A);
                if (bSize != BLOCK_128X128) psum += p(PARTITION_HORZ_4);
            }
            std::uint16_t tmp[3];
            tmp[0] = static_cast<std::uint16_t>((1 << 15) - psum);
            tmp[1] = 1 << 15;
            tmp[2] = 0;
            const int v = sd.read_symbol(tmp, 2);
            if (hasCols) {
                partition = v ? PARTITION_SPLIT : PARTITION_HORZ;
            } else {
                partition = v ? PARTITION_SPLIT : PARTITION_VERT;
            }
        } else {
            partition = PARTITION_SPLIT;
        }
    }
    const int subSize = Partition_Subsize[partition][bSize];
    const int splitSize = Partition_Subsize[PARTITION_SPLIT][bSize];
    if (subSize < 0 || subSize >= BLOCK_SIZES || error) return;
    if (seq->NumPlanes > 1 && get_plane_residual_size(subSize, 1) == BLOCK_INVALID) {
        error = true;
        return;
    }
    switch (partition) {
    case PARTITION_NONE:
        decode_block(r, c, subSize);
        break;
    case PARTITION_HORZ:
        decode_block(r, c, subSize);
        if (hasRows) decode_block(r + halfBlock4x4, c, subSize);
        break;
    case PARTITION_VERT:
        decode_block(r, c, subSize);
        if (hasCols) decode_block(r, c + halfBlock4x4, subSize);
        break;
    case PARTITION_SPLIT:
        decode_partition(r, c, subSize);
        decode_partition(r, c + halfBlock4x4, subSize);
        decode_partition(r + halfBlock4x4, c, subSize);
        decode_partition(r + halfBlock4x4, c + halfBlock4x4, subSize);
        break;
    case PARTITION_HORZ_A:
        decode_block(r, c, splitSize);
        decode_block(r, c + halfBlock4x4, splitSize);
        decode_block(r + halfBlock4x4, c, subSize);
        break;
    case PARTITION_HORZ_B:
        decode_block(r, c, subSize);
        decode_block(r + halfBlock4x4, c, splitSize);
        decode_block(r + halfBlock4x4, c + halfBlock4x4, splitSize);
        break;
    case PARTITION_VERT_A:
        decode_block(r, c, splitSize);
        decode_block(r + halfBlock4x4, c, splitSize);
        decode_block(r, c + halfBlock4x4, subSize);
        break;
    case PARTITION_VERT_B:
        decode_block(r, c, subSize);
        decode_block(r, c + halfBlock4x4, splitSize);
        decode_block(r + halfBlock4x4, c + halfBlock4x4, splitSize);
        break;
    case PARTITION_HORZ_4:
        decode_block(r + quarterBlock4x4 * 0, c, subSize);
        decode_block(r + quarterBlock4x4 * 1, c, subSize);
        decode_block(r + quarterBlock4x4 * 2, c, subSize);
        if (r + quarterBlock4x4 * 3 < fh->MiRows) decode_block(r + quarterBlock4x4 * 3, c, subSize);
        break;
    default:
        decode_block(r, c + quarterBlock4x4 * 0, subSize);
        decode_block(r, c + quarterBlock4x4 * 1, subSize);
        decode_block(r, c + quarterBlock4x4 * 2, subSize);
        if (c + quarterBlock4x4 * 3 < fh->MiCols) decode_block(r, c + quarterBlock4x4 * 3, subSize);
        break;
    }
}

int Tile::get_plane_residual_size(int subsize, int plane) const noexcept {
    const int subx = plane > 0 ? seq->subsampling_x : 0;
    const int suby = plane > 0 ? seq->subsampling_y : 0;
    return Subsampled_Size[subsize][subx][suby];
}

void Tile::reset_block_context(int bw4, int bh4) {
    for (int plane = 0; plane < 1 + 2 * HasChroma; ++plane) {
        const int subX = plane > 0 ? seq->subsampling_x : 0;
        const int subY = plane > 0 ? seq->subsampling_y : 0;
        for (int i = MiCol >> subX; i < ((MiCol + bw4) >> subX); ++i) {
            AboveLevelContext[plane][i] = 0;
            AboveDcContext[plane][i] = 0;
        }
        for (int i = MiRow >> subY; i < ((MiRow + bh4) >> subY); ++i) {
            LeftLevelContext[plane][i] = 0;
            LeftDcContext[plane][i] = 0;
        }
    }
}

void Tile::decode_block(int r, int c, int subSize) {
    if (error) return;
    MiRow = r;
    MiCol = c;
    MiSize = subSize;
    const int bw4 = Num_4x4_Blocks_Wide[subSize];
    const int bh4 = Num_4x4_Blocks_High[subSize];
    if (bh4 == 1 && seq->subsampling_y && (MiRow & 1) == 0) {
        HasChroma = 0;
    } else if (bw4 == 1 && seq->subsampling_x && (MiCol & 1) == 0) {
        HasChroma = 0;
    } else {
        HasChroma = seq->NumPlanes > 1;
    }
    AvailU = is_inside(r - 1, c);
    AvailL = is_inside(r, c - 1);
    AvailUChroma = AvailU;
    AvailLChroma = AvailL;
    if (HasChroma) {
        if (seq->subsampling_y && bh4 == 1) AvailUChroma = is_inside(r - 2, c);
        if (seq->subsampling_x && bw4 == 1) AvailLChroma = is_inside(r, c - 2);
    } else {
        AvailUChroma = 0;
        AvailLChroma = 0;
    }
    use_intrabc = 0;
    interintra = 0;
    motion_mode = SIMPLE;
    compound_type = COMPOUND_AVERAGE;
    comp_group_idx = 0;
    compound_idx = 1;
    use_filter_intra = 0;
    PaletteSizeY = 0;
    PaletteSizeUV = 0;
    AngleDeltaY = 0;
    AngleDeltaUV = 0;
    YMode = DC_PRED;
    UVMode = DC_PRED;
    CflAlphaU = 0;
    CflAlphaV = 0;
    interp_filter[0] = 0;
    interp_filter[1] = 0;
    MvVal[0] = Mv();
    MvVal[1] = Mv();
    if (fh->FrameIsIntra) {
        intra_frame_mode_info();
    } else {
        inter_frame_mode_info();
    }
    palette_tokens();
    read_block_tx_size();
    if (skip) reset_block_context(bw4, bh4);
    const int isCompound = RefFrame[1] > INTRA_FRAME;
    for (int y = 0; y < bh4; ++y) {
        MiInfo* row = &mi(r + y, c);
        for (int x = 0; x < bw4; ++x) {
            MiInfo& m = row[x];
            m.y_mode = static_cast<std::uint8_t>(YMode);
            m.ref_frame[0] = static_cast<std::int8_t>(RefFrame[0]);
            m.ref_frame[1] = static_cast<std::int8_t>(RefFrame[1]);
            m.written = d->mi_stamp;
            if (RefFrame[0] == INTRA_FRAME && HasChroma) m.uv_mode = static_cast<std::uint8_t>(UVMode);
            if (is_inter) {
                if (!use_intrabc) {
                    m.comp_group_idx = static_cast<std::uint8_t>(comp_group_idx);
                    m.compound_idx = static_cast<std::uint8_t>(compound_idx);
                }
                m.interp_filter[0] = static_cast<std::uint8_t>(interp_filter[0]);
                m.interp_filter[1] = static_cast<std::uint8_t>(interp_filter[1]);
                m.mv[0] = MvVal[0];
                if (isCompound) m.mv[1] = MvVal[1];
            }
            m.motion_mode = static_cast<std::uint8_t>(motion_mode);
        }
    }
    compute_prediction();
    residual();
    for (int y = 0; y < bh4; ++y) {
        MiInfo* row = &mi(r + y, c);
        for (int x = 0; x < bw4; ++x) {
            MiInfo& m = row[x];
            m.is_inter = static_cast<std::uint8_t>(is_inter);
            m.skip_mode = static_cast<std::uint8_t>(skip_mode);
            m.skip = static_cast<std::uint8_t>(skip);
            m.tx_size = static_cast<std::uint8_t>(TxSize);
            m.mi_size = static_cast<std::uint8_t>(MiSize);
            m.segment_id = static_cast<std::uint8_t>(segment_id);
            m.palette_size[0] = static_cast<std::uint8_t>(PaletteSizeY);
            m.palette_size[1] = static_cast<std::uint8_t>(PaletteSizeUV);
            for (int i = 0; i < FRAME_LF_COUNT; ++i) m.delta_lf[i] = static_cast<std::int8_t>(DeltaLF[i]);
        }
    }
    if (PaletteSizeY) {
        for (int x = 0; x < bw4; ++x) std::memcpy(&AbovePaletteColors[0][static_cast<std::size_t>(c + x) * 8], palette_colors_y, sizeof(palette_colors_y));
        for (int y = 0; y < bh4; ++y) std::memcpy(&LeftPaletteColors[0][static_cast<std::size_t>(r + y) * 8], palette_colors_y, sizeof(palette_colors_y));
    }
    if (PaletteSizeUV) {
        for (int x = 0; x < bw4; ++x) std::memcpy(&AbovePaletteColors[1][static_cast<std::size_t>(c + x) * 8], palette_colors_u, sizeof(palette_colors_u));
        for (int y = 0; y < bh4; ++y) std::memcpy(&LeftPaletteColors[1][static_cast<std::size_t>(r + y) * 8], palette_colors_u, sizeof(palette_colors_u));
    }
}

int Tile::seg_feature_active(int feature) const noexcept {
    return fh->segmentation_enabled && fh->FeatureEnabled[segment_id][feature];
}

int Tile::get_qindex(int ignoreDeltaQ, int segmentId) const noexcept {
    return d->get_qindex(ignoreDeltaQ, segmentId, CurrentQIndex);
}

void Tile::intra_frame_mode_info() {
    skip = 0;
    if (fh->SegIdPreSkip) intra_segment_id();
    skip_mode = 0;
    read_skip();
    if (!fh->SegIdPreSkip) intra_segment_id();
    read_cdef();
    read_delta_qindex();
    read_delta_lf();
    ReadDeltas = 0;
    RefFrame[0] = INTRA_FRAME;
    RefFrame[1] = NONE;
    if (fh->allow_intrabc) {
        use_intrabc = sd.read_symbol(cdf.Intrabc, 2);
    } else {
        use_intrabc = 0;
    }
    if (use_intrabc) {
        is_inter = 1;
        YMode = DC_PRED;
        UVMode = DC_PRED;
        motion_mode = SIMPLE;
        compound_type = COMPOUND_AVERAGE;
        PaletteSizeY = 0;
        PaletteSizeUV = 0;
        interp_filter[0] = BILINEAR;
        interp_filter[1] = BILINEAR;
        AV1_STAT(ST_INTRABC);
        find_mv_stack(0);
        assign_mv(0);
    } else {
        is_inter = 0;
        const int abovemode = Intra_Mode_Context[AvailU ? mi(MiRow - 1, MiCol).y_mode : DC_PRED];
        const int leftmode = Intra_Mode_Context[AvailL ? mi(MiRow, MiCol - 1).y_mode : DC_PRED];
        YMode = sd.read_symbol(intra_frame_y_mode_cdf[abovemode][leftmode], INTRA_MODES);
        intra_angle_info_y();
        if (HasChroma) {
            int cflAllowed;
            if (Lossless && get_plane_residual_size(MiSize, 1) == BLOCK_4X4) {
                cflAllowed = 1;
            } else if (!Lossless && imax(block_width(MiSize), block_height(MiSize)) <= 32) {
                cflAllowed = 1;
            } else {
                cflAllowed = 0;
            }
            if (cflAllowed) {
                UVMode = sd.read_symbol(cdf.UVModeCflAllowed[YMode], UV_INTRA_MODES_CFL_ALLOWED);
            } else {
                UVMode = sd.read_symbol(cdf.UVModeCflNotAllowed[YMode], UV_INTRA_MODES_CFL_NOT_ALLOWED);
            }
            if (UVMode == UV_CFL_PRED) read_cfl_alphas();
            intra_angle_info_uv();
        }
        PaletteSizeY = 0;
        PaletteSizeUV = 0;
        if (MiSize >= BLOCK_8X8 && block_width(MiSize) <= 64 && block_height(MiSize) <= 64 && fh->allow_screen_content_tools) palette_mode_info();
        filter_intra_mode_info();
    }
}

void Tile::intra_segment_id() {
    if (fh->segmentation_enabled) {
        read_segment_id();
    } else {
        segment_id = 0;
    }
    Lossless = fh->LosslessArray[segment_id];
}

namespace {

int neg_deinterleave(int diff, int ref, int mx) {
    if (!ref) return diff;
    if (ref >= (mx - 1)) return mx - diff - 1;
    if (2 * ref < mx) {
        if (diff <= 2 * ref) {
            if (diff & 1) return ref + ((diff + 1) >> 1);
            return ref - (diff >> 1);
        }
        return diff;
    } else {
        if (diff <= 2 * (mx - ref - 1)) {
            if (diff & 1) return ref + ((diff + 1) >> 1);
            return ref - (diff >> 1);
        }
        return mx - (diff + 1);
    }
}

} // namespace

void Tile::read_segment_id() {
    int prevUL = -1;
    int prevU = -1;
    int prevL = -1;
    if (AvailU && AvailL) prevUL = mi(MiRow - 1, MiCol - 1).segment_id;
    if (AvailU) prevU = mi(MiRow - 1, MiCol).segment_id;
    if (AvailL) prevL = mi(MiRow, MiCol - 1).segment_id;
    int pred;
    if (prevU == -1) {
        pred = prevL == -1 ? 0 : prevL;
    } else if (prevL == -1) {
        pred = prevU;
    } else {
        pred = prevUL == prevU ? prevU : prevL;
    }
    if (skip) {
        segment_id = pred;
    } else {
        int ctx;
        if (prevUL < 0) {
            ctx = 0;
        } else if (prevUL == prevU && prevUL == prevL) {
            ctx = 2;
        } else if (prevUL == prevU || prevUL == prevL || prevU == prevL) {
            ctx = 1;
        } else {
            ctx = 0;
        }
        const int v = sd.read_symbol(cdf.SegmentId[ctx], MAX_SEGMENTS);
        segment_id = neg_deinterleave(v, pred, fh->LastActiveSegId + 1);
    }
    segment_id = clip3(0, MAX_SEGMENTS - 1, segment_id);
}

void Tile::read_skip_mode() {
    if (seg_feature_active(SEG_LVL_SKIP) || seg_feature_active(SEG_LVL_REF_FRAME) || seg_feature_active(SEG_LVL_GLOBALMV) || !fh->skip_mode_present || block_width(MiSize) < 8 || block_height(MiSize) < 8) {
        skip_mode = 0;
    } else {
        int ctx = 0;
        if (AvailU) ctx += mi(MiRow - 1, MiCol).skip_mode;
        if (AvailL) ctx += mi(MiRow, MiCol - 1).skip_mode;
        skip_mode = sd.read_symbol(cdf.SkipMode[ctx], 2);
    }
}

void Tile::read_skip() {
    if (fh->SegIdPreSkip && seg_feature_active(SEG_LVL_SKIP)) {
        skip = 1;
    } else {
        int ctx = 0;
        if (AvailU) ctx += mi(MiRow - 1, MiCol).skip;
        if (AvailL) ctx += mi(MiRow, MiCol - 1).skip;
        skip = sd.read_symbol(cdf.Skip[ctx], 2);
    }
}

void Tile::read_cdef() {
    if (skip || fh->CodedLossless || !seq->enable_cdef || fh->allow_intrabc) return;
    const int cdefSize4 = Num_4x4_Blocks_Wide[BLOCK_64X64];
    const int cdefMask4 = ~(cdefSize4 - 1);
    const int r = MiRow & cdefMask4;
    const int c = MiCol & cdefMask4;
    const int cs = d->cdef_stride;
    std::int8_t& idx = d->cdef_idx[static_cast<std::size_t>(r >> 4) * cs + (c >> 4)];
    if (idx == -1) {
        idx = static_cast<std::int8_t>(sd.read_literal(fh->cdef_bits));
        const int w4 = Num_4x4_Blocks_Wide[MiSize];
        const int h4 = Num_4x4_Blocks_High[MiSize];
        for (int i = r; i < r + h4; i += cdefSize4) {
            for (int j = c; j < c + w4; j += cdefSize4) d->cdef_idx[static_cast<std::size_t>(i >> 4) * cs + (j >> 4)] = idx;
        }
    }
}

void Tile::read_delta_qindex() {
    const int sbSize = seq->use_128x128_superblock ? BLOCK_128X128 : BLOCK_64X64;
    if (MiSize == sbSize && skip) return;
    if (ReadDeltas) {
        int delta_q_abs = sd.read_symbol(cdf.DeltaQ, DELTA_Q_SMALL + 1);
        if (delta_q_abs == DELTA_Q_SMALL) {
            const int delta_q_rem_bits = sd.read_literal(3) + 1;
            const int delta_q_abs_bits = sd.read_literal(delta_q_rem_bits);
            delta_q_abs = delta_q_abs_bits + (1 << delta_q_rem_bits) + 1;
        }
        if (delta_q_abs) {
            const int delta_q_sign_bit = sd.read_literal(1);
            const int reducedDeltaQIndex = delta_q_sign_bit ? -delta_q_abs : delta_q_abs;
            CurrentQIndex = clip3(1, 255, CurrentQIndex + reducedDeltaQIndex * (1 << fh->delta_q_res));
        }
    }
}

void Tile::read_delta_lf() {
    const int sbSize = seq->use_128x128_superblock ? BLOCK_128X128 : BLOCK_64X64;
    if (MiSize == sbSize && skip) return;
    if (ReadDeltas && fh->delta_lf_present) {
        int frameLfCount = 1;
        if (fh->delta_lf_multi) frameLfCount = seq->NumPlanes > 1 ? FRAME_LF_COUNT : (FRAME_LF_COUNT - 2);
        for (int i = 0; i < frameLfCount; ++i) {
            std::uint16_t* c = fh->delta_lf_multi ? cdf.DeltaLFMulti[i] : cdf.DeltaLF;
            const int delta_lf_abs = sd.read_symbol(c, DELTA_LF_SMALL + 1);
            int deltaLfAbs;
            if (delta_lf_abs == DELTA_LF_SMALL) {
                const int n = sd.read_literal(3) + 1;
                const int delta_lf_abs_bits = sd.read_literal(n);
                deltaLfAbs = delta_lf_abs_bits + (1 << n) + 1;
            } else {
                deltaLfAbs = delta_lf_abs;
            }
            if (deltaLfAbs) {
                const int sign = sd.read_literal(1);
                const int reduced = sign ? -deltaLfAbs : deltaLfAbs;
                DeltaLF[i] = clip3(-MAX_LOOP_FILTER, MAX_LOOP_FILTER, DeltaLF[i] + reduced * (1 << fh->delta_lf_res));
            }
        }
    }
}

namespace {

int find_tx_size(int w, int h) {
    int txSz;
    for (txSz = 0; txSz < TX_SIZES_ALL; ++txSz) {
        if (Tx_Width[txSz] == w && Tx_Height[txSz] == h) break;
    }
    return txSz;
}

} // namespace

int Tile::get_above_tx_width(int row, int col) {
    if (row == MiRow) {
        if (!AvailU) {
            return 64;
        } else {
            const MiInfo& a = mi(row - 1, col);
            if (a.skip && a.is_inter) return block_width(a.mi_size);
        }
    }
    return Tx_Width[mi(row - 1, col).inter_tx_size];
}

int Tile::get_left_tx_height(int row, int col) {
    if (col == MiCol) {
        if (!AvailL) {
            return 64;
        } else {
            const MiInfo& l = mi(row, col - 1);
            if (l.skip && l.is_inter) return block_height(l.mi_size);
        }
    }
    return Tx_Height[mi(row, col - 1).inter_tx_size];
}

void Tile::read_tx_size(int allowSelect) {
    if (Lossless) {
        TxSize = TX_4X4;
        return;
    }
    const int maxRectTxSize = Max_Tx_Size_Rect[MiSize];
    const int maxTxDepth = Max_Tx_Depth[MiSize];
    TxSize = maxRectTxSize;
    if (MiSize > BLOCK_4X4 && allowSelect && fh->TxMode == TX_MODE_SELECT) {
        const int maxTxWidth = Tx_Width[maxRectTxSize];
        const int maxTxHeight = Tx_Height[maxRectTxSize];
        int aboveW;
        int leftH;
        if (AvailU && mi(MiRow - 1, MiCol).is_inter) {
            aboveW = block_width(mi(MiRow - 1, MiCol).mi_size);
        } else if (AvailU) {
            aboveW = get_above_tx_width(MiRow, MiCol);
        } else {
            aboveW = 0;
        }
        if (AvailL && mi(MiRow, MiCol - 1).is_inter) {
            leftH = block_height(mi(MiRow, MiCol - 1).mi_size);
        } else if (AvailL) {
            leftH = get_left_tx_height(MiRow, MiCol);
        } else {
            leftH = 0;
        }
        const int ctx = (aboveW >= maxTxWidth) + (leftH >= maxTxHeight);
        int tx_depth;
        if (maxTxDepth == 4) {
            tx_depth = sd.read_symbol(cdf.Tx64x64[ctx], 3);
        } else if (maxTxDepth == 3) {
            tx_depth = sd.read_symbol(cdf.Tx32x32[ctx], 3);
        } else if (maxTxDepth == 2) {
            tx_depth = sd.read_symbol(cdf.Tx16x16[ctx], 3);
        } else {
            tx_depth = sd.read_symbol(cdf.Tx8x8[ctx], 2);
        }
        for (int i = 0; i < tx_depth; ++i) TxSize = Split_Tx_Size[TxSize];
    }
}

void Tile::read_block_tx_size() {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    if (fh->TxMode == TX_MODE_SELECT && MiSize > BLOCK_4X4 && is_inter && !skip && !Lossless) {
        const int maxTxSz = Max_Tx_Size_Rect[MiSize];
        const int txW4 = Tx_Width[maxTxSz] / MI_SIZE;
        const int txH4 = Tx_Height[maxTxSz] / MI_SIZE;
        for (int row = MiRow; row < MiRow + bh4; row += txH4) {
            for (int col = MiCol; col < MiCol + bw4; col += txW4) read_var_tx_size(row, col, maxTxSz, 0);
        }
    } else {
        read_tx_size(!skip || !is_inter);
        for (int row = MiRow; row < MiRow + bh4; ++row) {
            MiInfo* m = &mi(row, MiCol);
            for (int col = 0; col < bw4; ++col) m[col].inter_tx_size = static_cast<std::uint8_t>(TxSize);
        }
    }
}

void Tile::read_var_tx_size(int row, int col, int txSz, int depth) {
    if (row >= fh->MiRows || col >= fh->MiCols) return;
    int txfm_split;
    if (txSz == TX_4X4 || depth == MAX_VARTX_DEPTH) {
        txfm_split = 0;
    } else {
        const int above = get_above_tx_width(row, col) < Tx_Width[txSz];
        const int left = get_left_tx_height(row, col) < Tx_Height[txSz];
        const int size = imin(64, imax(block_width(MiSize), block_height(MiSize)));
        const int maxTxSz = find_tx_size(size, size);
        const int txSzSqrUp = Tx_Size_Sqr_Up[txSz];
        const int ctx = (txSzSqrUp != maxTxSz) * 3 + (TX_SIZES - 1 - maxTxSz) * 6 + above + left;
        txfm_split = sd.read_symbol(cdf.TxfmSplit[ctx], 2);
        if (txfm_split) AV1_STAT(ST_VARTX);
    }
    const int w4 = Tx_Width[txSz] / MI_SIZE;
    const int h4 = Tx_Height[txSz] / MI_SIZE;
    if (txfm_split) {
        const int subTxSz = Split_Tx_Size[txSz];
        const int stepW = Tx_Width[subTxSz] / MI_SIZE;
        const int stepH = Tx_Height[subTxSz] / MI_SIZE;
        for (int i = 0; i < h4; i += stepH) {
            for (int j = 0; j < w4; j += stepW) read_var_tx_size(row + i, col + j, subTxSz, depth + 1);
        }
    } else {
        for (int i = 0; i < h4; ++i) {
            MiInfo* m = &mi(row + i, col);
            for (int j = 0; j < w4; ++j) m[j].inter_tx_size = static_cast<std::uint8_t>(txSz);
        }
        TxSize = txSz;
    }
}

void Tile::inter_frame_mode_info() {
    use_intrabc = 0;
    LeftRefFrame[0] = AvailL ? mi(MiRow, MiCol - 1).ref_frame[0] : INTRA_FRAME;
    AboveRefFrame[0] = AvailU ? mi(MiRow - 1, MiCol).ref_frame[0] : INTRA_FRAME;
    LeftRefFrame[1] = AvailL ? mi(MiRow, MiCol - 1).ref_frame[1] : NONE;
    AboveRefFrame[1] = AvailU ? mi(MiRow - 1, MiCol).ref_frame[1] : NONE;
    LeftIntra = LeftRefFrame[0] <= INTRA_FRAME;
    AboveIntra = AboveRefFrame[0] <= INTRA_FRAME;
    LeftSingle = LeftRefFrame[1] <= INTRA_FRAME;
    AboveSingle = AboveRefFrame[1] <= INTRA_FRAME;
    skip = 0;
    inter_segment_id(1);
    read_skip_mode();
    if (skip_mode) {
        skip = 1;
    } else {
        read_skip();
    }
    if (!fh->SegIdPreSkip) inter_segment_id(0);
    Lossless = fh->LosslessArray[segment_id];
    read_cdef();
    read_delta_qindex();
    read_delta_lf();
    ReadDeltas = 0;
    read_is_inter();
    if (is_inter) {
        inter_block_mode_info();
    } else {
        intra_block_mode_info();
    }
}

int Tile::get_segment_id() {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    const int xMis = imin(fh->MiCols - MiCol, bw4);
    const int yMis = imin(fh->MiRows - MiRow, bh4);
    int seg = 7;
    for (int y = 0; y < yMis; ++y) {
        for (int x = 0; x < xMis; ++x) seg = imin(seg, d->prev_segment_ids[static_cast<std::size_t>(MiRow + y) * fh->MiCols + MiCol + x]);
    }
    return seg;
}

void Tile::inter_segment_id(int preSkip) {
    if (fh->segmentation_enabled) {
        const int predictedSegmentId = get_segment_id();
        if (fh->segmentation_update_map) {
            if (preSkip && !fh->SegIdPreSkip) {
                segment_id = 0;
                return;
            }
            if (!preSkip) {
                if (skip) {
                    const int seg_id_predicted = 0;
                    for (int i = 0; i < Num_4x4_Blocks_Wide[MiSize]; ++i) AboveSegPredContext[MiCol + i] = seg_id_predicted;
                    for (int i = 0; i < Num_4x4_Blocks_High[MiSize]; ++i) LeftSegPredContext[MiRow + i] = seg_id_predicted;
                    read_segment_id();
                    return;
                }
            }
            if (fh->segmentation_temporal_update == 1) {
                const int ctx = LeftSegPredContext[MiRow] + AboveSegPredContext[MiCol];
                const int seg_id_predicted = sd.read_symbol(cdf.SegmentIdPredicted[ctx], 2);
                if (seg_id_predicted) {
                    segment_id = predictedSegmentId;
                } else {
                    read_segment_id();
                }
                for (int i = 0; i < Num_4x4_Blocks_Wide[MiSize]; ++i) AboveSegPredContext[MiCol + i] = static_cast<std::uint8_t>(seg_id_predicted);
                for (int i = 0; i < Num_4x4_Blocks_High[MiSize]; ++i) LeftSegPredContext[MiRow + i] = static_cast<std::uint8_t>(seg_id_predicted);
            } else {
                read_segment_id();
            }
        } else {
            segment_id = predictedSegmentId;
        }
    } else {
        segment_id = 0;
    }
}

void Tile::read_is_inter() {
    if (skip_mode) {
        is_inter = 1;
    } else if (seg_feature_active(SEG_LVL_REF_FRAME)) {
        is_inter = fh->FeatureData[segment_id][SEG_LVL_REF_FRAME] != INTRA_FRAME;
    } else if (seg_feature_active(SEG_LVL_GLOBALMV)) {
        is_inter = 1;
    } else {
        int ctx;
        if (AvailU && AvailL) {
            ctx = (LeftIntra && AboveIntra) ? 3 : (LeftIntra || AboveIntra);
        } else if (AvailU || AvailL) {
            ctx = 2 * (AvailU ? AboveIntra : LeftIntra);
        } else {
            ctx = 0;
        }
        is_inter = sd.read_symbol(cdf.IsInter[ctx], 2);
    }
}

void Tile::intra_block_mode_info() {
    RefFrame[0] = INTRA_FRAME;
    RefFrame[1] = NONE;
    YMode = sd.read_symbol(cdf.YMode[Size_Group[MiSize]], INTRA_MODES);
    intra_angle_info_y();
    if (HasChroma) {
        int cflAllowed;
        if (Lossless && get_plane_residual_size(MiSize, 1) == BLOCK_4X4) {
            cflAllowed = 1;
        } else if (!Lossless && imax(block_width(MiSize), block_height(MiSize)) <= 32) {
            cflAllowed = 1;
        } else {
            cflAllowed = 0;
        }
        if (cflAllowed) {
            UVMode = sd.read_symbol(cdf.UVModeCflAllowed[YMode], UV_INTRA_MODES_CFL_ALLOWED);
        } else {
            UVMode = sd.read_symbol(cdf.UVModeCflNotAllowed[YMode], UV_INTRA_MODES_CFL_NOT_ALLOWED);
        }
        if (UVMode == UV_CFL_PRED) read_cfl_alphas();
        intra_angle_info_uv();
    }
    PaletteSizeY = 0;
    PaletteSizeUV = 0;
    if (MiSize >= BLOCK_8X8 && block_width(MiSize) <= 64 && block_height(MiSize) <= 64 && fh->allow_screen_content_tools) palette_mode_info();
    filter_intra_mode_info();
}

namespace {

int has_nearmv_mode(int YMode) {
    return YMode == NEARMV || YMode == NEAR_NEARMV || YMode == NEAR_NEWMV || YMode == NEW_NEARMV;
}

} // namespace

void Tile::inter_block_mode_info() {
    PaletteSizeY = 0;
    PaletteSizeUV = 0;
    read_ref_frames();
    const int isCompound = RefFrame[1] > INTRA_FRAME;
    find_mv_stack(isCompound);
    if (skip_mode) {
        YMode = NEAREST_NEARESTMV;
    } else if (seg_feature_active(SEG_LVL_SKIP) || seg_feature_active(SEG_LVL_GLOBALMV)) {
        YMode = GLOBALMV;
    } else if (isCompound) {
        const int ctx = Compound_Mode_Ctx_Map[RefMvContext >> 1][imin(NewMvContext, COMP_NEWMV_CTXS - 1)];
        YMode = NEAREST_NEARESTMV + sd.read_symbol(cdf.CompoundMode[ctx], COMPOUND_MODES);
    } else {
        const int new_mv = sd.read_symbol(cdf.NewMv[NewMvContext], 2);
        if (new_mv == 0) {
            YMode = NEWMV;
        } else {
            const int zero_mv = sd.read_symbol(cdf.ZeroMv[ZeroMvContext], 2);
            if (zero_mv == 0) {
                YMode = GLOBALMV;
            } else {
                const int ref_mv = sd.read_symbol(cdf.RefMv[RefMvContext], 2);
                YMode = ref_mv == 0 ? NEARESTMV : NEARMV;
            }
        }
    }
    RefMvIdx = 0;
    if (YMode == NEWMV || YMode == NEW_NEWMV) {
        for (int idx = 0; idx < 2; ++idx) {
            if (NumMvFound > idx + 1) {
                const int drl_mode = sd.read_symbol(cdf.DrlMode[DrlCtxStack[idx]], 2);
                if (drl_mode == 0) {
                    RefMvIdx = idx;
                    break;
                }
                RefMvIdx = idx + 1;
            }
        }
    } else if (has_nearmv_mode(YMode)) {
        RefMvIdx = 1;
        for (int idx = 1; idx < 3; ++idx) {
            if (NumMvFound > idx + 1) {
                const int drl_mode = sd.read_symbol(cdf.DrlMode[DrlCtxStack[idx]], 2);
                if (drl_mode == 0) {
                    RefMvIdx = idx;
                    break;
                }
                RefMvIdx = idx + 1;
            }
        }
    }
    assign_mv(isCompound);
    read_interintra_mode(isCompound);
    read_motion_mode(isCompound);
    read_compound_type(isCompound);
    if (isCompound) AV1_STAT(ST_COMPOUND);
    if (fh->interpolation_filter == SWITCHABLE) {
        for (int dir = 0; dir < (seq->enable_dual_filter ? 2 : 1); ++dir) {
            if (needs_interp_filter()) {
                int ctx = ((dir & 1) * 2 + (RefFrame[1] > INTRA_FRAME)) * 4;
                int leftType = 3;
                int aboveType = 3;
                if (AvailL) {
                    const MiInfo& l = mi(MiRow, MiCol - 1);
                    if (l.ref_frame[0] == RefFrame[0] || l.ref_frame[1] == RefFrame[0]) leftType = l.interp_filter[dir];
                }
                if (AvailU) {
                    const MiInfo& a = mi(MiRow - 1, MiCol);
                    if (a.ref_frame[0] == RefFrame[0] || a.ref_frame[1] == RefFrame[0]) aboveType = a.interp_filter[dir];
                }
                if (leftType == aboveType) {
                    ctx += leftType;
                } else if (leftType == 3) {
                    ctx += aboveType;
                } else if (aboveType == 3) {
                    ctx += leftType;
                } else {
                    ctx += 3;
                }
                interp_filter[dir] = sd.read_symbol(cdf.InterpFilter[ctx], INTERP_FILTERS);
            } else {
                interp_filter[dir] = EIGHTTAP;
            }
        }
        if (!seq->enable_dual_filter) interp_filter[1] = interp_filter[0];
        if (interp_filter[0] != interp_filter[1]) AV1_STAT(ST_DUAL_FILTER);
        if (interp_filter[0] == EIGHTTAP_SMOOTH || interp_filter[1] == EIGHTTAP_SMOOTH) AV1_STAT(ST_INTERP_SMOOTH);
        if (interp_filter[0] == EIGHTTAP_SHARP || interp_filter[1] == EIGHTTAP_SHARP) AV1_STAT(ST_INTERP_SHARP);
    } else {
        for (int dir = 0; dir < 2; ++dir) interp_filter[dir] = fh->interpolation_filter;
    }
}

int Tile::needs_interp_filter() const noexcept {
    const int large = imin(block_width(MiSize), block_height(MiSize)) >= 8;
    if (skip_mode || motion_mode == LOCALWARP) {
        return 0;
    } else if (large && YMode == GLOBALMV) {
        return fh->GmType[RefFrame[0]] == TRANSLATION;
    } else if (large && YMode == GLOBAL_GLOBALMV) {
        return fh->GmType[RefFrame[0]] == TRANSLATION || fh->GmType[RefFrame[1]] == TRANSLATION;
    }
    return 1;
}

void Tile::filter_intra_mode_info() {
    use_filter_intra = 0;
    if (seq->enable_filter_intra && YMode == DC_PRED && PaletteSizeY == 0 && imax(block_width(MiSize), block_height(MiSize)) <= 32) {
        use_filter_intra = sd.read_symbol(cdf.FilterIntra[MiSize], 2);
        if (use_filter_intra) {
            filter_intra_mode = sd.read_symbol(cdf.FilterIntraMode, 5);
            AV1_STAT(ST_FILTER_INTRA);
        }
    }
}

int Tile::count_refs(int frameType) const noexcept {
    int c = 0;
    if (AvailU) {
        if (AboveRefFrame[0] == frameType) ++c;
        if (AboveRefFrame[1] == frameType) ++c;
    }
    if (AvailL) {
        if (LeftRefFrame[0] == frameType) ++c;
        if (LeftRefFrame[1] == frameType) ++c;
    }
    return c;
}

namespace {

int ref_count_ctx(int counts0, int counts1) {
    if (counts0 < counts1) return 0;
    if (counts0 == counts1) return 1;
    return 2;
}

int check_backward(int refFrame) {
    return refFrame >= BWDREF_FRAME && refFrame <= ALTREF_FRAME;
}

int is_samedir_ref_pair(int ref0, int ref1) {
    return (ref0 >= BWDREF_FRAME) == (ref1 >= BWDREF_FRAME);
}

} // namespace

void Tile::read_ref_frames() {
    if (skip_mode) {
        RefFrame[0] = fh->SkipModeFrame[0];
        RefFrame[1] = fh->SkipModeFrame[1];
        return;
    }
    if (seg_feature_active(SEG_LVL_REF_FRAME)) {
        RefFrame[0] = fh->FeatureData[segment_id][SEG_LVL_REF_FRAME];
        RefFrame[1] = NONE;
        return;
    }
    if (seg_feature_active(SEG_LVL_SKIP) || seg_feature_active(SEG_LVL_GLOBALMV)) {
        RefFrame[0] = LAST_FRAME;
        RefFrame[1] = NONE;
        return;
    }
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    int comp_mode = SINGLE_REFERENCE;
    if (fh->reference_select && imin(bw4, bh4) >= 2) {
        int ctx;
        if (AvailU && AvailL) {
            if (AboveSingle && LeftSingle) {
                ctx = check_backward(AboveRefFrame[0]) ^ check_backward(LeftRefFrame[0]);
            } else if (AboveSingle) {
                ctx = 2 + (check_backward(AboveRefFrame[0]) || AboveIntra);
            } else if (LeftSingle) {
                ctx = 2 + (check_backward(LeftRefFrame[0]) || LeftIntra);
            } else {
                ctx = 4;
            }
        } else if (AvailU) {
            ctx = AboveSingle ? check_backward(AboveRefFrame[0]) : 3;
        } else if (AvailL) {
            ctx = LeftSingle ? check_backward(LeftRefFrame[0]) : 3;
        } else {
            ctx = 1;
        }
        comp_mode = sd.read_symbol(cdf.CompMode[ctx], 2);
    }
    const int ctxSingleP1 = ref_count_ctx(count_refs(LAST_FRAME) + count_refs(LAST2_FRAME) + count_refs(LAST3_FRAME) + count_refs(GOLDEN_FRAME), count_refs(BWDREF_FRAME) + count_refs(ALTREF2_FRAME) + count_refs(ALTREF_FRAME));
    const int ctxCompRef = ref_count_ctx(count_refs(LAST_FRAME) + count_refs(LAST2_FRAME), count_refs(LAST3_FRAME) + count_refs(GOLDEN_FRAME));
    const int ctxCompRefP1 = ref_count_ctx(count_refs(LAST_FRAME), count_refs(LAST2_FRAME));
    const int ctxCompRefP2 = ref_count_ctx(count_refs(LAST3_FRAME), count_refs(GOLDEN_FRAME));
    const int ctxBwd = ref_count_ctx(count_refs(BWDREF_FRAME) + count_refs(ALTREF2_FRAME), count_refs(ALTREF_FRAME));
    const int ctxBwdP1 = ref_count_ctx(count_refs(BWDREF_FRAME), count_refs(ALTREF2_FRAME));
    if (comp_mode == COMPOUND_REFERENCE) {
        const int above0 = AboveRefFrame[0];
        const int above1 = AboveRefFrame[1];
        const int left0 = LeftRefFrame[0];
        const int left1 = LeftRefFrame[1];
        const int aboveCompInter = AvailU && !AboveIntra && !AboveSingle;
        const int leftCompInter = AvailL && !LeftIntra && !LeftSingle;
        const int aboveUniComp = aboveCompInter && is_samedir_ref_pair(above0, above1);
        const int leftUniComp = leftCompInter && is_samedir_ref_pair(left0, left1);
        int ctx;
        if (AvailU && !AboveIntra && AvailL && !LeftIntra) {
            const int samedir = is_samedir_ref_pair(above0, left0);
            if (!aboveCompInter && !leftCompInter) {
                ctx = 1 + 2 * samedir;
            } else if (!aboveCompInter) {
                ctx = !leftUniComp ? 1 : 3 + samedir;
            } else if (!leftCompInter) {
                ctx = !aboveUniComp ? 1 : 3 + samedir;
            } else {
                if (!aboveUniComp && !leftUniComp) {
                    ctx = 0;
                } else if (!aboveUniComp || !leftUniComp) {
                    ctx = 2;
                } else {
                    ctx = 3 + ((above0 == BWDREF_FRAME) == (left0 == BWDREF_FRAME));
                }
            }
        } else if (AvailU && AvailL) {
            if (aboveCompInter) {
                ctx = 1 + 2 * aboveUniComp;
            } else if (leftCompInter) {
                ctx = 1 + 2 * leftUniComp;
            } else {
                ctx = 2;
            }
        } else if (aboveCompInter) {
            ctx = 4 * aboveUniComp;
        } else if (leftCompInter) {
            ctx = 4 * leftUniComp;
        } else {
            ctx = 2;
        }
        const int comp_ref_type = sd.read_symbol(cdf.CompRefType[ctx], 2);
        if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
            AV1_STAT(ST_UNI_COMP);
            const int uni_comp_ref = sd.read_symbol(cdf.UniCompRef[ctxSingleP1][0], 2);
            if (uni_comp_ref) {
                RefFrame[0] = BWDREF_FRAME;
                RefFrame[1] = ALTREF_FRAME;
            } else {
                const int ctx1 = ref_count_ctx(count_refs(LAST2_FRAME), count_refs(LAST3_FRAME) + count_refs(GOLDEN_FRAME));
                const int uni_comp_ref_p1 = sd.read_symbol(cdf.UniCompRef[ctx1][1], 2);
                if (uni_comp_ref_p1) {
                    const int uni_comp_ref_p2 = sd.read_symbol(cdf.UniCompRef[ctxCompRefP2][2], 2);
                    if (uni_comp_ref_p2) {
                        RefFrame[0] = LAST_FRAME;
                        RefFrame[1] = GOLDEN_FRAME;
                    } else {
                        RefFrame[0] = LAST_FRAME;
                        RefFrame[1] = LAST3_FRAME;
                    }
                } else {
                    RefFrame[0] = LAST_FRAME;
                    RefFrame[1] = LAST2_FRAME;
                }
            }
        } else {
            const int comp_ref = sd.read_symbol(cdf.CompRef[ctxCompRef][0], 2);
            if (comp_ref == 0) {
                const int comp_ref_p1 = sd.read_symbol(cdf.CompRef[ctxCompRefP1][1], 2);
                RefFrame[0] = comp_ref_p1 ? LAST2_FRAME : LAST_FRAME;
            } else {
                const int comp_ref_p2 = sd.read_symbol(cdf.CompRef[ctxCompRefP2][2], 2);
                RefFrame[0] = comp_ref_p2 ? GOLDEN_FRAME : LAST3_FRAME;
            }
            const int comp_bwdref = sd.read_symbol(cdf.CompBwdRef[ctxBwd][0], 2);
            if (comp_bwdref == 0) {
                const int comp_bwdref_p1 = sd.read_symbol(cdf.CompBwdRef[ctxBwdP1][1], 2);
                RefFrame[1] = comp_bwdref_p1 ? ALTREF2_FRAME : BWDREF_FRAME;
            } else {
                RefFrame[1] = ALTREF_FRAME;
            }
        }
    } else {
        const int single_ref_p1 = sd.read_symbol(cdf.SingleRef[ctxSingleP1][0], 2);
        if (single_ref_p1) {
            const int single_ref_p2 = sd.read_symbol(cdf.SingleRef[ctxBwd][1], 2);
            if (single_ref_p2 == 0) {
                const int single_ref_p6 = sd.read_symbol(cdf.SingleRef[ctxBwdP1][5], 2);
                RefFrame[0] = single_ref_p6 ? ALTREF2_FRAME : BWDREF_FRAME;
            } else {
                RefFrame[0] = ALTREF_FRAME;
            }
        } else {
            const int single_ref_p3 = sd.read_symbol(cdf.SingleRef[ctxCompRef][2], 2);
            if (single_ref_p3) {
                const int single_ref_p5 = sd.read_symbol(cdf.SingleRef[ctxCompRefP2][4], 2);
                RefFrame[0] = single_ref_p5 ? GOLDEN_FRAME : LAST3_FRAME;
            } else {
                const int single_ref_p4 = sd.read_symbol(cdf.SingleRef[ctxCompRefP1][3], 2);
                RefFrame[0] = single_ref_p4 ? LAST2_FRAME : LAST_FRAME;
            }
        }
        RefFrame[1] = NONE;
    }
}

int Tile::get_mode(int refList) const noexcept {
    int compMode;
    if (refList == 0) {
        if (YMode < NEAREST_NEARESTMV) {
            compMode = YMode;
        } else if (YMode == NEW_NEWMV || YMode == NEW_NEARESTMV || YMode == NEW_NEARMV) {
            compMode = NEWMV;
        } else if (YMode == NEAREST_NEARESTMV || YMode == NEAREST_NEWMV) {
            compMode = NEARESTMV;
        } else if (YMode == NEAR_NEARMV || YMode == NEAR_NEWMV) {
            compMode = NEARMV;
        } else {
            compMode = GLOBALMV;
        }
    } else {
        if (YMode == NEW_NEWMV || YMode == NEAREST_NEWMV || YMode == NEAR_NEWMV) {
            compMode = NEWMV;
        } else if (YMode == NEAREST_NEARESTMV || YMode == NEW_NEARESTMV) {
            compMode = NEARESTMV;
        } else if (YMode == NEAR_NEARMV || YMode == NEW_NEARMV) {
            compMode = NEARMV;
        } else {
            compMode = GLOBALMV;
        }
    }
    return compMode;
}

void Tile::assign_mv(int isCompound) {
    for (int i = 0; i < 1 + isCompound; ++i) {
        int compMode;
        if (use_intrabc) {
            compMode = NEWMV;
        } else {
            compMode = get_mode(i);
        }
        if (use_intrabc) {
            PredMv[0] = RefStackMv[0][0];
            if (PredMv[0].row == 0 && PredMv[0].col == 0) PredMv[0] = RefStackMv[1][0];
            if (PredMv[0].row == 0 && PredMv[0].col == 0) {
                const int sbSize = seq->use_128x128_superblock ? BLOCK_128X128 : BLOCK_64X64;
                const int sbSize4 = Num_4x4_Blocks_High[sbSize];
                if (MiRow - sbSize4 < MiRowStart) {
                    PredMv[0] = make_mv(0, -(sbSize4 * MI_SIZE + INTRABC_DELAY_PIXELS) * 8);
                } else {
                    PredMv[0] = make_mv(-(sbSize4 * MI_SIZE * 8), 0);
                }
            }
        } else if (compMode == GLOBALMV) {
            PredMv[i] = GlobalMvs[i];
        } else {
            int pos = compMode == NEARESTMV ? 0 : RefMvIdx;
            if (compMode == NEWMV && NumMvFound <= 1) pos = 0;
            PredMv[i] = RefStackMv[pos][i];
        }
        if (compMode == NEWMV) {
            read_mv(i);
        } else {
            MvVal[i] = PredMv[i];
        }
    }
}

void Tile::read_mv(int ref) {
    int diffMv[2] = {0, 0};
    const int MvCtx = use_intrabc ? MV_INTRABC_CONTEXT : 0;
    const int mv_joint = sd.read_symbol(cdf.MvJoint[MvCtx], MV_JOINTS);
    if (fh->allow_high_precision_mv) AV1_STAT(ST_MV_JOINT_HP);
    if (mv_joint == MV_JOINT_HZVNZ || mv_joint == MV_JOINT_HNZVNZ) diffMv[0] = read_mv_component(MvCtx, 0);
    if (mv_joint == MV_JOINT_HNZVZ || mv_joint == MV_JOINT_HNZVNZ) diffMv[1] = read_mv_component(MvCtx, 1);
    MvVal[ref] = make_mv(PredMv[ref].row + diffMv[0], PredMv[ref].col + diffMv[1]);
}

int Tile::read_mv_component(int ctx, int comp) {
    const int mv_sign = sd.read_symbol(cdf.MvSign[ctx][comp], 2);
    const int mv_class = sd.read_symbol(cdf.MvClass[ctx][comp], MV_CLASSES);
    int mag;
    if (mv_class == 0) {
        const int mv_class0_bit = sd.read_symbol(cdf.MvClass0Bit[ctx][comp], 2);
        int mv_class0_fr;
        int mv_class0_hp;
        if (fh->force_integer_mv) {
            mv_class0_fr = 3;
        } else {
            mv_class0_fr = sd.read_symbol(cdf.MvClass0Fr[ctx][comp][mv_class0_bit], MV_JOINTS);
        }
        if (fh->allow_high_precision_mv) {
            mv_class0_hp = sd.read_symbol(cdf.MvClass0Hp[ctx][comp], 2);
        } else {
            mv_class0_hp = 1;
        }
        mag = ((mv_class0_bit << 3) | (mv_class0_fr << 1) | mv_class0_hp) + 1;
    } else {
        int dd = 0;
        for (int i = 0; i < mv_class; ++i) {
            const int mv_bit = sd.read_symbol(cdf.MvBit[ctx][comp][i], 2);
            dd |= mv_bit << i;
        }
        mag = CLASS0_SIZE << (mv_class + 2);
        int mv_fr;
        int mv_hp;
        if (fh->force_integer_mv) {
            mv_fr = 3;
        } else {
            mv_fr = sd.read_symbol(cdf.MvFr[ctx][comp], MV_JOINTS);
        }
        if (fh->allow_high_precision_mv) {
            mv_hp = sd.read_symbol(cdf.MvHp[ctx][comp], 2);
        } else {
            mv_hp = 1;
        }
        mag += ((dd << 3) | (mv_fr << 1) | mv_hp) + 1;
    }
    return mv_sign ? -mag : mag;
}

bool Tile::is_scaled(int refFrame) const noexcept {
    const SavedFrame* r = d->refs[fh->ref_frame_idx[refFrame - LAST_FRAME]].get();
    if (!r) return false;
    const int xScale = ((r->upscaled_width << REF_SCALE_SHIFT) + (fh->FrameWidth / 2)) / fh->FrameWidth;
    const int yScale = ((r->frame_height << REF_SCALE_SHIFT) + (fh->FrameHeight / 2)) / fh->FrameHeight;
    const int noScale = 1 << REF_SCALE_SHIFT;
    return xScale != noScale || yScale != noScale;
}

void Tile::read_motion_mode(int isCompound) {
    if (skip_mode) {
        motion_mode = SIMPLE;
        return;
    }
    if (!fh->is_motion_mode_switchable) {
        motion_mode = SIMPLE;
        return;
    }
    if (imin(block_width(MiSize), block_height(MiSize)) < 8) {
        motion_mode = SIMPLE;
        return;
    }
    if (!fh->force_integer_mv && (YMode == GLOBALMV || YMode == GLOBAL_GLOBALMV)) {
        if (fh->GmType[RefFrame[0]] > TRANSLATION) {
            motion_mode = SIMPLE;
            return;
        }
    }
    if (isCompound || RefFrame[1] == INTRA_FRAME || !has_overlappable_candidates()) {
        motion_mode = SIMPLE;
        return;
    }
    find_warp_samples();
    if (fh->force_integer_mv || NumSamples == 0 || !fh->allow_warped_motion || is_scaled(RefFrame[0])) {
        const int use_obmc = sd.read_symbol(cdf.UseObmc[MiSize], 2);
        motion_mode = use_obmc ? OBMC : SIMPLE;
    } else {
        motion_mode = sd.read_symbol(cdf.MotionMode[MiSize], MOTION_MODES);
    }
}

void Tile::read_interintra_mode(int isCompound) {
    if (!skip_mode && seq->enable_interintra_compound && !isCompound && MiSize >= BLOCK_8X8 && MiSize <= BLOCK_32X32) {
        const int ctx = Size_Group[MiSize] - 1;
        interintra = sd.read_symbol(cdf.InterIntra[ctx], 2);
        if (interintra) {
            interintra_mode = sd.read_symbol(cdf.InterIntraMode[ctx], INTERINTRA_MODES);
            RefFrame[1] = INTRA_FRAME;
            AngleDeltaY = 0;
            AngleDeltaUV = 0;
            use_filter_intra = 0;
            wedge_interintra = sd.read_symbol(cdf.WedgeInterIntra[MiSize], 2);
            if (wedge_interintra) {
                wedge_index = sd.read_symbol(cdf.WedgeIndex[MiSize], 16);
                wedge_sign = 0;
            }
        }
    } else {
        interintra = 0;
    }
}

void Tile::read_compound_type(int isCompound) {
    comp_group_idx = 0;
    compound_idx = 1;
    if (skip_mode) {
        compound_type = COMPOUND_AVERAGE;
        return;
    }
    if (isCompound) {
        const int n = Wedge_Bits[MiSize];
        if (seq->enable_masked_compound) {
            int ctx = 0;
            if (AvailU) {
                if (!AboveSingle) {
                    ctx += mi(MiRow - 1, MiCol).comp_group_idx;
                } else if (AboveRefFrame[0] == ALTREF_FRAME) {
                    ctx += 3;
                }
            }
            if (AvailL) {
                if (!LeftSingle) {
                    ctx += mi(MiRow, MiCol - 1).comp_group_idx;
                } else if (LeftRefFrame[0] == ALTREF_FRAME) {
                    ctx += 3;
                }
            }
            ctx = imin(5, ctx);
            comp_group_idx = sd.read_symbol(cdf.CompGroupIdx[ctx], 2);
        }
        if (comp_group_idx == 0) {
            if (seq->enable_jnt_comp) {
                const int fwd = iabs(d->get_relative_dist(fh->OrderHints[RefFrame[0]], fh->OrderHint));
                const int bck = iabs(d->get_relative_dist(fh->OrderHints[RefFrame[1]], fh->OrderHint));
                int ctx = (fwd == bck) ? 3 : 0;
                if (AvailU) {
                    if (!AboveSingle) {
                        ctx += mi(MiRow - 1, MiCol).compound_idx;
                    } else if (AboveRefFrame[0] == ALTREF_FRAME) {
                        ++ctx;
                    }
                }
                if (AvailL) {
                    if (!LeftSingle) {
                        ctx += mi(MiRow, MiCol - 1).compound_idx;
                    } else if (LeftRefFrame[0] == ALTREF_FRAME) {
                        ++ctx;
                    }
                }
                compound_idx = sd.read_symbol(cdf.CompoundIdx[ctx], 2);
                compound_type = compound_idx ? COMPOUND_AVERAGE : COMPOUND_DISTANCE;
            } else {
                compound_type = COMPOUND_AVERAGE;
            }
        } else {
            if (n == 0) {
                compound_type = COMPOUND_DIFFWTD;
            } else {
                compound_type = sd.read_symbol(cdf.CompoundType[MiSize], COMPOUND_TYPES);
            }
        }
        if (compound_type == COMPOUND_WEDGE) {
            wedge_index = sd.read_symbol(cdf.WedgeIndex[MiSize], 16);
            wedge_sign = sd.read_literal(1);
        } else if (compound_type == COMPOUND_DIFFWTD) {
            mask_type = sd.read_literal(1);
        }
    } else {
        if (interintra) {
            compound_type = wedge_interintra ? COMPOUND_WEDGE : COMPOUND_INTRA;
        } else {
            compound_type = COMPOUND_AVERAGE;
        }
    }
}

void Tile::intra_angle_info_y() {
    AngleDeltaY = 0;
    if (MiSize >= BLOCK_8X8 && YMode >= V_PRED && YMode <= D67_PRED) {
        const int angle_delta_y = sd.read_symbol(cdf.AngleDelta[YMode - V_PRED], 2 * MAX_ANGLE_DELTA + 1);
        AngleDeltaY = angle_delta_y - MAX_ANGLE_DELTA;
    }
}

void Tile::intra_angle_info_uv() {
    AngleDeltaUV = 0;
    if (MiSize >= BLOCK_8X8 && UVMode >= V_PRED && UVMode <= D67_PRED) {
        const int angle_delta_uv = sd.read_symbol(cdf.AngleDelta[UVMode - V_PRED], 2 * MAX_ANGLE_DELTA + 1);
        AngleDeltaUV = angle_delta_uv - MAX_ANGLE_DELTA;
    }
}

void Tile::read_cfl_alphas() {
    const int cfl_alpha_signs = sd.read_symbol(cdf.CflSign, CFL_JOINT_SIGNS);
    const int signU = (cfl_alpha_signs + 1) / 3;
    const int signV = (cfl_alpha_signs + 1) % 3;
    if (signU != CFL_SIGN_ZERO) {
        const int ctx = (signU - 1) * 3 + signV;
        CflAlphaU = 1 + sd.read_symbol(cdf.CflAlpha[ctx], CFL_ALPHABET_SIZE);
        if (signU == CFL_SIGN_NEG) CflAlphaU = -CflAlphaU;
    } else {
        CflAlphaU = 0;
    }
    if (signV != CFL_SIGN_ZERO) {
        const int ctx = (signV - 1) * 3 + signU;
        CflAlphaV = 1 + sd.read_symbol(cdf.CflAlpha[ctx], CFL_ALPHABET_SIZE);
        if (signV == CFL_SIGN_NEG) CflAlphaV = -CflAlphaV;
    } else {
        CflAlphaV = 0;
    }
}

int Tile::get_palette_cache(int plane, std::uint16_t* cache) {
    int aboveN = 0;
    if ((MiRow * MI_SIZE) % 64 && AvailU) aboveN = mi(MiRow - 1, MiCol).palette_size[plane];
    int leftN = 0;
    if (AvailL) leftN = mi(MiRow, MiCol - 1).palette_size[plane];
    const std::uint16_t* above = &AbovePaletteColors[plane][static_cast<std::size_t>(MiCol) * 8];
    const std::uint16_t* left = &LeftPaletteColors[plane][static_cast<std::size_t>(MiRow) * 8];
    int aboveIdx = 0;
    int leftIdx = 0;
    int n = 0;
    while (aboveIdx < aboveN && leftIdx < leftN) {
        const int aboveC = above[aboveIdx];
        const int leftC = left[leftIdx];
        if (leftC < aboveC) {
            if (n == 0 || leftC != cache[n - 1]) cache[n++] = static_cast<std::uint16_t>(leftC);
            ++leftIdx;
        } else {
            if (n == 0 || aboveC != cache[n - 1]) cache[n++] = static_cast<std::uint16_t>(aboveC);
            ++aboveIdx;
            if (leftC == aboveC) ++leftIdx;
        }
    }
    while (aboveIdx < aboveN) {
        const int val = above[aboveIdx++];
        if (n == 0 || val != cache[n - 1]) cache[n++] = static_cast<std::uint16_t>(val);
    }
    while (leftIdx < leftN) {
        const int val = left[leftIdx++];
        if (n == 0 || val != cache[n - 1]) cache[n++] = static_cast<std::uint16_t>(val);
    }
    return n;
}

void Tile::palette_mode_info() {
    const int BitDepth = seq->BitDepth;
    const int maxv = (1 << BitDepth) - 1;
    const int bsizeCtx = Mi_Width_Log2[MiSize] + Mi_Height_Log2[MiSize] - 2;
    std::uint16_t cache[16];
    if (YMode == DC_PRED) {
        int ctx = 0;
        if (AvailU && mi(MiRow - 1, MiCol).palette_size[0] > 0) ++ctx;
        if (AvailL && mi(MiRow, MiCol - 1).palette_size[0] > 0) ++ctx;
        const int has_palette_y = sd.read_symbol(cdf.PaletteYMode[bsizeCtx][ctx], 2);
        if (has_palette_y) {
            AV1_STAT(ST_PALETTE_Y);
            PaletteSizeY = sd.read_symbol(cdf.PaletteYSize[bsizeCtx], PALETTE_SIZES) + 2;
            const int cacheN = get_palette_cache(0, cache);
            if (cacheN) AV1_STAT(ST_PALETTE_CACHE);
            int idx = 0;
            for (int i = 0; i < cacheN && idx < PaletteSizeY; ++i) {
                if (sd.read_literal(1)) palette_colors_y[idx++] = cache[i];
            }
            if (idx < PaletteSizeY) palette_colors_y[idx++] = static_cast<std::uint16_t>(sd.read_literal(BitDepth));
            int paletteBits = 0;
            if (idx < PaletteSizeY) {
                const int minBits = BitDepth - 3;
                paletteBits = minBits + sd.read_literal(2);
            }
            while (idx < PaletteSizeY) {
                const int palette_delta_y = sd.read_literal(paletteBits) + 1;
                palette_colors_y[idx] = static_cast<std::uint16_t>(clip3(0, maxv, palette_colors_y[idx - 1] + palette_delta_y));
                const int range = (1 << BitDepth) - palette_colors_y[idx] - 1;
                paletteBits = imin(paletteBits, ceil_log2(static_cast<std::uint32_t>(range)));
                ++idx;
            }
            std::sort(palette_colors_y, palette_colors_y + PaletteSizeY);
        }
    }
    if (HasChroma && UVMode == DC_PRED) {
        const int ctx = PaletteSizeY > 0 ? 1 : 0;
        const int has_palette_uv = sd.read_symbol(cdf.PaletteUVMode[ctx], 2);
        if (has_palette_uv) {
            AV1_STAT(ST_PALETTE_UV);
            PaletteSizeUV = sd.read_symbol(cdf.PaletteUVSize[bsizeCtx], PALETTE_SIZES) + 2;
            const int cacheN = get_palette_cache(1, cache);
            int idx = 0;
            for (int i = 0; i < cacheN && idx < PaletteSizeUV; ++i) {
                if (sd.read_literal(1)) palette_colors_u[idx++] = cache[i];
            }
            if (idx < PaletteSizeUV) palette_colors_u[idx++] = static_cast<std::uint16_t>(sd.read_literal(BitDepth));
            int paletteBits = 0;
            if (idx < PaletteSizeUV) {
                const int minBits = BitDepth - 3;
                paletteBits = minBits + sd.read_literal(2);
            }
            while (idx < PaletteSizeUV) {
                const int palette_delta_u = sd.read_literal(paletteBits);
                palette_colors_u[idx] = static_cast<std::uint16_t>(clip3(0, maxv, palette_colors_u[idx - 1] + palette_delta_u));
                const int range = (1 << BitDepth) - palette_colors_u[idx];
                paletteBits = imin(paletteBits, ceil_log2(static_cast<std::uint32_t>(range)));
                ++idx;
            }
            std::sort(palette_colors_u, palette_colors_u + PaletteSizeUV);
            const int delta_encode = sd.read_literal(1);
            if (delta_encode) {
                const int minBits = BitDepth - 4;
                const int maxVal = 1 << BitDepth;
                const int bits = minBits + sd.read_literal(2);
                palette_colors_v[0] = static_cast<std::uint16_t>(sd.read_literal(BitDepth));
                for (idx = 1; idx < PaletteSizeUV; ++idx) {
                    int palette_delta_v = sd.read_literal(bits);
                    if (palette_delta_v) {
                        if (sd.read_literal(1)) palette_delta_v = -palette_delta_v;
                    }
                    int val = palette_colors_v[idx - 1] + palette_delta_v;
                    if (val < 0) val += maxVal;
                    if (val >= maxVal) val -= maxVal;
                    palette_colors_v[idx] = static_cast<std::uint16_t>(clip3(0, maxv, val));
                }
            } else {
                for (idx = 0; idx < PaletteSizeUV; ++idx) palette_colors_v[idx] = static_cast<std::uint16_t>(sd.read_literal(BitDepth));
            }
        }
    }
}

void Tile::get_palette_color_context(std::uint8_t (*colorMap)[64], int r, int c, int n, int* order, int* ctxOut) {
    int scores[PALETTE_COLORS];
    for (int i = 0; i < PALETTE_COLORS; ++i) {
        scores[i] = 0;
        order[i] = i;
    }
    if (c > 0) scores[colorMap[r][c - 1]] += 2;
    if (r > 0 && c > 0) scores[colorMap[r - 1][c - 1]] += 1;
    if (r > 0) scores[colorMap[r - 1][c]] += 2;
    for (int i = 0; i < PALETTE_NUM_NEIGHBORS; ++i) {
        int maxScore = scores[i];
        int maxIdx = i;
        for (int j = i + 1; j < n; ++j) {
            if (scores[j] > maxScore) {
                maxScore = scores[j];
                maxIdx = j;
            }
        }
        if (maxIdx != i) {
            maxScore = scores[maxIdx];
            const int maxColorOrder = order[maxIdx];
            for (int k = maxIdx; k > i; --k) {
                scores[k] = scores[k - 1];
                order[k] = order[k - 1];
            }
            scores[i] = maxScore;
            order[i] = maxColorOrder;
        }
    }
    int hash = 0;
    for (int i = 0; i < PALETTE_NUM_NEIGHBORS; ++i) hash += scores[i] * Palette_Color_Hash_Multipliers[i];
    int ctx = Palette_Color_Context[hash];
    if (ctx < 0) ctx = 0;
    *ctxOut = ctx;
}

void Tile::palette_tokens() {
    int blockHeight = block_height(MiSize);
    int blockWidth = block_width(MiSize);
    int onscreenHeight = imin(blockHeight, (fh->MiRows - MiRow) * MI_SIZE);
    int onscreenWidth = imin(blockWidth, (fh->MiCols - MiCol) * MI_SIZE);
    int order[PALETTE_COLORS];
    int ctx;
    if (PaletteSizeY) {
        ColorMapY[0][0] = static_cast<std::uint8_t>(sd.read_ns(PaletteSizeY));
        for (int i = 1; i < onscreenHeight + onscreenWidth - 1; ++i) {
            for (int j = imin(i, onscreenWidth - 1); j >= imax(0, i - onscreenHeight + 1); --j) {
                get_palette_color_context(ColorMapY, i - j, j, PaletteSizeY, order, &ctx);
                const int v = sd.read_symbol(cdf.PaletteYColor[PaletteSizeY - 2][ctx], PaletteSizeY);
                ColorMapY[i - j][j] = static_cast<std::uint8_t>(order[v]);
            }
        }
        for (int i = 0; i < onscreenHeight; ++i) {
            for (int j = onscreenWidth; j < blockWidth; ++j) ColorMapY[i][j] = ColorMapY[i][onscreenWidth - 1];
        }
        for (int i = onscreenHeight; i < blockHeight; ++i) {
            for (int j = 0; j < blockWidth; ++j) ColorMapY[i][j] = ColorMapY[onscreenHeight - 1][j];
        }
    }
    if (PaletteSizeUV) {
        ColorMapUV[0][0] = static_cast<std::uint8_t>(sd.read_ns(PaletteSizeUV));
        blockHeight = blockHeight >> seq->subsampling_y;
        blockWidth = blockWidth >> seq->subsampling_x;
        onscreenHeight = onscreenHeight >> seq->subsampling_y;
        onscreenWidth = onscreenWidth >> seq->subsampling_x;
        if (blockWidth < 4) {
            blockWidth += 2;
            onscreenWidth += 2;
        }
        if (blockHeight < 4) {
            blockHeight += 2;
            onscreenHeight += 2;
        }
        for (int i = 1; i < onscreenHeight + onscreenWidth - 1; ++i) {
            for (int j = imin(i, onscreenWidth - 1); j >= imax(0, i - onscreenHeight + 1); --j) {
                get_palette_color_context(ColorMapUV, i - j, j, PaletteSizeUV, order, &ctx);
                const int v = sd.read_symbol(cdf.PaletteUVColor[PaletteSizeUV - 2][ctx], PaletteSizeUV);
                ColorMapUV[i - j][j] = static_cast<std::uint8_t>(order[v]);
            }
        }
        for (int i = 0; i < onscreenHeight; ++i) {
            for (int j = onscreenWidth; j < blockWidth; ++j) ColorMapUV[i][j] = ColorMapUV[i][onscreenWidth - 1];
        }
        for (int i = onscreenHeight; i < blockHeight; ++i) {
            for (int j = 0; j < blockWidth; ++j) ColorMapUV[i][j] = ColorMapUV[onscreenHeight - 1][j];
        }
    }
}

int Tile::get_tx_set(int txSz) const noexcept {
    const int txSzSqr = Tx_Size_Sqr[txSz];
    const int txSzSqrUp = Tx_Size_Sqr_Up[txSz];
    if (txSzSqrUp > TX_32X32) return TX_SET_DCTONLY;
    if (is_inter) {
        if (fh->reduced_tx_set || txSzSqrUp == TX_32X32) return TX_SET_INTER_3;
        if (txSzSqr == TX_16X16) return TX_SET_INTER_2;
        return TX_SET_INTER_1;
    }
    if (txSzSqrUp == TX_32X32) return TX_SET_DCTONLY;
    if (fh->reduced_tx_set) return TX_SET_INTRA_2;
    if (txSzSqr == TX_16X16) return TX_SET_INTRA_2;
    return TX_SET_INTRA_1;
}

int Tile::compute_tx_type(int plane, int txSz, int blockX, int blockY) {
    const int txSzSqrUp = Tx_Size_Sqr_Up[txSz];
    if (Lossless || txSzSqrUp > TX_32X32) return DCT_DCT;
    const int txSet = get_tx_set(txSz);
    if (plane == 0) return mi(blockY, blockX).tx_type;
    if (is_inter) {
        const int x4 = imax(MiCol, blockX << seq->subsampling_x);
        const int y4 = imax(MiRow, blockY << seq->subsampling_y);
        const int txType = mi(y4, x4).tx_type;
        if (!Tx_Type_In_Set_Inter[txSet][txType]) return DCT_DCT;
        return txType;
    }
    const int txType = Mode_To_Txfm[UVMode];
    if (!Tx_Type_In_Set_Intra[txSet][txType]) return DCT_DCT;
    return txType;
}

void Tile::transform_type(int x4, int y4, int txSz) {
    const int set = get_tx_set(txSz);
    int TxType;
    if (set > 0 && (fh->segmentation_enabled ? get_qindex(1, segment_id) : fh->base_q_idx) > 0) {
        if (is_inter) {
            if (set == TX_SET_INTER_1) {
                TxType = Tx_Type_Inter_Inv_Set1[sd.read_symbol(cdf.InterTxTypeSet1[Tx_Size_Sqr[txSz]], 16)];
            } else if (set == TX_SET_INTER_2) {
                TxType = Tx_Type_Inter_Inv_Set2[sd.read_symbol(cdf.InterTxTypeSet2, 12)];
            } else {
                TxType = Tx_Type_Inter_Inv_Set3[sd.read_symbol(cdf.InterTxTypeSet3[Tx_Size_Sqr[txSz]], 2)];
            }
        } else {
            const int intraDir = use_filter_intra ? Filter_Intra_Mode_To_Intra_Dir[filter_intra_mode] : YMode;
            if (set == TX_SET_INTRA_1) {
                TxType = Tx_Type_Intra_Inv_Set1[sd.read_symbol(cdf.IntraTxTypeSet1[Tx_Size_Sqr[txSz]][intraDir], 7)];
            } else {
                TxType = Tx_Type_Intra_Inv_Set2[sd.read_symbol(cdf.IntraTxTypeSet2[Tx_Size_Sqr[txSz]][intraDir], 5)];
            }
        }
    } else {
        TxType = DCT_DCT;
    }
    for (int j = 0; j < (Tx_Height[txSz] >> 2); ++j) {
        MiInfo* m = &mi(y4 + j, x4);
        for (int i = 0; i < (Tx_Width[txSz] >> 2); ++i) m[i].tx_type = static_cast<std::uint8_t>(TxType);
    }
}

namespace {

struct ScanTables {
    std::uint16_t defaults[TX_SIZES_ALL][1024];
    std::uint16_t mrow[TX_SIZES_ALL][256];
    std::uint16_t mcol[TX_SIZES_ALL][256];
    ScanTables() {
        std::memset(this, 0, sizeof(*this));
        auto put = [](std::uint16_t* dst, const auto* src, int n) {
            for (int i = 0; i < n; ++i) dst[i] = static_cast<std::uint16_t>(src[i]);
        };
        put(defaults[TX_4X4], Default_Scan_4x4, 16);
        put(defaults[TX_4X8], Default_Scan_4x8, 32);
        put(defaults[TX_8X4], Default_Scan_8x4, 32);
        put(defaults[TX_8X8], Default_Scan_8x8, 64);
        put(defaults[TX_8X16], Default_Scan_8x16, 128);
        put(defaults[TX_16X8], Default_Scan_16x8, 128);
        put(defaults[TX_16X16], Default_Scan_16x16, 256);
        put(defaults[TX_16X32], Default_Scan_16x32, 512);
        put(defaults[TX_32X16], Default_Scan_32x16, 512);
        put(defaults[TX_4X16], Default_Scan_4x16, 64);
        put(defaults[TX_16X4], Default_Scan_16x4, 64);
        put(defaults[TX_8X32], Default_Scan_8x32, 256);
        put(defaults[TX_32X8], Default_Scan_32x8, 256);
        put(defaults[TX_32X32], Default_Scan_32x32, 1024);
        put(defaults[TX_64X64], Default_Scan_32x32, 1024);
        put(defaults[TX_32X64], Default_Scan_32x32, 1024);
        put(defaults[TX_64X32], Default_Scan_32x32, 1024);
        put(defaults[TX_16X64], Default_Scan_16x32, 512);
        put(defaults[TX_64X16], Default_Scan_32x16, 512);
        put(mrow[TX_4X4], Mrow_Scan_4x4, 16);
        put(mrow[TX_4X8], Mrow_Scan_4x8, 32);
        put(mrow[TX_8X4], Mrow_Scan_8x4, 32);
        put(mrow[TX_8X8], Mrow_Scan_8x8, 64);
        put(mrow[TX_8X16], Mrow_Scan_8x16, 128);
        put(mrow[TX_16X8], Mrow_Scan_16x8, 128);
        put(mrow[TX_16X16], Mrow_Scan_16x16, 256);
        put(mrow[TX_4X16], Mrow_Scan_4x16, 64);
        put(mrow[TX_16X4], Mrow_Scan_16x4, 64);
        put(mcol[TX_4X4], Mcol_Scan_4x4, 16);
        put(mcol[TX_4X8], Mcol_Scan_4x8, 32);
        put(mcol[TX_8X4], Mcol_Scan_8x4, 32);
        put(mcol[TX_8X8], Mcol_Scan_8x8, 64);
        put(mcol[TX_8X16], Mcol_Scan_8x16, 128);
        put(mcol[TX_16X8], Mcol_Scan_16x8, 128);
        put(mcol[TX_16X16], Mcol_Scan_16x16, 256);
        put(mcol[TX_4X16], Mcol_Scan_4x16, 64);
        put(mcol[TX_16X4], Mcol_Scan_16x4, 64);
    }
};

const ScanTables& scans() {
    static const ScanTables t;
    return t;
}

const std::uint16_t* select_scan(int txSz, int txType) {
    const ScanTables& s = scans();
    if (txSz == TX_16X64 || txSz == TX_64X16 || Tx_Size_Sqr_Up[txSz] == TX_64X64) return s.defaults[txSz];
    if (txType == IDTX) return s.defaults[txSz];
    const bool preferRow = txType == V_DCT || txType == V_ADST || txType == V_FLIPADST;
    const bool preferCol = txType == H_DCT || txType == H_ADST || txType == H_FLIPADST;
    if (preferRow) {
        if (Tx_Width[txSz] <= 16 && Tx_Height[txSz] <= 16) return s.mrow[txSz];
    } else if (preferCol) {
        if (Tx_Width[txSz] <= 16 && Tx_Height[txSz] <= 16) return s.mcol[txSz];
    }
    return s.defaults[txSz];
}

int get_tx_class(int txType) {
    if (txType == V_DCT || txType == V_ADST || txType == V_FLIPADST) return TX_CLASS_VERT;
    if (txType == H_DCT || txType == H_ADST || txType == H_FLIPADST) return TX_CLASS_HORIZ;
    return TX_CLASS_2D;
}

} // namespace

int Tile::coeffs(int plane, int startX, int startY, int txSz) {
    const int x4 = startX >> 2;
    const int y4 = startY >> 2;
    const int w4 = Tx_Width[txSz] >> 2;
    const int h4 = Tx_Height[txSz] >> 2;
    const int txSzCtx = (Tx_Size_Sqr[txSz] + Tx_Size_Sqr_Up[txSz] + 1) >> 1;
    const int ptype = plane > 0;
    const int segEob = (txSz == TX_16X64 || txSz == TX_64X16) ? 512 : imin(1024, Tx_Width[txSz] * Tx_Height[txSz]);
    std::memset(Quant, 0, sizeof(std::int32_t) * static_cast<std::size_t>(segEob));
    int eob = 0;
    int culLevel = 0;
    int dcCategory = 0;
    int maxX4 = fh->MiCols;
    int maxY4 = fh->MiRows;
    if (plane > 0) {
        maxX4 = maxX4 >> seq->subsampling_x;
        maxY4 = maxY4 >> seq->subsampling_y;
    }
    const int w = Tx_Width[txSz];
    const int h = Tx_Height[txSz];
    int allZeroCtx;
    {
        const int bsize = get_plane_residual_size(MiSize, plane);
        const int bw = block_width(bsize);
        const int bh = block_height(bsize);
        if (plane == 0) {
            int top = 0;
            int left = 0;
            for (int k = 0; k < w4; ++k) {
                if (x4 + k < maxX4) top = imax(top, AboveLevelContext[plane][x4 + k]);
            }
            for (int k = 0; k < h4; ++k) {
                if (y4 + k < maxY4) left = imax(left, LeftLevelContext[plane][y4 + k]);
            }
            top = imin(top, 255);
            left = imin(left, 255);
            if (bw == w && bh == h) {
                allZeroCtx = 0;
            } else if (top == 0 && left == 0) {
                allZeroCtx = 1;
            } else if (top == 0 || left == 0) {
                allZeroCtx = 2 + (imax(top, left) > 3);
            } else if (imax(top, left) <= 3) {
                allZeroCtx = 4;
            } else if (imin(top, left) <= 3) {
                allZeroCtx = 5;
            } else {
                allZeroCtx = 6;
            }
        } else {
            int above = 0;
            int left = 0;
            for (int i = 0; i < w4; ++i) {
                if (x4 + i < maxX4) {
                    above |= AboveLevelContext[plane][x4 + i];
                    above |= AboveDcContext[plane][x4 + i];
                }
            }
            for (int i = 0; i < h4; ++i) {
                if (y4 + i < maxY4) {
                    left |= LeftLevelContext[plane][y4 + i];
                    left |= LeftDcContext[plane][y4 + i];
                }
            }
            allZeroCtx = (above != 0) + (left != 0);
            allZeroCtx += 7;
            if (bw * bh > w * h) allZeroCtx += 3;
        }
    }
    const int all_zero = sd.read_symbol(cdf.TxbSkip[txSzCtx][allZeroCtx], 2);
    if (all_zero) {
        if (plane == 0) {
            for (int j = 0; j < h4; ++j) {
                MiInfo* m = &mi(y4 + j, x4);
                for (int i = 0; i < w4; ++i) m[i].tx_type = DCT_DCT;
            }
        }
    } else {
        if (plane == 0) transform_type(x4, y4, txSz);
        PlaneTxType = compute_tx_type(plane, txSz, x4, y4);
        if (Tx_Size_Sqr_Up[txSz] == TX_64X64) AV1_STAT(ST_TX64);
        if (Tx_Width[txSz] != Tx_Height[txSz]) AV1_STAT(ST_RECT_TX);
        if (PlaneTxType == IDTX) AV1_STAT(ST_IDTX);
        if (PlaneTxType >= V_DCT) AV1_STAT(ST_TX_1D);
        if ((PlaneTxType >= FLIPADST_DCT && PlaneTxType <= FLIPADST_ADST) || PlaneTxType == V_FLIPADST || PlaneTxType == H_FLIPADST) AV1_STAT(ST_FLIPADST);
        if (PlaneTxType != DCT_DCT && PlaneTxType != IDTX) AV1_STAT(ST_ADST);
        if (Lossless) AV1_STAT(ST_WHT);
        const std::uint16_t* scan = select_scan(txSz, PlaneTxType);
        const int txClass = get_tx_class(PlaneTxType);
        const int eobMultisize = imin(Tx_Width_Log2[txSz], 5) + imin(Tx_Height_Log2[txSz], 5) - 4;
        const int eobCtx = txClass == TX_CLASS_2D ? 0 : 1;
        int eobPt;
        switch (eobMultisize) {
        case 0: eobPt = sd.read_symbol(cdf.EobPt16[ptype][eobCtx], 5) + 1; break;
        case 1: eobPt = sd.read_symbol(cdf.EobPt32[ptype][eobCtx], 6) + 1; break;
        case 2: eobPt = sd.read_symbol(cdf.EobPt64[ptype][eobCtx], 7) + 1; break;
        case 3: eobPt = sd.read_symbol(cdf.EobPt128[ptype][eobCtx], 8) + 1; break;
        case 4: eobPt = sd.read_symbol(cdf.EobPt256[ptype][eobCtx], 9) + 1; break;
        case 5: eobPt = sd.read_symbol(cdf.EobPt512[ptype], 10) + 1; break;
        default: eobPt = sd.read_symbol(cdf.EobPt1024[ptype], 11) + 1; break;
        }
        eob = (eobPt < 2) ? eobPt : ((1 << (eobPt - 2)) + 1);
        int eobShift = imax(-1, eobPt - 3);
        if (eobShift >= 0) {
            const int eob_extra = sd.read_symbol(cdf.EobExtra[txSzCtx][ptype][eobPt - 3], 2);
            if (eob_extra) eob += (1 << eobShift);
            for (int i = 1; i < imax(0, eobPt - 2); ++i) {
                eobShift = imax(0, eobPt - 2) - 1 - i;
                if (sd.read_literal(1)) eob += (1 << eobShift);
            }
        }
        if (eob > segEob) eob = segEob;
        const int adjTxSz = Adjusted_Tx_Size[txSz];
        const int bwl = Tx_Width_Log2[adjTxSz];
        const int width = 1 << bwl;
        const int height = Tx_Height[adjTxSz];
        for (int c = eob - 1; c >= 0; --c) {
            const int pos = scan[c];
            int level;
            const int row = pos >> bwl;
            const int col = pos - (row << bwl);
            if (c == eob - 1) {
                int ctx;
                if (c == 0) {
                    ctx = SIG_COEF_CONTEXTS - 4;
                } else if (c <= (height << bwl) / 8) {
                    ctx = SIG_COEF_CONTEXTS - 3;
                } else if (c <= (height << bwl) / 4) {
                    ctx = SIG_COEF_CONTEXTS - 2;
                } else {
                    ctx = SIG_COEF_CONTEXTS - 1;
                }
                ctx = ctx - SIG_COEF_CONTEXTS + SIG_COEF_CONTEXTS_EOB;
                level = sd.read_symbol(cdf.CoeffBaseEob[txSzCtx][ptype][ctx], 3) + 1;
            } else {
                int mag = 0;
                for (int idx = 0; idx < SIG_REF_DIFF_OFFSET_NUM; ++idx) {
                    const int refRow = row + Sig_Ref_Diff_Offset[txClass][idx][0];
                    const int refCol = col + Sig_Ref_Diff_Offset[txClass][idx][1];
                    if (refRow >= 0 && refCol >= 0 && refRow < height && refCol < width) mag += imin(iabs(Quant[(refRow << bwl) + refCol]), 3);
                }
                int ctx = imin((mag + 1) >> 1, 4);
                if (txClass == TX_CLASS_2D) {
                    if (row == 0 && col == 0) {
                        ctx = 0;
                    } else {
                        ctx += Coeff_Base_Ctx_Offset[txSz][imin(row, 4)][imin(col, 4)];
                    }
                } else {
                    const int idx = txClass == TX_CLASS_VERT ? row : col;
                    ctx += Coeff_Base_Pos_Ctx_Offset[imin(idx, 2)];
                }
                level = sd.read_symbol(cdf.CoeffBase[txSzCtx][ptype][ctx], 4);
            }
            if (level > NUM_BASE_LEVELS) {
                int mag = 0;
                for (int idx = 0; idx < 3; ++idx) {
                    const int refRow = row + Mag_Ref_Offset_With_Tx_Class[txClass][idx][0];
                    const int refCol = col + Mag_Ref_Offset_With_Tx_Class[txClass][idx][1];
                    if (refRow >= 0 && refCol >= 0 && refRow < height && refCol < width) mag += imin(Quant[refRow * width + refCol], COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1);
                }
                mag = imin((mag + 1) >> 1, 6);
                int ctx;
                if (pos == 0) {
                    ctx = mag;
                } else if (txClass == 0) {
                    ctx = (row < 2 && col < 2) ? mag + 7 : mag + 14;
                } else if (txClass == 1) {
                    ctx = col == 0 ? mag + 7 : mag + 14;
                } else {
                    ctx = row == 0 ? mag + 7 : mag + 14;
                }
                std::uint16_t* brcdf = cdf.CoeffBr[imin(txSzCtx, TX_32X32)][ptype][ctx];
                for (int idx = 0; idx < COEFF_BASE_RANGE / (BR_CDF_SIZE - 1); ++idx) {
                    const int coeff_br = sd.read_symbol(brcdf, BR_CDF_SIZE);
                    level += coeff_br;
                    if (coeff_br < (BR_CDF_SIZE - 1)) break;
                }
            }
            Quant[pos] = level;
        }
        for (int c = 0; c < eob; ++c) {
            const int pos = scan[c];
            int sign = 0;
            if (Quant[pos] != 0) {
                if (c == 0) {
                    int dcSign = 0;
                    for (int k = 0; k < w4; ++k) {
                        if (x4 + k < maxX4) {
                            const int s = AboveDcContext[plane][x4 + k];
                            if (s == 1) {
                                --dcSign;
                            } else if (s == 2) {
                                ++dcSign;
                            }
                        }
                    }
                    for (int k = 0; k < h4; ++k) {
                        if (y4 + k < maxY4) {
                            const int s = LeftDcContext[plane][y4 + k];
                            if (s == 1) {
                                --dcSign;
                            } else if (s == 2) {
                                ++dcSign;
                            }
                        }
                    }
                    const int ctx = dcSign < 0 ? 1 : (dcSign > 0 ? 2 : 0);
                    sign = sd.read_symbol(cdf.DcSign[ptype][ctx], 2);
                } else {
                    sign = sd.read_literal(1);
                }
            }
            if (Quant[pos] > (NUM_BASE_LEVELS + COEFF_BASE_RANGE)) {
                int length = 0;
                int golomb_length_bit;
                do {
                    ++length;
                    golomb_length_bit = sd.read_literal(1);
                    if (length > 32) break;
                } while (!golomb_length_bit);
                std::uint32_t x = 1;
                for (int i = length - 2; i >= 0; --i) x = (x << 1) | static_cast<std::uint32_t>(sd.read_literal(1));
                Quant[pos] = static_cast<std::int32_t>((x + COEFF_BASE_RANGE + NUM_BASE_LEVELS) & 0x7FFFFFFFu);
            }
            if (pos == 0 && Quant[pos] > 0) dcCategory = sign ? 1 : 2;
            Quant[pos] = Quant[pos] & 0xFFFFF;
            culLevel += Quant[pos];
            if (culLevel > (1 << 24)) culLevel = 1 << 24;
            if (sign) Quant[pos] = -Quant[pos];
        }
        culLevel = imin(63, culLevel);
    }
    for (int i = 0; i < w4; ++i) {
        AboveLevelContext[plane][x4 + i] = static_cast<std::uint8_t>(culLevel);
        AboveDcContext[plane][x4 + i] = static_cast<std::uint8_t>(dcCategory);
    }
    for (int i = 0; i < h4; ++i) {
        LeftLevelContext[plane][y4 + i] = static_cast<std::uint8_t>(culLevel);
        LeftDcContext[plane][y4 + i] = static_cast<std::uint8_t>(dcCategory);
    }
    return eob;
}

int Tile::get_tx_size(int plane, int txSz) const noexcept {
    if (plane == 0) return txSz;
    const int uvTx = Max_Tx_Size_Rect[get_plane_residual_size(MiSize, plane)];
    if (Tx_Width[uvTx] == 64 || Tx_Height[uvTx] == 64) {
        if (Tx_Width[uvTx] == 16) return TX_16X32;
        if (Tx_Height[uvTx] == 16) return TX_32X16;
        return TX_32X32;
    }
    return uvTx;
}

void Tile::residual() {
    const int sbMask = seq->use_128x128_superblock ? 31 : 15;
    const int widthChunks = imax(1, block_width(MiSize) >> 6);
    const int heightChunks = imax(1, block_height(MiSize) >> 6);
    const int miSizeChunk = (widthChunks > 1 || heightChunks > 1) ? BLOCK_64X64 : MiSize;
    for (int chunkY = 0; chunkY < heightChunks; ++chunkY) {
        for (int chunkX = 0; chunkX < widthChunks; ++chunkX) {
            const int miRowChunk = MiRow + (chunkY << 4);
            const int miColChunk = MiCol + (chunkX << 4);
            (void)sbMask;
            for (int plane = 0; plane < 1 + HasChroma * 2; ++plane) {
                const int txSz = Lossless ? TX_4X4 : get_tx_size(plane, TxSize);
                const int stepX = Tx_Width[txSz] >> 2;
                const int stepY = Tx_Height[txSz] >> 2;
                const int planeSz = get_plane_residual_size(miSizeChunk, plane);
                const int num4x4W = Num_4x4_Blocks_Wide[planeSz];
                const int num4x4H = Num_4x4_Blocks_High[planeSz];
                const int subX = plane > 0 ? seq->subsampling_x : 0;
                const int subY = plane > 0 ? seq->subsampling_y : 0;
                const int baseX = (miColChunk >> subX) * MI_SIZE;
                const int baseY = (miRowChunk >> subY) * MI_SIZE;
                if (is_inter && !Lossless && !plane) {
                    transform_tree(baseX, baseY, num4x4W * 4, num4x4H * 4);
                } else {
                    const int baseXBlock = (MiCol >> subX) * MI_SIZE;
                    const int baseYBlock = (MiRow >> subY) * MI_SIZE;
                    for (int y = 0; y < num4x4H; y += stepY) {
                        for (int x = 0; x < num4x4W; x += stepX) transform_block(plane, baseXBlock, baseYBlock, txSz, x + ((chunkX << 4) >> subX), y + ((chunkY << 4) >> subY));
                    }
                }
            }
        }
    }
}

void Tile::transform_block(int plane, int baseX, int baseY, int txSz, int x, int y) {
    const int startX = baseX + 4 * x;
    const int startY = baseY + 4 * y;
    const int subX = plane > 0 ? seq->subsampling_x : 0;
    const int subY = plane > 0 ? seq->subsampling_y : 0;
    const int row = (startY << subY) >> MI_SIZE_LOG2;
    const int col = (startX << subX) >> MI_SIZE_LOG2;
    const int sbMask = seq->use_128x128_superblock ? 31 : 15;
    const int subBlockMiRow = row & sbMask;
    const int subBlockMiCol = col & sbMask;
    const int stepX = Tx_Width[txSz] >> MI_SIZE_LOG2;
    const int stepY = Tx_Height[txSz] >> MI_SIZE_LOG2;
    const int maxX = (fh->MiCols * MI_SIZE) >> subX;
    const int maxY = (fh->MiRows * MI_SIZE) >> subY;
    if (startX >= maxX || startY >= maxY) return;
    if (!is_inter) {
        if ((plane == 0 && PaletteSizeY) || (plane != 0 && PaletteSizeUV)) {
            predict_palette(plane, startX, startY, x, y, txSz);
        } else {
            const int isCfl = plane > 0 && UVMode == UV_CFL_PRED;
            int mode;
            if (plane == 0) {
                mode = YMode;
            } else {
                mode = isCfl ? DC_PRED : UVMode;
            }
            const int log2W = Tx_Width_Log2[txSz];
            const int log2H = Tx_Height_Log2[txSz];
            predict_intra(plane, startX, startY, (plane == 0 ? AvailL : AvailLChroma) || x > 0, (plane == 0 ? AvailU : AvailUChroma) || y > 0,
                          block_decoded(plane, (subBlockMiRow >> subY) - 1, (subBlockMiCol >> subX) + stepX),
                          block_decoded(plane, (subBlockMiRow >> subY) + stepY, (subBlockMiCol >> subX) - 1), mode, log2W, log2H);
            if (isCfl) predict_chroma_from_luma(plane, startX, startY, txSz);
        }
        if (plane == 0) {
            MaxLumaW = startX + stepX * 4;
            MaxLumaH = startY + stepY * 4;
        }
    }
    if (!skip) {
        const int eob = coeffs(plane, startX, startY, txSz);
        if (eob > 0) reconstruct(plane, startX, startY, txSz);
    }
    std::uint8_t* lf = d->lf_tx_size[plane].data();
    for (int i = 0; i < stepY; ++i) {
        const int rr = (row >> subY) + i;
        for (int j = 0; j < stepX; ++j) {
            lf[static_cast<std::size_t>(rr) * d->mi_stride + (col >> subX) + j] = static_cast<std::uint8_t>(txSz);
            const int by = (subBlockMiRow >> subY) + i;
            const int bx = (subBlockMiCol >> subX) + j;
            if (by <= 32 && bx <= 32) block_decoded(plane, by, bx) = 1;
        }
    }
}

void Tile::transform_tree(int startX, int startY, int w, int h) {
    const int maxX = fh->MiCols * MI_SIZE;
    const int maxY = fh->MiRows * MI_SIZE;
    if (startX >= maxX || startY >= maxY) return;
    const int row = startY >> MI_SIZE_LOG2;
    const int col = startX >> MI_SIZE_LOG2;
    const int lumaTxSz = mi(row, col).inter_tx_size;
    const int lumaW = Tx_Width[lumaTxSz];
    const int lumaH = Tx_Height[lumaTxSz];
    if (w <= lumaW && h <= lumaH) {
        const int txSz = find_tx_size(w, h);
        transform_block(0, startX, startY, txSz, 0, 0);
    } else {
        if (w > h) {
            transform_tree(startX, startY, w / 2, h);
            transform_tree(startX + w / 2, startY, w / 2, h);
        } else if (w < h) {
            transform_tree(startX, startY, w, h / 2);
            transform_tree(startX, startY + h / 2, w, h / 2);
        } else {
            transform_tree(startX, startY, w / 2, h / 2);
            transform_tree(startX + w / 2, startY, w / 2, h / 2);
            transform_tree(startX, startY + h / 2, w / 2, h / 2);
            transform_tree(startX + w / 2, startY + h / 2, w / 2, h / 2);
        }
    }
}

} // namespace av1
} // namespace video
} // namespace fizmo
