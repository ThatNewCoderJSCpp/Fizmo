#ifndef FIZMO_VIDEO_H264_HPP
#define FIZMO_VIDEO_H264_HPP

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {

struct H264Info {
    int profile_idc = 0;
    int level_idc = 0;
    int chroma_format_idc = 1;
    int bit_depth = 8;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t coded_width = 0;
    std::uint32_t coded_height = 0;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
    bool progressive = true;
    bool cabac = false;
    int max_num_reorder_frames = -1;
    int dpb_frames = 0;
    double frame_rate = 0.0;
    ColorInfo color;
    int bit_depth_chroma = 8;
    bool separate_colour_plane = false;
    bool lossless = false;
    bool mbaff = false;
    bool frame_mbs_only = true;
    PixelFormat format = PixelFormat::none;
};

bool is_annexb_h264(const void* data, std::size_t size) noexcept;

class H264Decoder {
public:
    struct State;

    H264Decoder();
    ~H264Decoder();
    H264Decoder(H264Decoder&&) noexcept;
    H264Decoder& operator=(H264Decoder&&) noexcept;
    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    bool open(const std::uint8_t* avcc, std::size_t size, std::string* error = nullptr);
    bool open_annexb();
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error = nullptr);
    bool receive(VideoFrame& out);
    void flush();
    void reset();
    void close();
    bool is_open() const noexcept;

    const H264Info& info() const noexcept;
    std::uint64_t frames_decoded() const noexcept;
    std::uint64_t error_count() const noexcept;

private:
    std::unique_ptr<State> m_state;
};

class H264FileReader {
public:
    H264FileReader() = default;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size);
    void close();
    bool is_open() const noexcept { return m_open; }
    bool read(std::vector<std::uint8_t>& access_unit);
    void rewind() noexcept { m_pos = m_first; }

private:
    std::size_t next_start(std::size_t from, std::size_t* code_size) const noexcept;

    std::vector<std::uint8_t> m_data;
    std::size_t m_pos = 0;
    std::size_t m_first = 0;
    bool m_open = false;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_H264_HPP
