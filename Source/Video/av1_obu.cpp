#include "av1_internal.hpp"

#include <algorithm>

namespace fizmo {
namespace video {
namespace av1 {

int Decoder::get_relative_dist(int a, int b) const noexcept {
    if (!seq.enable_order_hint) return 0;
    int diff = a - b;
    const int m = 1 << (seq.OrderHintBits - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
}

bool Decoder::parse_color_config(BitReader& br) {
    SequenceHeader& s = seq;
    const int high_bitdepth = static_cast<int>(br.f(1));
    if (s.seq_profile == 2 && high_bitdepth) {
        const int twelve_bit = static_cast<int>(br.f(1));
        s.BitDepth = twelve_bit ? 12 : 10;
    } else if (s.seq_profile <= 2) {
        s.BitDepth = high_bitdepth ? 10 : 8;
    } else {
        return false;
    }
    if (s.seq_profile == 1) {
        s.mono_chrome = 0;
    } else {
        s.mono_chrome = static_cast<int>(br.f(1));
    }
    s.NumPlanes = s.mono_chrome ? 1 : 3;
    s.color_description_present_flag = static_cast<int>(br.f(1));
    if (s.color_description_present_flag) {
        s.color_primaries = static_cast<int>(br.f(8));
        s.transfer_characteristics = static_cast<int>(br.f(8));
        s.matrix_coefficients = static_cast<int>(br.f(8));
    } else {
        s.color_primaries = CP_UNSPECIFIED;
        s.transfer_characteristics = TC_UNSPECIFIED;
        s.matrix_coefficients = MC_UNSPECIFIED;
    }
    if (s.mono_chrome) {
        s.color_range = static_cast<int>(br.f(1));
        s.subsampling_x = 1;
        s.subsampling_y = 1;
        s.chroma_sample_position = CSP_UNKNOWN;
        s.separate_uv_delta_q = 0;
        return true;
    } else if (s.color_primaries == CP_BT_709 && s.transfer_characteristics == TC_SRGB && s.matrix_coefficients == MC_IDENTITY) {
        s.color_range = 1;
        s.subsampling_x = 0;
        s.subsampling_y = 0;
    } else {
        s.color_range = static_cast<int>(br.f(1));
        if (s.seq_profile == 0) {
            s.subsampling_x = 1;
            s.subsampling_y = 1;
        } else if (s.seq_profile == 1) {
            s.subsampling_x = 0;
            s.subsampling_y = 0;
        } else {
            if (s.BitDepth == 12) {
                s.subsampling_x = static_cast<int>(br.f(1));
                if (s.subsampling_x) {
                    s.subsampling_y = static_cast<int>(br.f(1));
                } else {
                    s.subsampling_y = 0;
                }
            } else {
                s.subsampling_x = 1;
                s.subsampling_y = 0;
            }
        }
        if (s.subsampling_x && s.subsampling_y) s.chroma_sample_position = static_cast<int>(br.f(2));
    }
    s.separate_uv_delta_q = static_cast<int>(br.f(1));
    return true;
}

bool Decoder::parse_sequence_header(BitReader& br, std::string* error) {
    SequenceHeader s;
    SequenceHeader saved = seq;
    seq = s;
    SequenceHeader& q = seq;
    q.seq_profile = static_cast<int>(br.f(3));
    q.still_picture = static_cast<int>(br.f(1));
    q.reduced_still_picture_header = static_cast<int>(br.f(1));
    if (q.seq_profile > 2) {
        seq = saved;
        if (error) *error = "unsupported AV1 profile";
        return false;
    }
    if (q.reduced_still_picture_header) {
        q.operating_points_cnt_minus_1 = 0;
        q.operating_point_idc[0] = 0;
        q.seq_level_idx[0] = static_cast<int>(br.f(5));
        q.seq_tier[0] = 0;
    } else {
        q.timing_info_present_flag = static_cast<int>(br.f(1));
        if (q.timing_info_present_flag) {
            q.num_units_in_display_tick = br.f(32);
            q.time_scale = br.f(32);
            q.equal_picture_interval = static_cast<int>(br.f(1));
            if (q.equal_picture_interval) q.num_ticks_per_picture_minus_1 = br.uvlc();
            q.decoder_model_info_present_flag = static_cast<int>(br.f(1));
            if (q.decoder_model_info_present_flag) {
                q.buffer_delay_length_minus_1 = static_cast<int>(br.f(5));
                br.f(32);
                q.buffer_removal_time_length_minus_1 = static_cast<int>(br.f(5));
                q.frame_presentation_time_length_minus_1 = static_cast<int>(br.f(5));
            }
        }
        q.initial_display_delay_present_flag = static_cast<int>(br.f(1));
        q.operating_points_cnt_minus_1 = static_cast<int>(br.f(5));
        for (int i = 0; i <= q.operating_points_cnt_minus_1; ++i) {
            q.operating_point_idc[i] = static_cast<int>(br.f(12));
            q.seq_level_idx[i] = static_cast<int>(br.f(5));
            if (q.seq_level_idx[i] > 7) {
                q.seq_tier[i] = static_cast<int>(br.f(1));
            } else {
                q.seq_tier[i] = 0;
            }
            if (q.decoder_model_info_present_flag) {
                q.decoder_model_present_for_this_op[i] = static_cast<int>(br.f(1));
                if (q.decoder_model_present_for_this_op[i]) {
                    const int n = q.buffer_delay_length_minus_1 + 1;
                    br.f(n);
                    br.f(n);
                    br.f(1);
                }
            } else {
                q.decoder_model_present_for_this_op[i] = 0;
            }
            if (q.initial_display_delay_present_flag) {
                if (br.f(1)) br.f(4);
            }
        }
    }
    int op = options.operating_point;
    if (op < 0) op = 0;
    if (op > q.operating_points_cnt_minus_1) op = q.operating_points_cnt_minus_1;
    q.OperatingPointIdc = q.operating_point_idc[op];
    q.frame_width_bits_minus_1 = static_cast<int>(br.f(4));
    q.frame_height_bits_minus_1 = static_cast<int>(br.f(4));
    q.max_frame_width_minus_1 = static_cast<int>(br.f(q.frame_width_bits_minus_1 + 1));
    q.max_frame_height_minus_1 = static_cast<int>(br.f(q.frame_height_bits_minus_1 + 1));
    if (q.reduced_still_picture_header) {
        q.frame_id_numbers_present_flag = 0;
    } else {
        q.frame_id_numbers_present_flag = static_cast<int>(br.f(1));
    }
    if (q.frame_id_numbers_present_flag) {
        q.delta_frame_id_length_minus_2 = static_cast<int>(br.f(4));
        q.additional_frame_id_length_minus_1 = static_cast<int>(br.f(3));
    }
    q.use_128x128_superblock = static_cast<int>(br.f(1));
    q.enable_filter_intra = static_cast<int>(br.f(1));
    q.enable_intra_edge_filter = static_cast<int>(br.f(1));
    if (q.reduced_still_picture_header) {
        q.enable_interintra_compound = 0;
        q.enable_masked_compound = 0;
        q.enable_warped_motion = 0;
        q.enable_dual_filter = 0;
        q.enable_order_hint = 0;
        q.enable_jnt_comp = 0;
        q.enable_ref_frame_mvs = 0;
        q.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        q.seq_force_integer_mv = SELECT_INTEGER_MV;
        q.OrderHintBits = 0;
    } else {
        q.enable_interintra_compound = static_cast<int>(br.f(1));
        q.enable_masked_compound = static_cast<int>(br.f(1));
        q.enable_warped_motion = static_cast<int>(br.f(1));
        q.enable_dual_filter = static_cast<int>(br.f(1));
        q.enable_order_hint = static_cast<int>(br.f(1));
        if (q.enable_order_hint) {
            q.enable_jnt_comp = static_cast<int>(br.f(1));
            q.enable_ref_frame_mvs = static_cast<int>(br.f(1));
        } else {
            q.enable_jnt_comp = 0;
            q.enable_ref_frame_mvs = 0;
        }
        const int seq_choose_screen_content_tools = static_cast<int>(br.f(1));
        if (seq_choose_screen_content_tools) {
            q.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        } else {
            q.seq_force_screen_content_tools = static_cast<int>(br.f(1));
        }
        if (q.seq_force_screen_content_tools > 0) {
            const int seq_choose_integer_mv = static_cast<int>(br.f(1));
            if (seq_choose_integer_mv) {
                q.seq_force_integer_mv = SELECT_INTEGER_MV;
            } else {
                q.seq_force_integer_mv = static_cast<int>(br.f(1));
            }
        } else {
            q.seq_force_integer_mv = SELECT_INTEGER_MV;
        }
        if (q.enable_order_hint) {
            q.OrderHintBits = static_cast<int>(br.f(3)) + 1;
        } else {
            q.OrderHintBits = 0;
        }
    }
    q.enable_superres = static_cast<int>(br.f(1));
    q.enable_cdef = static_cast<int>(br.f(1));
    q.enable_restoration = static_cast<int>(br.f(1));
    if (!parse_color_config(br)) {
        seq = saved;
        if (error) *error = "invalid AV1 color config";
        return false;
    }
    q.film_grain_params_present = static_cast<int>(br.f(1));
    if (br.overrun()) {
        seq = saved;
        if (error) *error = "truncated AV1 sequence header";
        return false;
    }
    have_seq = true;
    info.profile = q.seq_profile;
    info.level = q.seq_level_idx[0];
    info.tier = q.seq_tier[0];
    info.bit_depth = q.BitDepth;
    info.monochrome = q.mono_chrome != 0;
    info.subsampling_x = q.subsampling_x;
    info.subsampling_y = q.subsampling_y;
    info.max_width = static_cast<std::uint32_t>(q.max_frame_width_minus_1 + 1);
    info.max_height = static_cast<std::uint32_t>(q.max_frame_height_minus_1 + 1);
    info.film_grain = q.film_grain_params_present != 0;
    info.still_picture = q.still_picture != 0;
    info.operating_points = q.operating_points_cnt_minus_1 + 1;
    if (q.timing_info_present_flag && q.num_units_in_display_tick && q.equal_picture_interval) {
        info.frame_rate = static_cast<double>(q.time_scale) / (static_cast<double>(q.num_units_in_display_tick) * (static_cast<double>(q.num_ticks_per_picture_minus_1) + 1.0));
    }
    ColorInfo& c = info.color;
    c.primaries = static_cast<std::uint8_t>(q.color_primaries);
    c.transfer = static_cast<std::uint8_t>(q.transfer_characteristics);
    c.matrix_coefficients = static_cast<std::uint8_t>(q.matrix_coefficients);
    c.range = q.color_range ? ColorRange::full : ColorRange::limited;
    switch (q.matrix_coefficients) {
    case 0: c.matrix = ColorMatrix::rgb; break;
    case 1: c.matrix = ColorMatrix::bt709; break;
    case 5: case 6: c.matrix = ColorMatrix::bt601; break;
    case 9: case 10: c.matrix = ColorMatrix::bt2020; break;
    default: c.matrix = ColorMatrix::unspecified; break;
    }
    if (q.chroma_sample_position == CSP_COLOCATED) {
        c.chroma_location = 2;
    } else {
        c.chroma_location = 0;
    }
    if (!q.subsampling_x && !q.subsampling_y) c.chroma_location = 0;
    return true;
}

void Decoder::mark_ref_frames(int idLen) {
    const int diffLen = seq.delta_frame_id_length_minus_2 + 2;
    for (int i = 0; i < NUM_REF_FRAMES; ++i) {
        if (fh.current_frame_id > (1 << diffLen)) {
            if (RefFrameId[i] > fh.current_frame_id || RefFrameId[i] < (fh.current_frame_id - (1 << diffLen))) RefValid[i] = 0;
        } else {
            if (RefFrameId[i] > fh.current_frame_id && RefFrameId[i] < ((1 << idLen) + fh.current_frame_id - (1 << diffLen))) RefValid[i] = 0;
        }
    }
}

void Decoder::superres_params(BitReader& br) {
    if (seq.enable_superres) {
        fh.use_superres = static_cast<int>(br.f(1));
    } else {
        fh.use_superres = 0;
    }
    if (fh.use_superres) {
        const int coded_denom = static_cast<int>(br.f(SUPERRES_DENOM_BITS));
        fh.SuperresDenom = coded_denom + SUPERRES_DENOM_MIN;
    } else {
        fh.SuperresDenom = SUPERRES_NUM;
    }
    fh.UpscaledWidth = fh.FrameWidth;
    fh.FrameWidth = (fh.UpscaledWidth * SUPERRES_NUM + (fh.SuperresDenom / 2)) / fh.SuperresDenom;
}

void Decoder::compute_image_size() {
    fh.MiCols = 2 * ((fh.FrameWidth + 7) >> 3);
    fh.MiRows = 2 * ((fh.FrameHeight + 7) >> 3);
}

bool Decoder::frame_size(BitReader& br) {
    if (fh.frame_size_override_flag) {
        fh.FrameWidth = static_cast<int>(br.f(seq.frame_width_bits_minus_1 + 1)) + 1;
        fh.FrameHeight = static_cast<int>(br.f(seq.frame_height_bits_minus_1 + 1)) + 1;
    } else {
        fh.FrameWidth = seq.max_frame_width_minus_1 + 1;
        fh.FrameHeight = seq.max_frame_height_minus_1 + 1;
    }
    superres_params(br);
    compute_image_size();
    return true;
}

void Decoder::render_size(BitReader& br) {
    const int render_and_frame_size_different = static_cast<int>(br.f(1));
    if (render_and_frame_size_different == 1) {
        fh.RenderWidth = static_cast<int>(br.f(16)) + 1;
        fh.RenderHeight = static_cast<int>(br.f(16)) + 1;
    } else {
        fh.RenderWidth = fh.UpscaledWidth;
        fh.RenderHeight = fh.FrameHeight;
    }
}

bool Decoder::frame_size_with_refs(BitReader& br) {
    int found_ref = 0;
    for (int i = 0; i < REFS_PER_FRAME; ++i) {
        found_ref = static_cast<int>(br.f(1));
        if (found_ref == 1) {
            const SavedFrame* r = refs[fh.ref_frame_idx[i]].get();
            if (!r) return false;
            fh.UpscaledWidth = r->upscaled_width;
            fh.FrameWidth = fh.UpscaledWidth;
            fh.FrameHeight = r->frame_height;
            fh.RenderWidth = r->render_width;
            fh.RenderHeight = r->render_height;
            break;
        }
    }
    if (found_ref == 0) {
        frame_size(br);
        render_size(br);
    } else {
        superres_params(br);
        compute_image_size();
    }
    return true;
}

void Decoder::set_frame_refs() {
    int usedFrame[NUM_REF_FRAMES];
    int shiftedOrderHints[NUM_REF_FRAMES];
    for (int i = 0; i < REFS_PER_FRAME; ++i) fh.ref_frame_idx[i] = -1;
    fh.ref_frame_idx[LAST_FRAME - LAST_FRAME] = fh.last_frame_idx;
    fh.ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = fh.gold_frame_idx;
    for (int i = 0; i < NUM_REF_FRAMES; ++i) usedFrame[i] = 0;
    usedFrame[fh.last_frame_idx] = 1;
    usedFrame[fh.gold_frame_idx] = 1;
    const int curFrameHint = 1 << (seq.OrderHintBits - 1);
    for (int i = 0; i < NUM_REF_FRAMES; ++i) shiftedOrderHints[i] = curFrameHint + get_relative_dist(RefOrderHint[i], fh.OrderHint);
    {
        int ref = -1;
        int latestOrderHint = 0;
        for (int i = 0; i < NUM_REF_FRAMES; ++i) {
            const int hint = shiftedOrderHints[i];
            if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint >= latestOrderHint)) {
                ref = i;
                latestOrderHint = hint;
            }
        }
        if (ref >= 0) {
            fh.ref_frame_idx[ALTREF_FRAME - LAST_FRAME] = ref;
            usedFrame[ref] = 1;
        }
    }
    for (int pass = 0; pass < 2; ++pass) {
        int ref = -1;
        int earliestOrderHint = 0;
        for (int i = 0; i < NUM_REF_FRAMES; ++i) {
            const int hint = shiftedOrderHints[i];
            if (!usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint < earliestOrderHint)) {
                ref = i;
                earliestOrderHint = hint;
            }
        }
        if (ref >= 0) {
            fh.ref_frame_idx[(pass == 0 ? BWDREF_FRAME : ALTREF2_FRAME) - LAST_FRAME] = ref;
            usedFrame[ref] = 1;
        }
    }
    for (int i = 0; i < REFS_PER_FRAME - 2; ++i) {
        const int refFrame = Ref_Frame_List[i];
        if (fh.ref_frame_idx[refFrame - LAST_FRAME] < 0) {
            int ref = -1;
            int latestOrderHint = 0;
            for (int j = 0; j < NUM_REF_FRAMES; ++j) {
                const int hint = shiftedOrderHints[j];
                if (!usedFrame[j] && hint < curFrameHint && (ref < 0 || hint >= latestOrderHint)) {
                    ref = j;
                    latestOrderHint = hint;
                }
            }
            if (ref >= 0) {
                fh.ref_frame_idx[refFrame - LAST_FRAME] = ref;
                usedFrame[ref] = 1;
            }
        }
    }
    int ref = -1;
    int earliestOrderHint = 0;
    for (int i = 0; i < NUM_REF_FRAMES; ++i) {
        const int hint = shiftedOrderHints[i];
        if (ref < 0 || hint < earliestOrderHint) {
            ref = i;
            earliestOrderHint = hint;
        }
    }
    for (int i = 0; i < REFS_PER_FRAME; ++i) {
        if (fh.ref_frame_idx[i] < 0) fh.ref_frame_idx[i] = ref;
    }
}

namespace {

int tile_log2(int blkSize, int target) {
    int k = 0;
    while ((blkSize << k) < target) ++k;
    return k;
}

} // namespace

bool Decoder::tile_info(BitReader& br) {
    const int sbCols = seq.use_128x128_superblock ? ((fh.MiCols + 31) >> 5) : ((fh.MiCols + 15) >> 4);
    const int sbRows = seq.use_128x128_superblock ? ((fh.MiRows + 31) >> 5) : ((fh.MiRows + 15) >> 4);
    const int sbShift = seq.use_128x128_superblock ? 5 : 4;
    const int sbSize = sbShift + 2;
    const int maxTileWidthSb = MAX_TILE_WIDTH >> sbSize;
    int maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSize);
    const int minLog2TileCols = tile_log2(maxTileWidthSb, sbCols);
    const int maxLog2TileCols = tile_log2(1, imin(sbCols, MAX_TILE_COLS));
    const int maxLog2TileRows = tile_log2(1, imin(sbRows, MAX_TILE_ROWS));
    const int minLog2Tiles = imax(minLog2TileCols, tile_log2(maxTileAreaSb, sbRows * sbCols));
    const int uniform_tile_spacing_flag = static_cast<int>(br.f(1));
    if (uniform_tile_spacing_flag) {
        fh.TileColsLog2 = minLog2TileCols;
        while (fh.TileColsLog2 < maxLog2TileCols) {
            if (br.f(1) == 1) {
                ++fh.TileColsLog2;
            } else {
                break;
            }
        }
        const int tileWidthSb = (sbCols + (1 << fh.TileColsLog2) - 1) >> fh.TileColsLog2;
        int i = 0;
        for (int startSb = 0; startSb < sbCols; startSb += tileWidthSb) {
            if (i >= MAX_TILE_COLS) return false;
            fh.MiColStarts[i] = startSb << sbShift;
            ++i;
        }
        fh.MiColStarts[i] = fh.MiCols;
        fh.TileCols = i;
        const int minLog2TileRows = imax(minLog2Tiles - fh.TileColsLog2, 0);
        fh.TileRowsLog2 = minLog2TileRows;
        while (fh.TileRowsLog2 < maxLog2TileRows) {
            if (br.f(1) == 1) {
                ++fh.TileRowsLog2;
            } else {
                break;
            }
        }
        const int tileHeightSb = (sbRows + (1 << fh.TileRowsLog2) - 1) >> fh.TileRowsLog2;
        i = 0;
        for (int startSb = 0; startSb < sbRows; startSb += tileHeightSb) {
            if (i >= MAX_TILE_ROWS) return false;
            fh.MiRowStarts[i] = startSb << sbShift;
            ++i;
        }
        fh.MiRowStarts[i] = fh.MiRows;
        fh.TileRows = i;
    } else {
        int widestTileSb = 0;
        int startSb = 0;
        int i;
        for (i = 0; startSb < sbCols; ++i) {
            if (i >= MAX_TILE_COLS) return false;
            fh.MiColStarts[i] = startSb << sbShift;
            const int maxWidth = imin(sbCols - startSb, maxTileWidthSb);
            const int sizeSb = static_cast<int>(br.ns(static_cast<std::uint32_t>(maxWidth))) + 1;
            widestTileSb = imax(sizeSb, widestTileSb);
            startSb += sizeSb;
        }
        fh.MiColStarts[i] = fh.MiCols;
        fh.TileCols = i;
        fh.TileColsLog2 = tile_log2(1, fh.TileCols);
        if (minLog2Tiles > 0) {
            maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
        } else {
            maxTileAreaSb = sbRows * sbCols;
        }
        const int maxTileHeightSb = imax(maxTileAreaSb / imax(widestTileSb, 1), 1);
        startSb = 0;
        for (i = 0; startSb < sbRows; ++i) {
            if (i >= MAX_TILE_ROWS) return false;
            fh.MiRowStarts[i] = startSb << sbShift;
            const int maxHeight = imin(sbRows - startSb, maxTileHeightSb);
            const int sizeSb = static_cast<int>(br.ns(static_cast<std::uint32_t>(maxHeight))) + 1;
            startSb += sizeSb;
        }
        fh.MiRowStarts[i] = fh.MiRows;
        fh.TileRows = i;
        fh.TileRowsLog2 = tile_log2(1, fh.TileRows);
    }
    for (int i = 0; i < fh.TileCols; ++i) {
        if (fh.MiColStarts[i] >= fh.MiCols) return false;
    }
    for (int i = 0; i < fh.TileRows; ++i) {
        if (fh.MiRowStarts[i] >= fh.MiRows) return false;
    }
    if (fh.TileColsLog2 > 0 || fh.TileRowsLog2 > 0) {
        fh.context_update_tile_id = static_cast<int>(br.f(fh.TileRowsLog2 + fh.TileColsLog2));
        fh.TileSizeBytes = static_cast<int>(br.f(2)) + 1;
        if (fh.context_update_tile_id >= fh.TileCols * fh.TileRows) return false;
    } else {
        fh.context_update_tile_id = 0;
    }
    return true;
}

