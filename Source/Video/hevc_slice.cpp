#include "hevc_internal.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace video {
namespace hevc_internal {

namespace {

inline int chroma_qp_map(int qpi, int chroma_array_type) noexcept {
    if (chroma_array_type != 1) return qpi < 51 ? qpi : 51;
    if (qpi < 30) return qpi;
    if (qpi >= 70) return qpi - 6;
    return k_chroma_qp_table[qpi];
}

} // namespace

PicDecoder::PicDecoder() {
    std::memset(m_refs, 0, sizeof(m_refs));
    std::memset(m_ref_poc, 0, sizeof(m_ref_poc));
    std::memset(m_ref_lt, 0, sizeof(m_ref_lt));
    std::memset(m_ctx, 0, sizeof(m_ctx));
    std::memset(m_ctx_wpp, 0, sizeof(m_ctx_wpp));
    std::memset(m_ctx_ds, 0, sizeof(m_ctx_ds));
    std::memset(m_coeff, 0, sizeof(m_coeff));
    std::memset(m_res, 0, sizeof(m_res));
    std::memset(m_res_y, 0, sizeof(m_res_y));
    std::memset(m_res_c, 0, sizeof(m_res_c));
    std::memset(m_tmp, 0, sizeof(m_tmp));
    std::memset(m_pred, 0, sizeof(m_pred));
    std::memset(m_mc_tmp, 0, sizeof(m_mc_tmp));
}

bool PicDecoder::begin(const Sps& sps, const Pps& pps, const Layout& lay, Picture* pic) {
    m_sps = &sps;
    m_pps = &pps;
    m_lay = &lay;
    m_pic = pic;
    m_w = sps.width;
    m_h = sps.height;
    m_w4 = lay.w4;
    m_h4 = lay.h4;
    m_log2_ctb = sps.log2_ctb;
    m_ctb_size = sps.ctb_size;
    m_min_cb = sps.log2_min_cb;
    m_bd_y = sps.bit_depth_y;
    m_bd_c = sps.bit_depth_c;
    m_chroma = sps.chroma_array_type;
    m_sx = sps.sub_width_c == 2 ? 1 : 0;
    m_sy = sps.sub_height_c == 2 ? 1 : 0;
    m_error = false;
    const std::size_t n4 = static_cast<std::size_t>(m_w4) * static_cast<std::size_t>(m_h4);
    m_cu_flags.assign(n4, 0);
    m_ct_depth.assign(n4, 0);
    m_ipm.assign(n4, 1);
    m_qp.assign(n4, 0);
    m_nz.assign(n4, 0);
    m_edge_v.assign(n4, 0);
    m_edge_h.assign(n4, 0);
    const std::size_t nctb = static_cast<std::size_t>(sps.pic_w_ctb) * static_cast<std::size_t>(sps.pic_h_ctb);
    m_ctb_slice_addr.assign(nctb, -1);
    m_ctb_slice_idx.assign(nctb, 0);
    SaoParams zero;
    std::memset(&zero, 0, sizeof(zero));
    m_sao.assign(nctb, zero);
    m_slices.clear();
    m_any_nofilter = false;
    m_any_sao = false;
    m_any_deblock = false;
    m_ds_valid = false;
    pic->ctb_slice.assign(nctb, 0);
    pic->slices.clear();
    MvField mz;
    std::memset(&mz, 0, sizeof(mz));
    mz.ref_idx[0] = mz.ref_idx[1] = -1;
    pic->mvf.assign(n4, mz);
    pic->mvf_stride = m_w4;
    m_log2_min_qg = m_log2_ctb - pps.diff_cu_qp_delta_depth;
    m_log2_min_cqg = m_log2_ctb - pps.diff_cu_chroma_qp_offset_depth;
    return true;
}

bool PicDecoder::avail_lt(int xn, int yn) const noexcept {
    if (xn < 0 || yn < 0 || xn >= m_w || yn >= m_h) return false;
    const int rs = (yn >> m_log2_ctb) * m_sps->pic_w_ctb + (xn >> m_log2_ctb);
    if (rs == m_ctb_rs) return true;
    if (m_ctb_slice_addr[static_cast<std::size_t>(rs)] != m_slice_addr) return false;
    return m_lay->tile_id[static_cast<std::size_t>(m_lay->rs_to_ts[static_cast<std::size_t>(rs)])] == m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)];
}

bool PicDecoder::avail(int xc, int yc, int xn, int yn) const noexcept {
    if (xn < 0 || yn < 0 || xn >= m_w || yn >= m_h) return false;
    const std::uint32_t zn = m_lay->zs[static_cast<std::size_t>(yn >> 2) * static_cast<std::size_t>(m_w4) + static_cast<std::size_t>(xn >> 2)];
    const std::uint32_t zc = m_lay->zs[static_cast<std::size_t>(yc >> 2) * static_cast<std::size_t>(m_w4) + static_cast<std::size_t>(xc >> 2)];
    if (zn > zc) return false;
    const int rs = (yn >> m_log2_ctb) * m_sps->pic_w_ctb + (xn >> m_log2_ctb);
    if (rs == m_ctb_rs) return true;
    if (m_ctb_slice_addr[static_cast<std::size_t>(rs)] != m_slice_addr) return false;
    return m_lay->tile_id[static_cast<std::size_t>(m_lay->rs_to_ts[static_cast<std::size_t>(rs)])] == m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)];
}

