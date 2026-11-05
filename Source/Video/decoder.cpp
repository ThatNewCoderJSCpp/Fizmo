#include "decoder.hpp"
#include "av1.hpp"
#include "h264.hpp"
#include "hevc.hpp"
#include "vp9.hpp"

#include <thread>
#include <utility>

namespace fizmo {
namespace video {

namespace {

class H264VideoDecoder : public VideoDecoder {
public:
    bool open(const TrackInfo& track, std::string* error) {
        if (track.codec_config.empty()) return m_decoder.open_annexb();
        if (track.codec_config.size() >= 5) m_length_size = (track.codec_config[4] & 3u) + 1u;
        return m_decoder.open(track.codec_config.data(), track.codec_config.size(), error);
    }

    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept override {
        if (!m_length_size || !data) return false;
        std::size_t pos = 0;
        bool slices = false;
        while (pos + m_length_size < size) {
            std::size_t len = 0;
            for (unsigned i = 0; i < m_length_size; ++i) len = (len << 8) | data[pos + i];
            pos += m_length_size;
            if (len == 0 || len > size - pos) return false;
            const unsigned type = data[pos] & 0x1Fu;
            const unsigned ref = (data[pos] >> 5) & 3u;
            if (type == 1 || type == 5 || type == 20) {
                if (ref != 0 || type == 5) return false;
                slices = true;
            }
            pos += len;
        }
        return slices;
    }

    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) override {
        return m_decoder.decode(data, size, pts, error);
    }

    bool receive(VideoFrame& out) override { return m_decoder.receive(out); }
    void flush() override { m_decoder.flush(); }
    void reset() override { m_decoder.reset(); }
    const char* codec() const noexcept override { return "h264"; }
    std::uint32_t width() const noexcept override { return m_decoder.info().width; }
    std::uint32_t height() const noexcept override { return m_decoder.info().height; }
    std::uint64_t frames_decoded() const noexcept override { return m_decoder.frames_decoded(); }
    std::uint64_t error_count() const noexcept override { return m_decoder.error_count(); }

private:
    H264Decoder m_decoder;
    unsigned m_length_size = 0;
};

class HevcVideoDecoder : public VideoDecoder {
public:
    bool open(const TrackInfo& track, std::string* error) {
        if (track.codec_config.empty()) return m_decoder.open_annexb();
        return m_decoder.open(track.codec_config.data(), track.codec_config.size(), error);
    }

    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept override { return m_decoder.is_disposable(data, size); }
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) override { return m_decoder.decode(data, size, pts, error); }
    bool receive(VideoFrame& out) override { return m_decoder.receive(out); }
    void flush() override { m_decoder.flush(); }
    void reset() override { m_decoder.reset(); }
    const char* codec() const noexcept override { return "hevc"; }
    std::uint32_t width() const noexcept override { return m_decoder.info().width; }
    std::uint32_t height() const noexcept override { return m_decoder.info().height; }
    std::uint64_t frames_decoded() const noexcept override { return m_decoder.frames_decoded(); }
    std::uint64_t error_count() const noexcept override { return m_decoder.error_count(); }

private:
    HevcDecoder m_decoder;
};

class Av1VideoDecoder : public VideoDecoder {
public:
    bool open(const TrackInfo& track, std::string* error) {
        if (track.codec_config.empty()) return m_decoder.open();
        return m_decoder.open(track.codec_config.data(), track.codec_config.size(), error);
    }

    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept override { return m_decoder.is_disposable(data, size); }
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) override { return m_decoder.decode(data, size, pts, error); }
    bool receive(VideoFrame& out) override { return m_decoder.receive(out); }
    void flush() override { m_decoder.flush(); }
    void reset() override { m_decoder.reset(); }
    const char* codec() const noexcept override { return "av1"; }
    std::uint32_t width() const noexcept override { return m_decoder.info().width ? m_decoder.info().width : m_decoder.info().max_width; }
    std::uint32_t height() const noexcept override { return m_decoder.info().height ? m_decoder.info().height : m_decoder.info().max_height; }
    std::uint64_t frames_decoded() const noexcept override { return m_decoder.frames_decoded(); }
    std::uint64_t error_count() const noexcept override { return m_decoder.error_count(); }

private:
    Av1Decoder m_decoder;
};

class Vp9VideoDecoder : public VideoDecoder {
public:
    bool open(const TrackInfo& track, std::string* error) {
        unsigned n = std::thread::hardware_concurrency();
        if (n == 0) n = 1;
        if (n > 8) n = 8;
        m_decoder.set_threads(static_cast<int>(n));
        return m_decoder.open(track.codec_config.empty() ? nullptr : track.codec_config.data(), track.codec_config.size(), error);
    }

