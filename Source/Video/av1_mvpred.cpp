#include "av1_internal.hpp"

namespace fizmo {
namespace video {
namespace av1 {

namespace {

bool has_newmv(int mode) {
    return mode == NEWMV || mode == NEW_NEWMV || mode == NEAR_NEWMV || mode == NEW_NEARMV || mode == NEAREST_NEWMV || mode == NEW_NEARESTMV;
}

} // namespace

void Tile::lower_mv_precision(Mv& mv) const noexcept {
    if (fh->allow_high_precision_mv) return;
    int v[2] = {mv.row, mv.col};
    for (int i = 0; i < 2; ++i) {
        if (fh->force_integer_mv) {
            const int a = iabs(v[i]);
            const int aInt = (a + 3) >> 3;
            if (v[i] > 0) {
                v[i] = aInt << 3;
            } else {
                v[i] = -(aInt << 3);
            }
        } else {
            if (v[i] & 1) {
                if (v[i] > 0) {
                    --v[i];
                } else {
                    ++v[i];
                }
            }
        }
    }
    mv = make_mv(v[0], v[1]);
}

Mv Tile::setup_global_mv(int refList) {
    const int ref = RefFrame[refList];
    Mv mv;
    int typ = IDENTITY;
    if (ref > INTRA_FRAME) typ = fh->GmType[ref];
    const int bw = block_width(MiSize);
    const int bh = block_height(MiSize);
    if (ref <= INTRA_FRAME || typ == IDENTITY) {
        mv = make_mv(0, 0);
    } else if (typ == TRANSLATION) {
        mv = make_mv(fh->gm_params[ref][0] >> (WARPEDMODEL_PREC_BITS - 3), fh->gm_params[ref][1] >> (WARPEDMODEL_PREC_BITS - 3));
    } else {
        const std::int64_t x = MiCol * MI_SIZE + bw / 2 - 1;
        const std::int64_t y = MiRow * MI_SIZE + bh / 2 - 1;
        const std::int64_t xc = (static_cast<std::int64_t>(fh->gm_params[ref][2]) - (1 << WARPEDMODEL_PREC_BITS)) * x + static_cast<std::int64_t>(fh->gm_params[ref][3]) * y + fh->gm_params[ref][0];
        const std::int64_t yc = static_cast<std::int64_t>(fh->gm_params[ref][4]) * x + (static_cast<std::int64_t>(fh->gm_params[ref][5]) - (1 << WARPEDMODEL_PREC_BITS)) * y + fh->gm_params[ref][1];
        if (fh->allow_high_precision_mv) {
            mv = make_mv(static_cast<int>(round2signed_64(yc, WARPEDMODEL_PREC_BITS - 3)), static_cast<int>(round2signed_64(xc, WARPEDMODEL_PREC_BITS - 3)));
        } else {
            mv = make_mv(static_cast<int>(round2signed_64(yc, WARPEDMODEL_PREC_BITS - 2) * 2), static_cast<int>(round2signed_64(xc, WARPEDMODEL_PREC_BITS - 2) * 2));
        }
    }
    lower_mv_precision(mv);
    return mv;
}

void Tile::find_mv_stack(int isCompound) {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    NumMvFound = 0;
    NewMvCount = 0;
    GlobalMvs[0] = setup_global_mv(0);
    if (isCompound) GlobalMvs[1] = setup_global_mv(1);
    FoundMatch = 0;
    scan_row(-1, isCompound);
    int foundAboveMatch = FoundMatch;
    FoundMatch = 0;
    scan_col(-1, isCompound);
    int foundLeftMatch = FoundMatch;
    FoundMatch = 0;
    if (imax(bw4, bh4) <= 16) scan_point(-1, bw4, isCompound);
    if (FoundMatch == 1) foundAboveMatch = 1;
    CloseMatches = foundAboveMatch + foundLeftMatch;
    const int numNearest = NumMvFound;
    const int numNew = NewMvCount;
    if (numNearest > 0) {
        for (int idx = 0; idx < numNearest; ++idx) WeightStack[idx] += REF_CAT_LEVEL;
    }
    ZeroMvContext = 0;
    if (fh->use_ref_frame_mvs == 1) temporal_scan(isCompound);
    scan_point(-1, -1, isCompound);
    if (FoundMatch == 1) foundAboveMatch = 1;
    FoundMatch = 0;
    scan_row(-3, isCompound);
    if (FoundMatch == 1) foundAboveMatch = 1;
    FoundMatch = 0;
    scan_col(-3, isCompound);
    if (FoundMatch == 1) foundLeftMatch = 1;
    FoundMatch = 0;
    if (bh4 > 1) scan_row(-5, isCompound);
    if (FoundMatch == 1) foundAboveMatch = 1;
    FoundMatch = 0;
    if (bw4 > 1) scan_col(-5, isCompound);
    if (FoundMatch == 1) foundLeftMatch = 1;
    TotalMatches = foundAboveMatch + foundLeftMatch;
    sort_stack(0, numNearest, isCompound);
    sort_stack(numNearest, NumMvFound, isCompound);
    if (NumMvFound < 2) extra_search(isCompound);
    context_and_clamping(isCompound, numNew);
}

void Tile::scan_row(int deltaRow, int isCompound) {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int end4 = imin(imin(bw4, fh->MiCols - MiCol), 16);
    int deltaCol = 0;
    const int useStep16 = bw4 >= 16;
    if (iabs(deltaRow) > 1) {
        deltaRow += MiRow & 1;
        deltaCol = 1 - (MiCol & 1);
    }
    int i = 0;
    while (i < end4) {
        const int mvRow = MiRow + deltaRow;
        const int mvCol = MiCol + deltaCol + i;
        if (!is_inside(mvRow, mvCol)) break;
        int len = imin(bw4, Num_4x4_Blocks_Wide[mi(mvRow, mvCol).mi_size]);
        if (iabs(deltaRow) > 1) len = imax(2, len);
        if (useStep16) len = imax(4, len);
        const int weight = len * 2;
        add_ref_mv_candidate(mvRow, mvCol, isCompound, weight);
        i += len;
    }
}

void Tile::scan_col(int deltaCol, int isCompound) {
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    const int end4 = imin(imin(bh4, fh->MiRows - MiRow), 16);
    int deltaRow = 0;
    const int useStep16 = bh4 >= 16;
    if (iabs(deltaCol) > 1) {
        deltaRow = 1 - (MiRow & 1);
        deltaCol += MiCol & 1;
    }
    int i = 0;
    while (i < end4) {
        const int mvRow = MiRow + deltaRow + i;
        const int mvCol = MiCol + deltaCol;
        if (!is_inside(mvRow, mvCol)) break;
        int len = imin(bh4, Num_4x4_Blocks_High[mi(mvRow, mvCol).mi_size]);
        if (iabs(deltaCol) > 1) len = imax(2, len);
        if (useStep16) len = imax(4, len);
        const int weight = len * 2;
        add_ref_mv_candidate(mvRow, mvCol, isCompound, weight);
        i += len;
    }
}

void Tile::scan_point(int deltaRow, int deltaCol, int isCompound) {
    const int mvRow = MiRow + deltaRow;
    const int mvCol = MiCol + deltaCol;
    const int weight = 4;
    if (is_inside(mvRow, mvCol) && mi(mvRow, mvCol).written == d->mi_stamp) add_ref_mv_candidate(mvRow, mvCol, isCompound, weight);
}

void Tile::temporal_scan(int isCompound) {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    const int stepW4 = bw4 >= 16 ? 4 : 2;
    const int stepH4 = bh4 >= 16 ? 4 : 2;
    for (int deltaRow = 0; deltaRow < imin(bh4, 16); deltaRow += stepH4) {
        for (int deltaCol = 0; deltaCol < imin(bw4, 16); deltaCol += stepW4) add_tpl_ref_mv(deltaRow, deltaCol, isCompound);
    }
    const int allowExtension = bh4 >= Num_4x4_Blocks_High[BLOCK_8X8] && bh4 < Num_4x4_Blocks_High[BLOCK_64X64] && bw4 >= Num_4x4_Blocks_Wide[BLOCK_8X8] && bw4 < Num_4x4_Blocks_Wide[BLOCK_64X64];
    if (allowExtension) {
        const int tplSamplePos[3][2] = {{bh4, -2}, {bh4, bw4}, {bh4 - 2, bw4}};
        for (int i = 0; i < 3; ++i) {
            const int deltaRow = tplSamplePos[i][0];
            const int deltaCol = tplSamplePos[i][1];
            const int row = (MiRow & 15) + deltaRow;
            const int col = (MiCol & 15) + deltaCol;
            if (row >= 0 && row < 16 && col >= 0 && col < 16) add_tpl_ref_mv(deltaRow, deltaCol, isCompound);
        }
    }
}

void Tile::add_tpl_ref_mv(int deltaRow, int deltaCol, int isCompound) {
    const int mvRow = (MiRow + deltaRow) | 1;
    const int mvCol = (MiCol + deltaCol) | 1;
    if (!is_inside(mvRow, mvCol)) return;
    const int x8 = mvCol >> 1;
    const int y8 = mvRow >> 1;
    const std::size_t w8 = static_cast<std::size_t>(fh->MiCols >> 1);
    const std::size_t at = static_cast<std::size_t>(y8) * w8 + static_cast<std::size_t>(x8);
    if (deltaRow == 0 && deltaCol == 0) ZeroMvContext = 1;
    if (!isCompound) {
        Mv candMv = d->motion_field_mvs[RefFrame[0]][at];
        if (candMv.row == -32768) return;
        lower_mv_precision(candMv);
        if (deltaRow == 0 && deltaCol == 0) {
            if (iabs(candMv.row - GlobalMvs[0].row) >= 16 || iabs(candMv.col - GlobalMvs[0].col) >= 16) {
                ZeroMvContext = 1;
            } else {
                ZeroMvContext = 0;
            }
        }
        int idx;
        for (idx = 0; idx < NumMvFound; ++idx) {
            if (candMv == RefStackMv[idx][0]) break;
        }
        if (idx < NumMvFound) {
            WeightStack[idx] += 2;
        } else if (NumMvFound < MAX_REF_MV_STACK_SIZE) {
            RefStackMv[NumMvFound][0] = candMv;
            WeightStack[NumMvFound] = 2;
            NumMvFound += 1;
        }
    } else {
        Mv candMv0 = d->motion_field_mvs[RefFrame[0]][at];
        if (candMv0.row == -32768) return;
        Mv candMv1 = d->motion_field_mvs[RefFrame[1]][at];
        if (candMv1.row == -32768) return;
        lower_mv_precision(candMv0);
        lower_mv_precision(candMv1);
        if (deltaRow == 0 && deltaCol == 0) {
            if (iabs(candMv0.row - GlobalMvs[0].row) >= 16 || iabs(candMv0.col - GlobalMvs[0].col) >= 16 || iabs(candMv1.row - GlobalMvs[1].row) >= 16 || iabs(candMv1.col - GlobalMvs[1].col) >= 16) {
                ZeroMvContext = 1;
            } else {
                ZeroMvContext = 0;
            }
        }
        int idx;
        for (idx = 0; idx < NumMvFound; ++idx) {
            if (candMv0 == RefStackMv[idx][0] && candMv1 == RefStackMv[idx][1]) break;
        }
        if (idx < NumMvFound) {
            WeightStack[idx] += 2;
        } else if (NumMvFound < MAX_REF_MV_STACK_SIZE) {
            RefStackMv[NumMvFound][0] = candMv0;
            RefStackMv[NumMvFound][1] = candMv1;
            WeightStack[NumMvFound] = 2;
            NumMvFound += 1;
        }
    }
}

void Tile::add_ref_mv_candidate(int mvRow, int mvCol, int isCompound, int weight) {
    const MiInfo& m = mi(mvRow, mvCol);
    if (!m.is_inter) return;
    if (!isCompound) {
        for (int candList = 0; candList < 2; ++candList) {
            if (m.ref_frame[candList] == RefFrame[0]) search_stack(mvRow, mvCol, candList, weight);
        }
    } else {
        if (m.ref_frame[0] == RefFrame[0] && m.ref_frame[1] == RefFrame[1]) compound_search_stack(mvRow, mvCol, weight);
    }
}

void Tile::search_stack(int mvRow, int mvCol, int candList, int weight) {
    const MiInfo& m = mi(mvRow, mvCol);
    const int candMode = m.y_mode;
    const int candSize = m.mi_size;
    const int large = imin(block_width(candSize), block_height(candSize)) >= 8;
    Mv candMv;
    if ((candMode == GLOBALMV || candMode == GLOBAL_GLOBALMV) && fh->GmType[RefFrame[0] > 0 ? RefFrame[0] : 0] > TRANSLATION && large) {
        candMv = GlobalMvs[0];
    } else {
        candMv = m.mv[candList];
    }
    lower_mv_precision(candMv);
    if (has_newmv(candMode)) NewMvCount += 1;
    FoundMatch = 1;
    int idx;
    for (idx = 0; idx < NumMvFound; ++idx) {
        if (candMv == RefStackMv[idx][0]) break;
    }
    if (idx < NumMvFound) {
        WeightStack[idx] += weight;
    } else if (NumMvFound < MAX_REF_MV_STACK_SIZE) {
        RefStackMv[NumMvFound][0] = candMv;
        WeightStack[NumMvFound] = weight;
        NumMvFound += 1;
    }
}

void Tile::compound_search_stack(int mvRow, int mvCol, int weight) {
    const MiInfo& m = mi(mvRow, mvCol);
    Mv candMvs[2] = {m.mv[0], m.mv[1]};
    const int candMode = m.y_mode;
    if (candMode == GLOBAL_GLOBALMV) {
        for (int refList = 0; refList < 2; ++refList) {
            if (fh->GmType[RefFrame[refList]] > TRANSLATION) candMvs[refList] = GlobalMvs[refList];
        }
    }
    for (int i = 0; i < 2; ++i) lower_mv_precision(candMvs[i]);
    FoundMatch = 1;
    int idx;
    for (idx = 0; idx < NumMvFound; ++idx) {
        if (candMvs[0] == RefStackMv[idx][0] && candMvs[1] == RefStackMv[idx][1]) break;
    }
    if (idx < NumMvFound) {
        WeightStack[idx] += weight;
    } else if (NumMvFound < MAX_REF_MV_STACK_SIZE) {
        RefStackMv[NumMvFound][0] = candMvs[0];
        RefStackMv[NumMvFound][1] = candMvs[1];
        WeightStack[NumMvFound] = weight;
        NumMvFound += 1;
    }
    if (has_newmv(candMode)) NewMvCount += 1;
}

void Tile::sort_stack(int start, int end, int isCompound) {
    while (end > start) {
        int newEnd = start;
        for (int idx = start + 1; idx < end; ++idx) {
            if (WeightStack[idx - 1] < WeightStack[idx]) {
                const int t = WeightStack[idx - 1];
                WeightStack[idx - 1] = WeightStack[idx];
                WeightStack[idx] = t;
                for (int list = 0; list < 1 + isCompound; ++list) {
                    const Mv tm = RefStackMv[idx - 1][list];
                    RefStackMv[idx - 1][list] = RefStackMv[idx][list];
                    RefStackMv[idx][list] = tm;
                }
                newEnd = idx;
            }
        }
        end = newEnd;
    }
}

void Tile::extra_search(int isCompound) {
    for (int list = 0; list < 2; ++list) {
        RefIdCount[list] = 0;
        RefDiffCount[list] = 0;
    }
    int w4 = imin(16, Num_4x4_Blocks_Wide[MiSize]);
    int h4 = imin(16, Num_4x4_Blocks_High[MiSize]);
    w4 = imin(w4, fh->MiCols - MiCol);
    h4 = imin(h4, fh->MiRows - MiRow);
    const int num4x4 = imin(w4, h4);
    for (int pass = 0; pass < 2; ++pass) {
        int idx = 0;
        while (idx < num4x4 && NumMvFound < 2) {
            int mvRow;
            int mvCol;
            if (pass == 0) {
                mvRow = MiRow - 1;
                mvCol = MiCol + idx;
            } else {
                mvRow = MiRow + idx;
                mvCol = MiCol - 1;
            }
            if (!is_inside(mvRow, mvCol)) break;
            add_extra_mv_candidate(mvRow, mvCol, isCompound);
            if (pass == 0) {
                idx += Num_4x4_Blocks_Wide[mi(mvRow, mvCol).mi_size];
            } else {
                idx += Num_4x4_Blocks_High[mi(mvRow, mvCol).mi_size];
            }
        }
    }
    if (isCompound) {
        Mv combinedMvs[2][2];
        for (int list = 0; list < 2; ++list) {
            int compCount = 0;
            for (int idx = 0; idx < RefIdCount[list]; ++idx) {
                combinedMvs[compCount][list] = RefIdMvs[list][idx];
                ++compCount;
            }
            for (int idx = 0; idx < RefDiffCount[list] && compCount < 2; ++idx) {
                combinedMvs[compCount][list] = RefDiffMvs[list][idx];
                ++compCount;
            }
            while (compCount < 2) {
                combinedMvs[compCount][list] = GlobalMvs[list];
                ++compCount;
            }
        }
        if (NumMvFound == 1) {
            if (combinedMvs[0][0] == RefStackMv[0][0] && combinedMvs[0][1] == RefStackMv[0][1]) {
                RefStackMv[NumMvFound][0] = combinedMvs[1][0];
                RefStackMv[NumMvFound][1] = combinedMvs[1][1];
            } else {
                RefStackMv[NumMvFound][0] = combinedMvs[0][0];
                RefStackMv[NumMvFound][1] = combinedMvs[0][1];
            }
            WeightStack[NumMvFound] = 2;
            ++NumMvFound;
        } else {
            for (int idx = 0; idx < 2; ++idx) {
                RefStackMv[NumMvFound][0] = combinedMvs[idx][0];
                RefStackMv[NumMvFound][1] = combinedMvs[idx][1];
                WeightStack[NumMvFound] = 2;
                ++NumMvFound;
            }
        }
    } else {
        for (int idx = NumMvFound; idx < 2; ++idx) RefStackMv[idx][0] = GlobalMvs[0];
    }
}

void Tile::add_extra_mv_candidate(int mvRow, int mvCol, int isCompound) {
    const MiInfo& m = mi(mvRow, mvCol);
    if (isCompound) {
        for (int candList = 0; candList < 2; ++candList) {
            const int candRef = m.ref_frame[candList];
            if (candRef > INTRA_FRAME) {
                for (int list = 0; list < 2; ++list) {
                    Mv candMv = m.mv[candList];
                    if (candRef == RefFrame[list] && RefIdCount[list] < 2) {
                        RefIdMvs[list][RefIdCount[list]] = candMv;
                        RefIdCount[list]++;
                    } else if (RefDiffCount[list] < 2) {
                        if (fh->RefFrameSignBias[candRef] != fh->RefFrameSignBias[RefFrame[list]]) candMv = make_mv(-candMv.row, -candMv.col);
                        RefDiffMvs[list][RefDiffCount[list]] = candMv;
                        RefDiffCount[list]++;
                    }
                }
            }
        }
    } else {
        for (int candList = 0; candList < 2; ++candList) {
            const int candRef = m.ref_frame[candList];
            if (candRef > INTRA_FRAME) {
                Mv candMv = m.mv[candList];
                if (fh->RefFrameSignBias[candRef] != fh->RefFrameSignBias[RefFrame[0] > 0 ? RefFrame[0] : 0]) candMv = make_mv(-candMv.row, -candMv.col);
                int idx;
                for (idx = 0; idx < NumMvFound; ++idx) {
                    if (candMv == RefStackMv[idx][0]) break;
                }
                if (idx == NumMvFound) {
                    RefStackMv[idx][0] = candMv;
                    WeightStack[idx] = 2;
                    ++NumMvFound;
                }
            }
        }
    }
}

int Tile::clamp_mv_row(int mvec, int border) const noexcept {
    const int bh4 = Num_4x4_Blocks_High[MiSize];
    const int mbToTopEdge = -((MiRow * MI_SIZE) * 8);
    const int mbToBottomEdge = ((fh->MiRows - bh4 - MiRow) * MI_SIZE) * 8;
    return clip3(mbToTopEdge - border, mbToBottomEdge + border, mvec);
}

int Tile::clamp_mv_col(int mvec, int border) const noexcept {
    const int bw4 = Num_4x4_Blocks_Wide[MiSize];
    const int mbToLeftEdge = -((MiCol * MI_SIZE) * 8);
    const int mbToRightEdge = ((fh->MiCols - bw4 - MiCol) * MI_SIZE) * 8;
    return clip3(mbToLeftEdge - border, mbToRightEdge + border, mvec);
}

void Tile::context_and_clamping(int isCompound, int numNew) {
    const int bw = block_width(MiSize);
    const int bh = block_height(MiSize);
    const int numLists = isCompound ? 2 : 1;
    for (int idx = 0; idx < NumMvFound; ++idx) {
        int z = 0;
        if (idx + 1 < NumMvFound) {
            const int w0 = WeightStack[idx];
            const int w1 = WeightStack[idx + 1];
            if (w0 >= REF_CAT_LEVEL) {
                if (w1 < REF_CAT_LEVEL) z = 1;
            } else {
                z = 2;
            }
        }
        DrlCtxStack[idx] = z;
    }
    for (int list = 0; list < numLists; ++list) {
        for (int idx = 0; idx < NumMvFound; ++idx) {
            Mv& refMv = RefStackMv[idx][list];
            refMv = make_mv(clamp_mv_row(refMv.row, MV_BORDER + bh * 8), clamp_mv_col(refMv.col, MV_BORDER + bw * 8));
        }
    }
    if (CloseMatches == 0) {
        NewMvContext = imin(TotalMatches, 1);
        RefMvContext = TotalMatches;
    } else if (CloseMatches == 1) {
        NewMvContext = 3 - imin(numNew, 1);
        RefMvContext = 2 + TotalMatches;
    } else {
        NewMvContext = 5 - imin(numNew, 1);
        RefMvContext = 5;
    }
}

int Tile::has_overlappable_candidates() {
    if (AvailU) {
        const int w4 = Num_4x4_Blocks_Wide[MiSize];
        for (int x4 = MiCol; x4 < imin(fh->MiCols, MiCol + w4); x4 += 2) {
            if (mi(MiRow - 1, x4 | 1).ref_frame[0] > INTRA_FRAME) return 1;
        }
    }
    if (AvailL) {
        const int h4 = Num_4x4_Blocks_High[MiSize];
        for (int y4 = MiRow; y4 < imin(fh->MiRows, MiRow + h4); y4 += 2) {
            if (mi(y4 | 1, MiCol - 1).ref_frame[0] > INTRA_FRAME) return 1;
        }
    }
    return 0;
}

void Tile::find_warp_samples() {
    NumSamples = 0;
    NumSamplesScanned = 0;
    const int w4 = Num_4x4_Blocks_Wide[MiSize];
    const int h4 = Num_4x4_Blocks_High[MiSize];
    int doTopLeft = 1;
    int doTopRight = 1;
    if (AvailU) {
        const int srcSize = mi(MiRow - 1, MiCol).mi_size;
        const int srcW = Num_4x4_Blocks_Wide[srcSize];
        if (w4 <= srcW) {
            const int colOffset = -(MiCol & (srcW - 1));
            if (colOffset < 0) doTopLeft = 0;
            if (colOffset + srcW > w4) doTopRight = 0;
            add_sample(-1, 0);
        } else {
            int miStep;
            for (int i = 0; i < imin(w4, fh->MiCols - MiCol); i += miStep) {
                const int s = mi(MiRow - 1, MiCol + i).mi_size;
                const int sw = Num_4x4_Blocks_Wide[s];
                miStep = imin(w4, sw);
                add_sample(-1, i);
            }
        }
    }
    if (AvailL) {
        const int srcSize = mi(MiRow, MiCol - 1).mi_size;
        const int srcH = Num_4x4_Blocks_High[srcSize];
        if (h4 <= srcH) {
            const int rowOffset = -(MiRow & (srcH - 1));
            if (rowOffset < 0) doTopLeft = 0;
            add_sample(0, -1);
        } else {
            int miStep;
            for (int i = 0; i < imin(h4, fh->MiRows - MiRow); i += miStep) {
                const int s = mi(MiRow + i, MiCol - 1).mi_size;
                const int sh = Num_4x4_Blocks_High[s];
                miStep = imin(h4, sh);
                add_sample(i, -1);
            }
        }
    }
    if (doTopLeft) add_sample(-1, -1);
    if (doTopRight) {
        if (imax(w4, h4) <= 16) add_sample(-1, w4);
    }
    if (NumSamples == 0 && NumSamplesScanned > 0) NumSamples = 1;
}

void Tile::add_sample(int deltaRow, int deltaCol) {
    if (NumSamplesScanned >= LEAST_SQUARES_SAMPLES_MAX) return;
    const int mvRow = MiRow + deltaRow;
    const int mvCol = MiCol + deltaCol;
    if (!is_inside(mvRow, mvCol)) return;
    const MiInfo& m = mi(mvRow, mvCol);
    if (m.written != d->mi_stamp) return;
    if (m.ref_frame[0] != RefFrame[0]) return;
    if (m.ref_frame[1] != NONE) return;
    const int candSz = m.mi_size;
    const int candW4 = Num_4x4_Blocks_Wide[candSz];
    const int candH4 = Num_4x4_Blocks_High[candSz];
    const int candRow = mvRow & ~(candH4 - 1);
    const int candCol = mvCol & ~(candW4 - 1);
    const int midY = candRow * 4 + candH4 * 2 - 1;
    const int midX = candCol * 4 + candW4 * 2 - 1;
    const int threshold = clip3(16, 112, imax(block_width(MiSize), block_height(MiSize)));
    const Mv cmv = mi(candRow, candCol).mv[0];
    const int mvDiffRow = iabs(cmv.row - MvVal[0].row);
    const int mvDiffCol = iabs(cmv.col - MvVal[0].col);
    const int valid = (mvDiffRow + mvDiffCol) <= threshold;
    int cand[4];
    cand[0] = midY * 8;
    cand[1] = midX * 8;
    cand[2] = midY * 8 + cmv.row;
    cand[3] = midX * 8 + cmv.col;
    NumSamplesScanned += 1;
    if (valid == 0 && NumSamplesScanned > 1) return;
    for (int j = 0; j < 4; ++j) CandList[NumSamples][j] = cand[j];
    if (valid == 1) NumSamples += 1;
}

} // namespace av1
} // namespace video
} // namespace fizmo