bool PicDecoder::decode_slice(const SliceHeader& sh, const std::uint8_t* data, std::size_t size, const std::vector<std::size_t>& substreams,
                              Picture* const refs[2][k_max_refs], std::string* error) {
    m_sh = &sh;
    m_slice_type = sh.slice_type;
    m_no_backward_pred = true;
    SliceRefs sr;
    std::memset(&sr, 0, sizeof(sr));
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < k_max_refs; ++i) {
            Picture* p = i < sh.num_ref_idx[l] ? refs[l][i] : nullptr;
            m_refs[l][i] = p;
            m_ref_poc[l][i] = p ? p->poc : 0;
            m_ref_lt[l][i] = p ? p->ref == 2 : false;
            sr.poc[l][i] = m_ref_poc[l][i];
            sr.lt[l][i] = m_ref_lt[l][i] ? 1 : 0;
            sr.uid[l][i] = p ? p->uid : 0;
            if (p && p->poc > m_pic->poc) m_no_backward_pred = false;
        }
    }
    if (m_pic->slices.size() >= 65535) {
        if (error) *error = "too many slices";
        return false;
    }
    m_slice_idx = static_cast<int>(m_pic->slices.size());
    m_pic->slices.push_back(sr);
    SliceParams sp;
    sp.beta_offset = sh.beta_offset_div2 * 2;
    sp.tc_offset = sh.tc_offset_div2 * 2;
    sp.deblocking_disabled = sh.deblocking_disabled;
    sp.lf_across_slices = sh.lf_across_slices;
    sp.sao_luma = sh.sao_luma;
    sp.sao_chroma = sh.sao_chroma;
    sp.slice_addr_rs = sh.slice_addr_rs;
    m_slices.push_back(sp);
    if (!sh.deblocking_disabled) m_any_deblock = true;
    if (sh.slice_type == SLICE_I) {
        m_init_type = 0;
    } else if (sh.slice_type == SLICE_P) {
        m_init_type = sh.cabac_init ? 2 : 1;
    } else {
        m_init_type = sh.cabac_init ? 1 : 2;
    }
    const int pw = m_sps->pic_w_ctb;
    const int nctb = pw * m_sps->pic_h_ctb;
    if (sh.segment_address < 0 || sh.segment_address >= nctb || substreams.empty()) {
        if (error) *error = "invalid slice segment address";
        return false;
    }
    m_slice_addr = sh.slice_addr_rs;
    m_ctb_rs = sh.segment_address;
    m_ctb_ts = m_lay->rs_to_ts[static_cast<std::size_t>(m_ctb_rs)];
    if (!sh.dependent) {
        m_cu_qp_offset_cb = 0;
        m_cu_qp_offset_cr = 0;
    }
    m_cabac.attach(cabac_oracle(), m_ctx);
    std::size_t sub = 0;
    auto sub_end = [&](std::size_t k) -> std::size_t {
        return k + 1 < substreams.size() ? std::min(substreams[k + 1], size) : size;
    };
    std::size_t sub_start = std::min(substreams[0], size);
    m_cabac.init(data + sub_start, sub_end(0) - sub_start);
    const bool wpp = m_pps->entropy_coding_sync;
    auto init_ctx = [&]() {
        init_contexts(m_ctx, m_init_type, sh.slice_qp);
        m_stat[0] = m_stat[1] = m_stat[2] = m_stat[3] = 0;
    };
    auto wpp_sync = [&](int x0, int y0) {
        const int xt = x0 + m_ctb_size;
        const int yt = y0 - m_ctb_size;
        bool ok = false;
        if (xt < m_w && yt >= 0) {
            const int rs = (yt >> m_log2_ctb) * pw + (xt >> m_log2_ctb);
            ok = m_ctb_slice_addr[static_cast<std::size_t>(rs)] == m_slice_addr &&
                 m_lay->tile_id[static_cast<std::size_t>(m_lay->rs_to_ts[static_cast<std::size_t>(rs)])] == m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)];
        }
        if (ok) {
            std::memcpy(m_ctx, m_ctx_wpp, sizeof(m_ctx));
            std::memcpy(m_stat, m_stat_wpp, sizeof(m_stat));
        } else {
            init_ctx();
        }
    };
    {
        const int cx = m_ctb_rs % pw;
        const int cy = m_ctb_rs / pw;
        const bool first_in_tile = m_ctb_ts == 0 || m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)] != m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts - 1)];
        if (first_in_tile) {
            init_ctx();
        } else if (wpp && cx == m_lay->ctb_tile_x0[static_cast<std::size_t>(m_ctb_rs)]) {
            wpp_sync(cx << m_log2_ctb, cy << m_log2_ctb);
        } else if (sh.dependent && m_ds_valid) {
            std::memcpy(m_ctx, m_ctx_ds, sizeof(m_ctx));
            std::memcpy(m_stat, m_stat_ds, sizeof(m_stat));
        } else {
            init_ctx();
        }
    }
    bool first_ctb = true;
    while (true) {
        const int cx = m_ctb_rs % pw;
        const int cy = m_ctb_rs / pw;
        const int tx0 = m_lay->ctb_tile_x0[static_cast<std::size_t>(m_ctb_rs)];
        const bool first_in_tile = m_ctb_ts == 0 || m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)] != m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts - 1)];
        if ((first_ctb && !sh.dependent) || first_in_tile || (wpp && cx == tx0)) m_last_qp = sh.slice_qp;
        first_ctb = false;
        m_ctb_slice_addr[static_cast<std::size_t>(m_ctb_rs)] = m_slice_addr;
        m_ctb_slice_idx[static_cast<std::size_t>(m_ctb_rs)] = static_cast<std::uint16_t>(m_slice_idx);
        m_pic->ctb_slice[static_cast<std::size_t>(m_ctb_rs)] = static_cast<std::uint16_t>(m_slice_idx);
        m_cur_sub_start = sub_start;
        m_cur_sub_end = sub_end(sub);
        m_cur_data = data;
        if (sh.sao_luma || sh.sao_chroma) sao_syntax(cx, cy, m_ctb_rs);
        coding_quadtree(cx << m_log2_ctb, cy << m_log2_ctb, m_log2_ctb, 0);
        sub_start = m_cur_sub_start;
        if (m_error || m_cabac.overrun()) {
            m_error = true;
            if (error) *error = "corrupt slice data";
            return false;
        }
        const int end = m_cabac.terminate(TERM_END_OF_SLICE);
        if (wpp && cx - tx0 == 1) {
            std::memcpy(m_ctx_wpp, m_ctx, sizeof(m_ctx));
            std::memcpy(m_stat_wpp, m_stat, sizeof(m_stat));
        }
        ++m_ctb_ts;
        if (end) {
            if (m_pps->dependent_slice_segments) {
                std::memcpy(m_ctx_ds, m_ctx, sizeof(m_ctx));
                std::memcpy(m_stat_ds, m_stat, sizeof(m_stat));
                m_ds_valid = true;
            }
            break;
        }
        if (m_ctb_ts >= nctb) {
            m_error = true;
            if (error) *error = "slice data runs past the end of the picture";
            return false;
        }
        m_ctb_rs = m_lay->ts_to_rs[static_cast<std::size_t>(m_ctb_ts)];
        const int ncx = m_ctb_rs % pw;
        const int ncy = m_ctb_rs / pw;
        const bool new_tile = m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts)] != m_lay->tile_id[static_cast<std::size_t>(m_ctb_ts - 1)];
        if ((m_pps->tiles && new_tile) || (wpp && ncx == m_lay->ctb_tile_x0[static_cast<std::size_t>(m_ctb_rs)])) {
            m_cabac.terminate(TERM_END_OF_SUBSET);
            if (m_cabac.oracle()) {
                ++sub;
                m_cabac.init(nullptr, 0);
                if (new_tile) {
                    init_ctx();
                } else {
                    wpp_sync(ncx << m_log2_ctb, ncy << m_log2_ctb);
                }
                continue;
            }
            std::size_t next;
            if (sub + 1 < substreams.size()) {
                next = substreams[sub + 1];
            } else {
                next = sub_start + m_cabac.aligned_position();
            }
            ++sub;
            if (next >= size) {
                m_error = true;
                if (error) *error = "missing slice substream";
                return false;
            }
            sub_start = next;
            const std::size_t e = sub < substreams.size() ? sub_end(sub) : size;
            m_cabac.init(data + sub_start, (e > sub_start ? e : size) - sub_start);
            if (new_tile) {
                init_ctx();
            } else {
                wpp_sync(ncx << m_log2_ctb, ncy << m_log2_ctb);
            }
        }
    }
    return true;
}

