#include "../Source/Video/decoder.hpp"
#include "../Source/Video/demux.hpp"
#include "../Source/Video/hevc.hpp"
#include "../Source/Video/hevc_internal.hpp"
#include "../Source/Video/player.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace fizmo {
namespace video {
namespace h264_internal {
extern const std::uint8_t k_cabac_range_lps[64][4];
extern const std::uint8_t k_cabac_trans_lps[64];
} // namespace h264_internal
} // namespace video
} // namespace fizmo

namespace hevcgen {

using namespace fizmo::video;
using namespace fizmo::video::hevc_internal;

class BitWriter {
public:
    void u(int n, std::uint32_t v) {
        for (int i = n - 1; i >= 0; --i) bit((v >> i) & 1u);
    }
    void bit(std::uint32_t b) {
        m_cur = static_cast<std::uint8_t>((m_cur << 1) | (b & 1u));
        if (++m_n == 8) {
            m_out.push_back(m_cur);
            m_cur = 0;
            m_n = 0;
        }
    }
    void flag(bool b) { bit(b ? 1u : 0u); }
    void ue(std::uint32_t v) {
        const std::uint64_t x = static_cast<std::uint64_t>(v) + 1;
        int len = 0;
        while ((x >> len) > 1) ++len;
        for (int i = 0; i < len; ++i) bit(0);
        for (int i = len; i >= 0; --i) bit(static_cast<std::uint32_t>((x >> i) & 1u));
    }
    void se(int v) { ue(v > 0 ? static_cast<std::uint32_t>(2 * v - 1) : static_cast<std::uint32_t>(-2 * v)); }
    void trailing() {
        bit(1);
        while (m_n) bit(0);
    }
    void align_zero() {
        while (m_n) bit(0);
    }
    bool aligned() const { return m_n == 0; }
    std::vector<std::uint8_t>& bytes() { return m_out; }

private:
    std::vector<std::uint8_t> m_out;
    std::uint8_t m_cur = 0;
    int m_n = 0;
};

inline std::vector<std::uint8_t> make_nal(int type, const std::vector<std::uint8_t>& rbsp) {
    std::vector<std::uint8_t> out = {0, 0, 0, 1, static_cast<std::uint8_t>(type << 1), 1};
    int zeros = 0;
    for (std::uint8_t c : rbsp) {
        if (zeros >= 2 && c <= 3) {
            out.push_back(3);
            zeros = 0;
        }
        out.push_back(c);
        zeros = c == 0 ? zeros + 1 : 0;
    }
    if (!rbsp.empty() && rbsp.back() == 0) out.push_back(3);
    return out;
}

class CabacWriter {
public:
    void restart() {
        m_low = 0;
        m_range = 510;
        m_outstanding = 0;
        m_first = true;
    }
    void decision(std::uint8_t& ctx, int bin) {
        int state = ctx >> 1;
        int mps = ctx & 1;
        const std::uint32_t lps = h264_internal::k_cabac_range_lps[state][(m_range >> 6) & 3];
        m_range -= lps;
        if (bin != mps) {
            m_low += m_range;
            m_range = lps;
            if (state == 0) mps = 1 - mps;
            state = h264_internal::k_cabac_trans_lps[state];
        } else {
            state = state < 62 ? state + 1 : 62;
        }
        ctx = static_cast<std::uint8_t>((state << 1) | mps);
        renorm();
    }
    void bypass(int bin) {
        m_low <<= 1;
        if (bin) m_low += m_range;
        if (m_low >= 1024) {
            put(1);
            m_low -= 1024;
        } else if (m_low < 512) {
            put(0);
        } else {
            m_low -= 512;
            ++m_outstanding;
        }
    }
    void terminate(int bin) {
        m_range -= 2;
        if (bin) {
            m_low += m_range;
            flush();
        } else {
            renorm();
        }
    }
    void raw(int n, std::uint32_t v) { m_w.u(n, v); }
    void align_zero() { m_w.align_zero(); }
    std::vector<std::uint8_t>& bytes() { return m_w.bytes(); }

private:
    void put(int b) {
        if (m_first) {
            m_first = false;
        } else {
            m_w.bit(static_cast<std::uint32_t>(b));
        }
        while (m_outstanding > 0) {
            m_w.bit(static_cast<std::uint32_t>(1 - b));
            --m_outstanding;
        }
    }
    void renorm() {
        while (m_range < 256) {
            if (m_low < 256) {
                put(0);
            } else if (m_low >= 512) {
                m_low -= 512;
                put(1);
            } else {
                m_low -= 256;
                ++m_outstanding;
            }
            m_range <<= 1;
            m_low <<= 1;
        }
    }
    void flush() {
        m_range = 2;
        renorm();
        put(static_cast<int>((m_low >> 9) & 1));
        m_w.u(2, ((m_low >> 7) & 3) | 1);
    }

    BitWriter m_w;
    std::uint32_t m_low = 0;
    std::uint32_t m_range = 510;
    int m_outstanding = 0;
    bool m_first = true;
};

struct Config {
    std::string name;
    int width = 112;
    int height = 72;
    int chroma = 1;
    int bit_depth = 8;
    int bit_depth_c = 8;
    int log2_ctb = 5;
    int log2_min_cb = 3;
    int log2_min_tb = 2;
    int log2_max_tb = 5;
    int depth_inter = 2;
    int depth_intra = 2;
    bool amp = true;
    bool sao = true;
    bool pcm = false;
    int pcm_bd = 8;
    int pcm_bd_c = 8;
    int log2_min_pcm = 3;
    int log2_max_pcm = 4;
    bool pcm_nofilter = false;
    bool scaling = false;
    bool pps_scaling = false;
    bool strong = true;
    bool tmvp = true;
    bool long_term = false;
    bool rot = false;
    bool ts_ctx = false;
    bool implicit_rdpcm = false;
    bool explicit_rdpcm = false;
    bool smooth_off = false;
    bool high_prec = false;
    bool rice = false;
    bool tiles = false;
    int tile_cols = 2;
    int tile_rows = 2;
    bool uniform = true;
    std::vector<int> col_w;
    std::vector<int> row_h;
    bool wpp = false;
    bool dependent = false;
    bool sdh = true;
    bool cabac_init = true;
    bool ts = false;
    int log2_max_ts = 2;
    bool qp_delta = true;
    int qp_depth = 1;
    int cb_off = 0;
    int cr_off = 0;
    bool slice_chroma_off = false;
    bool wp = false;
    bool bypass = false;
    bool lf_tiles = true;
    bool lf_slices = true;
    bool deblock_ctrl = true;
    bool lists_mod = false;
    int merge_level = 2;
    int extra_bits = 0;
    bool output_flag = false;
    bool cip = false;
    bool cross = false;
    bool cqp_list = false;
    int sao_scale = 0;
    bool header_ext = false;
    int frames = 4;
    bool bframes = true;
    int max_segments = 4;
    double p_pcm = 0.25;
    unsigned seed = 1;
};

class Oracle : public CabacOracle {
public:
    explicit Oracle(unsigned seed) : m_rng(seed) {}

    std::deque<int> plan;
    std::vector<std::vector<std::vector<std::uint8_t>>> segments;
    double p_pcm = 0.25;
    bool failed = false;

    void begin_substream() override {
        if (m_after_pcm) {
            m_after_pcm = false;
            m_w.restart();
            return;
        }
        if (m_ctus_left <= 0) {
            if (plan.empty()) {
                failed = true;
                m_ctus_left = 1 << 30;
            } else {
                m_ctus_left = plan.front();
                plan.pop_front();
            }
            segments.emplace_back();
        }
        m_w = CabacWriter();
        m_ones = 0;
    }

    int decision(std::uint8_t& ctx, int index) override {
        const int bin = pick(index);
        m_w.decision(ctx, bin);
        return bin;
    }

    int bypass() override {
        int b = coin(0.3);
        if (m_ones >= 3) b = 0;
        m_ones = b ? m_ones + 1 : 0;
        m_w.bypass(b);
        return b;
    }

    int terminate(int kind) override {
        if (kind == TERM_END_OF_SLICE) {
            --m_ctus_left;
            const int bin = m_ctus_left <= 0 ? 1 : 0;
            m_w.terminate(bin);
            if (bin) finish_substream();
            return bin;
        }
        if (kind == TERM_END_OF_SUBSET) {
            m_w.terminate(1);
            finish_substream();
            return 1;
        }
        const int bin = coin(p_pcm);
        m_w.terminate(bin);
        if (bin) {
            m_w.align_zero();
            m_after_pcm = true;
        }
        return bin;
    }

    void align_bypass() override {}

    std::uint32_t pcm_bits(int n) override {
        const std::uint32_t v = static_cast<std::uint32_t>(m_rng()) & ((1u << n) - 1u);
        m_w.raw(n, v);
        return v;
    }

    bool coin(double p) { return std::uniform_real_distribution<double>(0.0, 1.0)(m_rng) < p; }
    std::mt19937& rng() { return m_rng; }

private:
    void finish_substream() {
        m_w.align_zero();
        if (segments.empty()) segments.emplace_back();
        segments.back().push_back(m_w.bytes());
        m_w = CabacWriter();
    }

    int pick(int idx) {
        double p = 0.5;
        if (idx == CTX_SAO_MERGE) p = 0.3;
        else if (idx == CTX_SAO_TYPE) p = 0.7;
        else if (idx >= CTX_SPLIT_CU && idx < CTX_SPLIT_CU + 3) p = 0.55;
        else if (idx == CTX_TRANSQUANT_BYPASS) p = 0.2;
        else if (idx >= CTX_SKIP && idx < CTX_SKIP + 3) p = 0.3;
        else if (idx == CTX_PRED_MODE) p = 0.25;
        else if (idx == CTX_RQT_ROOT_CBF) p = 0.6;
        else if (idx >= CTX_QP_DELTA && idx < CTX_QP_DELTA + 2) p = 0.15;
        else if (idx >= CTX_MVD_G0 && idx <= CTX_MVD_G1) p = 0.3;
        else if (idx >= CTX_SIG && idx < CTX_SIG + 44) p = 0.35;
        else if (idx >= CTX_GT1 && idx < CTX_GT1 + 24) p = 0.3;
        else if (idx >= CTX_GT2 && idx < CTX_GT2 + 6) p = 0.3;
        else if (idx >= CTX_CBF_LUMA && idx < CTX_CBF_CHROMA + 5) p = 0.55;
        else if (idx >= CTX_REF_IDX && idx < CTX_REF_IDX + 2) p = 0.4;
        return coin(p) ? 1 : 0;
    }

