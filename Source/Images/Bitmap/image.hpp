#ifndef BITMAP_IMAGE_HPP
#define BITMAP_IMAGE_HPP

#include <vector>
#include <fstream>
#include <stdexcept>

#include "../../Graphics/color.hpp"
#include "../../Graphics/fixed_color.hpp"
#include "../../Graphics/content_version.hpp"

namespace fizmo {
namespace images {

#pragma pack(push, 1)
struct BMPHeader {
    std::uint16_t signature = 0x4D42;     // 'BM'
    std::uint32_t fileSize;               // Size of the BMP file in bytes
    std::uint16_t reserved1 = 0;          // Reserved
    std::uint16_t reserved2 = 0;          // Reserved
    std::uint32_t dataOffset = 54;        // Offset to image data in bytes
    std::uint32_t headerSize = 40;        // Header size in bytes
    std::int32_t width;                   // Width of the image
    std::int32_t height;                  // Height of the image
    std::uint16_t planes = 1;             // Number of color planes
    std::uint16_t bitsPerPixel = 24;      // Bits per pixel
    std::uint32_t compression = 0;        // Compression type
    std::uint32_t imageSize;              // Image size in bytes
    std::int32_t xPixelsPerMeter = 0;     // Pixels per meter in x axis
    std::int32_t yPixelsPerMeter = 0;     // Pixels per meter in y axis
    std::uint32_t totalColors = 0;        // Number of colors
    std::uint32_t importantColors = 0;    // Important colors
};
#pragma pack(pop)

class BitmapImage {
private:
    std::vector<fizmo::graphics::Color> m_pixels;
    unsigned int m_width;
    unsigned int m_height;
    fizmo::ContentVersion m_version;     

private:
    unsigned int get_padding() const { return (4 - (m_width * 3) % 4) % 4; }

    bool is_valid_range(const unsigned int x, const unsigned int y) const {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) { return true; }
        return false;
    }

public:
    BitmapImage() : m_width(0), m_height(0) { m_pixels.resize(0); }

    BitmapImage(const unsigned int width, const unsigned int height);

    BitmapImage(const std::string& filename);

    void set_pixel(const unsigned int x, const unsigned int y, const fizmo::graphics::Color& color) { if (is_valid_range(x, y)) { m_pixels[y * m_width + x] = color; m_version.touch(); } }
    
    fizmo::graphics::Color get_pixel(const unsigned int x, const unsigned int y) const { 
        if (is_valid_range(x, y)) { return m_pixels[y * m_width + x]; } 
        return fizmo::graphics::Color(); 
    }

    unsigned int width() const { return m_width; }
    unsigned int height() const { return m_height; }
    const std::vector<fizmo::graphics::Color>& pixels() const { return m_pixels; }
    bool is_valid_image() const noexcept { return !m_pixels.empty() && m_width != 0 && m_height != 0; }
    double pixel_distance(const unsigned int x, const unsigned int y) const { return std::sqrt(std::pow(x - m_width / 2, 2) + std::pow(y - m_height / 2, 2)); }
    double pixel_distance(const unsigned int x1, const unsigned int y1, const unsigned int x2, const unsigned int y2) { return std::sqrt(std::pow(x1 - x2, 2) + std::pow(y1 - y2, 2)); }
    std::uint64_t version() const noexcept { return m_version.get(); }

public:
    void fill_area(const unsigned int x1, const unsigned int y1, const unsigned int x2, const unsigned int y2, const fizmo::graphics::Color& color);

    void change_background(const fizmo::graphics::Color& color);

    BitmapImage as_resized(const unsigned int new_width, const unsigned int new_height) const;

    void resize(const unsigned int new_width, const unsigned int new_height);

public:
    bool save_to_file(const std::string& filename_input);

    bool save_to_jpeg(const std::string& filename_input, int quality = 90);
    bool save_to_png(const std::string& filename_input);

public:
    double aspect_ratio() const noexcept { return static_cast<double>(m_width) / static_cast<double>(m_height); }
    double inverse_aspect() const noexcept { return 1.0 / aspect_ratio(); }
    double diagonal() const noexcept { return std::sqrt(m_width * m_width + m_height * m_height); }

    void change_width_stretch(const unsigned int new_width);
    
    void change_width_proportional(const unsigned int new_width);
    
    void change_height_stretch(const unsigned int new_height);
    
    void change_height_proportional(const unsigned int new_height);
};

} // namespace images
} // namespace fizmo

#endif // BITMAP_IMAGE_HPP