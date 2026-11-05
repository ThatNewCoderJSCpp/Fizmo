#include "av1_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace fizmo {
namespace video {

namespace av1 {

void Plane::alloc(int w, int h) {
    width = w;
    height = h;
    stride = (w + 15) & ~15;
    const std::size_t need = static_cast<std::size_t>(stride) * static_cast<std::size_t>(h);
    if (buf.size() != need) buf.assign(need, 0);
}

void FrameBuffer::alloc(int w, int h, int np, int sx, int sy) {
    width = w;
    height = h;
    num_planes = np;
    subx = sx;
    suby = sy;
    planes[0].alloc(w, h);
    for (int p = 1; p < 3; ++p) {
        if (p < np) {
            planes[p].alloc((w + sx) >> sx, (h + sy) >> sy);
        } else {
            planes[p] = Plane();
        }
    }
}

ThreadPool::ThreadPool(int threads) {
    for (int i = 1; i < threads; ++i) m_threads.emplace_back([this] { worker(); });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    for (std::thread& t : m_threads) t.join();
}

bool ThreadPool::take(int* index) {
    if (!m_job || m_next >= m_count) return false;
    *index = m_next++;
    return true;
}

void ThreadPool::worker() {
    std::uint64_t seen = 0;
    std::unique_lock<std::mutex> lock(m_mutex);
    while (true) {
        m_wake.wait(lock, [&] { return m_stop || (m_generation != seen && m_job && m_next < m_count); });
        if (m_stop) return;
        seen = m_generation;
        int idx;
        while (take(&idx)) {
            const std::function<void(int)>* job = m_job;
            lock.unlock();
            (*job)(idx);
            lock.lock();
            ++m_finished;
            if (m_finished == m_count) m_done.notify_all();
        }
    }
}

void ThreadPool::run(int count, const std::function<void(int)>& fn) {
    if (count <= 0) return;
    if (m_threads.empty() || count == 1) {
        for (int i = 0; i < count; ++i) fn(i);
        return;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_job = &fn;
    m_next = 0;
    m_count = count;
    m_finished = 0;
    ++m_generation;
    lock.unlock();
    m_wake.notify_all();
    lock.lock();
    int idx;
    while (take(&idx)) {
        lock.unlock();
        fn(idx);
        lock.lock();
        ++m_finished;
    }
    m_done.wait(lock, [&] { return m_finished == m_count; });
    m_job = nullptr;
    m_count = 0;
}

int Decoder::thread_count() {
    int n = options.threads;
    if (n <= 0) {
        const unsigned hc = std::thread::hardware_concurrency();
        n = hc ? static_cast<int>(hc) : 1;
        if (n > 16) n = 16;
    }
    if (n > 64) n = 64;
    return n;
}

void Decoder::parallel(int count, const std::function<void(int)>& fn) {
    const int n = thread_count();
    if (n <= 1 || count <= 1) {
        for (int i = 0; i < count; ++i) fn(i);
        return;
    }
    if (!pool || pool->size() != n) pool.reset(new ThreadPool(n));
    pool->run(count, fn);
}

bool Decoder::setup_frame() {
    const int sbAlign = 128;
    const int alignedW = ((fh.MiCols * MI_SIZE + sbAlign - 1) / sbAlign) * sbAlign + 160;
    const int alignedH = ((fh.MiRows * MI_SIZE + sbAlign - 1) / sbAlign) * sbAlign + 160;
    frame.alloc(alignedW, alignedH, seq.NumPlanes, seq.subsampling_x, seq.subsampling_y);
    frame.width = fh.FrameWidth;
    frame.height = fh.FrameHeight;
    const int stride = fh.MiCols + 40;
    const int rows = fh.MiRows + 40;
    const std::size_t miCount = static_cast<std::size_t>(stride) * static_cast<std::size_t>(rows);
    if (mi_stride != stride || mi.size() != miCount) {
        mi_stride = stride;
        mi.assign(miCount, MiInfo());
        for (int p = 0; p < 3; ++p) lf_tx_size[p].assign(miCount, 0);
    }
    ++mi_stamp;
    if (mi_stamp == 0) mi_stamp = 1;
    cdef_stride = (fh.MiCols >> 4) + 4;
    cdef_idx.assign(static_cast<std::size_t>(cdef_stride) * static_cast<std::size_t>((fh.MiRows >> 4) + 4), -1);
    for (int plane = 0; plane < 3; ++plane) {
        lr_unit_rows[plane] = 0;
        lr_unit_cols[plane] = 0;
        if (plane < seq.NumPlanes && fh.FrameRestorationType[plane] != RESTORE_NONE) {
            const int subX = plane == 0 ? 0 : seq.subsampling_x;
            const int subY = plane == 0 ? 0 : seq.subsampling_y;
            const int unitSize = fh.LoopRestorationSize[plane];
            auto count_units = [](int unit, int size) { return imax((size + (unit >> 1)) / unit, 1); };
            lr_unit_rows[plane] = count_units(unitSize, round2(fh.FrameHeight, subY));
            lr_unit_cols[plane] = count_units(unitSize, round2(fh.UpscaledWidth, subX));
        }
        lr_units[plane].assign(static_cast<std::size_t>(lr_unit_rows[plane]) * static_cast<std::size_t>(imax(lr_unit_cols[plane], 0)), LrUnit());
    }
    if (fh.use_ref_frame_mvs == 1) {
        AV1_STAT(ST_TEMPORAL_MV);
        motion_field_estimation();
    }
    cur = std::make_shared<SavedFrame>();
    frame_in_progress = true;
    if (fh.frame_type == KEY_FRAME) AV1_STAT(ST_KEY_FRAME);
    if (fh.frame_type == INTRA_ONLY_FRAME) AV1_STAT(ST_INTRA_ONLY);
    if (fh.frame_type == SWITCH_FRAME) AV1_STAT(ST_SWITCH_FRAME);
    if (!fh.FrameIsIntra) AV1_STAT(ST_INTER_FRAME);
    if (fh.TileCols * fh.TileRows > 1) AV1_STAT(ST_TILES);
    if (fh.use_superres) AV1_STAT(ST_SUPERRES);
    if (fh.using_qmatrix) AV1_STAT(ST_QM);
    if (fh.delta_q_present) AV1_STAT(ST_DELTA_Q);
    if (fh.delta_lf_present) AV1_STAT(ST_DELTA_LF);
    if (fh.segmentation_enabled) AV1_STAT(ST_SEG);
    if (fh.segmentation_enabled && fh.segmentation_temporal_update) AV1_STAT(ST_SEG_PRED);
    if (fh.loop_filter_level[0] || fh.loop_filter_level[1]) AV1_STAT(ST_LF);
    if (seq.use_128x128_superblock) AV1_STAT(ST_SB128);
    if (seq.reduced_still_picture_header) AV1_STAT(ST_REDUCED_STILL);
    if (fh.error_resilient_mode) AV1_STAT(ST_ERROR_RES);
    if (fh.skip_mode_present) AV1_STAT(ST_SKIP_MODE);
    if (fh.frame_refs_short_signaling) AV1_STAT(ST_FRAME_REFS_SHORT);
    if (fh.allow_screen_content_tools) AV1_STAT(ST_SCREEN_CONTENT);
    if (fh.force_integer_mv && !fh.FrameIsIntra) AV1_STAT(ST_INTEGER_MV);
    if (seq.frame_id_numbers_present_flag) AV1_STAT(ST_FRAME_ID);
    if (!seq.enable_order_hint) AV1_STAT(ST_ORDER_HINT_OFF);
    if (seq.enable_jnt_comp && !fh.FrameIsIntra) AV1_STAT(ST_JNT_COMP_FRAME);
    if (seq.film_grain_params_present && fh.film_grain.apply_grain) AV1_STAT(ST_GRAIN);
    for (int pl = 0; pl < seq.NumPlanes; ++pl) {
        if (fh.FrameRestorationType[pl] == RESTORE_SWITCHABLE) AV1_STAT(ST_LR_SWITCHABLE);
    }
    for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) {
        if (fh.GmType[ref] != IDENTITY) AV1_STAT(ST_GM_FRAME);
    }
    for (int sg = 0; sg < MAX_SEGMENTS && fh.segmentation_enabled; ++sg) {
        if (fh.FeatureEnabled[sg][SEG_LVL_ALT_Q]) AV1_STAT(ST_SEG_FEATURE_Q);
        if (fh.FeatureEnabled[sg][SEG_LVL_ALT_LF_Y_V]) AV1_STAT(ST_SEG_FEATURE_LF);
        if (fh.FeatureEnabled[sg][SEG_LVL_REF_FRAME]) AV1_STAT(ST_SEG_FEATURE_REF);
        if (fh.FeatureEnabled[sg][SEG_LVL_SKIP]) AV1_STAT(ST_SEG_FEATURE_SKIP);
        if (fh.FeatureEnabled[sg][SEG_LVL_GLOBALMV]) AV1_STAT(ST_SEG_FEATURE_GMV);
    }
    if (refs_size_changed()) AV1_STAT(ST_SIZE_CHANGE);
    tile_errors = 0;
    saved_cdf_valid = false;
    return true;
}

namespace {

void get_mv_projection(Mv mv, int numerator, int denominator, Mv* out) {
    const int clippedDenominator = imin(MAX_FRAME_DISTANCE, denominator);
    const int clippedNumerator = clip3(-MAX_FRAME_DISTANCE, MAX_FRAME_DISTANCE, numerator);
    int v[2] = {mv.row, mv.col};
    int r[2];
    for (int i = 0; i < 2; ++i) {
        const int scaled = round2signed(v[i] * clippedNumerator * Div_Mult[clippedDenominator], 14);
        r[i] = clip3(-(1 << 14) + 1, (1 << 14) - 1, scaled);
    }
    *out = make_mv(r[0], r[1]);
}

int project(int v8, int delta, int dstSign, int max8, int maxOff8, int* posValid) {
    const int base8 = (v8 >> 3) << 3;
    int offset8;
    if (delta >= 0) {
        offset8 = delta >> (3 + 1 + MI_SIZE_LOG2);
    } else {
        offset8 = -((-delta) >> (3 + 1 + MI_SIZE_LOG2));
    }
    v8 += dstSign * offset8;
    if (v8 < 0 || v8 >= max8 || v8 < base8 - maxOff8 || v8 >= base8 + 8 + maxOff8) *posValid = 0;
    return v8;
}

} // namespace

bool Decoder::projection(int src, int dstSign) {
    const int srcIdx = fh.ref_frame_idx[src - LAST_FRAME];
    const int w8 = fh.MiCols >> 1;
    const int h8 = fh.MiRows >> 1;
    const SavedFrame* s = refs[srcIdx].get();
    if (!s) return false;
    if (s->mi_rows != fh.MiRows || s->mi_cols != fh.MiCols || s->frame_type == INTRA_ONLY_FRAME || s->frame_type == KEY_FRAME) return false;
    if (s->mf_ref_frames.size() < static_cast<std::size_t>(w8) * h8) return false;
    for (int y8 = 0; y8 < h8; ++y8) {
        for (int x8 = 0; x8 < w8; ++x8) {
            const std::size_t at = static_cast<std::size_t>(y8) * w8 + x8;
            const int srcRef = s->mf_ref_frames[at];
            if (srcRef > INTRA_FRAME) {
                const int refToCur = get_relative_dist(fh.OrderHints[src], fh.OrderHint);
                const int refOffset = get_relative_dist(fh.OrderHints[src], s->saved_order_hints[srcRef]);
                int posValid = iabs(refToCur) <= MAX_FRAME_DISTANCE && iabs(refOffset) <= MAX_FRAME_DISTANCE && refOffset > 0;
                if (posValid) {
                    const Mv mv = s->mf_mvs[at];
                    Mv projMv;
                    get_mv_projection(mv, refToCur * dstSign, refOffset, &projMv);
                    const int PosY8 = project(y8, projMv.row, dstSign, fh.MiRows >> 1, MAX_OFFSET_HEIGHT, &posValid);
                    const int PosX8 = project(x8, projMv.col, dstSign, fh.MiCols >> 1, MAX_OFFSET_WIDTH, &posValid);
                    if (posValid) {
                        for (int dst = LAST_FRAME; dst <= ALTREF_FRAME; ++dst) {
                            const int refToDst = get_relative_dist(fh.OrderHint, fh.OrderHints[dst]);
                            get_mv_projection(mv, refToDst, refOffset, &projMv);
                            motion_field_mvs[dst][static_cast<std::size_t>(PosY8) * w8 + PosX8] = projMv;
                        }
                    }
                }
            }
        }
    }
    return true;
}

void Decoder::motion_field_estimation() {
    const int w8 = fh.MiCols >> 1;
    const int h8 = fh.MiRows >> 1;
    for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) motion_field_mvs[ref].assign(static_cast<std::size_t>(w8) * h8, make_mv(-32768, -32768));
    const int lastIdx = fh.ref_frame_idx[0];
    const int curGoldOrderHint = fh.OrderHints[GOLDEN_FRAME];
    const int lastAltOrderHint = refs[lastIdx] ? refs[lastIdx]->saved_order_hints[ALTREF_FRAME] : 0;
    const int useLast = lastAltOrderHint != curGoldOrderHint;
    if (useLast) projection(LAST_FRAME, -1);
    int refStamp = MFMV_STACK_SIZE - 2;
    const int useBwd = get_relative_dist(fh.OrderHints[BWDREF_FRAME], fh.OrderHint) > 0;
    if (useBwd) {
        if (projection(BWDREF_FRAME, 1)) refStamp -= 1;
    }
    const int useAlt2 = get_relative_dist(fh.OrderHints[ALTREF2_FRAME], fh.OrderHint) > 0;
    if (useAlt2) {
        if (projection(ALTREF2_FRAME, 1)) refStamp -= 1;
    }
    const int useAlt = get_relative_dist(fh.OrderHints[ALTREF_FRAME], fh.OrderHint) > 0;
    if (useAlt && refStamp >= 0) {
        if (projection(ALTREF_FRAME, 1)) refStamp -= 1;
    }
    if (refStamp >= 0) projection(LAST2_FRAME, -1);
}

bool Decoder::tile_group_obu(BitReader& br, const std::uint8_t* data, std::size_t size, std::string* error) {
    if (!frame_in_progress) return false;
    const int NumTiles = fh.TileCols * fh.TileRows;
    const std::size_t startBitPos = br.position();
    int tile_start_and_end_present_flag = 0;
    if (NumTiles > 1) tile_start_and_end_present_flag = static_cast<int>(br.f(1));
    int tg_start;
    int tg_end;
    if (NumTiles == 1 || !tile_start_and_end_present_flag) {
        tg_start = 0;
        tg_end = NumTiles - 1;
    } else {
        const int tileBits = fh.TileColsLog2 + fh.TileRowsLog2;
        tg_start = static_cast<int>(br.f(tileBits));
        tg_end = static_cast<int>(br.f(tileBits));
    }
    br.byte_alignment();
    const std::size_t endBitPos = br.position();
    const std::size_t headerBytes = (endBitPos - startBitPos) / 8;
    if (headerBytes > size || tg_end >= NumTiles || tg_start > tg_end || tg_start != TileNum) {
        if (error) *error = "invalid AV1 tile group";
        frame_in_progress = false;
        return false;
    }
    std::size_t pos = headerBytes;
    std::size_t sz = size - headerBytes;
    bool ok = true;
    struct TileJob {
        int num;
        std::size_t offset;
        std::size_t size;
    };
    std::vector<TileJob> jobs;
    for (int num = tg_start; num <= tg_end; ++num) {
        const int lastTile = num == tg_end;
        std::size_t tileSize;
        if (lastTile) {
            tileSize = sz;
        } else {
            if (sz < static_cast<std::size_t>(fh.TileSizeBytes)) {
                ok = false;
                break;
            }
            std::size_t v = 0;
            for (int i = 0; i < fh.TileSizeBytes; ++i) v |= static_cast<std::size_t>(data[pos + i]) << (8 * i);
            tileSize = v + 1;
            pos += static_cast<std::size_t>(fh.TileSizeBytes);
            sz -= static_cast<std::size_t>(fh.TileSizeBytes);
            if (tileSize > sz) {
                ok = false;
                break;
            }
        }
        if (tileSize == 0) {
            ok = false;
            break;
        }
        TileJob j;
        j.num = num;
        j.offset = pos;
        j.size = tileSize;
        jobs.push_back(j);
        pos += tileSize;
        sz -= tileSize;
    }
    const int njobs = static_cast<int>(jobs.size());
    while (static_cast<int>(tile_pool.size()) < njobs) tile_pool.emplace_back(new Tile());
    std::vector<char> tileOk(static_cast<std::size_t>(njobs), 1);
    parallel(njobs, [&](int t) {
        const TileJob& j = jobs[static_cast<std::size_t>(t)];
        Tile& tile = *tile_pool[static_cast<std::size_t>(t)];
        tile.error = false;
        tile.init(this, j.num / fh.TileCols, j.num % fh.TileCols);
        if (!tile.decode(data + j.offset, j.size)) tileOk[static_cast<std::size_t>(t)] = 0;
    });
    for (int t = 0; t < njobs; ++t) {
        if (!tileOk[static_cast<std::size_t>(t)]) {
            ++tile_errors;
            ok = false;
        }
        if (fh.disable_frame_end_update_cdf == 0 && jobs[static_cast<std::size_t>(t)].num == fh.context_update_tile_id) {
            saved_cdf = tile_pool[static_cast<std::size_t>(t)]->cdf;
            saved_cdf_valid = true;
        }
    }
    TileNum = njobs > 0 ? jobs.back().num + 1 : tg_start;
    if (!ok && njobs > 0) TileNum = tg_end + 1;
    if (!ok) {
        if (error && error->empty()) *error = "corrupt AV1 tile data";
    }
    if (tg_end == NumTiles - 1 || !ok) {
        if (!fh.disable_frame_end_update_cdf && saved_cdf_valid) cdf = saved_cdf;
        if (ok || tile_errors < NumTiles) {
            decode_frame_wrapup();
        } else {
            frame_in_progress = false;
        }
        SeenFrameHeader = 0;
        frame_header_valid = false;
    }
    return ok;
}

void Decoder::decode_frame_wrapup() {
    frame_in_progress = false;
    if (fh.loop_filter_level[0] != 0 || fh.loop_filter_level[1] != 0) loop_filter_frame();
    const FrameBuffer* cdefOut = &frame;
    const bool cdefActive = seq.enable_cdef && !fh.CodedLossless && !fh.allow_intrabc;
    if (cdefActive) {
        cdef_frame.alloc(frame.planes[0].width, frame.planes[0].height, seq.NumPlanes, seq.subsampling_x, seq.subsampling_y);
        cdef_frame_process();
        cdefOut = &cdef_frame;
    }
    const FrameBuffer* upCdef = cdefOut;
    const FrameBuffer* upCur = &frame;
    if (fh.use_superres) {
        const int uw = fh.UpscaledWidth + 32;
        const int uh = fh.FrameHeight + 32;
        upscaled_cdef.alloc(uw, uh, seq.NumPlanes, seq.subsampling_x, seq.subsampling_y);
        upscale(*cdefOut, upscaled_cdef);
        upCdef = &upscaled_cdef;
        if (fh.UsesLr) {
            upscaled_cur.alloc(uw, uh, seq.NumPlanes, seq.subsampling_x, seq.subsampling_y);
            upscale(frame, upscaled_cur);
            upCur = &upscaled_cur;
        }
    }
    FrameBuffer* finalBuf;
    if (fh.UsesLr) {
        lr_frame.alloc(upCdef->planes[0].width, upCdef->planes[0].height, seq.NumPlanes, seq.subsampling_x, seq.subsampling_y);
        for (int p = 0; p < seq.NumPlanes; ++p) lr_frame.planes[p].buf = upCdef->planes[p].buf;
        lr_cdef_src = upCdef;
        lr_cur_src = upCur;
        loop_restoration();
        finalBuf = &lr_frame;
    } else {
        finalBuf = const_cast<FrameBuffer*>(upCdef);
    }
    SavedFrame& s = *cur;
    s.frame = std::move(*finalBuf);
    s.frame.width = fh.UpscaledWidth;
    s.frame.height = fh.FrameHeight;
    s.frame.num_planes = seq.NumPlanes;
    s.frame.subx = seq.subsampling_x;
    s.frame.suby = seq.subsampling_y;
    *finalBuf = FrameBuffer();
    s.frame_id = fh.current_frame_id;
    s.upscaled_width = fh.UpscaledWidth;
    s.frame_width = fh.FrameWidth;
    s.frame_height = fh.FrameHeight;
    s.render_width = fh.RenderWidth;
    s.render_height = fh.RenderHeight;
    s.mi_cols = fh.MiCols;
    s.mi_rows = fh.MiRows;
    s.frame_type = fh.frame_type;
    s.subsampling_x = seq.subsampling_x;
    s.subsampling_y = seq.subsampling_y;
    s.bit_depth = seq.BitDepth;
    s.order_hint = fh.OrderHint;
    s.showable_frame = fh.showable_frame;
    s.show_frame = fh.show_frame;
    s.temporal_id = fh.temporal_id;
    s.spatial_id = fh.spatial_id;
    for (int j = 0; j < REFS_PER_FRAME; ++j) s.saved_order_hints[j + LAST_FRAME] = fh.OrderHints[j + LAST_FRAME];
    const int w8 = fh.MiCols >> 1;
    const int h8 = fh.MiRows >> 1;
    s.mf_ref_frames.assign(static_cast<std::size_t>(w8) * h8, NONE);
    s.mf_mvs.assign(static_cast<std::size_t>(w8) * h8, Mv());
    for (int y8 = 0; y8 < h8; ++y8) {
        for (int x8 = 0; x8 < w8; ++x8) {
            const int row = 2 * y8 + 1;
            const int col = 2 * x8 + 1;
            const MiInfo& m = mi[static_cast<std::size_t>(row) * mi_stride + col];
            const std::size_t at = static_cast<std::size_t>(y8) * w8 + x8;
            for (int list = 0; list < 2; ++list) {
                const int r = m.ref_frame[list];
                if (r > INTRA_FRAME) {
                    const int refIdx = fh.ref_frame_idx[r - LAST_FRAME];
                    const int dist = get_relative_dist(RefOrderHint[refIdx], fh.OrderHint);
                    if (dist < 0) {
                        const int mvRow = m.mv[list].row;
                        const int mvCol = m.mv[list].col;
                        if (iabs(mvRow) <= REFMVS_LIMIT && iabs(mvCol) <= REFMVS_LIMIT) {
                            s.mf_ref_frames[at] = static_cast<std::int8_t>(r);
                            s.mf_mvs[at] = make_mv(mvRow, mvCol);
                        }
                    }
                }
            }
        }
    }
    std::memcpy(s.gm_params, fh.gm_params, sizeof(s.gm_params));
    s.segment_ids.resize(static_cast<std::size_t>(fh.MiRows) * fh.MiCols);
    if (fh.segmentation_enabled && !fh.segmentation_update_map) {
        s.segment_ids = prev_segment_ids;
        s.segment_ids.resize(static_cast<std::size_t>(fh.MiRows) * fh.MiCols, 0);
    } else {
        for (int r = 0; r < fh.MiRows; ++r) {
            const MiInfo* m = &mi[static_cast<std::size_t>(r) * mi_stride];
            std::uint8_t* o = &s.segment_ids[static_cast<std::size_t>(r) * fh.MiCols];
            for (int c = 0; c < fh.MiCols; ++c) o[c] = m[c].segment_id;
        }
    }
    s.cdfs = cdf;
    s.grain = fh.film_grain;
    std::memcpy(s.loop_filter_ref_deltas, fh.loop_filter_ref_deltas, sizeof(s.loop_filter_ref_deltas));
    std::memcpy(s.loop_filter_mode_deltas, fh.loop_filter_mode_deltas, sizeof(s.loop_filter_mode_deltas));
    std::memcpy(s.feature_enabled, fh.FeatureEnabled, sizeof(s.feature_enabled));
    std::memcpy(s.feature_data, fh.FeatureData, sizeof(s.feature_data));
    reference_frame_update();
    ++frames_decoded;
    if (fh.show_frame) output_frame(s, fh.film_grain);
    cur.reset();
}

bool Decoder::refs_size_changed() const noexcept {
    if (fh.FrameIsIntra) return false;
    for (int i = 0; i < REFS_PER_FRAME; ++i) {
        const SavedFrame* r = refs[fh.ref_frame_idx[i]].get();
        if (r && (r->upscaled_width != fh.UpscaledWidth || r->frame_height != fh.FrameHeight)) return true;
    }
    return false;
}

void Decoder::reference_frame_update() {
    for (int i = 0; i < NUM_REF_FRAMES; ++i) {
        if ((fh.refresh_frame_flags >> i) & 1) {
            refs[i] = cur;
            RefValid[i] = 1;
            RefFrameId[i] = fh.current_frame_id;
            RefOrderHint[i] = fh.OrderHint;
            RefFrameType[i] = fh.frame_type;
        }
    }
}

void Decoder::show_existing_frame() {
    std::shared_ptr<SavedFrame> f = refs[fh.frame_to_show_map_idx];
    if (!f) return;
    AV1_STAT(ST_SHOW_EXISTING);
    output_frame(*f, fh.film_grain);
    if (fh.frame_type == KEY_FRAME) {
        for (int i = 0; i < NUM_REF_FRAMES; ++i) {
            refs[i] = f;
            RefValid[i] = 1;
            RefFrameId[i] = f->frame_id;
            RefOrderHint[i] = f->order_hint;
            RefFrameType[i] = KEY_FRAME;
        }
    }
}

namespace {

std::uint32_t gcd_u32(std::uint32_t a, std::uint32_t b) {
    while (b) {
        const std::uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

} // namespace

void Decoder::output_frame(const SavedFrame& f, const FilmGrainParams& grain) {
    const FrameBuffer* src = &f.frame;
    FrameBuffer grained;
    if (seq.film_grain_params_present && grain.apply_grain && options.apply_film_grain) {
        FrameBuffer view;
        add_film_grain(grain, f.bit_depth, f.frame.num_planes == 1, f.subsampling_x, f.subsampling_y, seq.matrix_coefficients, f.frame, grained);
        src = &grained;
    }
    VideoFrame out;
    const PixelFormat fmt = av1_pixel_format(f.bit_depth, f.frame.num_planes == 1, f.subsampling_x, f.subsampling_y);
    const std::uint32_t w = static_cast<std::uint32_t>(f.upscaled_width);
    const std::uint32_t h = static_cast<std::uint32_t>(f.frame_height);
    if (!out.allocate(w, h, fmt)) return;
    out.color = info.color;
    out.pts = current_pts;
    out.keyframe = f.frame_type == KEY_FRAME;
    {
        std::uint64_t num = static_cast<std::uint64_t>(f.render_width) * f.frame_height;
        std::uint64_t den = static_cast<std::uint64_t>(f.render_height) * f.upscaled_width;
        if (num && den && num != den) {
            while (num > 0xFFFFFFFFull || den > 0xFFFFFFFFull) {
                num >>= 1;
                den >>= 1;
            }
            const std::uint32_t g = gcd_u32(static_cast<std::uint32_t>(num), static_cast<std::uint32_t>(den));
            out.sar_num = static_cast<std::uint32_t>(num) / (g ? g : 1);
            out.sar_den = static_cast<std::uint32_t>(den) / (g ? g : 1);
        }
    }
    const int planes = plane_count(fmt);
    const bool high = bytes_per_sample(fmt) == 2;
    for (int p = 0; p < planes; ++p) {
        const Plane& sp = src->planes[p];
        const std::uint32_t pw = out.plane_width(p);
        const std::uint32_t ph = out.plane_height(p);
        for (std::uint32_t y = 0; y < ph; ++y) {
            const std::uint16_t* s = sp.row(static_cast<int>(y));
            std::uint8_t* o = out.plane(p) + out.stride[p] * y;
            if (high) {
                for (std::uint32_t x = 0; x < pw; ++x) {
                    o[2 * x] = static_cast<std::uint8_t>(s[x] & 0xFF);
                    o[2 * x + 1] = static_cast<std::uint8_t>(s[x] >> 8);
                }
            } else {
                for (std::uint32_t x = 0; x < pw; ++x) o[x] = static_cast<std::uint8_t>(s[x]);
            }
        }
    }
    info.width = w;
    info.height = h;
    info.render_width = static_cast<std::uint32_t>(f.render_width);
    info.render_height = static_cast<std::uint32_t>(f.render_height);
    tu_frames.push_back(std::move(out));
}

void Decoder::reset() {
    for (int i = 0; i < NUM_REF_FRAMES; ++i) {
        refs[i].reset();
        RefValid[i] = 0;
        RefFrameId[i] = 0;
        RefOrderHint[i] = 0;
        RefFrameType[i] = 0;
    }
    cur.reset();
    SeenFrameHeader = 0;
    TileNum = 0;
    frame_in_progress = false;
    frame_header_valid = false;
    output.clear();
    tu_frames.clear();
}

} // namespace av1

PixelFormat av1_pixel_format(int bit_depth, bool monochrome, int subsampling_x, int subsampling_y) noexcept {
    if (monochrome) {
        if (bit_depth == 12) return PixelFormat::gray12;
        if (bit_depth == 10) return PixelFormat::gray10;
        return PixelFormat::gray;
    }
    if (subsampling_x && subsampling_y) {
        if (bit_depth == 12) return PixelFormat::yuv420p12;
        if (bit_depth == 10) return PixelFormat::yuv420p10;
        return PixelFormat::yuv420p;
    }
    if (subsampling_x) {
        if (bit_depth == 12) return PixelFormat::yuv422p12;
        if (bit_depth == 10) return PixelFormat::yuv422p10;
        return PixelFormat::yuv422p;
    }
    if (bit_depth == 12) return PixelFormat::yuv444p12;
    if (bit_depth == 10) return PixelFormat::yuv444p10;
    return PixelFormat::yuv444p;
}

#ifdef FIZMO_AV1_STATS
namespace av1 {
std::uint64_t g_av1_stats[ST_COUNT];
const char* const g_av1_stat_names[ST_COUNT] = {"palette_y", "palette_uv", "intrabc", "filter_intra", "cfl", "angle_delta", "edge_upsample", "edge_filter", "smooth", "paeth", "directional", "ii_smooth", "ii_wedge", "comp_wedge", "comp_diffwtd", "comp_dist", "comp_avg", "obmc", "localwarp", "globalwarp", "scaled_ref", "dual_filter", "wht", "qm", "delta_q", "delta_lf", "seg", "lf", "cdef", "lr_wiener", "lr_sgr", "superres", "grain", "tiles", "show_existing", "size_change", "intra_only", "switch_frame", "skip_mode", "uni_comp", "sb128", "reduced_still", "error_res", "tx64", "idtx", "flipadst", "adst", "tx_1d", "temporal_mv", "gm_frame", "interp_smooth", "interp_sharp", "bilinear", "seg_pred", "rect_tx", "vartx", "layer_drop", "frame_refs_short", "compound", "key_frame", "inter_frame", "chroma_subpred", "ii_dc", "ii_v", "ii_h", "dc_pred_st", "v_pred_st", "h_pred_st", "smooth_v", "smooth_h", "adst4", "adst8", "adst16", "dct64", "ident32", "palette_cache", "seg_feature_q", "seg_feature_lf", "seg_feature_ref", "seg_feature_skip", "seg_feature_gmv", "lr_switchable", "screen_content", "integer_mv", "frame_id", "order_hint_off", "jnt_comp_frame", "mv_joint_hp"};
} // namespace av1
#endif // FIZMO_AV1_STATS

struct Av1Decoder::State : public av1::Decoder {};

Av1Decoder::Av1Decoder() : m_state(new State()) {}
Av1Decoder::~Av1Decoder() = default;
Av1Decoder::Av1Decoder(Av1Decoder&&) noexcept = default;
Av1Decoder& Av1Decoder::operator=(Av1Decoder&&) noexcept = default;

bool Av1Decoder::open(const std::uint8_t* av1c, std::size_t size, std::string* error) {
    if (!m_state) m_state.reset(new State());
    m_state->reset();
    m_state->have_seq = false;
    m_state->opened = true;
    if (!av1c || size == 0) return true;
    if (size >= 4 && (av1c[0] & 0x80)) {
        if (size > 4) {
            std::string err;
            if (!m_state->decode_temporal_unit(av1c + 4, size - 4, &err)) {
                if (error) *error = err;
                return false;
            }
        }
        return true;
    }
    std::string err;
    if (!m_state->decode_temporal_unit(av1c, size, &err)) {
        if (error) *error = err;
        return false;
    }
    return true;
}

bool Av1Decoder::open() {
    if (!m_state) m_state.reset(new State());
    m_state->reset();
    m_state->have_seq = false;
    m_state->opened = true;
    return true;
}

void Av1Decoder::set_options(const Av1Options& options) {
    if (!m_state) m_state.reset(new State());
    m_state->options = options;
}

const Av1Options& Av1Decoder::options() const noexcept {
    static const Av1Options defaults;
    return m_state ? m_state->options : defaults;
}

bool Av1Decoder::decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) {
    if (!m_state || !m_state->opened) {
        if (error) *error = "decoder is not open";
        return false;
    }
    if (!data || size == 0) return true;
    m_state->current_pts = pts;
    return m_state->decode_temporal_unit(data, size, error);
}

bool Av1Decoder::receive(VideoFrame& out) {
    if (!m_state || m_state->output.empty()) return false;
    out = std::move(m_state->output.front());
    m_state->output.pop_front();
    return true;
}

void Av1Decoder::flush() {}

void Av1Decoder::reset() {
    if (!m_state) return;
    m_state->reset();
}

void Av1Decoder::close() {
    if (!m_state) return;
    m_state->reset();
    m_state->opened = false;
}

bool Av1Decoder::is_disposable(const std::uint8_t* data, std::size_t size) const noexcept {
    return m_state && m_state->disposable(data, size);
}

bool Av1Decoder::is_open() const noexcept {
    return m_state && m_state->opened;
}

const Av1Info& Av1Decoder::info() const noexcept {
    static const Av1Info empty;
    return m_state ? m_state->info : empty;
}

std::uint64_t Av1Decoder::frames_decoded() const noexcept {
    return m_state ? m_state->frames_decoded : 0;
}

std::uint64_t Av1Decoder::error_count() const noexcept {
    return m_state ? m_state->errors : 0;
}

namespace {

bool read_leb128(const std::uint8_t* p, std::size_t size, std::uint64_t* value, std::size_t* len) {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        if (i >= size) return false;
        v |= static_cast<std::uint64_t>(p[i] & 0x7f) << (i * 7);
        if (!(p[i] & 0x80)) {
            *value = v;
            *len = i + 1;
            return true;
        }
    }
    return false;
}

void write_leb128(std::vector<std::uint8_t>& out, std::uint64_t v) {
    do {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0x7f);
        v >>= 7;
        if (v) b |= 0x80;
        out.push_back(b);
    } while (v);
}

