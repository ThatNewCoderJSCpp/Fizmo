#include "fizmo_library.hpp"
#include "video_texture.hpp"

#include <atomic>
#include <cmath>
#include <cstring>
#include <utility>

namespace fizmo {
namespace graphics {

namespace {

std::uint32_t shifted_size(std::uint32_t size, int shift) noexcept {
    if (shift <= 0) return size;
    const std::uint32_t step = 1u << static_cast<unsigned>(shift);
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(size) + step - 1) >> static_cast<unsigned>(shift));
}

std::size_t align16(std::size_t v) noexcept {
    return (v + 15) & ~static_cast<std::size_t>(15);
}

} // namespace

std::uint32_t VideoFrameView::plane_width(int i) const noexcept {
    return i == 0 ? width : shifted_size(width, chroma_shift_x);
}

std::uint32_t VideoFrameView::plane_height(int i) const noexcept {
    return i == 0 ? height : shifted_size(height, chroma_shift_y);
}

std::uint64_t VideoTexture::next_id() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

VideoTexture::VideoTexture() noexcept : m_id(next_id()) {}

VideoTexture::VideoTexture(const VideoFrameView& frame, SampleFilter filter) : m_id(next_id()), m_filter(filter) {
    update(frame);
}

VideoTexture::VideoTexture(const VideoTexture& other)
    : m_data(other.m_data), m_offset(other.m_offset), m_stride(other.m_stride), m_width(other.m_width), m_height(other.m_height),
      m_bit_depth(other.m_bit_depth), m_bytes(other.m_bytes), m_planes(other.m_planes), m_shift_x(other.m_shift_x), m_shift_y(other.m_shift_y),
      m_matrix(other.m_matrix), m_range(other.m_range), m_chroma_location(other.m_chroma_location), m_id(next_id()),
      m_version(other.m_version), m_filter(other.m_filter) {}

VideoTexture& VideoTexture::operator=(const VideoTexture& other) {
    if (this == &other) return *this;
    m_data = other.m_data;
    m_offset = other.m_offset;
    m_stride = other.m_stride;
    m_width = other.m_width;
    m_height = other.m_height;
    m_bit_depth = other.m_bit_depth;
    m_bytes = other.m_bytes;
    m_planes = other.m_planes;
    m_shift_x = other.m_shift_x;
    m_shift_y = other.m_shift_y;
    m_matrix = other.m_matrix;
    m_range = other.m_range;
    m_chroma_location = other.m_chroma_location;
    m_filter = other.m_filter;
    ++m_version;
    return *this;
}

VideoTexture::VideoTexture(VideoTexture&& other) noexcept
    : m_data(std::move(other.m_data)), m_offset(other.m_offset), m_stride(other.m_stride), m_width(other.m_width), m_height(other.m_height),
      m_bit_depth(other.m_bit_depth), m_bytes(other.m_bytes), m_planes(other.m_planes), m_shift_x(other.m_shift_x), m_shift_y(other.m_shift_y),
      m_matrix(other.m_matrix), m_range(other.m_range), m_chroma_location(other.m_chroma_location), m_id(other.m_id),
      m_version(other.m_version), m_filter(other.m_filter) {
    other.m_id = next_id();
    other.clear();
}

VideoTexture& VideoTexture::operator=(VideoTexture&& other) noexcept {
    if (this == &other) return *this;
    m_data = std::move(other.m_data);
    m_offset = other.m_offset;
    m_stride = other.m_stride;
    m_width = other.m_width;
    m_height = other.m_height;
    m_bit_depth = other.m_bit_depth;
    m_bytes = other.m_bytes;
    m_planes = other.m_planes;
    m_shift_x = other.m_shift_x;
    m_shift_y = other.m_shift_y;
    m_matrix = other.m_matrix;
    m_range = other.m_range;
    m_chroma_location = other.m_chroma_location;
    m_id = other.m_id;
    m_version = other.m_version;
    m_filter = other.m_filter;
    other.m_id = next_id();
    other.clear();
    return *this;
}

bool VideoTexture::supported_layout(int bit_depth, int bytes_per_sample, int plane_count, int chroma_shift_x, int chroma_shift_y) noexcept {
    if (bit_depth < 8 || bit_depth > 16) return false;
    if (bytes_per_sample != (bit_depth > 8 ? 2 : 1)) return false;
    if (plane_count != 1 && plane_count != 3) return false;
    if (chroma_shift_x < 0 || chroma_shift_x > 2 || chroma_shift_y < 0 || chroma_shift_y > 2) return false;
    return true;
}

bool VideoTexture::supported_layout(const VideoFrameView& frame) noexcept {
    return supported_layout(frame.bit_depth, frame.sample_bytes(), frame.plane_count, frame.chroma_shift_x, frame.chroma_shift_y);
}

