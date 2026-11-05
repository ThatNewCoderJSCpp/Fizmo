#include "h264_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace fizmo {
namespace video {
namespace h264_internal {

namespace {

struct CavlcTables {
    Vlc coeff_token[4];
    Vlc chroma_dc_token;
    Vlc chroma422_dc_token;
    Vlc total_zeros[15];
    Vlc chroma_dc_total_zeros[3];
    Vlc chroma422_dc_total_zeros[7];
    Vlc run[7];

    CavlcTables() {
        std::int16_t symbols[68];
        for (int i = 0; i < 68; ++i) symbols[i] = static_cast<std::int16_t>(i);
        for (int t = 0; t < 4; ++t) coeff_token[t].build(k_coeff_token_len[t], k_coeff_token_bits[t], symbols, 68);
        chroma_dc_token.build(k_chroma_dc_coeff_token_len, k_chroma_dc_coeff_token_bits, symbols, 20);
        chroma422_dc_token.build(k_chroma422_dc_coeff_token_len, k_chroma422_dc_coeff_token_bits, symbols, 36);
        for (int t = 0; t < 15; ++t) total_zeros[t].build(k_total_zeros_len[t], k_total_zeros_bits[t], symbols, 16);
        for (int t = 0; t < 3; ++t) chroma_dc_total_zeros[t].build(k_chroma_dc_total_zeros_len[t], k_chroma_dc_total_zeros_bits[t], symbols, 4);
        for (int t = 0; t < 7; ++t) chroma422_dc_total_zeros[t].build(k_chroma422_dc_total_zeros_len[t], k_chroma422_dc_total_zeros_bits[t], symbols, 8);
        for (int t = 0; t < 7; ++t) run[t].build(k_run_len[t], k_run_bits[t], symbols, 16);
    }
};

const CavlcTables& cavlc_tables() {
    static const CavlcTables tables;
    return tables;
}

const std::uint8_t k_b_pair_pred[9][2] = {
    {1, 1}, {2, 2}, {1, 2}, {2, 1}, {1, 3}, {2, 3}, {3, 1}, {3, 2}, {3, 3},
};

const std::uint8_t k_b_sub_info[13][2] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {1, 1}, {1, 2}, {2, 1}, {2, 2}, {3, 1}, {3, 2}, {1, 3}, {2, 3}, {3, 3},
};

const int k_cbf_base[14] = {85, 89, 93, 97, 101, 1012, 460, 464, 468, 1016, 472, 476, 480, 1020};
const int k_sig_frame[14] = {105, 120, 134, 149, 152, 402, 484, 499, 513, 660, 528, 543, 557, 718};
const int k_sig_field[14] = {277, 292, 306, 321, 324, 436, 776, 791, 805, 675, 820, 835, 849, 733};
const int k_last_frame[14] = {166, 181, 195, 210, 213, 417, 572, 587, 601, 690, 616, 631, 645, 748};
const int k_last_field[14] = {338, 353, 367, 382, 385, 451, 864, 879, 893, 699, 908, 923, 937, 757};
const int k_abs_base[14] = {227, 237, 247, 257, 266, 426, 952, 962, 972, 708, 982, 992, 1002, 766};

inline std::int32_t dequant(std::int32_t level, std::int32_t scale, int shift) noexcept {
    std::int64_t v = static_cast<std::int64_t>(level) * scale;
    v = (v + (static_cast<std::int64_t>(1) << (shift - 1))) >> shift;
    if (v > (1 << 22)) v = 1 << 22;
    if (v < -(1 << 22)) v = -(1 << 22);
    return static_cast<std::int32_t>(v);
}

inline std::int32_t clamp_level(std::int64_t v) noexcept {
    if (v > (1 << 22)) return 1 << 22;
    if (v < -(1 << 22)) return -(1 << 22);
    return static_cast<std::int32_t>(v);
}

inline int cat_class(int cat) noexcept {
    if (cat == 0 || cat == 6 || cat == 10) return 0;
    if (cat == 1 || cat == 7 || cat == 11) return 1;
    if (cat == 2 || cat == 8 || cat == 12) return 2;
    if (cat == 5 || cat == 9 || cat == 13) return 5;
    return cat;
}

} // namespace

int DecoderState::nb_loc(int xn, int yn, int maxw, int maxh, int* xw, int* yw) const noexcept {
    if (yn > maxh - 1) return -1;
    int mbn = -1;
    int ym = yn;
    if (!mbaff) {
        if (xn < 0) {
            mbn = yn < 0 ? simple_nb[3] : simple_nb[0];
        } else if (xn < maxw) {
            mbn = yn < 0 ? simple_nb[1] : mb_addr;
        } else {
            if (yn >= 0) return -1;
            mbn = simple_nb[2];
        }
        if (mbn < 0) return -1;
        *xw = (xn + maxw) % maxw;
        *yw = (yn + maxh) % maxh;
        return mbn;
    }
    bool frame = !mb_field;
    bool top = !mb_bottom;
    int pa = pair_nb[0];
    int pb = pair_nb[1];
    int pc = pair_nb[2];
    int pd = pair_nb[3];
    auto fld = [this](int p) { return mbs[static_cast<std::size_t>(p)].field != 0; };
    if (xn < 0 && yn < 0) {
        if (frame) {
            if (top) {
                if (pd < 0) return -1;
                mbn = pd + 1;
            } else {
                if (pa < 0) return -1;
                if (!fld(pa)) {
                    mbn = pa;
                } else {
                    mbn = pa + 1;
                    ym = (yn + maxh) >> 1;
                }
            }
        } else {
            if (pd < 0) return -1;
            if (top) {
                if (!fld(pd)) {
                    mbn = pd + 1;
                    ym = 2 * yn;
                } else {
                    mbn = pd;
                }
            } else {
                mbn = pd + 1;
            }
        }
    } else if (xn < 0) {
        if (pa < 0) return -1;
        bool af = fld(pa);
        if (frame) {
            if (top) {
                if (!af) {
                    mbn = pa;
                } else {
                    mbn = (yn % 2 == 0) ? pa : pa + 1;
                    ym = yn >> 1;
                }
            } else {
                if (!af) {
                    mbn = pa + 1;
                } else {
                    mbn = (yn % 2 == 0) ? pa : pa + 1;
                    ym = (yn + maxh) >> 1;
                }
            }
        } else {
            if (top) {
                if (!af) {
                    if (yn < maxh / 2) {
                        mbn = pa;
                        ym = yn << 1;
                    } else {
                        mbn = pa + 1;
                        ym = (yn << 1) - maxh;
                    }
                } else {
                    mbn = pa;
                }
            } else {
                if (!af) {
                    if (yn < maxh / 2) {
                        mbn = pa;
                        ym = (yn << 1) + 1;
                    } else {
                        mbn = pa + 1;
                        ym = (yn << 1) + 1 - maxh;
                    }
                } else {
                    mbn = pa + 1;
                }
            }
        }
    } else if (xn < maxw) {
        if (yn >= 0) {
            mbn = mb_addr;
        } else if (frame) {
            if (top) {
                if (pb < 0) return -1;
                mbn = pb + 1;
            } else {
                mbn = mb_addr - 1;
            }
        } else {
            if (pb < 0) return -1;
            if (top) {
                if (!fld(pb)) {
                    mbn = pb + 1;
                    ym = 2 * yn;
                } else {
                    mbn = pb;
                }
            } else {
                mbn = pb + 1;
            }
        }
    } else {
        if (yn >= 0) return -1;
        if (frame) {
            if (!top || pc < 0) return -1;
            mbn = pc + 1;
        } else {
            if (pc < 0) return -1;
            if (top) {
                if (!fld(pc)) {
                    mbn = pc + 1;
                    ym = 2 * yn;
                } else {
                    mbn = pc;
                }
            } else {
                mbn = pc + 1;
            }
        }
    }
    *xw = (xn + maxw) % maxw;
    *yw = (ym + maxh) % maxh;
    return mbn;
}

int DecoderState::nb_mb_a() const noexcept {
    int xw = 0;
    int yw = 0;
    return nb_loc(-1, 0, 16, 16, &xw, &yw);
}

int DecoderState::nb_mb_b() const noexcept {
    int xw = 0;
    int yw = 0;
    return nb_loc(0, -1, 16, 16, &xw, &yw);
}

void DecoderState::setup_mb_neighbors() {
    for (int i = 0; i < 4; ++i) {
        simple_nb[i] = -1;
        pair_nb[i] = -1;
    }
    if (mbaff) {
        int pair = mb_addr >> 1;
        int px = pair % mb_width;
        int py = pair / mb_width;
        auto ok = [this](int p) { return p >= 0 && 2 * p < pic_mb_count && mbs[static_cast<std::size_t>(2 * p)].slice == slice_num; };
        if (px > 0 && ok(pair - 1)) pair_nb[0] = 2 * (pair - 1);
        if (py > 0 && ok(pair - mb_width)) pair_nb[1] = 2 * (pair - mb_width);
        if (py > 0 && px < mb_width - 1 && ok(pair - mb_width + 1)) pair_nb[2] = 2 * (pair - mb_width + 1);
        if (py > 0 && px > 0 && ok(pair - mb_width - 1)) pair_nb[3] = 2 * (pair - mb_width - 1);
        return;
    }
    if (mb_x > 0 && mb_is_available(mb_addr - 1)) simple_nb[0] = mb_addr - 1;
    if (mb_y > 0 && mb_is_available(mb_addr - mb_width)) simple_nb[1] = mb_addr - mb_width;
    if (mb_y > 0 && mb_x < mb_width - 1 && mb_is_available(mb_addr - mb_width + 1)) simple_nb[2] = mb_addr - mb_width + 1;
    if (mb_y > 0 && mb_x > 0 && mb_is_available(mb_addr - mb_width - 1)) simple_nb[3] = mb_addr - mb_width - 1;
}

void DecoderState::setup_mb_position() {
    int px;
    int py;
    int step = 1;
    if (mbaff) {
        int pair = mb_addr >> 1;
        pair_x = pair % mb_width;
        pair_y = pair / mb_width;
        mb_bottom = (mb_addr & 1) != 0;
        mb_x = pair_x;
        mb_y = pair_y * 2 + (mb_bottom ? 1 : 0);
        px = pair_x * 16;
        if (mb_field) {
            py = pair_y * 32 + (mb_bottom ? 1 : 0);
            step = 2;
        } else {
            py = pair_y * 32 + (mb_bottom ? 16 : 0);
        }
    } else {
        mb_bottom = false;
        mb_x = mb_addr % mb_width;
        mb_y = mb_addr / mb_width;
        px = mb_x * 16;
        py = mb_y * 16;
    }
    std::size_t b = static_cast<std::size_t>(cur_pixel_bytes);
    for (int c = 0; c < 3; ++c) {
        int cx = px;
        int cy = py;
        int cs = 16;
        if (c > 0 && num_planes == 1) {
            if (chroma_array_type == 0) {
                mb_pix[c] = nullptr;
                mb_pstride[c] = 0;
                continue;
            }
            cx = px / sub_w;
            if (mbaff) {
                int pyc = pair_y * 32 / sub_h;
                cy = mb_field ? pyc + (mb_bottom ? 1 : 0) : pyc + (mb_bottom ? 16 / sub_h : 0);
            } else {
                cy = py / sub_h;
            }
            cs = mb_wc;
        }
        (void)cs;
        int plane_index = c;
        if (num_planes > 1) plane_index = c == 0 ? colour_plane : c;
        mb_pix[c] = cv.plane[plane_index] + (static_cast<std::size_t>(cy) * static_cast<std::size_t>(cv.stride[plane_index]) + static_cast<std::size_t>(cx)) * b;
        mb_pstride[c] = cv.stride[plane_index] * step;
    }
}

void DecoderState::infer_pair_field() {
    int pair = mb_addr >> 1;
    int px = pair % mb_width;
    int py = pair / mb_width;
    pair_field_flag = field_pic;
    if (!mbaff) return;
    pair_field_flag = false;
    if (px > 0 && mbs[static_cast<std::size_t>(2 * (pair - 1))].slice == slice_num) {
        pair_field_flag = mbs[static_cast<std::size_t>(2 * (pair - 1))].field != 0;
    } else if (py > 0 && mbs[static_cast<std::size_t>(2 * (pair - mb_width))].slice == slice_num) {
        pair_field_flag = mbs[static_cast<std::size_t>(2 * (pair - mb_width))].field != 0;
    }
}

int DecoderState::luma_nc(int plane, int blk) const noexcept {
    int x = (blk & 3) * 4;
    int y = (blk >> 2) * 4;
    int na = -1;
    int nb = -1;
    int xw = 0;
    int yw = 0;
    int a = nb_loc(x - 1, y, 16, 16, &xw, &yw);
    if (a >= 0) na = nnz_at(a, plane, (yw >> 2) * 4 + (xw >> 2));
    int b = nb_loc(x, y - 1, 16, 16, &xw, &yw);
    if (b >= 0) nb = nnz_at(b, plane, (yw >> 2) * 4 + (xw >> 2));
    if (na >= 0 && nb >= 0) return (na + nb + 1) >> 1;
    if (na >= 0) return na;
    if (nb >= 0) return nb;
    return 0;
}

int DecoderState::chroma_nc(int comp, int blk) const noexcept {
    int bw = mb_wc / 4;
    int x = (blk % bw) * 4;
    int y = (blk / bw) * 4;
    int na = -1;
    int nb = -1;
    int xw = 0;
    int yw = 0;
    int a = nb_loc(x - 1, y, mb_wc, mb_hc, &xw, &yw);
    if (a >= 0) na = nnz_at(a, 1 + comp, (yw >> 2) * bw + (xw >> 2));
    int b = nb_loc(x, y - 1, mb_wc, mb_hc, &xw, &yw);
    if (b >= 0) nb = nnz_at(b, 1 + comp, (yw >> 2) * bw + (xw >> 2));
    if (na >= 0 && nb >= 0) return (na + nb + 1) >> 1;
    if (na >= 0) return na;
    if (nb >= 0) return nb;
    return 0;
}

int DecoderState::cavlc_nc_luma(int bx, int by) const {
    return luma_nc(0, (by & 3) * 4 + (bx & 3));
}

int DecoderState::cavlc_nc_chroma(int comp, int cx, int cy) const {
    return chroma_nc(comp, (cy % std::max(1, mb_hc / 4)) * std::max(1, mb_wc / 4) + (cx % std::max(1, mb_wc / 4)));
}

int DecoderState::read_cavlc_coeffs(int nc, int max_coeff, int* pos, std::int32_t* lev) {
    const CavlcTables& t = cavlc_tables();
    int sym;
    if (nc == -1) {
        sym = t.chroma_dc_token.read(br);
    } else if (nc == -2) {
        sym = t.chroma422_dc_token.read(br);
    } else {
        int table = nc < 2 ? 0 : (nc < 4 ? 1 : (nc < 8 ? 2 : 3));
        sym = t.coeff_token[table].read(br);
    }
    if (sym < 0) return -1;
    int total = sym >> 2;
    int trailing = sym & 3;
    if (total == 0) return 0;
    if (total > max_coeff || trailing > total) return -1;
    std::int32_t levels[16];
    int suffix_length = (total > 10 && trailing < 3) ? 1 : 0;
    for (int i = 0; i < total; ++i) {
        if (i < trailing) {
            levels[i] = br.u(1) ? -1 : 1;
            continue;
        }
        std::uint32_t peek = br.peek32();
        int prefix = count_leading_zeros32(peek);
        if (prefix > 28) return -1;
        br.skip(static_cast<std::size_t>(prefix + 1));
        std::int64_t level_code = static_cast<std::int64_t>(std::min(15, prefix)) << suffix_length;
        if (suffix_length > 0 || prefix >= 14) {
            int size;
            if (prefix == 14 && suffix_length == 0) {
                size = 4;
            } else if (prefix >= 15) {
                size = prefix - 3;
            } else {
                size = suffix_length;
            }
            if (size > 0) level_code += br.u(size);
        }
        if (prefix >= 15 && suffix_length == 0) level_code += 15;
        if (prefix >= 16) level_code += (static_cast<std::int64_t>(1) << (prefix - 3)) - 4096;
        if (i == trailing && trailing < 3) level_code += 2;
        std::int64_t level = (level_code % 2 == 0) ? (level_code + 2) >> 1 : (-level_code - 1) >> 1;
        levels[i] = clamp_level(level);
        if (suffix_length == 0) suffix_length = 1;
        std::int64_t mag = level < 0 ? -level : level;
        if (mag > (static_cast<std::int64_t>(3) << (suffix_length - 1)) && suffix_length < 6) ++suffix_length;
    }
    int zeros_left = 0;
    if (total < max_coeff) {
        int tz;
        if (nc == -1) {
            tz = t.chroma_dc_total_zeros[total - 1].read(br);
        } else if (nc == -2) {
            tz = t.chroma422_dc_total_zeros[total - 1].read(br);
        } else {
            tz = t.total_zeros[total - 1].read(br);
        }
        if (tz < 0) return -1;
        zeros_left = tz;
    }
    if (total + zeros_left > max_coeff) return -1;
    int runs[16];
    for (int i = 0; i < total - 1; ++i) {
        int run = 0;
        if (zeros_left > 0) {
            run = t.run[std::min(zeros_left, 7) - 1].read(br);
            if (run < 0 || run > zeros_left) return -1;
        }
        runs[i] = run;
        zeros_left -= run;
    }
    runs[total - 1] = zeros_left;
    int coeff_num = -1;
    for (int i = total - 1; i >= 0; --i) {
        coeff_num += runs[i] + 1;
        pos[total - 1 - i] = coeff_num;
        lev[total - 1 - i] = levels[i];
    }
    if (coeff_num >= max_coeff) return -1;
    return total;
}

int DecoderState::read_cabac_block(int cat, int plane, int max_coeff, int* pos, std::int32_t* lev) {
    (void)plane;
    bool fieldc = field_pic || mb_field;
    std::uint8_t* sig = cabac_state + (fieldc ? k_sig_field[cat] : k_sig_frame[cat]);
    std::uint8_t* last = cabac_state + (fieldc ? k_last_field[cat] : k_last_frame[cat]);
    std::uint8_t* abs_ctx = cabac_state + k_abs_base[cat];
    int cls = cat_class(cat);
    int count = 0;
    bool ended = false;
    const int n = max_coeff - 1;
    if (cls == 5) {
        const std::uint8_t* inc_sig = k_sig_8x8_offset[fieldc ? 1 : 0];
        for (int i = 0; i < n; ++i) {
            if (cabac.decision(sig[inc_sig[i]])) {
                pos[count++] = i;
                if (cabac.decision(last[k_last_8x8_offset[i]])) {
                    ended = true;
                    break;
                }
            }
        }
    } else if (cat == 3) {
        int div = chroma_array_type == 2 ? 2 : 1;
        for (int i = 0; i < n; ++i) {
            int inc = std::min(i / div, 2);
            if (cabac.decision(sig[inc])) {
                pos[count++] = i;
                if (cabac.decision(last[inc])) {
                    ended = true;
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < n; ++i) {
            if (cabac.decision(sig[i])) {
                pos[count++] = i;
                if (cabac.decision(last[i])) {
                    ended = true;
                    break;
                }
            }
        }
    }
    if (!ended) pos[count++] = max_coeff - 1;
    int eq1 = 0;
    int gt1 = 0;
    const int gt1_cap = cat == 3 ? 3 : 4;
    for (int j = count - 1; j >= 0; --j) {
        int inc = gt1 != 0 ? 0 : std::min(4, 1 + eq1);
        std::int64_t value;
        if (!cabac.decision(abs_ctx[inc])) {
            value = 1;
            ++eq1;
        } else {
            int inc2 = 5 + std::min(gt1_cap, gt1);
            int prefix = 1;
            while (prefix < 14 && cabac.decision(abs_ctx[inc2])) ++prefix;
            std::int64_t v = prefix;
            if (prefix == 14) {
                int k = 0;
                while (cabac.bypass()) {
                    v += static_cast<std::int64_t>(1) << k;
                    if (++k > 30) return -1;
                }
                while (k--) v += static_cast<std::int64_t>(cabac.bypass()) << k;
            }
            value = v + 1;
            ++gt1;
        }
        if (cabac.bypass()) value = -value;
        lev[j] = clamp_level(value);
    }
    return count;
}

int DecoderState::read_cabac_coeffs(int cat, int max_coeff, int* pos, std::int32_t* lev) {
    return read_cabac_block(cat, 0, max_coeff, pos, lev);
}

int DecoderState::cbf_ctx_inc(int cat, int plane, int blk) const noexcept {
    bool intra = (mbs[static_cast<std::size_t>(mb_addr)].flags & k_mb_intra) != 0;
    int cls = cat_class(cat);
    int cond[2] = {0, 0};
    for (int n = 0; n < 2; ++n) {
        int mbn;
        int blkn = 0;
        int xw = 0;
        int yw = 0;
        if (cls == 0 || cat == 3) {
            mbn = n == 0 ? nb_mb_a() : nb_mb_b();
        } else if (cat == 4) {
            int bw = mb_wc / 4;
            int x = (blk % bw) * 4;
            int y = (blk / bw) * 4;
            mbn = n == 0 ? nb_loc(x - 1, y, mb_wc, mb_hc, &xw, &yw) : nb_loc(x, y - 1, mb_wc, mb_hc, &xw, &yw);
            blkn = (yw >> 2) * bw + (xw >> 2);
        } else {
            int x = (blk & 3) * 4;
            int y = (blk >> 2) * 4;
            mbn = n == 0 ? nb_loc(x - 1, y, 16, 16, &xw, &yw) : nb_loc(x, y - 1, 16, 16, &xw, &yw);
            blkn = (yw >> 2) * 4 + (xw >> 2);
        }
        if (mbn < 0) {
            cond[n] = intra ? 1 : 0;
            continue;
        }
        const MbInfo& m = mbs[static_cast<std::size_t>(mbn)];
        if (m.flags & k_mb_pcm) {
            cond[n] = 1;
            continue;
        }
        bool skip = (m.flags & k_mb_skip) != 0;
        if (cls == 0) {
            cond[n] = (m.flags & k_mb_i16x16) ? ((m.cbf_dc >> plane) & 1) : 0;
        } else if (cat == 3) {
            cond[n] = (!skip && (m.cbp >> 4) != 0) ? ((m.cbf_dc >> (1 + plane)) & 1) : 0;
        } else if (cat == 4) {
            cond[n] = (!skip && (m.cbp >> 4) == 2) ? (nnz_at(mbn, 1 + plane, blkn) != 0) : 0;
        } else {
            int b8 = ((blkn >> 3) << 1) | ((blkn & 3) >> 1);
            bool coded = !skip && ((m.cbp >> b8) & 1);
            if (cls == 5) {
                cond[n] = (coded && m.transform8x8) ? (nnz_at(mbn, plane, blkn) != 0) : 0;
            } else {
                cond[n] = coded ? (nnz_at(mbn, plane, blkn) != 0) : 0;
            }
        }
    }
    return cond[0] + 2 * cond[1];
}

int DecoderState::cabac_cbf_ctx(int cat, int comp, int bx, int by) {
    int blk = cat == 4 ? (by * std::max(1, mb_wc / 4) + bx) : (by * 4 + bx);
    return k_cbf_base[cat] + cbf_ctx_inc(cat, comp, blk);
}

bool DecoderState::read_residual_block(int cat, int plane, int blk, int start, int end, int max_coeff, int* pos, std::int32_t* lev, int* count) {
    (void)start;
    (void)end;
    if (pps->cabac) {
        int cls = cat_class(cat);
        if (cls != 5 || chroma_array_type == 3) {
            int ctx = k_cbf_base[cat] + cbf_ctx_inc(cat, plane, blk);
            if (!cabac.decision(cabac_state[ctx])) {
                *count = 0;
                return true;
            }
        }
        int n = read_cabac_block(cat, plane, max_coeff, pos, lev);
        if (n < 0) return false;
        *count = n;
        return true;
    }
    int nc;
    if (cat == 3) {
        nc = chroma_array_type == 2 ? -2 : -1;
    } else if (cat == 4) {
        nc = chroma_nc(plane, blk);
    } else {
        nc = luma_nc(plane, blk);
    }
    int n = read_cavlc_coeffs(nc, max_coeff, pos, lev);
    if (n < 0) return false;
    *count = n;
    return true;
}

bool DecoderState::read_block(int cat, int comp, int bx, int by, int max_coeff, int* pos, std::int32_t* lev, int* count) {
    int blk = cat == 4 ? (by % std::max(1, mb_hc / 4)) * std::max(1, mb_wc / 4) + (bx % std::max(1, mb_wc / 4)) : (by & 3) * 4 + (bx & 3);
    return read_residual_block(cat, comp, blk, 0, 15, max_coeff, pos, lev, count);
}

bool DecoderState::parse_residual_luma(int plane, int list_intra4, int list_inter4) {
    MbParse& m = mp;
    bool intra = m.kind <= k_kind_pcm;
    int list4 = intra ? list_intra4 : list_inter4;
    int list8 = plane * 2 + (intra ? 0 : 1);
    if (num_planes > 1) list8 = intra ? 0 : 1;
    int qpp = plane == 0 || num_planes > 1 ? qp + qp_bd_y : chroma_qp[plane - 1] + qp_bd_c;
    if (qpp < 0) qpp = 0;
    if (qpp > 91) qpp = 91;
    int base_cat = plane == 0 ? 0 : (plane == 1 ? 6 : 10);
    bool fieldc = field_pic || mb_field;
    const std::uint8_t* scan4 = fieldc ? k_field_scan4x4 : k_zigzag4x4;
    const std::uint8_t* scan8 = fieldc ? k_field_scan8x8 : k_zigzag8x8;
    int pos[64];
    std::int32_t lev[64];
    int count = 0;
    Residual& r = res;
    int cbp_luma = m.cbp & 15;
    bool bypass = mb_bypass;
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    if (m.kind == k_kind_i16x16) {
        if (!read_residual_block(base_cat, plane, 0, 0, 15, 16, pos, lev, &count)) return false;
        if (count > 0) info.cbf_dc = static_cast<std::uint8_t>(info.cbf_dc | (1 << plane));
        if (count > 0) {
            std::int32_t c[16] = {0};
            for (int i = 0; i < count; ++i) c[scan4[pos[i]]] = lev[i];
            if (bypass) {
                for (int k = 0; k < 16; ++k) {
                    if (c[k] == 0) continue;
                    r.c4[plane][k][0] = c[k];
                    r.mask4[plane] |= 1u << k;
                }
            } else {
                std::int32_t t[16];
                for (int i = 0; i < 4; ++i) {
                    std::int32_t a = c[i * 4], b = c[i * 4 + 1], d = c[i * 4 + 2], e = c[i * 4 + 3];
                    t[i * 4] = a + b + d + e;
                    t[i * 4 + 1] = a + b - d - e;
                    t[i * 4 + 2] = a - b - d + e;
                    t[i * 4 + 3] = a - b + d - e;
                }
                std::int32_t scale = dq4[list4][qpp][0];
                for (int j = 0; j < 4; ++j) {
                    std::int32_t a = t[j], b = t[4 + j], d = t[8 + j], e = t[12 + j];
                    std::int32_t f[4] = {a + b + d + e, a + b - d - e, a - b - d + e, a - b + d - e};
                    for (int i = 0; i < 4; ++i) {
                        std::int32_t v = dequant(f[i], scale, 6);
                        if (v == 0) continue;
                        int blk = i * 4 + j;
                        r.c4[plane][blk][0] = v;
                        if (!(r.mask4[plane] & (1u << blk))) r.dc_only4[plane] |= 1u << blk;
                        r.mask4[plane] |= 1u << blk;
                    }
                }
            }
        }
        if (cbp_luma) {
            const std::int32_t* scale = dq4[list4][qpp];
            for (int bi = 0; bi < 16; ++bi) {
                int blk = k_blk_y[bi] * 4 + k_blk_x[bi];
                if (!read_residual_block(base_cat + 1, plane, blk, 1, 15, 15, pos, lev, &count)) return false;
                set_nnz(plane, blk, count);
                if (count == 0) continue;
                for (int i = 0; i < count; ++i) {
                    int p = scan4[pos[i] + 1];
                    r.c4[plane][blk][p] = bypass ? lev[i] : dequant(lev[i], scale[p], 4);
                }
                r.mask4[plane] |= 1u << blk;
                r.dc_only4[plane] &= ~(1u << blk);
            }
        }
        return true;
    }
    for (int b8 = 0; b8 < 4; ++b8) {
        if (!(cbp_luma & (1 << b8))) continue;
        if (m.transform8x8) {
            const std::int32_t* scale = dq8[list8][qpp];
            std::int32_t* dst = r.c8[plane][b8];
            bool any = false;
            int blk0 = (b8 >> 1) * 8 + (b8 & 1) * 2;
            if (pps->cabac) {
                if (!read_residual_block(plane == 0 ? 5 : base_cat + 3, plane, blk0, 0, 63, 64, pos, lev, &count)) return false;
                for (int i = 0; i < count; ++i) {
                    int p = scan8[pos[i]];
                    dst[p] = bypass ? lev[i] : dequant(lev[i], scale[p], 6);
                }
                set_nnz(plane, blk0, count);
                set_nnz(plane, blk0 + 1, count);
                set_nnz(plane, blk0 + 4, count);
                set_nnz(plane, blk0 + 5, count);
                any = count > 0;
            } else {
                for (int s = 0; s < 4; ++s) {
                    int blk = blk0 + (s & 1) + (s >> 1) * 4;
                    if (!read_residual_block(base_cat + 2, plane, blk, 0, 15, 16, pos, lev, &count)) return false;
                    set_nnz(plane, blk, count);
                    for (int i = 0; i < count; ++i) {
                        int p = scan8[pos[i] * 4 + s];
                        dst[p] = bypass ? lev[i] : dequant(lev[i], scale[p], 6);
                    }
                    if (count) any = true;
                }
            }
            if (any) r.mask8[plane] |= 1u << b8;
            continue;
        }
        const std::int32_t* scale = dq4[list4][qpp];
        for (int s = 0; s < 4; ++s) {
            int bi = b8 * 4 + s;
            int blk = k_blk_y[bi] * 4 + k_blk_x[bi];
            if (!read_residual_block(base_cat + 2, plane, blk, 0, 15, 16, pos, lev, &count)) return false;
            set_nnz(plane, blk, count);
            if (count == 0) continue;
            bool dc_only = true;
            for (int i = 0; i < count; ++i) {
                int p = scan4[pos[i]];
                r.c4[plane][blk][p] = bypass ? lev[i] : dequant(lev[i], scale[p], 4);
                if (p != 0) dc_only = false;
            }
            r.mask4[plane] |= 1u << blk;
            if (dc_only && !bypass) {
                r.dc_only4[plane] |= 1u << blk;
            } else {
                r.dc_only4[plane] &= ~(1u << blk);
            }
        }
    }
    return true;
}

bool DecoderState::parse_residual_chroma() {
    MbParse& m = mp;
    bool intra = m.kind <= k_kind_pcm;
    int cbp_chroma = m.cbp >> 4;
    if (cbp_chroma == 0) return true;
    int pos[64];
    std::int32_t lev[64];
    int count = 0;
    Residual& r = res;
    int list_c = intra ? 1 : 4;
    int num_c8 = chroma_array_type == 2 ? 2 : 1;
    int nblk = 4 * num_c8;
    bool bypass = mb_bypass;
    bool fieldc = field_pic || mb_field;
    const std::uint8_t* scan4 = fieldc ? k_field_scan4x4 : k_zigzag4x4;
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    for (int c = 0; c < 2; ++c) {
        if (!read_residual_block(3, c, 0, 0, nblk - 1, nblk, pos, lev, &count)) return false;
        if (count > 0) info.cbf_dc = static_cast<std::uint8_t>(info.cbf_dc | (2 << c));
        if (count == 0) continue;
        int qpc = clip3(0, 91, chroma_qp[c] + qp_bd_c);
        std::int32_t d[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        if (num_c8 == 1) {
            for (int i = 0; i < count; ++i) d[pos[i]] = lev[i];
        } else {
            for (int i = 0; i < count; ++i) d[k_chroma422_dc_scan[pos[i]]] = lev[i];
        }
        std::int32_t f[8];
        if (bypass) {
            for (int b = 0; b < nblk; ++b) f[b] = d[b];
        } else if (num_c8 == 1) {
            std::int32_t t[4] = {d[0] + d[1] + d[2] + d[3], d[0] - d[1] + d[2] - d[3], d[0] + d[1] - d[2] - d[3], d[0] - d[1] - d[2] + d[3]};
            std::int64_t scale = dq4[list_c + c][qpc][0];
            for (int b = 0; b < 4; ++b) f[b] = clamp_level((static_cast<std::int64_t>(t[b]) * scale) >> 5);
        } else {
            std::int32_t rr[4][2];
            for (int i = 0; i < 4; ++i) {
                rr[i][0] = d[i * 2] + d[i * 2 + 1];
                rr[i][1] = d[i * 2] - d[i * 2 + 1];
            }
            int qdc = clip3(0, 91, qpc + 3);
            std::int32_t scale = dq4[list_c + c][qdc][0];
            for (int j = 0; j < 2; ++j) {
                std::int32_t a = rr[0][j], b = rr[1][j], e = rr[2][j], g = rr[3][j];
                std::int32_t col[4] = {a + b + e + g, a + b - e - g, a - b - e + g, a - b + e - g};
                for (int i = 0; i < 4; ++i) f[i * 2 + j] = dequant(col[i], scale, 6);
            }
        }
        for (int b = 0; b < nblk; ++b) {
            if (f[b] == 0) continue;
            r.c4[1 + c][b][0] = f[b];
            if (!(r.mask4[1 + c] & (1u << b))) r.dc_only4[1 + c] |= 1u << b;
            r.mask4[1 + c] |= 1u << b;
        }
    }
    if (cbp_chroma < 2) return true;
    for (int c = 0; c < 2; ++c) {
        int qpc = clip3(0, 91, chroma_qp[c] + qp_bd_c);
        const std::int32_t* scale = dq4[list_c + c][qpc];
        for (int b = 0; b < nblk; ++b) {
            if (!read_residual_block(4, c, b, 1, 15, 15, pos, lev, &count)) return false;
            set_nnz(1 + c, b, count);
            if (count == 0) continue;
            for (int i = 0; i < count; ++i) {
                int p = scan4[pos[i] + 1];
                r.c4[1 + c][b][p] = bypass ? lev[i] : dequant(lev[i], scale[p], 4);
            }
            r.mask4[1 + c] |= 1u << b;
            r.dc_only4[1 + c] &= ~(1u << b);
        }
    }
    return true;
}

bool DecoderState::parse_residual() {
    bool intra = mp.kind <= k_kind_pcm;
    (void)intra;
    if (!parse_residual_luma(0, 0, 3)) return false;
    if (chroma_array_type == 3) {
        if (!parse_residual_luma(1, 1, 4)) return false;
        if (!parse_residual_luma(2, 2, 5)) return false;
        return true;
    }
    if (chroma_array_type == 1 || chroma_array_type == 2) return parse_residual_chroma();
    return true;
}

int DecoderState::cabac_skip_ctx_for(int addr_a, int addr_b) {
    int inc = 0;
    if (addr_a >= 0 && !(mbs[static_cast<std::size_t>(addr_a)].flags & k_mb_skip)) ++inc;
    if (addr_b >= 0 && !(mbs[static_cast<std::size_t>(addr_b)].flags & k_mb_skip)) ++inc;
    return inc;
}

int DecoderState::cabac_mb_skip() {
    int inc = cabac_skip_ctx_for(nb_mb_a(), nb_mb_b());
    int base = sh.slice_type == k_slice_b ? 24 : 11;
    return cabac.decision(cabac_state[base + inc]);
}

int DecoderState::cabac_field_flag() {
    int inc = 0;
    if (pair_nb[0] >= 0 && mbs[static_cast<std::size_t>(pair_nb[0])].field) ++inc;
    if (pair_nb[1] >= 0 && mbs[static_cast<std::size_t>(pair_nb[1])].field) ++inc;
    return cabac.decision(cabac_state[70 + inc]);
}

int DecoderState::cabac_mb_type_i(int ctx_base, bool in_i_slice) {
    std::uint8_t* st = cabac_state + ctx_base;
    if (in_i_slice) {
        int inc = 0;
        int a = nb_mb_a();
        int b = nb_mb_b();
        if (a >= 0 && (mbs[static_cast<std::size_t>(a)].flags & (k_mb_i16x16 | k_mb_pcm))) ++inc;
        if (b >= 0 && (mbs[static_cast<std::size_t>(b)].flags & (k_mb_i16x16 | k_mb_pcm))) ++inc;
        if (!cabac.decision(st[inc])) return 0;
        st += 2;
    } else {
        if (!cabac.decision(st[0])) return 0;
    }
    if (cabac.terminate()) return 25;
    int is = in_i_slice ? 1 : 0;
    int t = 1;
    t += 12 * cabac.decision(st[1]);
    if (cabac.decision(st[2])) t += 4 + 4 * cabac.decision(st[2 + is]);
    t += 2 * cabac.decision(st[3 + is]);
    t += cabac.decision(st[3 + 2 * is]);
    return t;
}

int DecoderState::cabac_ref_idx(int list, int x4, int y4) {
    int cond[2] = {0, 0};
    for (int n = 0; n < 2; ++n) {
        int xw = 0;
        int yw = 0;
        int mbn = n == 0 ? nb_loc(x4 * 4 - 1, y4 * 4, 16, 16, &xw, &yw) : nb_loc(x4 * 4, y4 * 4 - 1, 16, 16, &xw, &yw);
        if (mbn < 0) continue;
        const MbInfo& info = mbs[static_cast<std::size_t>(mbn)];
        if (mbn != mb_addr && (info.flags & (k_mb_skip | k_mb_intra))) continue;
        int blk = (yw >> 2) * 4 + (xw >> 2);
        int b8 = ((yw >> 3) << 1) | (xw >> 3);
        if (info.direct_mask & (1 << b8)) continue;
        int ref = cur->ref_idx[list][(motion_offset + static_cast<std::size_t>(mbn)) * 16 + static_cast<std::size_t>(blk)];
        if (ref < 0) continue;
        bool nf = info.field != 0;
        if (mbaff && !mb_field && nf) {
            if (ref > 1) cond[n] = 1;
        } else if (ref > 0) {
            cond[n] = 1;
        }
    }
    std::uint8_t* st = cabac_state + 54;
    int ref = 0;
    if (!cabac.decision(st[cond[0] + 2 * cond[1]])) return 0;
    ref = 1;
    if (!cabac.decision(st[4])) return ref;
    ref = 2;
    while (cabac.decision(st[5])) {
        if (++ref > 64) return -1;
    }
    return ref;
}

int DecoderState::cabac_mvd(int list, int comp, int bx, int by) {
    int sum = 0;
    for (int n = 0; n < 2; ++n) {
        int xw = 0;
        int yw = 0;
        int mbn = n == 0 ? nb_loc(bx * 4 - 1, by * 4, 16, 16, &xw, &yw) : nb_loc(bx * 4, by * 4 - 1, 16, 16, &xw, &yw);
        if (mbn < 0) continue;
        int blk = (yw >> 2) * 4 + (xw >> 2);
        int v = mvd[list][(static_cast<std::size_t>(mbn) * 16 + static_cast<std::size_t>(blk)) * 2 + static_cast<std::size_t>(comp)];
        if (comp == 1 && mbaff) {
            bool nf = mbs[static_cast<std::size_t>(mbn)].field != 0;
            if (!mb_field && nf) {
                v *= 2;
            } else if (mb_field && !nf) {
                v /= 2;
            }
        }
        sum += v;
    }
    int inc = sum < 3 ? 0 : (sum > 32 ? 2 : 1);
    std::uint8_t* st = cabac_state + (comp == 0 ? 40 : 47);
    if (!cabac.decision(st[inc])) return 0;
    int prefix = 1;
    static const int k_inc[8] = {3, 4, 5, 6, 6, 6, 6, 6};
    while (prefix < 9 && cabac.decision(st[k_inc[prefix - 1]])) ++prefix;
    std::int64_t value = prefix;
    if (prefix >= 9) {
        int k = 3;
        while (cabac.bypass()) {
            value += static_cast<std::int64_t>(1) << k;
            if (++k > 24) {
                mb_error = true;
                return 0;
            }
        }
        while (k--) value += static_cast<std::int64_t>(cabac.bypass()) << k;
    }
    if (value > 65535) value = 65535;
    return cabac.bypass() ? -static_cast<int>(value) : static_cast<int>(value);
}

bool DecoderState::setup_mb_type(int raw) {
    MbParse& m = mp;
    int st = sh.slice_type;
    int itype = -1;
    if (st == k_slice_i) {
        itype = raw;
    } else if (st == k_slice_p) {
        if (raw >= 5) itype = raw - 5;
    } else {
        if (raw >= 23) itype = raw - 23;
    }
    if (itype >= 0) {
        if (itype > 25) return false;
        if (itype == 0) {
            m.kind = k_kind_i4x4;
        } else if (itype == 25) {
            m.kind = k_kind_pcm;
        } else {
            m.kind = k_kind_i16x16;
            m.i16_mode = (itype - 1) % 4;
            m.i16_cbp_chroma = ((itype - 1) / 4) % 3;
            m.i16_cbp_luma = itype >= 13 ? 15 : 0;
        }
        return true;
    }
    if (st == k_slice_p) {
        if (raw < 0 || raw > 4) return false;
        if (raw == 0) {
            m.kind = k_kind_16x16;
            m.part_pred[0] = 1;
        } else if (raw == 1 || raw == 2) {
            m.kind = raw == 1 ? k_kind_16x8 : k_kind_8x16;
            m.part_pred[0] = m.part_pred[1] = 1;
        } else {
            m.kind = k_kind_8x8;
            m.ref0 = raw == 4;
        }
        return true;
    }
    if (raw < 0 || raw > 22) return false;
    if (raw == 0) {
        m.kind = k_kind_b_direct;
    } else if (raw <= 3) {
        m.kind = k_kind_16x16;
        m.part_pred[0] = raw;
    } else if (raw == 22) {
        m.kind = k_kind_8x8;
    } else {
        m.kind = (raw % 2 == 0) ? k_kind_16x8 : k_kind_8x16;
        int pair = (raw - 4) / 2;
        m.part_pred[0] = k_b_pair_pred[pair][0];
        m.part_pred[1] = k_b_pair_pred[pair][1];
    }
    return true;
}

namespace {

void write_ref(Picture* cur, std::size_t base, int x4, int y4, int w4, int h4, int list, int ref) {
    for (int y = 0; y < h4; ++y) {
        for (int x = 0; x < w4; ++x) cur->ref_idx[list][base + static_cast<std::size_t>((y4 + y) * 4 + x4 + x)] = static_cast<std::int8_t>(ref);
    }
}

} // namespace

bool DecoderState::parse_mb(bool skip_known_false) {
    (void)skip_known_false;
    MbParse& m = mp;
    bool cabac_mode = pps->cabac;
    int st = sh.slice_type;
    int raw;
    if (cabac_mode) {
        if (st == k_slice_i) {
            raw = cabac_mb_type_i(3, true);
        } else if (st == k_slice_p) {
            if (cabac.decision(cabac_state[14])) {
                raw = 5 + cabac_mb_type_i(17, false);
            } else if (!cabac.decision(cabac_state[15])) {
                raw = cabac.decision(cabac_state[16]) ? 3 : 0;
            } else {
                raw = cabac.decision(cabac_state[17]) ? 1 : 2;
            }
        } else {
            int inc = 0;
            int a = nb_mb_a();
            int b = nb_mb_b();
            if (a >= 0 && !(mbs[static_cast<std::size_t>(a)].flags & k_mb_direct)) ++inc;
            if (b >= 0 && !(mbs[static_cast<std::size_t>(b)].flags & k_mb_direct)) ++inc;
            if (!cabac.decision(cabac_state[27 + inc])) {
                raw = 0;
            } else if (!cabac.decision(cabac_state[27 + 3])) {
                raw = 1 + cabac.decision(cabac_state[27 + 5]);
            } else {
                int bits = cabac.decision(cabac_state[27 + 4]) << 3;
                bits |= cabac.decision(cabac_state[27 + 5]) << 2;
                bits |= cabac.decision(cabac_state[27 + 5]) << 1;
                bits |= cabac.decision(cabac_state[27 + 5]);
                if (bits < 8) {
                    raw = bits + 3;
                } else if (bits == 13) {
                    raw = 23 + cabac_mb_type_i(32, false);
                } else if (bits == 14) {
                    raw = 11;
                } else if (bits == 15) {
                    raw = 22;
                } else {
                    bits = (bits << 1) | cabac.decision(cabac_state[27 + 5]);
                    raw = bits - 4;
                }
            }
        }
    } else {
        std::uint32_t v = br.ue();
        if (v > 48) return false;
        raw = static_cast<int>(v);
    }
    if (!setup_mb_type(raw)) return false;
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    bool intra = m.kind <= k_kind_pcm;
    int flags = intra ? k_mb_intra : k_mb_inter;
    if (m.kind == k_kind_i4x4) flags |= k_mb_i4x4;
    if (m.kind == k_kind_i16x16) flags |= k_mb_i16x16;
    if (m.kind == k_kind_pcm) flags |= k_mb_pcm;
    if (m.kind == k_kind_b_direct) flags |= k_mb_direct;
    if (mb_field) flags |= k_mb_field;
    info.flags = static_cast<std::uint16_t>(flags);
    bool has_chroma = chroma_array_type == 1 || chroma_array_type == 2;
    std::size_t mbase = (motion_offset + static_cast<std::size_t>(mb_addr)) * 16;
    int nref[2] = {ref_count_mb(0), ref_count_mb(1)};
    bool ref_present_field = mbaff && mb_field;

    if (m.kind == k_kind_pcm) {
        int nc = chroma_array_type == 0 ? 0 : 2 * mb_wc * mb_hc;
        std::size_t bits = static_cast<std::size_t>(256 * bit_depth_y + nc * bit_depth_c);
        std::size_t need = bits / 8;
        std::size_t start;
        if (cabac_mode) {
            start = cabac_data_offset + cabac.pcm_start();
        } else {
            br.align();
            start = br.position() >> 3;
        }
        if (start > br.size_bytes() || start + need > br.size_bytes()) return false;
        BitReader pr;
        pr.init(br.data() + start, need + 4 <= br.size_bytes() - start ? need + 4 : br.size_bytes() - start);
        for (int i = 0; i < 256; ++i) m.pcm16[i] = static_cast<std::uint16_t>(pr.u(bit_depth_y));
        for (int i = 0; i < nc; ++i) m.pcm16[256 + i] = static_cast<std::uint16_t>(pr.u(bit_depth_c));
        if (cabac_mode) {
            cabac_data_offset = start + need;
            cabac.init(br.data() + cabac_data_offset, br.size_bytes() - cabac_data_offset);
        } else {
            br.set_position((start + need) * 8);
        }
        m.cbp = 0x2F;
        for (int i = 0; i < 48; ++i) nnz[static_cast<std::size_t>(mb_addr) * 48 + static_cast<std::size_t>(i)] = 16;
        info.cbf_dc = 7;
        last_qp_delta = 0;
        store_mb_info(flags | k_mb_pcm);
        info.cbp = 0x2F;
        reconstruct_mb();
        return true;
    }

    if (intra) {
        if (m.kind == k_kind_i4x4) {
            if (pps->transform_8x8_mode) {
                if (cabac_mode) {
                    int inc = 0;
                    int a = nb_mb_a();
                    int b = nb_mb_b();
                    if (a >= 0 && mbs[static_cast<std::size_t>(a)].transform8x8) ++inc;
                    if (b >= 0 && mbs[static_cast<std::size_t>(b)].transform8x8) ++inc;
                    m.transform8x8 = cabac.decision(cabac_state[399 + inc]) != 0;
                } else {
                    m.transform8x8 = br.flag();
                }
            }
            if (m.transform8x8) {
                m.kind = k_kind_i8x8;
                flags = (flags & ~k_mb_i4x4) | k_mb_i8x8;
                info.flags = static_cast<std::uint16_t>(flags);
            }
            int n = m.transform8x8 ? 4 : 16;
            for (int i = 0; i < n; ++i) {
                if (cabac_mode) {
                    m.prev_flag[i] = static_cast<std::int8_t>(cabac.decision(cabac_state[68]));
                    if (!m.prev_flag[i]) {
                        int v = cabac.decision(cabac_state[69]);
                        v |= cabac.decision(cabac_state[69]) << 1;
                        v |= cabac.decision(cabac_state[69]) << 2;
                        m.rem_mode[i] = static_cast<std::int8_t>(v);
                    }
                } else {
                    m.prev_flag[i] = static_cast<std::int8_t>(br.u(1));
                    if (!m.prev_flag[i]) m.rem_mode[i] = static_cast<std::int8_t>(br.u(3));
                }
            }
        }
        if (has_chroma) {
            if (cabac_mode) {
                int inc = 0;
                int ia = nb_mb_a();
                int ib = nb_mb_b();
                if (ia >= 0) {
                    const MbInfo& a = mbs[static_cast<std::size_t>(ia)];
                    if ((a.flags & k_mb_intra) && !(a.flags & k_mb_pcm) && a.chroma_pred_mode != 0) ++inc;
                }
                if (ib >= 0) {
                    const MbInfo& b = mbs[static_cast<std::size_t>(ib)];
                    if ((b.flags & k_mb_intra) && !(b.flags & k_mb_pcm) && b.chroma_pred_mode != 0) ++inc;
                }
                int mode = 0;
                if (cabac.decision(cabac_state[64 + inc])) {
                    mode = 1;
                    if (cabac.decision(cabac_state[67])) {
                        mode = 2;
                        if (cabac.decision(cabac_state[67])) mode = 3;
                    }
                }
                m.chroma_mode = mode;
            } else {
                std::uint32_t v = br.ue();
                if (v > 3) return false;
                m.chroma_mode = static_cast<int>(v);
            }
        }
    } else if (m.kind == k_kind_8x8) {
        int direct_mask = 0;
        bool small_parts = false;
        for (int i = 0; i < 4; ++i) {
            int t;
            if (cabac_mode) {
                if (st == k_slice_p) {
                    if (cabac.decision(cabac_state[21])) {
                        t = 0;
                    } else if (!cabac.decision(cabac_state[22])) {
                        t = 1;
                    } else {
                        t = cabac.decision(cabac_state[23]) ? 2 : 3;
                    }
                } else {
                    if (!cabac.decision(cabac_state[36])) {
                        t = 0;
                    } else if (!cabac.decision(cabac_state[37])) {
                        t = 1 + cabac.decision(cabac_state[39]);
                    } else {
                        t = 3;
                        if (cabac.decision(cabac_state[38])) {
                            if (cabac.decision(cabac_state[39])) {
                                t = 11 + cabac.decision(cabac_state[39]);
                                goto sub_done;
                            }
                            t += 4;
                        }
                        t += 2 * cabac.decision(cabac_state[39]);
                        t += cabac.decision(cabac_state[39]);
                    }
                }
            } else {
                std::uint32_t v = br.ue();
                if (v > (st == k_slice_p ? 3u : 12u)) return false;
                t = static_cast<int>(v);
            }
        sub_done:
            m.sub_type[i] = t;
            if (st == k_slice_p) {
                m.sub_pred[i] = 1;
                m.sub_shape[i] = t;
            } else {
                m.sub_pred[i] = k_b_sub_info[t][0];
                m.sub_shape[i] = k_b_sub_info[t][1];
            }
            if (m.sub_pred[i] == 0) {
                direct_mask |= 1 << i;
                if (!sps->direct_8x8_inference) small_parts = true;
            } else if (m.sub_shape[i] != 0) {
                small_parts = true;
            }
        }
        info.direct_mask = static_cast<std::uint8_t>(direct_mask);
        m.part_pred[0] = small_parts ? 1 : 0;
        for (int l = 0; l < 2; ++l) {
            for (int i = 0; i < 4; ++i) {
                if (!(m.sub_pred[i] & (1 << l))) continue;
                int ref = 0;
                bool present = (sh.num_ref_idx_active[l] > 1 || ref_present_field) && !m.ref0;
                if (present) {
                    if (cabac_mode) {
                        ref = cabac_ref_idx(l, (i & 1) * 2, (i >> 1) * 2);
                    } else if (nref[l] == 2) {
                        ref = br.u(1) ? 0 : 1;
                    } else {
                        ref = static_cast<int>(br.ue());
                    }
                    if (ref < 0 || ref >= nref[l]) return false;
                }
                m.ref_idx[l][i] = ref;
                write_ref(cur, mbase, (i & 1) * 2, (i >> 1) * 2, 2, 2, l, ref);
            }
        }
        static const std::uint8_t k_sub_pos[4][4][2] = {
            {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
            {{0, 0}, {0, 1}, {0, 0}, {0, 0}},
            {{0, 0}, {1, 0}, {0, 0}, {0, 0}},
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        };
        for (int l = 0; l < 2; ++l) {
            for (int i = 0; i < 4; ++i) {
                if (!(m.sub_pred[i] & (1 << l))) continue;
                int shape = m.sub_shape[i];
                int parts = shape == 0 ? 1 : (shape == 3 ? 4 : 2);
                int sw = (shape == 0 || shape == 1) ? 2 : 1;
                int shh = (shape == 0 || shape == 2) ? 2 : 1;
                for (int s = 0; s < parts; ++s) {
                    int x4 = (i & 1) * 2 + k_sub_pos[shape][s][0];
                    int y4 = (i >> 1) * 2 + k_sub_pos[shape][s][1];
                    if (!read_mvd(l, x4, y4, sw, shh)) return false;
                }
            }
        }
    } else if (m.kind != k_kind_b_direct) {
        int parts = m.kind == k_kind_16x16 ? 1 : 2;
        for (int l = 0; l < 2; ++l) {
            for (int p = 0; p < parts; ++p) {
                if (!(m.part_pred[p] & (1 << l))) continue;
                int x4 = m.kind == k_kind_8x16 ? p * 2 : 0;
                int y4 = m.kind == k_kind_16x8 ? p * 2 : 0;
                int w4 = m.kind == k_kind_8x16 ? 2 : 4;
                int h4 = m.kind == k_kind_16x8 ? 2 : 4;
                int ref = 0;
                if (sh.num_ref_idx_active[l] > 1 || ref_present_field) {
                    if (cabac_mode) {
                        ref = cabac_ref_idx(l, x4, y4);
                    } else if (nref[l] == 2) {
                        ref = br.u(1) ? 0 : 1;
                    } else {
                        ref = static_cast<int>(br.ue());
                    }
                    if (ref < 0 || ref >= nref[l]) return false;
                }
                m.ref_idx[l][p] = ref;
                write_ref(cur, mbase, x4, y4, w4, h4, l, ref);
            }
        }
        for (int l = 0; l < 2; ++l) {
            for (int p = 0; p < parts; ++p) {
                if (!(m.part_pred[p] & (1 << l))) continue;
                int x4 = m.kind == k_kind_8x16 ? p * 2 : 0;
                int y4 = m.kind == k_kind_16x8 ? p * 2 : 0;
                int w4 = m.kind == k_kind_8x16 ? 2 : 4;
                int h4 = m.kind == k_kind_16x8 ? 2 : 4;
                if (!read_mvd(l, x4, y4, w4, h4)) return false;
            }
        }
    } else {
        info.direct_mask = 15;
    }

    if (m.kind == k_kind_i16x16) {
        m.cbp = m.i16_cbp_luma | ((has_chroma ? m.i16_cbp_chroma : 0) << 4);
    } else if (cabac_mode) {
        int cbp = 0;
        for (int b8 = 0; b8 < 4; ++b8) {
            int x8 = b8 & 1;
            int y8 = b8 >> 1;
            int cnd[2];
            for (int n = 0; n < 2; ++n) {
                int xw = 0;
                int yw = 0;
                int mbn = n == 0 ? nb_loc(x8 * 8 - 1, y8 * 8, 16, 16, &xw, &yw) : nb_loc(x8 * 8, y8 * 8 - 1, 16, 16, &xw, &yw);
                int b8n = ((yw >> 3) << 1) | (xw >> 3);
                if (mbn < 0) {
                    cnd[n] = 0;
                } else if (mbn == mb_addr) {
                    cnd[n] = ((cbp >> b8n) & 1) == 0;
                } else {
                    const MbInfo& nb = mbs[static_cast<std::size_t>(mbn)];
                    if (nb.flags & k_mb_pcm) {
                        cnd[n] = 0;
                    } else if (nb.flags & k_mb_skip) {
                        cnd[n] = 1;
                    } else {
                        cnd[n] = ((nb.cbp >> b8n) & 1) == 0;
                    }
                }
            }
            if (cabac.decision(cabac_state[73 + cnd[0] + 2 * cnd[1]])) cbp |= 1 << b8;
        }
        if (has_chroma) {
            int ia = nb_mb_a();
            int ib = nb_mb_b();
            const MbInfo* a = ia >= 0 ? &mbs[static_cast<std::size_t>(ia)] : nullptr;
            const MbInfo* b = ib >= 0 ? &mbs[static_cast<std::size_t>(ib)] : nullptr;
            int ca = a && ((a->flags & k_mb_pcm) || (!(a->flags & k_mb_skip) && (a->cbp >> 4) != 0)) ? 1 : 0;
            int cb = b && ((b->flags & k_mb_pcm) || (!(b->flags & k_mb_skip) && (b->cbp >> 4) != 0)) ? 1 : 0;
            if (cabac.decision(cabac_state[77 + ca + 2 * cb])) {
                int ca2 = a && ((a->flags & k_mb_pcm) || (!(a->flags & k_mb_skip) && (a->cbp >> 4) == 2)) ? 1 : 0;
                int cb2 = b && ((b->flags & k_mb_pcm) || (!(b->flags & k_mb_skip) && (b->cbp >> 4) == 2)) ? 1 : 0;
                cbp |= (1 + cabac.decision(cabac_state[81 + ca2 + 2 * cb2])) << 4;
            }
        }
        m.cbp = cbp;
    } else {
        std::uint32_t v = br.ue();
        bool i4 = m.kind == k_kind_i4x4 || m.kind == k_kind_i8x8;
        if (has_chroma) {
            if (v > 47) return false;
            m.cbp = i4 ? k_golomb_to_intra4x4_cbp[v] : k_golomb_to_inter_cbp[v];
        } else {
            if (v > 15) return false;
            m.cbp = i4 ? k_golomb_to_intra4x4_cbp_gray[v] : k_golomb_to_inter_cbp_gray[v];
        }
    }
    info.cbp = static_cast<std::uint8_t>(m.cbp);

    if (!intra && (m.cbp & 15) && pps->transform_8x8_mode) {
        bool allowed;
        if (m.kind == k_kind_8x8) {
            allowed = m.part_pred[0] == 0;
        } else if (m.kind == k_kind_b_direct) {
            allowed = sps->direct_8x8_inference;
        } else {
            allowed = true;
        }
        if (allowed) {
            if (cabac_mode) {
                int inc = 0;
                int a = nb_mb_a();
                int b = nb_mb_b();
                if (a >= 0 && mbs[static_cast<std::size_t>(a)].transform8x8) ++inc;
                if (b >= 0 && mbs[static_cast<std::size_t>(b)].transform8x8) ++inc;
                m.transform8x8 = cabac.decision(cabac_state[399 + inc]) != 0;
            } else {
                m.transform8x8 = br.flag();
            }
        }
    }
    info.transform8x8 = m.transform8x8 ? 1 : 0;

    if (m.cbp != 0 || m.kind == k_kind_i16x16) {
        int delta;
        if (cabac_mode) {
            int k = 0;
            if (cabac.decision(cabac_state[60 + (last_qp_delta != 0 ? 1 : 0)])) {
                k = 1;
                if (cabac.decision(cabac_state[62])) {
                    k = 2;
                    while (cabac.decision(cabac_state[63])) {
                        if (++k > 200) return false;
                    }
                }
            }
            delta = (k & 1) ? (k + 1) / 2 : -(k / 2);
        } else {
            delta = br.se();
        }
        int lim = qp_bd_y / 2;
        if (delta < -(26 + lim) || delta > 25 + lim) return false;
        last_qp_delta = delta;
        qp = ((qp + delta + 52 + 2 * qp_bd_y) % (52 + qp_bd_y)) - qp_bd_y;
        for (int c = 0; c < 2; ++c) chroma_qp[c] = chroma_qp_for(qp, c, pps->chroma_qp_index_offset[c]);
    } else {
        last_qp_delta = 0;
    }
    info.qp = static_cast<std::int8_t>(qp);
    mb_bypass = sps->transform_bypass && qp + qp_bd_y == 0;
    if (mb_bypass) info.flags = static_cast<std::uint16_t>(info.flags | k_mb_bypass);
    flags = info.flags;

    if (!intra) compute_motion();
    if (intra) resolve_intra_modes();
    if ((m.cbp != 0 || m.kind == k_kind_i16x16) && !parse_residual()) return false;
    store_mb_info(flags);
    reconstruct_mb();
    return true;
}

bool DecoderState::read_mvd(int list, int x4, int y4, int w4, int h4) {
    int vx;
    int vy;
    if (pps->cabac) {
        vx = cabac_mvd(list, 0, x4, y4);
        vy = cabac_mvd(list, 1, x4, y4);
    } else {
        vx = br.se();
        vy = br.se();
        if (vx < -32768 || vx > 32767 || vy < -32768 || vy > 32767) return false;
    }
    int blk = k_blk_index[y4][x4];
    mp.mvd[list][blk].x = static_cast<std::int16_t>(clip3(-32768, 32767, vx));
    mp.mvd[list][blk].y = static_cast<std::int16_t>(clip3(-32768, 32767, vy));
    std::uint8_t ax = static_cast<std::uint8_t>(std::min(std::abs(vx), 127));
    std::uint8_t ay = static_cast<std::uint8_t>(std::min(std::abs(vy), 127));
    for (int y = 0; y < h4; ++y) {
        for (int x = 0; x < w4; ++x) {
            std::size_t idx = (static_cast<std::size_t>(mb_addr) * 16 + static_cast<std::size_t>((y4 + y) * 4 + x4 + x)) * 2;
            mvd[list][idx] = ax;
            mvd[list][idx + 1] = ay;
        }
    }
    return true;
}

void DecoderState::init_cabac_contexts() {
    int q = clip3(0, 51, sh.slice_qp);
    const std::int8_t (*table)[2] = sh.slice_type == k_slice_i ? k_cabac_init_i : k_cabac_init_pb[sh.cabac_init_idc];
    for (int i = 0; i < 1024; ++i) {
        int pre = clip3(1, 126, ((table[i][0] * q) >> 4) + table[i][1]);
        if (pre <= 63) {
            cabac_state[i] = static_cast<std::uint8_t>((63 - pre) << 1);
        } else {
            cabac_state[i] = static_cast<std::uint8_t>(((pre - 64) << 1) | 1);
        }
    }
}

bool DecoderState::decode_slice_data(std::string* error) {
    qp = sh.slice_qp;
    last_qp_delta = 0;
    cur_pixel_bytes = cur->pixel_bytes;
    for (int c = 0; c < 2; ++c) chroma_qp[c] = chroma_qp_for(qp, c, pps->chroma_qp_index_offset[c]);
    mb_addr = sh.first_mb * (mbaff ? 2 : 1);
    std::memset(&res, 0, sizeof(res));
    bool cabac_mode = pps->cabac;
    if (cabac_mode) {
        br.align();
        cabac_data_offset = br.position() >> 3;
        if (cabac_data_offset > br.size_bytes()) {
            if (error) *error = "slice data missing";
            return false;
        }
        init_cabac_contexts();
        cabac.init(br.data() + cabac_data_offset, br.size_bytes() - cabac_data_offset);
    }
    bool ok = true;
    bool more = true;
    bool top_skipped = false;
    pair_flag_known = false;
    pending_skip_bottom = false;
    mb_field = field_pic;
    auto begin_at = [&](int addr) {
        mb_addr = addr;
        if (mbaff && (addr & 1) == 0) {
            pair_flag_known = false;
            infer_pair_field();
        }
        mb_field = mbaff ? pair_field_flag : field_pic;
        mb_bottom = mbaff && (addr & 1);
        setup_mb_position();
        begin_mb_info();
        setup_mb_neighbors();
    };
    auto set_field = [&](bool f) {
        pair_field_flag = f;
        pair_flag_known = true;
        mb_field = f;
        mbs[static_cast<std::size_t>(mb_addr)].field = f ? 1 : 0;
        if (mb_bottom) mbs[static_cast<std::size_t>(mb_addr - 1)].field = f ? 1 : 0;
        setup_mb_position();
    };
    while (more) {
        if (mb_addr >= pic_mb_count) {
            ok = false;
            break;
        }
        mb_error = false;
        if (cabac_mode) {
            begin_at(mb_addr);
            bool skip = false;
            if (sh.slice_type != k_slice_i) {
                if (mbaff && mb_bottom && pending_skip_bottom) {
                    skip = pending_skip_value != 0;
                } else {
                    skip = cabac_mb_skip() != 0;
                }
            }
            pending_skip_bottom = false;
            if (skip) {
                if (mbaff && !mb_bottom) {
                    int top = mb_addr;
                    mbs[static_cast<std::size_t>(top)].flags = static_cast<std::uint16_t>(k_mb_inter | k_mb_skip);
                    if (top + 1 >= pic_mb_count) {
                        set_field(pair_field_flag);
                        decode_skip_mb(false);
                        mb_done[static_cast<std::size_t>(top)] = 1;
                        mb_addr = top + 1;
                        more = false;
                        continue;
                    }
                    mb_addr = top + 1;
                    mb_bottom = true;
                    setup_mb_position();
                    MbInfo& bi = mbs[static_cast<std::size_t>(mb_addr)];
                    bi = MbInfo();
                    bi.slice = slice_num;
                    bi.field = pair_field_flag ? 1 : 0;
                    setup_mb_neighbors();
                    int bskip = cabac_mb_skip();
                    bool f = pair_field_flag;
                    if (!bskip) f = cabac_field_flag() != 0;
                    bi.slice = -1;
                    mb_addr = top;
                    mb_bottom = false;
                    pending_skip_bottom = true;
                    pending_skip_value = bskip;
                    set_field(f);
                    setup_mb_neighbors();
                    decode_skip_mb(false);
                    mb_done[static_cast<std::size_t>(top)] = 1;
                    mb_addr = top + 1;
                    top_skipped = true;
                    continue;
                }
                decode_skip_mb(false);
            } else {
                if (mbaff && (!mb_bottom || (top_skipped && !pair_flag_known))) {
                    set_field(cabac_field_flag() != 0);
                } else if (mbaff) {
                    set_field(pair_field_flag);
                }
                if (!parse_mb(false)) {
                    ok = false;
                    break;
                }
            }
            top_skipped = false;
            mb_done[static_cast<std::size_t>(mb_addr)] = 1;
            ++mb_addr;
            if (cabac.overrun()) {
                ok = false;
                break;
            }
            if (mbaff && (mb_addr & 1)) {
                more = true;
            } else {
                more = cabac.terminate() == 0;
            }
        } else {
            bool prev_skipped = false;
            if (sh.slice_type != k_slice_i) {
                std::uint32_t run = br.ue();
                if (br.error() || run > static_cast<std::uint32_t>(pic_mb_count - mb_addr)) {
                    ok = false;
                    break;
                }
                prev_skipped = run > 0;
                for (std::uint32_t i = 0; i < run; ++i) {
                    begin_at(mb_addr);
                    if (mbaff && !mb_bottom) {
                        bool last = i + 1 == run;
                        if (last && br.more_rbsp_data()) {
                            set_field(br.flag());
                        } else {
                            set_field(pair_field_flag);
                        }
                    } else if (mbaff) {
                        set_field(pair_field_flag);
                    }
                    decode_skip_mb(false);
                    mb_done[static_cast<std::size_t>(mb_addr)] = 1;
                    ++mb_addr;
                }
                if (run > 0 && !br.more_rbsp_data()) break;
                if (mb_addr >= pic_mb_count) {
                    ok = false;
                    break;
                }
            }
            begin_at(mb_addr);
            if (mbaff) {
                if (!mb_bottom) {
                    set_field(br.flag());
                } else if (prev_skipped && !pair_flag_known) {
                    set_field(br.flag());
                } else {
                    set_field(pair_field_flag);
                }
            }
            if (!parse_mb(false) || br.error()) {
                ok = false;
                break;
            }
            mb_done[static_cast<std::size_t>(mb_addr)] = 1;
            ++mb_addr;
            more = br.more_rbsp_data();
        }
    }
    if (!ok) {
        std::memset(&res, 0, sizeof(res));
        if (mb_addr < pic_mb_count && mb_addr >= 0) {
            mbs[static_cast<std::size_t>(mb_addr)].slice = -1;
            mb_done[static_cast<std::size_t>(mb_addr)] = 0;
        }
        if (error) *error = "corrupt slice data";
        return false;
    }
    return true;
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