namespace {

int read_delta_q(BitReader& br) {
    if (br.f(1)) return br.su(7);
    return 0;
}

} // namespace

void Decoder::quantization_params(BitReader& br) {
    fh.base_q_idx = static_cast<int>(br.f(8));
    fh.DeltaQYDc = read_delta_q(br);
    if (seq.NumPlanes > 1) {
        int diff_uv_delta = 0;
        if (seq.separate_uv_delta_q) diff_uv_delta = static_cast<int>(br.f(1));
        fh.DeltaQUDc = read_delta_q(br);
        fh.DeltaQUAc = read_delta_q(br);
        if (diff_uv_delta) {
            fh.DeltaQVDc = read_delta_q(br);
            fh.DeltaQVAc = read_delta_q(br);
        } else {
            fh.DeltaQVDc = fh.DeltaQUDc;
            fh.DeltaQVAc = fh.DeltaQUAc;
        }
    } else {
        fh.DeltaQUDc = 0;
        fh.DeltaQUAc = 0;
        fh.DeltaQVDc = 0;
        fh.DeltaQVAc = 0;
    }
    fh.using_qmatrix = static_cast<int>(br.f(1));
    if (fh.using_qmatrix) {
        fh.qm_y = static_cast<int>(br.f(4));
        fh.qm_u = static_cast<int>(br.f(4));
        if (!seq.separate_uv_delta_q) {
            fh.qm_v = fh.qm_u;
        } else {
            fh.qm_v = static_cast<int>(br.f(4));
        }
    }
}