    bool is_disposable(const std::uint8_t* data, std::size_t size) const noexcept override { return vp9_is_disposable(data, size); }
    bool decode(const std::uint8_t* data, std::size_t size, double pts, std::string* error) override { return m_decoder.decode(data, size, pts, error); }
    bool receive(VideoFrame& out) override { return m_decoder.receive(out); }
    void flush() override { m_decoder.flush(); }
    void reset() override { m_decoder.reset(); }
    const char* codec() const noexcept override { return "vp9"; }
    std::uint32_t width() const noexcept override { return m_decoder.info().width; }
    std::uint32_t height() const noexcept override { return m_decoder.info().height; }
    std::uint64_t frames_decoded() const noexcept override { return m_decoder.frames_decoded(); }
    std::uint64_t error_count() const noexcept override { return m_decoder.error_count(); }

private:
    Vp9Decoder m_decoder;
};

} // namespace

bool VideoDecoder::is_disposable(const std::uint8_t* data, std::size_t size) const noexcept {
    (void)data;
    (void)size;
    return false;
}

bool VideoDecoder::decode_packet(const Packet& packet, std::string* error) {
    return decode(packet.data.data(), packet.data.size(), packet.pts, error);
}

bool video_codec_supported(const std::string& codec) noexcept {
    return codec == "h264" || codec == "hevc" || codec == "av1" || codec == "vp9";
}

std::vector<std::string> supported_video_codecs() {
    return { "h264", "hevc", "av1", "vp9" };
}

std::string video_codec_display_name(const std::string& codec) {
    if (codec == "h264") return "H.264 (AVC)";
    if (codec == "hevc") return "H.265 (HEVC)";
    if (codec == "vp8") return "VP8";
    if (codec == "vp9") return "VP9";
    if (codec == "av1") return "AV1";
    if (codec == "mpeg4") return "MPEG-4 Part 2";
    if (codec == "mpeg2video") return "MPEG-2";
    if (codec == "mpeg1video") return "MPEG-1";
    if (codec == "h263") return "H.263";
    if (codec == "mjpeg") return "Motion JPEG";
    if (codec == "prores") return "Apple ProRes";
    if (codec == "theora") return "Theora";
    if (codec == "vc1") return "VC-1";
    if (codec == "dnxhd") return "DNxHD";
    if (codec.empty()) return "unknown";
    return codec;
}

std::string unsupported_video_codec_message(const std::string& codec) {
    return "unsupported video codec " + (codec.empty() ? std::string("(unknown)") : codec) + " (" + video_codec_display_name(codec) + "); fizmo can decode H.264, H.265, AV1 and VP9 video";
}

std::unique_ptr<VideoDecoder> create_video_decoder(const TrackInfo& track, std::string* error) {
    if (track.type != MediaType::video) {
        if (error) *error = "not a video track";
        return nullptr;
    }
    if (track.codec == "h264") {
        std::unique_ptr<H264VideoDecoder> d(new H264VideoDecoder());
        std::string err;
        if (!d->open(track, &err)) {
            if (error) *error = "can't open the H.264 decoder: " + (err.empty() ? std::string("invalid avcC configuration") : err);
            return nullptr;
        }
        return std::unique_ptr<VideoDecoder>(d.release());
    }
    if (track.codec == "hevc") {
        std::unique_ptr<HevcVideoDecoder> d(new HevcVideoDecoder());
        std::string err;
        if (!d->open(track, &err)) {
            if (error) *error = "can't open the H.265 decoder: " + (err.empty() ? std::string("invalid hvcC configuration") : err);
            return nullptr;
        }
        return std::unique_ptr<VideoDecoder>(d.release());
    }
    if (track.codec == "av1") {
        std::unique_ptr<Av1VideoDecoder> d(new Av1VideoDecoder());
        std::string err;
        if (!d->open(track, &err)) {
            if (error) *error = "can't open the AV1 decoder: " + (err.empty() ? std::string("invalid av1C configuration") : err);
            return nullptr;
        }
        return std::unique_ptr<VideoDecoder>(d.release());
    }
    if (track.codec == "vp9") {
        std::unique_ptr<Vp9VideoDecoder> d(new Vp9VideoDecoder());
        std::string err;
        if (!d->open(track, &err)) {
            if (error) *error = "can't open the VP9 decoder: " + (err.empty() ? std::string("invalid configuration") : err);
            return nullptr;
        }
        return std::unique_ptr<VideoDecoder>(d.release());
    }
    if (error) *error = unsupported_video_codec_message(track.codec);
    return nullptr;
}

} // namespace video
} // namespace fizmo
