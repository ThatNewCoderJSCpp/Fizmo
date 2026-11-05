#ifndef FIZMO_VIDEO_VP9_HPP
#define FIZMO_VIDEO_VP9_HPP

#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {

struct Vp9Info {
    int profile = 0;
    int bit_depth = 8;
    int subsampling_x = 1;
    int subsampling_y = 1;
    int color_space = 0;
    bool full_range = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t render_width = 0;
    std::uint32_t render_height = 0;
    int tile_columns = 1;
    int tile_rows = 1;
    PixelFormat format = PixelFormat::none;
    ColorInfo color;
};

struct Vp9CodecConfig {
    int profile = -1;
    int level = -1;
    int bit_depth = 0;
    int chroma_subsampling = -1;
    int full_range = -1;
    int primaries = -1;
    int transfer = -1;
    int matrix = -1;
};

bool parse_vp9_codec_config(const std::uint8_t* data, std::size_t size, Vp9CodecConfig& out) noexcept;
std::size_t vp9_split_superframe(const std::uint8_t* data, std::size_t size, std::size_t* offsets, std::size_t* sizes, std::size_t max_frames) noexcept;
bool vp9_is_keyframe(const std::uint8_t* data, std::size_t size) noexcept;
bool vp9_is_disposable(const std::uint8_t* data, std::size_t size) noexcept;

class Vp9Decoder {
public:
    struct State;

    Vp9Decoder();
    ~Vp9Decoder();
    Vp9Decoder(Vp9Decoder&&) noexcept;
    Vp9Decoder& operator=(Vp9Decoder&&) noexcept;
    Vp9Decoder(const Vp9Decoder&) = delete;
    Vp9Decoder& operator=(const Vp9Decoder&) = delete;

    bool open(const std::uint8_t* config = nullptr, std::size_t size = 0, std::string* error = nullptr);
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error = nullptr);
    bool receive(VideoFrame& out);
    void flush();
    void reset();
    void close();
    bool is_open() const noexcept;
    void set_threads(int threads) noexcept;
    int threads() const noexcept;

    const Vp9Info& info() const noexcept;
    std::uint64_t frames_decoded() const noexcept;
    std::uint64_t error_count() const noexcept;

private:
    std::unique_ptr<State> m_state;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_VP9_HPP
