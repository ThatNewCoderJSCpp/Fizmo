#ifndef FIZMO_VIDEO_DEMUX_HPP
#define FIZMO_VIDEO_DEMUX_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace video {

enum class MediaType { video, audio, subtitle, other };

struct TrackInfo {
    std::uint32_t id = 0;
    MediaType type = MediaType::other;
    std::string codec;
    std::vector<std::uint8_t> codec_config;
    double timescale = 0.0;
    double duration = 0.0;
    std::uint64_t frame_count = 0;
    std::string language;
    std::string name;
    bool is_default = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    double frame_rate = 0.0;
    std::uint32_t display_width = 0;
    std::uint32_t display_height = 0;
    int rotation = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint32_t bitrate = 0;
};

struct Packet {
    std::uint32_t track = 0;
    std::vector<std::uint8_t> data;
    double pts = 0.0;
    double dts = 0.0;
    double duration = 0.0;
    bool keyframe = false;
    std::uint64_t position = 0;
    double discard_padding = 0.0;
};

const char* media_type_name(MediaType type) noexcept;

bool is_mp4(const void* data, std::size_t size) noexcept;
bool is_matroska(const void* data, std::size_t size) noexcept;
bool is_ogg_container(const void* data, std::size_t size) noexcept;
std::string detect_container(const void* data, std::size_t size);

class Demuxer {
public:
    struct Impl;

    Demuxer();
    ~Demuxer();
    Demuxer(Demuxer&&) noexcept;
    Demuxer& operator=(Demuxer&&) noexcept;
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    bool open(const std::string& path, std::string* error = nullptr);
    bool open(const void* data, std::size_t size, std::string* error = nullptr);
    bool open_shared(std::shared_ptr<const std::vector<std::uint8_t>> data, std::string* error = nullptr);
    void close();
    bool is_open() const noexcept;

    const std::vector<TrackInfo>& tracks() const noexcept;
    double duration() const noexcept;
    std::string format_name() const;
    std::vector<std::pair<std::string, std::string>> tags() const;
    int best_track(MediaType type) const noexcept;
    void select(std::uint32_t track, bool on);
    bool selected(std::uint32_t track) const noexcept;

    bool read(Packet& out);
    bool seek(double seconds, bool to_keyframe_before = true);
    bool rewind();

private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_DEMUX_HPP
