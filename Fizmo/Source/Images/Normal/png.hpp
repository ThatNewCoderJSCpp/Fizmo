#ifndef PNG_TO_BMP_CONVERTER_HPP
#define PNG_TO_BMP_CONVERTER_HPP

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <array>
#include <zlib.h>
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
    bool verify_png_signature(std::ifstream& file) {
        std::array<unsigned char, 8> signature;
        file.read(reinterpret_cast<char*>(signature.data()), 8);
        return std::equal(signature.begin(), signature.end(), PNGHeader::SIGNATURE.begin());
    }
    
    std::uint32_t read_uint32(std::ifstream& file) {
        std::uint32_t value;
        file.read(reinterpret_cast<char*>(&value), 4);
        return swap_endian(value);
    }
    
    std::uint32_t swap_endian(std::uint32_t value) {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    }
    
    PNGChunk read_chunk(std::ifstream& file) {
        PNGChunk chunk;
        chunk.length = read_uint32(file);
        file.read(chunk.type, 4);
        chunk.data.resize(chunk.length);
        file.read(reinterpret_cast<char*>(chunk.data.data()), chunk.length);
        chunk.crc = read_uint32(file);
        return chunk;
    }

    void process_IHDR_chunk(const PNGChunk& chunk) {
        if (chunk.length != 13) {
            throw std::runtime_error("Invalid IHDR chunk size");
        }

        m_ihdr.width = (chunk.data[0] << 24) | (chunk.data[1] << 16) | (chunk.data[2] << 8) | chunk.data[3];
        m_ihdr.height = (chunk.data[4] << 24) | (chunk.data[5] << 16) | (chunk.data[6] << 8) | chunk.data[7];
        m_ihdr.bit_depth = chunk.data[8];
        m_ihdr.color_type = chunk.data[9];
        m_ihdr.compression = chunk.data[10];
        m_ihdr.filter = chunk.data[11];
        m_ihdr.interlace = chunk.data[12];
        if (m_ihdr.compression != 0) { throw std::runtime_error("Unsupported compression method"); }
        if (m_ihdr.filter != 0) { throw std::runtime_error("Unsupported filter method"); }
        if (m_ihdr.interlace != 0) { throw std::runtime_error("Interlaced images not supported"); }
    }

    void process_IDAT_chunk(const PNGChunk& chunk) {
        m_image_data.insert(m_image_data.end(), chunk.data.begin(), chunk.data.end());
    }

    bool decompress_image_data() {
        if (m_image_data.empty()) { return false; }
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));
        if (inflateInit(&strm) != Z_OK) { return false; }
        std::size_t bytes_per_pixel = get_bytes_per_pixel();
        std::size_t row_size = m_ihdr.width * bytes_per_pixel + 1; // +1 for filter type
        std::size_t expected_size = row_size * m_ihdr.height;   
        m_decompressed_data.resize(expected_size);
        strm.next_in = m_image_data.data();
        strm.avail_in = m_image_data.size();
        strm.next_out = m_decompressed_data.data();
        strm.avail_out = m_decompressed_data.size();
        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        return ret == Z_STREAM_END;
    }

    std::size_t get_bytes_per_pixel() const {
        switch (m_ihdr.color_type) {
            case 0: return (m_ihdr.bit_depth + 7) / 8; // Grayscale
            case 2: return 3 * ((m_ihdr.bit_depth + 7) / 8); // RGB
            case 3: return (m_ihdr.bit_depth + 7) / 8; // Palette
            case 4: return 2 * ((m_ihdr.bit_depth + 7) / 8); // Grayscale + Alpha
            case 6: return 4 * ((m_ihdr.bit_depth + 7) / 8); // RGBA
            default: return 0;
        }
    }

    void remove_filters() {
        std::size_t bytes_per_pixel = get_bytes_per_pixel();
        std::size_t row_size = m_ihdr.width * bytes_per_pixel + 1; // +1 for filter type
        m_unfiltered_data.resize(m_ihdr.width * m_ihdr.height * bytes_per_pixel);

        for (std::size_t row = 0; row < m_ihdr.height; ++row) {
            std::size_t filter_type = m_decompressed_data[row * row_size];
            
            for (std::size_t col = 0; col < m_ihdr.width * bytes_per_pixel; ++col) {
                std::size_t x = col;
                std::size_t src_pos = row * row_size + 1 + col;
                std::size_t dst_pos = row * m_ihdr.width * bytes_per_pixel + col;
                unsigned char value = m_decompressed_data[src_pos];
                unsigned char a = 0;
                unsigned char b = 0;
                unsigned char c = 0;
                if (x >= bytes_per_pixel) { a = m_unfiltered_data[dst_pos - bytes_per_pixel]; }
                if (row > 0) { b = m_unfiltered_data[dst_pos - m_ihdr.width * bytes_per_pixel]; }
                if (row > 0 && x >= bytes_per_pixel) { c = m_unfiltered_data[dst_pos - m_ihdr.width * bytes_per_pixel - bytes_per_pixel]; }

                switch (filter_type) {
                    case 0: break; // None
                    case 1: value += a; break; // Sub
                    case 2: value += b; break; // Up
                    case 3: value += (a + b) / 2; break; // Average
                    case 4: value += paeth_predictor(a, b, c); break; // Paeth
                }

                m_unfiltered_data[dst_pos] = value;
            }
        }
    }

    static unsigned char paeth_predictor(int a, int b, int c) {
        int p = a + b - c;
        int pa = std::abs(p - a);
        int pb = std::abs(p - b);
        int pc = std::abs(p - c);        
        if (pa <= pb && pa <= pc) return a;
        if (pb <= pc) return b;
        return c;
    }