void PicDecoder::sao_syntax(int rx, int ry, int rs) {
    SaoParams& p = m_sao[static_cast<std::size_t>(rs)];
    std::memset(&p, 0, sizeof(p));
    const int pw = m_sps->pic_w_ctb;
    const std::vector<int>& tid = m_lay->tile_id;
    const std::vector<int>& r2t = m_lay->rs_to_ts;
    if (rx > 0) {
        const bool in_slice = rs > m_slice_addr;
        const bool in_tile = tid[static_cast<std::size_t>(m_ctb_ts)] == tid[static_cast<std::size_t>(r2t[static_cast<std::size_t>(rs - 1)])];
        if (in_slice && in_tile && m_cabac.decode(m_ctx[CTX_SAO_MERGE])) {
            p = m_sao[static_cast<std::size_t>(rs - 1)];
            if (p.type[0] || p.type[1] || p.type[2]) m_any_sao = true;
            return;
        }
    }
    if (ry > 0) {
        const bool in_slice = rs - pw >= m_slice_addr;
        const bool in_tile = tid[static_cast<std::size_t>(m_ctb_ts)] == tid[static_cast<std::size_t>(r2t[static_cast<std::size_t>(rs - pw)])];
        if (in_slice && in_tile && m_cabac.decode(m_ctx[CTX_SAO_MERGE])) {
            p = m_sao[static_cast<std::size_t>(rs - pw)];
            if (p.type[0] || p.type[1] || p.type[2]) m_any_sao = true;
            return;
        }
    }
    const int ncomp = m_chroma != 0 ? 3 : 1;
    for (int c = 0; c < ncomp; ++c) {
        if (!((m_sh->sao_luma && c == 0) || (m_sh->sao_chroma && c > 0))) {
            p.type[c] = 0;
            continue;
        }
        if (c == 2) {
            p.type[2] = p.type[1];
            p.eo_class[2] = p.eo_class[1];
        } else {
            int t = 0;
            if (m_cabac.decode(m_ctx[CTX_SAO_TYPE])) t = m_cabac.bypass() ? 2 : 1;
            p.type[c] = static_cast<std::uint8_t>(t);
        }
        if (p.type[c] == 0) continue;
        m_any_sao = true;
        const int bd = c == 0 ? m_bd_y : m_bd_c;
        const int cmax = (1 << ((bd < 10 ? bd : 10) - 5)) - 1;
        int abs_v[4];
        for (int i = 0; i < 4; ++i) {
            int v = 0;
            while (v < cmax && m_cabac.bypass()) ++v;
            abs_v[i] = v;
        }
        const int scale = c == 0 ? m_pps->log2_sao_offset_scale_luma : m_pps->log2_sao_offset_scale_chroma;
        if (p.type[c] == 1) {
            for (int i = 0; i < 4; ++i) {
                if (abs_v[i] && m_cabac.bypass()) abs_v[i] = -abs_v[i];
            }
            p.band_pos[c] = static_cast<std::uint8_t>(m_cabac.bypass_bits(5));
            for (int i = 0; i < 4; ++i) p.offset[c][i + 1] = static_cast<std::int16_t>(abs_v[i] * (1 << scale));
        } else {
            if (c == 0) p.eo_class[0] = static_cast<std::uint8_t>(m_cabac.bypass_bits(2));
            if (c == 1) p.eo_class[1] = static_cast<std::uint8_t>(m_cabac.bypass_bits(2));
            p.offset[c][1] = static_cast<std::int16_t>(abs_v[0] * (1 << scale));
            p.offset[c][2] = static_cast<std::int16_t>(abs_v[1] * (1 << scale));
            p.offset[c][3] = static_cast<std::int16_t>(-abs_v[2] * (1 << scale));
            p.offset[c][4] = static_cast<std::int16_t>(-abs_v[3] * (1 << scale));
        }
    }
    if (m_chroma != 0 && p.type[1] && m_sh->sao_chroma) {
        p.eo_class[2] = p.eo_class[1];
    }
}

void PicDecoder::start_qg(int xq, int yq) {
    const int mask = m_ctb_size - 1;
    const int prev = m_last_qp;
    const int qa = (xq & mask) ? m_qp[idx4(xq - 1, yq)] : prev;
    const int qb = (yq & mask) ? m_qp[idx4(xq, yq - 1)] : prev;
    m_qp_y_pred = (qa + qb + 1) >> 1;
    m_cu_qp_delta = 0;
}

void PicDecoder::update_qp() {
    const int off = m_sps->qp_bd_offset_y;
    m_qp_y = ((m_qp_y_pred + m_cu_qp_delta + 52 + 2 * off) % (52 + off)) - off;
}

int PicDecoder::chroma_qp(int cidx) const {
    const int off = cidx == 1 ? m_pps->cb_qp_offset + m_sh->cb_qp_offset + m_cu_qp_offset_cb : m_pps->cr_qp_offset + m_sh->cr_qp_offset + m_cu_qp_offset_cr;
    const int qpi = clip3(-m_sps->qp_bd_offset_c, 57, m_qp_y + off);
    return chroma_qp_map(qpi, m_chroma) + m_sps->qp_bd_offset_c;
}

void PicDecoder::coding_quadtree(int x0, int y0, int log2, int depth) {
    if (m_error) return;
    const int size = 1 << log2;
    bool split;
    if (x0 + size <= m_w && y0 + size <= m_h && log2 > m_min_cb) {
        int inc = 0;
        if (avail_lt(x0 - 1, y0) && m_ct_depth[idx4(x0 - 1, y0)] > depth) ++inc;
        if (avail_lt(x0, y0 - 1) && m_ct_depth[idx4(x0, y0 - 1)] > depth) ++inc;
        split = m_cabac.decode(m_ctx[CTX_SPLIT_CU + inc]) != 0;
    } else {
        split = log2 > m_min_cb;
    }
    if (log2 >= m_log2_min_qg) {
        m_qp_delta_coded = false;
        start_qg(x0, y0);
    }
    if (m_sh->cu_chroma_qp_offset_enabled && log2 >= m_log2_min_cqg) m_chroma_qp_offset_coded = false;
    if (split) {
        const int h = size >> 1;
        coding_quadtree(x0, y0, log2 - 1, depth + 1);
        if (x0 + h < m_w) coding_quadtree(x0 + h, y0, log2 - 1, depth + 1);
        if (y0 + h < m_h) coding_quadtree(x0, y0 + h, log2 - 1, depth + 1);
        if (x0 + h < m_w && y0 + h < m_h) coding_quadtree(x0 + h, y0 + h, log2 - 1, depth + 1);
    } else {
        m_cur_depth = depth;
        coding_unit(x0, y0, log2);
    }
}

