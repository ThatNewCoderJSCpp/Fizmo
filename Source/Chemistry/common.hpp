#ifndef FIZMO_CHEMISTRY_COMMON_HPP
#define FIZMO_CHEMISTRY_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fizmo {
namespace chemistry {

inline constexpr double NO_VALUE = std::numeric_limits<double>::quiet_NaN();
inline constexpr double INFINITE_TIME = std::numeric_limits<double>::infinity();

constexpr bool has_value(double v) noexcept {
    return v == v;
}

constexpr double value_or(double v, double fallback) noexcept {
    return v == v ? v : fallback;
}

enum class Phase : std::uint8_t {
    Solid,
    Liquid,
    Gas,
    Aqueous,
    Supercritical,
    Plasma,
    Unknown
};

enum class DataQuality : std::uint8_t {
    Unknown,
    Measured,
    Estimated,
    Calculated
};

constexpr const char* to_string(Phase p) noexcept {
    switch (p) {
        case Phase::Solid: return "solid";
        case Phase::Liquid: return "liquid";
        case Phase::Gas: return "gas";
        case Phase::Aqueous: return "aqueous";
        case Phase::Supercritical: return "supercritical";
        case Phase::Plasma: return "plasma";
        default: return "unknown";
    }
}

constexpr const char* phase_label(Phase p) noexcept {
    switch (p) {
        case Phase::Solid: return "s";
        case Phase::Liquid: return "l";
        case Phase::Gas: return "g";
        case Phase::Aqueous: return "aq";
        case Phase::Supercritical: return "sc";
        case Phase::Plasma: return "pl";
        default: return "";
    }
}

constexpr const char* to_string(DataQuality q) noexcept {
    switch (q) {
        case DataQuality::Measured: return "measured";
        case DataQuality::Estimated: return "estimated";
        case DataQuality::Calculated: return "calculated";
        default: return "unknown";
    }
}

template <typename T>
class Span {
public:
    constexpr Span() noexcept = default;
    constexpr Span(const T* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

    constexpr const T* begin() const noexcept { return m_data; }
    constexpr const T* end() const noexcept { return m_data + m_size; }
    constexpr const T* data() const noexcept { return m_data; }
    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr const T& operator[](std::size_t i) const noexcept { return m_data[i]; }
    constexpr const T& front() const noexcept { return m_data[0]; }
    constexpr const T& back() const noexcept { return m_data[m_size - 1]; }

    constexpr const T& at(std::size_t i) const {
        return i < m_size ? m_data[i] : throw std::out_of_range("fizmo::chemistry::Span::at");
    }

private:
    const T* m_data = nullptr;
    std::size_t m_size = 0;
};

constexpr char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr char ascii_upper(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

constexpr bool is_ascii_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

constexpr bool is_ascii_upper(char c) noexcept {
    return c >= 'A' && c <= 'Z';
}

constexpr bool is_ascii_lower(char c) noexcept {
    return c >= 'a' && c <= 'z';
}

constexpr bool equals_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) return false;
    }
    return true;
}

constexpr std::string_view trim(std::string_view s) noexcept {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\n' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\n' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

std::string format_signed(int value, bool sign_after = false);
std::string superscript(std::string_view text);
std::string subscript(std::string_view text);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_COMMON_HPP