void Decoder::segmentation_params(BitReader& br) {
    fh.segmentation_enabled = static_cast<int>(br.f(1));
    if (fh.segmentation_enabled == 1) {
        if (fh.primary_ref_frame == PRIMARY_REF_NONE) {
            fh.segmentation_update_map = 1;
            fh.segmentation_temporal_update = 0;
            fh.segmentation_update_data = 1;
        } else {
            fh.segmentation_update_map = static_cast<int>(br.f(1));
            fh.segmentation_temporal_update = 0;
            if (fh.segmentation_update_map == 1) fh.segmentation_temporal_update = static_cast<int>(br.f(1));
            fh.segmentation_update_data = static_cast<int>(br.f(1));
        }
        if (fh.segmentation_update_data == 1) {
            for (int i = 0; i < MAX_SEGMENTS; ++i) {
                for (int j = 0; j < SEG_LVL_MAX; ++j) {
                    const int feature_enabled = static_cast<int>(br.f(1));
                    fh.FeatureEnabled[i][j] = feature_enabled;
                    int clippedValue = 0;
                    if (feature_enabled == 1) {
                        const int bitsToRead = Segmentation_Feature_Bits[j];
                        const int limit = Segmentation_Feature_Max[j];
                        if (Segmentation_Feature_Signed[j] == 1) {
                            const int feature_value = br.su(1 + bitsToRead);
                            clippedValue = clip3(-limit, limit, feature_value);
                        } else {
                            const int feature_value = static_cast<int>(br.f(bitsToRead));
                            clippedValue = clip3(0, limit, feature_value);
                        }
                    }
                    fh.FeatureData[i][j] = clippedValue;
                }
            }
        }
    } else {
        for (int i = 0; i < MAX_SEGMENTS; ++i) {
            for (int j = 0; j < SEG_LVL_MAX; ++j) {
                fh.FeatureEnabled[i][j] = 0;
                fh.FeatureData[i][j] = 0;
            }
        }
    }
    fh.SegIdPreSkip = 0;
    fh.LastActiveSegId = 0;
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
        for (int j = 0; j < SEG_LVL_MAX; ++j) {
            if (fh.FeatureEnabled[i][j]) {
                fh.LastActiveSegId = i;
                if (j >= SEG_LVL_REF_FRAME) fh.SegIdPreSkip = 1;
            }
        }
    }
}

