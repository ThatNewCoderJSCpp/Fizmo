#include "av1_internal.hpp"

namespace fizmo {
namespace video {
namespace av1 {

namespace {

const std::uint8_t* sm_weights(int log2) {
    switch (log2) {
    case 2: return Sm_Weights_Tx_4x4;
    case 3: return Sm_Weights_Tx_8x8;
    case 4: return Sm_Weights_Tx_16x16;
    case 5: return Sm_Weights_Tx_32x32;
    default: return Sm_Weights_Tx_64x64;
    }
}

int edge_filter_strength(int w, int h, int filterType, int delta) {
    const int d = iabs(delta);
    const int blkWh = w + h;
    int strength = 0;
    if (filterType == 0) {
        if (blkWh <= 8) {
            if (d >= 56) strength = 1;
        } else if (blkWh <= 12) {
            if (d >= 40) strength = 1;
        } else if (blkWh <= 16) {
            if (d >= 40) strength = 1;
        } else if (blkWh <= 24) {
            if (d >= 8) strength = 1;
            if (d >= 16) strength = 2;
            if (d >= 32) strength = 3;
        } else if (blkWh <= 32) {
            strength = 1;
            if (d >= 4) strength = 2;
            if (d >= 32) strength = 3;
        } else {
            strength = 3;
        }
    } else {
        if (blkWh <= 8) {
            if (d >= 40) strength = 1;
            if (d >= 64) strength = 2;
        } else if (blkWh <= 16) {
            if (d >= 20) strength = 1;
            if (d >= 48) strength = 2;
        } else if (blkWh <= 24) {
            if (d >= 4) strength = 3;
        } else {
            strength = 3;
        }
    }
    return strength;
}

int use_upsample(int w, int h, int filterType, int delta) {
    const int d = iabs(delta);
    const int blkWh = w + h;
    if (d <= 0 || d >= 40) return 0;
    if (filterType == 0) return blkWh <= 16;
    return blkWh <= 8;
}

void edge_filter(int* buf, int sz, int strength) {
    if (strength == 0) return;
    int edge[300];
    for (int i = 0; i < sz; ++i) edge[i] = buf[i - 1];
    for (int i = 1; i < sz; ++i) {
        int s = 0;
        for (int j = 0; j < INTRA_EDGE_TAPS; ++j) {
            const int k = clip3(0, sz - 1, i - 2 + j);
            s += Intra_Edge_Kernel[strength - 1][j] * edge[k];
        }
        buf[i - 1] = (s + 8) >> 4;
    }
}

void edge_upsample(int* buf, int numPx, int maxv) {
    int dup[300];
    dup[0] = buf[-1];
    for (int i = -1; i < numPx; ++i) dup[i + 2] = buf[i];
    dup[numPx + 2] = buf[numPx - 1];
    buf[-2] = dup[0];
    for (int i = 0; i < numPx; ++i) {
        int s = -dup[i] + (9 * dup[i + 1]) + (9 * dup[i + 2]) - dup[i + 3];
        s = clip3(0, maxv, round2(s, 4));
        buf[2 * i - 1] = s;
        buf[2 * i] = dup[i + 2];
    }
}

} // namespace

void Tile::predict_intra(int plane, int x, int y, int haveLeft, int haveAbove, int haveAboveRight, int haveBelowLeft, int mode, int log2W, int log2H) {
    Plane& P = d->frame.planes[plane];
    const int BitDepth = seq->BitDepth;
    const int maxv = (1 << BitDepth) - 1;
    const int w = 1 << log2W;
    const int h = 1 << log2H;
    int maxX = (fh->MiCols * MI_SIZE) - 1;
    int maxY = (fh->MiRows * MI_SIZE) - 1;
    if (plane > 0) {
        maxX = ((fh->MiCols * MI_SIZE) >> seq->subsampling_x) - 1;
        maxY = ((fh->MiRows * MI_SIZE) >> seq->subsampling_y) - 1;
    }
    int aboveBuf[320];
    int leftBuf[320];
    int* AboveRow = aboveBuf + 16;
    int* LeftCol = leftBuf + 16;
    const int n = w + h;
    if (haveAbove == 0 && haveLeft == 1) {
        const int v = P.row(y)[x - 1];
        for (int i = 0; i < n; ++i) AboveRow[i] = v;
    } else if (haveAbove == 0 && haveLeft == 0) {
        for (int i = 0; i < n; ++i) AboveRow[i] = (1 << (BitDepth - 1)) - 1;
    } else {
        const int aboveLimit = imin(maxX, x + (haveAboveRight ? 2 * w : w) - 1);
        const std::uint16_t* r = P.row(y - 1);
        for (int i = 0; i < n; ++i) AboveRow[i] = r[imin(aboveLimit, x + i)];
    }
    if (haveLeft == 0 && haveAbove == 1) {
        const int v = P.row(y - 1)[x];
        for (int i = 0; i < n; ++i) LeftCol[i] = v;
    } else if (haveLeft == 0 && haveAbove == 0) {
        for (int i = 0; i < n; ++i) LeftCol[i] = (1 << (BitDepth - 1)) + 1;
    } else {
        const int leftLimit = imin(maxY, y + (haveBelowLeft ? 2 * h : h) - 1);
        for (int i = 0; i < n; ++i) LeftCol[i] = P.row(imin(leftLimit, y + i))[x - 1];
    }
    if (haveAbove == 1 && haveLeft == 1) {
        AboveRow[-1] = P.row(y - 1)[x - 1];
    } else if (haveAbove == 1) {
        AboveRow[-1] = P.row(y - 1)[x];
    } else if (haveLeft == 1) {
        AboveRow[-1] = P.row(y)[x - 1];
    } else {
        AboveRow[-1] = 1 << (BitDepth - 1);
    }
    LeftCol[-1] = AboveRow[-1];
    std::uint16_t* out = P.row(y) + x;
    const std::ptrdiff_t stride = P.stride;
    if (plane == 0 && use_filter_intra) {
        AV1_STAT(ST_FILTER_INTRA);
        const int w4 = w >> 2;
        const int h2 = h >> 1;
        for (int i2 = 0; i2 < h2; ++i2) {
            for (int j4 = 0; j4 < w4; ++j4) {
                int p[7];
                for (int i = 0; i < 7; ++i) {
                    if (i < 5) {
                        if (i2 == 0) {
                            p[i] = AboveRow[(j4 << 2) + i - 1];
                        } else if (j4 == 0 && i == 0) {
                            p[i] = LeftCol[(i2 << 1) - 1];
                        } else {
                            p[i] = out[((i2 << 1) - 1) * stride + (j4 << 2) + i - 1];
                        }
                    } else {
                        if (j4 == 0) {
                            p[i] = LeftCol[(i2 << 1) + i - 5];
                        } else {
                            p[i] = out[((i2 << 1) + i - 5) * stride + (j4 << 2) - 1];
                        }
                    }
                }
                for (int i1 = 0; i1 < 2; ++i1) {
                    for (int j1 = 0; j1 < 4; ++j1) {
                        int pr = 0;
                        for (int i = 0; i < 7; ++i) pr += Intra_Filter_Taps[filter_intra_mode][(i1 << 2) + j1][i] * p[i];
                        out[((i2 << 1) + i1) * stride + (j4 << 2) + j1] = static_cast<std::uint16_t>(clip3(0, maxv, round2signed(pr, INTRA_FILTER_SCALE_BITS)));
                    }
                }
            }
        }
        return;
    }
    if (mode >= V_PRED && mode <= D67_PRED) {
        const int angleDelta = plane == 0 ? AngleDeltaY : AngleDeltaUV;
        const int pAngle = Mode_To_Angle[mode] + angleDelta * ANGLE_STEP;
        AV1_STAT(ST_DIRECTIONAL);
        if (angleDelta) AV1_STAT(ST_ANGLE_DELTA);
        if (mode == V_PRED) AV1_STAT(ST_V_PRED_ST);
        if (mode == H_PRED) AV1_STAT(ST_H_PRED_ST);
        int upsampleAbove = 0;
        int upsampleLeft = 0;
        if (seq->enable_intra_edge_filter == 1) {
            int filterType = 0;
            if (pAngle != 90 && pAngle != 180) {
                if (pAngle > 90 && pAngle < 180 && (w + h) >= 24) {
                    const int s = LeftCol[0] * 5 + AboveRow[-1] * 6 + AboveRow[0] * 5;
                    LeftCol[-1] = AboveRow[-1] = round2(s, 4);
                }
            }
            {
                int aboveSmooth = 0;
                int leftSmooth = 0;
                auto is_smooth = [&](int row, int col) {
                    const MiInfo& m = mi(row, col);
                    int md;
                    if (plane == 0) {
                        md = m.y_mode;
                    } else {
                        if (m.ref_frame[0] > INTRA_FRAME) return 0;
                        md = m.uv_mode;
                    }
                    return (md == SMOOTH_PRED || md == SMOOTH_V_PRED || md == SMOOTH_H_PRED) ? 1 : 0;
                };
                if (plane == 0 ? AvailU : AvailUChroma) {
                    int r = MiRow - 1;
                    int c = MiCol;
                    if (plane > 0) {
                        if (seq->subsampling_x && !(MiCol & 1)) ++c;
                        if (seq->subsampling_y && (MiRow & 1)) --r;
                    }
                    aboveSmooth = is_smooth(r, c);
                }
                if (plane == 0 ? AvailL : AvailLChroma) {
                    int r = MiRow;
                    int c = MiCol - 1;
                    if (plane > 0) {
                        if (seq->subsampling_x && (MiCol & 1)) --c;
                        if (seq->subsampling_y && !(MiRow & 1)) ++r;
                    }
                    leftSmooth = is_smooth(r, c);
                }
                filterType = aboveSmooth || leftSmooth;
            }
            if (pAngle != 90 && pAngle != 180) {
                if (haveAbove == 1) {
                    const int strength = edge_filter_strength(w, h, filterType, pAngle - 90);
                    const int numPx = imin(w, (maxX - x + 1)) + (pAngle < 90 ? h : 0) + 1;
                    if (strength) AV1_STAT(ST_EDGE_FILTER);
                    edge_filter(AboveRow, numPx, strength);
                }
                if (haveLeft == 1) {
                    const int strength = edge_filter_strength(w, h, filterType, pAngle - 180);
                    const int numPx = imin(h, (maxY - y + 1)) + (pAngle > 180 ? w : 0) + 1;
                    edge_filter(LeftCol, numPx, strength);
                }
            }
            upsampleAbove = use_upsample(w, h, filterType, pAngle - 90);
            int numPx = w + (pAngle < 90 ? h : 0);
            if (upsampleAbove == 1) {
                AV1_STAT(ST_EDGE_UPSAMPLE);
                edge_upsample(AboveRow, numPx, maxv);
            }
            upsampleLeft = use_upsample(w, h, filterType, pAngle - 180);
            numPx = h + (pAngle > 180 ? w : 0);
            if (upsampleLeft == 1) edge_upsample(LeftCol, numPx, maxv);
        }
        int dx = 0;
        int dy = 0;
        if (pAngle < 90) {
            dx = Dr_Intra_Derivative[pAngle];
        } else if (pAngle > 90 && pAngle < 180) {
            dx = Dr_Intra_Derivative[180 - pAngle];
        }
        if (pAngle > 90 && pAngle < 180) {
            dy = Dr_Intra_Derivative[pAngle - 90];
        } else if (pAngle > 180) {
            dy = Dr_Intra_Derivative[270 - pAngle];
        }
        if (pAngle < 90) {
            const int maxBaseX = (w + h - 1) << upsampleAbove;
            for (int i = 0; i < h; ++i) {
                const int idx = (i + 1) * dx;
                const int shift = ((idx << upsampleAbove) >> 1) & 0x1F;
                std::uint16_t* o = out + i * stride;
                for (int j = 0; j < w; ++j) {
                    const int base = (idx >> (6 - upsampleAbove)) + (j << upsampleAbove);
                    if (base < maxBaseX) {
                        o[j] = static_cast<std::uint16_t>(round2(AboveRow[base] * (32 - shift) + AboveRow[base + 1] * shift, 5));
                    } else {
                        o[j] = static_cast<std::uint16_t>(AboveRow[maxBaseX]);
                    }
                }
            }
        } else if (pAngle > 90 && pAngle < 180) {
            for (int i = 0; i < h; ++i) {
                std::uint16_t* o = out + i * stride;
                for (int j = 0; j < w; ++j) {
                    int idx = (j << 6) - (i + 1) * dx;
                    int base = idx >> (6 - upsampleAbove);
                    if (base >= -(1 << upsampleAbove)) {
                        const int shift = ((idx * (1 << upsampleAbove)) >> 1) & 0x1F;
                        o[j] = static_cast<std::uint16_t>(round2(AboveRow[base] * (32 - shift) + AboveRow[base + 1] * shift, 5));
                    } else {
                        idx = (i << 6) - (j + 1) * dy;
                        base = idx >> (6 - upsampleLeft);
                        const int shift = ((idx * (1 << upsampleLeft)) >> 1) & 0x1F;
                        o[j] = static_cast<std::uint16_t>(round2(LeftCol[base] * (32 - shift) + LeftCol[base + 1] * shift, 5));
                    }
                }
            }
        } else if (pAngle > 180) {
            for (int j = 0; j < w; ++j) {
                const int idx = (j + 1) * dy;
                const int shift = ((idx << upsampleLeft) >> 1) & 0x1F;
                for (int i = 0; i < h; ++i) {
                    const int base = (idx >> (6 - upsampleLeft)) + (i << upsampleLeft);
                    out[i * stride + j] = static_cast<std::uint16_t>(round2(LeftCol[base] * (32 - shift) + LeftCol[base + 1] * shift, 5));
                }
            }
        } else if (pAngle == 90) {
            for (int i = 0; i < h; ++i) {
                std::uint16_t* o = out + i * stride;
                for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(AboveRow[j]);
            }
        } else {
            for (int i = 0; i < h; ++i) {
                std::uint16_t* o = out + i * stride;
                for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(LeftCol[i]);
            }
        }
        return;
    }
    if (mode == SMOOTH_PRED) {
        AV1_STAT(ST_SMOOTH);
        const std::uint8_t* wx = sm_weights(log2W);
        const std::uint8_t* wy = sm_weights(log2H);
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = out + i * stride;
            for (int j = 0; j < w; ++j) {
                const int smoothPred = wy[i] * AboveRow[j] + (256 - wy[i]) * LeftCol[h - 1] + wx[j] * LeftCol[i] + (256 - wx[j]) * AboveRow[w - 1];
                o[j] = static_cast<std::uint16_t>(round2(smoothPred, 9));
            }
        }
        return;
    }
    if (mode == SMOOTH_V_PRED) {
        AV1_STAT(ST_SMOOTH_V);
        const std::uint8_t* wy = sm_weights(log2H);
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = out + i * stride;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(round2(wy[i] * AboveRow[j] + (256 - wy[i]) * LeftCol[h - 1], 8));
        }
        return;
    }
    if (mode == SMOOTH_H_PRED) {
        AV1_STAT(ST_SMOOTH_H);
        const std::uint8_t* wx = sm_weights(log2W);
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = out + i * stride;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(round2(wx[j] * LeftCol[i] + (256 - wx[j]) * AboveRow[w - 1], 8));
        }
        return;
    }
    if (mode == DC_PRED) {
        AV1_STAT(ST_DC_PRED_ST);
        int avg;
        if (haveLeft == 1 && haveAbove == 1) {
            int sum = 0;
            for (int k = 0; k < h; ++k) sum += LeftCol[k];
            for (int k = 0; k < w; ++k) sum += AboveRow[k];
            sum += (w + h) >> 1;
            avg = sum / (w + h);
        } else if (haveLeft == 1) {
            int sum = 0;
            for (int k = 0; k < h; ++k) sum += LeftCol[k];
            avg = clip3(0, maxv, (sum + (h >> 1)) >> log2H);
        } else if (haveAbove == 1) {
            int sum = 0;
            for (int k = 0; k < w; ++k) sum += AboveRow[k];
            avg = clip3(0, maxv, (sum + (w >> 1)) >> log2W);
        } else {
            avg = 1 << (BitDepth - 1);
        }
        for (int i = 0; i < h; ++i) {
            std::uint16_t* o = out + i * stride;
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(avg);
        }
        return;
    }
    for (int i = 0; i < h; ++i) {
        std::uint16_t* o = out + i * stride;
        for (int j = 0; j < w; ++j) {
            if (i == 0 && j == 0) AV1_STAT(ST_PAETH);
            const int base = AboveRow[j] + LeftCol[i] - AboveRow[-1];
            const int pLeft = iabs(base - LeftCol[i]);
            const int pTop = iabs(base - AboveRow[j]);
            const int pTopLeft = iabs(base - AboveRow[-1]);
            int v;
            if (pLeft <= pTop && pLeft <= pTopLeft) {
                v = LeftCol[i];
            } else if (pTop <= pTopLeft) {
                v = AboveRow[j];
            } else {
                v = AboveRow[-1];
            }
            o[j] = static_cast<std::uint16_t>(v);
        }
    }
}