bool obu_info(const std::uint8_t* p, std::size_t size, int* type, std::size_t* total) {
    if (size < 1) return false;
    if (p[0] & 0x80) return false;
    *type = (p[0] >> 3) & 15;
    const bool ext = (p[0] >> 2) & 1;
    const bool hasSize = (p[0] >> 1) & 1;
    std::size_t hdr = ext ? 2 : 1;
    if (hdr > size) return false;
    if (!hasSize) {
        *total = size;
        return true;
    }
    std::uint64_t v;
    std::size_t len;
    if (!read_leb128(p + hdr, size - hdr, &v, &len)) return false;
    if (v > size - hdr - len) return false;
    *total = hdr + len + static_cast<std::size_t>(v);
    return true;
}

bool valid_annexb_tu(const std::uint8_t* p, std::size_t size) {
    std::uint64_t tuSize;
    std::size_t len;
    if (!read_leb128(p, size, &tuSize, &len)) return false;
    if (tuSize == 0 || tuSize > size - len) return false;
    const std::uint8_t* q = p + len;
    std::size_t left = static_cast<std::size_t>(tuSize);
    std::uint64_t fuSize;
    if (!read_leb128(q, left, &fuSize, &len)) return false;
    if (fuSize == 0 || fuSize > left - len) return false;
    q += len;
    left = static_cast<std::size_t>(fuSize);
    std::uint64_t obuLen;
    if (!read_leb128(q, left, &obuLen, &len)) return false;
    if (obuLen == 0 || obuLen > left - len) return false;
    q += len;
    if (q[0] & 0x80) return false;
    const int type = (q[0] >> 3) & 15;
    return type == av1::OBU_TEMPORAL_DELIMITER || type == av1::OBU_SEQUENCE_HEADER;
}

} // namespace

