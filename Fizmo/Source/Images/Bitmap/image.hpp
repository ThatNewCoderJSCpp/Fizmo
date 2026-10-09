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

    BitmapImage(const unsigned int width, const unsigned int height) : m_width(width), m_height(height) {
        if (m_width == 0 || m_width == 0) { throw std::runtime_error("Invalid image dimensions (must be positive)"); }
        m_pixels.resize(width * height, fizmo::graphics::Color(255, 255, 255)); // White background
    }

    BitmapImage(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) { throw std::runtime_error("Failed to open bitmap file"); }
        BMPHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.signature != 0x4D42) { // "BM" in hex
            throw std::runtime_error("Invalid BMP file format");
        }

        if (header.bitsPerPixel != 24) { throw std::runtime_error("Only 24-bit BMP files are supported"); }
        if (header.compression != 0) { throw std::runtime_error("Compressed BMP files are not supported"); }
        m_width = header.width;
        m_height = header.height;
        if (m_width == 0 || m_height == 0) { throw std::runtime_error("Invalid image dimensions (one or both is equal to 0)"); }
        m_pixels.resize(m_width * m_height);
        unsigned int padding = (4 - (m_width * 3) % 4) % 4;
        file.seekg(header.dataOffset, std::ios::beg);
        std::vector<unsigned char> row_buffer(m_width * 3 + padding);
        
        for (int y = m_height - 1; y >= 0; y--) {
            file.read(reinterpret_cast<char*>(row_buffer.data()), row_buffer.size());
            
            for (unsigned int x = 0; x < m_width; x++) {
                const unsigned int buffer_pos = x * 3; // Each pixel uses 3 bytes
                const unsigned char blue = row_buffer[buffer_pos];
                const unsigned char green = row_buffer[buffer_pos + 1];
                const unsigned char red = row_buffer[buffer_pos + 2];
                m_pixels[y * m_width + x] = fizmo::graphics::Color(red, green, blue);
            }
        }

        if (!file.good()) { throw std::runtime_error("Error occurred while reading BMP file"); }
    }

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
    void fill_area(const unsigned int x1, const unsigned int y1, const unsigned int x2, const unsigned int y2, const fizmo::graphics::Color& color) {
        m_version.touch();
        if (is_valid_range(x1, y1) && is_valid_range(x2, y2)) {
            for (unsigned int y = y1; y <= y2; ++y) {
                for (unsigned int x = x1; x <= x2; ++x) { m_pixels[y * m_width + x] = color; }
            }
        }
    }

    void change_background(const fizmo::graphics::Color& color) {
        m_version.touch();
        for (unsigned int i = 0; i < m_width; ++i) {
            for (unsigned int j = 0; j < m_height; ++j) { m_pixels[j * m_width + i] = color; }
        }
    }

    BitmapImage as_resized(const unsigned int new_width, const unsigned int new_height) const {
        if (new_width == 0 || new_height == 0) {
            throw std::runtime_error("Invalid new dimensions for resizing: width=" + std::to_string(new_width) + ", height=" + std::to_string(new_height));
        }
        BitmapImage resized(new_width, new_height);
        
        const double x_ratio = static_cast<double>(m_width) / new_width;
        const double y_ratio = static_cast<double>(m_height) / new_height;
        
        for (unsigned int y = 0; y < new_height; y++) {
            for (unsigned int x = 0; x < new_width; x++) {
                unsigned int src_x = static_cast<unsigned int>(x * x_ratio);
                unsigned int src_y = static_cast<unsigned int>(y * y_ratio);
                src_x = std::min(src_x, m_width - 1);
                src_y = std::min(src_y, m_height - 1);            
                resized.set_pixel(x, y, get_pixel(src_x, src_y));
            }
        }
        
        return resized;
    }

    void resize(const unsigned int new_width, const unsigned int new_height) {
        BitmapImage resized = as_resized(new_width, new_height);
        m_width = new_width;
        m_height = new_height;
        m_pixels = std::move(resized.m_pixels);
    }

