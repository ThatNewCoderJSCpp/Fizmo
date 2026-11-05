#include "vp9_internal.hpp"

#include <deque>
#include <new>
#include <thread>
#include <utility>

namespace fizmo {
namespace video {

namespace vp9_detail {

bool read_compressed_header(const std::uint8_t* data, std::size_t size, FrameHeader& h, ProbContext& fc);

bool Picture::allocate(int w, int h, int sx, int sy, int bd) {
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return false;
    width = w;
    height = h;
    ssx = sx;
    ssy = sy;
    bit_depth = bd;
    mi_cols = (w + 7) >> 3;
    mi_rows = (h + 7) >> 3;
    const int aw = ((mi_cols + 7) >> 3) * 64;
    const int ah = ((mi_rows + 7) >> 3) * 64;
    std::size_t total = 0;
    for (int p = 0; p < 3; ++p) {
        const int px = p ? sx : 0;
        const int py = p ? sy : 0;
        plane_w[p] = (w + px) >> px;
        plane_h[p] = (h + py) >> py;
        alloc_w[p] = aw >> px;
        alloc_h[p] = ah >> py;
        stride[p] = alloc_w[p] + 16;
        offset[p] = total + 8;
        total += static_cast<std::size_t>(stride[p]) * static_cast<std::size_t>(alloc_h[p]) + 16;
    }
    if (data.size() != total) data.assign(total, static_cast<std::uint16_t>(1 << (bd - 1)));
    mvs.assign(static_cast<std::size_t>(mi_cols) * static_cast<std::size_t>(mi_rows), MvRef());
    return true;
}

} // namespace vp9_detail

using namespace vp9_detail;

namespace {

PixelFormat format_for(int bd, int ssx, int ssy) noexcept {
    const int k = bd >= 12 ? 2 : (bd >= 10 ? 1 : 0);
    if (ssx && ssy) {
        const PixelFormat f[3] = {PixelFormat::yuv420p, PixelFormat::yuv420p10, PixelFormat::yuv420p12};
        return f[k];
    }
    if (ssx) {
        const PixelFormat f[3] = {PixelFormat::yuv422p, PixelFormat::yuv422p10, PixelFormat::yuv422p12};
        return f[k];
    }
    if (ssy) {
        const PixelFormat f[3] = {PixelFormat::yuv440p, PixelFormat::yuv440p10, PixelFormat::yuv440p12};
        return f[k];
    }
    const PixelFormat f[3] = {PixelFormat::yuv444p, PixelFormat::yuv444p10, PixelFormat::yuv444p12};
    return f[k];
}

ColorInfo color_for(int color_space, int color_range, const Vp9CodecConfig& cfg) noexcept {
    ColorInfo c;
    switch (color_space) {
    case 1:
        c.matrix = ColorMatrix::bt601;
        c.matrix_coefficients = 5;
        break;
    case 2:
        c.matrix = ColorMatrix::bt709;
        c.matrix_coefficients = 1;
        break;
    case 3:
        c.matrix = ColorMatrix::bt601;
        c.matrix_coefficients = 6;
        break;
    case 4:
        c.matrix = ColorMatrix::bt709;
        c.matrix_coefficients = 7;
        break;
    case 5:
        c.matrix = ColorMatrix::bt2020;
        c.matrix_coefficients = 9;
        break;
    case 7:
        c.matrix = ColorMatrix::rgb;
        c.matrix_coefficients = 0;
        break;
    default:
        c.matrix = ColorMatrix::unspecified;
        c.matrix_coefficients = 2;
        break;
    }
    c.range = color_range ? ColorRange::full : ColorRange::limited;
    if (cfg.primaries >= 0 && cfg.primaries < 256) c.primaries = static_cast<std::uint8_t>(cfg.primaries);
    if (cfg.transfer >= 0 && cfg.transfer < 256) c.transfer = static_cast<std::uint8_t>(cfg.transfer);
    if (color_space == 0 && cfg.matrix >= 0 && cfg.matrix < 256) {
        c.matrix_coefficients = static_cast<std::uint8_t>(cfg.matrix);
        if (cfg.matrix == 1) c.matrix = ColorMatrix::bt709;
        else if (cfg.matrix == 5 || cfg.matrix == 6) c.matrix = ColorMatrix::bt601;
        else if (cfg.matrix == 9 || cfg.matrix == 10) c.matrix = ColorMatrix::bt2020;
        else if (cfg.matrix == 0) c.matrix = ColorMatrix::rgb;
    }
    c.chroma_location = cfg.chroma_subsampling == 1 ? 2 : 0;
    return c;
}

} // namespace

struct Vp9Decoder::State {
    bool opened = false;
    Vp9Info info;
    Vp9CodecConfig config;
    int threads = 1;
    std::uint64_t frames = 0;
    std::uint64_t errors = 0;
    std::deque<VideoFrame> out;

