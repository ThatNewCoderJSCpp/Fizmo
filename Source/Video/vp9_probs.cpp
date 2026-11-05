#include "vp9_internal.hpp"

namespace fizmo {
namespace video {
namespace vp9_detail {

namespace {

inline std::uint8_t clip_prob(int p) noexcept { return static_cast<std::uint8_t>(p > 255 ? 255 : (p < 1 ? 1 : p)); }

inline std::uint8_t get_prob(std::uint32_t num, std::uint32_t den) noexcept {
    const int p = static_cast<int>((static_cast<std::uint64_t>(num) * 256 + (den >> 1)) / den);
    return clip_prob(p);
}

inline std::uint8_t merge_probs(std::uint8_t pre, std::uint32_t c0, std::uint32_t c1, std::uint32_t count_sat, std::uint32_t max_update) noexcept {
    const std::uint32_t den = c0 + c1;
    const int prob = den == 0 ? 128 : get_prob(c0, den);
    const std::uint32_t count = den < count_sat ? den : count_sat;
    const std::uint32_t factor = max_update * count / count_sat;
    return static_cast<std::uint8_t>((pre * (256 - static_cast<int>(factor)) + prob * static_cast<int>(factor) + 128) >> 8);
}

inline std::uint8_t mode_merge(std::uint8_t pre, std::uint32_t c0, std::uint32_t c1) noexcept {
    if (c0 + c1 == 0) return pre;
    return merge_probs(pre, c0, c1, 20, 128);
}

std::uint32_t tree_merge(const std::int8_t* tree, int i, const std::uint8_t* pre, const std::uint32_t* counts, std::uint8_t* probs) noexcept {
    const int l = tree[i];
    const std::uint32_t left = l <= 0 ? counts[-l] : tree_merge(tree, l, pre, counts, probs);
    const int r = tree[i + 1];
    const std::uint32_t right = r <= 0 ? counts[-r] : tree_merge(tree, r, pre, counts, probs);
    probs[i >> 1] = mode_merge(pre[i >> 1], left, right);
    return left + right;
}

} // namespace

void adapt_coef_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool intra_frame, bool last_was_key) noexcept {
    const std::uint32_t update = intra_frame ? 112 : (last_was_key ? 128 : 112);
    const std::uint32_t sat = 24;
    for (int t = 0; t < 4; ++t)
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                for (int k = 0; k < 6; ++k) {
                    const int nctx = k == 0 ? 3 : 6;
                    for (int l = 0; l < nctx; ++l) {
                        const std::uint32_t* c = counts.coef[t][i][j][k][l];
                        const std::uint32_t n0 = c[0];
                        const std::uint32_t n1 = c[1];
                        const std::uint32_t n2 = c[2];
                        const std::uint32_t neob = c[3];
                        const std::uint32_t eb = counts.eob_branch[t][i][j][k][l];
                        const std::uint8_t* pp = pre.coef[t][i][j][k][l];
                        std::uint8_t* p = fc.coef[t][i][j][k][l];
                        p[0] = merge_probs(pp[0], neob, eb - neob, sat, update);
                        p[1] = merge_probs(pp[1], n0, n1 + n2, sat, update);
                        p[2] = merge_probs(pp[2], n1, n2, sat, update);
                    }
                }
}

void adapt_mode_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool switchable_interp, bool tx_select) noexcept {
    for (int i = 0; i < 4; ++i) fc.is_inter[i] = mode_merge(pre.is_inter[i], counts.is_inter[i][0], counts.is_inter[i][1]);
    for (int i = 0; i < 5; ++i) fc.comp_mode[i] = mode_merge(pre.comp_mode[i], counts.comp_mode[i][0], counts.comp_mode[i][1]);
    for (int i = 0; i < 5; ++i) fc.comp_ref[i] = mode_merge(pre.comp_ref[i], counts.comp_ref[i][0], counts.comp_ref[i][1]);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 2; ++j) fc.single_ref[i][j] = mode_merge(pre.single_ref[i][j], counts.single_ref[i][j][0], counts.single_ref[i][j][1]);
    for (int i = 0; i < 7; ++i) tree_merge(k_inter_mode_tree, 0, pre.inter_mode[i], counts.inter_mode[i], fc.inter_mode[i]);
    for (int i = 0; i < 4; ++i) tree_merge(k_intra_mode_tree, 0, pre.y_mode[i], counts.y_mode[i], fc.y_mode[i]);
    for (int i = 0; i < 10; ++i) tree_merge(k_intra_mode_tree, 0, pre.uv_mode[i], counts.uv_mode[i], fc.uv_mode[i]);
    for (int i = 0; i < 16; ++i) tree_merge(k_partition_tree, 0, pre.partition[i], counts.partition[i], fc.partition[i]);
    if (switchable_interp) {
        for (int i = 0; i < 4; ++i) tree_merge(k_interp_filter_tree, 0, pre.interp_filter[i], counts.interp_filter[i], fc.interp_filter[i]);
    }
    if (tx_select) {
        for (int i = 0; i < 2; ++i) {
            const std::uint32_t* c8 = counts.tx8[i];
            fc.tx8[i][0] = mode_merge(pre.tx8[i][0], c8[0], c8[1]);
            const std::uint32_t* c16 = counts.tx16[i];
            fc.tx16[i][0] = mode_merge(pre.tx16[i][0], c16[0], c16[1] + c16[2]);
            fc.tx16[i][1] = mode_merge(pre.tx16[i][1], c16[1], c16[2]);
            const std::uint32_t* c32 = counts.tx32[i];
            fc.tx32[i][0] = mode_merge(pre.tx32[i][0], c32[0], c32[1] + c32[2] + c32[3]);
            fc.tx32[i][1] = mode_merge(pre.tx32[i][1], c32[1], c32[2] + c32[3]);
            fc.tx32[i][2] = mode_merge(pre.tx32[i][2], c32[2], c32[3]);
        }
    }
    for (int i = 0; i < 3; ++i) fc.skip[i] = mode_merge(pre.skip[i], counts.skip[i][0], counts.skip[i][1]);
}