void Decoder::delta_q_params(BitReader& br) {
    fh.delta_q_res = 0;
    fh.delta_q_present = 0;
    if (fh.base_q_idx > 0) fh.delta_q_present = static_cast<int>(br.f(1));
    if (fh.delta_q_present) fh.delta_q_res = static_cast<int>(br.f(2));
}

void Decoder::delta_lf_params(BitReader& br) {
    fh.delta_lf_present = 0;
    fh.delta_lf_res = 0;
    fh.delta_lf_multi = 0;
    if (fh.delta_q_present) {
        if (!fh.allow_intrabc) fh.delta_lf_present = static_cast<int>(br.f(1));
        if (fh.delta_lf_present) {
            fh.delta_lf_res = static_cast<int>(br.f(2));
            fh.delta_lf_multi = static_cast<int>(br.f(1));
        }
    }
}

void Decoder::loop_filter_params(BitReader& br) {
    if (fh.CodedLossless || fh.allow_intrabc) {
        fh.loop_filter_level[0] = 0;
        fh.loop_filter_level[1] = 0;
        fh.loop_filter_ref_deltas[INTRA_FRAME] = 1;
        fh.loop_filter_ref_deltas[LAST_FRAME] = 0;
        fh.loop_filter_ref_deltas[LAST2_FRAME] = 0;
        fh.loop_filter_ref_deltas[LAST3_FRAME] = 0;
        fh.loop_filter_ref_deltas[BWDREF_FRAME] = 0;
        fh.loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
        fh.loop_filter_ref_deltas[ALTREF_FRAME] = -1;
        fh.loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
        fh.loop_filter_mode_deltas[0] = 0;
        fh.loop_filter_mode_deltas[1] = 0;
        return;
    }
    fh.loop_filter_level[0] = static_cast<int>(br.f(6));
    fh.loop_filter_level[1] = static_cast<int>(br.f(6));
    if (seq.NumPlanes > 1) {
        if (fh.loop_filter_level[0] || fh.loop_filter_level[1]) {
            fh.loop_filter_level[2] = static_cast<int>(br.f(6));
            fh.loop_filter_level[3] = static_cast<int>(br.f(6));
        }
    }
    fh.loop_filter_sharpness = static_cast<int>(br.f(3));
    fh.loop_filter_delta_enabled = static_cast<int>(br.f(1));
    if (fh.loop_filter_delta_enabled == 1) {
        fh.loop_filter_delta_update = static_cast<int>(br.f(1));
        if (fh.loop_filter_delta_update == 1) {
            for (int i = 0; i < TOTAL_REFS_PER_FRAME; ++i) {
                if (br.f(1) == 1) fh.loop_filter_ref_deltas[i] = br.su(7);
            }
            for (int i = 0; i < 2; ++i) {
                if (br.f(1) == 1) fh.loop_filter_mode_deltas[i] = br.su(7);
            }
        }
    }
}

void Decoder::cdef_params(BitReader& br) {
    if (fh.CodedLossless || fh.allow_intrabc || !seq.enable_cdef) {
        fh.cdef_bits = 0;
        fh.cdef_y_pri_strength[0] = 0;
        fh.cdef_y_sec_strength[0] = 0;
        fh.cdef_uv_pri_strength[0] = 0;
        fh.cdef_uv_sec_strength[0] = 0;
        fh.cdef_damping = 3;
        return;
    }
    fh.cdef_damping = static_cast<int>(br.f(2)) + 3;
    fh.cdef_bits = static_cast<int>(br.f(2));
    for (int i = 0; i < (1 << fh.cdef_bits); ++i) {
        fh.cdef_y_pri_strength[i] = static_cast<int>(br.f(4));
        fh.cdef_y_sec_strength[i] = static_cast<int>(br.f(2));
        if (fh.cdef_y_sec_strength[i] == 3) fh.cdef_y_sec_strength[i] += 1;
        if (seq.NumPlanes > 1) {
            fh.cdef_uv_pri_strength[i] = static_cast<int>(br.f(4));
            fh.cdef_uv_sec_strength[i] = static_cast<int>(br.f(2));
            if (fh.cdef_uv_sec_strength[i] == 3) fh.cdef_uv_sec_strength[i] += 1;
        }
    }
}

void Decoder::lr_params(BitReader& br) {
    fh.UsesLr = 0;
    fh.FrameRestorationType[0] = RESTORE_NONE;
    fh.FrameRestorationType[1] = RESTORE_NONE;
    fh.FrameRestorationType[2] = RESTORE_NONE;
    if (fh.AllLossless || fh.allow_intrabc || !seq.enable_restoration) return;
    int usesChromaLr = 0;
    for (int i = 0; i < seq.NumPlanes; ++i) {
        const int lr_type = static_cast<int>(br.f(2));
        fh.FrameRestorationType[i] = Remap_Lr_Type[lr_type];
        if (fh.FrameRestorationType[i] != RESTORE_NONE) {
            fh.UsesLr = 1;
            if (i > 0) usesChromaLr = 1;
        }
    }
    if (fh.UsesLr) {
        int lr_unit_shift;
        if (seq.use_128x128_superblock) {
            lr_unit_shift = static_cast<int>(br.f(1));
            ++lr_unit_shift;
        } else {
            lr_unit_shift = static_cast<int>(br.f(1));
            if (lr_unit_shift) lr_unit_shift += static_cast<int>(br.f(1));
        }
        fh.LoopRestorationSize[0] = RESTORATION_TILESIZE_MAX >> (2 - lr_unit_shift);
        int lr_uv_shift = 0;
        if (seq.subsampling_x && seq.subsampling_y && usesChromaLr) lr_uv_shift = static_cast<int>(br.f(1));
        fh.LoopRestorationSize[1] = fh.LoopRestorationSize[0] >> lr_uv_shift;
        fh.LoopRestorationSize[2] = fh.LoopRestorationSize[0] >> lr_uv_shift;
    }
}

void Decoder::read_tx_mode(BitReader& br) {
    if (fh.CodedLossless == 1) {
        fh.TxMode = ONLY_4X4;
    } else {
        fh.TxMode = br.f(1) ? TX_MODE_SELECT : TX_MODE_LARGEST;
    }
}

void Decoder::skip_mode_params(BitReader& br) {
    int skipModeAllowed;
    if (fh.FrameIsIntra || !fh.reference_select || !seq.enable_order_hint) {
        skipModeAllowed = 0;
    } else {
        int forwardIdx = -1;
        int backwardIdx = -1;
        int forwardHint = 0;
        int backwardHint = 0;
        for (int i = 0; i < REFS_PER_FRAME; ++i) {
            const int refHint = RefOrderHint[fh.ref_frame_idx[i]];
            if (get_relative_dist(refHint, fh.OrderHint) < 0) {
                if (forwardIdx < 0 || get_relative_dist(refHint, forwardHint) > 0) {
                    forwardIdx = i;
                    forwardHint = refHint;
                }
            } else if (get_relative_dist(refHint, fh.OrderHint) > 0) {
                if (backwardIdx < 0 || get_relative_dist(refHint, backwardHint) < 0) {
                    backwardIdx = i;
                    backwardHint = refHint;
                }
            }
        }
        if (forwardIdx < 0) {
            skipModeAllowed = 0;
        } else if (backwardIdx >= 0) {
            skipModeAllowed = 1;
            fh.SkipModeFrame[0] = LAST_FRAME + imin(forwardIdx, backwardIdx);
            fh.SkipModeFrame[1] = LAST_FRAME + imax(forwardIdx, backwardIdx);
        } else {
            int secondForwardIdx = -1;
            int secondForwardHint = 0;
            for (int i = 0; i < REFS_PER_FRAME; ++i) {
                const int refHint = RefOrderHint[fh.ref_frame_idx[i]];
                if (get_relative_dist(refHint, forwardHint) < 0) {
                    if (secondForwardIdx < 0 || get_relative_dist(refHint, secondForwardHint) > 0) {
                        secondForwardIdx = i;
                        secondForwardHint = refHint;
                    }
                }
            }
            if (secondForwardIdx < 0) {
                skipModeAllowed = 0;
            } else {
                skipModeAllowed = 1;
                fh.SkipModeFrame[0] = LAST_FRAME + imin(forwardIdx, secondForwardIdx);
                fh.SkipModeFrame[1] = LAST_FRAME + imax(forwardIdx, secondForwardIdx);
            }
        }
    }
    if (skipModeAllowed) {
        fh.skip_mode_present = static_cast<int>(br.f(1));
    } else {
        fh.skip_mode_present = 0;
    }
}

