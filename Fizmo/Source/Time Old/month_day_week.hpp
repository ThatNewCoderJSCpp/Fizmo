#ifndef FIZMO_MONTH_DAY_WEEK_CLASSES_HPP
#define FIZMO_MONTH_DAY_WEEK_CLASSES_HPP

#include "year_time_classes.hpp"
#include <string>

namespace fizmo {
namespace time {

enum class months : std::uint8_t {
    January = 0,
    February,
    March,
    April,
    May,
    June,
    July,
    August,
    September,
    October,
    November,
    December
};

class month {
private:
    months m_value;
    
public:
    constexpr month() noexcept : m_value(months::January) {}
    constexpr month(const months m) noexcept : m_value(m) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr month(const T m) noexcept : m_value(static_cast<months>(fizmo::abs_constexpr(m) % 12)) {}
    
    constexpr month(const month& m) noexcept : m_value(m.m_value) {}
    constexpr month(month&& m) noexcept : m_value(m.m_value) {}
    
    OPTIONAL_CPP14_CONSTEXPR month& operator=(const month& m) noexcept {
        if (this != &m) { m_value = m.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR month& operator=(month&& m) noexcept {
        if (this != &m) { m_value = m.m_value; }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR month& operator=(const months m) noexcept {
        if (m_value != m) { m_value = m; }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR month& operator=(const T v) noexcept {
        if (v == 0) { 
            m_value = months::January;
            return *this;
        }
        
        const int n = static_cast<int>(v) - (v > 0 ? 1 : 0);
        const int idx = v > 0 ? (((n % 12) + 12) % 12) : (((12 + (n % 12)) % 12 + 11) % 12);
        m_value = static_cast<months>(idx);
        return *this;
    }

public:
    constexpr std::uint8_t to_int() const noexcept { return static_cast<std::uint8_t>(m_value) + 1; }
    constexpr months to_enum() const noexcept { return m_value; }
    
    std::string to_string() const {
        switch (m_value) {
            case months::January: return "January";
            case months::February: return "February"; 
            case months::March: return "March";
            case months::April: return "April";
            case months::May: return "May";
            case months::June: return "June";
            case months::July: return "July";
            case months::August: return "August";
            case months::September: return "September";
            case months::October: return "October";
            case months::November: return "November";
            case months::December: return "December";
            default: return "Unknown";
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const month m) {
        os << m.to_string();
        return os;
    }

    constexpr months value() const noexcept { return m_value; }
    constexpr months& value() noexcept { return m_value; }

public:
    constexpr bool operator==(const month m) const noexcept { return to_int() == to_int(); }
    constexpr bool operator!=(const month m) const noexcept { return to_int() != to_int(); }
    constexpr bool operator<=(const month m) const noexcept { return to_int() <= m.to_int(); }
    constexpr bool operator>=(const month m) const noexcept { return to_int() >= m.to_int(); }
    constexpr bool operator<(const month m) const noexcept { return to_int() < m.to_int(); }
    constexpr bool operator>(const month m) const noexcept { return to_int() > m.to_int(); }

public:
    OPTIONAL_CPP14_CONSTEXPR month& operator++() noexcept {
        m_value = (m_value == months::December) ? months::January : static_cast<months>(to_int());
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR month operator++(int) noexcept {
        month temp(*this);
        ++(*this);
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR month& operator--() noexcept {
        m_value = (m_value == months::January) ? months::December : static_cast<months>(to_int());
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR month operator--(int) noexcept {
        month temp(*this);
        --(*this);
        return temp;
    }
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr month operator+(const T n) const noexcept {
        if (n < 0) { return *this - fizmo::abs_constexpr(n); }
        if (n == 0) return *this;
        const std::uint8_t new_month = (to_int() - 1 + (n % 12)) % 12;
        return month(static_cast<months>(new_month));
    }
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR month& operator+=(const T n) noexcept {
        *this = *this + n;
        return *this;
    }
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr month operator-(const T n) const noexcept {
        if (n < 0) { return *this + fizmo::abs_constexpr(n); }
        if (n == 0) return *this;
        const std::uint8_t new_month = (to_int() + 11 - (n % 12)) % 12;
        return month(static_cast<months>(new_month));
    }
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR month& operator-=(const T n) noexcept {
        *this = *this - n;
        return *this;
    }
    
    constexpr std::uint8_t operator-(const month m) const noexcept {
        const std::uint8_t m1 = to_int() - 1;
        const std::uint8_t m2 = m.to_int() - 1;
        return (m1 >= m2) ? (m1 - m2) : (12 + m1 - m2);
    }

public:
    constexpr explicit operator std::uint8_t() const noexcept { return to_int(); }
    constexpr explicit operator months() const noexcept { return m_value; } 

public:
    static constexpr month january() noexcept { return month(months::January); }
    static constexpr month february() noexcept { return month(months::February); }
    static constexpr month march() noexcept { return month(months::March); }
    static constexpr month april() noexcept { return month(months::April); }
    static constexpr month may() noexcept { return month(months::May); }
    static constexpr month june() noexcept { return month(months::June); }
    static constexpr month july() noexcept { return month(months::July); }
    static constexpr month august() noexcept { return month(months::August); }
    static constexpr month september() noexcept { return month(months::September); }
    static constexpr month october() noexcept { return month(months::October); }
    static constexpr month november() noexcept { return month(months::November); }
    static constexpr month december() noexcept { return month(months::December); }
};

class week {
private:
    std::uint64_t m_value;
    
public:
    constexpr week() noexcept : m_value(0) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr week(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr week(const week& w) noexcept : m_value(w.m_value) {}
    constexpr week(week&& w) noexcept : m_value(w.m_value) { w.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator=(const week& w) noexcept {
        if (this != &w) { m_value = w.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator=(week&& w) noexcept {
        if (this != &w) { 
            m_value = w.m_value; 
            w.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR week& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }

public:
    constexpr bool operator==(const week w) const noexcept { return m_value == w.m_value; }
    constexpr bool operator!=(const week w) const noexcept { return m_value != w.m_value; }
    constexpr bool operator<=(const week w) const noexcept { return m_value <= w.m_value; }
    constexpr bool operator>=(const week w) const noexcept { return m_value >= w.m_value; }
    constexpr bool operator<(const week w) const noexcept { return m_value < w.m_value; }
    constexpr bool operator>(const week w) const noexcept { return m_value > w.m_value; }
    
public:
    constexpr week operator+(const week w) const noexcept { 
        if (w.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - w.m_value) { return week(std::numeric_limits<std::uint64_t>::max()); }
        return week(m_value + w.m_value); 
    }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator+=(const week w) noexcept {
        if (w.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - w.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += w.m_value;
        }
        return *this;
    }
    
    constexpr week operator-(const week w) const noexcept { return m_value >= w.m_value ? week(m_value - w.m_value) : week(0); }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator-=(const week w) noexcept {
        if (m_value >= w.m_value) {
            m_value -= w.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR week operator++(int) noexcept {
        week temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR week& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR week operator--(int) noexcept {
        week temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }

public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const week w) {
        os << w.to_string();
        return os;
    }

    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
    constexpr explicit operator minute() const noexcept;
    constexpr explicit operator hour() const noexcept;
    constexpr explicit operator day() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

enum class weekdays : std::uint8_t {
    Monday = 0,
    Tuesday,
    Wednesday,
    Thursday,
    Friday, 
    Saturday,
    Sunday
};

class weekday {
private:
    weekdays m_value;

public:
    constexpr weekday(const weekdays wd) noexcept : m_value(wd) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr weekday(const T n) noexcept : m_value(fizmo::abs_constexpr(n) % 7) {}

    constexpr weekday(const weekday& wd) noexcept : m_value(wd.m_value) {}
    constexpr weekday(weekday&& wd) noexcept : m_value(wd.m_value) { wd.m_value = weekdays::Monday; }

    OPTIONAL_CPP14_CONSTEXPR weekday& operator=(const weekday& wd) noexcept {
        if (this != &wd) { m_value = wd.m_value; }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR weekday& operator=(weekday&& wd) noexcept {
        if (this != &wd) {
            m_value = wd.m_value;
            wd.m_value = weekdays::Monday;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR weekday& operator=(const weekdays wd) noexcept {
        if (m_value != wd) { m_value = wd; }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR weekday& operator=(const T v) noexcept {
        m_value = static_cast<weekdays>(fizmo::abs_constexpr(v) % 7);
        return *this;
    }

    constexpr weekdays value() const noexcept { return m_value; }
    constexpr weekdays& value() noexcept { return m_value; }

public:
    constexpr std::uint8_t to_int() const noexcept { return static_cast<std::uint8_t>(m_value); }
    constexpr weekdays to_enum() const noexcept { return m_value; }

    std::string to_string() const {
        switch (m_value) {
            case weekdays::Monday: return "Monday";
            case weekdays::Tuesday: return "Tuesday";
            case weekdays::Wednesday: return "Wednesday";
            case weekdays::Thursday: return "Thursday";
            case weekdays::Friday: return "Friday";
            case weekdays::Saturday: return "Saturday";
            case weekdays::Sunday: return "Sunday";
            default: return "ERROR WEEKDAY";
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const weekday w) {
        os << w.to_string();
        return os;
    }

public:
    constexpr bool operator==(const weekday m) const noexcept { return m_value == m.m_value; }
    constexpr bool operator!=(const weekday m) const noexcept { return m_value != m.m_value; }
    constexpr bool operator<=(const weekday m) const noexcept { return to_int() <= m.to_int(); }
    constexpr bool operator>=(const weekday m) const noexcept { return to_int() >= m.to_int(); }
    constexpr bool operator<(const weekday m) const noexcept { return to_int() < m.to_int(); }
    constexpr bool operator>(const weekday m) const noexcept { return to_int() > m.to_int(); }

public:
    OPTIONAL_CPP14_CONSTEXPR weekday& operator++() noexcept {
        m_value = (m_value == weekdays::Sunday) ? weekdays::Monday : static_cast<weekdays>(to_int() + 1);
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR weekday operator++(int) noexcept {
        const weekday temp(*this);
        ++(*this);
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR weekday& operator--() noexcept {
        m_value = (m_value == weekdays::Monday) ? weekdays::Sunday : static_cast<weekdays>(to_int() - 1);
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR weekday operator--(int) noexcept {
        weekday temp(*this);
        --(*this);
        return temp;
    }
    
    constexpr weekday operator+(const std::uint64_t n) const noexcept {
        if (n == 0) return *this;
        const std::uint8_t new_weekday = (to_int() + (n % 7)) % 7;
        return weekday(static_cast<weekdays>(new_weekday));
    }
    
    OPTIONAL_CPP14_CONSTEXPR weekday& operator+=(const std::uint64_t n) noexcept {
        *this = *this + n;
        return *this;
    }
    
    constexpr weekday operator-(const std::uint64_t n) const noexcept {
        if (n == 0) return *this;
        const std::uint8_t new_weekday = (to_int() + 12 - (n % 12)) % 12;
        return weekday(static_cast<weekdays>(new_weekday));
    }
    
    OPTIONAL_CPP14_CONSTEXPR weekday& operator-=(const std::uint64_t n) noexcept {
        *this = *this - n;
        return *this;
    }
    
    constexpr std::uint8_t operator-(const weekday m) const noexcept {
        const std::uint8_t m1 = to_int();
        const std::uint8_t m2 = m.to_int();
        return (m1 >= m2) ? (m1 - m2) : (7 + m1 - m2);
    }

public:
    constexpr explicit operator std::uint8_t() const noexcept { return to_int(); }
    constexpr explicit operator weekdays() const noexcept { return m_value; } 

public:
    static constexpr weekday monday() noexcept { return weekday(weekdays::Monday); }
    static constexpr weekday tuesday() noexcept { return weekday(weekdays::Tuesday); }
    static constexpr weekday wednesday() noexcept { return weekday(weekdays::Wednesday); }
    static constexpr weekday thursday() noexcept { return weekday(weekdays::Thursday); }
    static constexpr weekday friday() noexcept { return weekday(weekdays::Friday); }
    static constexpr weekday saturday() noexcept { return weekday(weekdays::Saturday); }
    static constexpr weekday sunday() noexcept { return weekday(weekdays::Sunday); }
};

class day {
private:
    std::uint64_t m_value;
    
public:
    constexpr day() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr day(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr day(const day& d) noexcept : m_value(d.m_value) {}
    constexpr day(day&& d) noexcept : m_value(d.m_value) { d.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator=(const day& d) noexcept {
        if (this != &d) { m_value = d.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator=(day&& d) noexcept {
        if (this != &d) {
            m_value = d.m_value;
            d.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR day& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const day d) const noexcept { return m_value == d.m_value; }
    constexpr bool operator!=(const day d) const noexcept { return m_value != d.m_value; }
    constexpr bool operator<=(const day d) const noexcept { return m_value <= d.m_value; }
    constexpr bool operator>=(const day d) const noexcept { return m_value >= d.m_value; }
    constexpr bool operator<(const day d) const noexcept { return m_value < d.m_value; }
    constexpr bool operator>(const day d) const noexcept { return m_value > d.m_value; }
    
public:
    constexpr day operator+(const day d) const noexcept {
        if (d.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - d.m_value) { return day(std::numeric_limits<std::uint64_t>::max()); }
        return day(m_value + d.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator+=(const day d) noexcept {
        if (d.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - d.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += d.m_value;
        }
        return *this;
    }
    
    constexpr day operator-(const day d) const noexcept { return m_value >= d.m_value ? day(m_value - d.m_value) : day(0); }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator-=(const day d) noexcept {
        if (m_value >= d.m_value) {
            m_value -= d.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR day operator++(int) noexcept {
        day temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR day& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR day operator--(int) noexcept {
        day temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }

public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const day d) {
        os << d.to_string();
        return os;
    }
    
public:
    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
    constexpr explicit operator minute() const noexcept;
    constexpr explicit operator hour() const noexcept;
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_MONTH_DAY_WEEK_CLASSES_HPP