void Tile::predict_palette(int plane, int startX, int startY, int x, int y, int txSz) {
    Plane& P = d->frame.planes[plane];
    const int w = Tx_Width[txSz];
    const int h = Tx_Height[txSz];
    const std::uint16_t* palette = plane == 0 ? palette_colors_y : (plane == 1 ? palette_colors_u : palette_colors_v);
    std::uint8_t (*map)[64] = plane == 0 ? ColorMapY : ColorMapUV;
    for (int i = 0; i < h; ++i) {
        std::uint16_t* o = P.row(startY + i) + startX;
        const std::uint8_t* mrow = map[y * 4 + i] + x * 4;
        for (int j = 0; j < w; ++j) o[j] = palette[mrow[j] & 7];
    }
}

void Tile::predict_chroma_from_luma(int plane, int startX, int startY, int txSz) {
    Plane& P = d->frame.planes[plane];
    Plane& L0 = d->frame.planes[0];
    const int maxv = (1 << seq->BitDepth) - 1;
    const int w = Tx_Width[txSz];
    const int h = Tx_Height[txSz];
    const int subX = seq->subsampling_x;
    const int subY = seq->subsampling_y;
    const int alpha = plane == 1 ? CflAlphaU : CflAlphaV;
    AV1_STAT(ST_CFL);
    int* L = scratch32.data();
    if (scratch32.size() < 64 * 64) {
        scratch32.resize(64 * 64);
        L = scratch32.data();
    }
    int lumaAvg = 0;
    for (int i = 0; i < h; ++i) {
        int lumaY = (startY + i) << subY;
        lumaY = imin(lumaY, MaxLumaH - (1 << subY));
        for (int j = 0; j < w; ++j) {
            int lumaX = (startX + j) << subX;
            lumaX = imin(lumaX, MaxLumaW - (1 << subX));
            int t = 0;
            for (int dy = 0; dy <= subY; ++dy) {
                const std::uint16_t* r = L0.row(lumaY + dy);
                for (int dx = 0; dx <= subX; ++dx) t += r[lumaX + dx];
            }
            const int v = t << (3 - subX - subY);
            L[i * w + j] = v;
            lumaAvg += v;
        }
    }
    lumaAvg = round2(lumaAvg, Tx_Width_Log2[txSz] + Tx_Height_Log2[txSz]);
    for (int i = 0; i < h; ++i) {
        std::uint16_t* o = P.row(startY + i) + startX;
        for (int j = 0; j < w; ++j) {
            const int dc = o[j];
            const int scaledLuma = round2signed(alpha * (L[i * w + j] - lumaAvg), 6);
            o[j] = static_cast<std::uint16_t>(clip3(0, maxv, dc + scaledLuma));
        }
    }
}

} // namespace av1
} // namespace video
} // namespace fizmo