void PicDecoder::fill_map(std::vector<std::uint8_t>& map, int x0, int y0, int w, int h, std::uint8_t v) {
    const int xe = std::min(x0 + w, m_w4 * 4);
    const int ye = std::min(y0 + h, m_h4 * 4);
    for (int y = y0; y < ye; y += 4) {
        std::uint8_t* row = map.data() + static_cast<std::size_t>(y >> 2) * static_cast<std::size_t>(m_w4);
        for (int x = x0; x < xe; x += 4) row[x >> 2] = v;
    }
}

int PicDecoder::decode_part_mode(bool intra, int log2) {
    std::uint8_t* c = m_ctx + CTX_PART_MODE;
    if (intra) return m_cabac.decode(c[0]) ? PART_2Nx2N : PART_NxN;
    if (m_cabac.decode(c[0])) return PART_2Nx2N;
    if (log2 == m_min_cb) {
        if (m_cabac.decode(c[1])) return PART_2NxN;
        if (log2 == 3) return PART_Nx2N;
        return m_cabac.decode(c[2]) ? PART_Nx2N : PART_NxN;
    }
    if (!m_sps->amp) return m_cabac.decode(c[1]) ? PART_2NxN : PART_Nx2N;
    if (m_cabac.decode(c[1])) {
        if (m_cabac.decode(c[3])) return PART_2NxN;
        return m_cabac.bypass() ? PART_2NxnD : PART_2NxnU;
    }
    if (m_cabac.decode(c[3])) return PART_Nx2N;
    return m_cabac.bypass() ? PART_nRx2N : PART_nLx2N;
}

void PicDecoder::coding_unit(int x0, int y0, int log2) {
    const int n = 1 << log2;
    m_cu_x = x0;
    m_cu_y = y0;
    m_cu_log2 = log2;
    m_cu_bypass = m_pps->transquant_bypass ? m_cabac.decode(m_ctx[CTX_TRANSQUANT_BYPASS]) != 0 : false;
    bool skip = false;
    if (m_slice_type != SLICE_I) {
        int inc = 0;
        if (avail_lt(x0 - 1, y0) && (m_cu_flags[idx4(x0 - 1, y0)] & CU_SKIP)) ++inc;
        if (avail_lt(x0, y0 - 1) && (m_cu_flags[idx4(x0, y0 - 1)] & CU_SKIP)) ++inc;
        skip = m_cabac.decode(m_ctx[CTX_SKIP + inc]) != 0;
    }
    fill_map(m_ct_depth, x0, y0, n, n, static_cast<std::uint8_t>(m_cur_depth));
    std::uint8_t flags = 0;
    if (skip) flags |= CU_SKIP;
    if (m_cu_bypass) {
        flags |= CU_BYPASS | CU_NOFILTER;
        m_any_nofilter = true;
    }
    m_cu_intra = false;
    m_cu_part = PART_2Nx2N;
    m_merge_flag = false;
    m_cu_pcm = false;
    update_qp();
    if (skip) {
        fill_map(m_cu_flags, x0, y0, n, n, flags);
        prediction_unit(x0, y0, n, n, 0);
    } else {
        m_cu_intra = m_slice_type == SLICE_I ? true : m_cabac.decode(m_ctx[CTX_PRED_MODE]) != 0;
        if (!m_cu_intra || log2 == m_min_cb) m_cu_part = decode_part_mode(m_cu_intra, log2);
        if (m_cu_intra) flags |= CU_INTRA;
        fill_map(m_cu_flags, x0, y0, n, n, flags);
        if (m_cu_intra) {
            if (m_cu_part == PART_2Nx2N && m_sps->pcm && log2 >= m_sps->log2_min_pcm && log2 <= m_sps->log2_max_pcm) {
                m_cu_pcm = m_cabac.terminate(TERM_PCM_FLAG) != 0;
            }
            if (m_cu_pcm) {
                flags |= CU_PCM;
                if (m_sps->pcm_loop_filter_disabled) {
                    flags |= CU_NOFILTER;
                    m_any_nofilter = true;
                }
                fill_map(m_cu_flags, x0, y0, n, n, flags);
                fill_map(m_ipm, x0, y0, n, n, 1);
                pcm_sample(x0, y0, log2);
            } else {
                const int nparts = m_cu_part == PART_NxN ? 4 : 1;
                const int pb = m_cu_part == PART_NxN ? n >> 1 : n;
                int prev[4];
                for (int k = 0; k < nparts; ++k) prev[k] = m_cabac.decode(m_ctx[CTX_PREV_INTRA]);
                for (int k = 0; k < nparts; ++k) {
                    const int xp = x0 + (k & 1) * pb;
                    const int yp = y0 + (k >> 1) * pb;
                    int mpm_idx = -1;
                    int rem = 0;
                    if (prev[k]) {
                        mpm_idx = m_cabac.bypass() ? (m_cabac.bypass() ? 2 : 1) : 0;
                    } else {
                        rem = static_cast<int>(m_cabac.bypass_bits(5));
                    }
                    int ca = 1;
                    int cb = 1;
                    if (avail(xp, yp, xp - 1, yp)) {
                        const std::size_t i = idx4(xp - 1, yp);
                        if ((m_cu_flags[i] & (CU_INTRA | CU_PCM)) == CU_INTRA) ca = m_ipm[i];
                    }
                    if (yp - 1 >= ((yp >> m_log2_ctb) << m_log2_ctb) && avail(xp, yp, xp, yp - 1)) {
                        const std::size_t i = idx4(xp, yp - 1);
                        if ((m_cu_flags[i] & (CU_INTRA | CU_PCM)) == CU_INTRA) cb = m_ipm[i];
                    }
                    int list[3];
                    if (ca == cb) {
                        if (ca < 2) {
                            list[0] = 0;
                            list[1] = 1;
                            list[2] = 26;
                        } else {
                            list[0] = ca;
                            list[1] = 2 + ((ca + 29) % 32);
                            list[2] = 2 + ((ca - 2 + 1) % 32);
                        }
                    } else {
                        list[0] = ca;
                        list[1] = cb;
                        if (ca != 0 && cb != 0) {
                            list[2] = 0;
                        } else if (ca != 1 && cb != 1) {
                            list[2] = 1;
                        } else {
                            list[2] = 26;
                        }
                    }
                    int mode;
                    if (mpm_idx >= 0) {
                        mode = list[mpm_idx];
                    } else {
                        if (list[0] > list[1]) std::swap(list[0], list[1]);
                        if (list[0] > list[2]) std::swap(list[0], list[2]);
                        if (list[1] > list[2]) std::swap(list[1], list[2]);
                        mode = rem;
                        for (int i = 0; i < 3; ++i) {
                            if (mode >= list[i]) ++mode;
                        }
                    }
                    m_luma_mode[k] = mode;
                    fill_map(m_ipm, xp, yp, pb, pb, static_cast<std::uint8_t>(mode));
                }
                if (nparts == 1) m_luma_mode[1] = m_luma_mode[2] = m_luma_mode[3] = m_luma_mode[0];
                const int nchroma = m_chroma == 3 ? nparts : (m_chroma != 0 ? 1 : 0);
                for (int k = 0; k < nchroma; ++k) {
                    int s = 4;
                    if (m_cabac.decode(m_ctx[CTX_CHROMA_PRED])) s = static_cast<int>(m_cabac.bypass_bits(2));
                    m_chroma_syntax[k] = s;
                }
                if (nchroma == 1) m_chroma_syntax[1] = m_chroma_syntax[2] = m_chroma_syntax[3] = m_chroma_syntax[0];
                for (int k = 0; k < 4; ++k) {
                    const int luma = m_luma_mode[m_chroma == 3 ? k : 0];
                    const int s = m_chroma_syntax[m_chroma == 3 ? k : 0];
                    static const int modes[4] = {0, 26, 10, 1};
                    int mode = s == 4 ? luma : modes[s];
                    if (s != 4 && mode == luma) mode = 34;
                    if (m_chroma == 2) mode = k_intra_422_map[mode];
                    m_chroma_mode[k] = mode;
                }
            }
        } else {
            fill_map(m_ipm, x0, y0, n, n, 1);
            const int h2 = n >> 1;
            const int q = n >> 2;
            switch (m_cu_part) {
                case PART_2Nx2N: prediction_unit(x0, y0, n, n, 0); break;
                case PART_2NxN:
                    prediction_unit(x0, y0, n, h2, 0);
                    prediction_unit(x0, y0 + h2, n, h2, 1);
                    break;
                case PART_Nx2N:
                    prediction_unit(x0, y0, h2, n, 0);
                    prediction_unit(x0 + h2, y0, h2, n, 1);
                    break;
                case PART_2NxnU:
                    prediction_unit(x0, y0, n, q, 0);
                    prediction_unit(x0, y0 + q, n, n - q, 1);
                    break;
                case PART_2NxnD:
                    prediction_unit(x0, y0, n, n - q, 0);
                    prediction_unit(x0, y0 + n - q, n, q, 1);
                    break;
                case PART_nLx2N:
                    prediction_unit(x0, y0, q, n, 0);
                    prediction_unit(x0 + q, y0, n - q, n, 1);
                    break;
                case PART_nRx2N:
                    prediction_unit(x0, y0, n - q, n, 0);
                    prediction_unit(x0 + n - q, y0, q, n, 1);
                    break;
                default:
                    prediction_unit(x0, y0, h2, h2, 0);
                    prediction_unit(x0 + h2, y0, h2, h2, 1);
                    prediction_unit(x0, y0 + h2, h2, h2, 2);
                    prediction_unit(x0 + h2, y0 + h2, h2, h2, 3);
                    break;
            }
        }
        if (!m_cu_pcm) {
            bool root = true;
            if (!m_cu_intra && !(m_cu_part == PART_2Nx2N && m_merge_flag)) root = m_cabac.decode(m_ctx[CTX_RQT_ROOT_CBF]) != 0;
            if (root) {
                const int max_depth = m_cu_intra ? m_sps->max_th_depth_intra + (m_cu_part == PART_NxN ? 1 : 0) : m_sps->max_th_depth_inter;
                transform_tree(x0, y0, x0, y0, log2, 0, 0, max_depth, m_cu_intra && m_cu_part == PART_NxN, 1, 1);
            }
        }
    }
    if (!m_sh->deblocking_disabled) {
        mark_tu_edges(x0, y0, n);
        if (!m_cu_intra) {
            const int h2 = n >> 1;
            const int q = n >> 2;
            switch (m_cu_part) {
                case PART_2NxN: mark_pu_edges(x0, y0 + h2, n, 0); break;
                case PART_Nx2N: mark_pu_edges(x0 + h2, y0, 0, n); break;
                case PART_2NxnU: mark_pu_edges(x0, y0 + q, n, 0); break;
                case PART_2NxnD: mark_pu_edges(x0, y0 + n - q, n, 0); break;
                case PART_nLx2N: mark_pu_edges(x0 + q, y0, 0, n); break;
                case PART_nRx2N: mark_pu_edges(x0 + n - q, y0, 0, n); break;
                case PART_NxN:
                    mark_pu_edges(x0, y0 + h2, n, 0);
                    mark_pu_edges(x0 + h2, y0, 0, n);
                    break;
                default: break;
            }
        }
    }
    const int xe = std::min(x0 + n, m_w);
    const int ye = std::min(y0 + n, m_h);
    for (int y = y0; y < ye; y += 4) {
        std::int8_t* row = m_qp.data() + static_cast<std::size_t>(y >> 2) * static_cast<std::size_t>(m_w4);
        for (int x = x0; x < xe; x += 4) row[x >> 2] = static_cast<std::int8_t>(m_qp_y);
    }
    m_last_qp = m_qp_y;
}