public:
    PNGtoBMPConverter(const std::string& png_path) : m_input_path(png_path) {}
    
    bool convert_to_bmp(const std::string& output_path) {
        std::ifstream png_file(m_input_path, std::ios::binary);
        if (!png_file) { throw std::runtime_error("Cannot open PNG file: " + m_input_path); }
        if (!verify_png_signature(png_file)) { throw std::runtime_error("Invalid PNG signature"); }
        bool found_IHDR = false;

        while (png_file.good()) {
            PNGChunk chunk = read_chunk(png_file);
            
            if (std::strncmp(chunk.type, "IHDR", 4) == 0) {
                process_IHDR_chunk(chunk);
                found_IHDR = true;
            } else if (std::strncmp(chunk.type, "IDAT", 4) == 0) {
                process_IDAT_chunk(chunk);
            } else if (std::strncmp(chunk.type, "IEND", 4) == 0) {
                break;
            }
        }
        
        if (!found_IHDR || m_image_data.empty()) { throw std::runtime_error("Invalid PNG structure"); }
        if (!decompress_image_data()) { throw std::runtime_error("Failed to decompress image data"); }
        remove_filters();
        fizmo::images::BitmapImage bmp_image(m_ihdr.width, m_ihdr.height);
        std::size_t bytes_per_pixel = get_bytes_per_pixel();

        for (std::size_t y = 0; y < m_ihdr.height; ++y) {
            for (std::size_t x = 0; x < m_ihdr.width; ++x) {
                std::size_t pos = (y * m_ihdr.width + x) * bytes_per_pixel;
                unsigned char r, g, b;

                switch (m_ihdr.color_type) {
                    case 0: // Grayscale
                        r = g = b = m_unfiltered_data[pos];
                        break;
                    case 2: // RGB
                        r = m_unfiltered_data[pos];
                        g = m_unfiltered_data[pos + 1];
                        b = m_unfiltered_data[pos + 2];
                        break;
                    case 6: // RGBA
                        r = m_unfiltered_data[pos];
                        g = m_unfiltered_data[pos + 1];
                        b = m_unfiltered_data[pos + 2];
                        // Alpha channel is ignored for BMP
                        break;
                    default:
                        throw std::runtime_error("Unsupported color type: " + std::to_string(m_ihdr.color_type));
                }

                bmp_image.set_pixel(x, y, fizmo::graphics::Color(r, g, b));
            }
        }
        
        return bmp_image.save_to_file(output_path);
    }

    fizmo::images::BitmapImage convert_to_bmp_no_save() {
        std::ifstream png_file(m_input_path, std::ios::binary);
        if (!png_file) { throw std::runtime_error("Cannot open PNG file: " + m_input_path); }
        if (!verify_png_signature(png_file)) { throw std::runtime_error("Invalid PNG signature"); }
        bool found_IHDR = false;

        while (png_file.good()) {
            PNGChunk chunk = read_chunk(png_file);
            
            if (std::strncmp(chunk.type, "IHDR", 4) == 0) {
                process_IHDR_chunk(chunk);
                found_IHDR = true;
            } else if (std::strncmp(chunk.type, "IDAT", 4) == 0) {
                process_IDAT_chunk(chunk);
            } else if (std::strncmp(chunk.type, "IEND", 4) == 0) {
                break;
            }
        }
        
        if (!found_IHDR || m_image_data.empty()) { throw std::runtime_error("Invalid PNG structure"); }
        if (!decompress_image_data()) { throw std::runtime_error("Failed to decompress image data"); }
        remove_filters();
        fizmo::images::BitmapImage bmp_image(m_ihdr.width, m_ihdr.height);
        std::size_t bytes_per_pixel = get_bytes_per_pixel();
        
        for (std::size_t y = 0; y < m_ihdr.height; ++y) {
            for (std::size_t x = 0; x < m_ihdr.width; ++x) {
                std::size_t pos = (y * m_ihdr.width + x) * bytes_per_pixel;
                unsigned char r, g, b;

                switch (m_ihdr.color_type) {
                    case 0: // Grayscale
                        r = g = b = m_unfiltered_data[pos];
                        break;
                    case 2: // RGB
                        r = m_unfiltered_data[pos];
                        g = m_unfiltered_data[pos + 1];
                        b = m_unfiltered_data[pos + 2];
                        break;
                    case 6: // RGBA
                        r = m_unfiltered_data[pos];
                        g = m_unfiltered_data[pos + 1];
                        b = m_unfiltered_data[pos + 2];
                        // Alpha channel is ignored for BMP
                        break;
                    default:
                        throw std::runtime_error("Unsupported color type: " + std::to_string(m_ihdr.color_type));
                }

                bmp_image.set_pixel(x, y, fizmo::graphics::Color(r, g, b));
            }
        }

        return bmp_image;
    }
    
    bool validate_input() const {
        if (!is_png_file(m_input_path)) { throw std::runtime_error("Input file must be a PNG image"); }
        std::ifstream file(m_input_path, std::ios::binary);
        if (!file.good()) { throw std::runtime_error("Cannot read input file: " + m_input_path); }
        return true;
    }
    
    static bool is_png_file(const std::string& path) {
        if (path.length() < 4) return false;
        std::string ext = path.substr(path.length() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".png";
    }
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

    std::uint32_t swap_endian(uint32_t value) {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    }

    void write_uint32(std::ofstream& file, std::uint32_t value) {
        std::uint32_t swapped = swap_endian(value);
        file.write(reinterpret_cast<const char*>(&swapped), 4);
    }

    std::uint32_t calculate_crc(const std::vector<unsigned char>& data, std::size_t offset, std::size_t length) {
        return crc32(0, data.data() + offset, length);
    }

    void prepare_raw_data() {
        int width = m_bmp_image.width();
        int height = m_bmp_image.height();
        const std::size_t channels = m_alpha ? 4 : 3;
        std::size_t row_size = width * channels + 1;
        m_raw_data.resize(row_size * height);

        for (int y = 0; y < height; ++y) {
            m_raw_data[y * row_size] = 0;

            for (int x = 0; x < width; ++x) {
                auto color = m_bmp_image.get_pixel(x, y);
                std::size_t pixel_offset = y * row_size + 1 + x * channels;
                m_raw_data[pixel_offset] = color.red();
                m_raw_data[pixel_offset + 1] = color.green();
                m_raw_data[pixel_offset + 2] = color.blue();
                if (m_alpha) m_raw_data[pixel_offset + 3] = color.alpha();
            }
        }
    }

    bool compress_data() {
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));
        if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) { return false; }
        m_compressed_data.resize(deflateBound(&strm, static_cast<uLong>(m_raw_data.size())));
        strm.next_in = m_raw_data.data();
        strm.avail_in = m_raw_data.size();
        strm.next_out = m_compressed_data.data();
        strm.avail_out = m_compressed_data.size();
        int ret = deflate(&strm, Z_FINISH);
        deflateEnd(&strm);
        if (ret != Z_STREAM_END) { return false; }
        m_compressed_data.resize(strm.total_out);
        return true;
    }

    void write_chunk(std::ofstream& file, const char* type, const std::vector<unsigned char>& data) {
        write_uint32(file, data.size());
        file.write(type, 4);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        std::vector<unsigned char> crc_data;
        crc_data.insert(crc_data.end(), type, type + 4);
        crc_data.insert(crc_data.end(), data.begin(), data.end());
        uint32_t crc = calculate_crc(crc_data, 0, crc_data.size());
        write_uint32(file, crc);
    }