bool is_av1_obu_stream(const void* data, std::size_t size) noexcept {
    return detect_av1_stream(data, size) != Av1StreamFormat::unknown;
}

Av1StreamFormat detect_av1_stream(const void* data, std::size_t size) noexcept {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    if (!p || size < 2) return Av1StreamFormat::unknown;
    if (size >= 32 && std::memcmp(p, "DKIF", 4) == 0) {
        if (std::memcmp(p + 8, "AV01", 4) == 0 || std::memcmp(p + 8, "av01", 4) == 0) return Av1StreamFormat::ivf;
        return Av1StreamFormat::unknown;
    }
    int type;
    std::size_t total;
    if ((p[0] & 0x80) == 0 && ((p[0] >> 1) & 1) && obu_info(p, size, &type, &total)) {
        if (type == av1::OBU_TEMPORAL_DELIMITER && total == 2) return Av1StreamFormat::obu;
        if (type == av1::OBU_SEQUENCE_HEADER && total > 2 && total <= size) return Av1StreamFormat::obu;
    }
    if (valid_annexb_tu(p, size)) return Av1StreamFormat::annexb;
    return Av1StreamFormat::unknown;
}

bool Av1FileReader::open(const std::string& path, std::string* error) {
    close();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (error) *error = "can't open " + path;
        return false;
    }
    std::vector<std::uint8_t> data;
    std::uint8_t buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    m_data = std::move(data);
    if (!start()) {
        if (error) *error = "not an AV1 stream";
        close();
        return false;
    }
    return true;
}

