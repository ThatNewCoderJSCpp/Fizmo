#ifndef FIZMO_CHEMISTRY_OXIDATION_STATE_HPP
#define FIZMO_CHEMISTRY_OXIDATION_STATE_HPP

#include "common.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace fizmo {
namespace chemistry {

class OxidationState {
public:
    static constexpr int MIN = -8;
    static constexpr int MAX = 8;

    constexpr OxidationState() noexcept = default;
    constexpr OxidationState(int value) : m_value(checked(value)) {}

    static constexpr bool valid(int value) noexcept { return value >= MIN && value <= MAX; }
    static constexpr OxidationState unknown() noexcept { return OxidationState(UNKNOWN_TAG); }
    static constexpr OxidationState clamped(int value) noexcept {
        return OxidationState(static_cast<std::int8_t>(value < MIN ? MIN : (value > MAX ? MAX : value)), RAW_TAG);
    }
    static constexpr OxidationState from_charge(int charge) noexcept { return valid(charge) ? OxidationState(static_cast<std::int8_t>(charge), RAW_TAG) : unknown(); }

    constexpr bool known() const noexcept { return m_value != UNKNOWN_VALUE; }
    constexpr int value() const { return known() ? m_value : throw std::logic_error("fizmo::chemistry: oxidation state is unknown"); }
    constexpr int value_or(int fallback) const noexcept { return known() ? m_value : fallback; }
    constexpr bool is_positive() const noexcept { return known() && m_value > 0; }
    constexpr bool is_negative() const noexcept { return known() && m_value < 0; }
    constexpr bool is_zero() const noexcept { return m_value == 0; }

    constexpr OxidationState oxidized(int electrons = 1) const { return OxidationState(value() + electrons); }
    constexpr OxidationState reduced(int electrons = 1) const { return OxidationState(value() - electrons); }
    constexpr int electrons_lost_to(const OxidationState& target) const { return target.value() - value(); }

    constexpr OxidationState operator-() const { return OxidationState(-value()); }
    constexpr OxidationState operator+(int n) const { return OxidationState(value() + n); }
    constexpr OxidationState operator-(int n) const { return OxidationState(value() - n); }
    constexpr OxidationState& operator+=(int n) { m_value = checked(value() + n); return *this; }
    constexpr OxidationState& operator-=(int n) { m_value = checked(value() - n); return *this; }
    constexpr OxidationState& operator++() { return *this += 1; }
    constexpr OxidationState& operator--() { return *this -= 1; }

    constexpr bool operator==(const OxidationState& o) const noexcept { return m_value == o.m_value; }
    constexpr bool operator!=(const OxidationState& o) const noexcept { return m_value != o.m_value; }
    constexpr bool operator<(const OxidationState& o) const noexcept { return m_value < o.m_value; }
    constexpr bool operator>(const OxidationState& o) const noexcept { return m_value > o.m_value; }
    constexpr bool operator<=(const OxidationState& o) const noexcept { return m_value <= o.m_value; }
    constexpr bool operator>=(const OxidationState& o) const noexcept { return m_value >= o.m_value; }
    constexpr bool operator==(int v) const noexcept { return known() && m_value == v; }
    constexpr bool operator!=(int v) const noexcept { return !(*this == v); }

    std::string to_string() const;
    std::string roman() const;
    std::string stock_notation(std::string_view symbol_or_name) const;
    std::string superscript() const;

private:
    static constexpr std::int8_t UNKNOWN_VALUE = -128;
    struct UnknownTag {};
    struct RawTag {};
    static constexpr UnknownTag UNKNOWN_TAG{};
    static constexpr RawTag RAW_TAG{};

    constexpr explicit OxidationState(UnknownTag) noexcept : m_value(UNKNOWN_VALUE) {}
    constexpr OxidationState(std::int8_t v, RawTag) noexcept : m_value(v) {}

    static constexpr std::int8_t checked(int v) {
        return valid(v) ? static_cast<std::int8_t>(v) : throw std::out_of_range("fizmo::chemistry: oxidation states run from -8 to +8");
    }

    std::int8_t m_value = 0;
};

class AverageOxidationState {
public:
    constexpr AverageOxidationState() noexcept = default;
    constexpr AverageOxidationState(int numerator, int denominator = 1) : m_numerator(numerator), m_denominator(denominator) {
        if (m_denominator == 0) throw std::invalid_argument("fizmo::chemistry: zero denominator");
        if (m_denominator < 0) {
            m_numerator = -m_numerator;
            m_denominator = -m_denominator;
        }
        const int g = gcd(m_numerator < 0 ? -m_numerator : m_numerator, m_denominator);
        if (g > 1) {
            m_numerator /= g;
            m_denominator /= g;
        }
    }

    constexpr int numerator() const noexcept { return m_numerator; }
    constexpr int denominator() const noexcept { return m_denominator; }
    constexpr double value() const noexcept { return static_cast<double>(m_numerator) / m_denominator; }
    constexpr bool is_integer() const noexcept { return m_denominator == 1; }
    constexpr bool in_range() const noexcept { return value() >= OxidationState::MIN && value() <= OxidationState::MAX; }
    constexpr OxidationState state() const { return is_integer() ? OxidationState(m_numerator) : throw std::logic_error("fizmo::chemistry: oxidation state is not an integer"); }
    constexpr bool operator==(const AverageOxidationState& o) const noexcept { return m_numerator == o.m_numerator && m_denominator == o.m_denominator; }
    constexpr bool operator!=(const AverageOxidationState& o) const noexcept { return !(*this == o); }
    std::string to_string() const;

private:
    static constexpr int gcd(int a, int b) noexcept {
        while (b != 0) {
            const int t = a % b;
            a = b;
            b = t;
        }
        return a;
    }

    int m_numerator = 0;
    int m_denominator = 1;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_OXIDATION_STATE_HPP
