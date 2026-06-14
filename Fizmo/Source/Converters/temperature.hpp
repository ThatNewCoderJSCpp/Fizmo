#ifndef TEMPERATURE_CONVERSIONS_HPP
#define TEMPERATURE_CONVERSIONS_HPP

namespace fizmo {
namespace converters {
namespace temperature {

constexpr long double celsius_to_si(const long double celsius) noexcept { return celsius + 273.15; }
constexpr long double fahrenheit_to_si(const long double fahrenheit) noexcept { return (fahrenheit - 32.0) * 5.0 / 9.0 + 273.15; }
constexpr long double rankine_to_si(const long double rankine) noexcept { return rankine * 5.0 / 9.0; }
constexpr long double reaumur_to_si(const long double reaumur) noexcept { return reaumur * 5.0 / 4.0 + 273.15; }
constexpr long double romer_to_si(const long double romer) noexcept { return (romer - 7.5) * 40.0 / 21.0 + 273.15; }
constexpr long double delisle_to_si(const long double delisle) noexcept { return 373.15 - delisle * 2.0 / 3.0; }
constexpr long double newton_to_si(const long double newton) noexcept { return newton * 100.0 / 33.0 + 273.15; }

constexpr long double si_to_celsius(const long double kelvin) noexcept { return kelvin - 273.15; }
constexpr long double si_to_fahrenheit(const long double kelvin) noexcept { return (kelvin - 273.15) * 9.0 / 5.0 + 32.0; }
constexpr long double si_to_rankine(const long double kelvin) noexcept { return kelvin * 9.0 / 5.0; }
constexpr long double si_to_reaumur(const long double kelvin) noexcept { return (kelvin - 273.15) * 4.0 / 5.0; }
constexpr long double si_to_romer(const long double kelvin) noexcept { return (kelvin - 273.15) * 21.0 / 40.0 + 7.5; }
constexpr long double si_to_delisle(const long double kelvin) noexcept { return (373.15 - kelvin) * 3.0 / 2.0; }
constexpr long double si_to_newton(const long double kelvin) noexcept { return (kelvin - 273.15) * 33.0 / 100.0; }

} // namespace temperature
} // namespace converters
} // namespace fizmo

#endif // TEMPERATURE_CONVERSIONS_HPP