int Decoder::decode_subexp(BitReader& br, int numSyms) {
    int i = 0;
    int mk = 0;
    const int k = 3;
    while (true) {
        const int b2 = i ? k + i - 1 : k;
        const int a = 1 << b2;
        if (numSyms <= mk + 3 * a) {
            return static_cast<int>(br.ns(static_cast<std::uint32_t>(numSyms - mk))) + mk;
        } else {
            if (br.f(1)) {
                ++i;
                mk += a;
            } else {
                return static_cast<int>(br.f(b2)) + mk;
            }
        }
        if (br.overrun()) return 0;
    }
}

namespace {

int inverse_recenter(int r, int v) {
    if (v > 2 * r) return v;
    if (v & 1) return r - ((v + 1) >> 1);
    return r + (v >> 1);
}

} // namespace

int Decoder::decode_unsigned_subexp_with_ref(BitReader& br, int mx, int r) {
    const int v = decode_subexp(br, mx);
    if ((r << 1) <= mx) return inverse_recenter(r, v);
    return mx - 1 - inverse_recenter(mx - 1 - r, v);
}

int Decoder::decode_signed_subexp_with_ref(BitReader& br, int low, int high, int r) {
    const int x = decode_unsigned_subexp_with_ref(br, high - low, r - low);
    return x + low;
}

void Decoder::read_global_param(BitReader& br, int type, int ref, int idx) {
    int absBits = GM_ABS_ALPHA_BITS;
    int precBits = GM_ALPHA_PREC_BITS;
    if (idx < 2) {
        if (type == TRANSLATION) {
            absBits = GM_ABS_TRANS_ONLY_BITS - !fh.allow_high_precision_mv;
            precBits = GM_TRANS_ONLY_PREC_BITS - !fh.allow_high_precision_mv;
        } else {
            absBits = GM_ABS_TRANS_BITS;
            precBits = GM_TRANS_PREC_BITS;
        }
    }
    const int precDiff = WARPEDMODEL_PREC_BITS - precBits;
    const int round = (idx % 3) == 2 ? (1 << WARPEDMODEL_PREC_BITS) : 0;
    const int sub = (idx % 3) == 2 ? (1 << precBits) : 0;
    const int mx = 1 << absBits;
    const int r = (fh.PrevGmParams[ref][idx] >> precDiff) - sub;
    fh.gm_params[ref][idx] = (decode_signed_subexp_with_ref(br, -mx, mx + 1, r) * (1 << precDiff)) + round;
}

void Decoder::global_motion_params(BitReader& br) {
    for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) {
        fh.GmType[ref] = IDENTITY;
        for (int i = 0; i < 6; ++i) fh.gm_params[ref][i] = (i % 3 == 2) ? 1 << WARPEDMODEL_PREC_BITS : 0;
    }
    if (fh.FrameIsIntra) return;
    for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) {
        int type;
        const int is_global = static_cast<int>(br.f(1));
        if (is_global) {
            const int is_rot_zoom = static_cast<int>(br.f(1));
            if (is_rot_zoom) {
                type = ROTZOOM;
            } else {
                const int is_translation = static_cast<int>(br.f(1));
                type = is_translation ? TRANSLATION : AFFINE;
            }
        } else {
            type = IDENTITY;
        }
        fh.GmType[ref] = type;
        if (type >= ROTZOOM) {
            read_global_param(br, type, ref, 2);
            read_global_param(br, type, ref, 3);
            if (type == AFFINE) {
                read_global_param(br, type, ref, 4);
                read_global_param(br, type, ref, 5);
            } else {
                fh.gm_params[ref][4] = -fh.gm_params[ref][3];
                fh.gm_params[ref][5] = fh.gm_params[ref][2];
            }
        }
        if (type >= TRANSLATION) {
            read_global_param(br, type, ref, 0);
            read_global_param(br, type, ref, 1);
        }
    }
}

void Decoder::load_grain_params(int idx) {
    if (refs[idx]) {
        fh.film_grain = refs[idx]->grain;
    } else {
        fh.film_grain = FilmGrainParams();
    }
}

void Decoder::film_grain_params(BitReader& br) {
    FilmGrainParams& g = fh.film_grain;
    if (!seq.film_grain_params_present || (!fh.show_frame && !fh.showable_frame)) {
        g = FilmGrainParams();
        return;
    }
    g.apply_grain = static_cast<int>(br.f(1));
    if (!g.apply_grain) {
        g = FilmGrainParams();
        return;
    }
    g.grain_seed = static_cast<int>(br.f(16));
    if (fh.frame_type == INTER_FRAME) {
        g.update_grain = static_cast<int>(br.f(1));
    } else {
        g.update_grain = 1;
    }
    if (!g.update_grain) {
        const int film_grain_params_ref_idx = static_cast<int>(br.f(3));
        const int tempGrainSeed = g.grain_seed;
        load_grain_params(film_grain_params_ref_idx);
        g.grain_seed = tempGrainSeed;
        return;
    }
    g.num_y_points = static_cast<int>(br.f(4));
    if (g.num_y_points > 14) g.num_y_points = 14;
    for (int i = 0; i < g.num_y_points; ++i) {
        g.point_y_value[i] = static_cast<int>(br.f(8));
        g.point_y_scaling[i] = static_cast<int>(br.f(8));
    }
    if (seq.mono_chrome) {
        g.chroma_scaling_from_luma = 0;
    } else {
        g.chroma_scaling_from_luma = static_cast<int>(br.f(1));
    }
    if (seq.mono_chrome || g.chroma_scaling_from_luma || (seq.subsampling_x == 1 && seq.subsampling_y == 1 && g.num_y_points == 0)) {
        g.num_cb_points = 0;
        g.num_cr_points = 0;
    } else {
        g.num_cb_points = static_cast<int>(br.f(4));
        if (g.num_cb_points > 10) g.num_cb_points = 10;
        for (int i = 0; i < g.num_cb_points; ++i) {
            g.point_cb_value[i] = static_cast<int>(br.f(8));
            g.point_cb_scaling[i] = static_cast<int>(br.f(8));
        }
        g.num_cr_points = static_cast<int>(br.f(4));
        if (g.num_cr_points > 10) g.num_cr_points = 10;
        for (int i = 0; i < g.num_cr_points; ++i) {
            g.point_cr_value[i] = static_cast<int>(br.f(8));
            g.point_cr_scaling[i] = static_cast<int>(br.f(8));
        }
    }
    g.grain_scaling_minus_8 = static_cast<int>(br.f(2));
    g.ar_coeff_lag = static_cast<int>(br.f(2));
    const int numPosLuma = 2 * g.ar_coeff_lag * (g.ar_coeff_lag + 1);
    int numPosChroma;
    if (g.num_y_points) {
        numPosChroma = numPosLuma + 1;
        for (int i = 0; i < numPosLuma; ++i) g.ar_coeffs_y_plus_128[i] = static_cast<int>(br.f(8));
    } else {
        numPosChroma = numPosLuma;
    }
    if (g.chroma_scaling_from_luma || g.num_cb_points) {
        for (int i = 0; i < numPosChroma; ++i) g.ar_coeffs_cb_plus_128[i] = static_cast<int>(br.f(8));
    }
    if (g.chroma_scaling_from_luma || g.num_cr_points) {
        for (int i = 0; i < numPosChroma; ++i) g.ar_coeffs_cr_plus_128[i] = static_cast<int>(br.f(8));
    }
    g.ar_coeff_shift_minus_6 = static_cast<int>(br.f(2));
    g.grain_scale_shift = static_cast<int>(br.f(2));
    if (g.num_cb_points) {
        g.cb_mult = static_cast<int>(br.f(8));
        g.cb_luma_mult = static_cast<int>(br.f(8));
        g.cb_offset = static_cast<int>(br.f(9));
    }
    if (g.num_cr_points) {
        g.cr_mult = static_cast<int>(br.f(8));
        g.cr_luma_mult = static_cast<int>(br.f(8));
        g.cr_offset = static_cast<int>(br.f(9));
    }
    g.overlap_flag = static_cast<int>(br.f(1));
    g.clip_to_restricted_range = static_cast<int>(br.f(1));
}

