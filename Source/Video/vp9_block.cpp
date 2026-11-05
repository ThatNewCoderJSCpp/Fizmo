#include "vp9_internal.hpp"

namespace fizmo {
namespace video {
namespace vp9_detail {

struct TileDecoder::Block {
    int mi_row = 0;
    int mi_col = 0;
    int bsize = 0;
    int bw8 = 1;
    int bh8 = 1;
    int x_mis = 1;
    int y_mis = 1;
    bool avail_u = false;
    bool avail_l = false;
    const ModeInfo* above = nullptr;
    const ModeInfo* left = nullptr;
    int mb_to_left_edge = 0;
    int mb_to_right_edge = 0;
    int mb_to_top_edge = 0;
    int mb_to_bottom_edge = 0;
};

namespace {

inline bool is_inter_block(const ModeInfo* m) noexcept { return m->ref_frame[0] > kIntraFrame; }
inline bool has_second_ref(const ModeInfo* m) noexcept { return m->ref_frame[1] > kIntraFrame; }

int reference_mode_context(const FrameHeader& h, const ModeInfo* above, const ModeInfo* left) noexcept {
    int ctx;
    if (above && left) {
        if (!has_second_ref(above) && !has_second_ref(left)) {
            ctx = (above->ref_frame[0] == h.comp_fixed_ref) ^ (left->ref_frame[0] == h.comp_fixed_ref);
        } else if (!has_second_ref(above)) {
            ctx = 2 + (above->ref_frame[0] == h.comp_fixed_ref || !is_inter_block(above));
        } else if (!has_second_ref(left)) {
            ctx = 2 + (left->ref_frame[0] == h.comp_fixed_ref || !is_inter_block(left));
        } else {
            ctx = 4;
        }
    } else if (above || left) {
        const ModeInfo* edge = above ? above : left;
        if (!has_second_ref(edge)) ctx = edge->ref_frame[0] == h.comp_fixed_ref;
        else ctx = 3;
    } else {
        ctx = 1;
    }
    return ctx;
}

int comp_ref_context(const FrameHeader& h, const ModeInfo* above, const ModeInfo* left) noexcept {
    int ctx;
    const int fix_ref_idx = h.ref_frame_sign_bias[h.comp_fixed_ref];
    const int var_ref_idx = !fix_ref_idx;
    if (above && left) {
        const bool above_intra = !is_inter_block(above);
        const bool left_intra = !is_inter_block(left);
        if (above_intra && left_intra) {
            ctx = 2;
        } else if (above_intra || left_intra) {
            const ModeInfo* edge = above_intra ? left : above;
            if (!has_second_ref(edge)) ctx = 1 + 2 * (edge->ref_frame[0] != h.comp_var_ref[1]);
            else ctx = 1 + 2 * (edge->ref_frame[var_ref_idx] != h.comp_var_ref[1]);
        } else {
            const bool l_sg = !has_second_ref(left);
            const bool a_sg = !has_second_ref(above);
            const int vrfa = a_sg ? above->ref_frame[0] : above->ref_frame[var_ref_idx];
            const int vrfl = l_sg ? left->ref_frame[0] : left->ref_frame[var_ref_idx];
            if (vrfa == vrfl && h.comp_var_ref[1] == vrfa) {
                ctx = 0;
            } else if (l_sg && a_sg) {
                if ((vrfa == h.comp_fixed_ref && vrfl == h.comp_var_ref[0]) || (vrfl == h.comp_fixed_ref && vrfa == h.comp_var_ref[0])) ctx = 4;
                else if (vrfa == vrfl) ctx = 3;
                else ctx = 1;
            } else if (l_sg || a_sg) {
                const int vrfc = l_sg ? vrfa : vrfl;
                const int rfs = a_sg ? vrfa : vrfl;
                if (vrfc == h.comp_var_ref[1] && rfs != h.comp_var_ref[1]) ctx = 1;
                else if (rfs == h.comp_var_ref[1] && vrfc != h.comp_var_ref[1]) ctx = 2;
                else ctx = 4;
            } else if (vrfa == vrfl) {
                ctx = 4;
            } else {
                ctx = 2;
            }
        }
    } else if (above || left) {
        const ModeInfo* edge = above ? above : left;
        if (!is_inter_block(edge)) {
            ctx = 2;
        } else if (has_second_ref(edge)) {
            ctx = 4 * (edge->ref_frame[var_ref_idx] != h.comp_var_ref[1]);
        } else {
            ctx = 3 * (edge->ref_frame[0] != h.comp_var_ref[1]);
        }
    } else {
        ctx = 2;
    }
    return ctx;
}

int single_ref_p1_context(const ModeInfo* above, const ModeInfo* left) noexcept {
    int ctx;
    if (above && left) {
        const bool above_intra = !is_inter_block(above);
        const bool left_intra = !is_inter_block(left);
        if (above_intra && left_intra) {
            ctx = 2;
        } else if (above_intra || left_intra) {
            const ModeInfo* edge = above_intra ? left : above;
            if (!has_second_ref(edge)) ctx = 4 * (edge->ref_frame[0] == kLastFrame);
            else ctx = 1 + (edge->ref_frame[0] == kLastFrame || edge->ref_frame[1] == kLastFrame);
        } else {
            const bool above_two = has_second_ref(above);
            const bool left_two = has_second_ref(left);
            const int above0 = above->ref_frame[0];
            const int above1 = above->ref_frame[1];
            const int left0 = left->ref_frame[0];
            const int left1 = left->ref_frame[1];
            if (above_two && left_two) {
                ctx = 1 + (above0 == kLastFrame || above1 == kLastFrame || left0 == kLastFrame || left1 == kLastFrame);
            } else if (above_two || left_two) {
                const int rfs = !above_two ? above0 : left0;
                const int crf1 = above_two ? above0 : left0;
                const int crf2 = above_two ? above1 : left1;
                if (rfs == kLastFrame) ctx = 3 + (crf1 == kLastFrame || crf2 == kLastFrame);
                else ctx = (crf1 == kLastFrame || crf2 == kLastFrame);
            } else {
                ctx = 2 * (above0 == kLastFrame) + 2 * (left0 == kLastFrame);
            }
        }
    } else if (above || left) {
        const ModeInfo* edge = above ? above : left;
        if (!is_inter_block(edge)) {
            ctx = 2;
        } else if (!has_second_ref(edge)) {
            ctx = 4 * (edge->ref_frame[0] == kLastFrame);
        } else {
            ctx = 1 + (edge->ref_frame[0] == kLastFrame || edge->ref_frame[1] == kLastFrame);
        }
    } else {
        ctx = 2;
    }
    return ctx;
}

int single_ref_p2_context(const ModeInfo* above, const ModeInfo* left) noexcept {
    int ctx;
    if (above && left) {
        const bool above_intra = !is_inter_block(above);
        const bool left_intra = !is_inter_block(left);
        if (above_intra && left_intra) {
            ctx = 2;
        } else if (above_intra || left_intra) {
            const ModeInfo* edge = above_intra ? left : above;
            if (!has_second_ref(edge)) {
                if (edge->ref_frame[0] == kLastFrame) ctx = 3;
                else ctx = 4 * (edge->ref_frame[0] == kGoldenFrame);
            } else {
                ctx = 1 + 2 * (edge->ref_frame[0] == kGoldenFrame || edge->ref_frame[1] == kGoldenFrame);
            }
        } else {
            const bool above_two = has_second_ref(above);
            const bool left_two = has_second_ref(left);
            const int above0 = above->ref_frame[0];
            const int above1 = above->ref_frame[1];
            const int left0 = left->ref_frame[0];
            const int left1 = left->ref_frame[1];
            if (above_two && left_two) {
                if (above0 == left0 && above1 == left1) {
                    ctx = 3 * (above0 == kGoldenFrame || above1 == kGoldenFrame || left0 == kGoldenFrame || left1 == kGoldenFrame);
                } else {
                    ctx = 2;
                }
            } else if (above_two || left_two) {
                const int rfs = !above_two ? above0 : left0;
                const int crf1 = above_two ? above0 : left0;
                const int crf2 = above_two ? above1 : left1;
                if (rfs == kGoldenFrame) ctx = 3 + (crf1 == kGoldenFrame || crf2 == kGoldenFrame);
                else if (rfs == kAltrefFrame) ctx = crf1 == kGoldenFrame || crf2 == kGoldenFrame;
                else ctx = 1 + 2 * (crf1 == kGoldenFrame || crf2 == kGoldenFrame);
            } else {
                if (above0 == kLastFrame && left0 == kLastFrame) {
                    ctx = 3;
                } else if (above0 == kLastFrame || left0 == kLastFrame) {
                    const int edge0 = (above0 == kLastFrame) ? left0 : above0;
                    ctx = 4 * (edge0 == kGoldenFrame);
                } else {
                    ctx = 2 * (above0 == kGoldenFrame) + 2 * (left0 == kGoldenFrame);
                }
            }
        }
    } else if (above || left) {
        const ModeInfo* edge = above ? above : left;
        if (!is_inter_block(edge) || (edge->ref_frame[0] == kLastFrame && !has_second_ref(edge))) {
            ctx = 2;
        } else if (!has_second_ref(edge)) {
            ctx = 4 * (edge->ref_frame[0] == kGoldenFrame);
        } else {
            ctx = 3 * (edge->ref_frame[0] == kGoldenFrame || edge->ref_frame[1] == kGoldenFrame);
        }
    } else {
        ctx = 2;
    }
    return ctx;
}

inline bool use_mv_hp(const Mv& mv) noexcept {
    const int r = mv.row < 0 ? -mv.row : mv.row;
    const int c = mv.col < 0 ? -mv.col : mv.col;
    return (r >> 3) < kCompandedMvrefThresh && (c >> 3) < kCompandedMvrefThresh;
}

inline void lower_mv_precision(Mv& mv, bool allow_hp) noexcept {
    if (!(allow_hp && use_mv_hp(mv))) {
        if (mv.row & 1) mv.row = static_cast<std::int16_t>(mv.row + (mv.row > 0 ? -1 : 1));
        if (mv.col & 1) mv.col = static_cast<std::int16_t>(mv.col + (mv.col > 0 ? -1 : 1));
    }
}

inline void clamp_mv(Mv& mv, int min_col, int max_col, int min_row, int max_row) noexcept {
    mv.col = static_cast<std::int16_t>(iclamp(mv.col, min_col, max_col));
    mv.row = static_cast<std::int16_t>(iclamp(mv.row, min_row, max_row));
}

inline int round_q2(int v) noexcept { return (v < 0 ? v - 1 : v + 1) / 2; }
inline int round_q4(int v) noexcept { return (v < 0 ? v - 2 : v + 2) / 4; }

inline int uv_tx_size(int bsize, int tx_size, int ssx, int ssy) noexcept {
    if (bsize < kBlock8x8) return kTx4x4;
    const int w = (8 * k_num_8x8_w[bsize]) >> ssx;
    const int h = (8 * k_num_8x8_h[bsize]) >> ssy;
    const int m = imin(w, h);
    const int max_tx = m >= 32 ? kTx32x32 : (m >= 16 ? kTx16x16 : (m >= 8 ? kTx8x8 : kTx4x4));
    return imin(tx_size, max_tx);
}

} // namespace

TileDecoder::TileDecoder() : m_coef(32 * 32, 0), m_pred(64 * 64), m_mc_buf(160 * 160), m_mc_tmp(64 * 160) {
    std::memset(m_left_nonzero, 0, sizeof(m_left_nonzero));
    std::memset(m_left_partition, 0, sizeof(m_left_partition));
    std::memset(m_token_cache, 0, sizeof(m_token_cache));
    counts.clear();
}

bool TileDecoder::decode(FrameDecodeContext& ctx, const std::uint8_t* data, std::size_t size, int mi_row_start, int mi_row_end, int mi_col_start, int mi_col_end) {
    m_ctx = &ctx;
    m_mi_col_start = mi_col_start;
    m_mi_col_end = mi_col_end;
    if (!m_bd.init(data, size)) {
        corrupted = true;
        return false;
    }
    for (int r = mi_row_start; r < mi_row_end; r += 8) {
        std::memset(m_left_nonzero, 0, sizeof(m_left_nonzero));
        std::memset(m_left_partition, 0, sizeof(m_left_partition));
        for (int c = mi_col_start; c < mi_col_end; c += 8) decode_partition(r, c, kBlock64x64);
    }
    return !corrupted;
}

void TileDecoder::decode_partition(int mi_row, int mi_col, int bsize) {
    FrameDecodeContext& f = *m_ctx;
    if (mi_row >= f.mi_rows || mi_col >= f.mi_cols) return;
    const int n8 = k_num_8x8_w[bsize];
    const int half = n8 >> 1;
    const bool has_rows = (mi_row + half) < f.mi_rows;
    const bool has_cols = (mi_col + half) < f.mi_cols;
    const int bsl = k_mi_width_log2[bsize];
    const int above = (f.above_partition[mi_col] >> bsl) & 1;
    const int left = (m_left_partition[mi_row & 7] >> bsl) & 1;
    const int ctx = bsl * 4 + left * 2 + above;
    const std::uint8_t* probs = f.hdr->frame_is_intra ? k_kf_partition_probs[ctx] : f.fc->partition[ctx];
    int p;
    if (has_rows && has_cols) p = m_bd.read_tree(k_partition_tree, probs);
    else if (has_cols) p = m_bd.read(probs[1]) ? kPartitionSplit : kPartitionHorz;
    else if (has_rows) p = m_bd.read(probs[2]) ? kPartitionSplit : kPartitionVert;
    else p = kPartitionSplit;
    if (f.counting) ++counts.partition[ctx][p];
    const int subsize = k_subsize[p][bsize];
    if (subsize < kBlock8x8 || p == kPartitionNone) {
        decode_block(mi_row, mi_col, subsize);
    } else if (p == kPartitionHorz) {
        decode_block(mi_row, mi_col, subsize);
        if (has_rows) decode_block(mi_row + half, mi_col, subsize);
    } else if (p == kPartitionVert) {
        decode_block(mi_row, mi_col, subsize);
        if (has_cols) decode_block(mi_row, mi_col + half, subsize);
    } else {
        decode_partition(mi_row, mi_col, subsize);
        decode_partition(mi_row, mi_col + half, subsize);
        decode_partition(mi_row + half, mi_col, subsize);
        decode_partition(mi_row + half, mi_col + half, subsize);
    }
    if (bsize == kBlock8x8 || p != kPartitionSplit) {
        std::memset(f.above_partition + mi_col, k_partition_ctx_above[subsize], static_cast<std::size_t>(n8));
        std::memset(m_left_partition + (mi_row & 7), k_partition_ctx_left[subsize], static_cast<std::size_t>(n8));
    }
}

int TileDecoder::read_intra_segment_id(Block& b) {
    FrameDecodeContext& f = *m_ctx;
    const SegmentationParams& seg = *f.seg;
    if (!seg.enabled) return 0;
    if (!seg.update_map) {
        for (int y = 0; y < b.y_mis; ++y) {
            const std::size_t o = static_cast<std::size_t>(b.mi_row + y) * f.mi_cols + b.mi_col;
            std::memcpy(f.cur_seg + o, f.prev_seg + o, static_cast<std::size_t>(b.x_mis));
        }
        return 0;
    }
    const int id = m_bd.read_tree(k_segment_tree, seg.tree_probs);
    for (int y = 0; y < b.y_mis; ++y) {
        const std::size_t o = static_cast<std::size_t>(b.mi_row + y) * f.mi_cols + b.mi_col;
        std::memset(f.cur_seg + o, id, static_cast<std::size_t>(b.x_mis));
    }
    return id;
}

int TileDecoder::read_inter_segment_id(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    const SegmentationParams& seg = *f.seg;
    if (!seg.enabled) return 0;
    int predicted = kMaxSegments - 1;
    for (int y = 0; y < b.y_mis; ++y) {
        const std::size_t o = static_cast<std::size_t>(b.mi_row + y) * f.mi_cols + b.mi_col;
        for (int x = 0; x < b.x_mis; ++x) predicted = imin(predicted, f.prev_seg[o + x]);
    }
    if (!seg.update_map) {
        for (int y = 0; y < b.y_mis; ++y) {
            const std::size_t o = static_cast<std::size_t>(b.mi_row + y) * f.mi_cols + b.mi_col;
            std::memcpy(f.cur_seg + o, f.prev_seg + o, static_cast<std::size_t>(b.x_mis));
        }
        return predicted;
    }
    int id;
    if (seg.temporal_update) {
        const int ctx = (b.above ? b.above->seg_id_predicted : 0) + (b.left ? b.left->seg_id_predicted : 0);
        mi.seg_id_predicted = static_cast<std::uint8_t>(m_bd.read(seg.pred_probs[ctx]));
        id = mi.seg_id_predicted ? predicted : m_bd.read_tree(k_segment_tree, seg.tree_probs);
    } else {
        id = m_bd.read_tree(k_segment_tree, seg.tree_probs);
    }
    for (int y = 0; y < b.y_mis; ++y) {
        const std::size_t o = static_cast<std::size_t>(b.mi_row + y) * f.mi_cols + b.mi_col;
        std::memset(f.cur_seg + o, id, static_cast<std::size_t>(b.x_mis));
    }
    return id;
}

int TileDecoder::read_skip(Block& b, int segment_id) {
    FrameDecodeContext& f = *m_ctx;
    if (f.seg->active(segment_id, kSegLvlSkip)) return 1;
    const int ctx = (b.above ? b.above->skip : 0) + (b.left ? b.left->skip : 0);
    const int skip = m_bd.read(f.fc->skip[ctx]);
    if (f.counting) ++counts.skip[ctx][skip];
    return skip;
}

int TileDecoder::read_tx_size(Block& b, bool allow_select) {
    FrameDecodeContext& f = *m_ctx;
    const int max_tx = k_max_tx_size[b.bsize];
    const int tx_mode = f.hdr->tx_mode;
    if (allow_select && tx_mode == kTxModeSelect && b.bsize >= kBlock8x8) {
        int above = (b.above && !b.above->skip) ? b.above->tx_size : max_tx;
        int left = (b.left && !b.left->skip) ? b.left->tx_size : max_tx;
        if (!b.left) left = above;
        if (!b.above) above = left;
        const int ctx = (above + left) > max_tx;
        const std::uint8_t* probs = max_tx == kTx8x8 ? f.fc->tx8[ctx] : (max_tx == kTx16x16 ? f.fc->tx16[ctx] : f.fc->tx32[ctx]);
        int tx = m_bd.read(probs[0]);
        if (tx && max_tx > kTx8x8) {
            tx += m_bd.read(probs[1]);
            if (tx > 1 && max_tx > kTx16x16) tx += m_bd.read(probs[2]);
        }
        if (f.counting) {
            if (max_tx == kTx8x8) ++counts.tx8[ctx][tx];
            else if (max_tx == kTx16x16) ++counts.tx16[ctx][tx];
            else ++counts.tx32[ctx][tx];
        }
        return tx;
    }
    return imin(max_tx, k_tx_mode_to_biggest[tx_mode]);
}

void TileDecoder::intra_frame_mode_info(Block& b, ModeInfo& mi) {
    mi.segment_id = static_cast<std::uint8_t>(read_intra_segment_id(b));
    mi.skip = static_cast<std::uint8_t>(read_skip(b, mi.segment_id));
    mi.tx_size = static_cast<std::uint8_t>(read_tx_size(b, true));
    mi.ref_frame[0] = kIntraFrame;
    mi.ref_frame[1] = kNoneFrame;
    mi.is_inter = 0;
    mi.interp_filter = 3;
    auto above_mode = [&](int i) -> int {
        if (!b.above || is_inter_block(b.above)) return kDcPred;
        return b.above->sub_modes[2 + i];
    };
    auto left_mode = [&](int i) -> int {
        if (!b.left || is_inter_block(b.left)) return kDcPred;
        return b.left->sub_modes[i * 2 + 1];
    };
    if (b.bsize >= kBlock8x8) {
        const int m = m_bd.read_tree(k_intra_mode_tree, k_kf_y_mode_probs[above_mode(0)][left_mode(0)]);
        mi.y_mode = static_cast<std::uint8_t>(m);
        for (int i = 0; i < 4; ++i) mi.sub_modes[i] = static_cast<std::uint8_t>(m);
    } else {
        const int n4w = k_num_4x4_w[b.bsize];
        const int n4h = k_num_4x4_h[b.bsize];
        for (int idy = 0; idy < 2; idy += n4h) {
            for (int idx = 0; idx < 2; idx += n4w) {
                const int a = idy ? mi.sub_modes[idx] : above_mode(idx);
                const int l = idx ? mi.sub_modes[idy * 2] : left_mode(idy);
                const int m = m_bd.read_tree(k_intra_mode_tree, k_kf_y_mode_probs[a][l]);
                for (int y = 0; y < n4h; ++y)
                    for (int x = 0; x < n4w; ++x) mi.sub_modes[(idy + y) * 2 + idx + x] = static_cast<std::uint8_t>(m);
            }
        }
        mi.y_mode = mi.sub_modes[3];
    }
    mi.uv_mode = static_cast<std::uint8_t>(m_bd.read_tree(k_intra_mode_tree, k_kf_uv_mode_probs[mi.y_mode]));
}

void TileDecoder::intra_block_mode_info(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    mi.ref_frame[0] = kIntraFrame;
    mi.ref_frame[1] = kNoneFrame;
    mi.interp_filter = 3;
    if (b.bsize >= kBlock8x8) {
        const int g = k_size_group[b.bsize];
        const int m = m_bd.read_tree(k_intra_mode_tree, f.fc->y_mode[g]);
        if (f.counting) ++counts.y_mode[g][m];
        mi.y_mode = static_cast<std::uint8_t>(m);
        for (int i = 0; i < 4; ++i) mi.sub_modes[i] = static_cast<std::uint8_t>(m);
    } else {
        const int n4w = k_num_4x4_w[b.bsize];
        const int n4h = k_num_4x4_h[b.bsize];
        for (int idy = 0; idy < 2; idy += n4h) {
            for (int idx = 0; idx < 2; idx += n4w) {
                const int m = m_bd.read_tree(k_intra_mode_tree, f.fc->y_mode[0]);
                if (f.counting) ++counts.y_mode[0][m];
                for (int y = 0; y < n4h; ++y)
                    for (int x = 0; x < n4w; ++x) mi.sub_modes[(idy + y) * 2 + idx + x] = static_cast<std::uint8_t>(m);
            }
        }
        mi.y_mode = mi.sub_modes[3];
    }
    const int uv = m_bd.read_tree(k_intra_mode_tree, f.fc->uv_mode[mi.y_mode]);
    if (f.counting) ++counts.uv_mode[mi.y_mode][uv];
    mi.uv_mode = static_cast<std::uint8_t>(uv);
}

void TileDecoder::read_ref_frames(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    const FrameHeader& h = *f.hdr;
    if (f.seg->active(mi.segment_id, kSegLvlRefFrame)) {
        mi.ref_frame[0] = static_cast<std::int8_t>(f.seg->feature_data[mi.segment_id][kSegLvlRefFrame]);
        mi.ref_frame[1] = kNoneFrame;
        return;
    }
    int mode = h.reference_mode;
    if (mode == kReferenceModeSelect) {
        const int ctx = reference_mode_context(h, b.above, b.left);
        mode = m_bd.read(f.fc->comp_mode[ctx]);
        if (f.counting) ++counts.comp_mode[ctx][mode];
    }
    if (mode == kCompoundReference) {
        const int idx = h.ref_frame_sign_bias[h.comp_fixed_ref];
        const int ctx = comp_ref_context(h, b.above, b.left);
        const int bit = m_bd.read(f.fc->comp_ref[ctx]);
        if (f.counting) ++counts.comp_ref[ctx][bit];
        mi.ref_frame[idx] = static_cast<std::int8_t>(h.comp_fixed_ref);
        mi.ref_frame[!idx] = static_cast<std::int8_t>(h.comp_var_ref[bit]);
    } else {
        const int ctx0 = single_ref_p1_context(b.above, b.left);
        const int bit0 = m_bd.read(f.fc->single_ref[ctx0][0]);
        if (f.counting) ++counts.single_ref[ctx0][0][bit0];
        if (bit0) {
            const int ctx1 = single_ref_p2_context(b.above, b.left);
            const int bit1 = m_bd.read(f.fc->single_ref[ctx1][1]);
            if (f.counting) ++counts.single_ref[ctx1][1][bit1];
            mi.ref_frame[0] = static_cast<std::int8_t>(bit1 ? kAltrefFrame : kGoldenFrame);
        } else {
            mi.ref_frame[0] = kLastFrame;
        }
        mi.ref_frame[1] = kNoneFrame;
    }
}

int TileDecoder::read_mv_component(int comp, bool use_hp) {
    FrameDecodeContext& f = *m_ctx;
    const ProbContext& fc = *f.fc;
    const int sign = m_bd.read(fc.mv_sign[comp]);
    const int cls = m_bd.read_tree(k_mv_class_tree, fc.mv_class[comp]);
    int mag;
    int d;
    int fr;
    int hp;
    if (cls == 0) {
        d = m_bd.read(fc.mv_class0_bit[comp]);
        fr = m_bd.read_tree(k_mv_fr_tree, fc.mv_class0_fr[comp][d]);
        hp = use_hp ? m_bd.read(fc.mv_class0_hp[comp]) : 1;
        mag = 0;
        if (f.counting) {
            ++counts.mv_class0_bit[comp][d];
            ++counts.mv_class0_fr[comp][d][fr];
            ++counts.mv_class0_hp[comp][hp];
        }
    } else {
        d = 0;
        for (int i = 0; i < cls; ++i) {
            const int bit = m_bd.read(fc.mv_bits[comp][i]);
            d |= bit << i;
            if (f.counting) ++counts.mv_bits[comp][i][bit];
        }
        mag = 2 << (cls + 2);
        fr = m_bd.read_tree(k_mv_fr_tree, fc.mv_fr[comp]);
        hp = use_hp ? m_bd.read(fc.mv_hp[comp]) : 1;
        if (f.counting) {
            ++counts.mv_fr[comp][fr];
            ++counts.mv_hp[comp][hp];
        }
    }
    if (f.counting) {
        ++counts.mv_sign[comp][sign];
        ++counts.mv_class[comp][cls];
    }
    mag += ((d << 3) | (fr << 1) | hp) + 1;
    return sign ? -mag : mag;
}

void TileDecoder::read_mv(Mv& out, const Mv& ref, bool allow_hp) {
    FrameDecodeContext& f = *m_ctx;
    const int joint = m_bd.read_tree(k_mv_joint_tree, f.fc->mv_joint);
    if (f.counting) ++counts.mv_joint[joint];
    const bool hp = allow_hp && use_mv_hp(ref);
    int dr = 0;
    int dc = 0;
    if (joint == kMvJointHzvnz || joint == kMvJointHnzvnz) dr = read_mv_component(0, hp);
    if (joint == kMvJointHnzvz || joint == kMvJointHnzvnz) dc = read_mv_component(1, hp);
    const int r = ref.row + dr;
    const int c = ref.col + dc;
    const int lim = (1 << 14) - 1;
    if (r <= -(1 << 14) || r >= (1 << 14) || c <= -(1 << 14) || c >= (1 << 14)) corrupted = true;
    out.row = static_cast<std::int16_t>(iclamp(r, -lim, lim));
    out.col = static_cast<std::int16_t>(iclamp(c, -lim, lim));
}

void TileDecoder::find_mv_refs(Block& b, int ref_frame, int block, Mv* list, int* mode_context) {
    FrameDecodeContext& f = *m_ctx;
    const std::int8_t (*pos)[2] = k_mv_ref_blocks[b.bsize];
    const int* sign_bias = f.hdr->ref_frame_sign_bias;
    Mv lst[2];
    int count = 0;
    int counter = 0;
    bool different_ref_found = false;
    bool done = false;
    auto add = [&](const Mv& mv) {
        if (count) {
            if (!mv_equal(mv, lst[0])) {
                lst[1] = mv;
                done = true;
            }
        } else {
            lst[count++] = mv;
        }
    };
    auto inside = [&](int r, int c) {
        return !(r < 0 || c < m_mi_col_start || r >= f.mi_rows || c >= m_mi_col_end);
    };
    auto scaled = [&](const ModeInfo& m, int which) {
        Mv mv = m.mv[which];
        if (sign_bias[m.ref_frame[which]] != sign_bias[ref_frame]) {
            mv.row = static_cast<std::int16_t>(-mv.row);
            mv.col = static_cast<std::int16_t>(-mv.col);
        }
        return mv;
    };
    for (int i = 0; i < 2 && !done; ++i) {
        const int r = b.mi_row + pos[i][0];
        const int c = b.mi_col + pos[i][1];
        if (!inside(r, c)) continue;
        const ModeInfo& cand = f.mi[r * f.mi_stride + c];
        counter += k_mode_2_counter[cand.y_mode];
        different_ref_found = true;
        for (int w = 0; w < 2; ++w) {
            if (cand.ref_frame[w] == ref_frame) {
                const Mv mv = (block >= 0 && cand.sb_type < kBlock8x8) ? cand.bmv[k_idx_n_column_to_subblock[block][pos[i][1] == 0]][w] : cand.mv[w];
                add(mv);
                break;
            }
        }
    }
    for (int i = 2; i < 8 && !done; ++i) {
        const int r = b.mi_row + pos[i][0];
        const int c = b.mi_col + pos[i][1];
        if (!inside(r, c)) continue;
        const ModeInfo& cand = f.mi[r * f.mi_stride + c];
        different_ref_found = true;
        if (cand.ref_frame[0] == ref_frame) add(cand.mv[0]);
        else if (cand.ref_frame[1] == ref_frame) add(cand.mv[1]);
    }
    const MvRef* prev = f.prev_mvs ? &f.prev_mvs[static_cast<std::size_t>(b.mi_row) * f.mi_cols + b.mi_col] : nullptr;
    if (!done && prev) {
        if (prev->ref_frame[0] == ref_frame) add(prev->mv[0]);
        else if (prev->ref_frame[1] == ref_frame) add(prev->mv[1]);
    }
    if (!done && different_ref_found) {
        for (int i = 0; i < 8 && !done; ++i) {
            const int r = b.mi_row + pos[i][0];
            const int c = b.mi_col + pos[i][1];
            if (!inside(r, c)) continue;
            const ModeInfo& cand = f.mi[r * f.mi_stride + c];
            if (!is_inter_block(&cand)) continue;
            if (cand.ref_frame[0] != ref_frame) add(scaled(cand, 0));
            if (!done && has_second_ref(&cand) && cand.ref_frame[1] != ref_frame && !mv_equal(cand.mv[1], cand.mv[0])) add(scaled(cand, 1));
        }
    }
    if (!done && prev) {
        if (prev->ref_frame[0] != ref_frame && prev->ref_frame[0] > kIntraFrame) {
            Mv mv = prev->mv[0];
            if (sign_bias[prev->ref_frame[0]] != sign_bias[ref_frame]) {
                mv.row = static_cast<std::int16_t>(-mv.row);
                mv.col = static_cast<std::int16_t>(-mv.col);
            }
            add(mv);
        }
        if (!done && prev->ref_frame[1] > kIntraFrame && prev->ref_frame[1] != ref_frame && !mv_equal(prev->mv[1], prev->mv[0])) {
            Mv mv = prev->mv[1];
            if (sign_bias[prev->ref_frame[1]] != sign_bias[ref_frame]) {
                mv.row = static_cast<std::int16_t>(-mv.row);
                mv.col = static_cast<std::int16_t>(-mv.col);
            }
            add(mv);
        }
    }
    if (mode_context) *mode_context = k_counter_to_context[counter];
    for (int i = 0; i < 2; ++i) {
        clamp_mv(lst[i], b.mb_to_left_edge - kMvBorder, b.mb_to_right_edge + kMvBorder, b.mb_to_top_edge - kMvBorder, b.mb_to_bottom_edge + kMvBorder);
        list[i] = lst[i];
    }
}

void TileDecoder::append_sub8x8_mvs(Block& b, ModeInfo& mi, int block, int ref, Mv& nearest, Mv& near_mv) {
    Mv list[2];
    find_mv_refs(b, mi.ref_frame[ref], block, list, nullptr);
    near_mv = Mv();
    if (block == 0) {
        nearest = list[0];
        near_mv = list[1];
    } else if (block == 1 || block == 2) {
        nearest = mi.bmv[0][ref];
        for (int n = 0; n < 2; ++n) {
            if (!mv_equal(nearest, list[n])) {
                near_mv = list[n];
                break;
            }
        }
    } else {
        const Mv cands[4] = {mi.bmv[1][ref], mi.bmv[0][ref], list[0], list[1]};
        nearest = mi.bmv[2][ref];
        for (int n = 0; n < 4; ++n) {
            if (!mv_equal(nearest, cands[n])) {
                near_mv = cands[n];
                break;
            }
        }
    }
}

void TileDecoder::inter_block_mode_info(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    const FrameHeader& h = *f.hdr;
    read_ref_frames(b, mi);
    for (int r = 0; r < 2; ++r) {
        const int rf = mi.ref_frame[r];
        if (r == 1 && rf <= kIntraFrame) break;
        if (rf < kLastFrame || rf > kAltrefFrame || !f.refs[rf - 1] || !f.scale[rf - 1].valid) corrupted = true;
    }
    const bool compound = mi.ref_frame[1] > kIntraFrame;
    Mv nearest[2];
    Mv nearmv[2];
    int mode_ctx = 0;
    for (int r = 0; r < 1 + compound; ++r) {
        Mv list[2];
        int ctx = 0;
        find_mv_refs(b, mi.ref_frame[r], -1, list, &ctx);
        if (r == 0) mode_ctx = ctx;
        for (int i = 0; i < 2; ++i) {
            lower_mv_precision(list[i], h.allow_high_precision_mv);
            clamp_mv(list[i], b.mb_to_left_edge - kBestMvMargin, b.mb_to_right_edge + kBestMvMargin, b.mb_to_top_edge - kBestMvMargin, b.mb_to_bottom_edge + kBestMvMargin);
        }
        nearest[r] = list[0];
        nearmv[r] = list[1];
    }
    auto read_inter_mode = [&]() {
        const int m = m_bd.read_tree(k_inter_mode_tree, f.fc->inter_mode[mode_ctx]);
        if (f.counting) ++counts.inter_mode[mode_ctx][m];
        return kNearestMv + m;
    };
    if (f.seg->active(mi.segment_id, kSegLvlSkip)) {
        mi.y_mode = kZeroMv;
        if (b.bsize < kBlock8x8) corrupted = true;
    } else if (b.bsize >= kBlock8x8) {
        mi.y_mode = static_cast<std::uint8_t>(read_inter_mode());
    }
    if (h.interp_filter == kFilterSwitchable) {
        const int lt = (b.left && is_inter_block(b.left)) ? b.left->interp_filter : 3;
        const int at = (b.above && is_inter_block(b.above)) ? b.above->interp_filter : 3;
        int ctx;
        if (lt == at) ctx = lt;
        else if (lt == 3) ctx = at;
        else if (at == 3) ctx = lt;
        else ctx = 3;
        const int filt = m_bd.read_tree(k_interp_filter_tree, f.fc->interp_filter[ctx]);
        if (f.counting) ++counts.interp_filter[ctx][filt];
        mi.interp_filter = static_cast<std::uint8_t>(filt);
    } else {
        mi.interp_filter = static_cast<std::uint8_t>(h.interp_filter);
    }
    auto assign = [&](int mode, Mv* out, const Mv* nst, const Mv* nr) {
        for (int i = 0; i < 1 + compound; ++i) {
            if (mode == kNewMv) read_mv(out[i], nearest[i], h.allow_high_precision_mv);
            else if (mode == kNearestMv) out[i] = nst[i];
            else if (mode == kNearMv) out[i] = nr[i];
            else out[i] = Mv();
        }
        if (!compound) out[1] = Mv();
    };
    if (b.bsize < kBlock8x8) {
        const int n4w = k_num_4x4_w[b.bsize];
        const int n4h = k_num_4x4_h[b.bsize];
        int b_mode = kZeroMv;
        for (int idy = 0; idy < 2; idy += n4h) {
            for (int idx = 0; idx < 2; idx += n4w) {
                const int j = idy * 2 + idx;
                b_mode = read_inter_mode();
                Mv nst[2];
                Mv nr[2];
                if (b_mode == kNearestMv || b_mode == kNearMv) {
                    for (int r = 0; r < 1 + compound; ++r) append_sub8x8_mvs(b, mi, j, r, nst[r], nr[r]);
                }
                Mv out[2];
                assign(b_mode, out, nst, nr);
                mi.bmv[j][0] = out[0];
                mi.bmv[j][1] = out[1];
                if (n4h == 2) {
                    mi.bmv[j + 2][0] = out[0];
                    mi.bmv[j + 2][1] = out[1];
                }
                if (n4w == 2) {
                    mi.bmv[j + 1][0] = out[0];
                    mi.bmv[j + 1][1] = out[1];
                }
            }
        }
        mi.y_mode = static_cast<std::uint8_t>(b_mode);
        mi.mv[0] = mi.bmv[3][0];
        mi.mv[1] = mi.bmv[3][1];
    } else {
        assign(mi.y_mode, mi.mv, nearest, nearmv);
        for (int i = 0; i < 4; ++i) {
            mi.bmv[i][0] = mi.mv[0];
            mi.bmv[i][1] = mi.mv[1];
        }
    }
}

void TileDecoder::inter_frame_mode_info(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    mi.segment_id = static_cast<std::uint8_t>(read_inter_segment_id(b, mi));
    mi.skip = static_cast<std::uint8_t>(read_skip(b, mi.segment_id));
    int is_inter;
    if (f.seg->active(mi.segment_id, kSegLvlRefFrame)) {
        is_inter = f.seg->feature_data[mi.segment_id][kSegLvlRefFrame] != kIntraFrame;
    } else {
        int ctx;
        if (b.above && b.left) {
            const bool ai = !is_inter_block(b.above);
            const bool li = !is_inter_block(b.left);
            ctx = (li && ai) ? 3 : (li || ai);
        } else if (b.above || b.left) {
            ctx = 2 * !is_inter_block(b.above ? b.above : b.left);
        } else {
            ctx = 0;
        }
        is_inter = m_bd.read(f.fc->is_inter[ctx]);
        if (f.counting) ++counts.is_inter[ctx][is_inter];
    }
    mi.is_inter = static_cast<std::uint8_t>(is_inter);
    mi.tx_size = static_cast<std::uint8_t>(read_tx_size(b, !mi.skip || !is_inter));
    if (is_inter) {
        inter_block_mode_info(b, mi);
    } else {
        intra_block_mode_info(b, mi);
    }
}

int TileDecoder::decode_coefs(int plane, int tx_size, int tx_type, int ctx, const std::int16_t* dq, bool is_inter) {
    FrameDecodeContext& f = *m_ctx;
    const int ref = is_inter ? 1 : 0;
    const int type = plane > 0 ? 1 : 0;
    const std::uint8_t (*probs)[6][3] = f.fc->coef[tx_size][type][ref];
    std::uint32_t (*cnt)[6][4] = counts.coef[tx_size][type][ref];
    std::uint32_t (*eobc)[6] = counts.eob_branch[tx_size][type][ref];
    const bool counting = f.counting;
    const ScanOrder& so = scan_order(tx_size, tx_type);
    const std::int16_t* scan = so.scan;
    const std::int16_t* nb = so.neighbors;
    const int max_eob = 16 << (tx_size << 1);
    const std::uint8_t* band = tx_size == kTx4x4 ? k_band_4x4 : band_8x8_table();
    const int dq_shift = tx_size == kTx32x32 ? 1 : 0;
    const int bd = f.bit_depth;
    const std::uint8_t* cat6 = k_cat6_prob + (bd == 8 ? 4 : (bd == 10 ? 2 : 0));
    const int cat6_bits = bd == 8 ? 14 : (bd == 10 ? 16 : 18);
    const std::int64_t lim = (std::int64_t(1) << (7 + bd)) - 1;
    std::int32_t* coef = m_coef.data();
    std::uint8_t* tc = m_token_cache;
    int dqv = dq[0];
    int c = 0;
    while (c < max_eob) {
        int bnd = band[c];
        const std::uint8_t* p = probs[bnd][ctx];
        if (counting) ++eobc[bnd][ctx];
        if (!m_bd.read(p[0])) {
            if (counting) ++cnt[bnd][ctx][3];
            break;
        }
        while (!m_bd.read(p[1])) {
            if (counting) ++cnt[bnd][ctx][0];
            dqv = dq[1];
            tc[scan[c]] = 0;
            ++c;
            if (c >= max_eob) return c;
            ctx = (1 + tc[nb[2 * c]] + tc[nb[2 * c + 1]]) >> 1;
            bnd = band[c];
            p = probs[bnd][ctx];
        }
        int token;
        int val;
        if (!m_bd.read(p[2])) {
            if (counting) ++cnt[bnd][ctx][1];
            token = 1;
            val = 1;
        } else {
            if (counting) ++cnt[bnd][ctx][2];
            const std::uint8_t* pp = pareto_row(p[2]);
            if (!m_bd.read(pp[0])) {
                if (!m_bd.read(pp[1])) {
                    token = 2;
                    val = 2;
                } else {
                    token = 3 + m_bd.read(pp[2]);
                    val = token;
                }
            } else if (!m_bd.read(pp[3])) {
                if (!m_bd.read(pp[4])) {
                    token = 5;
                    val = 5 + m_bd.read(k_cat_probs[0][0]);
                } else {
                    token = 6;
                    val = 7;
                    val += m_bd.read(k_cat_probs[1][0]) << 1;
                    val += m_bd.read(k_cat_probs[1][1]);
                }
            } else if (!m_bd.read(pp[5])) {
                if (!m_bd.read(pp[6])) {
                    token = 7;
                    int v = 0;
                    for (int i = 0; i < 3; ++i) v = (v << 1) | m_bd.read(k_cat_probs[2][i]);
                    val = 11 + v;
                } else {
                    token = 8;
                    int v = 0;
                    for (int i = 0; i < 4; ++i) v = (v << 1) | m_bd.read(k_cat_probs[3][i]);
                    val = 19 + v;
                }
            } else if (!m_bd.read(pp[7])) {
                token = 9;
                int v = 0;
                for (int i = 0; i < 5; ++i) v = (v << 1) | m_bd.read(k_cat_probs[4][i]);
                val = 35 + v;
            } else {
                token = 10;
                int v = 0;
                for (int i = 0; i < cat6_bits; ++i) v = (v << 1) | m_bd.read(cat6[i]);
                val = 67 + v;
            }
        }
        std::int64_t v = (static_cast<std::int64_t>(val) * dqv) >> dq_shift;
        if (v > lim) v = lim;
        const int rc = scan[c];
        coef[rc] = static_cast<std::int32_t>(m_bd.read(128) ? -v : v);
        tc[rc] = k_energy_class[token];
        ++c;
        ctx = (1 + tc[nb[2 * c]] + tc[nb[2 * c + 1]]) >> 1;
        dqv = dq[1];
    }
    return c;
}

void TileDecoder::reconstruct_intra(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    Picture& pic = *f.cur;
    const bool lossless = f.hdr->lossless;
    const int bs8 = imax(b.bsize, kBlock8x8);
    for (int plane = 0; plane < 3; ++plane) {
        const int ssx = plane ? f.ssx : 0;
        const int ssy = plane ? f.ssy : 0;
        const int tx = plane ? uv_tx_size(b.bsize, mi.tx_size, ssx, ssy) : mi.tx_size;
        const int step = 1 << tx;
        const int n4w = imax(1, k_num_4x4_w[bs8] >> ssx);
        const int n4h = imax(1, k_num_4x4_h[bs8] >> ssy);
        const int max_w = n4w + (b.mb_to_right_edge >= 0 ? 0 : (b.mb_to_right_edge >> (5 + ssx)));
        const int max_h = n4h + (b.mb_to_bottom_edge >= 0 ? 0 : (b.mb_to_bottom_edge >> (5 + ssy)));
        const int px0 = (b.mi_col * 8) >> ssx;
        const int py0 = (b.mi_row * 8) >> ssy;
        const int fw = (f.mi_cols * 8) >> ssx;
        const int fh = (f.mi_rows * 8) >> ssy;
        std::uint16_t* base = pic.plane(plane);
        const std::ptrdiff_t stride = pic.stride[plane];
        std::uint8_t* above_ctx = f.above_nonzero[plane] + (px0 >> 2);
        std::uint8_t* left_ctx = m_left_nonzero[plane] + ((py0 >> 2) & 15);
        if (mi.skip) {
            std::memset(above_ctx, 0, static_cast<std::size_t>(n4w));
            std::memset(left_ctx, 0, static_cast<std::size_t>(n4h));
        }
        const std::int16_t* dq = f.dequant[mi.segment_id][plane > 0 ? 1 : 0];
        for (int row = 0; row < max_h; row += step) {
            for (int col = 0; col < max_w; col += step) {
                const int x = px0 + 4 * col;
                const int y = py0 + 4 * row;
                const int mode = plane == 0 ? (b.bsize < kBlock8x8 ? mi.sub_modes[(row << 1) + col] : mi.y_mode) : mi.uv_mode;
                const bool have_top = row > 0 || b.avail_u;
                const bool have_left = col > 0 || b.avail_l;
                const bool have_right = (col + step) < n4w;
                std::uint16_t* dst = base + y * stride + x;
                predict_intra(dst, stride, mode, tx, have_left, have_top, have_right, x, y, fw, fh, f.bit_depth);
                if (!mi.skip) {
                    const int tx_type = (plane || lossless || tx == kTx32x32) ? static_cast<int>(kDctDct) : static_cast<int>(k_intra_mode_to_tx_type[mode]);
                    int above_nz = 0;
                    int left_nz = 0;
                    for (int i = 0; i < step; ++i) {
                        above_nz |= above_ctx[col + i];
                        left_nz |= left_ctx[row + i];
                    }
                    const int ctx = (above_nz != 0) + (left_nz != 0);
                    const int eob = decode_coefs(plane, tx, tx_type, ctx, dq, false);
                    if (eob > 0) {
                        inverse_transform_add(dst, stride, m_coef.data(), tx, tx_type, eob, lossless, f.bit_depth);
                        const ScanOrder& so = scan_order(tx, tx_type);
                        for (int i = 0; i < eob; ++i) m_coef[static_cast<std::size_t>(so.scan[i])] = 0;
                    }
                    const std::uint8_t nz = eob > 0 ? 1 : 0;
                    for (int i = 0; i < step; ++i) {
                        above_ctx[col + i] = (col + i < max_w) ? nz : 0;
                        left_ctx[row + i] = (row + i < max_h) ? nz : 0;
                    }
                }
            }
        }
    }
}

void TileDecoder::predict_inter(Block& b, ModeInfo& mi, int plane, int ref, int x, int y, int w, int h, Mv mv, std::uint16_t* dst, std::ptrdiff_t stride, int pw, int ph) {
    FrameDecodeContext& f = *m_ctx;
    const int rf = mi.ref_frame[ref];
    if (rf < kLastFrame || rf > kAltrefFrame) return;
    const Picture* rp = f.refs[rf - 1];
    const ScaleFactors& sf = f.scale[rf - 1];
    if (!rp || !sf.valid) return;
    const int ssx = plane ? f.ssx : 0;
    const int ssy = plane ? f.ssy : 0;
    const int spel_left = (4 + pw) << 4;
    const int spel_right = spel_left - 16;
    const int spel_top = (4 + ph) << 4;
    const int spel_bottom = spel_top - 16;
    int mv_col = mv.col * (1 << (1 - ssx));
    int mv_row = mv.row * (1 << (1 - ssy));
    mv_col = iclamp(mv_col, b.mb_to_left_edge * (1 << (1 - ssx)) - spel_left, b.mb_to_right_edge * (1 << (1 - ssx)) + spel_right);
    mv_row = iclamp(mv_row, b.mb_to_top_edge * (1 << (1 - ssy)) - spel_top, b.mb_to_bottom_edge * (1 << (1 - ssy)) + spel_bottom);
    const int rw = (rp->width + ssx) >> ssx;
    const int rh = (rp->height + ssy) >> ssy;
    int xi;
    int yi;
    int fx;
    int fy;
    int xs = 16;
    int ys = 16;
    if (!sf.scaled) {
        xi = x + (mv_col >> 4);
        yi = y + (mv_row >> 4);
        fx = mv_col & 15;
        fy = mv_row & 15;
    } else {
        auto sx = [&](std::int64_t v) { return static_cast<int>((v * sf.x_scale) >> 14); };
        auto sy = [&](std::int64_t v) { return static_cast<int>((v * sf.y_scale) >> 14); };
        const int px0 = (b.mi_col * 8) >> ssx;
        const int py0 = (b.mi_row * 8) >> ssy;
        const int xoff = x - px0;
        const int yoff = y - py0;
        const int x0 = sx(x);
        const int y0 = sy(y);
        const int x_off_q4 = sx(static_cast<std::int64_t>(b.mi_col * 8 + xoff) << 4) & 15;
        const int y_off_q4 = sy(static_cast<std::int64_t>(b.mi_row * 8 + yoff) << 4) & 15;
        const int smv_col = sx(mv_col) + x_off_q4;
        const int smv_row = sy(mv_row) + y_off_q4;
        xi = x0 + (smv_col >> 4);
        yi = y0 + (smv_row >> 4);
        fx = smv_col & 15;
        fy = smv_row & 15;
        xs = sf.x_step;
        ys = sf.y_step;
    }
    const int x_first = xi - 3;
    const int y_first = yi - 3;
    const int x_last = xi + (((w - 1) * xs + fx) >> 4) + 4;
    const int y_last = yi + (((h - 1) * ys + fy) >> 4) + 4;
    const std::uint16_t* src;
    std::ptrdiff_t src_stride;
    const std::uint16_t* ref_plane = rp->plane(plane);
    const std::ptrdiff_t ref_stride = rp->stride[plane];
    if (x_first >= 0 && y_first >= 0 && x_last < rw && y_last < rh) {
        src = ref_plane + yi * ref_stride + xi;
        src_stride = ref_stride;
    } else {
        const int bw = x_last - x_first + 1;
        const int bh = y_last - y_first + 1;
        const std::size_t need = static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh);
        if (m_mc_buf.size() < need) m_mc_buf.resize(need);
        std::uint16_t* mb = m_mc_buf.data();
        for (int r = 0; r < bh; ++r) {
            const std::uint16_t* srow = ref_plane + iclamp(y_first + r, 0, rh - 1) * ref_stride;
            std::uint16_t* drow = mb + r * bw;
            for (int c = 0; c < bw; ++c) drow[c] = srow[iclamp(x_first + c, 0, rw - 1)];
        }
        src = mb + 3 * bw + 3;
        src_stride = bw;
    }
    convolve(src, src_stride, dst, stride, k_subpel_filters[mi.interp_filter & 3], fx, xs, fy, ys, w, h, f.bit_depth, ref > 0, m_mc_tmp.data());
}

void TileDecoder::reconstruct_inter(Block& b, ModeInfo& mi) {
    FrameDecodeContext& f = *m_ctx;
    Picture& pic = *f.cur;
    const int bs8 = imax(b.bsize, kBlock8x8);
    const bool compound = mi.ref_frame[1] > kIntraFrame;
    for (int plane = 0; plane < 3; ++plane) {
        const int ssx = plane ? f.ssx : 0;
        const int ssy = plane ? f.ssy : 0;
        const int pw = (4 * k_num_4x4_w[bs8]) >> ssx;
        const int ph = (4 * k_num_4x4_h[bs8]) >> ssy;
        const int px0 = (b.mi_col * 8) >> ssx;
        const int py0 = (b.mi_row * 8) >> ssy;
        std::uint16_t* base = pic.plane(plane);
        const std::ptrdiff_t stride = pic.stride[plane];
        for (int ref = 0; ref < 1 + compound; ++ref) {
            if (b.bsize < kBlock8x8) {
                const int n4w = imax(1, pw >> 2);
                const int n4h = imax(1, ph >> 2);
                int i = 0;
                for (int y = 0; y < n4h; ++y) {
                    for (int x = 0; x < n4w; ++x) {
                        Mv mv;
                        if (!ssx && !ssy) {
                            mv = mi.bmv[i][ref];
                        } else if (ssx && ssy) {
                            mv.row = static_cast<std::int16_t>(round_q4(mi.bmv[0][ref].row + mi.bmv[1][ref].row + mi.bmv[2][ref].row + mi.bmv[3][ref].row));
                            mv.col = static_cast<std::int16_t>(round_q4(mi.bmv[0][ref].col + mi.bmv[1][ref].col + mi.bmv[2][ref].col + mi.bmv[3][ref].col));
                        } else {
                            const int b0 = i;
                            const int b1 = ssy ? i + 2 : i + 1;
                            mv.row = static_cast<std::int16_t>(round_q2(mi.bmv[b0][ref].row + mi.bmv[b1 & 3][ref].row));
                            mv.col = static_cast<std::int16_t>(round_q2(mi.bmv[b0][ref].col + mi.bmv[b1 & 3][ref].col));
                        }
                        ++i;
                        predict_inter(b, mi, plane, ref, px0 + 4 * x, py0 + 4 * y, 4, 4, mv, base + (py0 + 4 * y) * stride + px0 + 4 * x, stride, pw, ph);
                    }
                }
            } else {
                predict_inter(b, mi, plane, ref, px0, py0, pw, ph, mi.mv[ref], base + py0 * stride + px0, stride, pw, ph);
            }
        }
    }
    int eobtotal = 0;
    for (int plane = 0; plane < 3; ++plane) {
        const int ssx = plane ? f.ssx : 0;
        const int ssy = plane ? f.ssy : 0;
        const int n4w = imax(1, k_num_4x4_w[bs8] >> ssx);
        const int n4h = imax(1, k_num_4x4_h[bs8] >> ssy);
        const int px0 = (b.mi_col * 8) >> ssx;
        const int py0 = (b.mi_row * 8) >> ssy;
        std::uint8_t* above_ctx = f.above_nonzero[plane] + (px0 >> 2);
        std::uint8_t* left_ctx = m_left_nonzero[plane] + ((py0 >> 2) & 15);
        if (mi.skip) {
            std::memset(above_ctx, 0, static_cast<std::size_t>(n4w));
            std::memset(left_ctx, 0, static_cast<std::size_t>(n4h));
            continue;
        }
        const int tx = plane ? uv_tx_size(b.bsize, mi.tx_size, ssx, ssy) : mi.tx_size;
        const int step = 1 << tx;
        const int max_w = n4w + (b.mb_to_right_edge >= 0 ? 0 : (b.mb_to_right_edge >> (5 + ssx)));
        const int max_h = n4h + (b.mb_to_bottom_edge >= 0 ? 0 : (b.mb_to_bottom_edge >> (5 + ssy)));
        std::uint16_t* base = pic.plane(plane);
        const std::ptrdiff_t stride = pic.stride[plane];
        const std::int16_t* dq = f.dequant[mi.segment_id][plane > 0 ? 1 : 0];
        for (int row = 0; row < max_h; row += step) {
            for (int col = 0; col < max_w; col += step) {
                int above_nz = 0;
                int left_nz = 0;
                for (int i = 0; i < step; ++i) {
                    above_nz |= above_ctx[col + i];
                    left_nz |= left_ctx[row + i];
                }
                const int ctx = (above_nz != 0) + (left_nz != 0);
                const int eob = decode_coefs(plane, tx, kDctDct, ctx, dq, true);
                if (eob > 0) {
                    std::uint16_t* dst = base + (py0 + 4 * row) * stride + px0 + 4 * col;
                    inverse_transform_add(dst, stride, m_coef.data(), tx, kDctDct, eob, f.hdr->lossless, f.bit_depth);
                    const ScanOrder& so = scan_order(tx, kDctDct);
                    for (int i = 0; i < eob; ++i) m_coef[static_cast<std::size_t>(so.scan[i])] = 0;
                }
                eobtotal += eob;
                const std::uint8_t nz = eob > 0 ? 1 : 0;
                for (int i = 0; i < step; ++i) {
                    above_ctx[col + i] = (col + i < max_w) ? nz : 0;
                    left_ctx[row + i] = (row + i < max_h) ? nz : 0;
                }
            }
        }
    }
    if (!mi.skip && b.bsize >= kBlock8x8 && eobtotal == 0) mi.skip = 1;
}

void TileDecoder::decode_block(int mi_row, int mi_col, int bsize) {
    FrameDecodeContext& f = *m_ctx;
    if (bsize >= kBlockInvalid) {
        corrupted = true;
        return;
    }
    Block b;
    b.mi_row = mi_row;
    b.mi_col = mi_col;
    b.bsize = bsize;
    b.bw8 = k_num_8x8_w[bsize];
    b.bh8 = k_num_8x8_h[bsize];
    b.x_mis = imin(b.bw8, f.mi_cols - mi_col);
    b.y_mis = imin(b.bh8, f.mi_rows - mi_row);
    b.avail_u = mi_row > 0;
    b.avail_l = mi_col > m_mi_col_start;
    b.above = b.avail_u ? &f.mi[(mi_row - 1) * f.mi_stride + mi_col] : nullptr;
    b.left = b.avail_l ? &f.mi[mi_row * f.mi_stride + mi_col - 1] : nullptr;
    b.mb_to_left_edge = -(mi_col * 64);
    b.mb_to_right_edge = (f.mi_cols - b.bw8 - mi_col) * 64;
    b.mb_to_top_edge = -(mi_row * 64);
    b.mb_to_bottom_edge = (f.mi_rows - b.bh8 - mi_row) * 64;
    ModeInfo mi;
    mi.sb_type = static_cast<std::uint8_t>(bsize);
    if (f.hdr->frame_is_intra) intra_frame_mode_info(b, mi);
    else inter_frame_mode_info(b, mi);
    if (mi.is_inter) reconstruct_inter(b, mi);
    else reconstruct_intra(b, mi);
    MvRef mr;
    mr.mv[0] = mi.mv[0];
    mr.mv[1] = mi.mv[1];
    mr.ref_frame[0] = mi.ref_frame[0];
    mr.ref_frame[1] = mi.ref_frame[1];
    for (int y = 0; y < b.y_mis; ++y) {
        ModeInfo* row = &f.mi[(mi_row + y) * f.mi_stride + mi_col];
        MvRef* mrow = &f.cur->mvs[static_cast<std::size_t>(mi_row + y) * f.mi_cols + mi_col];
        for (int x = 0; x < b.x_mis; ++x) {
            row[x] = mi;
            mrow[x] = mr;
        }
    }
    if (m_bd.overrun()) corrupted = true;
}

void Counts::add(const Counts& o) noexcept {
    const std::uint32_t* s = reinterpret_cast<const std::uint32_t*>(&o);
    std::uint32_t* d = reinterpret_cast<std::uint32_t*>(this);
    const std::size_t n = sizeof(Counts) / sizeof(std::uint32_t);
    for (std::size_t i = 0; i < n; ++i) d[i] += s[i];
}

} // namespace vp9_detail
} // namespace video
} // namespace fizmo
