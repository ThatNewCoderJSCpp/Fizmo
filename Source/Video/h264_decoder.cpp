#include "h264_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fizmo {
namespace video {
namespace h264_internal {

bool Picture::allocate(int mbw, int mbh, int chroma_format_idc) {
    return allocate(mbw, mbh, chroma_format_idc, 1, 1);
}

bool Picture::allocate(int mbw, int mbh, int chroma_format_idc, int bytes, int motion_planes) {
    int w = mbw * 16;
    int h = mbh * 16;
    int cw = chroma_format_idc == 3 ? w : w / 2;
    int ch = (chroma_format_idc == 3 || chroma_format_idc == 2) ? h : h / 2;
    bool same = !buffer.empty() && w == width && h == height && chroma_format == chroma_format_idc && pixel_bytes == bytes && planes == motion_planes;
    width = w;
    height = h;
    chroma_width = cw;
    chroma_height = ch;
    mb_width = mbw;
    mb_height = mbh;
    chroma_format = chroma_format_idc;
    pixel_bytes = bytes;
    planes = motion_planes;
    pad[0] = k_pad_luma;
    pad[1] = pad[2] = chroma_format_idc == 3 ? k_pad_luma : k_pad_chroma;
    if (!same) {
        int ls = (w + 2 * pad[0] + 31) & ~31;
        int cs = (cw + 2 * pad[1] + 31) & ~31;
        std::size_t luma_size = static_cast<std::size_t>(ls) * static_cast<std::size_t>(h + 2 * pad[0]);
        std::size_t chroma_size = static_cast<std::size_t>(cs) * static_cast<std::size_t>(ch + 2 * pad[1]);
        std::size_t total = (luma_size + 2 * chroma_size) * static_cast<std::size_t>(bytes);
        buffer.assign(total + 64, 0);
        std::size_t base = (64 - (reinterpret_cast<std::uintptr_t>(buffer.data()) & 63)) & 63;
        if (base + total > buffer.size()) base = 0;
        stride[0] = ls;
        stride[1] = stride[2] = cs;
        std::size_t b = static_cast<std::size_t>(bytes);
        plane[0] = buffer.data() + base + (static_cast<std::size_t>(pad[0]) * ls + pad[0]) * b;
        plane[1] = buffer.data() + base + (luma_size + static_cast<std::size_t>(pad[1]) * cs + pad[1]) * b;
        plane[2] = plane[1] + chroma_size * b;
        std::size_t mbs = static_cast<std::size_t>(mbw) * static_cast<std::size_t>(mbh) * static_cast<std::size_t>(motion_planes);
        for (int l = 0; l < 2; ++l) {
            mv[l].assign(mbs * 16, Mv{0, 0});
            ref_idx[l].assign(mbs * 16, -1);
            ref_uid[l].assign(mbs * 4, -1);
        }
        mb_intra.assign(mbs, 1);
        mb_field.assign(mbs, 0);
    }
    return true;
}

namespace {

template <typename P>
void extend_plane(std::uint8_t* raw, int s, int w, int h, int pad) {
    P* base = reinterpret_cast<P*>(raw);
    for (int y = 0; y < h; ++y) {
        P* row = base + static_cast<std::ptrdiff_t>(y) * s;
        P l = row[0];
        P r = row[w - 1];
        for (int x = 1; x <= pad; ++x) {
            row[-x] = l;
            row[w - 1 + x] = r;
        }
    }
    std::size_t bytes = static_cast<std::size_t>(w + 2 * pad) * sizeof(P);
    for (int y = 1; y <= pad; ++y) {
        std::memcpy(base - static_cast<std::ptrdiff_t>(y) * s - pad, base - pad, bytes);
        std::memcpy(base + static_cast<std::ptrdiff_t>(h - 1 + y) * s - pad, base + static_cast<std::ptrdiff_t>(h - 1) * s - pad, bytes);
    }
}

} // namespace

void Picture::extend_borders() {
    for (int p = 0; p < 3; ++p) {
        int w = p == 0 ? width : chroma_width;
        int h = p == 0 ? height : chroma_height;
        if (pixel_bytes == 2) {
            extend_plane<std::uint16_t>(plane[p], stride[p], w, h, pad[p]);
        } else {
            extend_plane<std::uint8_t>(plane[p], stride[p], w, h, pad[p]);
        }
    }
}

void Picture::sync_ref() noexcept {
    short_ref = field_ref[0] == 1 || field_ref[1] == 1;
    long_ref = field_ref[0] == 2 || field_ref[1] == 2;
}

std::int32_t Picture::structure_poc(int s) const noexcept {
    if (s == k_struct_top) return top_poc;
    if (s == k_struct_bottom) return bottom_poc;
    return poc;
}

PicView Picture::view(int s) const noexcept {
    PicView v;
    for (int p = 0; p < 3; ++p) {
        v.plane[p] = plane[p];
        v.stride[p] = stride[p];
        v.pad[p] = pad[p];
        if (s != k_struct_frame) {
            if (s == k_struct_bottom) v.plane[p] += static_cast<std::size_t>(stride[p]) * static_cast<std::size_t>(pixel_bytes);
            v.stride[p] = stride[p] * 2;
        }
    }
    v.width = width;
    v.chroma_width = chroma_width;
    v.height = s == k_struct_frame ? height : height / 2;
    v.chroma_height = s == k_struct_frame ? chroma_height : chroma_height / 2;
    v.vpad = s == k_struct_frame;
    return v;
}

std::size_t Picture::motion_base(int p, int s) const noexcept {
    std::size_t n = static_cast<std::size_t>(mb_width) * static_cast<std::size_t>(mb_height);
    std::size_t base = static_cast<std::size_t>(p) * n;
    if (s == k_struct_bottom) base += n / 2;
    return base;
}

DecoderState::DecoderState() {
    for (int l = 0; l < 2; ++l) {
        for (int i = 0; i < 32; ++i) {
            ref_list[l][i] = nullptr;
            ref_struct[l][i] = k_struct_frame;
        }
    }
    std::memset(implicit_w, 0, sizeof(implicit_w));
    std::memset(implicit_wf, 0, sizeof(implicit_wf));
    std::memset(cabac_state, 0, sizeof(cabac_state));
    std::memset(&res, 0, sizeof(res));
}

namespace {

int max_dpb_mbs(int level, int constraint_flags, int profile) {
    if (level == 11 && (constraint_flags & 0x10) && (profile == 66 || profile == 77 || profile == 88)) return 396;
    switch (level) {
    case 9:
    case 10: return 396;
    case 11: return 900;
    case 12:
    case 13:
    case 20: return 2376;
    case 21: return 4752;
    case 22:
    case 30: return 8100;
    case 31: return 18000;
    case 32: return 20480;
    case 40:
    case 41: return 32768;
    case 42: return 34816;
    case 50: return 110400;
    case 51:
    case 52: return 184320;
    default: return 696320;
    }
}

ColorMatrix matrix_from_vui(int mc) {
    switch (mc) {
    case 0: return ColorMatrix::rgb;
    case 1: return ColorMatrix::bt709;
    case 5:
    case 6: return ColorMatrix::bt601;
    case 9:
    case 10: return ColorMatrix::bt2020;
    default: return ColorMatrix::unspecified;
    }
}

int output_bit_depth(int d) {
    if (d <= 8) return 8;
    if (d <= 10) return 10;
    if (d <= 12) return 12;
    return 14;
}

template <typename P>
void fill_plane(std::uint8_t* raw, int s, int w, int h, int pad, int value) {
    P* base = reinterpret_cast<P*>(raw);
    for (int y = -pad; y < h + pad; ++y) {
        P* row = base + static_cast<std::ptrdiff_t>(y) * s;
        for (int x = -pad; x < w + pad; ++x) row[x] = static_cast<P>(value);
    }
}

void fill_picture_gray(Picture* p) {
    for (int c = 0; c < 3; ++c) {
        int w = c == 0 ? p->width : p->chroma_width;
        int h = c == 0 ? p->height : p->chroma_height;
        int bd = c == 0 ? p->bit_depth_luma : p->bit_depth_chroma;
        int v = 1 << (bd - 1);
        if (p->pixel_bytes == 2) {
            fill_plane<std::uint16_t>(p->plane[c], p->stride[c], w, h, p->pad[c], v);
        } else {
            fill_plane<std::uint8_t>(p->plane[c], p->stride[c], w, h, p->pad[c], v);
        }
    }
}

} // namespace

void DecoderState::plane_sizes_for(int s_chroma_format, int s_separate) {
    chroma_array_type = s_separate ? 0 : s_chroma_format;
    if (s_chroma_format == 1) {
        sub_w = 2;
        sub_h = 2;
    } else if (s_chroma_format == 2) {
        sub_w = 2;
        sub_h = 1;
    } else {
        sub_w = 1;
        sub_h = 1;
    }
    mb_wc = chroma_array_type == 0 ? 0 : 16 / sub_w;
    mb_hc = chroma_array_type == 0 ? 0 : 16 / sub_h;
}

bool DecoderState::setup_geometry(const Sps& s) {
    int bytes = (s.bit_depth_luma > 8 || s.bit_depth_chroma > 8) ? 2 : 1;
    int planes = s.separate_colour_plane ? 3 : 1;
    bool changed = s.mb_width != mb_width || s.mb_height != mb_height || !cur_geometry_valid || s.chroma_format_idc != cur_chroma_format ||
                   bytes != pixel_bytes || planes != num_planes || s.bit_depth_luma != bit_depth_y || s.bit_depth_chroma != bit_depth_c;
    int dpb_mbs = max_dpb_mbs(s.level_idc, s.constraint_flags, s.profile_idc);
    int frames = std::min(16, std::max(1, dpb_mbs / std::max(1, s.mb_width * s.mb_height)));
    if (s.vui.bitstream_restriction) frames = std::max(1, s.vui.max_dec_frame_buffering);
    frames = std::max(frames, std::max(1, s.max_num_ref_frames));
    frames = std::min(frames, 16);
    int reorder;
    bool intra_only = (s.constraint_flags & 0x10) && (s.profile_idc == 44 || s.profile_idc == 86 || s.profile_idc == 100 || s.profile_idc == 110 || s.profile_idc == 122 || s.profile_idc == 244);
    if (s.vui.bitstream_restriction) {
        reorder = s.vui.max_num_reorder_frames;
    } else if (s.poc_type == 2 || intra_only) {
        reorder = 0;
    } else {
        reorder = frames;
    }
    reorder = std::min(reorder, frames);
    if (changed) {
        flush_all();
        for (Picture* p : dpb) {
            p->field_ref[0] = p->field_ref[1] = 0;
            p->sync_ref();
            p->needs_output = false;
            p->awaiting_field = false;
        }
        dpb.clear();
        pending_field = nullptr;
        pool.clear();
        gray_pic.reset();
        mb_width = s.mb_width;
        mb_height = s.mb_height;
        mb_count = mb_width * mb_height;
        b4_stride = mb_width * 4;
        pixel_bytes = bytes;
        num_planes = planes;
        bit_depth_y = s.bit_depth_luma;
        bit_depth_c = s.bit_depth_chroma;
        std::size_t n = static_cast<std::size_t>(mb_count);
        active_plane = 0;
        for (int p = 0; p < 3; ++p) {
            PlaneStore& st = plane_store[p];
            if (p < planes && p > 0) {
                st.mbs.assign(n, MbInfo());
                st.nnz.assign(n * 48, 0);
                st.intra_modes.assign(n * 16, 2);
                st.mvd[0].assign(n * 32, 0);
                st.mvd[1].assign(n * 32, 0);
                st.mb_done.assign(n, 0);
                st.slices.clear();
            } else {
                st = PlaneStore();
            }
        }
        mbs.assign(n, MbInfo());
        nnz.assign(n * 48, 0);
        nnz_c[0].assign(n * 4, 0);
        nnz_c[1].assign(n * 4, 0);
        intra_modes.assign(n * 16, 2);
        mvd[0].assign(n * 32, 0);
        mvd[1].assign(n * 32, 0);
        mb_done.assign(n, 0);
        cur_geometry_valid = true;
        cur_chroma_format = s.chroma_format_idc;
    }
    plane_sizes_for(s.chroma_format_idc, s.separate_colour_plane ? 1 : 0);
    qp_bd_y = 6 * (s.bit_depth_luma - 8);
    qp_bd_c = 6 * (s.bit_depth_chroma - 8);
    dpb_size = frames;
    reorder_frames = reorder;
    info.profile_idc = s.profile_idc;
    info.level_idc = s.level_idc;
    info.chroma_format_idc = s.chroma_format_idc;
    info.bit_depth = s.bit_depth_luma;
    info.bit_depth_chroma = s.bit_depth_chroma;
    info.separate_colour_plane = s.separate_colour_plane;
    info.lossless = s.transform_bypass;
    info.mbaff = s.mb_adaptive_frame_field;
    info.frame_mbs_only = s.frame_mbs_only;
    int out_depth = output_bit_depth(s.chroma_format_idc == 0 ? s.bit_depth_luma : std::max(s.bit_depth_luma, s.bit_depth_chroma));
    info.format = pixel_format_for(s.chroma_format_idc, out_depth);
    info.coded_width = static_cast<std::uint32_t>(s.mb_width * 16);
    info.coded_height = static_cast<std::uint32_t>(s.mb_height * 16);
    info.width = info.coded_width - static_cast<std::uint32_t>(s.crop_left + s.crop_right);
    info.height = info.coded_height - static_cast<std::uint32_t>(s.crop_top + s.crop_bottom);
    info.progressive = s.frame_mbs_only;
    info.max_num_reorder_frames = s.vui.bitstream_restriction ? s.vui.max_num_reorder_frames : -1;
    info.dpb_frames = frames;
    if (s.vui.aspect_ratio_present && s.vui.sar_width && s.vui.sar_height) {
        info.sar_num = s.vui.sar_width;
        info.sar_den = s.vui.sar_height;
    } else {
        info.sar_num = info.sar_den = 1;
    }
    if (s.vui.timing_present && s.vui.num_units_in_tick) {
        info.frame_rate = static_cast<double>(s.vui.time_scale) / (2.0 * s.vui.num_units_in_tick);
    } else {
        info.frame_rate = 0.0;
    }
    info.color = ColorInfo();
    if (s.vui.video_signal_present) {
        info.color.range = s.vui.full_range ? ColorRange::full : ColorRange::limited;
        if (s.vui.colour_description_present) {
            info.color.matrix = matrix_from_vui(s.vui.matrix_coefficients);
            info.color.primaries = static_cast<std::uint8_t>(s.vui.colour_primaries);
            info.color.transfer = static_cast<std::uint8_t>(s.vui.transfer_characteristics);
            info.color.matrix_coefficients = static_cast<std::uint8_t>(s.vui.matrix_coefficients);
        }
    }
    if (s.vui.chroma_loc_present) info.color.chroma_location = static_cast<std::uint8_t>(s.vui.chroma_loc_top);
    return true;
}

Picture* DecoderState::alloc_picture() {
    Picture* found = nullptr;
    for (auto& p : pool) {
        bool used = p.get() == cur || p.get() == gray_pic.get() || p.get() == pending_field;
        if (!used) {
            for (Picture* d : dpb) {
                if (d == p.get()) {
                    used = true;
                    break;
                }
            }
        }
        if (!used) {
            found = p.get();
            break;
        }
    }
    if (!found) {
        std::size_t samples = static_cast<std::size_t>(256 + 2 * std::max(64, mb_wc * mb_hc));
        std::size_t per_picture = static_cast<std::size_t>(mb_count) * (samples * static_cast<std::size_t>(pixel_bytes) * 3 / 2 + 256 * static_cast<std::size_t>(num_planes));
        std::size_t budget = static_cast<std::size_t>(1536) * 1024 * 1024;
        if (pool.size() >= static_cast<std::size_t>(dpb_size) + 4 || (pool.size() + 1) * per_picture > budget) return nullptr;
        pool.push_back(std::unique_ptr<Picture>(new Picture()));
        found = pool.back().get();
    }
    found->allocate(mb_width, mb_height, sps->chroma_format_idc, pixel_bytes, num_planes);
    found->bit_depth_luma = sps->bit_depth_luma;
    found->bit_depth_chroma = sps->bit_depth_chroma;
    found->field_ref[0] = found->field_ref[1] = 0;
    found->sync_ref();
    found->needs_output = false;
    found->non_existing = false;
    found->idr = false;
    found->mmco5 = false;
    found->corrupt = false;
    found->field_done[0] = found->field_done[1] = false;
    found->field_pic_coded = false;
    found->awaiting_field = false;
    found->coding = k_coding_frame;
    found->interlaced_hint = false;
    found->pic_struct = -1;
    found->uid = next_uid++;
    if (next_uid > 0x1FFFFFFF) next_uid = 1;
    found->decode_order = ++decode_counter;
    found->crop_left = sps->crop_left;
    found->crop_right = sps->crop_right;
    found->crop_top = sps->crop_top;
    found->crop_bottom = sps->crop_bottom;
    found->sar_num = info.sar_num;
    found->sar_den = info.sar_den;
    found->color = info.color;
    return found;
}

void DecoderState::release_unused() {
    std::size_t j = 0;
    for (std::size_t i = 0; i < dpb.size(); ++i) {
        Picture* p = dpb[i];
        if (p->is_ref() || p->needs_output || p->awaiting_field) dpb[j++] = p;
    }
    dpb.resize(j);
}

template <typename P>
void DecoderState::output_t(Picture* pic, VideoFrame& f) {
    std::uint32_t w = static_cast<std::uint32_t>(pic->width - pic->crop_left - pic->crop_right);
    std::uint32_t h = static_cast<std::uint32_t>(pic->height - pic->crop_top - pic->crop_bottom);
    int cf = pic->chroma_format;
    int depth = output_bit_depth(cf == 0 ? pic->bit_depth_luma : std::max(pic->bit_depth_luma, pic->bit_depth_chroma));
    PixelFormat fmt = pixel_format_for(cf, depth);
    if (!f.allocate(w, h, fmt)) return;
    int planes = cf == 0 ? 1 : 3;
    int sx = cf == 3 ? 0 : 1;
    int sy = cf == 1 ? 1 : 0;
    const std::uint16_t probe = 1;
    std::uint8_t probe_low = 0;
    std::memcpy(&probe_low, &probe, 1);
    bool little = probe_low == 1;
    for (int c = 0; c < planes; ++c) {
        int shift = depth - (c == 0 ? pic->bit_depth_luma : pic->bit_depth_chroma);
        std::uint32_t pw = f.plane_width(c);
        std::uint32_t ph = f.plane_height(c);
        int ox = c == 0 ? pic->crop_left : (pic->crop_left >> sx);
        int oy = c == 0 ? pic->crop_top : (pic->crop_top >> sy);
        const P* src = reinterpret_cast<const P*>(pic->plane[c]);
        for (std::uint32_t y = 0; y < ph; ++y) {
            const P* row = src + static_cast<std::ptrdiff_t>(static_cast<int>(y) + oy) * pic->stride[c] + ox;
            if (sizeof(P) == 1 || (little && shift == 0)) {
                std::memcpy(f.plane(c) + f.stride[c] * y, row, pw * sizeof(P));
            } else {
                std::uint8_t* d = f.plane(c) + f.stride[c] * y;
                for (std::uint32_t x = 0; x < pw; ++x) {
                    unsigned v = static_cast<unsigned>(row[x]) << shift;
                    d[2 * x] = static_cast<std::uint8_t>(v & 0xFF);
                    d[2 * x + 1] = static_cast<std::uint8_t>(v >> 8);
                }
            }
        }
    }
}

void DecoderState::output_picture(Picture* pic) {
    pic->needs_output = false;
    if (pic->non_existing) return;
    VideoFrame f;
    if (pic->pixel_bytes == 2) {
        output_t<std::uint16_t>(pic, f);
    } else {
        output_t<std::uint8_t>(pic, f);
    }
    if (f.empty()) return;
    f.color = pic->color;
    f.sar_num = pic->sar_num;
    f.sar_den = pic->sar_den;
    f.pts = pic->pts;
    f.keyframe = pic->keyframe;
    int ps = pic->pic_struct;
    bool both = pic->field_done[0] && pic->field_done[1];
    if (pic->coding == k_coding_fields) {
        if (both) {
            f.structure = PictureStructure::field_pair;
        } else {
            f.structure = pic->field_done[0] ? PictureStructure::top_field : PictureStructure::bottom_field;
        }
    } else if (pic->coding == k_coding_mbaff) {
        f.structure = PictureStructure::mbaff_frame;
    } else {
        f.structure = PictureStructure::frame;
    }
    f.interlaced = pic->coding != k_coding_frame || (ps >= 1 && ps <= 4) || pic->interlaced_hint;
    if (ps == 3 || ps == 5) {
        f.top_field_first = true;
    } else if (ps == 4 || ps == 6) {
        f.top_field_first = false;
    } else if (pic->top_poc != pic->bottom_poc) {
        f.top_field_first = pic->top_poc < pic->bottom_poc;
    } else if (pic->coding == k_coding_fields) {
        f.top_field_first = pic->first_field_parity == 0;
    } else {
        f.top_field_first = true;
    }
    f.repeat_fields = (ps == 5 || ps == 6) ? 1 : (ps == 7 ? 2 : (ps == 8 ? 4 : 0));
    output.push_back(std::move(f));
    ++frames_decoded;
}

void DecoderState::bump_one() {
    Picture* best = nullptr;
    for (Picture* p : dpb) {
        if (!p->needs_output) continue;
        if (!best || p->poc < best->poc) best = p;
    }
    if (best) output_picture(best);
}

void DecoderState::bump_all() {
    while (true) {
        bool any = false;
        for (Picture* p : dpb) {
            if (p->needs_output) {
                any = true;
                break;
            }
        }
        if (!any) break;
        bump_one();
    }
    release_unused();
}

void DecoderState::store_picture(Picture* pic) {
    bool incomplete = pic->awaiting_field;
    if (pic->idr || pic->mmco5) {
        if (pic->idr && cur_first_header.no_output_of_prior_pics) {
            for (Picture* p : dpb) {
                if (p != pic) p->needs_output = false;
            }
        } else {
            bump_all();
        }
    }
    release_unused();
    if (!incomplete && !pic->is_ref() && !pic->non_existing && static_cast<int>(dpb.size()) >= dpb_size) {
        bool lowest = true;
        for (Picture* p : dpb) {
            if (p->needs_output && p->poc <= pic->poc) {
                lowest = false;
                break;
            }
        }
        if (lowest) {
            output_picture(pic);
            return;
        }
    }
    while (static_cast<int>(dpb.size()) >= dpb_size) {
        bool waiting = false;
        for (Picture* p : dpb) {
            if (p->needs_output) {
                waiting = true;
                break;
            }
        }
        if (!waiting) break;
        bump_one();
        release_unused();
    }
    if (static_cast<int>(dpb.size()) >= dpb_size + 1) {
        Picture* oldest = nullptr;
        for (Picture* p : dpb) {
            if (p->short_ref && (!oldest || p->frame_num_wrap < oldest->frame_num_wrap)) oldest = p;
        }
        if (oldest) {
            for (int f = 0; f < 2; ++f) {
                if (oldest->field_ref[f] == 1) oldest->field_ref[f] = 0;
            }
            oldest->sync_ref();
            release_unused();
        }
    }
    pic->needs_output = !pic->non_existing && !incomplete;
    bool present = false;
    for (Picture* p : dpb) {
        if (p == pic) present = true;
    }
    if (!present && (pic->is_ref() || pic->needs_output || incomplete)) dpb.push_back(pic);
    if (incomplete) return;
    int waiting = 0;
    for (Picture* p : dpb) {
        if (p->needs_output) ++waiting;
    }
    while (waiting > reorder_frames) {
        bump_one();
        --waiting;
    }
    release_unused();
}

void DecoderState::complete_frame(Picture* pic) {
    pic->awaiting_field = false;
    if (pic->non_existing) return;
    bool present = false;
    for (Picture* p : dpb) {
        if (p == pic) present = true;
    }
    if (!present) {
        store_picture(pic);
        return;
    }
    pic->needs_output = true;
    int waiting = 0;
    for (Picture* p : dpb) {
        if (p->needs_output) ++waiting;
    }
    while (waiting > reorder_frames) {
        bump_one();
        --waiting;
    }
    release_unused();
}

namespace {

template <typename P>
void double_field(Picture* p, int present) {
    for (int c = 0; c < 3; ++c) {
        int h = c == 0 ? p->height : p->chroma_height;
        int w = c == 0 ? p->width : p->chroma_width;
        P* base = reinterpret_cast<P*>(p->plane[c]);
        int s = p->stride[c];
        for (int y = 1 - present; y < h; y += 2) {
            int src = y + (present == 0 ? -1 : 1);
            if (src < 0) src = present;
            if (src >= h) src = h - 2 + present;
            std::memcpy(base + static_cast<std::ptrdiff_t>(y) * s, base + static_cast<std::ptrdiff_t>(src) * s, static_cast<std::size_t>(w) * sizeof(P));
        }
    }
}

} // namespace

void DecoderState::finalize_pending_field() {
    Picture* p = pending_field;
    pending_field = nullptr;
    if (!p || !p->awaiting_field) return;
    int present = p->field_done[0] ? 0 : 1;
    if (p->pixel_bytes == 2) {
        double_field<std::uint16_t>(p, present);
    } else {
        double_field<std::uint8_t>(p, present);
    }
    if (present == 0) {
        p->bottom_poc = p->top_poc;
    } else {
        p->top_poc = p->bottom_poc;
    }
    p->poc = std::min(p->top_poc, p->bottom_poc);
    p->extend_borders();
    complete_frame(p);
}

void DecoderState::flush_all() {
    if (cur) finish_picture();
    finalize_pending_field();
    std::vector<Picture*> waiting;
    for (Picture* p : dpb) {
        if (p->awaiting_field) waiting.push_back(p);
    }
    for (Picture* p : waiting) {
        pending_field = p;
        finalize_pending_field();
    }
    bump_all();
}

void DecoderState::compute_poc(const SliceHeader& h, Picture* pic) {
    const Sps& s = *sps;
    int max_frame_num = 1 << s.log2_max_frame_num;
    std::int32_t top = pic->top_poc;
    std::int32_t bottom = pic->bottom_poc;
    if (s.poc_type == 0) {
        int prev_msb;
        int prev_lsb;
        if (h.idr) {
            prev_msb = 0;
            prev_lsb = 0;
        } else if (prev_ref_has_mmco5) {
            if (prev_ref_bottom_field) {
                prev_msb = 0;
                prev_lsb = 0;
            } else {
                prev_msb = 0;
                prev_lsb = prev_ref_top_poc;
            }
        } else {
            prev_msb = prev_poc_msb;
            prev_lsb = prev_poc_lsb;
        }
        int max_lsb = 1 << s.log2_max_poc_lsb;
        int msb;
        if (h.poc_lsb < prev_lsb && prev_lsb - h.poc_lsb >= max_lsb / 2) {
            msb = prev_msb + max_lsb;
        } else if (h.poc_lsb > prev_lsb && h.poc_lsb - prev_lsb > max_lsb / 2) {
            msb = prev_msb - max_lsb;
        } else {
            msb = prev_msb;
        }
        if (!h.field_pic) {
            top = msb + h.poc_lsb;
            bottom = top + h.delta_poc_bottom;
        } else if (!h.bottom_field) {
            top = msb + h.poc_lsb;
        } else {
            bottom = msb + h.poc_lsb;
        }
        cur_poc_msb = msb;
    } else {
        int prev_offset = prev_has_mmco5 ? 0 : prev_frame_num_offset;
        if (h.idr) {
            frame_num_offset = 0;
        } else if (prev_frame_num > h.frame_num) {
            frame_num_offset = prev_offset + max_frame_num;
        } else {
            frame_num_offset = prev_offset;
        }
        if (s.poc_type == 1) {
            int n = s.num_ref_frames_in_poc_cycle;
            long abs_frame = n != 0 ? static_cast<long>(frame_num_offset) + h.frame_num : 0;
            if (h.nal_ref_idc == 0 && abs_frame > 0) --abs_frame;
            long expected = 0;
            if (abs_frame > 0) {
                long delta_cycle = 0;
                for (int i = 0; i < n; ++i) delta_cycle += s.offset_for_ref_frame[i];
                long cycle = (abs_frame - 1) / n;
                long in_cycle = (abs_frame - 1) % n;
                expected = cycle * delta_cycle;
                for (long i = 0; i <= in_cycle; ++i) expected += s.offset_for_ref_frame[i];
            }
            if (h.nal_ref_idc == 0) expected += s.offset_for_non_ref_pic;
            if (!h.field_pic) {
                top = static_cast<std::int32_t>(expected + h.delta_poc[0]);
                bottom = top + s.offset_for_top_to_bottom_field + h.delta_poc[1];
            } else if (!h.bottom_field) {
                top = static_cast<std::int32_t>(expected + h.delta_poc[0]);
            } else {
                bottom = static_cast<std::int32_t>(expected + s.offset_for_top_to_bottom_field + h.delta_poc[0]);
            }
        } else {
            int temp;
            if (h.idr) {
                temp = 0;
            } else if (h.nal_ref_idc == 0) {
                temp = 2 * (frame_num_offset + h.frame_num) - 1;
            } else {
                temp = 2 * (frame_num_offset + h.frame_num);
            }
            if (!h.field_pic) {
                top = temp;
                bottom = temp;
            } else if (!h.bottom_field) {
                top = temp;
            } else {
                bottom = temp;
            }
        }
    }
    pic->top_poc = top;
    pic->bottom_poc = bottom;
    if (!h.field_pic) {
        pic->poc = std::min(top, bottom);
    } else if (second_field) {
        pic->poc = std::min(top, bottom);
    } else {
        pic->poc = h.bottom_field ? bottom : top;
    }
}

void DecoderState::sliding_window() {
    int num_short = 0;
    int num_long = 0;
    for (Picture* p : dpb) {
        if (p == cur) continue;
        if (p->short_ref) ++num_short;
        if (p->long_ref && !p->short_ref) ++num_long;
    }
    int max_refs = std::max(sps->max_num_ref_frames, 1);
    while (num_short > 0 && num_short + num_long >= max_refs) {
        Picture* oldest = nullptr;
        for (Picture* p : dpb) {
            if (p == cur) continue;
            if (p->short_ref && (!oldest || p->frame_num_wrap < oldest->frame_num_wrap)) oldest = p;
        }
        if (!oldest) break;
        for (int f = 0; f < 2; ++f) {
            if (oldest->field_ref[f] == 1) oldest->field_ref[f] = 0;
        }
        oldest->sync_ref();
        if (oldest->long_ref) ++num_long;
        --num_short;
    }
}

void DecoderState::handle_frame_num_gap(const SliceHeader& h) {
    int max_frame_num = 1 << sps->log2_max_frame_num;
    int unused = (prev_ref_frame_num + 1) % max_frame_num;
    int gap = (h.frame_num - unused + max_frame_num) % max_frame_num;
    int keep = std::max(1, sps->max_num_ref_frames);
    Picture* source = nullptr;
    for (Picture* p : dpb) {
        if (p->is_ref() && !p->awaiting_field && (!source || p->decode_order > source->decode_order)) source = p;
    }
    int index = 0;
    while (unused != h.frame_num) {
        bool materialize = gap - index <= keep;
        ++index;
        if (sps->poc_type != 0) {
            int prev_offset = prev_has_mmco5 ? 0 : prev_frame_num_offset;
            frame_num_offset = prev_frame_num > unused ? prev_offset + max_frame_num : prev_offset;
            prev_frame_num_offset = frame_num_offset;
            prev_has_mmco5 = false;
        }
        if (materialize) {
            for (Picture* p : dpb) {
                if (p->short_ref) p->frame_num_wrap = p->frame_num > unused ? p->frame_num - max_frame_num : p->frame_num;
            }
            sliding_window();
            release_unused();
            Picture* pic = alloc_picture();
            if (!pic) break;
            pic->frame_num = unused;
            pic->frame_num_wrap = unused;
            pic->non_existing = true;
            pic->field_ref[0] = pic->field_ref[1] = 1;
            pic->sync_ref();
            pic->field_done[0] = pic->field_done[1] = true;
            int base_poc = sps->poc_type != 0 ? 2 * (frame_num_offset + unused) : (source ? source->poc : 0);
            pic->top_poc = pic->bottom_poc = pic->poc = base_poc;
            if (source) {
                std::memcpy(pic->buffer.data(), source->buffer.data(), std::min(pic->buffer.size(), source->buffer.size()));
            } else {
                fill_picture_gray(pic);
            }
            std::fill(pic->mb_intra.begin(), pic->mb_intra.end(), static_cast<std::uint8_t>(1));
            std::fill(pic->mb_field.begin(), pic->mb_field.end(), static_cast<std::uint8_t>(0));
            for (int l = 0; l < 2; ++l) std::fill(pic->ref_idx[l].begin(), pic->ref_idx[l].end(), static_cast<std::int8_t>(-1));
            store_picture(pic);
        }
        prev_ref_frame_num = unused;
        prev_frame_num = unused;
        unused = (unused + 1) % max_frame_num;
    }
}

void DecoderState::mark_field_references(Picture* pic, const SliceHeader& h) {
    mark_references(pic, h);
}

void DecoderState::mark_references(Picture* pic, const SliceHeader& h) {
    int max_frame_num = 1 << sps->log2_max_frame_num;
    bool field = h.field_pic;
    int parity = h.bottom_field ? 1 : 0;
    for (Picture* p : dpb) {
        if (p->is_ref()) p->frame_num_wrap = p->frame_num > h.frame_num ? p->frame_num - max_frame_num : p->frame_num;
    }
    pic->frame_num_wrap = pic->frame_num;
    auto set_current = [&](int value) {
        if (field) {
            pic->field_ref[parity] = static_cast<std::uint8_t>(value);
        } else {
            pic->field_ref[0] = pic->field_ref[1] = static_cast<std::uint8_t>(value);
        }
        pic->sync_ref();
    };
    if (h.idr) {
        for (Picture* p : dpb) {
            if (p == pic) continue;
            p->field_ref[0] = p->field_ref[1] = 0;
            p->sync_ref();
        }
        if (!(field && second_field)) pic->field_ref[0] = pic->field_ref[1] = 0;
        if (h.long_term_reference_flag) {
            set_current(2);
            pic->long_term_frame_idx = 0;
            max_long_term_frame_idx = 0;
        } else {
            set_current(1);
            max_long_term_frame_idx = -1;
        }
        return;
    }
    int cur_pic_num = field ? 2 * h.frame_num + 1 : h.frame_num;
    bool current_long = false;
    auto pic_num_of = [&](const Picture* p, int f) -> int { return field ? 2 * p->frame_num_wrap + (f == parity ? 1 : 0) : p->frame_num_wrap; };
    auto lt_pic_num_of = [&](const Picture* p, int f) -> int { return field ? 2 * p->long_term_frame_idx + (f == parity ? 1 : 0) : p->long_term_frame_idx; };
    std::vector<Picture*> all = dpb;
    bool has_self = false;
    for (Picture* p : all) {
        if (p == pic) has_self = true;
    }
    if (!has_self) all.push_back(pic);
    if (h.adaptive_marking) {
        for (int i = 0; i < h.num_mmco; ++i) {
            const Mmco& m = h.mmco[i];
            int pic_num_x = cur_pic_num - static_cast<int>(std::min<std::uint32_t>(m.difference_of_pic_nums_minus1, 1u << 20)) - 1;
            switch (m.op) {
            case 1:
                for (Picture* p : all) {
                    for (int f = 0; f < 2; ++f) {
                        if (p == pic && field && f == parity) continue;
                        if (p->field_ref[f] != 1) continue;
                        if (pic_num_of(p, f) != pic_num_x) continue;
                        if (field) {
                            p->field_ref[f] = 0;
                        } else {
                            for (int g = 0; g < 2; ++g) {
                                if (p->field_ref[g] == 1) p->field_ref[g] = 0;
                            }
                        }
                    }
                    p->sync_ref();
                }
                break;
            case 2:
                for (Picture* p : all) {
                    for (int f = 0; f < 2; ++f) {
                        if (p == pic && field && f == parity) continue;
                        if (p->field_ref[f] != 2) continue;
                        if (static_cast<std::uint32_t>(lt_pic_num_of(p, f)) != m.long_term_pic_num) continue;
                        if (field) {
                            p->field_ref[f] = 0;
                        } else {
                            for (int g = 0; g < 2; ++g) {
                                if (p->field_ref[g] == 2) p->field_ref[g] = 0;
                            }
                        }
                    }
                    p->sync_ref();
                }
                break;
            case 3: {
                Picture* target = nullptr;
                int target_field = -1;
                for (Picture* p : all) {
                    for (int f = 0; f < 2; ++f) {
                        if (p == pic && field && f == parity) continue;
                        if (p->field_ref[f] == 1 && pic_num_of(p, f) == pic_num_x) {
                            target = p;
                            target_field = f;
                        }
                    }
                }
                int idx = static_cast<int>(std::min<std::uint32_t>(m.long_term_frame_idx, 31));
                for (Picture* p : all) {
                    if (p == target) continue;
                    if (!p->long_ref || p->long_term_frame_idx != idx) continue;
                    for (int g = 0; g < 2; ++g) {
                        if (p->field_ref[g] == 2) p->field_ref[g] = 0;
                    }
                    p->sync_ref();
                }
                if (target) {
                    if (target->long_ref && target->long_term_frame_idx != idx) {
                        for (int g = 0; g < 2; ++g) {
                            if (target->field_ref[g] == 2) target->field_ref[g] = 0;
                        }
                    }
                    if (field) {
                        target->field_ref[target_field] = 2;
                    } else {
                        target->field_ref[0] = target->field_ref[1] = 2;
                    }
                    target->long_term_frame_idx = idx;
                    target->sync_ref();
                }
                break;
            }
            case 4:
                max_long_term_frame_idx = static_cast<int>(std::min<std::uint32_t>(m.max_long_term_frame_idx_plus1, 32)) - 1;
                for (Picture* p : all) {
                    if (p->long_ref && p->long_term_frame_idx > max_long_term_frame_idx) {
                        for (int g = 0; g < 2; ++g) {
                            if (p->field_ref[g] == 2) p->field_ref[g] = 0;
                        }
                        p->sync_ref();
                    }
                }
                break;
            case 5:
                for (Picture* p : all) {
                    if (p == pic) {
                        if (field) p->field_ref[1 - parity] = 0;
                        p->sync_ref();
                        continue;
                    }
                    p->field_ref[0] = p->field_ref[1] = 0;
                    p->sync_ref();
                }
                max_long_term_frame_idx = -1;
                pic->mmco5 = true;
                break;
            case 6: {
                int idx = static_cast<int>(std::min<std::uint32_t>(m.long_term_frame_idx, 31));
                for (Picture* p : all) {
                    if (p == pic) continue;
                    if (p->long_ref && p->long_term_frame_idx == idx) {
                        for (int g = 0; g < 2; ++g) {
                            if (p->field_ref[g] == 2) p->field_ref[g] = 0;
                        }
                        p->sync_ref();
                    }
                }
                if (field && pic->long_ref && pic->long_term_frame_idx != idx) {
                    for (int g = 0; g < 2; ++g) {
                        if (g != parity && pic->field_ref[g] == 2) pic->field_ref[g] = 0;
                    }
                }
                set_current(2);
                pic->long_term_frame_idx = idx;
                current_long = true;
                break;
            }
            default:
                break;
            }
        }
    } else {
        bool first_short = field && second_field && pic->field_ref[1 - parity] == 1;
        if (!first_short) sliding_window();
    }
    if (!current_long) {
        if (field && second_field && pic->field_ref[1 - parity] == 2) {
            set_current(2);
        } else {
            set_current(1);
        }
    }
    int total = 0;
    for (Picture* p : dpb) {
        if (p != pic && p->is_ref()) ++total;
    }
    int max_refs = std::max(sps->max_num_ref_frames, 1);
    while (total >= max_refs + 1) {
        Picture* oldest = nullptr;
        for (Picture* p : dpb) {
            if (p != pic && p->short_ref && (!oldest || p->frame_num_wrap < oldest->frame_num_wrap)) oldest = p;
        }
        if (!oldest) break;
        for (int f = 0; f < 2; ++f) {
            if (oldest->field_ref[f] == 1) oldest->field_ref[f] = 0;
        }
        oldest->sync_ref();
        if (!oldest->is_ref()) --total;
    }
}

void DecoderState::build_frame_lists(std::vector<Picture*>* init, int lists) {
    std::vector<Picture*> shorts;
    std::vector<Picture*> longs;
    int max_frame_num = 1 << sps->log2_max_frame_num;
    for (Picture* p : dpb) {
        if (p == cur || p->awaiting_field) continue;
        if (p->frame_is_short()) {
            p->frame_num_wrap = p->frame_num > sh.frame_num ? p->frame_num - max_frame_num : p->frame_num;
            shorts.push_back(p);
        } else if (p->frame_is_long()) {
            longs.push_back(p);
        }
    }
    std::sort(longs.begin(), longs.end(), [](const Picture* a, const Picture* b) { return a->long_term_frame_idx < b->long_term_frame_idx; });
    if (lists == 1) {
        std::sort(shorts.begin(), shorts.end(), [](const Picture* a, const Picture* b) { return a->frame_num_wrap > b->frame_num_wrap; });
        init[0] = shorts;
        init[0].insert(init[0].end(), longs.begin(), longs.end());
        return;
    }
    std::vector<Picture*> before;
    std::vector<Picture*> after;
    for (Picture* p : shorts) {
        if (p->poc <= cur->poc) {
            before.push_back(p);
        } else {
            after.push_back(p);
        }
    }
    std::sort(before.begin(), before.end(), [](const Picture* a, const Picture* b) { return a->poc > b->poc; });
    std::sort(after.begin(), after.end(), [](const Picture* a, const Picture* b) { return a->poc < b->poc; });
    init[0] = before;
    init[0].insert(init[0].end(), after.begin(), after.end());
    init[0].insert(init[0].end(), longs.begin(), longs.end());
    init[1] = after;
    init[1].insert(init[1].end(), before.begin(), before.end());
    init[1].insert(init[1].end(), longs.begin(), longs.end());
    if (init[1].size() > 1 && init[0] == init[1]) std::swap(init[1][0], init[1][1]);
}

namespace {

void alternate_fields(const std::vector<Picture*>& frames, int parity, int mark, std::vector<RefEntry>& out) {
    std::size_t idx[2] = {0, 0};
    int turn = 0;
    while (true) {
        int f = turn == 0 ? parity : 1 - parity;
        std::size_t& k = idx[turn];
        while (k < frames.size() && frames[k]->field_ref[f] != mark) ++k;
        if (k < frames.size()) {
            RefEntry e;
            e.pic = frames[k];
            e.structure = f == 0 ? k_struct_top : k_struct_bottom;
            out.push_back(e);
            ++k;
            turn ^= 1;
            continue;
        }
        int of = 1 - f;
        std::size_t& k2 = idx[turn ^ 1];
        while (k2 < frames.size()) {
            if (frames[k2]->field_ref[of] == mark) {
                RefEntry e;
                e.pic = frames[k2];
                e.structure = of == 0 ? k_struct_top : k_struct_bottom;
                out.push_back(e);
            }
            ++k2;
        }
        break;
    }
}

} // namespace

void DecoderState::build_field_lists(std::vector<RefEntry>* init, int lists) {
    int parity = bottom_field ? 1 : 0;
    int max_frame_num = 1 << sps->log2_max_frame_num;
    std::vector<Picture*> shorts;
    std::vector<Picture*> longs;
    for (Picture* p : dpb) {
        bool use_short = false;
        bool use_long = false;
        for (int f = 0; f < 2; ++f) {
            if (p == cur && f == parity) continue;
            if (p->field_ref[f] == 1) use_short = true;
            if (p->field_ref[f] == 2) use_long = true;
        }
        if (use_short) {
            p->frame_num_wrap = p->frame_num > sh.frame_num ? p->frame_num - max_frame_num : p->frame_num;
            shorts.push_back(p);
        }
        if (use_long) longs.push_back(p);
    }
    std::sort(longs.begin(), longs.end(), [](const Picture* a, const Picture* b) { return a->long_term_frame_idx < b->long_term_frame_idx; });
    std::vector<RefEntry> long_fields;
    alternate_fields(longs, parity, 2, long_fields);
    if (lists == 1) {
        std::stable_sort(shorts.begin(), shorts.end(), [](const Picture* a, const Picture* b) { return a->frame_num_wrap > b->frame_num_wrap; });
        alternate_fields(shorts, parity, 1, init[0]);
        init[0].insert(init[0].end(), long_fields.begin(), long_fields.end());
        return;
    }
    std::int32_t cur_poc = cur->structure_poc(structure);
    auto entry_poc = [](const Picture* p) -> std::int32_t {
        bool t = p->field_ref[0] == 1;
        bool b = p->field_ref[1] == 1;
        if (t && b) return std::min(p->top_poc, p->bottom_poc);
        return t ? p->top_poc : p->bottom_poc;
    };
    std::vector<Picture*> before;
    std::vector<Picture*> after;
    for (Picture* p : shorts) {
        if (entry_poc(p) <= cur_poc) {
            before.push_back(p);
        } else {
            after.push_back(p);
        }
    }
    std::stable_sort(before.begin(), before.end(), [&](const Picture* a, const Picture* b) { return entry_poc(a) > entry_poc(b); });
    std::stable_sort(after.begin(), after.end(), [&](const Picture* a, const Picture* b) { return entry_poc(a) < entry_poc(b); });
    std::vector<Picture*> l0 = before;
    l0.insert(l0.end(), after.begin(), after.end());
    std::vector<Picture*> l1 = after;
    l1.insert(l1.end(), before.begin(), before.end());
    alternate_fields(l0, parity, 1, init[0]);
    alternate_fields(l1, parity, 1, init[1]);
    init[0].insert(init[0].end(), long_fields.begin(), long_fields.end());
    init[1].insert(init[1].end(), long_fields.begin(), long_fields.end());
    bool same = init[0].size() == init[1].size();
    for (std::size_t i = 0; same && i < init[0].size(); ++i) {
        if (init[0][i].pic != init[1][i].pic || init[0][i].structure != init[1][i].structure) same = false;
    }
    if (same && init[1].size() > 1) std::swap(init[1][0], init[1][1]);
}

void DecoderState::apply_modifications(int l, std::vector<RefEntry>& init, int n, bool* missing) {
    int max_frame_num = 1 << sps->log2_max_frame_num;
    bool field = field_pic;
    int parity = bottom_field ? 1 : 0;
    int max_pic_num = field ? 2 * max_frame_num : max_frame_num;
    int cur_pic_num = field ? 2 * sh.frame_num + 1 : sh.frame_num;
    RefEntry list[34];
    for (int i = 0; i < 34; ++i) list[i] = RefEntry();
    for (int i = 0; i < n && i < static_cast<int>(init.size()); ++i) list[i] = init[static_cast<std::size_t>(i)];
    int pred = cur_pic_num;
    int ref_idx = 0;
    auto same_entry = [](const RefEntry& a, const RefEntry& b) { return a.pic == b.pic && a.structure == b.structure; };
    for (int k = 0; k < sh.num_modifications[l] && ref_idx < n; ++k) {
        const RefModification& mod = sh.modifications[l][k];
        RefEntry found;
        if (mod.idc == 0 || mod.idc == 1) {
            int diff = static_cast<int>(std::min<std::uint32_t>(mod.value, static_cast<std::uint32_t>(max_pic_num))) + 1;
            int no_wrap;
            if (mod.idc == 0) {
                no_wrap = pred - diff;
                if (no_wrap < 0) no_wrap += max_pic_num;
            } else {
                no_wrap = pred + diff;
                if (no_wrap >= max_pic_num) no_wrap -= max_pic_num;
            }
            pred = no_wrap;
            int pic_num = no_wrap > cur_pic_num ? no_wrap - max_pic_num : no_wrap;
            for (Picture* p : dpb) {
                if (p->non_existing && false) continue;
                int fnw = p->frame_num > sh.frame_num ? p->frame_num - max_frame_num : p->frame_num;
                if (field) {
                    for (int f = 0; f < 2; ++f) {
                        if (p == cur && f == parity) continue;
                        if (p == cur && !second_field) continue;
                        if (p->field_ref[f] != 1) continue;
                        if (2 * fnw + (f == parity ? 1 : 0) == pic_num) {
                            found.pic = p;
                            found.structure = f == 0 ? k_struct_top : k_struct_bottom;
                        }
                    }
                } else if (p != cur && p->frame_is_short() && fnw == pic_num) {
                    found.pic = p;
                    found.structure = k_struct_frame;
                }
            }
        } else if (mod.idc == 2) {
            for (Picture* p : dpb) {
                if (field) {
                    for (int f = 0; f < 2; ++f) {
                        if (p == cur && f == parity) continue;
                        if (p->field_ref[f] != 2) continue;
                        if (static_cast<std::uint32_t>(2 * p->long_term_frame_idx + (f == parity ? 1 : 0)) == mod.value) {
                            found.pic = p;
                            found.structure = f == 0 ? k_struct_top : k_struct_bottom;
                        }
                    }
                } else if (p != cur && p->frame_is_long() && static_cast<std::uint32_t>(p->long_term_frame_idx) == mod.value) {
                    found.pic = p;
                    found.structure = k_struct_frame;
                }
            }
        } else {
            continue;
        }
        if (!found.pic) *missing = true;
        for (int c = n; c > ref_idx; --c) list[c] = list[c - 1];
        list[ref_idx++] = found;
        int nidx = ref_idx;
        for (int c = ref_idx; c <= n; ++c) {
            if (!found.pic || !same_entry(list[c], found)) list[nidx++] = list[c];
        }
    }
    init.assign(list, list + n);
}

bool DecoderState::build_ref_lists(std::string* error) {
    for (int l = 0; l < 2; ++l) {
        ref_count[l] = 0;
        for (int i = 0; i < 32; ++i) {
            ref_list[l][i] = nullptr;
            ref_struct[l][i] = k_struct_frame;
        }
    }
    if (sh.slice_type == k_slice_i || sh.slice_type == k_slice_si) return true;
    int lists = sh.slice_type == k_slice_b ? 2 : 1;
    std::vector<RefEntry> init[2];
    if (field_pic) {
        build_field_lists(init, lists);
    } else {
        std::vector<Picture*> frames[2];
        build_frame_lists(frames, lists);
        for (int l = 0; l < lists; ++l) {
            for (Picture* p : frames[l]) {
                RefEntry e;
                e.pic = p;
                e.structure = k_struct_frame;
                init[l].push_back(e);
            }
        }
    }
    bool missing = false;
    for (int l = 0; l < lists; ++l) {
        int n = sh.num_ref_idx_active[l];
        apply_modifications(l, init[l], n, &missing);
        RefEntry fallback;
        for (int i = 0; i < n; ++i) {
            if (init[l][static_cast<std::size_t>(i)].pic) {
                fallback = init[l][static_cast<std::size_t>(i)];
                break;
            }
        }
        if (!fallback.pic) {
            for (Picture* p : dpb) {
                if (p != cur && !p->awaiting_field && (!fallback.pic || p->decode_order > fallback.pic->decode_order)) fallback.pic = p;
            }
            fallback.structure = field_pic ? structure : k_struct_frame;
        }
        if (!fallback.pic && field_pic && second_field) {
            fallback.pic = cur;
            fallback.structure = bottom_field ? k_struct_top : k_struct_bottom;
        }
        if (!fallback.pic) {
            if (!gray_pic) {
                gray_pic.reset(new Picture());
                gray_pic->allocate(mb_width, mb_height, sps->chroma_format_idc, pixel_bytes, num_planes);
                gray_pic->bit_depth_luma = sps->bit_depth_luma;
                gray_pic->bit_depth_chroma = sps->bit_depth_chroma;
                fill_picture_gray(gray_pic.get());
                gray_pic->uid = 0;
            }
            fallback.pic = gray_pic.get();
            fallback.structure = field_pic ? structure : k_struct_frame;
        }
        for (int i = 0; i < n; ++i) {
            RefEntry e = init[l][static_cast<std::size_t>(i)];
            if (!e.pic) {
                e = fallback;
                missing = true;
            }
            ref_list[l][i] = e.pic;
            ref_struct[l][i] = e.structure;
        }
        ref_count[l] = n;
    }
    if (missing) {
        ++errors;
        if (error) *error = "missing reference picture";
    }
    return !missing;
}

RefEntry DecoderState::ref_entry(int list, int idx) const noexcept {
    RefEntry e;
    if (mbaff && mb_field) {
        int fi = idx >> 1;
        if (fi < 0 || fi >= ref_count[list]) return e;
        e.pic = ref_list[list][fi];
        int same = mb_bottom ? k_struct_bottom : k_struct_top;
        int opp = mb_bottom ? k_struct_top : k_struct_bottom;
        e.structure = (idx & 1) ? opp : same;
        return e;
    }
    if (idx < 0 || idx >= ref_count[list]) return e;
    e.pic = ref_list[list][idx];
    e.structure = ref_struct[list][idx];
    return e;
}

int DecoderState::ref_count_mb(int list) const noexcept {
    return (mbaff && mb_field) ? ref_count[list] * 2 : ref_count[list];
}

std::int32_t DecoderState::ref_entry_id(const RefEntry& e) const noexcept {
    if (!e.pic) return -1;
    return e.pic->uid * 4 + e.structure;
}

std::int32_t DecoderState::ref_entry_poc(const RefEntry& e) const noexcept {
    if (!e.pic) return 0;
    return e.pic->structure_poc(e.structure);
}

bool DecoderState::ref_entry_long(const RefEntry& e) const noexcept {
    if (!e.pic) return false;
    if (e.structure == k_struct_frame) return e.pic->field_ref[0] == 2 || e.pic->field_ref[1] == 2;
    return e.pic->field_ref[e.structure == k_struct_top ? 0 : 1] == 2;
}

std::int32_t DecoderState::cur_poc_for_mb() const noexcept {
    if (mbaff && mb_field) return mb_bottom ? cur->bottom_poc : cur->top_poc;
    if (field_pic) return bottom_field ? cur->bottom_poc : cur->top_poc;
    return std::min(cur->top_poc, cur->bottom_poc);
}

namespace {

int implicit_weight(std::int32_t cur_poc, std::int32_t poc0, std::int32_t poc1, bool long0, bool long1) {
    int w0 = 32;
    int td = clip3(-128, 127, poc1 - poc0);
    if (poc1 != poc0 && !long0 && !long1 && td != 0) {
        int tb = clip3(-128, 127, cur_poc - poc0);
        int tx = (16384 + std::abs(td / 2)) / td;
        int dsf = clip3(-1024, 1023, (tb * tx + 32) >> 6);
        if ((dsf >> 2) >= -64 && (dsf >> 2) <= 128) w0 = 64 - (dsf >> 2);
    }
    return w0;
}

} // namespace

void DecoderState::compute_implicit_weights() {
    std::int32_t cp = field_pic ? cur->structure_poc(structure) : std::min(cur->top_poc, cur->bottom_poc);
    for (int i = 0; i < ref_count[0]; ++i) {
        for (int j = 0; j < ref_count[1]; ++j) {
            RefEntry a;
            a.pic = ref_list[0][i];
            a.structure = ref_struct[0][i];
            RefEntry b;
            b.pic = ref_list[1][j];
            b.structure = ref_struct[1][j];
            implicit_w[i][j] = implicit_weight(cp, ref_entry_poc(a), ref_entry_poc(b), ref_entry_long(a), ref_entry_long(b));
        }
    }
    if (mbaff) compute_implicit_field_weights();
}

void DecoderState::compute_implicit_field_weights() {
    for (int par = 0; par < 2; ++par) {
        std::int32_t cp = par ? cur->bottom_poc : cur->top_poc;
        int same = par ? k_struct_bottom : k_struct_top;
        int opp = par ? k_struct_top : k_struct_bottom;
        for (int i = 0; i < ref_count[0] * 2; ++i) {
            for (int j = 0; j < ref_count[1] * 2; ++j) {
                RefEntry a;
                a.pic = ref_list[0][i >> 1];
                a.structure = (i & 1) ? opp : same;
                RefEntry b;
                b.pic = ref_list[1][j >> 1];
                b.structure = (j & 1) ? opp : same;
                implicit_wf[par][i][j] = implicit_weight(cp, ref_entry_poc(a), ref_entry_poc(b), ref_entry_long(a), ref_entry_long(b));
            }
        }
    }
}

void DecoderState::update_dequant() {
    std::uint8_t s4[6][16];
    std::uint8_t s8[6][64];
    resolve_scaling(*sps, *pps, s4, s8);
    if (dq_valid && std::memcmp(s4, dq_lists4, sizeof(s4)) == 0 && std::memcmp(s8, dq_lists8, sizeof(s8)) == 0) return;
    std::memcpy(dq_lists4, s4, sizeof(s4));
    std::memcpy(dq_lists8, s8, sizeof(s8));
    dq_valid = true;
    for (int q = 0; q < 92; ++q) {
        int m = q % 6;
        int shift = q / 6;
        for (int list = 0; list < 6; ++list) {
            for (int pos = 0; pos < 16; ++pos) {
                int i = pos >> 2;
                int j = pos & 3;
                int v;
                if ((i & 1) == 0 && (j & 1) == 0) {
                    v = k_norm_adjust4[m][0];
                } else if ((i & 1) == 1 && (j & 1) == 1) {
                    v = k_norm_adjust4[m][1];
                } else {
                    v = k_norm_adjust4[m][2];
                }
                dq4[list][q][pos] = (s4[list][pos] * v) << shift;
            }
            for (int pos = 0; pos < 64; ++pos) {
                int i = pos >> 3;
                int j = pos & 7;
                int idx;
                if ((i & 3) == 0 && (j & 3) == 0) {
                    idx = 0;
                } else if ((i & 1) == 1 && (j & 1) == 1) {
                    idx = 1;
                } else if ((i & 3) == 2 && (j & 3) == 2) {
                    idx = 2;
                } else if (((i & 3) == 0 && (j & 1) == 1) || ((i & 1) == 1 && (j & 3) == 0)) {
                    idx = 3;
                } else if (((i & 3) == 0 && (j & 3) == 2) || ((i & 3) == 2 && (j & 3) == 0)) {
                    idx = 4;
                } else {
                    idx = 5;
                }
                dq8[list][q][pos] = (s8[list][pos] * k_norm_adjust8[m][idx]) << shift;
            }
        }
    }
}

void DecoderState::switch_plane(int p) {
    if (p == active_plane || num_planes == 1) return;
    PlaneStore& a = plane_store[active_plane];
    a.mbs.swap(mbs);
    a.nnz.swap(nnz);
    a.intra_modes.swap(intra_modes);
    a.mvd[0].swap(mvd[0]);
    a.mvd[1].swap(mvd[1]);
    a.slices.swap(slices);
    a.mb_done.swap(mb_done);
    PlaneStore& b = plane_store[p];
    b.mbs.swap(mbs);
    b.nnz.swap(nnz);
    b.intra_modes.swap(intra_modes);
    b.mvd[0].swap(mvd[0]);
    b.mvd[1].swap(mvd[1]);
    b.slices.swap(slices);
    b.mb_done.swap(mb_done);
    active_plane = p;
}

bool DecoderState::start_picture(const SliceHeader& h, double pts, std::string* error) {
    (void)error;
    int max_frame_num = 1 << sps->log2_max_frame_num;
    bool pair = false;
    if (h.field_pic && pending_field && pending_field->awaiting_field) {
        int parity = h.bottom_field ? 1 : 0;
        Picture* p = pending_field;
        if (!p->field_done[parity] && p->field_done[1 - parity] && p->frame_num == h.frame_num && (!h.idr || p->idr) && !p->mmco5) pair = true;
    }
    if (!pair && pending_field) finalize_pending_field();
    second_field = pair;
    if (!pair) {
        if (h.idr) {
            prev_ref_frame_num = 0;
        } else if (h.frame_num != prev_ref_frame_num && h.frame_num != (prev_ref_frame_num + 1) % max_frame_num && seen_reference) {
            handle_frame_num_gap(h);
        }
    }
    Picture* pic;
    if (pair) {
        pic = pending_field;
        pending_field = nullptr;
    } else {
        pic = alloc_picture();
        if (!pic) {
            if (error) *error = "out of picture buffers";
            return false;
        }
        pic->frame_num = h.frame_num;
        pic->frame_num_wrap = h.frame_num;
        pic->idr = h.idr;
        pic->pts = pts;
        pic->keyframe = h.idr || h.slice_type == k_slice_i;
        pic->long_term_frame_idx = 0;
        pic->top_poc = pic->bottom_poc = 0;
        pic->first_field_parity = h.bottom_field ? 1 : 0;
        pic->coding = h.field_pic ? k_coding_fields : (h.mbaff ? k_coding_mbaff : k_coding_frame);
        pic->field_pic_coded = h.field_pic;
        pic->pic_struct = cur_pic_struct;
        pic->interlaced_hint = false;
    }
    cur = pic;
    field_pic = h.field_pic;
    bottom_field = h.bottom_field;
    mbaff = h.mbaff;
    structure = h.field_pic ? (h.bottom_field ? k_struct_bottom : k_struct_top) : k_struct_frame;
    pic_mb_height = h.field_pic ? mb_height / 2 : mb_height;
    pic_mb_count = mb_width * pic_mb_height;
    cv = pic->view(structure);
    if (pair) {
        pic->field_pts[h.bottom_field ? 1 : 0] = pts;
    } else {
        pic->field_pts[0] = pic->field_pts[1] = pts;
    }
    compute_poc(h, pic);
    cur_first_header = h;
    cur_has_slices = false;
    cur_mbs_decoded = 0;
    for (int p = 0; p < 3; ++p) mbs_per_plane_done[p] = 0;
    for (int p = num_planes - 1; p >= 0; --p) {
        switch_plane(p);
        slices.clear();
        for (int a = 0; a < pic_mb_count; ++a) mbs[static_cast<std::size_t>(a)].slice = -1;
        std::fill(mb_done.begin(), mb_done.end(), static_cast<std::uint8_t>(0));
    }
    std::size_t base = pic->motion_base(0, structure);
    std::size_t n = static_cast<std::size_t>(pic_mb_count);
    for (int p = 0; p < num_planes; ++p) {
        std::size_t b = pic->motion_base(p, structure);
        std::fill(pic->mb_intra.begin() + static_cast<std::ptrdiff_t>(b), pic->mb_intra.begin() + static_cast<std::ptrdiff_t>(b + n), static_cast<std::uint8_t>(0));
        std::fill(pic->mb_field.begin() + static_cast<std::ptrdiff_t>(b), pic->mb_field.begin() + static_cast<std::ptrdiff_t>(b + n), static_cast<std::uint8_t>(h.field_pic ? 1 : 0));
    }
    (void)base;
    return true;
}

template <typename P>
void DecoderState::conceal_t() {
    Picture* src = nullptr;
    for (Picture* p : dpb) {
        if (p != cur && !p->awaiting_field && !p->non_existing && (!src || p->decode_order > src->decode_order)) src = p;
    }
    if (!src) {
        for (Picture* p : dpb) {
            if (p != cur && !p->awaiting_field && (!src || p->decode_order > src->decode_order)) src = p;
        }
    }
    PicView sv;
    if (src) sv = src->view(structure);
    int planes_pix = chroma_array_type == 0 && num_planes == 1 ? 1 : 3;
    for (int pl = 0; pl < num_planes; ++pl) {
        switch_plane(pl);
        std::size_t mbase = cur->motion_base(pl, structure);
        for (int addr = 0; addr < pic_mb_count; ++addr) {
            if (mb_done[static_cast<std::size_t>(addr)]) continue;
            mbs[static_cast<std::size_t>(addr)].slice = -1;
            cur->mb_intra[mbase + static_cast<std::size_t>(addr)] = 1;
            for (int l = 0; l < 2; ++l) {
                for (int b = 0; b < 16; ++b) cur->ref_idx[l][(mbase + static_cast<std::size_t>(addr)) * 16 + static_cast<std::size_t>(b)] = -1;
            }
            int px;
            int py;
            int step = 1;
            if (mbaff) {
                int pair = addr >> 1;
                px = pair % mb_width;
                py = (pair / mb_width) * 32 + (addr & 1) * 16;
            } else {
                px = addr % mb_width;
                py = (addr / mb_width) * 16;
            }
            for (int c = 0; c < 3; ++c) {
                if (num_planes == 1 && c > 0 && planes_pix == 1) continue;
                if (num_planes > 1 && c != pl) continue;
                int cw = c == 0 || num_planes > 1 ? 16 : mb_wc;
                int chh = c == 0 || num_planes > 1 ? 16 : mb_hc;
                int cy = c == 0 || num_planes > 1 ? py : py * chh / 16;
                int cx = px * cw;
                int bd = c == 0 || num_planes > 1 ? bit_depth_y : bit_depth_c;
                P* d = reinterpret_cast<P*>(cv.plane[c]);
                int s = cv.stride[c];
                for (int y = 0; y < chh; ++y) {
                    P* row = d + static_cast<std::ptrdiff_t>(cy + y * step) * s + cx;
                    if (src) {
                        const P* srow = reinterpret_cast<const P*>(sv.plane[c]) + static_cast<std::ptrdiff_t>(cy + y * step) * sv.stride[c] + cx;
                        std::memcpy(row, srow, static_cast<std::size_t>(cw) * sizeof(P));
                    } else {
                        for (int x = 0; x < cw; ++x) row[x] = static_cast<P>(1 << (bd - 1));
                    }
                }
            }
        }
    }
    switch_plane(0);
}

void DecoderState::conceal_picture() {
    if (cur->pixel_bytes == 2) {
        conceal_t<std::uint16_t>();
    } else {
        conceal_t<std::uint8_t>();
    }
    cur->corrupt = true;
}

void DecoderState::finish_field_or_frame() {
    finish_picture();
}

void DecoderState::finish_picture() {
    Picture* pic = cur;
    if (!pic) return;
    if (cur_mbs_decoded < pic_mb_count * num_planes) {
        conceal_picture();
        ++errors;
    }
    deblock_picture();
    if (field_pic) {
        pic->field_done[bottom_field ? 1 : 0] = true;
    } else {
        pic->field_done[0] = pic->field_done[1] = true;
    }
    pic->extend_borders();
    const SliceHeader& h = cur_first_header;
    if (h.nal_ref_idc != 0) {
        mark_references(pic, h);
        seen_reference = true;
    }
    if (pic->mmco5) {
        if (!field_pic) {
            int temp = std::min(pic->top_poc, pic->bottom_poc);
            pic->top_poc -= temp;
            pic->bottom_poc -= temp;
        } else if (bottom_field) {
            pic->bottom_poc = 0;
        } else {
            pic->top_poc = 0;
        }
        pic->poc = field_pic && !second_field ? 0 : std::min(pic->top_poc, pic->bottom_poc);
    }
    if (sps->poc_type != 0) {
        prev_frame_num_offset = frame_num_offset;
    }
    prev_has_mmco5 = pic->mmco5;
    prev_frame_num = pic->mmco5 ? 0 : h.frame_num;
    if (h.nal_ref_idc != 0) {
        prev_ref_frame_num = pic->mmco5 ? 0 : h.frame_num;
        prev_ref_has_mmco5 = pic->mmco5;
        prev_ref_bottom_field = field_pic && bottom_field;
        if (pic->mmco5) {
            prev_ref_top_poc = field_pic ? (bottom_field ? 0 : pic->top_poc) : pic->top_poc;
            prev_poc_msb = 0;
            prev_poc_lsb = prev_ref_top_poc;
        } else {
            prev_poc_msb = cur_poc_msb;
            prev_poc_lsb = h.poc_lsb;
        }
    }
    if (pic->mmco5) {
        pic->frame_num = 0;
    }
    cur = nullptr;
    if (!field_pic) {
        store_picture(pic);
    } else if (second_field) {
        pic->poc = std::min(pic->top_poc, pic->bottom_poc);
        complete_frame(pic);
    } else {
        pic->awaiting_field = true;
        pending_field = pic;
        store_picture(pic);
    }
    second_field = false;
}

void DecoderState::reset_state(bool keep_params) {
    cur = nullptr;
    pending_field = nullptr;
    second_field = false;
    dpb.clear();
    output.clear();
    for (auto& p : pool) {
        p->field_ref[0] = p->field_ref[1] = 0;
        p->sync_ref();
        p->needs_output = false;
        p->awaiting_field = false;
    }
    prev_ref_frame_num = 0;
    prev_frame_num = 0;
    prev_frame_num_offset = 0;
    prev_poc_msb = 0;
    prev_poc_lsb = 0;
    prev_has_mmco5 = false;
    prev_ref_has_mmco5 = false;
    prev_ref_top_poc = 0;
    prev_ref_bottom_field = false;
    frame_num_offset = 0;
    max_long_term_frame_idx = -1;
    seen_reference = false;
    cur_mbs_decoded = 0;
    cur_pic_struct = -1;
    if (!keep_params) {
        for (auto& s : sps_table) s.valid = false;
        for (auto& p : pps_table) p.valid = false;
        sps = nullptr;
        pps = nullptr;
        pool.clear();
        gray_pic.reset();
        cur_geometry_valid = false;
        mb_width = mb_height = mb_count = 0;
        dq_valid = false;
        info = H264Info();
    }
}

bool DecoderState::handle_slice(int nal_type, int nal_ref_idc, double pts, std::string* error) {
    SliceHeader h;
    if (!parse_slice_header(br, nal_type, nal_ref_idc, sps_table, pps_table, h, error)) {
        ++errors;
        return false;
    }
    if (h.redundant_pic_cnt > 0) return true;
    const Pps& p = pps_table[h.pps_id];
    const Sps& s = sps_table[p.sps_id];
    if (h.slice_type == k_slice_sp || h.slice_type == k_slice_si) {
        ++errors;
        if (error) *error = "SP/SI slices are not supported";
        return false;
    }
    if (s.chroma_format_idc == 3 && !s.separate_colour_plane && s.profile_idc != 244 && s.profile_idc != 44 && s.profile_idc != 100 && s.profile_idc != 110 && s.profile_idc != 122) {
        ++errors;
        if (error) *error = "unsupported profile for 4:4:4";
        return false;
    }
    if (cur) {
        const SliceHeader& f = cur_first_header;
        bool new_pic = h.frame_num != f.frame_num || h.pps_id != f.pps_id || (h.nal_ref_idc != f.nal_ref_idc && (h.nal_ref_idc == 0 || f.nal_ref_idc == 0)) ||
                       h.idr != f.idr || (h.idr && f.idr && h.idr_pic_id != f.idr_pic_id) || h.field_pic != f.field_pic || h.bottom_field != f.bottom_field;
        if (s.poc_type == 0 && (h.poc_lsb != f.poc_lsb || h.delta_poc_bottom != f.delta_poc_bottom)) new_pic = true;
        if (s.poc_type == 1 && (h.delta_poc[0] != f.delta_poc[0] || h.delta_poc[1] != f.delta_poc[1])) new_pic = true;
        if (sps != &s) new_pic = true;
        if (!new_pic) {
            int first = h.first_mb * (h.mbaff ? 2 : 1);
            int plane = s.separate_colour_plane ? h.colour_plane_id : 0;
            switch_plane(plane);
            if (first < pic_mb_count && mb_done[static_cast<std::size_t>(first)]) new_pic = true;
        }
        if (new_pic) finish_picture();
    }
    if (!cur) {
        if (!h.idr && !seen_reference && h.slice_type != k_slice_i) {
            ++errors;
            if (error) *error = "stream does not start with a keyframe";
            return false;
        }
        int bytes = (s.bit_depth_luma > 8 || s.bit_depth_chroma > 8) ? 2 : 1;
        bool geometry = !cur_geometry_valid || s.mb_width != mb_width || s.mb_height != mb_height || s.chroma_format_idc != cur_chroma_format ||
                        bytes != pixel_bytes || s.bit_depth_luma != bit_depth_y || s.bit_depth_chroma != bit_depth_c || (s.separate_colour_plane ? 3 : 1) != num_planes;
        sps = &s;
        pps = &p;
        if (geometry || active_sps_id != s.id || h.idr) setup_geometry(s);
        active_sps_id = s.id;
        switch_plane(0);
        if (!start_picture(h, pts, error)) {
            ++errors;
            return false;
        }
        cur_pic_struct = -1;
    }
    sps = &s;
    pps = &p;
    colour_plane = s.separate_colour_plane ? h.colour_plane_id : 0;
    switch_plane(colour_plane);
    motion_offset = cur->motion_base(colour_plane, structure);
    int first = h.first_mb * (h.mbaff ? 2 : 1);
    if (first >= pic_mb_count) {
        ++errors;
        if (error) *error = "first_mb_in_slice out of range";
        return false;
    }
    sh = h;
    SliceParams sp;
    sp.disable_deblocking = h.disable_deblocking;
    sp.alpha_offset = h.alpha_offset;
    sp.beta_offset = h.beta_offset;
    sp.chroma_qp_offset[0] = p.chroma_qp_index_offset[0];
    sp.chroma_qp_offset[1] = p.chroma_qp_index_offset[1];
    sp.slice_type = h.slice_type;
    slices.push_back(sp);
    slice_num = static_cast<int>(slices.size()) - 1;
    bool ok = true;
    std::string list_error;
    if (!build_ref_lists(&list_error)) {
        ok = false;
        if (error) *error = list_error;
    }
    if (h.slice_type == k_slice_b && p.weighted_bipred_idc == 2) compute_implicit_weights();
    col_short = false;
    if (h.slice_type == k_slice_b && ref_list[1][0]) {
        RefEntry e;
        e.pic = ref_list[1][0];
        e.structure = ref_struct[1][0];
        col_short = e.structure == k_struct_frame ? (e.pic->field_ref[0] == 1 || e.pic->field_ref[1] == 1) : e.pic->field_ref[e.structure == k_struct_top ? 0 : 1] == 1;
    }
    update_dequant();
    std::string slice_error;
    bool data_ok = decode_slice_data(&slice_error);
    int decoded = 0;
    for (int a = first; a < mb_addr && a < pic_mb_count; ++a) {
        if (mb_done[static_cast<std::size_t>(a)]) ++decoded;
    }
    cur_mbs_decoded += decoded;
    cur_has_slices = true;
    if (!data_ok) {
        ++errors;
        ok = false;
        if (error) *error = slice_error;
    }
    if (cur_mbs_decoded >= pic_mb_count * num_planes) finish_picture();
    return ok;
}

bool DecoderState::handle_nal(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (size < 1) return true;
    std::uint8_t header = data[0];
    if (header & 0x80) {
        ++errors;
        if (error) *error = "forbidden_zero_bit set";
        return false;
    }
    int nal_ref_idc = (header >> 5) & 3;
    int type = header & 31;
    if (type == 14 || type == 20 || type == 21) return true;
    std::size_t n = unescape_nal(data + 1, size - 1, rbsp);
    br.init(rbsp.data(), n);
    switch (type) {
    case 1:
    case 5:
        return handle_slice(type, nal_ref_idc, pts, error);
    case 2:
    case 3:
    case 4:
        ++errors;
        if (error) *error = "data partitioning is not supported";
        return false;
    case 7: {
        Sps s;
        bool ok = false;
        parse_sps(br, s, error, &ok);
        if (!ok) {
            ++errors;
            return false;
        }
        if (cur && sps == &sps_table[s.id]) finish_picture();
        sps_table[s.id] = s;
        if (active_sps_id == s.id) active_sps_id = -1;
        return true;
    }
    case 8: {
        Pps p;
        bool ok = false;
        parse_pps(br, p, sps_table, error, &ok);
        if (!ok) {
            ++errors;
            return false;
        }
        if (cur && pps == &pps_table[p.id]) finish_picture();
        pps_table[p.id] = p;
        return true;
    }
    case 6: {
        if (cur && cur_has_slices) finish_picture();
        const Sps* s = sps;
        if (!s) {
            for (const Sps& c : sps_table) {
                if (c.valid) {
                    s = &c;
                    break;
                }
            }
        }
        int ps = -1;
        if (s && parse_sei_pic_struct(br, s, &ps)) cur_pic_struct = ps;
        return true;
    }
    case 9:
    case 10:
    case 11:
        if (cur && cur_has_slices) finish_picture();
        return true;
    default:
        return true;
    }
}

bool DecoderState::open_avcc(const std::uint8_t* data, std::size_t size, std::string* error) {
    if (size >= 4 && data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1))) {
        avcc = false;
        opened = true;
        return decode_packet(data, size, 0.0, error);
    }
    if (size < 7 || data[0] != 1) {
        if (error) *error = "invalid avcC record";
        return false;
    }
    nal_length_size = (data[4] & 3) + 1;
    if (nal_length_size == 3) {
        if (error) *error = "invalid NAL length size";
        return false;
    }
    int profile = data[1];
    std::size_t pos = 5;
    int num_sps = data[pos++] & 31;
    bool ok = true;
    for (int pass = 0; pass < 2; ++pass) {
        int count = pass == 0 ? num_sps : 0;
        if (pass == 1) {
            if (pos >= size) break;
            count = data[pos++];
        }
        for (int i = 0; i < count; ++i) {
            if (pos + 2 > size) {
                if (error) *error = "truncated avcC record";
                return false;
            }
            std::size_t len = (static_cast<std::size_t>(data[pos]) << 8) | data[pos + 1];
            pos += 2;
            if (pos + len > size) {
                if (error) *error = "truncated avcC record";
                return false;
            }
            if (!handle_nal(data + pos, len, 0.0, error)) ok = false;
            pos += len;
        }
    }
    if ((profile == 100 || profile == 110 || profile == 122 || profile == 144 || profile == 244 || profile == 44) && pos + 4 <= size) {
        int cf = data[pos] & 3;
        int bdl = (data[pos + 1] & 7) + 8;
        int bdc = (data[pos + 2] & 7) + 8;
        int num_ext = data[pos + 3];
        pos += 4;
        for (int i = 0; i < num_ext && pos + 2 <= size; ++i) {
            std::size_t len = (static_cast<std::size_t>(data[pos]) << 8) | data[pos + 1];
            pos += 2;
            if (pos + len > size) break;
            pos += len;
        }
        if (!cur_geometry_valid) {
            info.chroma_format_idc = cf;
            info.bit_depth = bdl;
            info.bit_depth_chroma = bdc;
            info.format = pixel_format_for(cf, output_bit_depth(cf == 0 ? bdl : std::max(bdl, bdc)));
        }
    }
    avcc = true;
    opened = true;
    return ok;
}

