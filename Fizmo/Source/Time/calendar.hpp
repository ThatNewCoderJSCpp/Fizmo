#ifndef FIZMO_CALENDAR_CLASS_HPP
#define FIZMO_CALENDAR_CLASS_HPP

#include "small_time.hpp"

namespace fizmo {
namespace time {

class Calendar {
public:
    static constexpr bool is_leap_year(const year y) noexcept {
        const std::uint64_t year_value = y.value();
        return (year_value % 4 == 0 && (year_value % 100 != 0 || year_value % 400 == 0));
    }
    
    static constexpr std::uint8_t days_in_month(const month m, const year y) noexcept {
        constexpr std::uint8_t days_per_month[] = {
            31, // January
            28, // February (non-leap)
            31, // March
            30, // April
            31, // May
            30, // June
            31, // July
            31, // August
            30, // September
            31, // October
            30, // November
            31  // December
        };
        
        const std::uint8_t month_idx = m.to_int();
        if (month_idx == 2) { return is_leap_year(y) ? 29 : 28; }
        return days_per_month[month_idx - 1];
    }

    static constexpr std::uint8_t days_in_month(const month m, const bool is_leap_year = false) noexcept {
        const year yr = year(is_leap_year ? 1600 : 1601);
        return days_in_month(m, yr);
    }
    
    static constexpr std::uint16_t days_in_year(const year y) noexcept { return is_leap_year(y) ? 366 : 365; }
    static constexpr double exact_days_in_year(const year y) noexcept { return is_leap_year(y) ? 366.2422 : 365.2422; }
    static constexpr weekday get_weekday(const year y, const month m, const day d, const bool is_ad_year = true) noexcept;
};

} // namespace time
} // namespace fizmo 

#endif // FIZMO_CALENDAR_CLASS_HPP