    std::mt19937 m_rng;
    CabacWriter m_w;
    int m_ctus_left = 0;
    bool m_after_pcm = false;
    int m_ones = 0;
};

struct Segment {
    int ts = 0;
    int count = 0;
    bool dependent = false;
};

struct PicPlan {
    int poc = 0;
    int nal = 1;
    int slice_type = SLICE_I;
    std::vector<int> st_deltas;
    std::vector<int> lt_pocs;
    int nref[2] = {0, 0};
    bool mod[2] = {false, false};
    int entry[2][16] = {};
    bool tmvp = false;
    bool col_l0 = true;
    int col_idx = 0;
    bool output = true;
    bool use_sps_rps = false;
    bool lt_from_sps = false;
    bool lt_msb = false;
    bool lf_across = true;
    bool dbk_mode_b = false;
    bool dbk_dis = false;
    int beta = 0;
    int tc = 0;
    std::vector<Segment> segs;
};

class Generator {
public:
    explicit Generator(const Config& c) : m_cfg(c), m_rng(c.seed * 7919u + 13u) {}

    std::vector<VideoFrame> frames;

    bool run(std::vector<std::uint8_t>& out, std::string* error) {
        out.clear();
        std::vector<std::uint8_t> vps = make_nal(32, write_vps());
        std::vector<std::uint8_t> sps_r = write_sps();
        std::vector<std::uint8_t> pps_r = write_pps();
        std::vector<std::uint8_t> sps = make_nal(33, sps_r);
        std::vector<std::uint8_t> pps = make_nal(34, pps_r);
        {
            Bits b(sps_r.data(), sps_r.size());
            std::string e;
            if (!parse_sps(b, m_sps, &e)) {
                if (error) *error = "own SPS rejected: " + e;
                return false;
            }
            Bits b2(pps_r.data(), pps_r.size());
            if (!parse_pps(b2, m_pps, &e)) {
                if (error) *error = "own PPS rejected: " + e;
                return false;
            }
            if (!build_layout(m_sps, m_pps, m_lay)) {
                if (error) *error = "bad layout";
                return false;
            }
        }
        HevcDecoder dec;
        dec.open_annexb();
        frames.clear();
        Oracle oracle(m_cfg.seed);
        oracle.p_pcm = m_cfg.p_pcm;
        cabac_oracle() = &oracle;
        std::vector<std::uint8_t> head;
        head.insert(head.end(), vps.begin(), vps.end());
        head.insert(head.end(), sps.begin(), sps.end());
        head.insert(head.end(), pps.begin(), pps.end());
        out = head;
        bool ok = true;
        std::string err;
        for (int k = 0; k < m_cfg.frames && ok; ++k) {
            PicPlan pp = plan_picture(k);
            std::vector<std::uint8_t> pass;
            if (k == 0) pass = head;
            oracle.plan.clear();
            oracle.segments.clear();
            for (const Segment& s : pp.segs) {
                oracle.plan.push_back(s.count);
                std::vector<std::uint8_t> h = write_slice(pp, s, nullptr);
                std::vector<std::uint8_t> n = make_nal(pp.nal, h);
                pass.insert(pass.end(), n.begin(), n.end());
            }
            if (!dec.decode(pass.data(), pass.size(), k * 0.04, &err) || oracle.failed || oracle.segments.size() != pp.segs.size()) {
                ok = false;
                if (err.empty()) err = "oracle plan mismatch";
                break;
            }
            for (std::size_t i = 0; i < pp.segs.size(); ++i) {
                const std::vector<std::vector<std::uint8_t>>& subs = oracle.segments[i];
                std::vector<std::uint8_t> h = write_slice(pp, pp.segs[i], &subs);
                for (const auto& sb : subs) h.insert(h.end(), sb.begin(), sb.end());
                std::vector<std::uint8_t> n = make_nal(pp.nal, h);
                out.insert(out.end(), n.begin(), n.end());
            }
            m_history.push_back(pp);
            VideoFrame f;
            while (dec.receive(f)) frames.push_back(f);
        }
        dec.flush();
        {
            VideoFrame f;
            while (dec.receive(f)) frames.push_back(f);
        }
        cabac_oracle() = nullptr;
        if (!ok && error) *error = err;
        return ok;
    }

private:
    int rnd(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(m_rng); }
    bool coin(double p) { return std::uniform_real_distribution<double>(0.0, 1.0)(m_rng) < p; }

    void ptl(BitWriter& w) {
        const int prof = (m_cfg.chroma == 1 && m_cfg.bit_depth == 8 && !rext()) ? 1 : ((m_cfg.chroma == 1 && m_cfg.bit_depth <= 10 && !rext()) ? 2 : 4);
        w.u(2, 0);
        w.u(1, 0);
        w.u(5, static_cast<std::uint32_t>(prof));
        w.u(32, 1u << (31 - prof));
        w.u(1, 1);
        w.u(1, 0);
        w.u(1, 0);
        w.u(1, 1);
        w.u(32, 0);
        w.u(11, 0);
        w.u(1, 0);
        w.u(8, 120);
    }

    bool rext() const {
        return m_cfg.rot || m_cfg.ts_ctx || m_cfg.implicit_rdpcm || m_cfg.explicit_rdpcm || m_cfg.smooth_off || m_cfg.high_prec || m_cfg.rice || m_cfg.cross || m_cfg.cqp_list || m_cfg.sao_scale || m_cfg.log2_max_ts > 2;
    }

    std::vector<std::uint8_t> write_vps() {
        BitWriter w;
        w.u(4, 0);
        w.u(1, 1);
        w.u(1, 1);
        w.u(6, 0);
        w.u(3, 0);
        w.u(1, 1);
        w.u(16, 0xFFFF);
        ptl(w);
        w.u(1, 1);
        w.ue(5);
        w.ue(0);
        w.ue(0);
        w.u(6, 0);
        w.ue(0);
        w.u(1, 0);
        w.u(1, 0);
        w.trailing();
        return w.bytes();
    }

    void scaling_list(BitWriter& w) {
        for (int size_id = 0; size_id < 4; ++size_id) {
            for (int m = 0; m < 6; m += (size_id == 3) ? 3 : 1) {
                const int mode = rnd(0, 2);
                if (mode == 0) {
                    w.u(1, 0);
                    w.ue(0);
                } else if (mode == 1 && m > 0) {
                    w.u(1, 0);
                    w.ue(1);
                } else {
                    w.u(1, 1);
                    const int n = std::min(64, 1 << (4 + (size_id << 1)));
                    int next = 8;
                    if (size_id > 1) {
                        const int dc = rnd(4, 40);
                        w.se(dc - 8);
                        next = dc;
                    }
                    for (int i = 0; i < n; ++i) {
                        const int target = rnd(6, 60);
                        int d = target - next;
                        if (d > 127) d -= 256;
                        if (d < -128) d += 256;
                        w.se(d);
                        next = (next + d + 256) % 256;
                    }
                }
            }
        }
    }

    std::vector<std::uint8_t> write_sps() {
        BitWriter w;
        w.u(4, 0);
        w.u(3, 0);
        w.u(1, 1);
        ptl(w);
        w.ue(0);
        w.ue(static_cast<std::uint32_t>(m_cfg.chroma));
        if (m_cfg.chroma == 3) w.u(1, 0);
        const int mcb = 1 << m_cfg.log2_min_cb;
        const int cw = (m_cfg.width + mcb - 1) / mcb * mcb;
        const int ch = (m_cfg.height + mcb - 1) / mcb * mcb;
        w.ue(static_cast<std::uint32_t>(cw));
        w.ue(static_cast<std::uint32_t>(ch));
        const int sw = (m_cfg.chroma == 1 || m_cfg.chroma == 2) ? 2 : 1;
        const int sh = m_cfg.chroma == 1 ? 2 : 1;
        if (cw != m_cfg.width || ch != m_cfg.height) {
            w.u(1, 1);
            w.ue(0);
            w.ue(static_cast<std::uint32_t>((cw - m_cfg.width) / sw));
            w.ue(0);
            w.ue(static_cast<std::uint32_t>((ch - m_cfg.height) / sh));
        } else {
            w.u(1, 0);
        }
        w.ue(static_cast<std::uint32_t>(m_cfg.bit_depth - 8));
        w.ue(static_cast<std::uint32_t>(m_cfg.bit_depth_c - 8));
        w.ue(4);
        w.u(1, 1);
        w.ue(5);
        w.ue(0);
        w.ue(0);
        w.ue(static_cast<std::uint32_t>(m_cfg.log2_min_cb - 3));
        w.ue(static_cast<std::uint32_t>(m_cfg.log2_ctb - m_cfg.log2_min_cb));
        w.ue(static_cast<std::uint32_t>(m_cfg.log2_min_tb - 2));
        w.ue(static_cast<std::uint32_t>(m_cfg.log2_max_tb - m_cfg.log2_min_tb));
        w.ue(static_cast<std::uint32_t>(m_cfg.depth_inter));
        w.ue(static_cast<std::uint32_t>(m_cfg.depth_intra));
        w.flag(m_cfg.scaling);
        if (m_cfg.scaling) {
            w.u(1, 1);
            scaling_list(w);
        }
        w.flag(m_cfg.amp);
        w.flag(m_cfg.sao);
        w.flag(m_cfg.pcm);
        if (m_cfg.pcm) {
            w.u(4, static_cast<std::uint32_t>(m_cfg.pcm_bd - 1));
            w.u(4, static_cast<std::uint32_t>(m_cfg.pcm_bd_c - 1));
            w.ue(static_cast<std::uint32_t>(m_cfg.log2_min_pcm - 3));
            w.ue(static_cast<std::uint32_t>(m_cfg.log2_max_pcm - m_cfg.log2_min_pcm));
            w.flag(m_cfg.pcm_nofilter);
        }
        w.ue(2);
        w.ue(1);
        w.ue(0);
        w.ue(0);
        w.u(1, 1);
        w.u(1, 1);
        w.u(1, 1);
        w.ue(0);
        w.u(1, 1);
        w.u(1, 1);
        w.flag(m_cfg.long_term);
        if (m_cfg.long_term) {
            w.ue(1);
            w.u(8, 0);
            w.u(1, 1);
        }
        w.flag(m_cfg.tmvp);
        w.flag(m_cfg.strong);
        w.u(1, 0);
        const bool ext = rext();
        w.flag(ext);
        if (ext) {
            w.u(1, 1);
            w.u(7, 0);
            w.flag(m_cfg.rot);
            w.flag(m_cfg.ts_ctx);
            w.flag(m_cfg.implicit_rdpcm);
            w.flag(m_cfg.explicit_rdpcm);
            w.u(1, 0);
            w.flag(m_cfg.smooth_off);
            w.flag(m_cfg.high_prec);
            w.flag(m_cfg.rice);
            w.u(1, 0);
        }
        w.trailing();
        return w.bytes();
    }