void PicDecoder::pcm_sample(int x0, int y0, int log2) {
    if (m_cabac.oracle()) {
        const int n = 1 << log2;
        std::uint16_t* p0 = m_pic->plane[0];
        const std::ptrdiff_t s0 = m_pic->stride[0];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) p0[(y0 + y) * s0 + x0 + x] = static_cast<std::uint16_t>(m_cabac.pcm_bits(m_sps->pcm_bit_depth_y) << (m_bd_y - m_sps->pcm_bit_depth_y));
        }
        if (m_chroma != 0) {
            for (int c = 1; c < 3; ++c) {
                std::uint16_t* pc = m_pic->plane[c];
                const std::ptrdiff_t s = m_pic->stride[c];
                for (int y = 0; y < (n >> m_sy); ++y) {
                    for (int x = 0; x < (n >> m_sx); ++x) pc[((y0 >> m_sy) + y) * s + (x0 >> m_sx) + x] = static_cast<std::uint16_t>(m_cabac.pcm_bits(m_sps->pcm_bit_depth_c) << (m_bd_c - m_sps->pcm_bit_depth_c));
                }
            }
        }
        m_cabac.init(nullptr, 0);
        return;
    }
    const std::size_t pos = m_cur_sub_start + m_cabac.aligned_position();
    if (pos >= m_cur_sub_end) {
        m_error = true;
        return;
    }
    Bits b(m_cur_data + pos, m_cur_sub_end - pos);
    const int n = 1 << log2;
    const int sy = m_bd_y - m_sps->pcm_bit_depth_y;
    std::uint16_t* p0 = m_pic->plane[0];
    const std::ptrdiff_t s0 = m_pic->stride[0];
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) p0[(y0 + y) * s0 + x0 + x] = static_cast<std::uint16_t>(b.u(m_sps->pcm_bit_depth_y) << sy);
    }
    if (m_chroma != 0) {
        const int sc = m_bd_c - m_sps->pcm_bit_depth_c;
        const int cw = n >> m_sx;
        const int ch = n >> m_sy;
        for (int c = 1; c < 3; ++c) {
            std::uint16_t* pc = m_pic->plane[c];
            const std::ptrdiff_t s = m_pic->stride[c];
            const int xc = x0 >> m_sx;
            const int yc = y0 >> m_sy;
            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) pc[(yc + y) * s + xc + x] = static_cast<std::uint16_t>(b.u(m_sps->pcm_bit_depth_c) << sc);
            }
        }
    }
    if (b.error()) {
        m_error = true;
        return;
    }
    const std::size_t used = (b.position() + 7) / 8;
    const std::size_t next = pos + used;
    if (next > m_cur_sub_end) {
        m_error = true;
        return;
    }
    m_cabac.init(m_cur_data + next, m_cur_sub_end - next);
    m_cur_sub_start = next;
}

