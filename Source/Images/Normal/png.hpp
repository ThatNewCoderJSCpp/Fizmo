#ifndef PNG_TO_BMP_CONVERTER_HPP
#define PNG_TO_BMP_CONVERTER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <array>
#include "../Bitmap/image.hpp"

namespace fizmo {
namespace images {

class PNGtoBMPConverter {
private:
    struct PNGHeader {
        static constexpr std::array<unsigned char, 8> SIGNATURE = {137, 80, 78, 71, 13, 10, 26, 10};
    };

    struct PNGChunk {
        uint32_t length;
        char type[4];
        std::vector<unsigned char> data;
        std::uint32_t crc;
    };

    struct IHDRData {
        std::uint32_t width;
        std::uint32_t height;
        std::uint8_t bit_depth;
        std::uint8_t color_type;
        std::uint8_t compression;
        std::uint8_t filter;
        std::uint8_t interlace;
    };

    std::string m_input_path;
    IHDRData m_ihdr;
    std::vector<unsigned char> m_image_data;
    std::vector<unsigned char> m_decompressed_data;
    std::vector<unsigned char> m_unfiltered_data;
    
private:
    bool verify_png_signature(std::ifstream& file);
    
    std::uint32_t read_uint32(std::ifstream& file);
    
    std::uint32_t swap_endian(std::uint32_t value);
    
    PNGChunk read_chunk(std::ifstream& file);

    void process_IHDR_chunk(const PNGChunk& chunk);

    void process_IDAT_chunk(const PNGChunk& chunk) {
        m_image_data.insert(m_image_data.end(), chunk.data.begin(), chunk.data.end());
    }

    bool decompress_image_data();

    std::size_t get_bytes_per_pixel() const;

    void remove_filters();

    static unsigned char paeth_predictor(int a, int b, int c);

public:
    PNGtoBMPConverter(const std::string& png_path) : m_input_path(png_path) {}
    
    bool convert_to_bmp(const std::string& output_path);

    fizmo::images::BitmapImage convert_to_bmp_no_save();
    
    bool validate_input() const;
    
    static bool is_png_file(const std::string& path);
};

class BMPtoPNGConverter {
private:
    struct PNGHeader {
        static constexpr std::array<unsigned char, 8> SIGNATURE = {137, 80, 78, 71, 13, 10, 26, 10};
    };

    struct IHDRChunk {
        uint32_t width;
        uint32_t height;
        uint8_t bit_depth = 8;
        uint8_t color_type = 2;  // RGB
        uint8_t compression = 0;
        uint8_t filter = 0;
        uint8_t interlace = 0;
    };

private:
    const fizmo::images::BitmapImage& m_bmp_image;
    std::vector<unsigned char> m_raw_data;
    std::vector<unsigned char> m_compressed_data;
    bool m_alpha = false;

    bool has_transparency() const {
        for (const auto& c : m_bmp_image.pixels()) if (c.alpha() != 255) return true;
        return false;
    }

    std::uint32_t swap_endian(uint32_t value);

    void write_uint32(std::ofstream& file, std::uint32_t value);

    std::uint32_t calculate_crc(const std::vector<unsigned char>& data, std::size_t offset, std::size_t length);

    void prepare_raw_data();

    bool compress_data();

    void write_chunk(std::ofstream& file, const char* type, const std::vector<unsigned char>& data);

public:
    BMPtoPNGConverter(const fizmo::images::BitmapImage& bmp_image) : m_bmp_image(bmp_image) {}

    bool convert_to_png(const std::string& output_path);
};

fizmo::images::BitmapImage create_png_as_bitmap(const std::string& png_file);

bool convert_png_to_bitmap(const std::string& png_file, const std::string& out_file);

bool convert_bmp_to_png(const std::string& input_path, const std::string& output_path);



} // namespace images
} // namespace fizmo

#endif // PNG_TO_BMP_CONVERTER_HPP