    std::vector<std::uint8_t> write_pps() {
        BitWriter w;
        w.ue(0);
        w.ue(0);
        w.flag(m_cfg.dependent);
        w.flag(m_cfg.output_flag);
        w.u(3, static_cast<std::uint32_t>(m_cfg.extra_bits));
        w.flag(m_cfg.sdh);
        w.flag(m_cfg.cabac_init);
        w.ue(1);
        w.ue(0);
        w.se(0);
        w.flag(m_cfg.cip);
        w.flag(m_cfg.ts);
        w.flag(m_cfg.qp_delta);
        if (m_cfg.qp_delta) w.ue(static_cast<std::uint32_t>(m_cfg.qp_depth));
        w.se(m_cfg.cb_off);
        w.se(m_cfg.cr_off);
        w.flag(m_cfg.slice_chroma_off);
        w.flag(m_cfg.wp);
        w.flag(m_cfg.wp);
        w.flag(m_cfg.bypass);
        w.flag(m_cfg.tiles);
        w.flag(m_cfg.wpp);
        if (m_cfg.tiles) {
            w.ue(static_cast<std::uint32_t>(m_cfg.tile_cols - 1));
            w.ue(static_cast<std::uint32_t>(m_cfg.tile_rows - 1));
            w.flag(m_cfg.uniform);
            if (!m_cfg.uniform) {
                for (int i = 0; i < m_cfg.tile_cols - 1; ++i) w.ue(static_cast<std::uint32_t>(m_cfg.col_w[static_cast<std::size_t>(i)] - 1));
                for (int i = 0; i < m_cfg.tile_rows - 1; ++i) w.ue(static_cast<std::uint32_t>(m_cfg.row_h[static_cast<std::size_t>(i)] - 1));
            }
            w.flag(m_cfg.lf_tiles);
        }
        w.flag(m_cfg.lf_slices);
        w.flag(m_cfg.deblock_ctrl);
        if (m_cfg.deblock_ctrl) {
            w.u(1, 1);
            w.u(1, 0);
            w.se(0);
            w.se(0);
        }
        w.flag(m_cfg.pps_scaling);
        if (m_cfg.pps_scaling) scaling_list(w);
        w.flag(m_cfg.lists_mod);
        w.ue(static_cast<std::uint32_t>(m_cfg.merge_level - 2));
        w.flag(m_cfg.header_ext);
        const bool ext = m_cfg.log2_max_ts > 2 || m_cfg.cross || m_cfg.cqp_list || m_cfg.sao_scale;
        w.flag(ext);
        if (ext) {
            w.u(1, 1);
            w.u(7, 0);
            if (m_cfg.ts) w.ue(static_cast<std::uint32_t>(m_cfg.log2_max_ts - 2));
            w.flag(m_cfg.cross);
            w.flag(m_cfg.cqp_list);
            if (m_cfg.cqp_list) {
                w.ue(1);
                w.ue(5);
                w.se(-3);
                w.se(4);
                w.se(5);
                w.se(-2);
                w.se(2);
                w.se(2);
                w.se(-6);
                w.se(-5);
                w.se(7);
                w.se(1);
                w.se(0);
                w.se(-1);
            }
            w.ue(static_cast<std::uint32_t>(m_cfg.sao_scale));
            w.ue(static_cast<std::uint32_t>(m_cfg.sao_scale ? m_cfg.sao_scale - 1 : 0));
        }
        w.trailing();
        return w.bytes();
    }

    int tile_width_at(int rs) const {
        const int tx0 = m_lay.ctb_tile_x0[static_cast<std::size_t>(rs)];
        for (std::size_t i = 0; i + 1 < m_lay.col_bd.size(); ++i) {
            if (m_lay.col_bd[i] == tx0) return m_lay.col_bd[i + 1] - m_lay.col_bd[i];
        }
        return m_sps.pic_w_ctb;
    }

    std::vector<Segment> plan_segments() {
        std::vector<Segment> segs;
        const int pw = m_sps.pic_w_ctb;
        const int n = pw * m_sps.pic_h_ctb;
        int ts = 0;
        int slice_bound = -1;
        while (ts < n) {
            const int tid = m_lay.tile_id[static_cast<std::size_t>(ts)];
            int tile_end = ts;
            while (tile_end < n && m_lay.tile_id[static_cast<std::size_t>(tile_end)] == tid) ++tile_end;
            const bool tile_start = ts == 0 || m_lay.tile_id[static_cast<std::size_t>(ts - 1)] != tid;
            const int rs = m_lay.ts_to_rs[static_cast<std::size_t>(ts)];
            const int x = rs % pw;
            const int tx0 = m_lay.ctb_tile_x0[static_cast<std::size_t>(rs)];
            const int tw = tile_width_at(rs);
            bool dependent = !segs.empty() && m_cfg.dependent && coin(0.5) && !(m_cfg.tiles && tile_start);
            if (slice_bound >= 0 && ts >= slice_bound) dependent = false;
            int len;
            const bool last_allowed = static_cast<int>(segs.size()) >= m_cfg.max_segments - 1;
            if (m_cfg.tiles && tile_start && (last_allowed || coin(0.35))) {
                int end = tile_end;
                while (end < n && (last_allowed || coin(0.5))) {
                    const int t2 = m_lay.tile_id[static_cast<std::size_t>(end)];
                    int e2 = end;
                    while (e2 < n && m_lay.tile_id[static_cast<std::size_t>(e2)] == t2) ++e2;
                    end = e2;
                }
                len = end - ts;
                dependent = false;
            } else if (last_allowed && !m_cfg.tiles && !m_cfg.wpp) {
                len = n - ts;
            } else {
                int limit = tile_end - ts;
                if (m_cfg.wpp && x != tx0) limit = std::min(limit, tw - (x - tx0));
                if (dependent && slice_bound >= 0) limit = std::min(limit, slice_bound - ts);
                if (last_allowed) {
                    len = limit;
                } else {
                    len = rnd(1, limit);
                }
                if (m_cfg.wpp && x == tx0 && len > tw) len = (len / tw) * tw;
                if (len < 1) len = 1;
            }
            Segment s;
            s.ts = ts;
            s.count = len;
            s.dependent = dependent;
            if (!dependent) {
                slice_bound = -1;
                if (m_cfg.wpp && x != tx0) slice_bound = ts + (tw - (x - tx0));
            }
            segs.push_back(s);
            ts += len;
        }
        return segs;
    }

    PicPlan plan_picture(int k) {
        PicPlan p;
        p.poc = k;
        p.segs = plan_segments();
        p.output = !m_cfg.output_flag || coin(0.75);
        p.lf_across = coin(0.5);
        p.dbk_mode_b = coin(0.5);
        p.dbk_dis = coin(0.2);
        p.beta = rnd(-6, 6);
        p.tc = rnd(-6, 6);
        if (k == 0) {
            p.nal = 19;
            p.slice_type = SLICE_I;
            return p;
        }
        p.nal = 1;
        p.slice_type = coin(0.15) ? SLICE_I : ((m_cfg.bframes && coin(0.5)) ? SLICE_B : SLICE_P);
        const bool lt = m_cfg.long_term && k >= 2;
        p.st_deltas.push_back(-1);
        if (k >= 2 && !(lt && k - 2 == 0)) p.st_deltas.push_back(-2);
        if (k >= 3 && !(lt && k - 3 == 0) && coin(0.5)) p.st_deltas.push_back(-3);
        if (lt) p.lt_pocs.push_back(0);
        const int total = static_cast<int>(p.st_deltas.size() + p.lt_pocs.size());
        if (p.slice_type != SLICE_I) {
            p.nref[0] = rnd(1, std::min(4, total + 1));
            p.nref[1] = p.slice_type == SLICE_B ? rnd(1, std::min(4, total + 1)) : 0;
            for (int l = 0; l < 2; ++l) {
                if (m_cfg.lists_mod && total > 1 && coin(0.6)) {
                    p.mod[l] = true;
                    for (int i = 0; i < p.nref[l]; ++i) p.entry[l][i] = rnd(0, total - 1);
                }
            }
            p.tmvp = m_cfg.tmvp && coin(0.8);
            p.col_l0 = true;
            p.col_idx = rnd(0, p.nref[0] - 1);
        }
        p.use_sps_rps = coin(0.6);
        p.lt_from_sps = coin(0.5);
        p.lt_msb = coin(0.5);
        return p;
    }

