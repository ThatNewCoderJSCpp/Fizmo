#ifndef FIZMO_FANCY_TEXT_FWD_HPP
#define FIZMO_FANCY_TEXT_FWD_HPP

#include <cstdint>

namespace fizmo {
namespace graphics {
class Renderer;
} // namespace graphics

namespace text {

class FancyText;
struct FancyTexture;
struct MathRenderOptions;
class Text;

enum class FancyAnchor : std::uint8_t { TopLeft, Baseline };

enum class FancyDrawMode : std::uint8_t { Auto, Native, Rasterized };

void draw_fancy_text_native(graphics::Renderer& r, const FancyText& ft, float x, float y, FancyAnchor anchor);

} // namespace text
} // namespace fizmo

#endif // FIZMO_FANCY_TEXT_FWD_HPP
