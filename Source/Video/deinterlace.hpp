#ifndef FIZMO_VIDEO_DEINTERLACE_HPP
#define FIZMO_VIDEO_DEINTERLACE_HPP

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace fizmo {
namespace video {

enum class DeinterlaceMode {
    weave,
    bob,
    linear,
    yadif,
    yadif_spatial
};

enum class DeinterlaceSetting {
    off,
    automatic,
    on
};

struct DeinterlaceOptions {
    DeinterlaceMode mode = DeinterlaceMode::yadif;
    bool field_rate = false;
    bool only_interlaced = true;
    int parity = -1;
};

const char* deinterlace_mode_name(DeinterlaceMode mode) noexcept;
const char* deinterlace_setting_name(DeinterlaceSetting setting) noexcept;

bool deinterlace_field(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, DeinterlaceMode mode, int keep_parity, bool tff);
bool deinterlace_bob(const VideoFrame& in, VideoFrame& out, bool second_field = false);
bool deinterlace_frame(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next, VideoFrame& out, DeinterlaceMode mode = DeinterlaceMode::yadif, bool second_field = false);

class Deinterlacer {
public:
    Deinterlacer() = default;
    explicit Deinterlacer(const DeinterlaceOptions& options) : m_options(options) {}

    void set_options(const DeinterlaceOptions& options);
    const DeinterlaceOptions& options() const noexcept { return m_options; }
    void push(const VideoFrame& frame);
    void push(VideoFrame&& frame);
    bool pull(VideoFrame& out);
    void flush();
    void reset();
    std::size_t pending() const noexcept { return m_out.size(); }
    std::uint64_t frames_deinterlaced() const noexcept { return m_count; }
    bool last_was_deinterlaced() const noexcept { return m_last_active; }

private:
    void process(const VideoFrame* prev, const VideoFrame& cur, const VideoFrame* next);
    bool wants(const VideoFrame& f) const noexcept;
    bool temporal() const noexcept;

    DeinterlaceOptions m_options;
    std::deque<VideoFrame> m_in;
    std::deque<VideoFrame> m_out;
    VideoFrame m_prev;
    bool m_has_prev = false;
    double m_last_delta = 0.0;
    std::uint64_t m_count = 0;
    bool m_last_active = false;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_DEINTERLACE_HPP
