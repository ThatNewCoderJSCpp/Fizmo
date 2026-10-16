#ifndef FIZMO_IMAGES_FORMATS_HPP
#define FIZMO_IMAGES_FORMATS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../Bitmap/image.hpp"

namespace fizmo {
namespace images {

enum class ImageFormat : std::uint8_t { Unknown = 0, Png, Jpeg, Bmp, Gif, WebP, Tga, Qoi, Pnm, Ico, Cur, Tiff, Svg };

struct AnimatedFrame {
    BitmapImage   image;
    std::uint32_t delay_ms = 100;
};

class AnimatedImage {
public:
    AnimatedImage() = default;
    explicit AnimatedImage(BitmapImage still) : m_width(still.width()), m_height(still.height()) { m_frames.push_back(AnimatedFrame{ std::move(still), 0 }); }

    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    std::size_t frame_count() const noexcept { return m_frames.size(); }
    bool empty() const noexcept { return m_frames.empty(); }
    bool animated() const noexcept { return m_frames.size() > 1; }
    std::uint32_t loop_count() const noexcept { return m_loops; }

    const std::vector<AnimatedFrame>& frames() const noexcept { return m_frames; }
    std::vector<AnimatedFrame>& frames() noexcept { return m_frames; }
    const AnimatedFrame& frame(std::size_t i) const { return m_frames.at(i); }

    void set_size(unsigned int w, unsigned int h) noexcept {
        m_width = w;
        m_height = h;
    }

    void set_loop_count(std::uint32_t loops) noexcept { m_loops = loops; }

    void add_frame(BitmapImage image, std::uint32_t delay_ms) {
        if (m_frames.empty() && m_width == 0) {
            m_width = image.width();
            m_height = image.height();
        }

        m_frames.push_back(AnimatedFrame{ std::move(image), delay_ms });
    }

    std::uint64_t duration_ms() const noexcept {
        std::uint64_t t = 0;
        for (const auto& f : m_frames) t += effective_delay(f.delay_ms);
        return t;
    }

    std::size_t frame_index_at(std::uint64_t time_ms) const noexcept {
        if (m_frames.size() <= 1) return 0;
        const std::uint64_t total = duration_ms();

        if (m_loops != 0 && time_ms >= total * m_loops) return m_frames.size() - 1;
        std::uint64_t t = total ? time_ms % total : 0;

        for (std::size_t i = 0; i < m_frames.size(); ++i) {
            const std::uint64_t d = effective_delay(m_frames[i].delay_ms);
            if (t < d) return i;
            t -= d;
        }

        return m_frames.size() - 1;
    }

    const BitmapImage& frame_at(std::uint64_t time_ms) const { return m_frames.at(frame_index_at(time_ms)).image; }

    static std::uint32_t effective_delay(std::uint32_t d) noexcept { return d < 20 ? 100 : d; }

private:
    std::vector<AnimatedFrame> m_frames;
    unsigned int                m_width = 0;
    unsigned int                m_height = 0;
    std::uint32_t               m_loops = 0;
};

struct EncodeOptions {
    int  quality = 90;
    bool alpha = true;
    bool lossless = true;
    int  compression = 6;
    bool dither = true;
};

struct SvgOptions {
    unsigned int width = 0;
    unsigned int height = 0;
    float        scale = 1.0f;
    float        dpi = 96.0f;
    colors::Color background = colors::Color(0, 0, 0, 0);
};

ImageFormat detect_format(const std::uint8_t* data, std::size_t size) noexcept;
inline ImageFormat detect_format(const std::vector<std::uint8_t>& data) noexcept { return detect_format(data.data(), data.size()); }
ImageFormat format_from_extension(std::string_view path_or_extension) noexcept;
const char* format_name(ImageFormat f) noexcept;
const char* format_extension(ImageFormat f) noexcept;
const char* format_mime_type(ImageFormat f) noexcept;
bool can_decode(ImageFormat f) noexcept;
bool can_encode(ImageFormat f) noexcept;
bool can_animate(ImageFormat f) noexcept;

bool decode_image(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool decode_animation(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error = nullptr);
bool encode_image(const BitmapImage& img, ImageFormat format, std::vector<std::uint8_t>& out, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);
bool encode_animation(const AnimatedImage& anim, ImageFormat format, std::vector<std::uint8_t>& out, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);

bool load_image(const std::string& path, BitmapImage& out, std::string* error = nullptr);
bool load_animation(const std::string& path, AnimatedImage& out, std::string* error = nullptr);
bool save_image(const std::string& path, const BitmapImage& img, ImageFormat format = ImageFormat::Unknown, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);
bool save_animation(const std::string& path, const AnimatedImage& anim, ImageFormat format = ImageFormat::Unknown, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);

BitmapImage load_image(const std::string& path);
AnimatedImage load_animation(const std::string& path);

bool decode_bmp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool encode_bmp(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = true);

bool is_tga(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_tga(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool encode_tga(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = true, bool rle = true);

bool is_qoi(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_qoi(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool encode_qoi(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = true);

bool is_pnm(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_pnm(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool encode_pnm(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = false);

bool is_ico(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_ico(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool decode_ico_all(const std::uint8_t* data, std::size_t size, std::vector<BitmapImage>& out, std::string* error = nullptr);
bool encode_ico(const std::vector<BitmapImage>& images, std::vector<std::uint8_t>& out, std::string* error = nullptr);

bool is_gif(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_gif(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error = nullptr);
bool encode_gif(const AnimatedImage& anim, std::vector<std::uint8_t>& out, bool dither = true, std::string* error = nullptr);

bool is_apng(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_apng(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error = nullptr);
bool encode_apng(const AnimatedImage& anim, std::vector<std::uint8_t>& out, std::string* error = nullptr);

bool is_webp(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_webp(const std::uint8_t* data, std::size_t size, AnimatedImage& out, std::string* error = nullptr);
bool decode_webp(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool encode_webp(const BitmapImage& img, std::vector<std::uint8_t>& out, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);
bool encode_webp(const AnimatedImage& anim, std::vector<std::uint8_t>& out, const EncodeOptions& options = EncodeOptions(), std::string* error = nullptr);

bool is_tiff(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_tiff(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error = nullptr);
bool decode_tiff_pages(const std::uint8_t* data, std::size_t size, std::vector<BitmapImage>& out, std::string* error = nullptr);
bool encode_tiff(const BitmapImage& img, std::vector<std::uint8_t>& out, bool alpha = true, bool compress = true);

bool is_svg(const std::uint8_t* data, std::size_t size) noexcept;
bool decode_svg(const std::uint8_t* data, std::size_t size, BitmapImage& out, const SvgOptions& options = SvgOptions(), std::string* error = nullptr);
bool decode_svg(std::string_view source, BitmapImage& out, const SvgOptions& options = SvgOptions(), std::string* error = nullptr);

} // namespace images
} // namespace fizmo

#endif // FIZMO_IMAGES_FORMATS_HPP
