#ifndef FIZMO_VIDEO_DECODER_HPP
#define FIZMO_VIDEO_DECODER_HPP

#include "demux.hpp"
#include "frame.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace video {

bool video_codec_supported(const std::string& codec) noexcept;
std::string video_codec_display_name(const std::string& codec);
std::string unsupported_video_codec_message(const std::string& codec);
std::vector<std::string> supported_video_codecs();

class VideoDecoder {
public:
    virtual ~VideoDecoder() = default;

    virtual bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error = nullptr) = 0;
    bool decode_packet(const Packet& packet, std::string* error = nullptr);
    virtual bool receive(VideoFrame& out) = 0;
    virtual void flush() = 0;
    virtual void reset() = 0;
    virtual bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept;

    virtual const char* codec() const noexcept = 0;
    virtual std::uint32_t width() const noexcept = 0;
    virtual std::uint32_t height() const noexcept = 0;
    virtual std::uint64_t frames_decoded() const noexcept = 0;
    virtual std::uint64_t error_count() const noexcept = 0;
};

std::unique_ptr<VideoDecoder> create_video_decoder(const TrackInfo& track, std::string* error = nullptr);

} // namespace video
} // namespace fizmo

#endif // FIZMO_VIDEO_DECODER_HPP
