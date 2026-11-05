#ifndef FIZMO_VIDEO_AV1_HPP
#define FIZMO_VIDEO_AV1_HPP

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {

struct Av1Info {
    int profile = 0;
    int level = 0;
    int tier = 0;
    int bit_depth = 8;
    bool monochrome = false;
    int subsampling_x = 1;
    int subsampling_y = 1;
    std::uint32_t max_width = 0;
    std::uint32_t max_height = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t render_width = 0;
    std::uint32_t render_height = 0;
    bool film_grain = false;
    bool still_picture = false;
    int operating_points = 1;
    double frame_rate = 0.0;
    ColorInfo color;
};

struct Av1Options {
    bool apply_film_grain = true;
    int operating_point = 0;
    bool output_all_layers = false;
    int threads = 0;
};

enum class Av1StreamFormat {
    unknown,
    obu,
    annexb,
    ivf
};

bool is_av1_obu_stream(const void* data, std::size_t size) noexcept;
Av1StreamFormat detect_av1_stream(const void* data, std::size_t size) noexcept;
PixelFormat av1_pixel_format(int bit_depth, bool monochrome, int subsampling_x, int subsampling_y) noexcept;

class Av1Decoder {
public:
    struct State;

    Av1Decoder();
    ~Av1Decoder();
    Av1Decoder(Av1Decoder&&) noexcept;
    Av1Decoder& operator=(Av1Decoder&&) noexcept;
    Av1Decoder(const Av1Decoder&) = delete;
    Av1Decoder& operator=(const Av1Decoder&) = delete;

    bool open(const std::uint8_t* av1c, std::size_t size, std::string* error = nullptr);
    bool open();
    void set_options(const Av1Options& options);
    const Av1Options& options() const noexcept;
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error = nullptr);
    bool receive(VideoFrame& out);
    void flush();
    void reset();
    void close();
    bool is_open() const noexcept;
    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept;

    const Av1Info& info() const noexcept;
    std::uint64_t frames_decoded() const noexcept;
    std::uint64_t error_count() const noexcept;

private:
    std::unique_ptr<State> m_state;
};

class Av1FileReader {
public:
    Av1FileReader() = default;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size);
    void close();
    bool is_open() const noexcept { return m_open; }
    Av1StreamFormat format() const noexcept { return m_format; }
    bool read(std::vector<std::uint8_t>& temporal_unit, double* pts = nullptr);
    void rewind() noexcept;
    std::uint32_t ivf_width() const noexcept { return m_ivf_width; }
    std::uint32_t ivf_height() const noexcept { return m_ivf_height; }
    double frame_rate() const noexcept { return m_rate; }

private:
    bool start();
    bool read_obu_tu(std::vector<std::uint8_t>& out);
    bool read_annexb_tu(std::vector<std::uint8_t>& out);

    std::vector<std::uint8_t> m_data;
    std::size_t m_pos = 0;
    std::size_t m_first = 0;
    Av1StreamFormat m_format = Av1StreamFormat::unknown;
    bool m_open = false;
    std::uint32_t m_ivf_width = 0;
    std::uint32_t m_ivf_height = 0;
    std::uint32_t m_ivf_rate = 0;
    std::uint32_t m_ivf_scale = 0;
    double m_rate = 0.0;
    std::uint64_t m_count = 0;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_AV1_HPP
