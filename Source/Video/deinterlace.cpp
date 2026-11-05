#include "deinterlace.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace fizmo {
namespace video {

namespace {

bool same_layout(const VideoFrame& a, const VideoFrame& b) noexcept {
    return !a.empty() && !b.empty() && a.width == b.width && a.height == b.height && a.format == b.format;
}

inline int iabs(int v) noexcept {
    return v < 0 ? -v : v;
}

inline int max3(int a, int b, int c) noexcept {
    return std::max(a, std::max(b, c));
}

inline int min3(int a, int b, int c) noexcept {
    return std::min(a, std::min(b, c));
}

template <typename T>
const T* row_of(const VideoFrame& f, int plane, int y) noexcept {
    return reinterpret_cast<const T*>(f.plane(plane) + f.stride[plane] * static_cast<std::size_t>(y));
}

template <typename T>
void interpolate_plane(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, int plane, DeinterlaceMode mode, int keep, bool first_in_time) {
    const int w = static_cast<int>(cur.plane_width(plane));
    const int h = static_cast<int>(cur.plane_height(plane));
    const VideoFrame& pv = prev ? *prev : cur;
    const VideoFrame& nx = next ? *next : cur;
    const VideoFrame& p2f = first_in_time ? pv : cur;
    const VideoFrame& n2f = first_in_time ? cur : nx;
    if (h < 2) return;
    auto cx = [w](int x) { return x < 0 ? 0 : (x >= w ? w - 1 : x); };
    for (int y = 0; y < h; ++y) {
        if ((y & 1) == keep) continue;
        int ya = y - 1;
        int yb = y + 1;
        if (ya < 0) ya = yb;
        if (yb >= h) yb = ya;
        if (ya < 0 || ya >= h) continue;
        const T* up = row_of<T>(cur, plane, ya);
        const T* dn = row_of<T>(cur, plane, yb);
        T* dst = reinterpret_cast<T*>(out.plane(plane) + out.stride[plane] * static_cast<std::size_t>(y));
        if (mode == DeinterlaceMode::bob) {
            const T* src = (y - 1 >= 0) ? up : dn;
            std::memcpy(dst, src, static_cast<std::size_t>(w) * sizeof(T));
            continue;
        }
        if (mode == DeinterlaceMode::linear) {
            for (int x = 0; x < w; ++x) dst[x] = static_cast<T>((static_cast<int>(up[x]) + dn[x] + 1) >> 1);
            continue;
        }
        const T* p2 = row_of<T>(p2f, plane, y);
        const T* n2 = row_of<T>(n2f, plane, y);
        const T* pu = row_of<T>(pv, plane, ya);
        const T* pd = row_of<T>(pv, plane, yb);
        const T* nu = row_of<T>(nx, plane, ya);
        const T* nd = row_of<T>(nx, plane, yb);
        bool check = y - 2 >= 0 && y + 2 < h;
        const T* p2u = check ? row_of<T>(p2f, plane, y - 2) : nullptr;
        const T* n2u = check ? row_of<T>(n2f, plane, y - 2) : nullptr;
        const T* p2d = check ? row_of<T>(p2f, plane, y + 2) : nullptr;
        const T* n2d = check ? row_of<T>(n2f, plane, y + 2) : nullptr;
        for (int x = 0; x < w; ++x) {
            int c = up[x];
            int e = dn[x];
            int spatial_pred = (c + e) >> 1;
            int spatial_score = iabs(up[cx(x - 1)] - dn[cx(x - 1)]) + iabs(c - e) + iabs(up[cx(x + 1)] - dn[cx(x + 1)]) - 1;
            for (int j = -1; j >= -2; --j) {
                int score = iabs(up[cx(x - 1 + j)] - dn[cx(x - 1 - j)]) + iabs(up[cx(x + j)] - dn[cx(x - j)]) + iabs(up[cx(x + 1 + j)] - dn[cx(x + 1 - j)]);
                if (score >= spatial_score) break;
                spatial_score = score;
                spatial_pred = (up[cx(x + j)] + dn[cx(x - j)]) >> 1;
            }
            for (int j = 1; j <= 2; ++j) {
                int score = iabs(up[cx(x - 1 + j)] - dn[cx(x - 1 - j)]) + iabs(up[cx(x + j)] - dn[cx(x - j)]) + iabs(up[cx(x + 1 + j)] - dn[cx(x + 1 - j)]);
                if (score >= spatial_score) break;
                spatial_score = score;
                spatial_pred = (up[cx(x + j)] + dn[cx(x - j)]) >> 1;
            }
            if (mode == DeinterlaceMode::yadif_spatial) {
                dst[x] = static_cast<T>(spatial_pred);
                continue;
            }
            int d = (static_cast<int>(p2[x]) + n2[x]) >> 1;
            int td0 = iabs(static_cast<int>(p2[x]) - n2[x]);
            int td1 = (iabs(static_cast<int>(pu[x]) - c) + iabs(static_cast<int>(pd[x]) - e)) >> 1;
            int td2 = (iabs(static_cast<int>(nu[x]) - c) + iabs(static_cast<int>(nd[x]) - e)) >> 1;
            int diff = max3(td0 >> 1, td1, td2);
            if (check) {
                int b = (static_cast<int>(p2u[x]) + n2u[x]) >> 1;
                int f = (static_cast<int>(p2d[x]) + n2d[x]) >> 1;
                int mx = max3(d - e, d - c, std::min(b - c, f - e));
                int mn = min3(d - e, d - c, std::max(b - c, f - e));
                diff = max3(diff, mn, -mx);
            }
            int pred = spatial_pred;
            if (pred > d + diff) pred = d + diff;
            if (pred < d - diff) pred = d - diff;
            dst[x] = static_cast<T>(pred);
        }
    }
}

template <typename T>
void interpolate_all(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, DeinterlaceMode mode, int keep, bool first_in_time) {
    int planes = plane_count(cur.format);
    for (int p = 0; p < planes; ++p) interpolate_plane<T>(prev, cur, next, out, p, mode, keep, first_in_time);
}

} // namespace

const char* deinterlace_mode_name(DeinterlaceMode mode) noexcept {
    switch (mode) {
    case DeinterlaceMode::weave: return "weave";
    case DeinterlaceMode::bob: return "bob";
    case DeinterlaceMode::linear: return "linear";
    case DeinterlaceMode::yadif: return "yadif";
    case DeinterlaceMode::yadif_spatial: return "yadif_spatial";
    default: return "unknown";
    }
}

const char* deinterlace_setting_name(DeinterlaceSetting setting) noexcept {
    switch (setting) {
    case DeinterlaceSetting::off: return "off";
    case DeinterlaceSetting::automatic: return "auto";
    case DeinterlaceSetting::on: return "on";
    default: return "unknown";
    }
}

bool deinterlace_field(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, DeinterlaceMode mode, int keep_parity, bool tff) {
    if (cur.empty() || cur.width == 0 || cur.height == 0 || cur.format == PixelFormat::none) return false;
    if (prev && !same_layout(*prev, cur)) prev = nullptr;
    if (next && !same_layout(*next, cur)) next = nullptr;
    if (&out != &cur) out = cur;
    out.interlaced = false;
    out.structure = PictureStructure::frame;
    out.repeat_fields = 0;
    if (mode == DeinterlaceMode::weave) return true;
    int keep = keep_parity ? 1 : 0;
    bool first_in_time = keep == (tff ? 0 : 1);
    if (bytes_per_sample(cur.format) == 2) {
        interpolate_all<std::uint16_t>(prev, cur, next, out, mode, keep, first_in_time);
    } else {
        interpolate_all<std::uint8_t>(prev, cur, next, out, mode, keep, first_in_time);
    }
    return true;
}

bool deinterlace_bob(const VideoFrame& in, VideoFrame& out, bool second_field) {
    bool tff = in.top_field_first;
    int keep = second_field ? (tff ? 1 : 0) : (tff ? 0 : 1);
    if (&out == &in) {
        VideoFrame copy = in;
        return deinterlace_field(nullptr, copy, nullptr, out, DeinterlaceMode::bob, keep, tff);
    }
    return deinterlace_field(nullptr, in, nullptr, out, DeinterlaceMode::bob, keep, tff);
}

bool deinterlace_frame(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, DeinterlaceMode mode, bool second_field) {
    bool tff = cur.top_field_first;
    int keep = second_field ? (tff ? 1 : 0) : (tff ? 0 : 1);
    if (&out == &cur) {
        VideoFrame copy = cur;
        return deinterlace_field(prev, copy, next, out, mode, keep, tff);
    }
    return deinterlace_field(prev, cur, next, out, mode, keep, tff);
}

void Deinterlacer::set_options(const DeinterlaceOptions& options) {
    bool was = temporal();
    m_options = options;
    if (was && !temporal()) flush();
}

bool Deinterlacer::wants(const VideoFrame& f) const noexcept {
    if (m_options.mode == DeinterlaceMode::weave) return false;
    return !m_options.only_interlaced || f.interlaced;
}

bool Deinterlacer::temporal() const noexcept {
    return m_options.mode == DeinterlaceMode::yadif;
}

void Deinterlacer::process(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next) {
    if (!wants(cur)) {
        m_out.push_back(cur);
        m_last_active = false;
        return;
    }
    bool tff = m_options.parity < 0 ? cur.top_field_first : m_options.parity == 0;
    int first = tff ? 0 : 1;
    VideoFrame a;
    if (!deinterlace_field(prev, cur, next, a, m_options.mode, first, tff)) {
        m_out.push_back(cur);
        m_last_active = false;
        return;
    }
    double delta = 0.0;
    if (next && next->pts > cur.pts) delta = next->pts - cur.pts;
    if (delta <= 0.0 && prev && cur.pts > prev->pts) delta = cur.pts - prev->pts;
    if (delta <= 0.0) delta = m_last_delta > 0.0 ? m_last_delta : 0.04;
    m_last_delta = delta;
    m_out.push_back(std::move(a));
    ++m_count;
    m_last_active = true;
    if (m_options.field_rate) {
        VideoFrame b;
        if (deinterlace_field(prev, cur, next, b, m_options.mode, 1 - first, tff)) {
            b.pts = cur.pts + delta * 0.5;
            b.keyframe = false;
            m_out.push_back(std::move(b));
        }
    }
}

void Deinterlacer::push(const VideoFrame& frame) {
    VideoFrame copy = frame;
    push(std::move(copy));
}

void Deinterlacer::push(VideoFrame&& frame) {
    if (!temporal()) {
        process(nullptr, frame, nullptr);
        return;
    }
    if (!wants(frame)) {
        while (!m_in.empty()) {
            const VideoFrame* next = m_in.size() > 1 ? &m_in[1] : &frame;
            process(m_has_prev ? &m_prev : nullptr, m_in[0], next);
            m_prev = std::move(m_in.front());
            m_has_prev = true;
            m_in.pop_front();
        }
        m_out.push_back(frame);
        m_last_active = false;
        m_prev = std::move(frame);
        m_has_prev = true;
        return;
    }
    m_in.push_back(std::move(frame));
    while (m_in.size() >= 2) {
        const VideoFrame& cur = m_in[0];
        const VideoFrame& next = m_in[1];
        process(m_has_prev ? &m_prev : nullptr, cur, &next);
        m_prev = std::move(m_in.front());
        m_has_prev = true;
        m_in.pop_front();
    }
}

bool Deinterlacer::pull(VideoFrame& out) {
    if (m_out.empty()) return false;
    out = std::move(m_out.front());
    m_out.pop_front();
    return true;
}

void Deinterlacer::flush() {
    while (!m_in.empty()) {
        const VideoFrame& cur = m_in[0];
        const VideoFrame* next = m_in.size() > 1 ? &m_in[1] : nullptr;
        process(m_has_prev ? &m_prev : nullptr, cur, next);
        m_prev = std::move(m_in.front());
        m_has_prev = true;
        m_in.pop_front();
    }
    m_has_prev = false;
    m_prev.clear();
}

void Deinterlacer::reset() {
    m_in.clear();
    m_out.clear();
    m_prev.clear();
    m_has_prev = false;
    m_last_delta = 0.0;
    m_last_active = false;
}

} // namespace video
} // namespace fizmo
