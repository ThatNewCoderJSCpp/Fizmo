#include "demux_internal.hpp"

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace video {
namespace demux_detail {

namespace {

std::uint32_t ivf_le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

const char* ivf_codec(const std::uint8_t* p, std::size_t size) noexcept {
    if (!p || size < 32 || std::memcmp(p, "DKIF", 4) != 0) return nullptr;
    if (std::memcmp(p + 8, "VP90", 4) == 0 || std::memcmp(p + 8, "vp90", 4) == 0) return "vp9";
    if (std::memcmp(p + 8, "VP80", 4) == 0 || std::memcmp(p + 8, "vp80", 4) == 0) return "vp8";
    return nullptr;
}

bool vp9_frame_is_key(const std::uint8_t* p, std::size_t n, bool super) noexcept {
    if (n == 0) return false;
    const std::uint8_t last = p[n - 1];
    if (super && (last & 0xE0) == 0xC0) {
        const std::size_t count = (last & 7u) + 1;
        const std::size_t bytes = ((last >> 3) & 3u) + 1;
        const std::size_t index = 2 + bytes * count;
        if (n >= index && p[n - index] == last) {
            std::size_t first = 0;
            for (std::size_t b = 0; b < bytes; ++b) first |= static_cast<std::size_t>(p[n - index + 1 + b]) << (8 * b);
            if (first == 0 || first > n - index) return false;
            n = first;
        }
    }
    std::size_t bit = 0;
    const std::size_t total = n * 8;
    auto get = [&](int count) -> std::uint32_t {
        std::uint32_t v = 0;
        for (int i = 0; i < count; ++i) {
            if (bit >= total) return 0xFFFFFFFFu;
            v = (v << 1) | ((p[bit >> 3] >> (7 - (bit & 7))) & 1u);
            ++bit;
        }
        return v;
    };
    if (get(2) != 2) return false;
    const std::uint32_t lo = get(1);
    const std::uint32_t hi = get(1);
    if (lo > 1 || hi > 1) return false;
    if (((hi << 1) | lo) == 3 && get(1) != 0) return false;
    if (get(1) != 0) return false;
    return get(1) == 0;
}

struct IvfEntry {
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    double pts = 0.0;
    bool key = false;
};

class IvfDemux : public Reader {
public:
    bool open(Source& source, std::string* error) override {
        m_src = &source;
        std::uint8_t head[32];
        if (!source.read_exact(0, head, sizeof(head))) {
            if (error) *error = "truncated IVF header";
            return false;
        }
        const char* codec = ivf_codec(head, sizeof(head));
        if (!codec) {
            if (error) *error = "not a VP8/VP9 IVF file";
            return false;
        }
        const bool vp9 = codec[2] == '9';
        const std::uint32_t header_size = head[6] | (static_cast<std::uint32_t>(head[7]) << 8);
        const std::uint32_t width = head[12] | (static_cast<std::uint32_t>(head[13]) << 8);
        const std::uint32_t height = head[14] | (static_cast<std::uint32_t>(head[15]) << 8);
        const std::uint32_t rate = ivf_le32(head + 16);
        const std::uint32_t scale = ivf_le32(head + 20);
        const double timebase = rate && scale ? static_cast<double>(scale) / static_cast<double>(rate) : 0.0;
        const std::uint64_t total = source.size();
        std::uint64_t pos = header_size < 32 ? 32 : header_size;
        std::vector<std::uint8_t> peek;
        std::uint64_t first_ts = 0;
        bool have_first = false;
        while (pos + 12 <= total) {
            std::uint8_t fh[12];
            if (!source.read_exact(pos, fh, sizeof(fh))) break;
            const std::uint32_t len = ivf_le32(fh);
            std::uint64_t ts = 0;
            for (int i = 0; i < 8; ++i) ts |= static_cast<std::uint64_t>(fh[4 + i]) << (8 * i);
            pos += 12;
            if (len == 0 || len > total - pos || len > k_max_packet) break;
            IvfEntry e;
            e.offset = pos;
            e.size = len;
            if (!have_first) {
                first_ts = ts;
                have_first = true;
            }
            e.pts = timebase > 0.0 ? static_cast<double>(ts >= first_ts ? ts - first_ts : 0) * timebase : -1.0;
            if (vp9) {
                std::uint8_t last = 0;
                const bool super = source.read_exact(pos + len - 1, &last, 1) && (last & 0xE0) == 0xC0;
                const std::size_t want = super ? static_cast<std::size_t>(len) : std::min<std::size_t>(len, 16);
                if (source.read_vector(pos, want, peek)) e.key = vp9_frame_is_key(peek.data(), peek.size(), super);
            } else {
                std::uint8_t tag = 0;
                e.key = source.read_exact(pos, &tag, 1) && (tag & 1u) == 0;
            }
            m_frames.push_back(e);
            pos += len;
        }
        if (m_frames.empty()) {
            if (error) *error = "IVF file has no frames";
            return false;
        }
        double fps = 0.0;
        if (m_frames.size() > 1 && m_frames.back().pts > m_frames.front().pts && m_frames.front().pts >= 0.0) fps = static_cast<double>(m_frames.size() - 1) / (m_frames.back().pts - m_frames.front().pts);
        if (!(fps > 0.1 && fps < 1000.0)) fps = timebase > 0.0 && 1.0 / timebase < 1000.0 ? 1.0 / timebase : 25.0;
        m_dur = 1.0 / fps;
        for (std::size_t i = 0; i < m_frames.size(); ++i) {
            if (m_frames[i].pts < 0.0) m_frames[i].pts = static_cast<double>(i) * m_dur;
        }
        TrackInfo t;
        t.id = 0;
        t.type = MediaType::video;
        t.codec = codec;
        t.width = width;
        t.height = height;
        t.display_width = width;
        t.display_height = height;
        t.frame_rate = fps;
        t.timescale = rate ? static_cast<double>(rate) : 90000.0;
        t.frame_count = m_frames.size();
        t.duration = m_frames.back().pts + m_dur;
        t.is_default = true;
        tracks.push_back(t);
        enabled.assign(1, 1);
        duration = t.duration;
        format = "ivf";
        m_next = 0;
        return true;
    }

    bool read(Packet& out) override {
        while (m_next < m_frames.size()) {
            const IvfEntry& e = m_frames[m_next++];
            if (!is_enabled(0)) continue;
            out.track = 0;
            if (!m_src->read_vector(e.offset, e.size, out.data)) return false;
            out.pts = e.pts;
            out.dts = e.pts;
            out.duration = m_dur;
            out.keyframe = e.key;
            out.position = e.offset;
            out.discard_padding = 0.0;
            return true;
        }
        return false;
    }

    bool seek(double seconds, bool before) override {
        if (m_frames.empty()) return false;
        std::size_t best = 0;
        bool found = false;
        for (std::size_t i = 0; i < m_frames.size(); ++i) {
            if (!m_frames[i].key) continue;
            const double t = m_frames[i].pts;
            if (before) {
                if (t <= seconds + 1e-6 || !found) {
                    best = i;
                    found = true;
                }
                if (t > seconds + 1e-6) break;
            } else if (t >= seconds - 1e-6) {
                best = i;
                found = true;
                break;
            }
        }
        if (!found) {
            if (!before) {
                m_next = m_frames.size();
                return true;
            }
            best = 0;
        }
        m_next = best;
        return true;
    }

private:
    Source* m_src = nullptr;
    std::vector<IvfEntry> m_frames;
    std::size_t m_next = 0;
    double m_dur = 0.04;
};

} // namespace

bool is_ivf_vpx(const void* data, std::size_t size) noexcept {
    return ivf_codec(static_cast<const std::uint8_t*>(data), size) != nullptr;
}

std::unique_ptr<Reader> make_ivf_reader() {
    return std::unique_ptr<Reader>(new IvfDemux());
}

} // namespace demux_detail
} // namespace video
} // namespace fizmo