void adapt_mv_probs(ProbContext& fc, const ProbContext& pre, const Counts& counts, bool allow_hp) noexcept {
    tree_merge(k_mv_joint_tree, 0, pre.mv_joint, counts.mv_joint, fc.mv_joint);
    for (int i = 0; i < 2; ++i) {
        fc.mv_sign[i] = mode_merge(pre.mv_sign[i], counts.mv_sign[i][0], counts.mv_sign[i][1]);
        tree_merge(k_mv_class_tree, 0, pre.mv_class[i], counts.mv_class[i], fc.mv_class[i]);
        fc.mv_class0_bit[i] = mode_merge(pre.mv_class0_bit[i], counts.mv_class0_bit[i][0], counts.mv_class0_bit[i][1]);
        for (int j = 0; j < 10; ++j) fc.mv_bits[i][j] = mode_merge(pre.mv_bits[i][j], counts.mv_bits[i][j][0], counts.mv_bits[i][j][1]);
        for (int j = 0; j < 2; ++j) tree_merge(k_mv_fr_tree, 0, pre.mv_class0_fr[i][j], counts.mv_class0_fr[i][j], fc.mv_class0_fr[i][j]);
        tree_merge(k_mv_fr_tree, 0, pre.mv_fr[i], counts.mv_fr[i], fc.mv_fr[i]);
        if (allow_hp) {
            fc.mv_class0_hp[i] = mode_merge(pre.mv_class0_hp[i], counts.mv_class0_hp[i][0], counts.mv_class0_hp[i][1]);
            fc.mv_hp[i] = mode_merge(pre.mv_hp[i], counts.mv_hp[i][0], counts.mv_hp[i][1]);
        }
    }
}

namespace {

int inv_recenter_nonneg(int v, int m) noexcept {
    if (v > 2 * m) return v;
    return (v & 1) ? m - ((v + 1) >> 1) : m + (v >> 1);
}

int inv_remap_prob(int v, int m) noexcept {
    v = inv_map_table()[iclamp(v, 0, 253)];
    m--;
    if ((m << 1) <= 255) return 1 + inv_recenter_nonneg(v, m);
    return 255 - inv_recenter_nonneg(v, 255 - 1 - m);
}

int decode_term_subexp(BoolDecoder& r) noexcept {
    if (!r.read_bit()) return static_cast<int>(r.read_literal(4));
    if (!r.read_bit()) return static_cast<int>(r.read_literal(4)) + 16;
    if (!r.read_bit()) return static_cast<int>(r.read_literal(5)) + 32;
    const int v = static_cast<int>(r.read_literal(7));
    if (v < 65) return v + 64;
    return (v << 1) - 1 + r.read_bit();
}

void diff_update(BoolDecoder& r, std::uint8_t& p) noexcept {
    if (r.read(252)) {
        const int d = decode_term_subexp(r);
        p = static_cast<std::uint8_t>(iclamp(inv_remap_prob(d, p), 1, 255));
    }
}

void mv_update(BoolDecoder& r, std::uint8_t& p) noexcept {
    if (r.read(252)) p = static_cast<std::uint8_t>((r.read_literal(7) << 1) | 1);
}

} // namespace

bool read_compressed_header(const std::uint8_t* data, std::size_t size, FrameHeader& h, ProbContext& fc);