    std::vector<std::uint8_t> write_slice(const PicPlan& p, const Segment& s, const std::vector<std::vector<std::uint8_t>>* subs) {
        std::mt19937 local(static_cast<unsigned>(p.poc * 1000 + s.ts * 7 + 1));
        auto lr = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(local); };
        auto lc = [&](double pr) { return std::uniform_real_distribution<double>(0.0, 1.0)(local) < pr; };
        BitWriter w;
        const bool first = s.ts == 0;
        w.flag(first);
        if (p.nal >= 16 && p.nal <= 23) w.u(1, 0);
        w.ue(0);
        const int nctb = m_sps.pic_w_ctb * m_sps.pic_h_ctb;
        if (!first) {
            if (m_cfg.dependent) w.flag(s.dependent);
            w.u(ceil_log2(static_cast<std::uint32_t>(nctb)), static_cast<std::uint32_t>(m_lay.ts_to_rs[static_cast<std::size_t>(s.ts)]));
        }
        if (!s.dependent) {
            for (int i = 0; i < m_cfg.extra_bits; ++i) w.u(1, static_cast<std::uint32_t>(lr(0, 1)));
            int st = p.slice_type;
            if (st != SLICE_I && lc(0.15)) st = SLICE_I;
            if (st == SLICE_B && lc(0.3)) st = SLICE_P;
            w.ue(static_cast<std::uint32_t>(st));
            if (m_cfg.output_flag) w.flag(p.output);
            if (p.nal != 19 && p.nal != 20) {
                w.u(8, static_cast<std::uint32_t>(p.poc & 255));
                bool use_sps = false;
                int idx = 0;
                if (p.st_deltas.size() == 1 && p.st_deltas[0] == -1) {
                    use_sps = true;
                    idx = 0;
                } else if (p.st_deltas.size() == 2 && p.st_deltas[0] == -1 && p.st_deltas[1] == -2) {
                    use_sps = true;
                    idx = 1;
                }
                if (use_sps && p.use_sps_rps) {
                    w.u(1, 1);
                    w.u(1, static_cast<std::uint32_t>(idx));
                } else {
                    w.u(1, 0);
                    w.u(1, 0);
                    w.ue(static_cast<std::uint32_t>(p.st_deltas.size()));
                    w.ue(0);
                    int prev = 0;
                    for (int d : p.st_deltas) {
                        w.ue(static_cast<std::uint32_t>(prev - d - 1));
                        w.u(1, 1);
                        prev = d;
                    }
                }
                if (m_cfg.long_term) {
                    const bool from_sps = !p.lt_pocs.empty() && p.lt_from_sps;
                    w.ue(from_sps ? 1 : 0);
                    w.ue(from_sps ? 0 : static_cast<std::uint32_t>(p.lt_pocs.size()));
                    for (std::size_t i = 0; i < p.lt_pocs.size(); ++i) {
                        if (!from_sps) {
                            w.u(8, static_cast<std::uint32_t>(p.lt_pocs[i] & 255));
                            w.u(1, 1);
                        }
                        const bool msb = p.lt_msb;
                        w.flag(msb);
                        if (msb) w.ue(static_cast<std::uint32_t>(((p.poc & ~255) - (p.lt_pocs[i] & ~255)) / 256));
                    }
                }
                if (m_cfg.tmvp) w.flag(p.tmvp);
            }
            bool sao_l = false;
            bool sao_c = false;
            if (m_cfg.sao) {
                sao_l = lc(0.7);
                w.flag(sao_l);
                if (m_cfg.chroma) {
                    sao_c = lc(0.7);
                    w.flag(sao_c);
                }
            }
            int nref[2] = {p.nref[0], st == SLICE_B ? p.nref[1] : 0};
            if (st != SLICE_I) {
                w.u(1, 1);
                w.ue(static_cast<std::uint32_t>(nref[0] - 1));
                if (st == SLICE_B) w.ue(static_cast<std::uint32_t>(nref[1] - 1));
                const int total = static_cast<int>(p.st_deltas.size() + p.lt_pocs.size());
                if (m_cfg.lists_mod && total > 1) {
                    const int bits = ceil_log2(static_cast<std::uint32_t>(total));
                    for (int l = 0; l < (st == SLICE_B ? 2 : 1); ++l) {
                        w.flag(p.mod[l]);
                        if (p.mod[l]) {
                            for (int i = 0; i < nref[l]; ++i) w.u(bits, static_cast<std::uint32_t>(p.entry[l][i]));
                        }
                    }
                }
                if (st == SLICE_B) w.flag(lc(0.3));
                if (m_cfg.cabac_init) w.flag(lc(0.5));
                if (p.tmvp) {
                    if (st == SLICE_B) w.flag(true);
                    if (nref[0] > 1) w.ue(static_cast<std::uint32_t>(p.col_idx));
                }
                if (m_cfg.wp) {
                    const int ld = lr(0, 7);
                    w.ue(static_cast<std::uint32_t>(ld));
                    int cd = ld;
                    if (m_cfg.chroma) {
                        cd = lr(0, 7);
                        w.se(cd - ld);
                    }
                    const int half_y = m_cfg.high_prec ? (1 << (m_cfg.bit_depth - 1)) : 128;
                    const int half_c = m_cfg.high_prec ? (1 << (m_cfg.bit_depth_c - 1)) : 128;
                    for (int l = 0; l < (st == SLICE_B ? 2 : 1); ++l) {
                        std::vector<bool> lf;
                        std::vector<bool> cf;
                        for (int i = 0; i < nref[l]; ++i) {
                            lf.push_back(lc(0.6));
                            w.flag(lf.back());
                        }
                        if (m_cfg.chroma) {
                            for (int i = 0; i < nref[l]; ++i) {
                                cf.push_back(lc(0.6));
                                w.flag(cf.back());
                            }
                        }
                        for (int i = 0; i < nref[l]; ++i) {
                            if (lf[static_cast<std::size_t>(i)]) {
                                w.se(lr(-20, 20));
                                w.se(lr(-half_y / 4, half_y / 4));
                            }
                            if (m_cfg.chroma && cf[static_cast<std::size_t>(i)]) {
                                for (int j = 0; j < 2; ++j) {
                                    w.se(lr(-20, 20));
                                    w.se(lr(-half_c, half_c));
                                }
                            }
                        }
                    }
                }
                w.ue(static_cast<std::uint32_t>(lr(0, 4)));
            }
            w.se(lr(-8, 10));
            if (m_cfg.slice_chroma_off) {
                w.se(lr(-4, 4));
                w.se(lr(-4, 4));
            }
            if (m_cfg.cqp_list) w.flag(lc(0.7));
            bool dis = false;
            if (m_cfg.deblock_ctrl) {
                const bool ov = p.dbk_mode_b ? true : lc(0.5);
                w.flag(ov);
                if (ov) {
                    dis = p.dbk_mode_b ? false : p.dbk_dis;
                    w.flag(dis);
                    if (!dis) {
                        w.se(p.dbk_mode_b ? p.beta : 0);
                        w.se(p.dbk_mode_b ? p.tc : 0);
                    }
                }
            }
            if (m_cfg.lf_slices && (sao_l || sao_c || !dis)) w.flag(p.lf_across);
        }
        if (m_cfg.tiles || m_cfg.wpp) {
            if (!subs || subs->size() <= 1) {
                w.ue(0);
            } else {
                w.ue(static_cast<std::uint32_t>(subs->size() - 1));
                std::uint32_t maxv = 1;
                for (std::size_t i = 0; i + 1 < subs->size(); ++i) maxv = std::max<std::uint32_t>(maxv, static_cast<std::uint32_t>((*subs)[i].size()));
                const int bits = std::max(1, ceil_log2(maxv + 1));
                w.ue(static_cast<std::uint32_t>(bits - 1));
                for (std::size_t i = 0; i + 1 < subs->size(); ++i) w.u(bits, static_cast<std::uint32_t>((*subs)[i].size() - 1));
            }
        }
        if (m_cfg.header_ext) {
            w.ue(2);
            w.u(8, 0xA5);
            w.u(8, 0x5A);
        }
        w.u(1, 1);
        w.align_zero();
        return w.bytes();
    }

    Config m_cfg;
    std::mt19937 m_rng;
    Sps m_sps;
    Pps m_pps;
    Layout m_lay;
    std::vector<PicPlan> m_history;
};

inline Config config_for(const std::string& mode, unsigned seed) {
    Config c;
    c.seed = seed;
    c.name = mode;
    if (mode == "tiles") {
        c.tiles = true;
        c.width = 192;
        c.height = 128;
        c.tile_cols = 3;
        c.tile_rows = 2;
        c.lf_tiles = (seed & 1) != 0;
    } else if (mode == "tiles2") {
        c.tiles = true;
        c.width = 216;
        c.height = 136;
        c.tile_cols = 3;
        c.tile_rows = 2;
        c.uniform = false;
        c.col_w = {1, 3};
        c.row_h = {2};
        c.lf_tiles = false;
        c.lf_slices = false;
    } else if (mode == "wpp") {
        c.wpp = true;
        c.dependent = true;
        c.width = 160;
        c.height = 96;
        c.max_segments = 6;
    } else if (mode == "tileswpp") {
        c.wpp = true;
        c.tiles = true;
        c.dependent = true;
        c.width = 192;
        c.height = 128;
    } else if (mode == "dep") {
        c.dependent = true;
        c.width = 160;
        c.height = 96;
        c.max_segments = 6;
    } else if (mode == "pcm") {
        c.pcm = true;
        c.pcm_bd = 7;
        c.pcm_bd_c = 5;
        c.log2_min_pcm = 3;
        c.log2_max_pcm = 5;
        c.ts = true;
    } else if (mode == "pcmnf") {
        c.pcm = true;
        c.pcm_bd = 6;
        c.pcm_bd_c = 7;
        c.log2_min_pcm = 4;
        c.log2_max_pcm = 5;
        c.pcm_nofilter = true;
        c.bypass = true;
        c.sao = false;
        c.bit_depth = c.bit_depth_c = 10;
    } else if (mode == "misc") {
        c.long_term = true;
        c.lists_mod = true;
        c.wp = true;
        c.scaling = true;
        c.pps_scaling = true;
        c.extra_bits = 2;
        c.output_flag = true;
        c.cip = true;
        c.slice_chroma_off = true;
        c.cb_off = 3;
        c.cr_off = -2;
        c.header_ext = true;
        c.merge_level = 4;
        c.frames = 6;
        c.dependent = true;
    } else if (mode == "mono" || mode == "monopcm") {
        c.chroma = 0;
        c.bit_depth = c.bit_depth_c = 12;
        c.tiles = true;
        c.width = 128;
        c.height = 96;
        c.ts = true;
        c.rice = true;
        c.sao_scale = 1;
        if (mode == "monopcm") {
            c.pcm = true;
            c.pcm_bd = c.pcm_bd_c = 9;
        }
    } else if (mode == "rext444") {
        c.chroma = 3;
        c.bit_depth = c.bit_depth_c = 10;
        c.rot = true;
        c.ts_ctx = true;
        c.implicit_rdpcm = true;
        c.explicit_rdpcm = true;
        c.rice = true;
        c.ts = true;
        c.log2_max_ts = 4;
        c.cross = true;
        c.cqp_list = true;
        c.wp = true;
        c.smooth_off = (seed & 1) != 0;
    } else if (mode == "rext444b") {
        c.chroma = 3;
        c.explicit_rdpcm = true;
        c.bypass = true;
        c.ts = true;
        c.log2_max_ts = 5;
        c.ts_ctx = true;
        c.rice = true;
        c.cross = true;
        c.sao = false;
    } else if (mode == "rext422") {
        c.chroma = 2;
        c.bit_depth = c.bit_depth_c = 12;
        c.rot = true;
        c.ts_ctx = true;
        c.implicit_rdpcm = true;
        c.explicit_rdpcm = true;
        c.rice = true;
        c.ts = true;
        c.log2_max_ts = 3;
        c.cqp_list = true;
        c.sao_scale = 2;
        c.wp = true;
    } else if (mode == "lossless") {
        c.bypass = true;
        c.implicit_rdpcm = true;
        c.explicit_rdpcm = true;
        c.rot = true;
        c.ts = true;
        c.pcm = true;
        c.pcm_nofilter = true;
        c.high_prec = true;
        c.wp = true;
        c.bit_depth = c.bit_depth_c = 10;
    }
    return c;
}

} // namespace hevcgen