bool Av1FileReader::open(const void* data, std::size_t size) {
    close();
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    if (!p) return false;
    m_data.assign(p, p + size);
    if (!start()) {
        close();
        return false;
    }
    return true;
}

bool Av1FileReader::start() {
    m_format = detect_av1_stream(m_data.data(), m_data.size());
    if (m_format == Av1StreamFormat::unknown) return false;
    m_pos = 0;
    if (m_format == Av1StreamFormat::ivf) {
        const std::uint8_t* p = m_data.data();
        const std::size_t hdr = static_cast<std::size_t>(p[6] | (p[7] << 8));
        m_ivf_width = static_cast<std::uint32_t>(p[12] | (p[13] << 8));
        m_ivf_height = static_cast<std::uint32_t>(p[14] | (p[15] << 8));
        m_ivf_rate = static_cast<std::uint32_t>(p[16] | (p[17] << 8) | (p[18] << 16) | (static_cast<std::uint32_t>(p[19]) << 24));
        m_ivf_scale = static_cast<std::uint32_t>(p[20] | (p[21] << 8) | (p[22] << 16) | (static_cast<std::uint32_t>(p[23]) << 24));
        if (m_ivf_rate && m_ivf_scale) m_rate = static_cast<double>(m_ivf_rate) / m_ivf_scale;
        m_pos = hdr < 32 ? 32 : hdr;
    }
    m_first = m_pos;
    m_count = 0;
    m_open = true;
    return true;
}