bool DecoderState::decode_packet(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    bool ok = true;
    std::string err;
    bool length_prefixed = avcc;
    if (avcc && size >= 4 && data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1))) {
        std::size_t pos = 0;
        bool valid = true;
        while (pos + static_cast<std::size_t>(nal_length_size) <= size) {
            std::size_t len = 0;
            for (int i = 0; i < nal_length_size; ++i) len = (len << 8) | data[pos + static_cast<std::size_t>(i)];
            pos += static_cast<std::size_t>(nal_length_size);
            if (len == 0 || len > size - pos) {
                valid = false;
                break;
            }
            pos += len;
        }
        if (!valid || pos != size) length_prefixed = false;
    }
    if (length_prefixed) {
        std::size_t pos = 0;
        while (pos + static_cast<std::size_t>(nal_length_size) <= size) {
            std::size_t len = 0;
            for (int i = 0; i < nal_length_size; ++i) len = (len << 8) | data[pos + static_cast<std::size_t>(i)];
            pos += static_cast<std::size_t>(nal_length_size);
            if (len > size - pos) {
                ok = false;
                ++errors;
                err = "NAL length exceeds packet";
                len = size - pos;
            }
            std::string e;
            if (!handle_nal(data + pos, len, pts, &e)) {
                ok = false;
                err = e;
            }
            pos += len;
        }
        if (cur && cur_has_slices) finish_picture();
    } else {
        std::size_t i = 0;
        std::size_t nal_start = size;
        while (i + 3 <= size) {
            if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
                if (nal_start < size) {
                    std::size_t end = i;
                    while (end > nal_start && data[end - 1] == 0) --end;
                    std::string e;
                    if (!handle_nal(data + nal_start, end - nal_start, pts, &e)) {
                        ok = false;
                        err = e;
                    }
                }
                i += 3;
                nal_start = i;
            } else {
                ++i;
            }
        }
        if (nal_start == size && size > 0 && !(size >= 3 && data[0] == 0 && data[1] == 0)) nal_start = 0;
        if (nal_start < size) {
            std::size_t end = size;
            while (end > nal_start && data[end - 1] == 0) --end;
            std::string e;
            if (!handle_nal(data + nal_start, end - nal_start, pts, &e)) {
                ok = false;
                err = e;
            }
        }
        if (avcc && cur && cur_has_slices) finish_picture();
    }
    if (!ok) {
        last_error = err;
        if (error) *error = err;
    }
    return ok;
}

} // namespace h264_internal