namespace {

using namespace fizmo::video;

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

std::string hex_digest(const std::uint8_t d[16]) {
    std::string out;
    char buf[3];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", d[i]);
        out += buf;
    }
    return out;
}

class Md5 {
public:
    Md5() {
        m_state[0] = 0x67452301u;
        m_state[1] = 0xefcdab89u;
        m_state[2] = 0x98badcfeu;
        m_state[3] = 0x10325476u;
    }

    void update(const std::uint8_t* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            m_buffer[m_used++] = data[i];
            if (m_used == 64) {
                block(m_buffer);
                m_used = 0;
            }
        }
        m_length += size;
    }

    std::string hex() {
        const std::uint64_t bits = m_length * 8;
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (m_used != 56) update(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<std::uint8_t>(bits >> (8 * i));
        update(len, 8);
        std::uint8_t out[16];
        for (int i = 0; i < 4; ++i) {
            for (int b = 0; b < 4; ++b) out[i * 4 + b] = static_cast<std::uint8_t>(m_state[i] >> (8 * b));
        }
        return hex_digest(out);
    }

private:
    static std::uint32_t rotl(std::uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

    void block(const std::uint8_t* p) {
        static const std::uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
        };
        static const int r[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
        };
        std::uint32_t w[16];
        for (int i = 0; i < 16; ++i) {
            w[i] = static_cast<std::uint32_t>(p[i * 4]) | (static_cast<std::uint32_t>(p[i * 4 + 1]) << 8) |
                   (static_cast<std::uint32_t>(p[i * 4 + 2]) << 16) | (static_cast<std::uint32_t>(p[i * 4 + 3]) << 24);
        }
        std::uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f;
            int g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }
            const std::uint32_t t = d;
            d = c;
            c = b;
            b = b + rotl(a + f + k[i] + w[g], r[i]);
            a = t;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    std::uint32_t m_state[4];
    std::uint8_t m_buffer[64];
    std::size_t m_used = 0;
    std::uint64_t m_length = 0;
};

void hash_frame(Md5& m, const VideoFrame& f) {
    const int bps = bytes_per_sample(f.format);
    for (int c = 0; c < plane_count(f.format); ++c) {
        const std::size_t row = static_cast<std::size_t>(f.plane_width(c)) * static_cast<std::size_t>(bps);
        for (std::uint32_t y = 0; y < f.plane_height(c); ++y) m.update(f.plane(c) + y * f.stride[c], row);
    }
}

std::string frames_md5(const std::vector<VideoFrame>& frames) {
    Md5 m;
    for (const VideoFrame& f : frames) hash_frame(m, f);
    return m.hex();
}

