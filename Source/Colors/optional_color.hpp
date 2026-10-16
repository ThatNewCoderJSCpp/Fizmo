#ifndef FIZMO_COLORS_OPTIONAL_COLOR_HPP
#define FIZMO_COLORS_OPTIONAL_COLOR_HPP

#include "color.hpp"

namespace fizmo {
namespace colors {

class OptionalColor {
private:
    Color m_color;
    bool m_set = false;

public:
    constexpr OptionalColor()                             noexcept = default;
    constexpr OptionalColor(const Color& c)               noexcept : m_color(c), m_set(true) {}
    constexpr bool is_set()                         const noexcept { return m_set;   }
    constexpr const Color& value()                  const noexcept { return m_color; }
    constexpr Color value_or(const Color& fallback) const noexcept { return m_set ? m_color : fallback; }
    void set(const Color& c)                              noexcept { m_color = c; m_set = true; }
    void clear()                                          noexcept { m_set = false;             }

    constexpr bool operator==(const OptionalColor& o) const noexcept {
        if (m_set != o.m_set) return false;
        return !m_set || m_color == o.m_color;
    }

    constexpr bool operator!=(const OptionalColor& o) const noexcept { return !(*this == o); }
};

} // namespace colors
} // namespace fizmo

#endif // FIZMO_COLORS_OPTIONAL_COLOR_HPP
