#ifndef FIZMO_VIDEO_TEXTURE_HPP
#define FIZMO_VIDEO_TEXTURE_HPP

#include "texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace graphics {

enum class VideoMatrix : std::uint8_t {
    Unspecified = 0,
    BT601,
    BT709,
    BT2020,
    RGB
};

enum class VideoRange : std::uint8_t {
    Limited = 0,
    Full
};

struct VideoPlaneView {
    const void* data   = nullptr;
    std::size_t stride = 0;
};

struct VideoFrameView {
    std::uint32_t  width            = 0;
    std::uint32_t  height           = 0;
    int            bit_depth        = 8;
    int            bytes_per_sample = 0;
    int            plane_count      = 3;
    int            chroma_shift_x   = 1;
    int            chroma_shift_y   = 1;
    VideoMatrix    matrix           = VideoMatrix::Unspecified;
    VideoRange     range            = VideoRange::Limited;
    std::uint8_t   chroma_location  = 0;
    VideoPlaneView planes[3];

    int sample_bytes() const noexcept { return bytes_per_sample > 0 ? bytes_per_sample : (bit_depth > 8 ? 2 : 1); }
    std::uint32_t plane_width(int i) const noexcept;
    std::uint32_t plane_height(int i) const noexcept;
};

struct VideoConversion {
    int   mode            = 0;
    float y_offset        = 0.0f;
    float y_scale         = 1.0f;
    float c_offset        = 0.0f;
    float cr_to_r         = 0.0f;
    float cb_to_g         = 0.0f;
    float cr_to_g         = 0.0f;
    float cb_to_b         = 0.0f;
    float chroma_offset_x = 0.0f;
    float chroma_offset_y = 0.0f;
    float sample_scale    = 255.0f;
};

class VideoTexture {
private:
    std::vector<std::uint8_t>   m_data;
    std::array<std::size_t, 3>  m_offset{};
    std::array<std::size_t, 3>  m_stride{};
    std::uint32_t               m_width          = 0;
    std::uint32_t               m_height         = 0;
    int                         m_bit_depth      = 0;
    int                         m_bytes          = 0;
    int                         m_planes         = 0;
    int                         m_shift_x        = 0;
    int                         m_shift_y        = 0;
    VideoMatrix                 m_matrix         = VideoMatrix::Unspecified;
    VideoRange                  m_range          = VideoRange::Limited;
    std::uint8_t                m_chroma_location = 0;
    std::uint64_t               m_id             = 0;
    std::uint64_t               m_version        = 0;
    SampleFilter                m_filter         = SampleFilter::Bilinear;

    static std::uint64_t next_id() noexcept;

public:
    VideoTexture() noexcept;
    explicit VideoTexture(const VideoFrameView& frame, SampleFilter filter = SampleFilter::Bilinear);
    VideoTexture(const VideoTexture& other);
    VideoTexture& operator=(const VideoTexture& other);
    VideoTexture(VideoTexture&& other) noexcept;
    VideoTexture& operator=(VideoTexture&& other) noexcept;
    ~VideoTexture() = default;

    static bool supported_layout(int bit_depth, int bytes_per_sample, int plane_count, int chroma_shift_x, int chroma_shift_y) noexcept;
    static bool supported_layout(const VideoFrameView& frame) noexcept;

    bool update(const VideoFrameView& frame);
    void clear() noexcept;

    bool          valid()            const noexcept { return m_planes > 0 && m_width > 0 && m_height > 0; }
    std::uint32_t width()            const noexcept { return m_width; }
    std::uint32_t height()           const noexcept { return m_height; }
    int           bit_depth()        const noexcept { return m_bit_depth; }
    int           bytes_per_sample() const noexcept { return m_bytes; }
    int           plane_count()      const noexcept { return m_planes; }
    int           chroma_shift_x()   const noexcept { return m_shift_x; }
    int           chroma_shift_y()   const noexcept { return m_shift_y; }
    VideoMatrix   matrix()           const noexcept { return m_matrix; }
    VideoRange    range()            const noexcept { return m_range; }
    std::uint8_t  chroma_location()  const noexcept { return m_chroma_location; }
    std::uint64_t id()               const noexcept { return m_id; }
    std::uint64_t version()          const noexcept { return m_version; }
    SampleFilter  filter()           const noexcept { return m_filter; }
    void          set_filter(SampleFilter f) noexcept { m_filter = f; }
    TextureRect   full_rect()        const noexcept { return TextureRect(0, 0, m_width, m_height); }

    VideoMatrix effective_matrix() const noexcept;

    std::uint32_t       plane_width(int i)  const noexcept;
    std::uint32_t       plane_height(int i) const noexcept;
    std::size_t         plane_stride(int i) const noexcept { return i >= 0 && i < m_planes ? m_stride[static_cast<std::size_t>(i)] : 0; }
    const std::uint8_t* plane_data(int i)   const noexcept { return i >= 0 && i < m_planes ? m_data.data() + m_offset[static_cast<std::size_t>(i)] : nullptr; }
    std::uint32_t       plane_row_pixels(int i) const noexcept { return m_bytes > 0 ? static_cast<std::uint32_t>(plane_stride(i) / static_cast<std::size_t>(m_bytes)) : 0; }

    VideoConversion conversion() const noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_VIDEO_TEXTURE_HPP
