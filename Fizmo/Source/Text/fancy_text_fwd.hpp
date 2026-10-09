#ifndef FIZMO_FANCY_TEXT_FWD_HPP
#define FIZMO_FANCY_TEXT_FWD_HPP

#include <cstdint>

namespace fizmo {
namespace text {

class FancyText;
struct FancyTexture;
struct MathRenderOptions;

enum class FancyAnchor : std::uint8_t { TopLeft, Baseline };

enum class FancyDrawMode : std::uint8_t { Auto, Native, Rasterized };

} // namespace text
} // namespace fizmo

#endif // FIZMO_FANCY_TEXT_FWD_HPP