bool read_compressed_header(const std::uint8_t* data, std::size_t size, FrameHeader& h, ProbContext& fc) {
    BoolDecoder r;
    if (!r.init(data, size)) return false;
    if (h.lossless) {
        h.tx_mode = kOnly4x4;
    } else {
        h.tx_mode = static_cast<int>(r.read_literal(2));
        if (h.tx_mode == kAllow32x32) h.tx_mode += static_cast<int>(r.read_literal(1));
    }
    if (h.tx_mode == kTxModeSelect) {
        for (int i = 0; i < 2; ++i) diff_update(r, fc.tx8[i][0]);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j) diff_update(r, fc.tx16[i][j]);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 3; ++j) diff_update(r, fc.tx32[i][j]);
    }
    const int max_tx = k_tx_mode_to_biggest[h.tx_mode];
    for (int t = 0; t <= max_tx; ++t) {
        if (!r.read_literal(1)) continue;
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                for (int k = 0; k < 6; ++k) {
                    const int nctx = k == 0 ? 3 : 6;
                    for (int l = 0; l < nctx; ++l)
                        for (int m = 0; m < 3; ++m) diff_update(r, fc.coef[t][i][j][k][l][m]);
                }
    }
    for (int i = 0; i < 3; ++i) diff_update(r, fc.skip[i]);
    if (!h.frame_is_intra) {
        for (int i = 0; i < 7; ++i)
            for (int j = 0; j < 3; ++j) diff_update(r, fc.inter_mode[i][j]);
        if (h.interp_filter == kFilterSwitchable) {
            for (int j = 0; j < 4; ++j)
                for (int i = 0; i < 2; ++i) diff_update(r, fc.interp_filter[j][i]);
        }
        for (int i = 0; i < 4; ++i) diff_update(r, fc.is_inter[i]);
        bool compound_allowed = false;
        for (int i = 2; i <= 3; ++i) {
            if (h.ref_frame_sign_bias[i] != h.ref_frame_sign_bias[1]) compound_allowed = true;
        }
        if (compound_allowed) {
            if (r.read_literal(1)) h.reference_mode = r.read_literal(1) ? kReferenceModeSelect : kCompoundReference;
            else h.reference_mode = kSingleReference;
        } else {
            h.reference_mode = kSingleReference;
        }
        if (h.ref_frame_sign_bias[kLastFrame] == h.ref_frame_sign_bias[kGoldenFrame]) {
            h.comp_fixed_ref = kAltrefFrame;
            h.comp_var_ref[0] = kLastFrame;
            h.comp_var_ref[1] = kGoldenFrame;
        } else if (h.ref_frame_sign_bias[kLastFrame] == h.ref_frame_sign_bias[kAltrefFrame]) {
            h.comp_fixed_ref = kGoldenFrame;
            h.comp_var_ref[0] = kLastFrame;
            h.comp_var_ref[1] = kAltrefFrame;
        } else {
            h.comp_fixed_ref = kLastFrame;
            h.comp_var_ref[0] = kGoldenFrame;
            h.comp_var_ref[1] = kAltrefFrame;
        }
        if (h.reference_mode == kReferenceModeSelect) {
            for (int i = 0; i < 5; ++i) diff_update(r, fc.comp_mode[i]);
        }
        if (h.reference_mode != kCompoundReference) {
            for (int i = 0; i < 5; ++i) {
                diff_update(r, fc.single_ref[i][0]);
                diff_update(r, fc.single_ref[i][1]);
            }
        }
        if (h.reference_mode != kSingleReference) {
            for (int i = 0; i < 5; ++i) diff_update(r, fc.comp_ref[i]);
        }
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 9; ++j) diff_update(r, fc.y_mode[i][j]);
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 3; ++j) diff_update(r, fc.partition[i][j]);
        for (int j = 0; j < 3; ++j) mv_update(r, fc.mv_joint[j]);
        for (int i = 0; i < 2; ++i) {
            mv_update(r, fc.mv_sign[i]);
            for (int j = 0; j < 10; ++j) mv_update(r, fc.mv_class[i][j]);
            mv_update(r, fc.mv_class0_bit[i]);
            for (int j = 0; j < 10; ++j) mv_update(r, fc.mv_bits[i][j]);
        }
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j)
                for (int k = 0; k < 3; ++k) mv_update(r, fc.mv_class0_fr[i][j][k]);
            for (int k = 0; k < 3; ++k) mv_update(r, fc.mv_fr[i][k]);
        }
        if (h.allow_high_precision_mv) {
            for (int i = 0; i < 2; ++i) {
                mv_update(r, fc.mv_class0_hp[i]);
                mv_update(r, fc.mv_hp[i]);
            }
        }
    }
    return true;
}

} // namespace vp9_detail
} // namespace video
} // namespace fizmo
