#ifndef FIZMO_TEXT_EMOJI_HPP
#define FIZMO_TEXT_EMOJI_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace images {
class AnimatedImage;
class BitmapImage;
} // namespace images

namespace text {

using EmojiImage = std::shared_ptr<const images::AnimatedImage>;

class EmojiRegistry {
public:
    EmojiRegistry() = default;
    EmojiRegistry(const EmojiRegistry&) = delete;
    EmojiRegistry& operator=(const EmojiRegistry&) = delete;

    void add(std::string name, EmojiImage image);
    void add(std::string name, const images::BitmapImage& still);
    void add(std::string name, images::AnimatedImage animation);
    bool add_file(std::string name, const std::string& path, std::string* error = nullptr);
    bool add_memory(std::string name, const std::uint8_t* data, std::size_t size, std::string* error = nullptr);
    std::size_t add_directory(const std::string& directory, std::string* error = nullptr);

    bool        remove(std::string_view name);
    void        clear();
    bool        contains(std::string_view name) const;
    EmojiImage  find(std::string_view name) const;
    std::size_t size() const;
    std::vector<std::string> names() const;

    void set_resolver(std::function<EmojiImage(std::string_view)> resolver);

private:
    static std::string normalize(std::string_view name);

    mutable std::mutex                          m_mutex;
    mutable std::unordered_map<std::string, EmojiImage> m_images;
    std::function<EmojiImage(std::string_view)> m_resolver;
};

EmojiRegistry& emoji_registry();

std::string_view unicode_emoji(std::string_view shortcode) noexcept;

std::vector<std::string_view> unicode_emoji_shortcodes();

bool is_emoji_only(std::string_view utf8) noexcept;

double animation_clock_ms();

void set_animation_clock(std::function<double()> clock);

void reset_animation_clock();

} // namespace text
} // namespace fizmo

#endif // FIZMO_TEXT_EMOJI_HPP