    std::shared_ptr<Picture> refs[kNumRefFrames];
    ProbContext contexts[4];
    LoopFilterParams lf;
    SegmentationParams seg;
    int bit_depth = 8;
    int ssx = 1;
    int ssy = 1;
    int color_space = 0;
    int color_range = 0;
    int interp_filter = 0;
    bool have_keyframe = false;
    std::shared_ptr<Picture> prev_frame;
    bool last_show_frame = false;
    int last_width = 0;
    int last_height = 0;
    int last_frame_type = kKeyFrame;
    int seg_cols = 0;
    int seg_rows = 0;
    std::vector<std::uint8_t> seg_map[2];
    int seg_cur = 0;
    std::vector<ModeInfo> mi;
    std::vector<std::uint8_t> above_nonzero[3];
    std::vector<std::uint8_t> above_partition;
    std::vector<std::shared_ptr<Picture>> pool;
    std::vector<std::unique_ptr<TileDecoder>> tiles;

    State() {
        for (auto& c : contexts) default_probs(c);
    }

    void reset_state() {
        for (auto& r : refs) r.reset();
        for (auto& c : contexts) default_probs(c);
        lf = LoopFilterParams();
        seg = SegmentationParams();
        bit_depth = 8;
        ssx = ssy = 1;
        color_space = 0;
        color_range = 0;
        have_keyframe = false;
        prev_frame.reset();
        last_show_frame = false;
        last_width = last_height = 0;
        last_frame_type = kKeyFrame;
        seg_cols = seg_rows = 0;
        seg_map[0].clear();
        seg_map[1].clear();
        out.clear();
    }

    std::shared_ptr<Picture> get_picture() {
        for (auto& p : pool) {
            if (p.use_count() == 1) return p;
        }
        pool.push_back(std::make_shared<Picture>());
        return pool.back();
    }

    bool fail(std::string* error, const char* msg) {
        if (error) *error = msg;
        ++errors;
        return false;
    }

    bool read_color_config(BitReader& br, int profile, std::string* error) {
        int bd = 8;
        if (profile >= 2) bd = br.bit() ? 12 : 10;
        const int cs = static_cast<int>(br.bits(3));
        int range;
        int sx;
        int sy;
        if (cs != 7) {
            range = static_cast<int>(br.bit());
            if (profile == 1 || profile == 3) {
                sx = static_cast<int>(br.bit());
                sy = static_cast<int>(br.bit());
                if (sx == 1 && sy == 1) return fail(error, "VP9: 4:2:0 color is not allowed in profile 1 or 3");
                if (br.bit()) return fail(error, "VP9: reserved bit set in color config");
            } else {
                sx = sy = 1;
            }
        } else {
            range = 1;
            if (profile == 1 || profile == 3) {
                sx = sy = 0;
                if (br.bit()) return fail(error, "VP9: reserved bit set in color config");
            } else {
                return fail(error, "VP9: RGB is not allowed in profile 0 or 2");
            }
        }
        bit_depth = bd;
        color_space = cs;
        color_range = range;
        ssx = sx;
        ssy = sy;
        return true;
    }

    void setup_past_independence(FrameHeader& h) {
        for (int i = 0; i < kMaxSegments; ++i)
            for (int j = 0; j < 4; ++j) {
                seg.feature_enabled[i][j] = false;
                seg.feature_data[i][j] = 0;
            }
        seg.abs_delta = false;
        for (auto& m : seg_map) std::fill(m.begin(), m.end(), static_cast<std::uint8_t>(0));
        lf.delta_enabled = true;
        lf.ref_deltas[0] = 1;
        lf.ref_deltas[1] = 0;
        lf.ref_deltas[2] = -1;
        lf.ref_deltas[3] = -1;
        lf.mode_deltas[0] = 0;
        lf.mode_deltas[1] = 0;
        ProbContext fc;
        default_probs(fc);
        if (h.frame_type == kKeyFrame || h.error_resilient_mode || h.reset_frame_context == 3) {
            for (auto& c : contexts) c = fc;
        } else if (h.reset_frame_context == 2) {
            contexts[h.frame_context_idx] = fc;
        }
        for (int i = 0; i < 4; ++i) h.ref_frame_sign_bias[i] = 0;
        h.frame_context_idx = 0;
    }