int PicDecoder::intra_mode_luma_at(int x, int y) const noexcept {
    if (m_cu_part != PART_NxN) return m_luma_mode[0];
    const int h = 1 << (m_cu_log2 - 1);
    const int k = (x - m_cu_x >= h ? 1 : 0) + (y - m_cu_y >= h ? 2 : 0);
    return m_luma_mode[k];
}

int PicDecoder::chroma_mode_at(int x, int y) const noexcept {
    if (m_cu_part != PART_NxN || m_chroma != 3) return m_chroma_mode[0];
    const int h = 1 << (m_cu_log2 - 1);
    const int k = (x - m_cu_x >= h ? 1 : 0) + (y - m_cu_y >= h ? 2 : 0);
    return m_chroma_mode[k];
}

int PicDecoder::chroma_syntax_at(int x, int y) const noexcept {
    if (m_cu_part != PART_NxN || m_chroma != 3) return m_chroma_syntax[0];
    const int h = 1 << (m_cu_log2 - 1);
    const int k = (x - m_cu_x >= h ? 1 : 0) + (y - m_cu_y >= h ? 2 : 0);
    return m_chroma_syntax[k];
}

void PicDecoder::transform_tree(int x0, int y0, int xb, int yb, int log2, int depth, int blk, int max_depth, bool intra_split, int parent_cb, int parent_cr) {
    if (m_error) return;
    bool split;
    if (log2 <= m_sps->log2_max_tb && log2 > m_sps->log2_min_tb && depth < max_depth && !(intra_split && depth == 0)) {
        split = m_cabac.decode(m_ctx[CTX_SPLIT_TRANSFORM + 5 - log2]) != 0;
    } else {
        const bool inter_split = m_sps->max_th_depth_inter == 0 && !m_cu_intra && m_cu_part != PART_2Nx2N && depth == 0;
        split = log2 > m_sps->log2_max_tb || (intra_split && depth == 0) || inter_split;
    }
    int cbf_cb = 0;
    int cbf_cr = 0;
    if ((log2 > 2 && m_chroma != 0) || m_chroma == 3) {
        const bool two = m_chroma == 2 && (!split || log2 == 3);
        if (depth == 0 || parent_cb) {
            cbf_cb = m_cabac.decode(m_ctx[CTX_CBF_CHROMA + depth]);
            if (two) cbf_cb |= m_cabac.decode(m_ctx[CTX_CBF_CHROMA + depth]) << 1;
        }
        if (depth == 0 || parent_cr) {
            cbf_cr = m_cabac.decode(m_ctx[CTX_CBF_CHROMA + depth]);
            if (two) cbf_cr |= m_cabac.decode(m_ctx[CTX_CBF_CHROMA + depth]) << 1;
        }
    } else if (m_chroma != 0) {
        cbf_cb = parent_cb;
        cbf_cr = parent_cr;
    }
    if (split) {
        const int h = 1 << (log2 - 1);
        transform_tree(x0, y0, x0, y0, log2 - 1, depth + 1, 0, max_depth, intra_split, cbf_cb, cbf_cr);
        transform_tree(x0 + h, y0, x0, y0, log2 - 1, depth + 1, 1, max_depth, intra_split, cbf_cb, cbf_cr);
        transform_tree(x0, y0 + h, x0, y0, log2 - 1, depth + 1, 2, max_depth, intra_split, cbf_cb, cbf_cr);
        transform_tree(x0 + h, y0 + h, x0, y0, log2 - 1, depth + 1, 3, max_depth, intra_split, cbf_cb, cbf_cr);
    } else {
        int cbf_luma = 1;
        if (m_cu_intra || depth != 0 || cbf_cb || cbf_cr) cbf_luma = m_cabac.decode(m_ctx[CTX_CBF_LUMA + (depth == 0 ? 1 : 0)]);
        transform_unit(x0, y0, xb, yb, log2, depth, blk, cbf_luma, cbf_cb, cbf_cr);
    }
}

void PicDecoder::decode_qp_delta() {
    int v = 0;
    while (v < 5 && m_cabac.decode(m_ctx[CTX_QP_DELTA + (v ? 1 : 0)])) ++v;
    if (v == 5) {
        int k = 0;
        int add = 0;
        while (m_cabac.bypass()) {
            add += 1 << k;
            if (++k > 16) {
                m_error = true;
                return;
            }
        }
        add += static_cast<int>(m_cabac.bypass_bits(k));
        v += add;
    }
    if (v && m_cabac.bypass()) v = -v;
    const int lim = 26 + m_sps->qp_bd_offset_y / 2;
    if (v < -lim || v > lim - 1) {
        m_error = true;
        return;
    }
    m_cu_qp_delta = v;
    m_qp_delta_coded = true;
    update_qp();
}

void PicDecoder::decode_chroma_qp_offset() {
    const int flag = m_cabac.decode(m_ctx[CTX_CHROMA_QP_OFFSET_FLAG]);
    int idx = 0;
    if (flag && m_pps->chroma_qp_offset_list_len > 1) {
        while (idx < m_pps->chroma_qp_offset_list_len - 1 && m_cabac.decode(m_ctx[CTX_CHROMA_QP_OFFSET_IDX])) ++idx;
    }
    m_cu_qp_offset_cb = flag ? m_pps->cb_qp_offset_list[idx] : 0;
    m_cu_qp_offset_cr = flag ? m_pps->cr_qp_offset_list[idx] : 0;
    m_chroma_qp_offset_coded = true;
}