bool VideoTexture::update(const VideoFrameView& frame) {
    if (!supported_layout(frame)) return false;
    if (frame.width == 0 || frame.height == 0 || frame.width > 65536 || frame.height > 65536) return false;
    const int planes = frame.plane_count;
    const std::size_t bps = static_cast<std::size_t>(frame.sample_bytes());
    std::array<std::size_t, 3> offset{};
    std::array<std::size_t, 3> stride{};
    std::array<std::size_t, 3> source{};
    std::size_t total = 0;

    for (int i = 0; i < planes; ++i) {
        const VideoPlaneView& p = frame.planes[i];
        if (!p.data) return false;
        const std::size_t row = static_cast<std::size_t>(frame.plane_width(i)) * bps;
        const std::size_t given = p.stride ? p.stride : row;
        if (given < row) return false;
        const std::size_t k = static_cast<std::size_t>(i);
        source[k] = given;
        stride[k] = given % bps == 0 && given - row <= 1024 ? given : row;
        offset[k] = total;
        total = align16(total + stride[k] * frame.plane_height(i));
    }

    m_data.resize(total);

    for (int i = 0; i < planes; ++i) {
        const std::size_t k = static_cast<std::size_t>(i);
        const std::size_t row = static_cast<std::size_t>(frame.plane_width(i)) * bps;
        const std::size_t h = frame.plane_height(i);
        const std::uint8_t* src = static_cast<const std::uint8_t*>(frame.planes[i].data);
        std::uint8_t* dst = m_data.data() + offset[k];

        if (stride[k] == source[k]) {
            std::memcpy(dst, src, stride[k] * (h - 1) + row);
        } else {
            for (std::size_t y = 0; y < h; ++y) std::memcpy(dst + y * stride[k], src + y * source[k], row);
        }
    }

    m_offset = offset;
    m_stride = stride;
    m_width = frame.width;
    m_height = frame.height;
    m_bit_depth = frame.bit_depth;
    m_bytes = static_cast<int>(bps);
    m_planes = planes;
    m_shift_x = planes > 1 ? frame.chroma_shift_x : 0;
    m_shift_y = planes > 1 ? frame.chroma_shift_y : 0;
    m_matrix = frame.matrix;
    m_range = frame.range;
    m_chroma_location = frame.chroma_location;
    ++m_version;
    return true;
}

void VideoTexture::clear() noexcept {
    m_data.clear();
    m_offset = {};
    m_stride = {};
    m_width = m_height = 0;
    m_bit_depth = m_bytes = m_planes = 0;
    m_shift_x = m_shift_y = 0;
    ++m_version;
}

VideoMatrix VideoTexture::effective_matrix() const noexcept {
    if (m_matrix != VideoMatrix::Unspecified) return m_matrix;
    return (m_height > 576 || m_width > 1024) ? VideoMatrix::BT709 : VideoMatrix::BT601;
}

std::uint32_t VideoTexture::plane_width(int i) const noexcept {
    if (i < 0 || i >= m_planes) return 0;
    return i == 0 ? m_width : shifted_size(m_width, m_shift_x);
}

std::uint32_t VideoTexture::plane_height(int i) const noexcept {
    if (i < 0 || i >= m_planes) return 0;
    return i == 0 ? m_height : shifted_size(m_height, m_shift_y);
}

VideoConversion VideoTexture::conversion() const noexcept {
    VideoConversion c;
    if (!valid()) return c;
    const double unit = std::ldexp(1.0, m_bit_depth - 8);
    const double max_value = std::ldexp(1.0, m_bit_depth) - 1.0;
    double y_range = 219.0 * unit;
    double c_range = 224.0 * unit;
    double y_off = 16.0 * unit;

    if (m_range == VideoRange::Full) {
        y_off = 0.0;
        y_range = max_value;
        c_range = max_value;
    }

    c.y_offset = static_cast<float>(y_off);
    c.y_scale = static_cast<float>(1.0 / y_range);
    c.c_offset = static_cast<float>(128.0 * unit);
    c.sample_scale = m_bytes > 1 ? 65535.0f : 255.0f;
    const VideoMatrix m = effective_matrix();

    if (m_planes == 1) {
        c.mode = 0;
    } else if (m == VideoMatrix::RGB) {
        c.mode = 2;
    } else {
        c.mode = 1;
        double kr = 0.299;
        double kb = 0.114;

        if (m == VideoMatrix::BT709) {
            kr = 0.2126;
            kb = 0.0722;
        } else if (m == VideoMatrix::BT2020) {
            kr = 0.2627;
            kb = 0.0593;
        }

        const double kg = 1.0 - kr - kb;
        c.cr_to_r = static_cast<float>(2.0 * (1.0 - kr) / c_range);
        c.cb_to_b = static_cast<float>(2.0 * (1.0 - kb) / c_range);
        c.cb_to_g = static_cast<float>(2.0 * kb * (1.0 - kb) / kg / c_range);
        c.cr_to_g = static_cast<float>(2.0 * kr * (1.0 - kr) / kg / c_range);
    }

    if (m_planes > 1) {
        const int loc = m_chroma_location;
        const bool h_cosited = loc == 0 || loc == 2 || loc == 4;
        const bool v_cosited = loc == 2 || loc == 3;
        if (m_shift_x > 0 && !h_cosited) c.chroma_offset_x = 0.5f * (std::ldexp(1.0f, -m_shift_x) - 1.0f);
        if (m_shift_y > 0 && !v_cosited) c.chroma_offset_y = 0.5f * (std::ldexp(1.0f, -m_shift_y) - 1.0f);
    }

    return c;
}

} // namespace graphics
} // namespace fizmo