    void output(const Picture& p, double pts, bool keyframe) {
        VideoFrame f;
        const PixelFormat fmt = format_for(p.bit_depth, p.ssx, p.ssy);
        if (!f.allocate(static_cast<std::uint32_t>(p.width), static_cast<std::uint32_t>(p.height), fmt)) return;
        f.color = color_for(p.color_space, p.color_range, config);
        f.pts = pts;
        f.keyframe = keyframe;
        for (int pl = 0; pl < 3; ++pl) {
            const int w = p.plane_w[pl];
            const int h = p.plane_h[pl];
            const std::uint16_t* src = p.plane(pl);
            std::uint8_t* dst = f.plane(pl);
            if (p.bit_depth == 8) {
                for (int y = 0; y < h; ++y) {
                    const std::uint16_t* s = src + y * p.stride[pl];
                    std::uint8_t* d = dst + y * f.stride[pl];
                    for (int x = 0; x < w; ++x) d[x] = static_cast<std::uint8_t>(s[x]);
                }
            } else {
                for (int y = 0; y < h; ++y) {
                    const std::uint16_t* s = src + y * p.stride[pl];
                    std::uint8_t* d = dst + y * f.stride[pl];
                    for (int x = 0; x < w; ++x) {
                        d[2 * x] = static_cast<std::uint8_t>(s[x] & 0xFF);
                        d[2 * x + 1] = static_cast<std::uint8_t>(s[x] >> 8);
                    }
                }
            }
        }
        if (p.render_width > 0 && p.render_height > 0 && (p.render_width != p.width || p.render_height != p.height)) {
            const std::uint64_t num = static_cast<std::uint64_t>(p.render_width) * static_cast<std::uint64_t>(p.height);
            const std::uint64_t den = static_cast<std::uint64_t>(p.render_height) * static_cast<std::uint64_t>(p.width);
            std::uint64_t a = num;
            std::uint64_t b = den;
            while (b) {
                const std::uint64_t t = a % b;
                a = b;
                b = t;
            }
            if (a && num / a <= 0xFFFFFFFFu && den / a <= 0xFFFFFFFFu) {
                f.sar_num = static_cast<std::uint32_t>(num / a);
                f.sar_den = static_cast<std::uint32_t>(den / a);
            }
        }
        out.push_back(std::move(f));
    }

    void update_info(const FrameHeader& h, const Picture& p) {
        info.profile = h.profile;
        info.bit_depth = p.bit_depth;
        info.subsampling_x = p.ssx;
        info.subsampling_y = p.ssy;
        info.color_space = p.color_space;
        info.full_range = p.color_range != 0;
        info.width = static_cast<std::uint32_t>(p.width);
        info.height = static_cast<std::uint32_t>(p.height);
        info.render_width = static_cast<std::uint32_t>(p.render_width);
        info.render_height = static_cast<std::uint32_t>(p.render_height);
        info.tile_columns = 1 << h.tile_cols_log2;
        info.tile_rows = 1 << h.tile_rows_log2;
        info.format = format_for(p.bit_depth, p.ssx, p.ssy);
        info.color = color_for(p.color_space, p.color_range, config);
    }

    bool decode_frame(const std::uint8_t* data, std::size_t size, double pts, std::string* error);
};

bool Vp9Decoder::State::decode_frame(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    FrameHeader h;
    BitReader br(data, size);
    if (br.bits(2) != 2) return fail(error, "VP9: invalid frame marker");
    const int low = static_cast<int>(br.bit());
    const int high = static_cast<int>(br.bit());
    h.profile = (high << 1) | low;
    if (h.profile == 3 && br.bit()) return fail(error, "VP9: unsupported profile");
    h.show_existing_frame = br.bit() != 0;
    if (h.show_existing_frame) {
        const int idx = static_cast<int>(br.bits(3));
        if (br.failed() || !refs[idx]) return fail(error, "VP9: show_existing_frame refers to an empty slot");
        output(*refs[idx], pts, false);
        last_show_frame = true;
        ++frames;
        return true;
    }
    h.frame_type = static_cast<int>(br.bit());
    h.show_frame = br.bit() != 0;
    h.error_resilient_mode = br.bit() != 0;
    int width = 0;
    int height = 0;
    int render_w = 0;
    int render_h = 0;
    auto read_sync = [&]() { return br.bits(8) == 0x49 && br.bits(8) == 0x83 && br.bits(8) == 0x42; };
    auto read_frame_size = [&]() {
        width = static_cast<int>(br.bits(16)) + 1;
        height = static_cast<int>(br.bits(16)) + 1;
    };
    auto read_render_size = [&]() {
        if (br.bit()) {
            render_w = static_cast<int>(br.bits(16)) + 1;
            render_h = static_cast<int>(br.bits(16)) + 1;
        } else {
            render_w = width;
            render_h = height;
        }
    };
    if (h.frame_type == kKeyFrame) {
        if (!read_sync()) return fail(error, "VP9: invalid frame sync code");
        if (!read_color_config(br, h.profile, error)) return false;
        read_frame_size();
        read_render_size();
        h.refresh_frame_flags = 0xFF;
        h.frame_is_intra = true;
    } else {
        h.intra_only = h.show_frame ? false : (br.bit() != 0);
        h.reset_frame_context = h.error_resilient_mode ? 0 : static_cast<int>(br.bits(2));
        if (h.intra_only) {
            if (!read_sync()) return fail(error, "VP9: invalid frame sync code");
            if (h.profile > 0) {
                if (!read_color_config(br, h.profile, error)) return false;
            } else {
                bit_depth = 8;
                ssx = ssy = 1;
                color_space = 1;
                color_range = 0;
            }
            h.refresh_frame_flags = static_cast<int>(br.bits(8));
            read_frame_size();
            read_render_size();
            h.frame_is_intra = true;
        } else {
            h.refresh_frame_flags = static_cast<int>(br.bits(8));
            for (int i = 0; i < 3; ++i) {
                h.ref_frame_idx[i] = static_cast<int>(br.bits(3));
                h.ref_frame_sign_bias[kLastFrame + i] = static_cast<int>(br.bit());
            }
            bool found = false;
            for (int i = 0; i < 3; ++i) {
                if (br.bit()) {
                    const Picture* rp = refs[h.ref_frame_idx[i]].get();
                    if (!rp) return fail(error, "VP9: frame size refers to an empty reference");
                    width = rp->width;
                    height = rp->height;
                    found = true;
                    break;
                }
            }
            if (!found) read_frame_size();
            read_render_size();
            h.allow_high_precision_mv = br.bit() != 0;
            if (br.bit()) h.interp_filter = kFilterSwitchable;
            else h.interp_filter = k_literal_to_filter[br.bits(2)];
            h.frame_is_intra = false;
            if (!have_keyframe) return fail(error, "VP9: inter frame before the first keyframe");
        }
    }
    if (!h.error_resilient_mode) {
        h.refresh_frame_context = br.bit() != 0;
        h.frame_parallel_decoding_mode = br.bit() != 0;
    } else {
        h.refresh_frame_context = false;
        h.frame_parallel_decoding_mode = true;
    }
    h.frame_context_idx = static_cast<int>(br.bits(2));
    if (br.failed()) return fail(error, "VP9: truncated frame header");
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) return fail(error, "VP9: unsupported frame size");
    if (!h.frame_is_intra) {
        for (int i = 0; i < 3; ++i) {
            if (!refs[h.ref_frame_idx[i]]) return fail(error, "VP9: missing reference frame");
        }
    }
    const int new_mi_cols = (width + 7) >> 3;
    const int new_mi_rows = (height + 7) >> 3;
    if (new_mi_cols != seg_cols || new_mi_rows != seg_rows) {
        seg_cols = new_mi_cols;
        seg_rows = new_mi_rows;
        const std::size_t n = static_cast<std::size_t>(seg_cols) * static_cast<std::size_t>(seg_rows);
        seg_map[0].assign(n, 0);
        seg_map[1].assign(n, 0);
    }
    if (h.frame_is_intra || h.error_resilient_mode) setup_past_independence(h);