void Av1FileReader::close() {
    m_data.clear();
    m_pos = m_first = 0;
    m_format = Av1StreamFormat::unknown;
    m_open = false;
    m_ivf_width = m_ivf_height = 0;
    m_ivf_rate = m_ivf_scale = 0;
    m_rate = 0.0;
    m_count = 0;
}

void Av1FileReader::rewind() noexcept {
    m_pos = m_first;
    m_count = 0;
}

bool Av1FileReader::read_obu_tu(std::vector<std::uint8_t>& out) {
    const std::size_t size = m_data.size();
    std::size_t pos = m_pos;
    bool started = false;
    while (pos < size) {
        int type;
        std::size_t total;
        if (!obu_info(m_data.data() + pos, size - pos, &type, &total) || total == 0) {
            pos = size;
            break;
        }
        if (type == av1::OBU_TEMPORAL_DELIMITER && started) break;
        started = true;
        out.insert(out.end(), m_data.begin() + static_cast<std::ptrdiff_t>(pos), m_data.begin() + static_cast<std::ptrdiff_t>(pos + total));
        pos += total;
    }
    m_pos = pos;
    return !out.empty();
}

bool Av1FileReader::read_annexb_tu(std::vector<std::uint8_t>& out) {
    const std::size_t size = m_data.size();
    std::uint64_t tuSize;
    std::size_t len;
    if (m_pos >= size || !read_leb128(m_data.data() + m_pos, size - m_pos, &tuSize, &len)) {
        m_pos = size;
        return false;
    }
    std::size_t pos = m_pos + len;
    if (tuSize > size - pos) tuSize = size - pos;
    const std::size_t tuEnd = pos + static_cast<std::size_t>(tuSize);
    while (pos < tuEnd) {
        std::uint64_t fuSize;
        if (!read_leb128(m_data.data() + pos, tuEnd - pos, &fuSize, &len)) break;
        pos += len;
        if (fuSize > tuEnd - pos) fuSize = tuEnd - pos;
        const std::size_t fuEnd = pos + static_cast<std::size_t>(fuSize);
        while (pos < fuEnd) {
            std::uint64_t obuLen;
            if (!read_leb128(m_data.data() + pos, fuEnd - pos, &obuLen, &len)) {
                pos = fuEnd;
                break;
            }
            pos += len;
            if (obuLen > fuEnd - pos) obuLen = fuEnd - pos;
            const std::uint8_t* o = m_data.data() + pos;
            const std::size_t olen = static_cast<std::size_t>(obuLen);
            if (olen >= 1) {
                const bool ext = (o[0] >> 2) & 1;
                const bool hasSize = (o[0] >> 1) & 1;
                const std::size_t hdr = ext ? 2 : 1;
                if (hasSize || olen < hdr) {
                    out.insert(out.end(), o, o + olen);
                } else {
                    out.push_back(static_cast<std::uint8_t>(o[0] | 2));
                    if (ext) out.push_back(o[1]);
                    write_leb128(out, olen - hdr);
                    out.insert(out.end(), o + hdr, o + olen);
                }
            }
            pos += olen;
        }
        pos = fuEnd;
    }
    m_pos = tuEnd;
    return true;
}