using h264_internal::DecoderState;

struct H264Decoder::State : public DecoderState {};

bool is_annexb_h264(const void* data, std::size_t size) noexcept {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    if (!p || size < 5) return false;
    std::size_t i = 0;
    while (i < size && i < 64 && p[i] == 0) ++i;
    if (i < 2 || i >= size || p[i] != 1 || i + 1 >= size) return false;
    std::uint8_t h = p[i + 1];
    if (h & 0x80) return false;
    int type = h & 31;
    return type == 7 || type == 9 || type == 6 || type == 5 || type == 1 || type == 8;
}

H264Decoder::H264Decoder() : m_state(new State()) {}
H264Decoder::~H264Decoder() = default;
H264Decoder::H264Decoder(H264Decoder&&) noexcept = default;
H264Decoder& H264Decoder::operator=(H264Decoder&&) noexcept = default;

bool H264Decoder::open(const std::uint8_t* avcc, std::size_t size, std::string* error) {
    if (!m_state) m_state.reset(new State());
    m_state->reset_state(false);
    if (!avcc || size == 0) {
        if (error) *error = "empty avcC record";
        return false;
    }
    return m_state->open_avcc(avcc, size, error);
}

bool H264Decoder::open_annexb() {
    if (!m_state) m_state.reset(new State());
    m_state->reset_state(false);
    m_state->avcc = false;
    m_state->opened = true;
    return true;
}