    lf.level = static_cast<int>(br.bits(6));
    lf.sharpness = static_cast<int>(br.bits(3));
    lf.delta_enabled = br.bit() != 0;
    if (lf.delta_enabled) {
        if (br.bit()) {
            for (int i = 0; i < 4; ++i)
                if (br.bit()) lf.ref_deltas[i] = br.signed_bits(6);
            for (int i = 0; i < 2; ++i)
                if (br.bit()) lf.mode_deltas[i] = br.signed_bits(6);
        }
    }
    h.base_q_idx = static_cast<int>(br.bits(8));
    auto delta_q = [&]() { return br.bit() ? br.signed_bits(4) : 0; };
    h.delta_q_y_dc = delta_q();
    h.delta_q_uv_dc = delta_q();
    h.delta_q_uv_ac = delta_q();
    h.lossless = h.base_q_idx == 0 && h.delta_q_y_dc == 0 && h.delta_q_uv_dc == 0 && h.delta_q_uv_ac == 0;

    seg.update_map = false;
    seg.update_data = false;
    seg.enabled = br.bit() != 0;
    if (seg.enabled) {
        seg.update_map = br.bit() != 0;
        if (seg.update_map) {
            for (int i = 0; i < 7; ++i) seg.tree_probs[i] = br.bit() ? static_cast<std::uint8_t>(br.bits(8)) : 255;
            seg.temporal_update = br.bit() != 0;
            for (int i = 0; i < 3; ++i) seg.pred_probs[i] = (seg.temporal_update && br.bit()) ? static_cast<std::uint8_t>(br.bits(8)) : 255;
        }
        seg.update_data = br.bit() != 0;
        if (seg.update_data) {
            seg.abs_delta = br.bit() != 0;
            for (int i = 0; i < kMaxSegments; ++i) {
                for (int j = 0; j < 4; ++j) {
                    int v = 0;
                    const bool en = br.bit() != 0;
                    seg.feature_enabled[i][j] = en;
                    if (en) {
                        v = static_cast<int>(br.bits(k_seg_feature_bits[j]));
                        if (j == kSegLvlAltL && v > kMaxLoopFilter) v = kMaxLoopFilter;
                        if (j == kSegLvlRefFrame && v > 3) v = 3;
                        if (k_seg_feature_signed[j] && br.bit()) v = -v;
                    }
                    seg.feature_data[i][j] = v;
                }
            }
        }
    }