public:
    BMPtoPNGConverter(const fizmo::images::BitmapImage& bmp_image) : m_bmp_image(bmp_image) {}

    bool convert_to_png(const std::string& output_path) {
        std::ofstream png_file(output_path, std::ios::binary);
        if (!png_file) { throw std::runtime_error("Cannot create output file: " + output_path); }
        png_file.write(reinterpret_cast<const char*>(PNGHeader::SIGNATURE.data()), PNGHeader::SIGNATURE.size());
        IHDRChunk ihdr;
        m_alpha = has_transparency();
        if (m_alpha) ihdr.color_type = 6;
        ihdr.width = m_bmp_image.width();
        ihdr.height = m_bmp_image.height();

        std::vector<unsigned char> ihdr_data = {
            static_cast<unsigned char>((ihdr.width >> 24) & 0xFF),
            static_cast<unsigned char>((ihdr.width >> 16) & 0xFF),
            static_cast<unsigned char>((ihdr.width >> 8) & 0xFF),
            static_cast<unsigned char>(ihdr.width & 0xFF),
            static_cast<unsigned char>((ihdr.height >> 24) & 0xFF),
            static_cast<unsigned char>((ihdr.height >> 16) & 0xFF),
            static_cast<unsigned char>((ihdr.height >> 8) & 0xFF),
            static_cast<unsigned char>(ihdr.height & 0xFF),
            ihdr.bit_depth,
            ihdr.color_type,
            ihdr.compression,
            ihdr.filter,
            ihdr.interlace
        };

        write_chunk(png_file, "IHDR", ihdr_data);
        prepare_raw_data();
        if (!compress_data()) { throw std::runtime_error("Failed to compress image data"); }
        write_chunk(png_file, "IDAT", m_compressed_data);
        write_chunk(png_file, "IEND", std::vector<unsigned char>());
        return true;
    }
};

