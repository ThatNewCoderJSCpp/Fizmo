#ifndef FIZMO_TEMP_TIME_CALENDAR_HPP
#define FIZMO_TEMP_TIME_CALENDAR_HPP

#include "month_weekdays.hpp"
#include "time_unit.hpp"
#include <cstdint>

namespace fizmo {
namespace temp_time {

class Calendar {
public:
    template<typename Y, typename = typename std::enable_if<detail::is_integer_like_v<Y>>::type>
    static constexpr bool is_leap_year(const Y& year) noexcept {
        using W = detail::wider_t<Y, int>;
        const W y = static_cast<W>(year);
        return (y % W(4) == W(0)) && ((y % W(100) != W(0)) || (y % W(400) == W(0)));
    }

    template<typename Y, typename = typename std::enable_if<detail::is_integer_like_v<Y>>::type>
    static constexpr std::uint8_t days_in_month(Month m, const Y& year) noexcept {
        constexpr std::uint8_t base_days[] = {
            31, 28, 31, 30, 31, 30,
            31, 31, 30, 31, 30, 31
        };
        const std::uint8_t idx = static_cast<std::uint8_t>(m.value());
        if (idx == 1) { return is_leap_year(year) ? 29 : 28; }
        return base_days[idx];
    }

    static constexpr std::uint8_t days_in_month(Month m, bool leap) noexcept { return days_in_month(m, (leap ? 1600 : 1601)); }

    template<typename Y, typename = typename std::enable_if<detail::is_integer_like_v<Y>>::type>
    static constexpr std::uint16_t days_in_year(const Y& year) noexcept { return is_leap_year(year) ? 366 : 365; }

    template<typename Y, typename = typename std::enable_if<detail::is_integer_like_v<Y>>::type>
    static constexpr double exact_days_in_year(const Y& year) noexcept { return is_leap_year(year) ? 366.2422 : 365.2422; }

    template<typename Y, typename D, typename = typename std::enable_if<detail::is_integer_like_v<Y> && detail::is_integer_like_v<D>>::type>
    static constexpr Weekday get_weekday(
        const Y& year,
        Month month,
        const D& day,
        bool is_ad_year = true
    ) noexcept {
        using W = detail::wider_t<Y, D>;
        using WW = detail::wider_t<W, int>;
        WW y = is_ad_year ? WW(year) : -WW(year - 1);
        WW m = WW(static_cast<std::uint8_t>(month.value())) + 1;
        WW d = WW(day);
        if (m <= 2) {
            m += 12;
            --y;
        }
        const WW K = y % 100;
        const WW J = y / 100;
        const WW h = (d + (13 * (m + 1)) / 5 + K + K/4 + J/4 + 5*J) % 7;
        constexpr Weekdays map[7] = {
            Weekdays::saturday,
            Weekdays::sunday,
            Weekdays::monday,
            Weekdays::tuesday,
            Weekdays::wednesday,
            Weekdays::thursday,
            Weekdays::friday
        };
        return Weekday(map[static_cast<long long>(h)]);
    }
};

} // namespace temp_time

template<temp_time::Unit U, typename = void>
struct calendar_unit_traits;

template<temp_time::Unit U>
struct calendar_unit_traits<U, typename std::enable_if<(static_cast<std::uint8_t>(U) >= static_cast<std::uint8_t>(temp_time::Unit::month))
>::type> {
    static constexpr std::uint64_t months_per_unit() noexcept {
        constexpr std::uint64_t table[] = {
            1ULL,      // month
            12ULL,     // year
            120ULL,    // decade
            1200ULL,   // century
            12000ULL   // millennium
        };
        return table[static_cast<std::uint8_t>(U) - static_cast<std::uint8_t>(temp_time::Unit::month)];
    }
};

template<temp_time::Unit U>
constexpr std::uint64_t months_per_unit_v = calendar_unit_traits<U>::months_per_unit();

template<temp_time::Unit U>
constexpr bool is_month_based_unit_v = (static_cast<std::uint8_t>(U) >= static_cast<std::uint8_t>(temp_time::Unit::month));

} // namespace fizmo

#endif // FIZMO_TEMP_TIME_CALENDAR_HPP