    const int sb64_cols = (new_mi_cols + 7) >> 3;
    int min_log2 = 0;
    while ((kMaxTileWidthB64 << min_log2) < sb64_cols) ++min_log2;
    int max_log2 = 1;
    while ((sb64_cols >> max_log2) >= kMinTileWidthB64) ++max_log2;
    --max_log2;
    h.tile_cols_log2 = min_log2;
    while (h.tile_cols_log2 < max_log2 && br.bit()) ++h.tile_cols_log2;
    h.tile_rows_log2 = static_cast<int>(br.bit());
    if (h.tile_rows_log2) h.tile_rows_log2 += static_cast<int>(br.bit());
    h.header_size = br.bits(16);
    if (br.failed()) return fail(error, "VP9: truncated frame header");
    h.uncompressed_size = br.byte_pos();
    if (h.tile_cols_log2 > 6) return fail(error, "VP9: invalid number of tile columns");
    if (h.header_size == 0) return fail(error, "VP9: invalid compressed header size");
    if (h.uncompressed_size + h.header_size > size) return fail(error, "VP9: truncated compressed header");

    std::shared_ptr<Picture> cur;
    try {
        cur = get_picture();
        if (!cur->allocate(width, height, ssx, ssy, bit_depth)) return fail(error, "VP9: can't allocate the frame");
    } catch (const std::bad_alloc&) {
        return fail(error, "VP9: out of memory");
    }
    cur->color_space = color_space;
    cur->color_range = color_range;
    cur->render_width = render_w;
    cur->render_height = render_h;
    cur->intra_only = h.frame_is_intra;

    FrameDecodeContext ctx;
    ctx.hdr = &h;
    ctx.seg = &seg;
    ctx.lf = &lf;
    ctx.cur = cur.get();
    ctx.mi_cols = cur->mi_cols;
    ctx.mi_rows = cur->mi_rows;
    ctx.mi_stride = cur->mi_cols;
    ctx.ssx = ssx;
    ctx.ssy = ssy;
    ctx.bit_depth = bit_depth;
    h.interp_filter = h.frame_is_intra ? 0 : h.interp_filter;
    if (!h.frame_is_intra) {
        bool any_valid = false;
        for (int i = 0; i < 3; ++i) {
            const Picture* rp = refs[h.ref_frame_idx[i]].get();
            ctx.refs[i] = rp;
            ScaleFactors& sf = ctx.scale[i];
            if (rp->bit_depth != bit_depth || rp->ssx != ssx || rp->ssy != ssy) return fail(error, "VP9: reference frame has an incompatible color format");
            sf.valid = 2 * width >= rp->width && 2 * height >= rp->height && width <= 16 * rp->width && height <= 16 * rp->height;
            if (sf.valid) {
                any_valid = true;
                sf.x_scale = static_cast<int>((static_cast<std::int64_t>(rp->width) << 14) / width);
                sf.y_scale = static_cast<int>((static_cast<std::int64_t>(rp->height) << 14) / height);
                sf.x_step = (16 * sf.x_scale) >> 14;
                sf.y_step = (16 * sf.y_scale) >> 14;
                sf.scaled = sf.x_scale != (1 << 14) || sf.y_scale != (1 << 14);
            }
        }
        if (!any_valid) return fail(error, "VP9: referenced frames have invalid sizes");
    }
    const bool use_prev = !h.error_resilient_mode && prev_frame && width == last_width && height == last_height && last_show_frame && !prev_frame->intra_only &&
                          prev_frame->mi_cols == cur->mi_cols && prev_frame->mi_rows == cur->mi_rows;
    ctx.prev_mvs = use_prev ? prev_frame->mvs.data() : nullptr;
    ctx.prev_seg = seg_map[seg_cur ^ 1].data();
    ctx.cur_seg = seg_map[seg_cur].data();

    ProbContext fc = contexts[h.frame_context_idx];
    const ProbContext pre = fc;
    if (!read_compressed_header(data + h.uncompressed_size, h.header_size, h, fc)) return fail(error, "VP9: invalid compressed header");
    ctx.fc = &fc;

