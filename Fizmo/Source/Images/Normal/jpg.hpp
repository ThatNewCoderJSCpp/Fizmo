#ifndef JPEG_TO_BMP_CONVERTER_HPP
#define JPEG_TO_BMP_CONVERTER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <array>
#include <jpeglib.h>
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

    bool decompress_jpeg() {
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE* infile;

        #ifdef _WIN32
            if (fopen_s(&infile, m_input_path.c_str(), "rb") != 0) {
                return false;
            }
        #else
            infile = fopen(m_input_path.c_str(), "rb");
            if (!infile) {
                return false;
            }
        #endif

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, infile);

        if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
            fclose(infile);
            jpeg_destroy_decompress(&cinfo);
            return false;
        }

        jpeg_start_decompress(&cinfo);
        m_width = cinfo.output_width;
        m_height = cinfo.output_height;
        m_num_components = cinfo.output_components;
        std::size_t row_stride = m_width * m_num_components;
        m_decompressed_data.resize(m_height * row_stride);

        while (cinfo.output_scanline < m_height) {
            unsigned char* row_pointer = &m_decompressed_data[cinfo.output_scanline * row_stride];
            jpeg_read_scanlines(&cinfo, &row_pointer, 1);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return true;
    }

public:
    JPEGtoBMPConverter(const std::string& jpeg_path) : m_input_path(jpeg_path) {}

    fizmo::images::BitmapImage convert_to_bmp() {
        if (!decompress_jpeg()) { throw std::runtime_error("Failed to decompress JPEG file: " + m_input_path); }
        fizmo::images::BitmapImage bmp_image(m_width, m_height);

        for (unsigned int y = 0; y < m_height; ++y) {
            for (unsigned int x = 0; x < m_width; ++x) {
                size_t pos = (y * m_width + x) * m_num_components;
                unsigned char r, g, b;

                if (m_num_components == 3 || m_num_components == 4) {
                    r = m_decompressed_data[pos];
                    g = m_decompressed_data[pos + 1];
                    b = m_decompressed_data[pos + 2];
                } else if (m_num_components == 1) {
                    r = g = b = m_decompressed_data[pos];
                } else {
                    throw std::runtime_error("Unsupported number of components: " + std::to_string(m_num_components));
                }

                bmp_image.set_pixel(x, y, fizmo::graphics::Color(r, g, b));
            }
        }

        return bmp_image;
    }

    bool validate_input() const {
        if (!is_jpeg_file(m_input_path)) { throw std::runtime_error("Input file must be a JPEG image"); }
        std::ifstream file(m_input_path, std::ios::binary);
        if (!file.good()) { throw std::runtime_error("Cannot read input file: " + m_input_path); }
        return true;
    }
    
    static bool is_jpeg_file(const std::string& path) {
        if (path.length() < 4) return false;
        std::string ext = path.substr(path.length() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".jpg" || ext == "jpeg";
    }
};

class BMPtoJPEGConverter {
private:
    const fizmo::images::BitmapImage& m_bmp_image;

public:
    BMPtoJPEGConverter(const fizmo::images::BitmapImage& bmp_image) : m_bmp_image(bmp_image) {}

    bool convert_to_jpeg(const std::string& output_path, int quality = 90) {
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE* outfile;
        int width = m_bmp_image.width();
        int height = m_bmp_image.height();

        #ifdef _WIN32
            if (fopen_s(&outfile, output_path.c_str(), "wb") != 0) {
                return false;
            }
        #else
            outfile = fopen(output_path.c_str(), "wb");
            if (!outfile) {
                return false;
            }
        #endif

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, outfile);
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE);
        jpeg_start_compress(&cinfo, TRUE);
        std::vector<unsigned char> row_buffer(width * 3);
        JSAMPROW row_pointer[1];

        while (cinfo.next_scanline < cinfo.image_height) {
            for (int x = 0; x < width; ++x) {
                auto color = m_bmp_image.get_pixel(x, cinfo.next_scanline);
                row_buffer[x * 3] = color.red();
                row_buffer[x * 3 + 1] = color.green();
                row_buffer[x * 3 + 2] = color.blue();
            }

            row_pointer[0] = row_buffer.data();
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);
        return true;
    }
};

inline fizmo::images::BitmapImage create_jpeg_as_bmp(const std::string& jpeg_file) {
    try {
        JPEGtoBMPConverter converter(jpeg_file);
        return converter.convert_to_bmp();
    } catch (const std::exception& e) {
        std::string message = "Failed to convert JPEG to BMP: ";
        message += e.what();
        throw std::runtime_error(message.c_str());
    }
}

inline bool convert_jpeg_to_bmp(const std::string& jpeg_file, const std::string& out_file) {
    try {
        JPEGtoBMPConverter converter(jpeg_file);
        auto bmp_image = converter.convert_to_bmp();
        return bmp_image.save_to_file(out_file);
    } catch (const std::exception&) {
        return false;
    }
}

inline bool convert_bitmap_to_jpeg(const std::string& input_path, const std::string& output_path, int quality = 90) {
    try {
        fizmo::images::BitmapImage bmp_image(input_path);
        return bmp_image.save_to_jpeg(output_path, quality);
    } catch (const std::exception&) {
        return false;
    }
}

inline bool BitmapImage::save_to_jpeg(const std::string& filename_input, int quality) {
    std::string filename = filename_input;
    std::string jpg_ext = ".jpg";
    std::string jpeg_ext = ".jpeg";
    bool has_extension = false;
    
    if (filename.length() >= jpg_ext.length()) {
        std::string file_ending = filename.substr(filename.length() - jpg_ext.length());
        std::transform(file_ending.begin(), file_ending.end(), file_ending.begin(), [](unsigned char c) { return std::tolower(c); });
        if (file_ending == jpg_ext || file_ending == jpeg_ext) { has_extension = true; }
    }
    
    if (!has_extension && filename.length() >= jpeg_ext.length()) {
        std::string file_ending = filename.substr(filename.length() - jpeg_ext.length());
        std::transform(file_ending.begin(), file_ending.end(), file_ending.begin(), [](unsigned char c) { return std::tolower(c); });
        if (file_ending == jpeg_ext) { has_extension = true; }
    }

    if (!has_extension) { filename += jpg_ext; }
    BMPtoJPEGConverter converter(*this);
    return converter.convert_to_jpeg(filename, quality);
}

} // namespace images
} // namespace fizmo

#endif // JPEG_TO_BMP_CONVERTER_HPP