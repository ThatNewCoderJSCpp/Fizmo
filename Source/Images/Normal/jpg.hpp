#ifndef JPEG_TO_BMP_CONVERTER_HPP
#define JPEG_TO_BMP_CONVERTER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <array>
#include "../Bitmap/image.hpp"

namespace fizmo {
namespace images {

class JPEGtoBMPConverter {
private:
    std::string m_input_path;
    std::vector<unsigned char> m_decompressed_data;
    unsigned int m_width;
    unsigned int m_height;
    int m_num_components;

    bool decompress_jpeg();

public:
    JPEGtoBMPConverter(const std::string& jpeg_path) : m_input_path(jpeg_path) {}

    fizmo::images::BitmapImage convert_to_bmp();

    bool validate_input() const;
    
    static bool is_jpeg_file(const std::string& path);
};

class BMPtoJPEGConverter {
private:
    const fizmo::images::BitmapImage& m_bmp_image;

public:
    BMPtoJPEGConverter(const fizmo::images::BitmapImage& bmp_image) : m_bmp_image(bmp_image) {}

    bool convert_to_jpeg(const std::string& output_path, int quality = 90);
};

fizmo::images::BitmapImage create_jpeg_as_bmp(const std::string& jpeg_file);

bool convert_jpeg_to_bmp(const std::string& jpeg_file, const std::string& out_file);

bool convert_bitmap_to_jpeg(const std::string& input_path, const std::string& output_path, int quality = 90);



} // namespace images
} // namespace fizmo

#endif // JPEG_TO_BMP_CONVERTER_HPP