    for (int s = 0; s < kMaxSegments; ++s) {
        int q = h.base_q_idx;
        if (seg.active(s, kSegLvlAltQ)) {
            const int d = seg.feature_data[s][kSegLvlAltQ];
            q = iclamp(seg.abs_delta ? d : h.base_q_idx + d, 0, 255);
        }
        const int bi = bit_depth == 8 ? 0 : (bit_depth == 10 ? 1 : 2);
        ctx.dequant[s][0][0] = k_dc_qlookup[bi][iclamp(q + h.delta_q_y_dc, 0, 255)];
        ctx.dequant[s][0][1] = k_ac_qlookup[bi][iclamp(q, 0, 255)];
        ctx.dequant[s][1][0] = k_dc_qlookup[bi][iclamp(q + h.delta_q_uv_dc, 0, 255)];
        ctx.dequant[s][1][1] = k_ac_qlookup[bi][iclamp(q + h.delta_q_uv_ac, 0, 255)];
    }
    const int scale = 1 << (lf.level >> 5);
    for (int s = 0; s < kMaxSegments; ++s) {
        int lvl_seg = lf.level;
        if (seg.active(s, kSegLvlAltL)) {
            const int d = seg.feature_data[s][kSegLvlAltL];
            lvl_seg = iclamp(seg.abs_delta ? d : lf.level + d, 0, kMaxLoopFilter);
        }
        if (!lf.delta_enabled) {
            for (int r = 0; r < 4; ++r) ctx.lf_level[s][r][0] = ctx.lf_level[s][r][1] = static_cast<std::uint8_t>(lvl_seg);
        } else {
            const int intra_lvl = lvl_seg + lf.ref_deltas[kIntraFrame] * scale;
            ctx.lf_level[s][kIntraFrame][0] = ctx.lf_level[s][kIntraFrame][1] = static_cast<std::uint8_t>(iclamp(intra_lvl, 0, kMaxLoopFilter));
            for (int r = kLastFrame; r <= kAltrefFrame; ++r)
                for (int m = 0; m < 2; ++m)
                    ctx.lf_level[s][r][m] = static_cast<std::uint8_t>(iclamp(lvl_seg + lf.ref_deltas[r] * scale + lf.mode_deltas[m] * scale, 0, kMaxLoopFilter));
        }
    }
    ctx.counting = !h.error_resilient_mode && !h.frame_parallel_decoding_mode;

    try {
        const std::size_t mi_n = static_cast<std::size_t>(cur->mi_cols) * static_cast<std::size_t>(cur->mi_rows);
        if (mi.size() < mi_n) mi.resize(mi_n);
        const std::size_t cols4 = static_cast<std::size_t>(cur->mi_cols) * 2 + 64;
        for (int p = 0; p < 3; ++p) above_nonzero[p].assign(cols4, 0);
        above_partition.assign(static_cast<std::size_t>(cur->mi_cols) + 16, 0);
    } catch (const std::bad_alloc&) {
        return fail(error, "VP9: out of memory");
    }
    ctx.mi = mi.data();
    for (int p = 0; p < 3; ++p) ctx.above_nonzero[p] = above_nonzero[p].data();
    ctx.above_partition = above_partition.data();

    const int tile_cols = 1 << h.tile_cols_log2;
    const int tile_rows = 1 << h.tile_rows_log2;
    struct TileBuf {
        const std::uint8_t* data;
        std::size_t size;
    };
    std::vector<TileBuf> bufs(static_cast<std::size_t>(tile_cols * tile_rows));
    const std::uint8_t* p = data + h.uncompressed_size + h.header_size;
    const std::uint8_t* end = data + size;
    for (int tr = 0; tr < tile_rows; ++tr) {
        for (int tc = 0; tc < tile_cols; ++tc) {
            const bool last = tr == tile_rows - 1 && tc == tile_cols - 1;
            std::size_t n;
            if (last) {
                n = static_cast<std::size_t>(end - p);
            } else {
                if (end - p < 4) return fail(error, "VP9: truncated tile size");
                n = (static_cast<std::size_t>(p[0]) << 24) | (static_cast<std::size_t>(p[1]) << 16) | (static_cast<std::size_t>(p[2]) << 8) | p[3];
                p += 4;
                if (n > static_cast<std::size_t>(end - p)) return fail(error, "VP9: truncated tile data");
            }
            bufs[static_cast<std::size_t>(tr * tile_cols + tc)] = TileBuf{p, n};
            p += n;
        }
    }
    auto tile_offset = [](int idx, int mis, int log2) {
        const int sbs = (mis + 7) >> 3;
        const int off = ((idx * sbs) >> log2) << 3;
        return imin(off, mis);
    };
    while (static_cast<int>(tiles.size()) < tile_cols) tiles.emplace_back(new TileDecoder());
    for (int tc = 0; tc < tile_cols; ++tc) {
        tiles[static_cast<std::size_t>(tc)]->counts.clear();
        tiles[static_cast<std::size_t>(tc)]->corrupted = false;
    }
    auto run_column = [&](int tc) {
        TileDecoder& td = *tiles[static_cast<std::size_t>(tc)];
        const int c0 = tile_offset(tc, cur->mi_cols, h.tile_cols_log2);
        const int c1 = tile_offset(tc + 1, cur->mi_cols, h.tile_cols_log2);
        for (int tr = 0; tr < tile_rows; ++tr) {
            const int r0 = tile_offset(tr, cur->mi_rows, h.tile_rows_log2);
            const int r1 = tile_offset(tr + 1, cur->mi_rows, h.tile_rows_log2);
            if (c0 >= c1 || r0 >= r1) continue;
            const TileBuf& b = bufs[static_cast<std::size_t>(tr * tile_cols + tc)];
            td.decode(ctx, b.data, b.size, r0, r1, c0, c1);
        }
    };
    const int workers = imin(threads, tile_cols);
    if (workers > 1) {
        std::vector<std::thread> pool_threads;
        std::atomic<int> next(0);
        auto worker = [&]() {
            for (;;) {
                const int tc = next.fetch_add(1);
                if (tc >= tile_cols) break;
                run_column(tc);
            }
        };
        try {
            for (int i = 1; i < workers; ++i) pool_threads.emplace_back(worker);
        } catch (...) {
        }
        worker();
        for (auto& t : pool_threads) t.join();
    } else {
        for (int tc = 0; tc < tile_cols; ++tc) run_column(tc);
    }
    bool corrupted = false;
    Counts* total = nullptr;
    for (int tc = 0; tc < tile_cols; ++tc) {
        TileDecoder& td = *tiles[static_cast<std::size_t>(tc)];
        corrupted = corrupted || td.corrupted;
        if (!total) total = &td.counts;
        else total->add(td.counts);
    }