void Decoder::setup_past_independence() {
    for (int i = 0; i < MAX_SEGMENTS; ++i) {
        for (int j = 0; j < SEG_LVL_MAX; ++j) {
            fh.FeatureData[i][j] = 0;
            fh.FeatureEnabled[i][j] = 0;
        }
    }
    for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) {
        for (int i = 0; i < 6; ++i) fh.PrevGmParams[ref][i] = (i % 3 == 2) ? 1 << WARPEDMODEL_PREC_BITS : 0;
    }
    fh.loop_filter_delta_enabled = 1;
    fh.loop_filter_ref_deltas[INTRA_FRAME] = 1;
    fh.loop_filter_ref_deltas[LAST_FRAME] = 0;
    fh.loop_filter_ref_deltas[LAST2_FRAME] = 0;
    fh.loop_filter_ref_deltas[LAST3_FRAME] = 0;
    fh.loop_filter_ref_deltas[BWDREF_FRAME] = 0;
    fh.loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
    fh.loop_filter_ref_deltas[ALTREF_FRAME] = -1;
    fh.loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
    fh.loop_filter_mode_deltas[0] = 0;
    fh.loop_filter_mode_deltas[1] = 0;
}

void Decoder::load_previous() {
    const int prevFrame = fh.ref_frame_idx[fh.primary_ref_frame];
    const SavedFrame* p = refs[prevFrame].get();
    if (!p) {
        setup_past_independence();
        return;
    }
    std::memcpy(fh.PrevGmParams, p->gm_params, sizeof(fh.PrevGmParams));
    std::memcpy(fh.loop_filter_ref_deltas, p->loop_filter_ref_deltas, sizeof(fh.loop_filter_ref_deltas));
    std::memcpy(fh.loop_filter_mode_deltas, p->loop_filter_mode_deltas, sizeof(fh.loop_filter_mode_deltas));
    std::memcpy(fh.FeatureEnabled, p->feature_enabled, sizeof(fh.FeatureEnabled));
    std::memcpy(fh.FeatureData, p->feature_data, sizeof(fh.FeatureData));
}

void Decoder::load_previous_segment_ids() {
    const std::size_t n = static_cast<std::size_t>(fh.MiRows) * static_cast<std::size_t>(fh.MiCols);
    prev_segment_ids.assign(n, 0);
    if (fh.primary_ref_frame == PRIMARY_REF_NONE) return;
    const int prevFrame = fh.ref_frame_idx[fh.primary_ref_frame];
    const SavedFrame* p = refs[prevFrame].get();
    if (p && fh.segmentation_enabled && p->mi_cols == fh.MiCols && p->mi_rows == fh.MiRows && p->segment_ids.size() == n) {
        prev_segment_ids = p->segment_ids;
    }
}

int Decoder::get_qindex(int ignoreDeltaQ, int segmentId, int currentQIndex) const noexcept {
    if (fh.segmentation_enabled && fh.FeatureEnabled[segmentId][SEG_LVL_ALT_Q]) {
        const int data = fh.FeatureData[segmentId][SEG_LVL_ALT_Q];
        int qindex = fh.base_q_idx + data;
        if (ignoreDeltaQ == 0 && fh.delta_q_present == 1) qindex = currentQIndex + data;
        return clip3(0, 255, qindex);
    }
    if (ignoreDeltaQ == 0 && fh.delta_q_present == 1) return currentQIndex;
    return fh.base_q_idx;
}