bool same_pixels(const VideoFrame& a, const VideoFrame& b) {
    if (a.width != b.width || a.height != b.height || a.format != b.format) return false;
    const int bps = bytes_per_sample(a.format);
    for (int c = 0; c < plane_count(a.format); ++c) {
        const std::size_t row = static_cast<std::size_t>(a.plane_width(c)) * static_cast<std::size_t>(bps);
        for (std::uint32_t y = 0; y < a.plane_height(c); ++y) {
            if (std::memcmp(a.plane(c) + y * a.stride[c], b.plane(c) + y * b.stride[c], row) != 0) return false;
        }
    }
    return true;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out) {
    out.clear();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::uint8_t buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

std::string source_dir() {
    std::string f = __FILE__;
    const std::size_t p = f.find_last_of("/\\");
    return p == std::string::npos ? std::string(".") : f.substr(0, p);
}

std::string data_dir() {
    const char* e = std::getenv("FIZMO_HEVC_TEST_DATA");
    if (e && *e) return e;
    return source_dir() + "/video/hevc";
}

std::string temp_dir() {
    const char* t = std::getenv("TMPDIR");
    if (!t || !*t) t = std::getenv("TEMP");
    if (!t || !*t) t = "/tmp";
    return t;
}

bool decode_es(const std::vector<std::uint8_t>& data, std::vector<VideoFrame>& out, HevcStats* stats, bool weave, std::uint64_t* errors) {
    out.clear();
    HevcFileReader r;
    if (!r.open(data.data(), data.size())) return false;
    HevcDecoder d;
    d.open_annexb();
    d.set_verify_hash(true);
    d.set_weave_fields(weave);
    std::vector<std::uint8_t> au;
    VideoFrame f;
    int i = 0;
    while (r.read(au)) {
        d.decode(au.data(), au.size(), i * 0.04);
        ++i;
        while (d.receive(f)) out.push_back(f);
    }
    d.flush();
    while (d.receive(f)) out.push_back(f);
    if (stats) *stats = d.stats();
    if (errors) *errors = d.error_count();
    return true;
}

bool decode_demux(const std::vector<std::uint8_t>& data, std::vector<VideoFrame>& out, std::string* error, std::uint64_t* errors) {
    out.clear();
    Demuxer dm;
    if (!dm.open(data.data(), data.size(), error)) return false;
    const int vt = dm.best_track(MediaType::video);
    if (vt < 0) {
        if (error) *error = "no video track";
        return false;
    }
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(dm.tracks()[static_cast<std::size_t>(vt)], error);
    if (!dec) return false;
    Packet p;
    VideoFrame f;
    while (dm.read(p)) {
        if (p.track != static_cast<std::uint32_t>(vt)) continue;
        dec->decode_packet(p);
        while (dec->receive(f)) out.push_back(f);
    }
    dec->flush();
    while (dec->receive(f)) out.push_back(f);
    if (errors) *errors = dec->error_count();
    return true;
}

struct ManifestEntry {
    std::string file;
    std::string kind;
    int frames = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string format;
    std::string md5;
    std::string flags;
};

std::vector<ManifestEntry> load_manifest(const std::string& dir) {
    std::vector<ManifestEntry> out;
    std::vector<std::uint8_t> text;
    if (!read_file(dir + "/manifest.txt", text)) return out;
    std::string s(text.begin(), text.end());
    std::size_t pos = 0;
    while (pos < s.size()) {
        std::size_t e = s.find('\n', pos);
        if (e == std::string::npos) e = s.size();
        const std::string line = s.substr(pos, e - pos);
        pos = e + 1;
        if (line.empty()) continue;
        ManifestEntry m;
        char file[256], kind[32], fmt[32], md5[64], flags[128];
        unsigned w = 0, h = 0;
        int n = 0;
        flags[0] = 0;
        const int got = std::sscanf(line.c_str(), "%255s %31s %d %u %u %31s %63s %127s", file, kind, &n, &w, &h, fmt, md5, flags);
        if (got < 7) continue;
        m.file = file;
        m.kind = kind;
        m.frames = n;
        m.width = w;
        m.height = h;
        m.format = fmt;
        m.md5 = md5;
        m.flags = got >= 8 ? flags : "";
        out.push_back(m);
    }
    return out;
}

bool has_flag(const ManifestEntry& m, const char* f) {
    return m.flags.find(f) != std::string::npos;
}

void test_corpus(const std::string& dir, const std::vector<ManifestEntry>& man) {
    check(!man.empty(), "test corpus manifest found in " + dir);
    for (const ManifestEntry& m : man) {
        std::vector<std::uint8_t> data;
        if (!read_file(dir + "/" + m.file, data)) {
            check(false, "read " + m.file);
            continue;
        }
        std::vector<VideoFrame> frames;
        HevcStats st;
        std::uint64_t errors = 0;
        std::string err;
        bool ok;
        if (m.kind == "es") {
            ok = decode_es(data, frames, &st, !has_flag(m, "fields"), &errors);
        } else {
            ok = decode_demux(data, frames, &err, &errors);
        }
        check(ok, m.file + ": opens " + err);
        if (!ok) continue;
        check(static_cast<int>(frames.size()) == m.frames, m.file + ": frame count " + std::to_string(frames.size()) + " vs " + std::to_string(m.frames));
        check(!frames.empty() && frames[0].width == m.width && frames[0].height == m.height, m.file + ": size");
        check(!frames.empty() && m.format == format_name(frames[0].format), m.file + ": format " + (frames.empty() ? std::string("?") : std::string(format_name(frames[0].format))));
        check(frames_md5(frames) == m.md5, m.file + ": bit-exact output (md5)");
        check(errors == 0, m.file + ": no decode errors");
        if (m.kind == "es" && has_flag(m, "hash")) {
            check(st.hash_checked > 0 && st.hash_mismatches == 0, m.file + ": SEI picture hashes verified (" + std::to_string(st.hash_checked) + ")");
        }
    }
}

void test_fields(const std::string& dir) {
    std::vector<std::uint8_t> data;
    if (!read_file(dir + "/fields_tff.hevc", data)) {
        check(false, "field stream present");
        return;
    }
    std::vector<VideoFrame> fields;
    std::vector<VideoFrame> woven;
    decode_es(data, fields, nullptr, false, nullptr);
    HevcStats st;
    decode_es(data, woven, &st, true, nullptr);
    check(fields.size() == woven.size() * 2 && !woven.empty(), "fields weave into frames");
    check(st.fields_woven == woven.size(), "woven field pair count");
    bool ok = !woven.empty();
    for (std::size_t k = 0; k < woven.size() && ok; ++k) {
        const VideoFrame& f = woven[k];
        const VideoFrame& t = fields[2 * k];
        const VideoFrame& b = fields[2 * k + 1];
        if (f.height != t.height * 2 || !f.interlaced || !f.top_field_first || f.structure != PictureStructure::field_pair) ok = false;
        if (t.structure != PictureStructure::top_field || b.structure != PictureStructure::bottom_field) ok = false;
        const int bps = bytes_per_sample(f.format);
        for (int c = 0; c < plane_count(f.format) && ok; ++c) {
            const std::size_t row = static_cast<std::size_t>(t.plane_width(c)) * static_cast<std::size_t>(bps);
            for (std::uint32_t y = 0; y < t.plane_height(c) && ok; ++y) {
                if (std::memcmp(f.plane(c) + (2 * y) * f.stride[c], t.plane(c) + y * t.stride[c], row) != 0) ok = false;
                if (std::memcmp(f.plane(c) + (2 * y + 1) * f.stride[c], b.plane(c) + y * b.stride[c], row) != 0) ok = false;
            }
        }
    }
    check(ok, "woven frames interleave top and bottom fields and are flagged interlaced");
}

void test_seek(const std::string& dir, const std::string& file) {
    std::vector<std::uint8_t> data;
    if (!read_file(dir + "/" + file, data)) {
        check(false, "seek stream " + file);
        return;
    }
    std::vector<VideoFrame> ref;
    std::string err;
    check(decode_demux(data, ref, &err, nullptr), file + ": reference decode " + err);
    Demuxer dm;
    check(dm.open(data.data(), data.size(), &err), file + ": demux open");
    const int vt = dm.best_track(MediaType::video);
    if (vt < 0) return;
    std::vector<double> keys;
    Packet p;
    while (dm.read(p)) {
        if (p.keyframe) keys.push_back(p.pts);
    }
    check(keys.size() >= 3, file + ": has several random access points");
    std::unique_ptr<VideoDecoder> dec = create_video_decoder(dm.tracks()[static_cast<std::size_t>(vt)], &err);
    if (!dec) return;
    int checked = 0;
    bool all_ok = true;
    bool first_is_key = true;
    for (std::size_t k = 1; k < keys.size(); ++k) {
        dm.seek(keys[k], true);
        dec->reset();
        std::vector<VideoFrame> got;
        VideoFrame f;
        int packets = 0;
        while (dm.read(p) && packets < 12) {
            if (p.track != static_cast<std::uint32_t>(vt)) continue;
            dec->decode_packet(p);
            ++packets;
            while (dec->receive(f)) got.push_back(f);
        }
        dec->flush();
        while (dec->receive(f)) got.push_back(f);
        if (got.empty() || std::fabs(got[0].pts - keys[k]) > 1e-6) first_is_key = false;
        for (const VideoFrame& g : got) {
            bool found = false;
            for (const VideoFrame& r : ref) {
                if (std::fabs(r.pts - g.pts) < 1e-6) {
                    found = true;
                    if (!same_pixels(r, g)) all_ok = false;
                    ++checked;
                    break;
                }
            }
            if (!found) all_ok = false;
        }
    }
    check(all_ok && checked > 0, file + ": frames after seeking to each keyframe match the full decode (" + std::to_string(checked) + ")");
    check(first_is_key, file + ": RASL pictures are skipped and output starts at the CRA");
}

void test_rasl_counter(const std::string& dir) {
    std::vector<std::uint8_t> data;
    if (!read_file(dir + "/opengop.hevc", data)) return;
    HevcFileReader r;
    check(r.open(data.data(), data.size()), "open-GOP stream opens");
    std::vector<std::vector<std::uint8_t>> aus;
    std::vector<std::uint8_t> au;
    while (r.read(au)) aus.push_back(au);
    auto has_type = [](const std::vector<std::uint8_t>& a, int lo, int hi) {
        for (std::size_t j = 0; j + 4 < a.size(); ++j) {
            if (a[j] == 0 && a[j + 1] == 0 && a[j + 2] == 1) {
                const int t = (a[j + 3] >> 1) & 63;
                if (t >= lo && t <= hi) return true;
            }
        }
        return false;
    };
    std::size_t cra = 0;
    std::size_t last_cra = 0;
    for (std::size_t i = 1; i < aus.size() && !cra; ++i) {
        if (has_type(aus[i], 21, 21)) last_cra = i;
        if (last_cra && has_type(aus[i], 8, 9)) cra = last_cra;
    }
    check(cra > 0, "open-GOP stream has a CRA picture after the first");
    HevcDecoder d;
    d.open_annexb();
    VideoFrame f;
    int out = 0;
    for (std::size_t i = cra; i < aus.size(); ++i) {
        d.decode(aus[i].data(), aus[i].size(), static_cast<double>(i));
        while (d.receive(f)) ++out;
    }
    d.flush();
    while (d.receive(f)) ++out;
    check(d.stats().skipped_rasl > 0, "decoding from a CRA skips its RASL pictures (" + std::to_string(d.stats().skipped_rasl) + ")");
    check(out > 0 && d.error_count() == 0, "decoding from a CRA produces frames without errors");
}

void test_api(const std::string& dir) {
    std::vector<std::uint8_t> es;
    read_file(dir + "/main_bpyramid.hevc", es);
    check(is_annexb_hevc(es.data(), es.size()), "is_annexb_hevc on H.265 stream");
    const std::uint8_t avc[] = {0, 0, 0, 1, 0x67, 0x42, 0xc0, 0x0a, 0xdb, 0x35};
    check(!is_annexb_hevc(avc, sizeof(avc)), "is_annexb_hevc rejects H.264");
    const std::uint8_t avc_aud[] = {0, 0, 0, 1, 0x09, 0xf0, 0, 0, 0, 1};
    check(!is_annexb_hevc(avc_aud, sizeof(avc_aud)), "is_annexb_hevc rejects H.264 AUD");
    check(detect_container(es.data(), std::min<std::size_t>(es.size(), 64)) == "hevc", "detect_container reports hevc");
    check(video_codec_supported("hevc") && video_codec_display_name("hevc") == "H.265 (HEVC)", "hevc codec supported");
    TrackInfo t;
    t.type = MediaType::video;
    t.codec = "hevc";
    std::string err;
    check(create_video_decoder(t, &err) != nullptr, "hevc decoder without hvcC opens in Annex B mode");
    t.codec_config = {1, 2, 3};
    check(!create_video_decoder(t, &err) && !err.empty(), "broken hvcC rejected");
    HevcFileReader r;
    check(r.open(es.data(), es.size()), "HevcFileReader opens");
    std::vector<std::uint8_t> au;
    int n = 0;
    while (r.read(au)) ++n;
    r.rewind();
    int n2 = 0;
    while (r.read(au)) ++n2;
    check(n > 0 && n == n2, "HevcFileReader rewind");
    HevcDecoder d;
    check(!d.is_open() && d.open_annexb() && d.is_open(), "decoder open state");
    check(d.decode(nullptr, 0, 0.0), "empty packet accepted");
    r.rewind();
    VideoFrame f;
    int got = 0;
    while (r.read(au)) {
        d.decode(au.data(), au.size(), got * 0.04);
        while (d.receive(f)) ++got;
    }
    const int before_flush = got;
    d.flush();
    while (d.receive(f)) ++got;
    check(got == n && before_flush < got, "flush drains reordered pictures");
    const HevcInfo& info = d.info();
    check(info.width == 176 && info.height == 144 && info.bit_depth == 8 && info.chroma_format_idc == 1 && info.profile_idc == 1, "stream info");
    check(info.max_num_reorder_pics > 0 && info.dpb_frames > 0 && std::fabs(info.frame_rate - 25.0) < 1e-6, "dpb and timing info");
    check(d.frames_decoded() == static_cast<std::uint64_t>(n) && d.error_count() == 0, "frame and error counters");
    const std::uint8_t junk[] = {0, 0, 1, 0x40, 0x01, 0xff, 0xff, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x77};
    HevcDecoder j;
    j.open_annexb();
    j.decode(junk, sizeof(junk), 0.0);
    check(!j.receive(f), "junk produces no frames");
    std::vector<std::uint8_t> mp4;
    read_file(dir + "/main_bpyramid.mp4", mp4);
    Demuxer dm;
    check(dm.open(mp4.data(), mp4.size(), &err) && dm.best_track(MediaType::video) == 0, "hevc mp4 opens");
    if (dm.is_open()) {
        const TrackInfo& vt = dm.tracks()[0];
        check(vt.codec == "hevc" && !vt.codec_config.empty() && vt.width == 176 && vt.height == 144, "hevc mp4 track info");
        std::unique_ptr<VideoDecoder> dec = create_video_decoder(vt, &err);
        check(dec != nullptr && std::string(dec->codec()) == "hevc", "decoder from hvcC");
        Packet p;
        int disposable = 0;
        int frames = 0;
        int decoded_packets = 0;
        while (dm.read(p)) {
            if (dec && dec->is_disposable(p.data.data(), p.data.size())) {
                ++disposable;
                continue;
            }
            ++decoded_packets;
            if (dec) dec->decode_packet(p);
            while (dec && dec->receive(f)) ++frames;
        }
        if (dec) dec->flush();
        while (dec && dec->receive(f)) ++frames;
        check(disposable > 0 && frames == decoded_packets && dec && dec->error_count() == 0, "skipping disposable pictures keeps the rest decodable (" + std::to_string(disposable) + " skipped)");
    }
    Demuxer de;
    check(de.open(es.data(), es.size(), &err) && de.format_name() == "hevc", "elementary stream demuxer");
    if (de.is_open()) {
        const TrackInfo& vt = de.tracks()[0];
        check(vt.codec == "hevc" && vt.width == 176 && vt.height == 144 && std::fabs(vt.frame_rate - 25.0) < 1e-6 && vt.frame_count == static_cast<std::uint64_t>(n), "elementary stream track info");
        check(std::fabs(de.duration() - n / 25.0) < 1e-6, "elementary stream duration");
        Packet p;
        std::vector<double> pts;
        while (de.read(p)) pts.push_back(p.pts);
        std::vector<double> sorted = pts;
        std::sort(sorted.begin(), sorted.end());
        bool steps = sorted.size() == static_cast<std::size_t>(n);
        for (std::size_t i = 0; i < sorted.size() && steps; ++i) steps = std::fabs(sorted[i] - i / 25.0) < 1e-6;
        check(steps && pts != sorted, "elementary stream pts follow picture order");
    }
}

void test_player(const std::string& dir) {
    const char* files[] = {"main_bpyramid.mp4", "main_bpyramid.mkv", "main_bpyramid.hevc"};
    std::vector<std::uint8_t> es;
    read_file(dir + "/main_bpyramid.hevc", es);
    std::vector<VideoFrame> ref;
    decode_es(es, ref, nullptr, true, nullptr);
    for (const char* name : files) {
        const std::string path = dir + "/" + name;
        MediaInfo mi = probe_media(path);
        check(mi.valid && mi.video_supported && mi.video_codec == "hevc" && mi.width == 176 && mi.height == 144, std::string("probe ") + name);
        VideoFrame f;
        std::string err;
        const bool ok = extract_frame(path, 0.25, f, &err);
        check(ok && ref.size() > 6 && same_pixels(f, ref[6]), std::string("extract_frame ") + name + " " + err);
        MediaPlayer p;
        check(p.open(path, &err) && p.has_video() && p.width() == 176, std::string("MediaPlayer opens ") + name + " " + err);
        const VideoFrame* cf = nullptr;
        std::uint64_t serial = 0;
        bool shown = false;
        for (int i = 0; i < 3000 && !shown; ++i) {
            p.current_frame(cf, serial);
            shown = cf != nullptr && !p.is_seeking();
            if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(shown && !ref.empty() && same_pixels(*cf, ref[0]), std::string("MediaPlayer shows the first frame of ") + name);
        p.seek(0.4, true);
        bool sought = false;
        for (int i = 0; i < 3000 && !sought; ++i) {
            p.current_frame(cf, serial);
            sought = !p.is_seeking() && cf != nullptr && std::fabs(cf->pts - 0.4) < 1e-6;
            if (!sought) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(sought && ref.size() > 10 && same_pixels(*cf, ref[10]), std::string("MediaPlayer accurate seek in ") + name);
    }
}

void test_generated_consistency() {
    const char* modes[] = {"basic", "dep", "wpp", "tiles", "tiles2", "tileswpp", "pcm", "pcmnf", "misc", "mono", "monopcm", "rext444", "rext444b", "rext422", "lossless"};
    for (const char* mode : modes) {
        for (unsigned seed = 11; seed <= 12; ++seed) {
            hevcgen::Config c = hevcgen::config_for(mode, seed);
            hevcgen::Generator g(c);
            std::vector<std::uint8_t> stream;
            std::string err;
            const bool ok = g.run(stream, &err);
            check(ok, std::string("generate ") + mode + " " + err);
            if (!ok) continue;
            std::vector<VideoFrame> frames;
            std::uint64_t errors = 0;
            decode_es(stream, frames, nullptr, true, &errors);
            check(errors == 0 && !frames.empty() && frames.size() == g.frames.size() && frames_md5(frames) == frames_md5(g.frames),
                  std::string("generated ") + mode + " stream decodes identically from its bytes (" + std::to_string(frames.size()) + " frames)");
        }
    }
}

std::uint32_t fuzz_rand(std::uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

void test_fuzz(const std::string& dir, const std::vector<ManifestEntry>& man) {
    int iters = 240;
    if (const char* e = std::getenv("FIZMO_HEVC_FUZZ_ITERS")) iters = std::atoi(e);
    std::vector<std::vector<std::uint8_t>> inputs;
    for (const ManifestEntry& m : man) {
        if (m.kind != "es") continue;
        std::vector<std::uint8_t> d;
        if (read_file(dir + "/" + m.file, d) && d.size() < 400000) inputs.push_back(d);
    }
    if (inputs.empty()) return;
    std::uint32_t s = 0x9e3779b9u;
    int survived = 0;
    for (int it = 0; it < iters; ++it) {
        std::vector<std::uint8_t> d = inputs[static_cast<std::size_t>(it) % inputs.size()];
        const int kind = static_cast<int>(fuzz_rand(s) % 6);
        const int count = 1 + static_cast<int>(fuzz_rand(s) % 24);
        for (int k = 0; k < count && !d.empty(); ++k) {
            const std::size_t at = fuzz_rand(s) % d.size();
            switch (kind) {
            case 0: d[at] ^= static_cast<std::uint8_t>(1u << (fuzz_rand(s) % 8)); break;
            case 1: d[at] = static_cast<std::uint8_t>(fuzz_rand(s)); break;
            case 2: d.insert(d.begin() + static_cast<std::ptrdiff_t>(at), static_cast<std::uint8_t>(fuzz_rand(s))); break;
            case 3: d.erase(d.begin() + static_cast<std::ptrdiff_t>(at)); break;
            case 4: d.resize(at + 1); break;
            default: {
                const std::size_t len = 1 + fuzz_rand(s) % 64;
                const std::size_t from = fuzz_rand(s) % d.size();
                for (std::size_t q = 0; q < len && from + q < d.size() && at + q < d.size(); ++q) d[at + q] = d[from + q];
                break;
            }
            }
        }
        HevcDecoder dec;
        dec.open_annexb();
        dec.set_verify_hash(true);
        HevcFileReader r;
        VideoFrame f;
        if (r.open(d.data(), d.size())) {
            std::vector<std::uint8_t> au;
            int aus = 0;
            while (r.read(au) && aus < 400) {
                dec.decode(au.data(), au.size(), aus * 0.04);
                ++aus;
                while (dec.receive(f)) {
                }
            }
        } else {
            dec.decode(d.data(), d.size(), 0.0);
        }
        dec.flush();
        while (dec.receive(f)) {
        }
        dec.reset();
        dec.decode(d.data(), d.size(), 1.0);
        dec.flush();
        while (dec.receive(f)) {
        }
        ++survived;
    }
    check(survived == iters, "fuzzed streams decode without crashing (" + std::to_string(iters) + " mutations)");
}

bool have_ffmpeg() {
    if (std::getenv("FIZMO_HEVC_NO_FFMPEG")) return false;
#if defined(_WIN32)
    return std::system("ffmpeg -hide_banner -loglevel error -version > NUL 2>&1") == 0;
#else
    return std::system("ffmpeg -hide_banner -loglevel error -version > /dev/null 2>&1") == 0;
#endif // _WIN32
}

bool ffmpeg_md5(const std::string& input, std::string& md5, int& frames, std::size_t frame_size, const std::string& extra = "") {
    const std::string raw = input + ".raw";
    const std::string cmd = "ffmpeg -hide_banner -loglevel error -y " + extra + " -i \"" + input + "\" -f rawvideo \"" + raw + "\"";
    if (std::system(cmd.c_str()) != 0) return false;
    std::vector<std::uint8_t> data;
    const bool ok = read_file(raw, data);
    std::remove(raw.c_str());
    if (!ok) return false;
    Md5 m;
    m.update(data.data(), data.size());
    md5 = m.hex();
    frames = frame_size ? static_cast<int>(data.size() / frame_size) : 0;
    return true;
}

std::size_t frame_bytes(const VideoFrame& f) {
    std::size_t n = 0;
    for (int c = 0; c < plane_count(f.format); ++c) n += static_cast<std::size_t>(f.plane_width(c)) * f.plane_height(c) * static_cast<std::size_t>(bytes_per_sample(f.format));
    return n;
}

struct X265Case {
    const char* name;
    const char* params;
    const char* pre;
    const char* pix;
    const char* size;
    int frames;
};

const X265Case k_corpus_x265[] = {
    {"main_bpyramid", "bframes=4:b-pyramid=1:ref=3:keyint=30", "", "yuv420p", "176x144", 12},
    {"main_slow", "bframes=3:rect=1:amp=1:ref=4", "-preset slow", "yuv420p", "128x96", 8},
    {"main_ctu16_odd", "ctu=16:bframes=2:max-tu-size=8:no-sao=1", "", "yuv420p", "98x66", 8},
    {"main_ctu32", "ctu=32:min-cu-size=16:bframes=2:tu-intra-depth=3:tu-inter-depth=3", "", "yuv420p", "136x72", 8},
    {"main_wpp", "wpp=1:bframes=2", "", "yuv420p", "256x144", 6},
    {"main_slices", "slices=4:bframes=2", "", "yuv420p", "256x144", 6},
    {"main_weighted", "weightp=1:weightb=1:bframes=3", "-vf fade=in:0:8", "yuv420p", "128x96", 10},
    {"main_scaling", "scaling-list=default:bframes=2", "", "yuv420p", "128x96", 6},
    {"main_tskip", "tskip=1:bframes=2:signhide=0", "-preset slow", "yuv420p", "96x64", 6},
    {"main_lossless", "lossless=1:bframes=2", "", "yuv420p", "96x64", 5},
    {"main_culossless", "cu-lossless=1:bframes=2", "-preset slow", "yuv420p", "96x64", 5},
    {"main_cip", "constrained-intra=1:bframes=2:keyint=4", "", "yuv420p", "96x64", 8},
    {"main_temporal", "temporal-layers=1:bframes=3:b-pyramid=1", "", "yuv420p", "96x64", 9},
    {"main_qp", "qp=4:bframes=1:no-sao=1:deblock=-3,3", "", "yuv420p", "64x64", 3},
    {"opengop", "open-gop=1:keyint=8:min-keyint=8:bframes=3:scenecut=0", "", "yuv420p", "96x64", 24},
    {"main10", "bframes=3", "", "yuv420p10le", "128x96", 6},
    {"main12", "bframes=3:weightp=1", "", "yuv420p12le", "128x96", 6},
    {"main422_10", "bframes=3", "", "yuv422p10le", "128x96", 6},
    {"main422_12", "bframes=2:tskip=1", "-preset slow", "yuv422p12le", "96x64", 5},
    {"main444_8", "bframes=3", "", "yuv444p", "128x96", 6},
    {"main444_10", "bframes=3:lossless=0:scaling-list=default", "", "yuv444p10le", "128x96", 6},
    {"main444_12", "bframes=2", "-preset slow", "yuv444p12le", "96x64", 5},
    {"main444_gbr", "bframes=2", "", "gbrp", "96x64", 5},
    {"main444_lossless", "lossless=1:bframes=2", "", "yuv444p10le", "64x64", 4},
    {"gray8", "bframes=2", "", "gray", "96x64", 5},
    {"gray10", "bframes=2", "", "gray10le", "96x64", 5},
    {"gray12", "bframes=2", "", "gray12le", "96x64", 5},
};

const X265Case k_extended_x265[] = {
    {"x_placebo", "bframes=3", "-preset placebo", "yuv420p", "176x144", 6},
    {"x_ufast", "bframes=3", "-preset ultrafast", "yuv420p", "176x144", 8},
    {"x_refs", "ref=6:bframes=0:keyint=40", "", "yuv420p", "128x96", 16},
    {"x_aq", "aq-mode=3:cutree=1:bframes=3", "", "yuv420p", "128x96", 10},
    {"x_merge", "max-merge=5:bframes=3:me=star:subme=7", "", "yuv420p", "128x96", 10},
    {"x_saonb", "sao-non-deblock=1:bframes=2", "", "yuv420p", "128x96", 8},
    {"x_10placebo", "bframes=2", "-preset placebo", "yuv420p10le", "128x96", 5},
    {"x_422placebo", "bframes=2", "-preset placebo", "yuv422p10le", "128x96", 5},
    {"x_444placebo", "bframes=2", "-preset placebo", "yuv444p12le", "128x96", 5},
    {"x_422odd", "bframes=2:ctu=16", "", "yuv422p10le", "90x50", 6},
    {"x_444odd", "bframes=2:ctu=32", "", "yuv444p", "90x50", 6},
    {"x_12odd", "bframes=2", "", "yuv420p12le", "90x50", 6},
    {"x_still", "", "-profile:v mainstillpicture", "yuv420p", "176x144", 1},
};

bool x265_encode(const X265Case& c, const std::string& out) {
    const std::string cmd = std::string("ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=") + c.size + ":rate=25 -frames:v " + std::to_string(c.frames) + " " + c.pre +
                            " -pix_fmt " + c.pix + " -c:v libx265 -x265-params \"log-level=error:hash=1" + (c.params[0] ? ":" : "") + c.params + "\" \"" + out + "\"";
    return std::system(cmd.c_str()) == 0;
}

bool compare_with_ffmpeg(const std::string& path, const std::string& what, bool fields = false) {
    std::vector<std::uint8_t> data;
    if (!read_file(path, data)) return false;
    std::vector<VideoFrame> frames;
    std::uint64_t errors = 0;
    decode_es(data, frames, nullptr, !fields, &errors);
    if (frames.empty()) {
        check(false, what + ": decodes");
        return false;
    }
    std::string md5;
    int n = 0;
    if (!ffmpeg_md5(path, md5, n, frame_bytes(frames[0]))) {
        check(false, what + ": ffmpeg reference");
        return false;
    }
    const bool ok = md5 == frames_md5(frames) && n == static_cast<int>(frames.size()) && errors == 0;
    check(ok, what + ": bit-exact against ffmpeg (" + std::to_string(frames.size()) + " frames)");
    return ok;
}

void test_extended_ffmpeg() {
    const std::string tmp = temp_dir() + "/fizmo_hevc_ext";
    for (const X265Case& c : k_extended_x265) {
        const std::string path = tmp + "_" + c.name + ".hevc";
        if (!x265_encode(c, path)) {
            check(false, std::string("x265 encode ") + c.name);
            continue;
        }
        compare_with_ffmpeg(path, c.name);
        std::remove(path.c_str());
    }
    const char* modes[] = {"basic", "dep", "wpp", "tiles", "tiles2", "pcm", "pcmnf", "misc", "mono", "rext444", "rext444b", "rext422"};
    for (const char* mode : modes) {
        for (unsigned seed = 21; seed <= 23; ++seed) {
            hevcgen::Config c = hevcgen::config_for(mode, seed);
            hevcgen::Generator g(c);
            std::vector<std::uint8_t> stream;
            std::string err;
            if (!g.run(stream, &err)) {
                check(false, std::string("generate ") + mode + " " + err);
                continue;
            }
            const std::string path = tmp + "_gen_" + mode + "_" + std::to_string(seed) + ".hevc";
            write_file(path, stream);
            compare_with_ffmpeg(path, std::string("generated ") + mode + " seed " + std::to_string(seed));
            std::remove(path.c_str());
        }
    }
}

int make_corpus(const std::string& dir) {
    if (!have_ffmpeg()) {
        std::printf("ffmpeg with libx265 is needed to build the corpus\n");
        return 1;
    }
    std::string manifest;
    auto add = [&](const std::string& file, const std::string& kind, const std::string& flags, bool fields) -> bool {
        const std::string path = dir + "/" + file;
        std::vector<std::uint8_t> data;
        if (!read_file(path, data)) return false;
        std::vector<VideoFrame> frames;
        std::string err;
        if (kind == "es") {
            decode_es(data, frames, nullptr, !fields, nullptr);
        } else {
            decode_demux(data, frames, &err, nullptr);
        }
        if (frames.empty()) {
            std::printf("%s: no frames\n", file.c_str());
            return false;
        }
        std::string md5;
        int n = 0;
        if (!ffmpeg_md5(path, md5, n, frame_bytes(frames[0]))) return false;
        if (md5 != frames_md5(frames) || n != static_cast<int>(frames.size())) std::printf("%s: decoder differs from ffmpeg\n", file.c_str());
        char line[512];
        std::snprintf(line, sizeof(line), "%s %s %d %u %u %s %s %s\n", file.c_str(), kind.c_str(), n, frames[0].width, frames[0].height, format_name(frames[0].format), md5.c_str(), flags.empty() ? "-" : flags.c_str());
        manifest += line;
        return true;
    };
    for (const X265Case& c : k_corpus_x265) {
        const std::string file = std::string(c.name) + ".hevc";
        if (!x265_encode(c, dir + "/" + file)) return 1;
        add(file, "es", "hash", false);
    }
    {
        X265Case sc = {"main_ctu16_sao_odd", "ctu=16:bframes=2:max-tu-size=8", "", "yuv420p", "98x66", 8};
        const std::string file = "main_ctu16_sao_odd.hevc";
        if (!x265_encode(sc, dir + "/" + file)) return 1;
        std::vector<std::uint8_t> data;
        read_file(dir + "/" + file, data);
        std::vector<VideoFrame> frames;
        HevcStats st;
        decode_es(data, frames, &st, true, nullptr);
        if (frames.empty() || st.hash_checked != frames.size() || st.hash_mismatches) return 1;
        char line[512];
        std::snprintf(line, sizeof(line), "%s es %zu %u %u %s %s hash,sei-reference\n", file.c_str(), frames.size(), frames[0].width, frames[0].height, format_name(frames[0].format), frames_md5(frames).c_str());
        manifest += line;
    }
    {
        std::vector<std::uint8_t> og;
        if (!read_file(dir + "/opengop.hevc", og)) return 1;
        int cra = 0;
        for (std::size_t i = 0; i + 3 < og.size(); ++i) {
            if (og[i] == 0 && og[i + 1] == 0 && og[i + 2] == 1) {
                if (((og[i + 3] >> 1) & 63) == 21 && ++cra == 2) og[i + 3] = static_cast<std::uint8_t>((16 << 1) | (og[i + 3] & 1));
                i += 2;
            }
        }
        write_file(dir + "/opengop_bla.hevc", og);
        add("opengop_bla.hevc", "es", "hash", false);
    }
    {
        const std::string cmd = "ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=96x48:rate=50 -frames:v 12 -pix_fmt yuv420p -c:v libx265 -x265-params \"log-level=error:hash=1:interlace=tff:bframes=2\" \"" + dir + "/fields_tff.hevc\"";
        if (std::system(cmd.c_str()) != 0) return 1;
        add("fields_tff.hevc", "es", "hash,fields", true);
    }
    const char* containers[][3] = {{"main_bpyramid", "mp4", "-tag:v hvc1"}, {"main_bpyramid", "mkv", ""}, {"opengop", "mp4", "-tag:v hev1"}, {"opengop", "mkv", ""}, {"main10", "mp4", "-tag:v hvc1"}};
    for (const auto& c : containers) {
        const X265Case* base = nullptr;
        for (const X265Case& x : k_corpus_x265) {
            if (std::string(x.name) == c[0]) base = &x;
        }
        if (!base) return 1;
        const std::string pre = std::string(base->pre) + " " + c[2];
        X265Case xc = *base;
        xc.pre = pre.c_str();
        const std::string out = std::string(c[0]) + "." + c[1];
        if (!x265_encode(xc, dir + "/" + out)) return 1;
        add(out, c[1], "", false);
    }
    const char* modes[] = {"basic", "dep", "wpp", "tiles", "tiles2", "pcm", "pcmnf", "misc", "mono", "rext444", "rext444b", "rext422"};
    for (const char* mode : modes) {
        hevcgen::Config cfg = hevcgen::config_for(mode, 1);
        hevcgen::Generator g(cfg);
        std::vector<std::uint8_t> stream;
        std::string err;
        if (!g.run(stream, &err)) {
            std::printf("generate %s: %s\n", mode, err.c_str());
            return 1;
        }
        const std::string file = std::string("gen_") + mode + ".hevc";
        write_file(dir + "/" + file, stream);
        add(file, "es", "gen", false);
    }
    write_file(dir + "/manifest.txt", std::vector<std::uint8_t>(manifest.begin(), manifest.end()));
    std::printf("corpus written to %s\n", dir.c_str());
    return 0;
}

int bench(const std::string& path) {
    std::vector<std::uint8_t> data;
    if (!read_file(path, data)) return 1;
    HevcFileReader r;
    if (!r.open(data.data(), data.size())) return 1;
    std::vector<std::vector<std::uint8_t>> aus;
    std::vector<std::uint8_t> au;
    while (r.read(au)) aus.push_back(au);
    HevcDecoder d;
    d.open_annexb();
    VideoFrame f;
    int frames = 0;
    const std::clock_t c0 = std::clock();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < aus.size(); ++i) {
        d.decode(aus[i].data(), aus[i].size(), static_cast<double>(i));
        while (d.receive(f)) ++frames;
    }
    d.flush();
    while (d.receive(f)) ++frames;
    const double cpu = static_cast<double>(std::clock() - c0) / CLOCKS_PER_SEC;
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%d frames %ux%u %s: cpu %.3f s (%.1f fps), wall %.3f s (%.1f fps)\n", frames, f.width, f.height, format_name(f.format), cpu, frames / cpu, wall, frames / wall);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--make-corpus") return make_corpus(argv[2]);
    if (argc >= 3 && std::string(argv[1]) == "--bench") return bench(argv[2]);
    const std::string dir = argc >= 2 ? std::string(argv[1]) : data_dir();
    const std::vector<ManifestEntry> man = load_manifest(dir);
    test_corpus(dir, man);
    test_fields(dir);
    test_rasl_counter(dir);
    test_seek(dir, "opengop.mp4");
    test_seek(dir, "opengop.mkv");
    test_seek(dir, "opengop.hevc");
    test_api(dir);
    test_player(dir);
    test_generated_consistency();
    test_fuzz(dir, man);
    if (have_ffmpeg()) {
        test_extended_ffmpeg();
    } else {
        std::printf("ffmpeg not found: skipping the extended comparisons\n");
    }
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