    if (lf.level) loop_filter_frame(*cur, mi.data(), cur->mi_cols, ctx.lf_level, lf.sharpness, threads);

    if (!h.error_resilient_mode && !h.frame_parallel_decoding_mode && total) {
        adapt_coef_probs(fc, pre, *total, h.frame_is_intra, last_frame_type == kKeyFrame);
        if (!h.frame_is_intra) {
            adapt_mode_probs(fc, pre, *total, h.interp_filter == kFilterSwitchable, h.tx_mode == kTxModeSelect);
            adapt_mv_probs(fc, pre, *total, h.allow_high_precision_mv);
        }
    }
    if (h.refresh_frame_context) contexts[h.frame_context_idx] = fc;
    for (int i = 0; i < kNumRefFrames; ++i) {
        if (h.refresh_frame_flags & (1 << i)) refs[i] = cur;
    }
    if (h.frame_type == kKeyFrame) have_keyframe = true;
    if (h.intra_only) have_keyframe = true;
    prev_frame = cur;
    last_width = width;
    last_height = height;
    last_show_frame = h.show_frame;
    last_frame_type = h.frame_type;
    if (seg.enabled) seg_cur ^= 1;
    interp_filter = h.interp_filter;
    update_info(h, *cur);
    if (h.show_frame) {
        output(*cur, pts, h.frame_type == kKeyFrame);
        ++frames;
    }
    if (corrupted) return fail(error, "VP9: corrupt tile data");
    return true;
}

bool parse_vp9_codec_config(const std::uint8_t* data, std::size_t size, Vp9CodecConfig& out) noexcept {
    out = Vp9CodecConfig();
    if (!data || size == 0) return false;
    if (size >= 12 && data[0] == 1 && data[1] == 0 && data[2] == 0 && data[3] == 0) {
        out.profile = data[4];
        out.level = data[5];
        out.bit_depth = data[6] >> 4;
        out.chroma_subsampling = (data[6] >> 1) & 7;
        out.full_range = data[6] & 1;
        out.primaries = data[7];
        out.transfer = data[8];
        out.matrix = data[9];
        return true;
    }
    if (size >= 8 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0) {
        out.profile = data[4];
        out.level = data[5];
        out.bit_depth = data[6] >> 4;
        const int cs = data[6] & 15;
        out.matrix = cs;
        out.chroma_subsampling = data[7] >> 4;
        out.full_range = data[7] & 1;
        return true;
    }
    std::size_t pos = 0;
    bool any = false;
    while (pos + 2 <= size) {
        const int id = data[pos];
        const std::size_t len = data[pos + 1];
        pos += 2;
        if (pos + len > size) return any;
        if (len >= 1) {
            if (id == 1) out.profile = data[pos];
            else if (id == 2) out.level = data[pos];
            else if (id == 3) out.bit_depth = data[pos];
            else if (id == 4) out.chroma_subsampling = data[pos];
            any = true;
        }
        pos += len;
    }
    return any;
}

std::size_t vp9_split_superframe(const std::uint8_t* data, std::size_t size, std::size_t* offsets, std::size_t* sizes, std::size_t max_frames) noexcept {
    if (!data || size == 0 || max_frames == 0) return 0;
    const std::uint8_t marker = data[size - 1];
    if ((marker & 0xE0) == 0xC0) {
        const std::size_t frames = static_cast<std::size_t>(marker & 7) + 1;
        const std::size_t mag = static_cast<std::size_t>((marker >> 3) & 3) + 1;
        const std::size_t index_size = 2 + mag * frames;
        if (size >= index_size && data[size - index_size] == marker) {
            const std::uint8_t* x = data + size - index_size + 1;
            std::size_t pos = 0;
            std::size_t count = 0;
            for (std::size_t i = 0; i < frames; ++i) {
                std::size_t fs = 0;
                for (std::size_t b = 0; b < mag; ++b) fs |= static_cast<std::size_t>(x[b]) << (8 * b);
                x += mag;
                if (fs > size - index_size - pos) break;
                if (fs > 0 && count < max_frames) {
                    offsets[count] = pos;
                    sizes[count] = fs;
                    ++count;
                }
                pos += fs;
            }
            return count;
        }
    }
    offsets[0] = 0;
    sizes[0] = size;
    return 1;
}

namespace {

bool frame_refreshes(const std::uint8_t* data, std::size_t size, bool* keyframe, bool* shown) noexcept {
    BitReader br(data, size);
    if (br.bits(2) != 2) return true;
    const int low = static_cast<int>(br.bit());
    const int high = static_cast<int>(br.bit());
    const int profile = (high << 1) | low;
    if (profile == 3) br.bit();
    if (br.bit()) {
        br.bits(3);
        if (shown) *shown = true;
        return false;
    }
    const int frame_type = static_cast<int>(br.bit());
    const bool show = br.bit() != 0;
    const bool err = br.bit() != 0;
    if (shown) *shown = show;
    if (frame_type == kKeyFrame) {
        if (keyframe) *keyframe = true;
        return true;
    }
    const bool intra_only = show ? false : (br.bit() != 0);
    if (!err) br.bits(2);
    if (intra_only) return true;
    const unsigned refresh = br.bits(8);
    if (br.failed()) return true;
    return refresh != 0;
}

} // namespace

bool vp9_is_keyframe(const std::uint8_t* data, std::size_t size) noexcept {
    std::size_t offs[8];
    std::size_t sizes[8];
    const std::size_t n = vp9_split_superframe(data, size, offs, sizes, 8);
    for (std::size_t i = 0; i < n; ++i) {
        bool key = false;
        frame_refreshes(data + offs[i], sizes[i], &key, nullptr);
        if (key) return true;
    }
    return false;
}

bool vp9_is_disposable(const std::uint8_t* data, std::size_t size) noexcept {
    std::size_t offs[8];
    std::size_t sizes[8];
    const std::size_t n = vp9_split_superframe(data, size, offs, sizes, 8);
    if (n == 0) return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (frame_refreshes(data + offs[i], sizes[i], nullptr, nullptr)) return false;
    }
    return true;
}