bool Decoder::uncompressed_header(BitReader& br, std::string* error) {
    FrameHeader prev = fh;
    fh = FrameHeader();
    fh.temporal_id = temporal_id;
    fh.spatial_id = spatial_id;
    fh.current_frame_id = prev.current_frame_id;
    int idLen = 0;
    if (seq.frame_id_numbers_present_flag) idLen = seq.additional_frame_id_length_minus_1 + seq.delta_frame_id_length_minus_2 + 3;
    const int allFrames = (1 << NUM_REF_FRAMES) - 1;
    if (seq.reduced_still_picture_header) {
        fh.show_existing_frame = 0;
        fh.frame_type = KEY_FRAME;
        fh.FrameIsIntra = 1;
        fh.show_frame = 1;
        fh.showable_frame = 0;
    } else {
        fh.show_existing_frame = static_cast<int>(br.f(1));
        if (fh.show_existing_frame == 1) {
            fh.frame_to_show_map_idx = static_cast<int>(br.f(3));
            if (seq.decoder_model_info_present_flag && !seq.equal_picture_interval) br.f(seq.frame_presentation_time_length_minus_1 + 1);
            fh.refresh_frame_flags = 0;
            if (seq.frame_id_numbers_present_flag) br.f(idLen);
            if (!refs[fh.frame_to_show_map_idx]) {
                if (error) *error = "show_existing_frame references an empty slot";
                return false;
            }
            fh.frame_type = RefFrameType[fh.frame_to_show_map_idx];
            if (fh.frame_type == KEY_FRAME) fh.refresh_frame_flags = allFrames;
            if (seq.film_grain_params_present) load_grain_params(fh.frame_to_show_map_idx);
            return true;
        }
        fh.frame_type = static_cast<int>(br.f(2));
        fh.FrameIsIntra = (fh.frame_type == INTRA_ONLY_FRAME || fh.frame_type == KEY_FRAME);
        fh.show_frame = static_cast<int>(br.f(1));
        if (fh.show_frame && seq.decoder_model_info_present_flag && !seq.equal_picture_interval) br.f(seq.frame_presentation_time_length_minus_1 + 1);
        if (fh.show_frame) {
            fh.showable_frame = fh.frame_type != KEY_FRAME;
        } else {
            fh.showable_frame = static_cast<int>(br.f(1));
        }
        if (fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame)) {
            fh.error_resilient_mode = 1;
        } else {
            fh.error_resilient_mode = static_cast<int>(br.f(1));
        }
    }
    if (fh.frame_type == KEY_FRAME && fh.show_frame) {
        for (int i = 0; i < NUM_REF_FRAMES; ++i) {
            RefValid[i] = 0;
            RefOrderHint[i] = 0;
        }
        for (int i = 0; i < REFS_PER_FRAME; ++i) fh.OrderHints[LAST_FRAME + i] = 0;
    }
    fh.disable_cdf_update = static_cast<int>(br.f(1));
    if (seq.seq_force_screen_content_tools == SELECT_SCREEN_CONTENT_TOOLS) {
        fh.allow_screen_content_tools = static_cast<int>(br.f(1));
    } else {
        fh.allow_screen_content_tools = seq.seq_force_screen_content_tools;
    }
    if (fh.allow_screen_content_tools) {
        if (seq.seq_force_integer_mv == SELECT_INTEGER_MV) {
            fh.force_integer_mv = static_cast<int>(br.f(1));
        } else {
            fh.force_integer_mv = seq.seq_force_integer_mv;
        }
    } else {
        fh.force_integer_mv = 0;
    }
    if (fh.FrameIsIntra) fh.force_integer_mv = 1;
    if (seq.frame_id_numbers_present_flag) {
        fh.current_frame_id = static_cast<int>(br.f(idLen));
        mark_ref_frames(idLen);
    } else {
        fh.current_frame_id = 0;
    }
    if (fh.frame_type == SWITCH_FRAME) {
        fh.frame_size_override_flag = 1;
    } else if (seq.reduced_still_picture_header) {
        fh.frame_size_override_flag = 0;
    } else {
        fh.frame_size_override_flag = static_cast<int>(br.f(1));
    }
    fh.OrderHint = static_cast<int>(br.f(seq.OrderHintBits));
    if (fh.FrameIsIntra || fh.error_resilient_mode) {
        fh.primary_ref_frame = PRIMARY_REF_NONE;
    } else {
        fh.primary_ref_frame = static_cast<int>(br.f(3));
    }
    if (seq.decoder_model_info_present_flag) {
        const int buffer_removal_time_present_flag = static_cast<int>(br.f(1));
        if (buffer_removal_time_present_flag) {
            for (int opNum = 0; opNum <= seq.operating_points_cnt_minus_1; ++opNum) {
                if (seq.decoder_model_present_for_this_op[opNum]) {
                    const int opPtIdc = seq.operating_point_idc[opNum];
                    const int inTemporalLayer = (opPtIdc >> temporal_id) & 1;
                    const int inSpatialLayer = (opPtIdc >> (spatial_id + 8)) & 1;
                    if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer)) br.f(seq.buffer_removal_time_length_minus_1 + 1);
                }
            }
        }
    }
    fh.allow_high_precision_mv = 0;
    fh.use_ref_frame_mvs = 0;
    fh.allow_intrabc = 0;
    if (fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame)) {
        fh.refresh_frame_flags = allFrames;
    } else {
        fh.refresh_frame_flags = static_cast<int>(br.f(8));
    }
    if (!fh.FrameIsIntra || fh.refresh_frame_flags != allFrames) {
        if (fh.error_resilient_mode && seq.enable_order_hint) {
            for (int i = 0; i < NUM_REF_FRAMES; ++i) {
                fh.ref_order_hint[i] = static_cast<int>(br.f(seq.OrderHintBits));
                if (fh.ref_order_hint[i] != RefOrderHint[i]) {
                    RefValid[i] = 0;
                    RefOrderHint[i] = fh.ref_order_hint[i];
                }
            }
        }
    }
    if (fh.FrameIsIntra) {
        frame_size(br);
        render_size(br);
        if (fh.allow_screen_content_tools && fh.UpscaledWidth == fh.FrameWidth) fh.allow_intrabc = static_cast<int>(br.f(1));
    } else {
        if (!seq.enable_order_hint) {
            fh.frame_refs_short_signaling = 0;
        } else {
            fh.frame_refs_short_signaling = static_cast<int>(br.f(1));
            if (fh.frame_refs_short_signaling) {
                fh.last_frame_idx = static_cast<int>(br.f(3));
                fh.gold_frame_idx = static_cast<int>(br.f(3));
                set_frame_refs();
            }
        }
        for (int i = 0; i < REFS_PER_FRAME; ++i) {
            if (!fh.frame_refs_short_signaling) fh.ref_frame_idx[i] = static_cast<int>(br.f(3));
            if (seq.frame_id_numbers_present_flag) br.f(seq.delta_frame_id_length_minus_2 + 2);
        }
        for (int i = 0; i < REFS_PER_FRAME; ++i) {
            const SavedFrame* r = refs[fh.ref_frame_idx[i]].get();
            if (!r) {
                if (error) *error = "AV1 inter frame references a missing frame";
                return false;
            }
            if (r->bit_depth != seq.BitDepth || r->subsampling_x != seq.subsampling_x || r->subsampling_y != seq.subsampling_y || r->frame.num_planes != seq.NumPlanes) {
                if (error) *error = "AV1 reference frame format mismatch";
                return false;
            }
        }
        if (fh.frame_size_override_flag && !fh.error_resilient_mode) {
            if (!frame_size_with_refs(br)) {
                if (error) *error = "invalid AV1 frame size reference";
                return false;
            }
        } else {
            frame_size(br);
            render_size(br);
        }
        if (fh.force_integer_mv) {
            fh.allow_high_precision_mv = 0;
        } else {
            fh.allow_high_precision_mv = static_cast<int>(br.f(1));
        }
        fh.is_filter_switchable = static_cast<int>(br.f(1));
        if (fh.is_filter_switchable == 1) {
            fh.interpolation_filter = SWITCHABLE;
        } else {
            fh.interpolation_filter = static_cast<int>(br.f(2));
        }
        fh.is_motion_mode_switchable = static_cast<int>(br.f(1));
        if (fh.error_resilient_mode || !seq.enable_ref_frame_mvs) {
            fh.use_ref_frame_mvs = 0;
        } else {
            fh.use_ref_frame_mvs = static_cast<int>(br.f(1));
        }
        for (int i = 0; i < REFS_PER_FRAME; ++i) {
            const int refFrame = LAST_FRAME + i;
            const int hint = RefOrderHint[fh.ref_frame_idx[i]];
            fh.OrderHints[refFrame] = hint;
            if (!seq.enable_order_hint) {
                fh.RefFrameSignBias[refFrame] = 0;
            } else {
                fh.RefFrameSignBias[refFrame] = get_relative_dist(hint, fh.OrderHint) > 0;
            }
        }
        for (int i = 0; i < REFS_PER_FRAME; ++i) {
            const SavedFrame* r = refs[fh.ref_frame_idx[i]].get();
            if (2 * fh.FrameWidth < r->upscaled_width || 2 * fh.FrameHeight < r->frame_height || fh.FrameWidth > 16 * r->upscaled_width || fh.FrameHeight > 16 * r->frame_height) {
                if (error) *error = "AV1 reference scaling out of range";
                return false;
            }
        }
    }
    if (fh.FrameWidth <= 0 || fh.FrameHeight <= 0 || fh.FrameWidth > 65536 || fh.FrameHeight > 65536 || static_cast<std::int64_t>(fh.UpscaledWidth) * fh.FrameHeight > (std::int64_t(1) << 28)) {
        if (error) *error = "unsupported AV1 frame size";
        return false;
    }
    if (seq.reduced_still_picture_header || fh.disable_cdf_update) {
        fh.disable_frame_end_update_cdf = 1;
    } else {
        fh.disable_frame_end_update_cdf = static_cast<int>(br.f(1));
    }
    if (fh.primary_ref_frame == PRIMARY_REF_NONE) {
        cdf.init_non_coeff();
        setup_past_independence();
    } else {
        const SavedFrame* p = refs[fh.ref_frame_idx[fh.primary_ref_frame]].get();
        if (p) {
            cdf = p->cdfs;
            cdf.reset_counters();
        } else {
            cdf.init_non_coeff();
        }
        load_previous();
    }
    if (!tile_info(br)) {
        if (error) *error = "invalid AV1 tile info";
        return false;
    }
    quantization_params(br);
    segmentation_params(br);
    delta_q_params(br);
    delta_lf_params(br);
    if (fh.primary_ref_frame == PRIMARY_REF_NONE) {
        cdf.init_coeff(fh.base_q_idx);
    }
    load_previous_segment_ids();
    fh.CodedLossless = 1;
    for (int segmentId = 0; segmentId < MAX_SEGMENTS; ++segmentId) {
        const int qindex = get_qindex(1, segmentId, fh.base_q_idx);
        fh.LosslessArray[segmentId] = qindex == 0 && fh.DeltaQYDc == 0 && fh.DeltaQUAc == 0 && fh.DeltaQUDc == 0 && fh.DeltaQVAc == 0 && fh.DeltaQVDc == 0;
        if (!fh.LosslessArray[segmentId]) fh.CodedLossless = 0;
        if (fh.using_qmatrix) {
            if (fh.LosslessArray[segmentId]) {
                fh.SegQMLevel[0][segmentId] = 15;
                fh.SegQMLevel[1][segmentId] = 15;
                fh.SegQMLevel[2][segmentId] = 15;
            } else {
                fh.SegQMLevel[0][segmentId] = fh.qm_y;
                fh.SegQMLevel[1][segmentId] = fh.qm_u;
                fh.SegQMLevel[2][segmentId] = fh.qm_v;
            }
        }
    }
    fh.AllLossless = fh.CodedLossless && (fh.FrameWidth == fh.UpscaledWidth);
    loop_filter_params(br);
    cdef_params(br);
    lr_params(br);
    read_tx_mode(br);
    if (fh.FrameIsIntra) {
        fh.reference_select = 0;
    } else {
        fh.reference_select = static_cast<int>(br.f(1));
    }
    skip_mode_params(br);
    if (fh.FrameIsIntra || fh.error_resilient_mode || !seq.enable_warped_motion) {
        fh.allow_warped_motion = 0;
    } else {
        fh.allow_warped_motion = static_cast<int>(br.f(1));
    }
    fh.reduced_tx_set = static_cast<int>(br.f(1));
    global_motion_params(br);
    film_grain_params(br);
    if (br.overrun()) {
        if (error) *error = "truncated AV1 frame header";
        return false;
    }
    return true;
}