void PicDecoder::transform_unit(int x0, int y0, int xb, int yb, int log2, int depth, int blk, int cbf_luma, int cbf_cb, int cbf_cr) {
    (void)depth;
    m_tu_x = x0;
    m_tu_y = y0;
    const int log2c = std::max(2, log2 - (m_chroma == 3 ? 0 : 1));
    const bool chroma_here = m_chroma != 0 && (log2 > 2 || m_chroma == 3);
    const bool chroma_parent = m_chroma != 0 && !chroma_here && blk == 3;
    const bool any_chroma = (cbf_cb | cbf_cr) != 0;
    if (!m_sh->deblocking_disabled && log2 < m_cu_log2) mark_tu_edges(x0, y0, 1 << log2);
    if (cbf_luma) fill_map(m_nz, x0, y0, 1 << log2, 1 << log2, 1);
    if (cbf_luma || any_chroma) {
        if (m_pps->cu_qp_delta && !m_qp_delta_coded) {
            decode_qp_delta();
            if (m_error) return;
        }
        if (any_chroma && !m_cu_bypass && m_sh->cu_chroma_qp_offset_enabled && !m_chroma_qp_offset_coded) decode_chroma_qp_offset();
    }
    const int lmode = m_cu_intra ? intra_mode_luma_at(x0, y0) : 0;
    if (m_cu_intra) intra_predict(0, x0, y0, log2, lmode);
    const int nl = 1 << log2;
    if (cbf_luma) {
        residual_coding(x0, y0, log2, 0, m_res_y);
        if (m_error) return;
        reconstruct(0, x0, y0, log2, m_res_y);
    }
    if (chroma_here) {
        const int xc = x0 >> m_sx;
        const int yc = y0 >> m_sy;
        const int cmode = m_cu_intra ? chroma_mode_at(x0, y0) : 0;
        const bool cross = m_pps->cross_component_prediction && cbf_luma && (!m_cu_intra || chroma_syntax_at(x0, y0) == 4);
        const int nt = m_chroma == 2 ? 2 : 1;
        for (int c = 1; c < 3; ++c) {
            int res_scale = 0;
            if (cross) {
                const int k = c - 1;
                int v = 0;
                while (v < 4 && m_cabac.decode(m_ctx[CTX_LOG2_RES_SCALE + 4 * k + v])) ++v;
                if (v) {
                    const int sign = m_cabac.decode(m_ctx[CTX_RES_SCALE_SIGN + k]);
                    res_scale = (1 << (v - 1)) * (1 - 2 * sign);
                }
            }
            const int cbf = c == 1 ? cbf_cb : cbf_cr;
            for (int t = 0; t < nt; ++t) {
                const int yo = yc + (t << log2c);
                if (m_cu_intra) intra_predict(c, xc, yo, log2c, cmode);
                const bool coded = ((cbf >> t) & 1) != 0;
                if (coded) {
                    residual_coding(xc, yo, log2c, c, m_res_c);
                    if (m_error) return;
                }
                if (res_scale) {
                    if (!coded) std::memset(m_res_c, 0, sizeof(std::int32_t) * static_cast<std::size_t>(nl * nl));
                    cross_component(log2c, res_scale, m_res_c, m_res_y);
                }
                if (coded || res_scale) reconstruct(c, xc, yo, log2c, m_res_c);
            }
        }
    } else if (chroma_parent) {
        const int xc = xb >> m_sx;
        const int yc = yb >> m_sy;
        const int cmode = m_cu_intra ? chroma_mode_at(xb, yb) : 0;
        const int nt = m_chroma == 2 ? 2 : 1;
        for (int c = 1; c < 3; ++c) {
            const int cbf = c == 1 ? cbf_cb : cbf_cr;
            for (int t = 0; t < nt; ++t) {
                const int yo = yc + (t << 2);
                if (m_cu_intra) intra_predict(c, xc, yo, 2, cmode);
                if ((cbf >> t) & 1) {
                    residual_coding(xc, yo, 2, c, m_res_c);
                    if (m_error) return;
                    reconstruct(c, xc, yo, 2, m_res_c);
                }
            }
        }
    }
}