bool Av1FileReader::read(std::vector<std::uint8_t>& temporal_unit, double* pts) {
    temporal_unit.clear();
    if (!m_open || m_pos >= m_data.size()) return false;
    bool ok = false;
    double t = 0.0;
    if (m_format == Av1StreamFormat::ivf) {
        if (m_pos + 12 > m_data.size()) {
            m_pos = m_data.size();
            return false;
        }
        const std::uint8_t* p = m_data.data() + m_pos;
        std::size_t len = static_cast<std::size_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24));
        std::uint64_t ts = 0;
        for (int i = 0; i < 8; ++i) ts |= static_cast<std::uint64_t>(p[4 + i]) << (8 * i);
        m_pos += 12;
        if (len > m_data.size() - m_pos) len = m_data.size() - m_pos;
        temporal_unit.assign(m_data.begin() + static_cast<std::ptrdiff_t>(m_pos), m_data.begin() + static_cast<std::ptrdiff_t>(m_pos + len));
        m_pos += len;
        t = m_ivf_rate ? static_cast<double>(ts) * m_ivf_scale / m_ivf_rate : static_cast<double>(m_count);
        ok = true;
    } else if (m_format == Av1StreamFormat::annexb) {
        ok = read_annexb_tu(temporal_unit);
        t = static_cast<double>(m_count);
    } else {
        ok = read_obu_tu(temporal_unit);
        t = static_cast<double>(m_count);
    }
    if (ok) {
        ++m_count;
        if (pts) *pts = t;
    }
    return ok;
}

} // namespace video
} // namespace fizmo