Vp9Decoder::Vp9Decoder() = default;
Vp9Decoder::~Vp9Decoder() = default;
Vp9Decoder::Vp9Decoder(Vp9Decoder&&) noexcept = default;
Vp9Decoder& Vp9Decoder::operator=(Vp9Decoder&&) noexcept = default;

bool Vp9Decoder::open(const std::uint8_t* config, std::size_t size, std::string* error) {
    (void)error;
    int threads = 1;
    if (m_state) threads = m_state->threads;
    try {
        m_state.reset(new State());
    } catch (const std::bad_alloc&) {
        if (error) *error = "VP9: out of memory";
        return false;
    }
    m_state->threads = threads;
    if (config && size) parse_vp9_codec_config(config, size, m_state->config);
    m_state->opened = true;
    return true;
}

bool Vp9Decoder::decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (!m_state || !m_state->opened) {
        if (error) *error = "VP9 decoder is not open";
        return false;
    }
    if (!data || size == 0) return true;
    std::size_t offs[8];
    std::size_t sizes[8];
    const std::size_t n = vp9_split_superframe(data, size, offs, sizes, 8);
    bool ok = true;
    for (std::size_t i = 0; i < n; ++i) {
        std::string err;
        bool r;
        try {
            r = m_state->decode_frame(data + offs[i], sizes[i], pts, &err);
        } catch (const std::bad_alloc&) {
            ++m_state->errors;
            err = "VP9: out of memory";
            r = false;
        }
        if (!r) {
            ok = false;
            if (error && error->empty()) *error = err;
        }
    }
    return ok;
}

bool Vp9Decoder::receive(VideoFrame& out) {
    if (!m_state || m_state->out.empty()) return false;
    out = std::move(m_state->out.front());
    m_state->out.pop_front();
    return true;
}

void Vp9Decoder::flush() {}

void Vp9Decoder::reset() {
    if (m_state) m_state->reset_state();
}

void Vp9Decoder::close() {
    m_state.reset();
}

bool Vp9Decoder::is_open() const noexcept {
    return m_state && m_state->opened;
}

void Vp9Decoder::set_threads(int threads) noexcept {
    if (threads < 1) threads = 1;
    if (threads > 64) threads = 64;
    if (!m_state) {
        try {
            m_state.reset(new State());
        } catch (...) {
            return;
        }
    }
    m_state->threads = threads;
}

int Vp9Decoder::threads() const noexcept {
    return m_state ? m_state->threads : 1;
}

const Vp9Info& Vp9Decoder::info() const noexcept {
    static const Vp9Info empty;
    return m_state ? m_state->info : empty;
}

std::uint64_t Vp9Decoder::frames_decoded() const noexcept {
    return m_state ? m_state->frames : 0;
}

std::uint64_t Vp9Decoder::error_count() const noexcept {
    return m_state ? m_state->errors : 0;
}

} // namespace video
} // namespace fizmo