bool Decoder::frame_header_obu(BitReader& br, std::string* error) {
    if (SeenFrameHeader == 1) {
        return frame_header_valid;
    }
    SeenFrameHeader = 1;
    frame_header_valid = false;
    if (!have_seq) {
        if (error) *error = "AV1 frame header before sequence header";
        return false;
    }
    if (!uncompressed_header(br, error)) return false;
    frame_header_valid = true;
    if (fh.show_existing_frame) {
        show_existing_frame();
        SeenFrameHeader = 0;
        frame_header_valid = false;
    } else {
        TileNum = 0;
        SeenFrameHeader = 1;
        if (!setup_frame()) {
            frame_header_valid = false;
            if (error) *error = "AV1 frame setup failed";
            return false;
        }
    }
    return true;
}

bool Decoder::decode_obu(const std::uint8_t* data, std::size_t size, std::size_t* consumed, std::string* error) {
    BitReader hb(data, size);
    if (size < 1) {
        *consumed = size;
        return false;
    }
    const int obu_forbidden_bit = static_cast<int>(hb.f(1));
    const int obu_type = static_cast<int>(hb.f(4));
    const int obu_extension_flag = static_cast<int>(hb.f(1));
    const int obu_has_size_field = static_cast<int>(hb.f(1));
    hb.f(1);
    int tid = 0;
    int sid = 0;
    if (obu_extension_flag) {
        if (size < 2) {
            *consumed = size;
            return false;
        }
        tid = static_cast<int>(hb.f(3));
        sid = static_cast<int>(hb.f(2));
        hb.f(3);
    }
    std::size_t header = obu_extension_flag ? 2 : 1;
    std::uint64_t obu_size;
    if (obu_has_size_field) {
        BitReader lb(data + header, size - header);
        obu_size = lb.leb128();
        const std::size_t lebBytes = lb.position() / 8;
        if (lb.overrun()) {
            *consumed = size;
            if (error) *error = "truncated AV1 OBU size";
            return false;
        }
        header += lebBytes;
    } else {
        obu_size = size - header;
    }
    if (obu_size > size - header) {
        *consumed = size;
        if (error) *error = "AV1 OBU size exceeds the packet";
        return false;
    }
    *consumed = header + static_cast<std::size_t>(obu_size);
    if (obu_forbidden_bit) return true;
    const std::uint8_t* payload = data + header;
    const std::size_t psize = static_cast<std::size_t>(obu_size);
    if (obu_type != OBU_SEQUENCE_HEADER && obu_type != OBU_TEMPORAL_DELIMITER && have_seq && seq.OperatingPointIdc != 0 && obu_extension_flag == 1) {
        const int inTemporalLayer = (seq.OperatingPointIdc >> tid) & 1;
        const int inSpatialLayer = (seq.OperatingPointIdc >> (sid + 8)) & 1;
        if (!inTemporalLayer || !inSpatialLayer) {
            AV1_STAT(ST_LAYER_DROP);
            return true;
        }
    }
    temporal_id = tid;
    spatial_id = sid;
    BitReader br(payload, psize);
    switch (obu_type) {
    case OBU_SEQUENCE_HEADER:
        return parse_sequence_header(br, error);
    case OBU_TEMPORAL_DELIMITER:
        SeenFrameHeader = 0;
        return true;
    case OBU_FRAME_HEADER:
    case OBU_REDUNDANT_FRAME_HEADER:
        return frame_header_obu(br, error);
    case OBU_FRAME: {
        if (!frame_header_obu(br, error)) return false;
        br.byte_alignment();
        const std::size_t headerBytes = br.position() / 8;
        if (headerBytes > psize) return false;
        BitReader tb(payload + headerBytes, psize - headerBytes);
        return tile_group_obu(tb, payload + headerBytes, psize - headerBytes, error);
    }
    case OBU_TILE_GROUP:
        if (!frame_header_valid || !SeenFrameHeader) return false;
        return tile_group_obu(br, payload, psize, error);
    default:
        return true;
    }
}

bool Decoder::disposable(const std::uint8_t* data, std::size_t size) const noexcept {
    if (!have_seq || !data) return false;
    std::size_t pos = 0;
    bool sawFrame = false;
    while (pos < size) {
        BitReader hb(data + pos, size - pos);
        if (hb.f(1)) return false;
        const int type = static_cast<int>(hb.f(4));
        const int ext = static_cast<int>(hb.f(1));
        const int hasSize = static_cast<int>(hb.f(1));
        hb.f(1);
        int tid = 0;
        int sid = 0;
        if (ext) {
            tid = static_cast<int>(hb.f(3));
            sid = static_cast<int>(hb.f(2));
            hb.f(3);
        }
        std::size_t header = ext ? 2 : 1;
        std::uint64_t obuSize;
        if (hasSize) {
            BitReader lb(data + pos + header, size - pos - header);
            obuSize = lb.leb128();
            if (lb.overrun()) return false;
            header += lb.position() / 8;
        } else {
            obuSize = size - pos - header;
        }
        if (header > size - pos || obuSize > size - pos - header) return false;
        const std::size_t next = pos + header + static_cast<std::size_t>(obuSize);
        if (type == OBU_SEQUENCE_HEADER) return false;
        if (type == OBU_FRAME_HEADER || type == OBU_FRAME) {
            if (seq.OperatingPointIdc != 0 && ext) {
                if (!((seq.OperatingPointIdc >> tid) & 1) || !((seq.OperatingPointIdc >> (sid + 8)) & 1)) {
                    pos = next;
                    continue;
                }
            }
            sawFrame = true;
            BitReader br(data + pos + header, static_cast<std::size_t>(obuSize));
            if (seq.reduced_still_picture_header) return false;
            if (br.f(1)) {
                const int idx = static_cast<int>(br.f(3));
                if (RefFrameType[idx] == KEY_FRAME) return false;
                pos = next;
                continue;
            }
            const int frame_type = static_cast<int>(br.f(2));
            const int show_frame = static_cast<int>(br.f(1));
            if (frame_type == KEY_FRAME || frame_type == SWITCH_FRAME) return false;
            const int intra = frame_type == INTRA_ONLY_FRAME;
            if (show_frame && seq.decoder_model_info_present_flag && !seq.equal_picture_interval) br.f(seq.frame_presentation_time_length_minus_1 + 1);
            if (!show_frame) br.f(1);
            const int error_resilient_mode = static_cast<int>(br.f(1));
            br.f(1);
            int allow_sct;
            if (seq.seq_force_screen_content_tools == SELECT_SCREEN_CONTENT_TOOLS) {
                allow_sct = static_cast<int>(br.f(1));
            } else {
                allow_sct = seq.seq_force_screen_content_tools;
            }
            if (allow_sct && seq.seq_force_integer_mv == SELECT_INTEGER_MV) br.f(1);
            if (seq.frame_id_numbers_present_flag) br.f(seq.additional_frame_id_length_minus_1 + seq.delta_frame_id_length_minus_2 + 3);
            br.f(1);
            br.f(seq.OrderHintBits);
            if (!intra && !error_resilient_mode) br.f(3);
            if (seq.decoder_model_info_present_flag) {
                if (br.f(1)) {
                    for (int op = 0; op <= seq.operating_points_cnt_minus_1; ++op) {
                        if (seq.decoder_model_present_for_this_op[op]) {
                            const int idc = seq.operating_point_idc[op];
                            if (idc == 0 || (((idc >> tid) & 1) && ((idc >> (sid + 8)) & 1))) br.f(seq.buffer_removal_time_length_minus_1 + 1);
                        }
                    }
                }
            }
            const int refresh = static_cast<int>(br.f(8));
            if (br.overrun() || refresh != 0) return false;
        }
        pos = next;
    }
    return sawFrame;
}

bool Decoder::decode_temporal_unit(const std::uint8_t* data, std::size_t size, std::string* error) {
    std::size_t pos = 0;
    bool ok = true;
    tu_frames.clear();
    while (pos < size) {
        std::size_t consumed = 0;
        std::string err;
        if (!decode_obu(data + pos, size - pos, &consumed, &err)) {
            ok = false;
            ++errors;
            if (error && error->empty()) *error = err.empty() ? std::string("invalid AV1 OBU") : err;
            if (frame_in_progress) {
                frame_in_progress = false;
                SeenFrameHeader = 0;
                frame_header_valid = false;
            }
        }
        if (consumed == 0) break;
        pos += consumed;
    }
    if (!tu_frames.empty()) {
        if (options.output_all_layers || seq.OperatingPointIdc == 0) {
            for (VideoFrame& f : tu_frames) output.push_back(std::move(f));
        } else {
            output.push_back(std::move(tu_frames.back()));
        }
        tu_frames.clear();
    }
    return ok;
}

} // namespace av1
} // namespace video
} // namespace fizmo
