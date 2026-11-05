#include "av1_internal.hpp"

namespace fizmo {
namespace video {
namespace av1 {

namespace {

inline std::int32_t r2_12(std::int64_t x) { return static_cast<std::int32_t>((x + 2048) >> 12); }

void inverse_dct(std::int32_t* T, int n, int r) {
    switch (n) {
    case 2: fast_idct4(T, r); break;
    case 3: fast_idct8(T, r); break;
    case 4: fast_idct16(T, r); break;
    case 5: fast_idct32(T, r); break;
    default:
        AV1_STAT(ST_DCT64);
        fast_idct64(T, r);
        break;
    }
}

void inverse_adst4(std::int32_t* T) {
    AV1_STAT(ST_ADST4);
    const std::int64_t SINPI_1_9 = 1321;
    const std::int64_t SINPI_2_9 = 2482;
    const std::int64_t SINPI_3_9 = 3344;
    const std::int64_t SINPI_4_9 = 3803;
    std::int64_t s[7];
    std::int64_t x[4];
    s[0] = SINPI_1_9 * T[0];
    s[1] = SINPI_2_9 * T[0];
    s[2] = SINPI_3_9 * T[1];
    s[3] = SINPI_4_9 * T[2];
    s[4] = SINPI_1_9 * T[2];
    s[5] = SINPI_2_9 * T[3];
    s[6] = SINPI_4_9 * T[3];
    const std::int64_t a7 = static_cast<std::int64_t>(T[0]) - T[2];
    const std::int64_t b7 = a7 + T[3];
    s[0] = s[0] + s[3];
    s[1] = s[1] - s[4];
    s[3] = s[2];
    s[2] = SINPI_3_9 * b7;
    s[0] = s[0] + s[5];
    s[1] = s[1] - s[6];
    x[0] = s[0] + s[3];
    x[1] = s[1] + s[3];
    x[2] = s[2];
    x[3] = s[0] + s[1];
    x[3] = x[3] - s[3];
    T[0] = r2_12(x[0]);
    T[1] = r2_12(x[1]);
    T[2] = r2_12(x[2]);
    T[3] = r2_12(x[3]);
}

void inverse_adst(std::int32_t* T, int n, int r) {
    if (n == 2) {
        inverse_adst4(T);
    } else if (n == 3) {
        AV1_STAT(ST_ADST8);
        fast_iadst8(T, r);
    } else {
        AV1_STAT(ST_ADST16);
        fast_iadst16(T, r);
    }
}

void inverse_wht(std::int32_t* T, int shift) {
    std::int32_t a = T[0] >> shift;
    std::int32_t c = T[1] >> shift;
    std::int32_t dd = T[2] >> shift;
    std::int32_t b = T[3] >> shift;
    a += c;
    dd -= b;
    const std::int32_t e = (a - dd) >> 1;
    b = e - b;
    c = e - c;
    a -= b;
    dd += c;
    T[0] = a;
    T[1] = b;
    T[2] = c;
    T[3] = dd;
}

void inverse_identity(std::int32_t* T, int n) {
    const int n0 = 1 << n;
    if (n == 2) {
        for (int i = 0; i < n0; ++i) T[i] = r2_12(static_cast<std::int64_t>(T[i]) * 5793);
    } else if (n == 3) {
        for (int i = 0; i < n0; ++i) T[i] = T[i] * 2;
    } else if (n == 4) {
        for (int i = 0; i < n0; ++i) T[i] = r2_12(static_cast<std::int64_t>(T[i]) * 11586);
    } else {
        AV1_STAT(ST_IDENT32);
        for (int i = 0; i < n0; ++i) T[i] = T[i] * 4;
    }
}

int row_kind(int txType) {
    switch (txType) {
    case DCT_DCT: case ADST_DCT: case FLIPADST_DCT: case H_DCT: return 0;
    case DCT_ADST: case ADST_ADST: case DCT_FLIPADST: case FLIPADST_FLIPADST: case ADST_FLIPADST: case FLIPADST_ADST: case H_ADST: case H_FLIPADST: return 1;
    default: return 2;
    }
}

int col_kind(int txType) {
    switch (txType) {
    case DCT_DCT: case DCT_ADST: case DCT_FLIPADST: case V_DCT: return 0;
    case ADST_DCT: case ADST_ADST: case FLIPADST_DCT: case FLIPADST_FLIPADST: case ADST_FLIPADST: case FLIPADST_ADST: case V_ADST: case V_FLIPADST: return 1;
    default: return 2;
    }
}

inline std::int32_t round2_32(std::int32_t x, int n) {
    if (n == 0) return x;
    return static_cast<std::int32_t>((static_cast<std::int64_t>(x) + (std::int64_t(1) << (n - 1))) >> n);
}

} // namespace

void inverse_transform_2d(std::int32_t* coeffs, int txSz, int txType, int lossless, int bitDepth, int rowsNonZero, int colsNonZero, std::int32_t* residual) {
    const int log2W = Tx_Width_Log2[txSz];
    const int log2H = Tx_Height_Log2[txSz];
    const int w = 1 << log2W;
    const int h = 1 << log2H;
    const int rowShift = lossless ? 0 : Transform_Row_Shift[txSz];
    const int colShift = lossless ? 0 : 4;
    const int rowClampRange = bitDepth + 8;
    const int colClampRange = imax(bitDepth + 6, 16);
    const std::int32_t clo = -(1 << (colClampRange - 1));
    const std::int32_t chi = (1 << (colClampRange - 1)) - 1;
    const int rk = row_kind(txType);
    const int ck = col_kind(txType);
    std::int32_t T[64];
    for (int i = 0; i < h; ++i) {
        std::int32_t* out = residual + i * w;
        if (i >= rowsNonZero) {
            for (int j = 0; j < w; ++j) out[j] = 0;
            continue;
        }
        for (int j = 0; j < w; ++j) T[j] = (i < 32 && j < 32 && j < colsNonZero) ? coeffs[i * w + j] : 0;
        if (iabs(log2W - log2H) == 1) {
            for (int j = 0; j < w; ++j) T[j] = r2_12(static_cast<std::int64_t>(T[j]) * 2896);
        }
        if (lossless) {
            inverse_wht(T, 2);
        } else if (rk == 0) {
            inverse_dct(T, log2W, rowClampRange);
        } else if (rk == 1) {
            inverse_adst(T, log2W, rowClampRange);
        } else {
            inverse_identity(T, log2W);
        }
        for (int j = 0; j < w; ++j) {
            std::int32_t v = round2_32(T[j], rowShift);
            if (v < clo) v = clo;
            if (v > chi) v = chi;
            out[j] = v;
        }
    }
    for (int j = 0; j < w; ++j) {
        for (int i = 0; i < h; ++i) T[i] = residual[i * w + j];
        if (lossless) {
            inverse_wht(T, 0);
        } else if (ck == 0) {
            inverse_dct(T, log2H, colClampRange);
        } else if (ck == 1) {
            inverse_adst(T, log2H, colClampRange);
        } else {
            inverse_identity(T, log2H);
        }
        for (int i = 0; i < h; ++i) residual[i * w + j] = round2_32(T[i], colShift);
    }
}

void Tile::reconstruct(int plane, int x, int y, int txSz) {
    const int BitDepth = seq->BitDepth;
    int dqDenom;
    if (txSz == TX_32X32 || txSz == TX_16X32 || txSz == TX_32X16 || txSz == TX_16X64 || txSz == TX_64X16) {
        dqDenom = 2;
    } else if (txSz == TX_64X64 || txSz == TX_32X64 || txSz == TX_64X32) {
        dqDenom = 4;
    } else {
        dqDenom = 1;
    }
    const int log2W = Tx_Width_Log2[txSz];
    const int log2H = Tx_Height_Log2[txSz];
    const int w = 1 << log2W;
    const int h = 1 << log2H;
    const int tw = imin(32, w);
    const int th = imin(32, h);
    const int flipUD = PlaneTxType == FLIPADST_DCT || PlaneTxType == FLIPADST_ADST || PlaneTxType == V_FLIPADST || PlaneTxType == FLIPADST_FLIPADST;
    const int flipLR = PlaneTxType == DCT_FLIPADST || PlaneTxType == ADST_FLIPADST || PlaneTxType == H_FLIPADST || PlaneTxType == FLIPADST_FLIPADST;
    const int qindex = get_qindex(0, segment_id);
    const int bdIdx = (BitDepth - 8) >> 1;
    int dcq;
    int acq;
    if (plane == 0) {
        dcq = Dc_Qlookup[bdIdx][clip3(0, 255, qindex + fh->DeltaQYDc)];
        acq = Ac_Qlookup[bdIdx][clip3(0, 255, qindex)];
    } else if (plane == 1) {
        dcq = Dc_Qlookup[bdIdx][clip3(0, 255, qindex + fh->DeltaQUDc)];
        acq = Ac_Qlookup[bdIdx][clip3(0, 255, qindex + fh->DeltaQUAc)];
    } else {
        dcq = Dc_Qlookup[bdIdx][clip3(0, 255, qindex + fh->DeltaQVDc)];
        acq = Ac_Qlookup[bdIdx][clip3(0, 255, qindex + fh->DeltaQVAc)];
    }
    const int qmLevel = fh->using_qmatrix ? fh->SegQMLevel[plane][segment_id] : 15;
    const bool useQm = fh->using_qmatrix && PlaneTxType < IDTX && qmLevel < 15;
    const std::uint8_t* qm = useQm ? &Quantizer_Matrix[qmLevel][plane > 0][Qm_Offset[txSz]] : nullptr;
    const std::int64_t dqMax = (std::int64_t(1) << (7 + BitDepth)) - 1;
    const std::int64_t dqMin = -(std::int64_t(1) << (7 + BitDepth));
    int rowsNonZero = 0;
    int colsNonZero = 0;
    std::memset(Dequant, 0, sizeof(std::int32_t) * static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    for (int i = 0; i < th; ++i) {
        for (int j = 0; j < tw; ++j) {
            const std::int32_t qv = Quant[i * tw + j];
            if (qv == 0) continue;
            const int q = (i == 0 && j == 0) ? dcq : acq;
            int q2;
            if (useQm) {
                q2 = round2(q * qm[i * tw + j], 5);
            } else {
                q2 = q;
            }
            const std::int64_t dq = static_cast<std::int64_t>(qv) * q2;
            const std::int64_t mag = ((dq < 0 ? -dq : dq) & 0xFFFFFF) / dqDenom;
            std::int64_t dq2 = dq < 0 ? -mag : mag;
            if (dq2 < dqMin) dq2 = dqMin;
            if (dq2 > dqMax) dq2 = dqMax;
            Dequant[i * w + j] = static_cast<std::int32_t>(dq2);
            if (dq2 != 0) {
                if (i + 1 > rowsNonZero) rowsNonZero = i + 1;
                if (j + 1 > colsNonZero) colsNonZero = j + 1;
            }
        }
    }
    inverse_transform_2d(Dequant, txSz, PlaneTxType, Lossless, BitDepth, Lossless ? h : rowsNonZero, Lossless ? w : colsNonZero, Residual);
    Plane& P = d->frame.planes[plane];
    const int maxv = (1 << BitDepth) - 1;
    for (int i = 0; i < h; ++i) {
        const int yy = flipUD ? (h - i - 1) : i;
        std::uint16_t* o = P.row(y + yy) + x;
        const std::int32_t* rr = Residual + i * w;
        if (flipLR) {
            for (int j = 0; j < w; ++j) o[w - j - 1] = static_cast<std::uint16_t>(clip3(0, maxv, o[w - j - 1] + rr[j]));
        } else {
            for (int j = 0; j < w; ++j) o[j] = static_cast<std::uint16_t>(clip3(0, maxv, o[j] + rr[j]));
        }
    }
}

} // namespace av1
} // namespace video
} // namespace fizmo