void PicDecoder::residual_coding(int x0, int y0, int log2, int cidx, std::int32_t* out) {
    (void)x0;
    (void)y0;
    const int n = 1 << log2;
    Cabac& cb = m_cabac;
    std::uint8_t* ctx = m_ctx;
    bool ts = false;
    if (m_pps->transform_skip && !m_cu_bypass && log2 <= m_pps->log2_max_transform_skip_size) ts = cb.decode(ctx[CTX_TRANSFORM_SKIP + (cidx ? 1 : 0)]) != 0;
    bool explicit_rdpcm = false;
    int rdpcm_dir = 0;
    if (!m_cu_intra && m_sps->explicit_rdpcm && (ts || m_cu_bypass)) {
        explicit_rdpcm = cb.decode(ctx[CTX_RDPCM + (cidx ? 1 : 0)]) != 0;
        if (explicit_rdpcm) rdpcm_dir = cb.decode(ctx[CTX_RDPCM_DIR + (cidx ? 1 : 0)]);
    }
    int ctx_off;
    int ctx_shift;
    if (cidx == 0) {
        ctx_off = 3 * (log2 - 2) + ((log2 - 1) >> 2);
        ctx_shift = (log2 + 1) >> 2;
    } else {
        ctx_off = 15;
        ctx_shift = log2 - 2;
    }
    const int cmax = (log2 << 1) - 1;
    int px = 0;
    while (px < cmax && cb.decode(ctx[CTX_LAST_X + ctx_off + (px >> ctx_shift)])) ++px;
    int py = 0;
    while (py < cmax && cb.decode(ctx[CTX_LAST_Y + ctx_off + (py >> ctx_shift)])) ++py;
    int lx = px;
    int ly = py;
    if (px > 3) {
        const int nb = (px >> 1) - 1;
        lx = (1 << nb) * (2 + (px & 1)) + static_cast<int>(cb.bypass_bits(nb));
    }
    if (py > 3) {
        const int nb = (py >> 1) - 1;
        ly = (1 << nb) * (2 + (py & 1)) + static_cast<int>(cb.bypass_bits(nb));
    }
    int mode = 0;
    if (m_cu_intra) mode = cidx == 0 ? intra_mode_luma_at(m_tu_x, m_tu_y) : chroma_mode_at(m_tu_x, m_tu_y);
    int scan = 0;
    if (m_cu_intra && (log2 == 2 || (log2 == 3 && cidx == 0) || (log2 == 3 && m_chroma == 3))) {
        if (mode >= 6 && mode <= 14) {
            scan = 2;
        } else if (mode >= 22 && mode <= 30) {
            scan = 1;
        }
    }
    if (scan == 2) std::swap(lx, ly);
    if (lx >= n || ly >= n) {
        m_error = true;
        return;
    }
    const ScanTables& st = scan_tables();
    const int log2sb = log2 - 2;
    const int nsb = 1 << log2sb;
    const std::uint8_t (*sbpos)[2] = st.pos[log2sb][scan];
    const std::uint8_t (*cpos)[2] = st.pos[2][scan];
    int last_sb = (1 << (2 * log2sb)) - 1;
    {
        const int xs = lx >> 2;
        const int ys = ly >> 2;
        while (last_sb > 0 && (sbpos[last_sb][0] != xs || sbpos[last_sb][1] != ys)) --last_sb;
    }
    int last_pos = 15;
    {
        const int xp = lx & 3;
        const int yp = ly & 3;
        while (last_pos > 0 && (cpos[last_pos][0] != xp || cpos[last_pos][1] != yp)) --last_pos;
    }
    std::int32_t* coeff = m_coeff;
    std::memset(coeff, 0, sizeof(std::int32_t) * static_cast<std::size_t>(n * n));
    std::uint8_t csbf[8][8];
    std::memset(csbf, 0, sizeof(csbf));
    const bool bypass_like = ts || m_cu_bypass;
    const int sb_type = (cidx == 0 ? 2 : 0) + (bypass_like ? 1 : 0);
    bool sdh = m_pps->sign_data_hiding;
    if (m_cu_bypass || (m_cu_intra && m_sps->implicit_rdpcm && ts && (mode == 10 || mode == 26)) || explicit_rdpcm) sdh = false;
    const bool ts_ctx = m_sps->transform_skip_context && bypass_like;
    int prev_c1 = 1;
    bool g1_before = false;
    int max_x = 0;
    int max_y = 0;
    std::uint8_t* sig_ctx_base = ctx + CTX_SIG + (cidx ? 27 : 0);
    std::uint8_t* gt1_base = ctx + CTX_GT1 + (cidx ? 16 : 0);
    std::uint8_t* gt2_base = ctx + CTX_GT2 + (cidx ? 4 : 0);
    for (int i = last_sb; i >= 0; --i) {
        const int xs = sbpos[i][0];
        const int ys = sbpos[i][1];
        bool infer_dc = false;
        int flag;
        if (i < last_sb && i > 0) {
            int c = 0;
            if (xs < nsb - 1) c += csbf[xs + 1][ys];
            if (ys < nsb - 1) c += csbf[xs][ys + 1];
            flag = cb.decode(ctx[CTX_CSBF + (cidx ? 2 : 0) + (c > 0 ? 1 : 0)]);
            infer_dc = true;
        } else {
            flag = 1;
        }
        csbf[xs][ys] = static_cast<std::uint8_t>(flag);
        int sig[16];
        int nsig = 0;
        int start = 15;
        if (i == last_sb) {
            sig[nsig++] = last_pos;
            start = last_pos - 1;
        }
        if (flag) {
            int sctx[16];
            if (ts_ctx) {
                for (int k = 0; k < 16; ++k) sctx[k] = cidx == 0 ? 42 : 16;
            } else if (log2 == 2) {
                for (int k = 0; k < 16; ++k) sctx[k] = k_sig_ctx_4x4[(cpos[k][1] << 2) + cpos[k][0]];
            } else {
                int prev = 0;
                if (xs < nsb - 1) prev += csbf[xs + 1][ys];
                if (ys < nsb - 1) prev += csbf[xs][ys + 1] << 1;
                const int add = (cidx == 0 && (xs > 0 || ys > 0) ? 3 : 0) + (log2 == 3 ? ((scan == 0 || cidx > 0) ? 9 : 15) : (cidx == 0 ? 21 : 12));
                for (int k = 0; k < 16; ++k) {
                    const int xp = cpos[k][0];
                    const int yp = cpos[k][1];
                    int s;
                    if (prev == 0) {
                        s = (xp + yp == 0) ? 2 : (xp + yp < 3) ? 1 : 0;
                    } else if (prev == 1) {
                        s = yp == 0 ? 2 : (yp == 1 ? 1 : 0);
                    } else if (prev == 2) {
                        s = xp == 0 ? 2 : (xp == 1 ? 1 : 0);
                    } else {
                        s = 2;
                    }
                    sctx[k] = s + add;
                }
                if (xs == 0 && ys == 0) sctx[0] = 0;
            }
            for (int k = start; k >= 0; --k) {
                if (k > 0 || !infer_dc) {
                    if (cb.decode(sig_ctx_base[sctx[k]])) {
                        sig[nsig++] = k;
                        infer_dc = false;
                    }
                } else {
                    sig[nsig++] = 0;
                }
            }
        }
        if (nsig == 0) continue;
        int ctx_set = (i == 0 || cidx > 0) ? 0 : 2;
        if (g1_before && prev_c1 == 0) ++ctx_set;
        g1_before = true;
        int c1 = 1;
        int base[16];
        int first_g1 = -1;
        bool escape = false;
        for (int k = 0; k < nsig; ++k) {
            if (k < 8) {
                const int f = cb.decode(gt1_base[ctx_set * 4 + (c1 < 3 ? c1 : 3)]);
                base[k] = 1 + f;
                if (f) {
                    c1 = 0;
                    if (first_g1 < 0) {
                        first_g1 = k;
                    } else {
                        escape = true;
                    }
                } else if (c1 > 0) {
                    ++c1;
                }
            } else {
                base[k] = 1;
                escape = true;
            }
        }
        prev_c1 = c1;
        if (first_g1 >= 0) {
            const int f = cb.decode(gt2_base[ctx_set]);
            base[first_g1] += f;
            if (f) escape = true;
        }
        const bool hidden = sdh && (sig[0] - sig[nsig - 1] > 3);
        if (m_sps->cabac_bypass_alignment && escape) cb.align_bypass();
        const int nsigns = hidden ? nsig - 1 : nsig;
        std::uint32_t signs = nsigns > 0 ? cb.bypass_bits(nsigns) << (32 - nsigns) : 0;
        int rice = m_sps->persistent_rice ? m_stat[sb_type] / 4 : 0;
        bool first_rem = true;
        int sum_abs = 0;
        for (int k = 0; k < nsig; ++k) {
            int lvl = base[k];
            const int thresh = k < 8 ? (k == first_g1 ? 3 : 2) : 1;
            if (lvl == thresh) {
                int prefix = 0;
                while (prefix < 32 && cb.bypass()) ++prefix;
                std::int64_t rem;
                if (prefix <= 3) {
                    rem = (static_cast<std::int64_t>(prefix) << rice) + cb.bypass_bits(rice);
                } else {
                    const int len = prefix - 3 + rice;
                    if (len > 31) {
                        m_error = true;
                        return;
                    }
                    rem = ((((static_cast<std::int64_t>(1) << (prefix - 3)) + 3 - 1)) << rice) + cb.bypass_bits(len);
                }
                if (rem > 65536) rem = 65536;
                if (m_sps->persistent_rice && first_rem) {
                    int& s = m_stat[sb_type];
                    if (rem >= (static_cast<std::int64_t>(3) << (s / 4))) {
                        ++s;
                    } else if (2 * rem < (static_cast<std::int64_t>(1) << (s / 4)) && s > 0) {
                        --s;
                    }
                }
                first_rem = false;
                lvl += static_cast<int>(rem);
                if (lvl > 3 * (1 << rice)) rice = rice + 1 < 4 ? rice + 1 : 4;
            }
            const int p = sig[k];
            const int xc = (xs << 2) + cpos[p][0];
            const int yc = (ys << 2) + cpos[p][1];
            int v = lvl;
            if (!(hidden && k == nsig - 1)) {
                if (signs & 0x80000000u) v = -v;
                signs <<= 1;
            }
            if (hidden) {
                sum_abs += lvl;
                if (k == nsig - 1 && (sum_abs & 1)) v = -v;
            }
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            coeff[yc * n + xc] = v;
            if (xc > max_x) max_x = xc;
            if (yc > max_y) max_y = yc;
        }
    }
    m_ts_flag = ts;
    m_rdpcm_explicit = explicit_rdpcm;
    m_rdpcm_dir = rdpcm_dir;
    transform_residual(cidx, log2, ts, m_cu_intra, mode, coeff, max_x, max_y, out);
}

} // namespace hevc_internal
} // namespace video
} // namespace fizmo
