#ifndef FIZMO_VIDEO_FRAME_HPP
#define FIZMO_VIDEO_FRAME_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace video {

enum class PixelFormat {
    none,
    gray,
    yuv420p,
    yuv422p,
    yuv444p,
    gray10,
    yuv420p10,
    yuv422p10,
    yuv444p10,
    gray12,
    yuv420p12,
    yuv422p12,
    yuv444p12,
    gray14,
    yuv420p14,
    yuv422p14,
    yuv444p14,
    yuv440p,
    yuv440p10,
    yuv440p12
};

enum class PictureStructure {
    frame,
    mbaff_frame,
    field_pair,
    top_field,
    bottom_field
};

enum class ColorMatrix {
    unspecified,
    bt601,
    bt709,
    bt2020,
    rgb
};

enum class ColorRange {
    limited,
    full
};

struct ColorInfo {
    ColorMatrix matrix = ColorMatrix::unspecified;
    ColorRange range = ColorRange::limited;
    std::uint8_t primaries = 2;
    std::uint8_t transfer = 2;
    std::uint8_t matrix_coefficients = 2;
    std::uint8_t chroma_location = 0;
};

int bit_depth(PixelFormat format) noexcept;
int plane_count(PixelFormat format) noexcept;
int bytes_per_sample(PixelFormat format) noexcept;
int chroma_shift_x(PixelFormat format) noexcept;
int chroma_shift_y(PixelFormat format) noexcept;
const char* format_name(PixelFormat format) noexcept;
int chroma_format_of(PixelFormat format) noexcept;
int max_sample_value(PixelFormat format) noexcept;
PixelFormat pixel_format_for(int chroma_format_idc, int bit_depth) noexcept;
const char* picture_structure_name(PictureStructure structure) noexcept;

struct VideoFrame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::none;
    ColorInfo color;
    std::uint32_t sar_num = 1;
    std::uint32_t sar_den = 1;
    double pts = 0.0;
    bool keyframe = false;
    std::vector<std::uint8_t> data;
    std::size_t offset[3] = {0, 0, 0};
    std::size_t stride[3] = {0, 0, 0};
    bool interlaced = false;
    bool top_field_first = true;
    PictureStructure structure = PictureStructure::frame;
    int repeat_fields = 0;

    bool allocate(std::uint32_t w, std::uint32_t h, PixelFormat f);
    void clear();
    bool empty() const noexcept { return data.empty(); }
    std::uint8_t* plane(int i) noexcept { return data.data() + offset[i]; }
    const std::uint8_t* plane(int i) const noexcept { return data.data() + offset[i]; }
    std::uint32_t plane_width(int i) const noexcept;
    std::uint32_t plane_height(int i) const noexcept;
    double display_aspect() const noexcept;
};

void to_rgba(const VideoFrame& frame, std::uint8_t* rgba, std::size_t stride);
std::vector<std::uint8_t> to_rgba(const VideoFrame& frame);

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_FRAME_HPP
