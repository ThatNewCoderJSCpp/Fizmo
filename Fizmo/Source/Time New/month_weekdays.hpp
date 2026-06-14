#ifndef FIZMO_MONTHS_WEEKDAYS_HPP
#define FIZMO_MONTHS_WEEKDAYS_HPP

#include <cstdint>
#include <string>
#include <ostream>
#include <sstream>
#include <type_traits>

namespace fizmo {
namespace temp_time {

enum class Months : std::uint8_t {
    january = 0,
    february,
    march,
    april,
    may,
    june,
    july,
    august,
    september,
    october,
    november,
    december
};

inline constexpr const char* month_name(Months m) noexcept {
    switch (m) {
        case Months::january:   return "January";
        case Months::february:  return "February";
        case Months::march:     return "March";
        case Months::april:     return "April";
        case Months::may:       return "May";
        case Months::june:      return "June";
        case Months::july:      return "July";
        case Months::august:    return "August";
        case Months::september: return "September";
        case Months::october:   return "October";
        case Months::november:  return "November";
        case Months::december:  return "December";
    }
    return "Unknown";
}

enum class Weekdays : std::uint8_t {
    monday = 0,
    tuesday,
    wednesday,
    thursday,
    friday,
    saturday,
    sunday
};

inline constexpr const char* weekday_name(Weekdays d) noexcept {
    switch (d) {
        case Weekdays::sunday:    return "Sunday";
        case Weekdays::monday:    return "Monday";
        case Weekdays::tuesday:   return "Tuesday";
        case Weekdays::wednesday: return "Wednesday";
        case Weekdays::thursday:  return "Thursday";
        case Weekdays::friday:    return "Friday";
        case Weekdays::saturday:  return "Saturday";
    }
    return "Unknown";
}

class Month {
private:
    Months m_;

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static constexpr Months wrap(T v) noexcept {
        v %= 12;
        if (v < 0) v += 12;
        return static_cast<Months>(v);
    }

public:
    constexpr Month() noexcept : m_(Months::january) {}
    constexpr Month(Months m) noexcept : m_(m) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr explicit Month(T v) noexcept : m_(wrap(v)) {}

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() noexcept { return static_cast<T>(m_); }

    constexpr std::uint8_t as_int() const noexcept { return static_cast<std::uint8_t>(m_); }

    constexpr Months value() const noexcept { return m_; }
    constexpr Month& operator++() noexcept {
        m_ = wrap(static_cast<int>(m_) + 1);
        return *this;
    }
    constexpr Month operator++(int) noexcept {
        Month tmp(*this);
        ++(*this);
        return tmp;
    }
    constexpr Month& operator--() noexcept {
        m_ = wrap(static_cast<int>(m_) - 1);
        return *this;
    }
    constexpr Month operator--(int) noexcept {
        Month tmp(*this);
        --(*this);
        return tmp;
    }
    friend std::ostream& operator<<(std::ostream& os, const Month& m) { return os << month_name(m.m_); }
    std::string to_string() const {
        std::ostringstream oss;
        oss << *this;
        return oss.str();
    }
};

class Weekday {
private:
    Weekdays d_;

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static constexpr Weekdays wrap(T v) noexcept {
        v %= 7;
        if (v < 0) v += 7;
        return static_cast<Weekdays>(v);
    }

public:
    constexpr Weekday() noexcept : d_(Weekdays::monday) {}
    constexpr Weekday(Weekdays d) noexcept : d_(d) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr explicit Weekday(T v) noexcept : d_(wrap(v)) {}

    constexpr Weekdays value() const noexcept { return d_; }

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() noexcept { return static_cast<T>(d_); }

    constexpr Weekday& operator++() noexcept {
        d_ = wrap(static_cast<int>(d_) + 1);
        return *this;
    }
    constexpr Weekday operator++(int) noexcept {
        Weekday tmp(*this);
        ++(*this);
        return tmp;
    }
    constexpr Weekday& operator--() noexcept {
        d_ = wrap(static_cast<int>(d_) - 1);
        return *this;
    }
    constexpr Weekday operator--(int) noexcept {
        Weekday tmp(*this);
        --(*this);
        return tmp;
    }
    friend std::ostream& operator<<(std::ostream& os, const Weekday& d) { return os << weekday_name(d.d_); }
    std::string to_string() const {
        std::ostringstream oss;
        oss << *this;
        return oss.str();
    }
};

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_MONTHS_WEEKDAYS_HPP