bool H264Decoder::decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (!m_state || !m_state->opened) {
        if (error) *error = "decoder is not open";
        return false;
    }
    if (!data || size == 0) return true;
    return m_state->decode_packet(data, size, pts, error);
}

bool H264Decoder::receive(VideoFrame& out) {
    if (!m_state || m_state->output.empty()) return false;
    out = std::move(m_state->output.front());
    m_state->output.pop_front();
    return true;
}

void H264Decoder::flush() {
    if (!m_state) return;
    m_state->flush_all();
}

void H264Decoder::reset() {
    if (!m_state) return;
    m_state->reset_state(true);
}

void H264Decoder::close() {
    if (!m_state) return;
    m_state->reset_state(false);
    m_state->opened = false;
}

bool H264Decoder::is_open() const noexcept {
    return m_state && m_state->opened;
}

const H264Info& H264Decoder::info() const noexcept {
    static const H264Info empty;
    return m_state ? m_state->info : empty;
}

std::uint64_t H264Decoder::frames_decoded() const noexcept {
    return m_state ? m_state->frames_decoded : 0;
}

std::uint64_t H264Decoder::error_count() const noexcept {
    return m_state ? m_state->errors : 0;
}

bool H264FileReader::open(const std::string& path, std::string* error) {
    close();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::vector<std::uint8_t> data;
    std::uint8_t chunk[65536];
    while (true) {
        std::size_t n = std::fread(chunk, 1, sizeof(chunk), f);
        if (n == 0) break;
        data.insert(data.end(), chunk, chunk + n);
    }
    std::fclose(f);
    if (!open(data.data(), data.size())) {
        if (error) *error = "not an Annex B H.264 stream";
        return false;
    }
    return true;
}