public:
    bool save_to_file(const std::string& filename_input) {
        std::string filename = filename_input;
        std::string extension = ".bmp";
        bool has_extension = false;
        
        if (filename.length() >= extension.length()) {
            std::string file_ending = filename.substr(filename.length() - extension.length());
            std::transform(file_ending.begin(), file_ending.end(), file_ending.begin(), [](unsigned char c) { return std::tolower(c); });
            has_extension = (file_ending == extension);
        }
        
        if (!has_extension) { filename += extension; }
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        BMPHeader header;
        unsigned int padding = get_padding();
        header.imageSize = (m_width * 3 + padding) * m_height;
        header.fileSize = header.dataOffset + header.imageSize;
        header.width = m_width;
        header.height = m_height;
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        std::vector<unsigned char> padding_bytes(padding, 0);

        for (unsigned int y = m_height - 1; y < m_height; y--) { 
            for (unsigned int x = 0; x < m_width; x++) {
                const fizmo::graphics::Color& pixel = get_pixel(x, y);

                unsigned char color[3] = {
                    static_cast<unsigned char>(pixel.blue()),
                    static_cast<unsigned char>(pixel.green()),
                    static_cast<unsigned char>(pixel.red())
                };

                file.write(reinterpret_cast<const char*>(color), 3);
            }

            if (padding > 0) { 
                file.write(reinterpret_cast<const char*>(padding_bytes.data()), padding); 
            }
        }

        return file.good();
    }

    bool save_to_jpeg(const std::string& filename_input, int quality = 90);
    bool save_to_png(const std::string& filename_input);

public:
    double aspect_ratio() const noexcept { return static_cast<double>(m_width) / static_cast<double>(m_height); }
    double inverse_aspect() const noexcept { return 1.0 / aspect_ratio(); }
    double diagonal() const noexcept { return std::sqrt(m_width * m_width + m_height * m_height); }

    void change_width_stretch(const unsigned int new_width) {
        if (new_width == 0) { throw std::runtime_error("Invalid new width: " + std::to_string(new_width)); }
        std::vector<fizmo::graphics::Color> new_pixels(new_width * m_height);
        double x_ratio = static_cast<double>(m_width) / new_width;
        
        for (unsigned int y = 0; y < m_height; y++) {
            for (unsigned int x = 0; x < new_width; x++) {
                unsigned int src_x = static_cast<int>(x * x_ratio);
                src_x = std::min(src_x, m_width - 1);
                new_pixels[y * new_width + x] = m_pixels[y * m_width + src_x];
            }
        }
        
        m_width = new_width;
        m_pixels = std::move(new_pixels);
    }
    
    void change_width_proportional(const unsigned int new_width) {
        if (new_width == 0) { throw std::runtime_error("Invalid new width: " + std::to_string(new_width)); }
        const double aspect_ratio = static_cast<double>(m_height) / m_width;
        const unsigned int new_height = static_cast<unsigned int>(new_width * aspect_ratio);
        resize(new_width, new_height);
    }
    
    void change_height_stretch(const unsigned int new_height) {
        if (new_height == 0) { throw std::runtime_error("Invalid new height: " + std::to_string(new_height)); }
        std::vector<fizmo::graphics::Color> new_pixels(m_width * new_height);
        double y_ratio = static_cast<double>(m_height) / new_height;
        
        for (unsigned int y = 0; y < new_height; y++) {
            unsigned int src_y = static_cast<int>(y * y_ratio);
            src_y = std::min(src_y, m_height - 1);
            for (unsigned int x = 0; x < m_width; x++) { new_pixels[y * m_width + x] = m_pixels[src_y * m_width + x]; }
        }
        
        m_height = new_height;
        m_pixels = std::move(new_pixels);
    }
    
    void change_height_proportional(const unsigned int new_height) {
        if (new_height == 0) { throw std::runtime_error("Invalid new height: " + std::to_string(new_height)); }
        const double aspect_ratio = static_cast<double>(m_width) / m_height;
        const unsigned int new_width = static_cast<unsigned int>(new_height * aspect_ratio);
        resize(new_width, new_height);
    }
};

} // namespace images
} // namespace fizmo

#endif // BITMAP_IMAGE_HPP