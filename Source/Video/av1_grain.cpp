#include "av1_internal.hpp"

namespace fizmo {
namespace video {
namespace av1 {

namespace {

struct GrainState {
    int RandomRegister = 0;
    int get_random_number(int bits) {
        int r = RandomRegister;
        const int bit = ((r >> 0) ^ (r >> 1) ^ (r >> 3) ^ (r >> 12)) & 1;
        r = (r >> 1) | (bit << 15);
        const int result = (r >> (16 - bits)) & ((1 << bits) - 1);
        RandomRegister = r;
        return result;
    }
};

} // namespace

void add_film_grain(const FilmGrainParams& p, int BitDepth, int mono, int subX, int subY, int matrix_coefficients, const FrameBuffer& in, FrameBuffer& out) {
    const int w = in.width;
    const int h = in.height;
    const int NumPlanes = mono ? 1 : 3;
    out.alloc(w, h, NumPlanes, subX, subY);
    for (int pl = 0; pl < NumPlanes; ++pl) {
        const Plane& s = in.planes[pl];
        Plane& t = out.planes[pl];
        for (int y = 0; y < t.height; ++y) std::memcpy(t.row(y), s.row(y), sizeof(std::uint16_t) * static_cast<std::size_t>(t.width));
    }
    GrainState g;
    g.RandomRegister = p.grain_seed;
    const int GrainCenter = 128 << (BitDepth - 8);
    const int GrainMin = -GrainCenter;
    const int GrainMax = (256 << (BitDepth - 8)) - 1 - GrainCenter;
    static thread_local int LumaGrain[73][82];
    static thread_local int CbGrain[73][82];
    static thread_local int CrGrain[73][82];
    {
        const int shift = 12 - BitDepth + p.grain_scale_shift;
        for (int y = 0; y < 73; ++y) {
            for (int x = 0; x < 82; ++x) {
                int gv = 0;
                if (p.num_y_points > 0) gv = Gaussian_Sequence[g.get_random_number(11)];
                LumaGrain[y][x] = round2(gv, shift);
            }
        }
        const int arShift = p.ar_coeff_shift_minus_6 + 6;
        for (int y = 3; y < 73; ++y) {
            for (int x = 3; x < 82 - 3; ++x) {
                int s = 0;
                int pos = 0;
                for (int deltaRow = -p.ar_coeff_lag; deltaRow <= 0; ++deltaRow) {
                    for (int deltaCol = -p.ar_coeff_lag; deltaCol <= p.ar_coeff_lag; ++deltaCol) {
                        if (deltaRow == 0 && deltaCol == 0) break;
                        const int c = p.ar_coeffs_y_plus_128[pos] - 128;
                        s += LumaGrain[y + deltaRow][x + deltaCol] * c;
                        ++pos;
                    }
                }
                LumaGrain[y][x] = clip3(GrainMin, GrainMax, LumaGrain[y][x] + round2(s, arShift));
            }
        }
    }
    const int chromaW = subX ? 44 : 82;
    const int chromaH = subY ? 38 : 73;
    if (!mono) {
        const int shift = 12 - BitDepth + p.grain_scale_shift;
        g.RandomRegister = p.grain_seed ^ 0xb524;
        for (int y = 0; y < chromaH; ++y) {
            for (int x = 0; x < chromaW; ++x) {
                int gv = 0;
                if (p.num_cb_points > 0 || p.chroma_scaling_from_luma) gv = Gaussian_Sequence[g.get_random_number(11)];
                CbGrain[y][x] = round2(gv, shift);
            }
        }
        g.RandomRegister = p.grain_seed ^ 0x49d8;
        for (int y = 0; y < chromaH; ++y) {
            for (int x = 0; x < chromaW; ++x) {
                int gv = 0;
                if (p.num_cr_points > 0 || p.chroma_scaling_from_luma) gv = Gaussian_Sequence[g.get_random_number(11)];
                CrGrain[y][x] = round2(gv, shift);
            }
        }
        const int arShift = p.ar_coeff_shift_minus_6 + 6;
        for (int y = 3; y < chromaH; ++y) {
            for (int x = 3; x < chromaW - 3; ++x) {
                int s0 = 0;
                int s1 = 0;
                int pos = 0;
                for (int deltaRow = -p.ar_coeff_lag; deltaRow <= 0; ++deltaRow) {
                    for (int deltaCol = -p.ar_coeff_lag; deltaCol <= p.ar_coeff_lag; ++deltaCol) {
                        const int c0 = p.ar_coeffs_cb_plus_128[pos] - 128;
                        const int c1 = p.ar_coeffs_cr_plus_128[pos] - 128;
                        if (deltaRow == 0 && deltaCol == 0) {
                            if (p.num_y_points > 0) {
                                int luma = 0;
                                const int lumaX = ((x - 3) << subX) + 3;
                                const int lumaY = ((y - 3) << subY) + 3;
                                for (int i = 0; i <= subY; ++i) {
                                    for (int j = 0; j <= subX; ++j) luma += LumaGrain[lumaY + i][lumaX + j];
                                }
                                luma = round2(luma, subX + subY);
                                s0 += luma * c0;
                                s1 += luma * c1;
                            }
                            break;
                        }
                        s0 += CbGrain[y + deltaRow][x + deltaCol] * c0;
                        s1 += CrGrain[y + deltaRow][x + deltaCol] * c1;
                        ++pos;
                    }
                }
                CbGrain[y][x] = clip3(GrainMin, GrainMax, CbGrain[y][x] + round2(s0, arShift));
                CrGrain[y][x] = clip3(GrainMin, GrainMax, CrGrain[y][x] + round2(s1, arShift));
            }
        }
    }
    int ScalingLut[3][256];
    for (int plane = 0; plane < NumPlanes; ++plane) {
        const int* xs;
        const int* ys;
        int numPoints;
        if (plane == 0 || p.chroma_scaling_from_luma) {
            numPoints = p.num_y_points;
            xs = p.point_y_value;
            ys = p.point_y_scaling;
        } else if (plane == 1) {
            numPoints = p.num_cb_points;
            xs = p.point_cb_value;
            ys = p.point_cb_scaling;
        } else {
            numPoints = p.num_cr_points;
            xs = p.point_cr_value;
            ys = p.point_cr_scaling;
        }
        if (numPoints == 0) {
            for (int x = 0; x < 256; ++x) ScalingLut[plane][x] = 0;
        } else {
            for (int x = 0; x < xs[0] && x < 256; ++x) ScalingLut[plane][x] = ys[0];
            for (int i = 0; i < numPoints - 1; ++i) {
                const int deltaY = ys[i + 1] - ys[i];
                const int deltaX = xs[i + 1] - xs[i];
                if (deltaX <= 0) continue;
                const int delta = deltaY * ((65536 + (deltaX >> 1)) / deltaX);
                for (int x = 0; x < deltaX; ++x) {
                    const int v = ys[i] + ((x * delta + 32768) >> 16);
                    const int idx = xs[i] + x;
                    if (idx >= 0 && idx < 256) ScalingLut[plane][idx] = v;
                }
            }
            for (int x = xs[numPoints - 1]; x < 256; ++x) ScalingLut[plane][x] = ys[numPoints - 1];
        }
    }
    auto scale_lut = [&](int plane, int index) {
        const int shift = BitDepth - 8;
        const int x = index >> shift;
        const int rem = index - (x << shift);
        if (BitDepth == 8 || x == 255) return ScalingLut[plane][x];
        const int start = ScalingLut[plane][x];
        const int end = ScalingLut[plane][x + 1];
        return start + round2((end - start) * rem, shift);
    };
    const int stripes = ((h + 1) / 2 + 15) / 16;
    const int stripeW[3] = {w + 34, ((w + subX) >> subX) + 34, ((w + subX) >> subX) + 34};
    std::vector<int> noiseStripe[3];
    for (int plane = 0; plane < NumPlanes; ++plane) {
        const int sh = plane > 0 ? subY : 0;
        noiseStripe[plane].assign(static_cast<std::size_t>(stripes) * (34 >> sh) * stripeW[plane], 0);
    }
    auto ns = [&](int lumaNum, int plane, int i, int x) -> int& {
        const int sh = plane > 0 ? subY : 0;
        return noiseStripe[plane][(static_cast<std::size_t>(lumaNum) * (34 >> sh) + i) * stripeW[plane] + x];
    };
    int lumaNum = 0;
    for (int y = 0; y < (h + 1) / 2; y += 16) {
        g.RandomRegister = p.grain_seed;
        g.RandomRegister ^= ((lumaNum * 37 + 178) & 255) << 8;
        g.RandomRegister ^= ((lumaNum * 173 + 105) & 255);
        for (int x = 0; x < (w + 1) / 2; x += 16) {
            const int rnd = g.get_random_number(8);
            const int offsetX = rnd >> 4;
            const int offsetY = rnd & 15;
            for (int plane = 0; plane < NumPlanes; ++plane) {
                const int planeSubX = plane > 0 ? subX : 0;
                const int planeSubY = plane > 0 ? subY : 0;
                const int planeOffsetX = planeSubX ? 6 + offsetX : 9 + offsetX * 2;
                const int planeOffsetY = planeSubY ? 6 + offsetY : 9 + offsetY * 2;
                for (int i = 0; i < (34 >> planeSubY); ++i) {
                    for (int j = 0; j < (34 >> planeSubX); ++j) {
                        int gv;
                        if (plane == 0) {
                            gv = LumaGrain[planeOffsetY + i][planeOffsetX + j];
                        } else if (plane == 1) {
                            gv = CbGrain[planeOffsetY + i][planeOffsetX + j];
                        } else {
                            gv = CrGrain[planeOffsetY + i][planeOffsetX + j];
                        }
                        if (planeSubX == 0) {
                            if (j < 2 && p.overlap_flag && x > 0) {
                                const int old = ns(lumaNum, plane, i, x * 2 + j);
                                if (j == 0) {
                                    gv = old * 27 + gv * 17;
                                } else {
                                    gv = old * 17 + gv * 27;
                                }
                                gv = clip3(GrainMin, GrainMax, round2(gv, 5));
                            }
                            ns(lumaNum, plane, i, x * 2 + j) = gv;
                        } else {
                            if (j == 0 && p.overlap_flag && x > 0) {
                                const int old = ns(lumaNum, plane, i, x + j);
                                gv = old * 23 + gv * 22;
                                gv = clip3(GrainMin, GrainMax, round2(gv, 5));
                            }
                            ns(lumaNum, plane, i, x + j) = gv;
                        }
                    }
                }
            }
        }
        ++lumaNum;
    }
    auto noise_at = [&](int plane, int y, int x) {
        const int planeSubY = plane > 0 ? subY : 0;
        const int ln = y >> (5 - planeSubY);
        const int i = y - (ln << (5 - planeSubY));
        int gv = ns(ln, plane, i, x);
        if (planeSubY == 0) {
            if (i < 2 && ln > 0 && p.overlap_flag) {
                const int old = ns(ln - 1, plane, i + 32, x);
                if (i == 0) {
                    gv = old * 27 + gv * 17;
                } else {
                    gv = old * 17 + gv * 27;
                }
                gv = clip3(GrainMin, GrainMax, round2(gv, 5));
            }
        } else {
            if (i < 1 && ln > 0 && p.overlap_flag) {
                const int old = ns(ln - 1, plane, i + 16, x);
                gv = old * 23 + gv * 22;
                gv = clip3(GrainMin, GrainMax, round2(gv, 5));
            }
        }
        return gv;
    };
    int minValue;
    int maxLuma;
    int maxChroma;
    if (p.clip_to_restricted_range) {
        minValue = 16 << (BitDepth - 8);
        maxLuma = 235 << (BitDepth - 8);
        if (matrix_coefficients == MC_IDENTITY) {
            maxChroma = maxLuma;
        } else {
            maxChroma = 240 << (BitDepth - 8);
        }
    } else {
        minValue = 0;
        maxLuma = (256 << (BitDepth - 8)) - 1;
        maxChroma = maxLuma;
    }
    const int maxv = (1 << BitDepth) - 1;
    const int ScalingShift = p.grain_scaling_minus_8 + 8;
    const Plane& Y = in.planes[0];
    if (!mono) {
        for (int y = 0; y < ((h + subY) >> subY); ++y) {
            for (int x = 0; x < ((w + subX) >> subX); ++x) {
                const int lumaX = x << subX;
                const int lumaY = y << subY;
                const int lumaNextX = imin(lumaX + 1, w - 1);
                int averageLuma;
                if (subX) {
                    averageLuma = round2(Y.row(lumaY)[lumaX] + Y.row(lumaY)[lumaNextX], 1);
                } else {
                    averageLuma = Y.row(lumaY)[lumaX];
                }
                if (p.num_cb_points > 0 || p.chroma_scaling_from_luma) {
                    const int orig = in.planes[1].row(y)[x];
                    int merged;
                    if (p.chroma_scaling_from_luma) {
                        merged = averageLuma;
                    } else {
                        const int combined = averageLuma * (p.cb_luma_mult - 128) + orig * (p.cb_mult - 128);
                        merged = clip3(0, maxv, (combined >> 6) + ((p.cb_offset - 256) * (1 << (BitDepth - 8))));
                    }
                    int noise = noise_at(1, y, x);
                    noise = round2(scale_lut(1, merged) * noise, ScalingShift);
                    out.planes[1].row(y)[x] = static_cast<std::uint16_t>(clip3(minValue, maxChroma, orig + noise));
                }
                if (p.num_cr_points > 0 || p.chroma_scaling_from_luma) {
                    const int orig = in.planes[2].row(y)[x];
                    int merged;
                    if (p.chroma_scaling_from_luma) {
                        merged = averageLuma;
                    } else {
                        const int combined = averageLuma * (p.cr_luma_mult - 128) + orig * (p.cr_mult - 128);
                        merged = clip3(0, maxv, (combined >> 6) + ((p.cr_offset - 256) * (1 << (BitDepth - 8))));
                    }
                    int noise = noise_at(2, y, x);
                    noise = round2(scale_lut(2, merged) * noise, ScalingShift);
                    out.planes[2].row(y)[x] = static_cast<std::uint16_t>(clip3(minValue, maxChroma, orig + noise));
                }
            }
        }
    }
    if (p.num_y_points > 0) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int orig = Y.row(y)[x];
                int noise = noise_at(0, y, x);
                noise = round2(scale_lut(0, orig) * noise, ScalingShift);
                out.planes[0].row(y)[x] = static_cast<std::uint16_t>(clip3(minValue, maxLuma, orig + noise));
            }
        }
    }
}

} // namespace av1
} // namespace video
} // namespace fizmo