bool H264FileReader::open(const void* data, std::size_t size) {
    close();
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    m_data.assign(p, p + size);
    std::size_t cs = 0;
    m_first = next_start(0, &cs);
    if (m_first >= m_data.size()) {
        m_data.clear();
        return false;
    }
    m_pos = m_first;
    m_open = true;
    return true;
}

void H264FileReader::close() {
    m_data.clear();
    m_pos = 0;
    m_first = 0;
    m_open = false;
}

std::size_t H264FileReader::next_start(std::size_t from, std::size_t* code_size) const noexcept {
    std::size_t n = m_data.size();
    for (std::size_t i = from; i + 3 <= n; ++i) {
        if (m_data[i] == 0 && m_data[i + 1] == 0 && m_data[i + 2] == 1) {
            if (i > from && m_data[i - 1] == 0) {
                *code_size = 4;
                return i - 1;
            }
            *code_size = 3;
            return i;
        }
    }
    *code_size = 0;
    return n;
}

bool H264FileReader::read(std::vector<std::uint8_t>& access_unit) {
    access_unit.clear();
    if (!m_open || m_pos >= m_data.size()) return false;
    std::size_t begin = m_pos;
    std::size_t pos = m_pos;
    bool seen_vcl = false;
    std::size_t n = m_data.size();
    while (pos < n) {
        std::size_t cs = 0;
        std::size_t s = next_start(pos, &cs);
        if (s >= n) break;
        std::size_t nal = s + cs;
        if (nal >= n) break;
        int type = m_data[nal] & 31;
        bool vcl = type >= 1 && type <= 5;
        if (seen_vcl) {
            bool boundary = (type >= 6 && type <= 9) || (type >= 14 && type <= 18);
            if (vcl && nal + 1 < n && (m_data[nal + 1] & 0x80)) boundary = true;
            if (boundary) {
                access_unit.assign(m_data.begin() + static_cast<std::ptrdiff_t>(begin), m_data.begin() + static_cast<std::ptrdiff_t>(s));
                m_pos = s;
                return true;
            }
        }
        if (vcl) seen_vcl = true;
        pos = nal + 1;
    }
    access_unit.assign(m_data.begin() + static_cast<std::ptrdiff_t>(begin), m_data.end());
    m_pos = n;
    return true;
}

} // namespace video
} // namespace fizmo