inline fizmo::images::BitmapImage create_png_as_bitmap(const std::string& png_file) {
    try {
        PNGtoBMPConverter converter(png_file);
        return converter.convert_to_bmp_no_save();
    } catch (const std::exception& e) {
        std::string message = "Failed to convert PNG to BMP: ";
        message += e.what();
        throw std::runtime_error(message.c_str());
    }
}

inline bool convert_png_to_bitmap(const std::string& png_file, const std::string& out_file) {
    try {
        PNGtoBMPConverter converter(png_file);
        converter.convert_to_bmp(out_file);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

inline bool convert_bmp_to_png(const std::string& input_path, const std::string& output_path) {
    try {
        fizmo::images::BitmapImage bmp_image(input_path);
        return bmp_image.save_to_png(output_path);
    } catch (const std::exception& e) {
        return false;
    }
}

inline bool BitmapImage::save_to_png(const std::string& filename_input) {
    std::string filename = filename_input;
    std::string png_ext = ".png";
    bool has_extension = false;
    
    if (filename.length() >= png_ext.length()) {
        std::string file_ending = filename.substr(filename.length() - png_ext.length());
        std::transform(file_ending.begin(), file_ending.end(), file_ending.begin(), [](unsigned char c) { return std::tolower(c); });
        if (file_ending == png_ext) { has_extension = true; }
    }
    
    if (!has_extension) { filename += png_ext; }
    BMPtoPNGConverter converter(*this);
    return converter.convert_to_png(filename);
}

} // namespace images
} // namespace fizmo

#endif // PNG_TO_BMP_CONVERTER_HPP