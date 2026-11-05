#include "h264_internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace fizmo {
namespace video {
namespace h264_internal {

namespace {

inline std::int16_t wrap16(int v) noexcept {
    int u = (v + 65536) & 0xFFFF;
    return static_cast<std::int16_t>(u >= 32768 ? u - 65536 : u);
}

inline int min_positive(int a, int b) noexcept {
    return (a >= 0 && b >= 0) ? std::min(a, b) : std::max(a, b);
}

inline int median3(int a, int b, int c) noexcept {
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

const std::uint8_t k_sub_pos[4][4][2] = {
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 1}, {0, 0}, {0, 0}},
    {{0, 0}, {1, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
};

enum VertScale {
    k_one_to_one = 0,
    k_frm_to_fld = 1,
    k_fld_to_frm = 2
};

struct ColBlock {
    const Picture* pic = nullptr;
    std::size_t mb = 0;
    int blk = 0;
    int b8 = 0;
    int scale = k_one_to_one;
    bool intra = true;
    bool field_ref = false;
};

} // namespace

bool DecoderState::mb_available_intra(int mbx, int mby) const noexcept {
    if (!mb_available(mbx, mby)) return false;
    if (!pps->constrained_intra_pred) return true;
    return (mbs[static_cast<std::size_t>(mby * mb_width + mbx)].flags & k_mb_intra) != 0;
}

NbMotion DecoderState::nb_motion(int list, int xn, int yn, int cur_order) const noexcept {
    NbMotion n;
    n.available = false;
    n.ref = -1;
    n.mv.x = 0;
    n.mv.y = 0;
    n.mb = -1;
    n.blk = 0;
    int xw = 0;
    int yw = 0;
    int mbn = nb_loc(xn, yn, 16, 16, &xw, &yw);
    if (mbn < 0) return n;
    if (mbn == mb_addr && k_blk_index[yw >> 2][xw >> 2] >= cur_order) return n;
    n.available = true;
    n.mb = mbn;
    n.blk = (yw >> 2) * 4 + (xw >> 2);
    const MbInfo& info = mbs[static_cast<std::size_t>(mbn)];
    if (mbn != mb_addr && (info.flags & k_mb_intra)) return n;
    std::size_t idx = (motion_offset + static_cast<std::size_t>(mbn)) * 16 + static_cast<std::size_t>(n.blk);
    int ref = cur->ref_idx[list][idx];
    if (ref < 0) return n;
    Mv mv = cur->mv[list][idx];
    if (mbaff) {
        bool nf = info.field != 0;
        if (mb_field && !nf) {
            mv.y = static_cast<std::int16_t>(mv.y / 2);
            ref *= 2;
        } else if (!mb_field && nf) {
            mv.y = wrap16(mv.y * 2);
            ref >>= 1;
        }
    }
    n.ref = ref;
    n.mv = mv;
    return n;
}

Neighbor DecoderState::neighbor(int list, int x4, int y4, int cur_blk) const noexcept {
    NbMotion m = nb_motion(list, x4 * 4, y4 * 4, cur_blk);
    Neighbor n;
    n.available = m.available;
    n.ref = m.ref;
    n.mv = m.mv;
    return n;
}

void DecoderState::mv_pred_part(int list, int ref, int x4, int y4, int w4, int h4, int shape, int order, Mv& out) const noexcept {
    (void)h4;
    NbMotion a = nb_motion(list, x4 * 4 - 1, y4 * 4, order);
    NbMotion b = nb_motion(list, x4 * 4, y4 * 4 - 1, order);
    NbMotion c = nb_motion(list, (x4 + w4) * 4, y4 * 4 - 1, order);
    if (!c.available) c = nb_motion(list, x4 * 4 - 1, y4 * 4 - 1, order);
    if (shape == 1) {
        if (y4 == 0 && b.ref == ref) {
            out = b.mv;
            return;
        }
        if (y4 != 0 && a.ref == ref) {
            out = a.mv;
            return;
        }
    } else if (shape == 2) {
        if (x4 == 0 && a.ref == ref) {
            out = a.mv;
            return;
        }
        if (x4 != 0 && c.ref == ref) {
            out = c.mv;
            return;
        }
    }
    if (!b.available && !c.available && a.available) {
        b = a;
        c = a;
    }
    int matches = (a.ref == ref) + (b.ref == ref) + (c.ref == ref);
    if (matches == 1) {
        if (a.ref == ref) {
            out = a.mv;
        } else if (b.ref == ref) {
            out = b.mv;
        } else {
            out = c.mv;
        }
        return;
    }
    out.x = static_cast<std::int16_t>(median3(a.mv.x, b.mv.x, c.mv.x));
    out.y = static_cast<std::int16_t>(median3(a.mv.y, b.mv.y, c.mv.y));
}

void DecoderState::mv_pred(int list, int ref, int x4, int y4, int w4, int h4, int shape, Mv& out) const noexcept {
    mv_pred_part(list, ref, x4, y4, w4, h4, shape, k_blk_index[y4][x4], out);
}

void DecoderState::set_motion(int x4, int y4, int w4, int h4, int list, int ref, Mv mv) {
    Picture* p = cur;
    std::int8_t r = static_cast<std::int8_t>(ref);
    if (ref < 0) {
        mv.x = 0;
        mv.y = 0;
    }
    std::size_t base = (motion_offset + static_cast<std::size_t>(mb_addr)) * 16;
    for (int y = y4; y < y4 + h4; ++y) {
        for (int x = x4; x < x4 + w4; ++x) {
            std::size_t idx = base + static_cast<std::size_t>(y * 4 + x);
            p->ref_idx[list][idx] = r;
            p->mv[list][idx] = mv;
        }
    }
    std::int32_t uid = -1;
    if (ref >= 0) uid = ref_entry_id(ref_entry(list, ref));
    std::size_t b8base = (motion_offset + static_cast<std::size_t>(mb_addr)) * 4;
    for (int y8 = y4 >> 1; y8 <= (y4 + h4 - 1) >> 1; ++y8) {
        for (int x8 = x4 >> 1; x8 <= (x4 + w4 - 1) >> 1; ++x8) p->ref_uid[list][b8base + static_cast<std::size_t>(y8 * 2 + x8)] = uid;
    }
}

void DecoderState::pskip_motion() {
    Mv zero;
    zero.x = 0;
    zero.y = 0;
    NbMotion a = nb_motion(0, -1, 0, 0);
    NbMotion b = nb_motion(0, 0, -1, 0);
    Mv mv = zero;
    bool zero_mv = !a.available || !b.available || (a.ref == 0 && a.mv.x == 0 && a.mv.y == 0) || (b.ref == 0 && b.mv.x == 0 && b.mv.y == 0);
    if (!zero_mv) mv_pred_part(0, 0, 0, 0, 4, 4, 0, 0, mv);
    set_motion(0, 0, 4, 4, 0, 0, mv);
    set_motion(0, 0, 4, 4, 1, -1, zero);
}

void DecoderState::direct_motion(int sub_mask) {
    if (sh.direct_spatial) {
        direct_spatial(sub_mask);
    } else {
        direct_temporal(sub_mask);
    }
}

namespace {

ColBlock locate_col(const DecoderState& d, int xcol, int ycol) {
    ColBlock cb;
    const Picture* rp = d.ref_list[1][0];
    if (!rp) return cb;
    cb.pic = rp;
    int w = d.mb_width;
    int addr = d.mb_addr;
    int plane = d.colour_plane;
    std::size_t base = 0;
    int mb_col = addr;
    int ym = ycol;
    int scale = k_one_to_one;
    std::int32_t cur_poc = std::min(d.cur->top_poc, d.cur->bottom_poc);
    auto closest = [&]() -> int {
        int dt = std::abs(rp->top_poc - cur_poc);
        int db = std::abs(rp->bottom_poc - cur_poc);
        return dt < db ? 0 : 1;
    };
    if (d.field_pic) {
        if (rp->coding == k_coding_fields) {
            int st = d.ref_struct[1][0];
            base = rp->motion_base(plane, st);
            cb.field_ref = true;
        } else if (rp->coding == k_coding_frame) {
            base = rp->motion_base(plane, k_struct_frame);
            mb_col = 2 * w * (addr / w) + (addr % w) + w * (ycol / 8);
            ym = (2 * ycol) % 16;
            scale = k_frm_to_fld;
        } else {
            base = rp->motion_base(plane, k_struct_frame);
            bool fx = rp->mb_field[base + static_cast<std::size_t>(2 * addr)] != 0;
            if (!fx) {
                mb_col = 2 * addr + (ycol / 8);
                ym = (2 * ycol) % 16;
                scale = k_frm_to_fld;
            } else {
                mb_col = 2 * addr + (d.bottom_field ? 1 : 0);
                cb.field_ref = true;
            }
        }
    } else if (!d.mbaff) {
        if (rp->coding == k_coding_fields) {
            int par = closest();
            base = rp->motion_base(plane, par ? k_struct_bottom : k_struct_top);
            mb_col = w * (addr / (2 * w)) + (addr % w);
            ym = 8 * ((addr / w) % 2) + 4 * (ycol / 8);
            scale = k_fld_to_frm;
            cb.field_ref = true;
        } else {
            base = rp->motion_base(plane, k_struct_frame);
        }
    } else {
        if (rp->coding == k_coding_fields) {
            if (!d.mb_field) {
                int par = closest();
                base = rp->motion_base(plane, par ? k_struct_bottom : k_struct_top);
                mb_col = addr / 2;
                ym = 8 * (addr % 2) + 4 * (ycol / 8);
                scale = k_fld_to_frm;
            } else {
                base = rp->motion_base(plane, d.mb_bottom ? k_struct_bottom : k_struct_top);
                mb_col = addr / 2;
            }
            cb.field_ref = true;
        } else {
            base = rp->motion_base(plane, k_struct_frame);
            bool fx = rp->mb_field[base + static_cast<std::size_t>(addr)] != 0;
            if (!d.mb_field) {
                if (fx) {
                    mb_col = 2 * (addr / 2) + closest();
                    ym = 8 * (addr % 2) + 4 * (ycol / 8);
                    scale = k_fld_to_frm;
                    cb.field_ref = true;
                }
            } else {
                if (!fx) {
                    mb_col = 2 * (addr / 2) + (ycol / 8);
                    ym = (2 * ycol) % 16;
                    scale = k_frm_to_fld;
                } else {
                    cb.field_ref = true;
                }
            }
        }
    }
    std::size_t total = rp->mb_intra.size();
    std::size_t mbi = base + static_cast<std::size_t>(mb_col);
    if (mbi >= total) mbi = total ? total - 1 : 0;
    cb.mb = mbi;
    cb.blk = (ym >> 2) * 4 + (xcol >> 2);
    cb.b8 = (ym >> 3) * 2 + (xcol >> 3);
    cb.scale = scale;
    cb.intra = rp->mb_intra[mbi] != 0;
    return cb;
}

} // namespace

void DecoderState::direct_spatial(int sub_mask) {
    int ref[2];
    Mv mvp[2];
    for (int l = 0; l < 2; ++l) {
        NbMotion a = nb_motion(l, -1, 0, 0);
        NbMotion b = nb_motion(l, 0, -1, 0);
        NbMotion c = nb_motion(l, 16, -1, 0);
        if (!c.available) c = nb_motion(l, -1, -1, 0);
        ref[l] = min_positive(a.ref, min_positive(b.ref, c.ref));
        mvp[l].x = 0;
        mvp[l].y = 0;
    }
    bool direct_zero = false;
    if (ref[0] < 0 && ref[1] < 0) {
        ref[0] = ref[1] = 0;
        direct_zero = true;
    }
    if (!direct_zero) {
        for (int l = 0; l < 2; ++l) {
            if (ref[l] >= 0) mv_pred_part(l, ref[l], 0, 0, 4, 4, 0, 0, mvp[l]);
        }
    }
    bool l1_short = false;
    {
        RefEntry e = ref_entry(1, 0);
        if (e.pic) {
            if (e.structure == k_struct_frame) {
                l1_short = e.pic->field_ref[0] == 1 && e.pic->field_ref[1] == 1;
            } else {
                l1_short = e.pic->field_ref[e.structure == k_struct_top ? 0 : 1] == 1;
            }
            if (e.pic == gray_pic.get()) l1_short = false;
        }
    }
    bool inference = sps->direct_8x8_inference;
    for (int b8 = 0; b8 < 4; ++b8) {
        if (!(sub_mask & (1 << b8))) continue;
        int x8 = (b8 & 1) * 2;
        int y8 = (b8 >> 1) * 2;
        int steps = inference ? 1 : 4;
        for (int sub = 0; sub < steps; ++sub) {
            int x4 = x8 + (sub & 1);
            int y4 = y8 + (sub >> 1);
            int cx = x4;
            int cy = y4;
            if (inference) {
                cx = (b8 & 1) * 3;
                cy = (b8 >> 1) * 3;
            }
            bool col_zero = false;
            if (l1_short && !direct_zero) {
                ColBlock cb = locate_col(*this, cx * 4, cy * 4);
                if (cb.pic && !cb.intra) {
                    std::size_t idx = cb.mb * 16 + static_cast<std::size_t>(cb.blk);
                    int cref = cb.pic->ref_idx[0][idx];
                    Mv cmv = cb.pic->mv[0][idx];
                    if (cref < 0) {
                        cref = cb.pic->ref_idx[1][idx];
                        cmv = cb.pic->mv[1][idx];
                    }
                    col_zero = cref == 0 && cmv.x >= -1 && cmv.x <= 1 && cmv.y >= -1 && cmv.y <= 1;
                }
            }
            int size = inference ? 2 : 1;
            for (int l = 0; l < 2; ++l) {
                Mv mv;
                mv.x = 0;
                mv.y = 0;
                if (ref[l] >= 0 && !direct_zero && !(ref[l] == 0 && col_zero)) mv = mvp[l];
                set_motion(x4, y4, size, size, l, ref[l], mv);
            }
        }
    }
}

std::int32_t DecoderState::col_ref_lookup(int list, std::int32_t id, int mode) const noexcept {
    (void)list;
    if (id < 0) return 0;
    std::int32_t uid = id >> 2;
    int st = id & 3;
    int n = ref_count_mb(0);
    for (int i = 0; i < n; ++i) {
        RefEntry e = ref_entry(0, i);
        if (!e.pic || e.pic->uid != uid) continue;
        if (mode == 0) {
            if (e.structure == st) return i;
        } else if (mode == 1) {
            int want = (field_pic ? bottom_field : mb_bottom) ? k_struct_bottom : k_struct_top;
            if (e.structure == want) return i;
        } else {
            if (e.structure == k_struct_frame) return i;
        }
    }
    return 0;
}

void DecoderState::direct_temporal(int sub_mask) {
    bool inference = sps->direct_8x8_inference;
    std::int32_t cp = cur_poc_for_mb();
    RefEntry e1 = ref_entry(1, 0);
    for (int b8 = 0; b8 < 4; ++b8) {
        if (!(sub_mask & (1 << b8))) continue;
        int x8 = (b8 & 1) * 2;
        int y8 = (b8 >> 1) * 2;
        int steps = inference ? 1 : 4;
        for (int sub = 0; sub < steps; ++sub) {
            int x4 = x8 + (sub & 1);
            int y4 = y8 + (sub >> 1);
            int cx = inference ? (b8 & 1) * 3 : x4;
            int cy = inference ? (b8 >> 1) * 3 : y4;
            Mv mv_col;
            mv_col.x = 0;
            mv_col.y = 0;
            int ref0 = 0;
            ColBlock cb = locate_col(*this, cx * 4, cy * 4);
            if (cb.pic && !cb.intra) {
                std::size_t idx = cb.mb * 16 + static_cast<std::size_t>(cb.blk);
                int list = cb.pic->ref_idx[0][idx] >= 0 ? 0 : 1;
                int cref = cb.pic->ref_idx[list][idx];
                if (cref >= 0) {
                    mv_col = cb.pic->mv[list][idx];
                    std::int32_t id = cb.pic->ref_uid[list][cb.mb * 4 + static_cast<std::size_t>(cb.b8)];
                    int mode;
                    bool cur_field = field_pic || (mbaff && mb_field);
                    if (cb.scale == k_frm_to_fld) {
                        mode = 1;
                    } else if (cb.scale == k_fld_to_frm) {
                        mode = 2;
                    } else {
                        mode = cur_field ? 0 : 2;
                    }
                    if (mode == 0 && (id & 3) == k_struct_frame) mode = 1;
                    ref0 = col_ref_lookup(0, id, mode);
                }
            }
            if (cb.scale == k_frm_to_fld) {
                mv_col.y = static_cast<std::int16_t>(mv_col.y / 2);
            } else if (cb.scale == k_fld_to_frm) {
                mv_col.y = wrap16(mv_col.y * 2);
            }
            Mv mv0;
            Mv mv1;
            RefEntry e0 = ref_entry(0, ref0);
            int poc0 = ref_entry_poc(e0);
            int poc1 = ref_entry_poc(e1);
            if (!e0.pic || !e1.pic || ref_entry_long(e0) || poc1 - poc0 == 0) {
                mv0 = mv_col;
                mv1.x = 0;
                mv1.y = 0;
            } else {
                int tb = clip3(-128, 127, cp - poc0);
                int td = clip3(-128, 127, poc1 - poc0);
                int tx = (16384 + std::abs(td / 2)) / td;
                int dsf = clip3(-1024, 1023, (tb * tx + 32) >> 6);
                mv0.x = wrap16((dsf * mv_col.x + 128) >> 8);
                mv0.y = wrap16((dsf * mv_col.y + 128) >> 8);
                mv1.x = wrap16(mv0.x - mv_col.x);
                mv1.y = wrap16(mv0.y - mv_col.y);
            }
            int size = inference ? 2 : 1;
            set_motion(x4, y4, size, size, 0, ref0, mv0);
            set_motion(x4, y4, size, size, 1, 0, mv1);
        }
    }
}

void DecoderState::compute_motion() {
    const MbParse& m = mp;
    Mv mv;
    mv.x = 0;
    mv.y = 0;
    if (m.kind == k_kind_16x16) {
        for (int l = 0; l < 2; ++l) {
            if (m.part_pred[0] & (1 << l)) {
                int ref = m.ref_idx[l][0];
                mv_pred_part(l, ref, 0, 0, 4, 4, 0, 0, mv);
                mv.x = wrap16(mv.x + m.mvd[l][0].x);
                mv.y = wrap16(mv.y + m.mvd[l][0].y);
                set_motion(0, 0, 4, 4, l, ref, mv);
            } else {
                set_motion(0, 0, 4, 4, l, -1, mv);
            }
        }
    } else if (m.kind == k_kind_16x8 || m.kind == k_kind_8x16) {
        bool horizontal = m.kind == k_kind_16x8;
        for (int part = 0; part < 2; ++part) {
            int x4 = horizontal ? 0 : part * 2;
            int y4 = horizontal ? part * 2 : 0;
            int w4 = horizontal ? 4 : 2;
            int h4 = horizontal ? 2 : 4;
            int blk = k_blk_index[y4][x4];
            for (int l = 0; l < 2; ++l) {
                if (m.part_pred[part] & (1 << l)) {
                    int ref = m.ref_idx[l][part];
                    mv_pred_part(l, ref, x4, y4, w4, h4, horizontal ? 1 : 2, blk, mv);
                    mv.x = wrap16(mv.x + m.mvd[l][blk].x);
                    mv.y = wrap16(mv.y + m.mvd[l][blk].y);
                    set_motion(x4, y4, w4, h4, l, ref, mv);
                } else {
                    mv.x = mv.y = 0;
                    set_motion(x4, y4, w4, h4, l, -1, mv);
                }
            }
        }
    } else if (m.kind == k_kind_8x8) {
        int direct_mask = 0;
        for (int i = 0; i < 4; ++i) {
            if (m.sub_pred[i] == 0) direct_mask |= 1 << i;
        }
        if (direct_mask) direct_motion(direct_mask);
        for (int i = 0; i < 4; ++i) {
            if (m.sub_pred[i] == 0) continue;
            int x8 = (i & 1) * 2;
            int y8 = (i >> 1) * 2;
            int shape = m.sub_shape[i];
            int sw = (shape == 0 || shape == 1) ? 2 : 1;
            int shh = (shape == 0 || shape == 2) ? 2 : 1;
            int parts = (shape == 0) ? 1 : (shape == 3 ? 4 : 2);
            for (int l = 0; l < 2; ++l) {
                if (!(m.sub_pred[i] & (1 << l))) {
                    mv.x = mv.y = 0;
                    set_motion(x8, y8, 2, 2, l, -1, mv);
                    continue;
                }
                int ref = m.ref_idx[l][i];
                for (int s = 0; s < parts; ++s) {
                    int x4 = x8 + k_sub_pos[shape][s][0];
                    int y4 = y8 + k_sub_pos[shape][s][1];
                    int blk = k_blk_index[y4][x4];
                    mv_pred_part(l, ref, x4, y4, sw, shh, 0, blk, mv);
                    mv.x = wrap16(mv.x + m.mvd[l][blk].x);
                    mv.y = wrap16(mv.y + m.mvd[l][blk].y);
                    set_motion(x4, y4, sw, shh, l, ref, mv);
                }
            }
        }
    } else if (m.kind == k_kind_p_skip) {
        pskip_motion();
    } else if (m.kind == k_kind_b_skip || m.kind == k_kind_b_direct) {
        direct_motion(15);
    }
}

void DecoderState::begin_mb_info() {
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    info = MbInfo();
    info.slice = slice_num;
    info.field = mb_field ? 1 : 0;
    std::memset(&nnz[static_cast<std::size_t>(mb_addr) * 48], 0, 48);
    std::memset(&mvd[0][static_cast<std::size_t>(mb_addr) * 32], 0, 32);
    std::memset(&mvd[1][static_cast<std::size_t>(mb_addr) * 32], 0, 32);
    std::size_t base = (motion_offset + static_cast<std::size_t>(mb_addr)) * 16;
    for (int l = 0; l < 2; ++l) {
        for (int x = 0; x < 16; ++x) cur->ref_idx[l][base + static_cast<std::size_t>(x)] = -1;
    }
    MbParse& m = mp;
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < 4; ++i) m.ref_idx[l][i] = 0;
        for (int i = 0; i < 16; ++i) {
            m.mvd[l][i].x = 0;
            m.mvd[l][i].y = 0;
        }
    }
    m.transform8x8 = false;
    m.cbp = 0;
    m.qp_delta = 0;
    m.chroma_mode = 0;
    m.ref0 = false;
    m.part_pred[0] = m.part_pred[1] = 0;
    mb_bypass = false;
    cur->mb_intra[motion_offset + static_cast<std::size_t>(mb_addr)] = 0;
}

void DecoderState::resolve_intra_modes() {
    const MbParse& m = mp;
    auto pred_mode = [&](int xn, int yn, bool is8x8, int b8self, int n_is_a) -> int {
        int xw = 0;
        int yw = 0;
        int mbn = nb_loc(xn, yn, 16, 16, &xw, &yw);
        if (mbn < 0) return -1;
        const MbInfo& info = mbs[static_cast<std::size_t>(mbn)];
        if (mbn != mb_addr) {
            if (!(info.flags & k_mb_intra)) return pps->constrained_intra_pred ? -1 : 2;
            if (!(info.flags & (k_mb_i4x4 | k_mb_i8x8))) return 2;
        }
        int blk = (yw >> 2) * 4 + (xw >> 2);
        if (is8x8 && mbn != mb_addr && (info.flags & k_mb_i4x4)) {
            int b8n = ((yw >> 3) << 1) | (xw >> 3);
            int n = n_is_a ? 1 : 2;
            if (n_is_a && mbaff && !mb_field && info.field && b8self == 2) n = 3;
            int bi = b8n * 4 + n;
            blk = k_blk_y[bi] * 4 + k_blk_x[bi];
        }
        return intra_modes[static_cast<std::size_t>(mbn) * 16 + static_cast<std::size_t>(blk)];
    };
    std::int8_t* modes = &intra_modes[static_cast<std::size_t>(mb_addr) * 16];
    if (m.kind == k_kind_i4x4) {
        for (int bi = 0; bi < 16; ++bi) {
            int x4 = k_blk_x[bi];
            int y4 = k_blk_y[bi];
            int a = pred_mode(x4 * 4 - 1, y4 * 4, false, 0, 1);
            int b = pred_mode(x4 * 4, y4 * 4 - 1, false, 0, 0);
            int pred = (a < 0 || b < 0) ? 2 : std::min(a, b);
            int mode = m.prev_flag[bi] ? pred : (m.rem_mode[bi] < pred ? m.rem_mode[bi] : m.rem_mode[bi] + 1);
            modes[y4 * 4 + x4] = static_cast<std::int8_t>(mode);
        }
    } else if (m.kind == k_kind_i8x8) {
        for (int b8 = 0; b8 < 4; ++b8) {
            int x4 = (b8 & 1) * 2;
            int y4 = (b8 >> 1) * 2;
            int a = pred_mode(x4 * 4 - 1, y4 * 4, true, b8, 1);
            int b = pred_mode(x4 * 4, y4 * 4 - 1, true, b8, 0);
            int pred = (a < 0 || b < 0) ? 2 : std::min(a, b);
            int mode = m.prev_flag[b8] ? pred : (m.rem_mode[b8] < pred ? m.rem_mode[b8] : m.rem_mode[b8] + 1);
            for (int y = 0; y < 2; ++y) {
                modes[(y4 + y) * 4 + x4] = static_cast<std::int8_t>(mode);
                modes[(y4 + y) * 4 + x4 + 1] = static_cast<std::int8_t>(mode);
            }
        }
    }
}

void DecoderState::store_mb_info(int flags) {
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    const MbParse& m = mp;
    info.flags = static_cast<std::uint16_t>(flags | (mb_field ? k_mb_field : 0));
    info.cbp = static_cast<std::uint8_t>(m.cbp);
    info.qp = static_cast<std::int8_t>((flags & k_mb_pcm) ? 0 : qp);
    info.transform8x8 = m.transform8x8 ? 1 : 0;
    info.chroma_pred_mode = static_cast<std::uint8_t>(m.chroma_mode);
    info.field = mb_field ? 1 : 0;
    info.i16_mode = static_cast<std::uint8_t>(m.i16_mode);
    if (!(flags & (k_mb_i4x4 | k_mb_i8x8))) std::memset(&intra_modes[static_cast<std::size_t>(mb_addr) * 16], 2, 16);
    for (int p = 0; p < 3; ++p) {
        std::uint16_t nzmask = 0;
        for (int b = 0; b < 16; ++b) {
            if (nnz_at(mb_addr, p, b)) nzmask = static_cast<std::uint16_t>(nzmask | (1u << b));
        }
        if (m.transform8x8 && (p == 0 || chroma_array_type == 3)) {
            std::uint16_t full = 0;
            for (int b8 = 0; b8 < 4; ++b8) {
                std::uint16_t q = static_cast<std::uint16_t>(0x33u << ((b8 & 1) * 2 + (b8 >> 1) * 8));
                if (nzmask & q) full = static_cast<std::uint16_t>(full | q);
            }
            nzmask = full;
        }
        if (p == 0) {
            info.nz = nzmask;
        } else {
            info.nz_c[p - 1] = nzmask;
        }
    }
    std::size_t mi = motion_offset + static_cast<std::size_t>(mb_addr);
    if (flags & k_mb_intra) cur->mb_intra[mi] = 1;
    cur->mb_field[mi] = mb_field ? 1 : 0;
}

int DecoderState::chroma_qp_for(int q, int comp, int offset) const noexcept {
    (void)comp;
    int i = clip3(-qp_bd_c, 51, q + offset);
    return i < 0 ? i : k_chroma_qp_table[i];
}

namespace {

inline int read_sample(const std::uint8_t* p, int bytes) noexcept {
    if (bytes == 2) {
        std::uint16_t v;
        std::memcpy(&v, p, 2);
        return v;
    }
    return *p;
}

} // namespace

void DecoderState::gather_intra_edge(int plane, int bx, int by, int n, bool inner_tr, IntraEdge& e) const {
    gather_intra_edge2(plane, bx, by, n, n, inner_tr, e);
}

void DecoderState::gather_intra_edge2(int plane, int bx, int by, int n, int nh, bool inner_tr, IntraEdge& e) const {
    int maxw = 16;
    int maxh = 16;
    bool chroma_plane = plane > 0 && num_planes == 1 && chroma_array_type != 3;
    if (chroma_plane) {
        maxw = mb_wc;
        maxh = mb_hc;
    }
    int bytes = cur_pixel_bytes;
    bool cip = pps->constrained_intra_pred;
    auto fetch = [&](int xn, int yn, int* v) -> bool {
        int xw = 0;
        int yw = 0;
        int mbn = nb_loc(xn, yn, maxw, maxh, &xw, &yw);
        if (mbn < 0) return false;
        const MbInfo& info = mbs[static_cast<std::size_t>(mbn)];
        if (mbn != mb_addr && cip && !(info.flags & k_mb_intra)) return false;
        const std::uint8_t* ptr;
        if (mbn == mb_addr) {
            ptr = mb_pix[plane] + (static_cast<std::size_t>(yw) * static_cast<std::size_t>(mb_pstride[plane]) + static_cast<std::size_t>(xw)) * static_cast<std::size_t>(bytes);
        } else {
            int c = plane;
            int plane_index = num_planes > 1 ? colour_plane : c;
            int px;
            int py;
            int step = 1;
            int cw = chroma_plane ? mb_wc : 16;
            int ch = chroma_plane ? mb_hc : 16;
            if (mbaff) {
                int pair = mbn >> 1;
                int bot = mbn & 1;
                px = (pair % mb_width) * cw;
                int y0 = (pair / mb_width) * 2 * ch;
                if (info.field) {
                    py = y0 + bot;
                    step = 2;
                } else {
                    py = y0 + bot * ch;
                }
            } else {
                px = (mbn % mb_width) * cw;
                py = (mbn / mb_width) * ch;
            }
            ptr = cv.plane[plane_index] + (static_cast<std::size_t>(py + yw * step) * static_cast<std::size_t>(cv.stride[plane_index]) + static_cast<std::size_t>(px + xw)) * static_cast<std::size_t>(bytes);
        }
        *v = read_sample(ptr, bytes);
        return true;
    };
    int v = 0;
    e.have_left = true;
    int lh = nh;
    for (int y = 0; y < lh; ++y) {
        if (!fetch(bx - 1, by + y, &v)) {
            e.have_left = false;
            break;
        }
        e.left[y] = v;
    }
    int tw = n;
    e.have_top = true;
    for (int x = 0; x < tw; ++x) {
        if (!fetch(bx + x, by - 1, &v)) {
            e.have_top = false;
            break;
        }
        e.top[x] = v;
    }
    e.have_top_left = fetch(bx - 1, by - 1, &v);
    e.top_left = e.have_top_left ? v : 0;
    e.have_top_right = false;
    if ((n == 4 || n == 8) && n == nh) {
        bool ok = true;
        if (inner_tr) {
            for (int x = n; x < 2 * n; ++x) {
                if (!fetch(bx + x, by - 1, &v)) {
                    ok = false;
                    break;
                }
                e.top[x] = v;
            }
        } else {
            ok = false;
        }
        e.have_top_right = ok;
    }
}

namespace {

void dpcm(std::int32_t* r, int stride, int w, int h, bool vertical) {
    if (vertical) {
        for (int y = 1; y < h; ++y)
            for (int x = 0; x < w; ++x) r[y * stride + x] += r[(y - 1) * stride + x];
    } else {
        for (int y = 0; y < h; ++y)
            for (int x = 1; x < w; ++x) r[y * stride + x] += r[y * stride + x - 1];
    }
}

} // namespace

template <typename P>
void DecoderState::reconstruct_intra_t(int plane) {
    const MbParse& m = mp;
    int bd = (plane == 0 || num_planes > 1) ? bit_depth_y : bit_depth_c;
    int stride = mb_pstride[plane];
    P* base = reinterpret_cast<P*>(mb_pix[plane]);
    Residual& r = res;
    bool bypass = mb_bypass;
    const std::int8_t* modes = &intra_modes[static_cast<std::size_t>(mb_addr) * 16];
    if (m.kind == k_kind_i16x16) {
        IntraEdge e;
        gather_intra_edge(plane, 0, 0, 16, false, e);
        intra_pred_16x16<P>(base, stride, m.i16_mode, e, bd);
        if (bypass) {
            std::int32_t* rm = res_mb[plane];
            std::memset(rm, 0, sizeof(res_mb[plane]));
            for (int blk = 0; blk < 16; ++blk) {
                if (!(r.mask4[plane] & (1u << blk))) continue;
                int ox = (blk & 3) * 4;
                int oy = (blk >> 2) * 4;
                for (int k = 0; k < 16; ++k) rm[(oy + (k >> 2)) * 16 + ox + (k & 3)] = r.c4[plane][blk][k];
                std::memset(r.c4[plane][blk], 0, sizeof(r.c4[plane][blk]));
            }
            if (m.i16_mode == 0 || m.i16_mode == 1) dpcm(rm, 16, 16, 16, m.i16_mode == 0);
            residual_add_t<P>(base, stride, rm, 16, 16, 16, bd);
        } else {
            for (int blk = 0; blk < 16; ++blk) {
                if (!(r.mask4[plane] & (1u << blk))) continue;
                P* dst = base + (blk >> 2) * 4 * stride + (blk & 3) * 4;
                idct4x4_add_t<P>(dst, stride, r.c4[plane][blk], bd);
            }
        }
        r.mask4[plane] = 0;
        r.dc_only4[plane] = 0;
        return;
    }
    if (m.kind == k_kind_i8x8) {
        for (int b8 = 0; b8 < 4; ++b8) {
            int x8 = b8 & 1;
            int y8 = b8 >> 1;
            P* dst = base + y8 * 8 * stride + x8 * 8;
            IntraEdge e;
            bool inner_tr = !(x8 == 1 && y8 == 1);
            gather_intra_edge(plane, x8 * 8, y8 * 8, 8, inner_tr, e);
            int mode = modes[(y8 * 2) * 4 + x8 * 2];
            intra_pred_nxn<P>(dst, stride, 8, mode, e, bd);
            if (r.mask8[plane] & (1u << b8)) {
                if (bypass) {
                    if (mode == 0 || mode == 1) dpcm(r.c8[plane][b8], 8, 8, 8, mode == 0);
                    residual_add_t<P>(dst, stride, r.c8[plane][b8], 8, 8, 8, bd);
                    std::memset(r.c8[plane][b8], 0, sizeof(r.c8[plane][b8]));
                } else {
                    idct8x8_add_t<P>(dst, stride, r.c8[plane][b8], bd);
                }
            }
        }
        r.mask8[plane] = 0;
        return;
    }
    for (int bi = 0; bi < 16; ++bi) {
        int x4 = k_blk_x[bi];
        int y4 = k_blk_y[bi];
        int blk = y4 * 4 + x4;
        P* dst = base + y4 * 4 * stride + x4 * 4;
        bool inner_tr;
        if (y4 == 0) {
            inner_tr = true;
        } else {
            inner_tr = x4 < 3 && k_blk_index[y4 - 1][x4 + 1] < bi;
        }
        IntraEdge e;
        gather_intra_edge(plane, x4 * 4, y4 * 4, 4, inner_tr, e);
        int mode = modes[blk];
        intra_pred_nxn<P>(dst, stride, 4, mode, e, bd);
        if (r.mask4[plane] & (1u << blk)) {
            if (bypass) {
                if (mode == 0 || mode == 1) dpcm(r.c4[plane][blk], 4, 4, 4, mode == 0);
                residual_add_t<P>(dst, stride, r.c4[plane][blk], 4, 4, 4, bd);
                std::memset(r.c4[plane][blk], 0, sizeof(r.c4[plane][blk]));
            } else {
                idct4x4_add_t<P>(dst, stride, r.c4[plane][blk], bd);
            }
        }
    }
    r.mask4[plane] = 0;
    r.dc_only4[plane] = 0;
}

template <typename P>
void DecoderState::reconstruct_chroma_t() {
    if (chroma_array_type != 1 && chroma_array_type != 2) return;
    const MbParse& m = mp;
    bool intra = m.kind <= k_kind_pcm;
    int bd = bit_depth_c;
    int bw = mb_wc / 4;
    int nblk = (mb_wc / 4) * (mb_hc / 4);
    Residual& r = res;
    for (int c = 0; c < 2; ++c) {
        int stride = mb_pstride[1 + c];
        P* base = reinterpret_cast<P*>(mb_pix[1 + c]);
        if (intra) {
            IntraEdge e;
            gather_intra_edge2(1 + c, 0, 0, mb_wc, mb_hc, false, e);
            intra_pred_chroma_block<P>(base, stride, mb_wc, mb_hc, m.chroma_mode, e, bd);
        }
        if (!r.mask4[1 + c]) continue;
        if (mb_bypass) {
            std::int32_t* rm = res_mb[1 + c];
            std::memset(rm, 0, sizeof(res_mb[1 + c]));
            for (int b = 0; b < nblk; ++b) {
                if (!(r.mask4[1 + c] & (1u << b))) continue;
                int ox = (b % bw) * 4;
                int oy = (b / bw) * 4;
                for (int k = 0; k < 16; ++k) rm[(oy + (k >> 2)) * 8 + ox + (k & 3)] = r.c4[1 + c][b][k];
                std::memset(r.c4[1 + c][b], 0, sizeof(r.c4[1 + c][b]));
            }
            if (intra && (m.chroma_mode == 1 || m.chroma_mode == 2)) dpcm(rm, 8, mb_wc, mb_hc, m.chroma_mode == 2);
            residual_add_t<P>(base, stride, rm, 8, mb_wc, mb_hc, bd);
        } else {
            for (int b = 0; b < nblk; ++b) {
                if (!(r.mask4[1 + c] & (1u << b))) continue;
                P* dst = base + (b / bw) * 4 * stride + (b % bw) * 4;
                idct4x4_add_t<P>(dst, stride, r.c4[1 + c][b], bd);
            }
        }
        r.mask4[1 + c] = 0;
        r.dc_only4[1 + c] = 0;
    }
}

template <typename P>
void DecoderState::add_residual_plane_t(int plane, bool intra_blocks) {
    (void)intra_blocks;
    Residual& r = res;
    int bd = (plane == 0 || num_planes > 1) ? bit_depth_y : bit_depth_c;
    int stride = mb_pstride[plane];
    P* base = reinterpret_cast<P*>(mb_pix[plane]);
    if (mp.transform8x8) {
        for (int b8 = 0; b8 < 4; ++b8) {
            if (!(r.mask8[plane] & (1u << b8))) continue;
            P* dst = base + (b8 >> 1) * 8 * stride + (b8 & 1) * 8;
            if (mb_bypass) {
                residual_add_t<P>(dst, stride, r.c8[plane][b8], 8, 8, 8, bd);
                std::memset(r.c8[plane][b8], 0, sizeof(r.c8[plane][b8]));
            } else {
                idct8x8_add_t<P>(dst, stride, r.c8[plane][b8], bd);
            }
        }
        r.mask8[plane] = 0;
        return;
    }
    for (int blk = 0; blk < 16; ++blk) {
        if (!(r.mask4[plane] & (1u << blk))) continue;
        P* dst = base + (blk >> 2) * 4 * stride + (blk & 3) * 4;
        if (mb_bypass) {
            residual_add_t<P>(dst, stride, r.c4[plane][blk], 4, 4, 4, bd);
            std::memset(r.c4[plane][blk], 0, sizeof(r.c4[plane][blk]));
        } else {
            idct4x4_add_t<P>(dst, stride, r.c4[plane][blk], bd);
        }
    }
    r.mask4[plane] = 0;
    r.dc_only4[plane] = 0;
}

template <typename P>
void DecoderState::write_pcm_t() {
    const std::uint16_t* s = mp.pcm16;
    {
        P* y = reinterpret_cast<P*>(mb_pix[0]);
        for (int r = 0; r < 16; ++r)
            for (int x = 0; x < 16; ++x) y[r * mb_pstride[0] + x] = static_cast<P>(s[r * 16 + x]);
    }
    if (chroma_array_type == 0) return;
    int cw = chroma_array_type == 3 ? 16 : mb_wc;
    int ch = chroma_array_type == 3 ? 16 : mb_hc;
    for (int c = 0; c < 2; ++c) {
        P* p = reinterpret_cast<P*>(mb_pix[1 + c]);
        const std::uint16_t* src = s + 256 + c * cw * ch;
        for (int r = 0; r < ch; ++r)
            for (int x = 0; x < cw; ++x) p[r * mb_pstride[1 + c] + x] = static_cast<P>(src[r * cw + x]);
    }
}

void DecoderState::write_pcm() {
    if (cur_pixel_bytes == 2) {
        write_pcm_t<std::uint16_t>();
    } else {
        write_pcm_t<std::uint8_t>();
    }
}

void DecoderState::add_luma_residual() {
    if (cur_pixel_bytes == 2) {
        add_residual_plane_t<std::uint16_t>(0, false);
    } else {
        add_residual_plane_t<std::uint8_t>(0, false);
    }
}

void DecoderState::add_chroma_residual() {
    if (chroma_array_type == 3) {
        for (int p = 1; p < 3; ++p) {
            if (cur_pixel_bytes == 2) {
                add_residual_plane_t<std::uint16_t>(p, false);
            } else {
                add_residual_plane_t<std::uint8_t>(p, false);
            }
        }
        return;
    }
    if (cur_pixel_bytes == 2) {
        reconstruct_chroma_t<std::uint16_t>();
    } else {
        reconstruct_chroma_t<std::uint8_t>();
    }
}

void DecoderState::reconstruct_intra_luma() {
    if (cur_pixel_bytes == 2) {
        reconstruct_intra_t<std::uint16_t>(0);
    } else {
        reconstruct_intra_t<std::uint8_t>(0);
    }
}

void DecoderState::reconstruct_intra_chroma() {
    if (chroma_array_type == 3) {
        for (int p = 1; p < 3; ++p) {
            if (cur_pixel_bytes == 2) {
                reconstruct_intra_t<std::uint16_t>(p);
            } else {
                reconstruct_intra_t<std::uint8_t>(p);
            }
        }
        return;
    }
    if (cur_pixel_bytes == 2) {
        reconstruct_chroma_t<std::uint16_t>();
    } else {
        reconstruct_chroma_t<std::uint8_t>();
    }
}

void DecoderState::reconstruct_plane_intra(int plane) {
    if (cur_pixel_bytes == 2) {
        reconstruct_intra_t<std::uint16_t>(plane);
    } else {
        reconstruct_intra_t<std::uint8_t>(plane);
    }
}

template <typename P>
void DecoderState::mc_partition_t(int x4, int y4, int w4, int h4) {
    std::size_t idx = (motion_offset + static_cast<std::size_t>(mb_addr)) * 16 + static_cast<std::size_t>(y4 * 4 + x4);
    int r0 = cur->ref_idx[0][idx];
    int r1 = cur->ref_idx[1][idx];
    Mv mv0 = cur->mv[0][idx];
    Mv mv1 = cur->mv[1][idx];
    RefEntry e0 = r0 >= 0 ? ref_entry(0, r0) : RefEntry();
    RefEntry e1 = r1 >= 0 ? ref_entry(1, r1) : RefEntry();
    if (!e0.pic && !e1.pic) {
        mb_error = true;
        e0 = ref_entry(0, 0);
        if (!e0.pic) e0 = ref_entry(1, 0);
        if (!e0.pic) return;
        r0 = 0;
        mv0.x = mv0.y = 0;
        r1 = -1;
    }
    int w = w4 * 4;
    int h = h4 * 4;
    int px = mb_x * 16 + x4 * 4;
    int py;
    if (mbaff) {
        py = mb_field ? pair_y * 16 + y4 * 4 : pair_y * 32 + (mb_bottom ? 16 : 0) + y4 * 4;
    } else {
        py = mb_y * 16 + y4 * 4;
    }
    int lplane = num_planes > 1 ? colour_plane : 0;
    int ls = mb_pstride[0];
    P* dst = reinterpret_cast<P*>(mb_pix[0]) + y4 * 4 * ls + x4 * 4;
    bool explicit_w = sh.has_weights;
    bool use_implicit = sh.slice_type == k_slice_b && pps->weighted_bipred_idc == 2;
    int wr0 = (mbaff && mb_field) ? r0 >> 1 : r0;
    int wr1 = (mbaff && mb_field) ? r1 >> 1 : r1;
    int bdy = bit_depth_y;
    int bdc = bit_depth_c;
    int oy_shift = bdy - 8;
    int oc_shift = bdc - 8;
    P* t0 = reinterpret_cast<P*>(mc_tmp[0]);
    P* t1 = reinterpret_cast<P*>(mc_tmp[1]);
    int iw0 = 32;
    if (use_implicit && e0.pic && e1.pic) {
        if (mbaff && mb_field) {
            iw0 = implicit_wf[mb_bottom ? 1 : 0][clip3(0, 63, r0)][clip3(0, 63, r1)];
        } else {
            iw0 = implicit_w[clip3(0, 31, r0)][clip3(0, 31, r1)];
        }
    }
    int cur_par = field_pic ? (bottom_field ? 1 : 0) : (mb_bottom ? 1 : 0);
    bool cur_is_field = field_pic || (mbaff && mb_field);
    auto chroma_mvy = [&](const RefEntry& e, int mvy) -> int {
        if (chroma_array_type != 1 || !cur_is_field || e.structure == k_struct_frame) return mvy;
        int rp = e.structure == k_struct_bottom ? 1 : 0;
        if (cur_par == 0 && rp == 1) return mvy - 2;
        if (cur_par == 1 && rp == 0) return mvy + 2;
        return mvy;
    };
    PicView view0;
    PicView view1;
    if (e0.pic) view0 = e0.pic->view(e0.structure);
    if (e1.pic) view1 = e1.pic->view(e1.structure);
    auto pred_plane = [&](P* out, int os, const RefEntry& e, Mv mv, int c) {
        const PicView& v = &e == &e0 ? view0 : view1;
        if (c == 0 || chroma_array_type == 3) {
            int pl = c == 0 ? lplane : c;
            mc_luma_t<P>(out, os, v, pl, px, py, mv.x, mv.y, w, h, c == 0 ? bdy : bdc);
        } else {
            int cx = px / sub_w;
            int cy = py / sub_h;
            int cmvy = chroma_mvy(e, mv.y);
            int mvy8 = chroma_array_type == 2 ? cmvy * 2 : cmvy;
            mc_chroma_t<P>(out, os, v, c, cx, cy, mv.x, mvy8, w / sub_w, h / sub_h);
        }
    };
    int planes = (chroma_array_type == 0) ? 1 : 3;
    for (int c = 0; c < planes; ++c) {
        int cw = (c == 0 || chroma_array_type == 3) ? w : w / sub_w;
        int chh = (c == 0 || chroma_array_type == 3) ? h : h / sub_h;
        int cs = mb_pstride[c];
        P* cd;
        if (c == 0) {
            cd = dst;
        } else {
            int ox = (chroma_array_type == 3) ? x4 * 4 : x4 * 4 / sub_w;
            int oy = (chroma_array_type == 3) ? y4 * 4 : y4 * 4 / sub_h;
            cd = reinterpret_cast<P*>(mb_pix[c]) + oy * cs + ox;
        }
        int bd = c == 0 ? bdy : bdc;
        int osh = c == 0 ? oy_shift : oc_shift;
        if (e0.pic && e1.pic && r1 >= 0) {
            pred_plane(t0, 16, e0, mv0, c);
            pred_plane(t1, 16, e1, mv1, c);
            if (explicit_w) {
                int lw0;
                int lw1;
                int off;
                int lwd;
                if (c == 0) {
                    lwd = sh.luma_log2_denom;
                    lw0 = sh.luma_weight[0][wr0];
                    lw1 = sh.luma_weight[1][wr1];
                    off = ((sh.luma_offset[0][wr0] * (1 << osh)) + (sh.luma_offset[1][wr1] * (1 << osh)) + 1) >> 1;
                } else {
                    lwd = sh.chroma_log2_denom;
                    lw0 = sh.chroma_weight[0][wr0][c - 1];
                    lw1 = sh.chroma_weight[1][wr1][c - 1];
                    off = ((sh.chroma_offset[0][wr0][c - 1] * (1 << osh)) + (sh.chroma_offset[1][wr1][c - 1] * (1 << osh)) + 1) >> 1;
                }
                mc_weight_bi_t<P>(cd, cs, t0, t1, 16, cw, chh, lwd, lw0, lw1, off, bd);
            } else if (use_implicit) {
                mc_weight_bi_t<P>(cd, cs, t0, t1, 16, cw, chh, 5, iw0, 64 - iw0, 0, bd);
            } else {
                mc_avg_t<P>(cd, cs, t0, t1, 16, cw, chh);
            }
            continue;
        }
        const RefEntry& e = e0.pic ? e0 : e1;
        int list = e0.pic ? 0 : 1;
        int wr = e0.pic ? wr0 : wr1;
        Mv mv = e0.pic ? mv0 : mv1;
        if (explicit_w) {
            pred_plane(t0, 16, e, mv, c);
            if (c == 0) {
                mc_weight_uni_t<P>(cd, cs, t0, 16, cw, chh, sh.luma_log2_denom, sh.luma_weight[list][wr], sh.luma_offset[list][wr] * (1 << osh), bd);
            } else {
                mc_weight_uni_t<P>(cd, cs, t0, 16, cw, chh, sh.chroma_log2_denom, sh.chroma_weight[list][wr][c - 1], sh.chroma_offset[list][wr][c - 1] * (1 << osh), bd);
            }
        } else {
            pred_plane(cd, cs, e, mv, c);
        }
    }
}

void DecoderState::mc_partition(int x4, int y4, int w4, int h4) {
    if (cur_pixel_bytes == 2) {
        mc_partition_t<std::uint16_t>(x4, y4, w4, h4);
    } else {
        mc_partition_t<std::uint8_t>(x4, y4, w4, h4);
    }
}

template <typename P>
void DecoderState::reconstruct_inter_t() {
    const MbParse& m = mp;
    std::size_t mbase = (motion_offset + static_cast<std::size_t>(mb_addr)) * 16;
    auto same_motion = [&](int x4, int y4, int w4, int h4) {
        std::size_t b = mbase + static_cast<std::size_t>(y4 * 4 + x4);
        for (int y = 0; y < h4; ++y) {
            for (int x = 0; x < w4; ++x) {
                std::size_t i = mbase + static_cast<std::size_t>((y4 + y) * 4 + x4 + x);
                for (int l = 0; l < 2; ++l) {
                    if (cur->ref_idx[l][i] != cur->ref_idx[l][b]) return false;
                    if (cur->ref_idx[l][i] >= 0 && (cur->mv[l][i].x != cur->mv[l][b].x || cur->mv[l][i].y != cur->mv[l][b].y)) return false;
                }
            }
        }
        return true;
    };
    auto mc_8x8_auto = [&](int b8) {
        int x4 = (b8 & 1) * 2;
        int y4 = (b8 >> 1) * 2;
        if (same_motion(x4, y4, 2, 2)) {
            mc_partition_t<P>(x4, y4, 2, 2);
        } else {
            for (int s = 0; s < 4; ++s) mc_partition_t<P>(x4 + (s & 1), y4 + (s >> 1), 1, 1);
        }
    };
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    info.shape = 3;
    switch (m.kind) {
    case k_kind_16x16:
    case k_kind_p_skip:
        info.shape = 0;
        mc_partition_t<P>(0, 0, 4, 4);
        break;
    case k_kind_16x8:
        info.shape = 1;
        mc_partition_t<P>(0, 0, 4, 2);
        mc_partition_t<P>(0, 2, 4, 2);
        break;
    case k_kind_8x16:
        info.shape = 2;
        mc_partition_t<P>(0, 0, 2, 4);
        mc_partition_t<P>(2, 0, 2, 4);
        break;
    case k_kind_b_skip:
    case k_kind_b_direct:
        if (same_motion(0, 0, 4, 4)) {
            info.shape = 0;
            mc_partition_t<P>(0, 0, 4, 4);
        } else {
            for (int b8 = 0; b8 < 4; ++b8) mc_8x8_auto(b8);
        }
        break;
    default:
        for (int i = 0; i < 4; ++i) {
            int x8 = (i & 1) * 2;
            int y8 = (i >> 1) * 2;
            if (m.sub_pred[i] == 0) {
                mc_8x8_auto(i);
                continue;
            }
            switch (m.sub_shape[i]) {
            case 0:
                mc_partition_t<P>(x8, y8, 2, 2);
                break;
            case 1:
                mc_partition_t<P>(x8, y8, 2, 1);
                mc_partition_t<P>(x8, y8 + 1, 2, 1);
                break;
            case 2:
                mc_partition_t<P>(x8, y8, 1, 2);
                mc_partition_t<P>(x8 + 1, y8, 1, 2);
                break;
            default:
                for (int s = 0; s < 4; ++s) mc_partition_t<P>(x8 + (s & 1), y8 + (s >> 1), 1, 1);
                break;
            }
        }
        break;
    }
    add_residual_plane_t<P>(0, false);
    if (chroma_array_type == 3) {
        add_residual_plane_t<P>(1, false);
        add_residual_plane_t<P>(2, false);
    } else if (chroma_array_type != 0) {
        reconstruct_chroma_t<P>();
    }
}

void DecoderState::reconstruct_inter() {
    if (cur_pixel_bytes == 2) {
        reconstruct_inter_t<std::uint16_t>();
    } else {
        reconstruct_inter_t<std::uint8_t>();
    }
}

template <typename P>
void DecoderState::reconstruct_mb_t() {
    if (mp.kind == k_kind_pcm) {
        write_pcm_t<P>();
        return;
    }
    if (mp.kind <= k_kind_i16x16) {
        reconstruct_intra_t<P>(0);
        if (chroma_array_type == 3) {
            reconstruct_intra_t<P>(1);
            reconstruct_intra_t<P>(2);
        } else {
            reconstruct_chroma_t<P>();
        }
        return;
    }
    reconstruct_inter_t<P>();
}

void DecoderState::reconstruct_mb() {
    if (cur_pixel_bytes == 2) {
        reconstruct_mb_t<std::uint16_t>();
    } else {
        reconstruct_mb_t<std::uint8_t>();
    }
}

void DecoderState::decode_skip_mb(bool begin) {
    if (begin) begin_mb_info();
    MbParse& m = mp;
    bool b = sh.slice_type == k_slice_b;
    m.kind = b ? k_kind_b_skip : k_kind_p_skip;
    m.cbp = 0;
    m.transform8x8 = false;
    last_qp_delta = 0;
    int flags = k_mb_inter | k_mb_skip;
    if (b) flags |= k_mb_direct;
    MbInfo& info = mbs[static_cast<std::size_t>(mb_addr)];
    info.direct_mask = b ? 15 : 0;
    info.flags = static_cast<std::uint16_t>(flags | (mb_field ? k_mb_field : 0));
    info.qp = static_cast<std::int8_t>(qp);
    mb_bypass = sps->transform_bypass && qp + qp_bd_y == 0;
    compute_motion();
    store_mb_info(flags);
    info.direct_mask = b ? 15 : 0;
    reconstruct_inter();
}

} // namespace h264_internal
} // namespace video
} // namespace fizmo
