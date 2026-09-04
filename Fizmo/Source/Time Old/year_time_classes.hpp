#ifndef FIZMO_YEAR_TIME_CLASS_HPP
#define FIZMO_YEAR_TIME_CLASS_HPP

#include "../Basic/fizmo_defines.hpp"
#include "../Standard Overloads/abs.hpp"
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace time {

class millennium;
class century;
class decade;
class year;
class week;
class day;
class hour;
class minute;
class second;
class decisecond;
class centisecond;
class millisecond;
class microsecond;
class nanosecond;

class millennium {
private:
    std::uint64_t m_value;
    
public:
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr millennium(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}

    constexpr millennium() noexcept : m_value(0) {}
    constexpr millennium(const millennium& m) noexcept : m_value(m.m_value) {}
    constexpr millennium(millennium&& m) noexcept : m_value(m.m_value) { m.m_value = 0; }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator=(const millennium& m) noexcept {
        if (this != &m) { m_value = m.m_value; }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator=(millennium&& m) noexcept {
        if (this != &m) { 
            m_value = m.m_value; 
            m.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR millennium& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }

public:
    constexpr bool operator==(const millennium m) const noexcept { return m_value == m.m_value; }
    constexpr bool operator!=(const millennium m) const noexcept { return m_value != m.m_value; }
    constexpr bool operator<=(const millennium m) const noexcept { return m_value <= m.m_value; }
    constexpr bool operator>=(const millennium m) const noexcept { return m_value >= m.m_value; }
    constexpr bool operator<(const millennium m) const noexcept { return m_value < m.m_value; }
    constexpr bool operator>(const millennium m) const noexcept { return m_value > m.m_value; }

public:
    constexpr millennium operator+(const millennium m) const noexcept { 
        if (m.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - m.m_value) { return millennium(std::numeric_limits<std::uint64_t>::max()); }
        return millennium(m_value + m.m_value); 
    }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator+=(const millennium m) noexcept {
        if (m.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - m.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += m.m_value;
        }
        return *this;
    }

    constexpr millennium operator-(const millennium m) const noexcept { return m_value >= m.m_value ? millennium(m_value - m.m_value) : millennium(0); }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator-=(const millennium m) noexcept {
        if (m_value >= m.m_value) {
            m_value -= m.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator++() noexcept {
        ++m_value;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR millennium operator++(int) noexcept {
        millennium temp(*this);
        ++m_value;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR millennium& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR millennium operator--(int) noexcept {
        millennium temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }

public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const millennium m) {
        os << m.to_string();
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
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class century {
private:
    std::uint64_t m_value;
    
public:
    constexpr century() noexcept : m_value(0) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr century(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr century(const century& c) noexcept : m_value(c.m_value) {}
    constexpr century(century&& c) noexcept : m_value(c.m_value) { c.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator=(const century& c) noexcept {
        if (this != &c) { m_value = c.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator=(century&& c) noexcept {
        if (this != &c) { 
            m_value = c.m_value; 
            c.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR century& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }

public:
    constexpr bool operator==(const century c) const noexcept { return m_value == c.m_value; }
    constexpr bool operator!=(const century c) const noexcept { return m_value != c.m_value; }
    constexpr bool operator<=(const century c) const noexcept { return m_value <= c.m_value; }
    constexpr bool operator>=(const century c) const noexcept { return m_value >= c.m_value; }
    constexpr bool operator<(const century c) const noexcept { return m_value < c.m_value; }
    constexpr bool operator>(const century c) const noexcept { return m_value > c.m_value; }
    
public:
    constexpr century operator+(const century c) const noexcept { 
        if (c.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - c.m_value) { return century(std::numeric_limits<std::uint64_t>::max()); }
        return century(m_value + c.m_value); 
    }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator+=(const century c) noexcept {
        if (c.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - c.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += c.m_value;
        }
        return *this;
    }
    
    constexpr century operator-(const century c) const noexcept { return m_value >= c.m_value ? century(m_value - c.m_value) : century(0); }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator-=(const century c) noexcept {
        if (m_value >= c.m_value) {
            m_value -= c.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR century operator++(int) noexcept {
        century temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR century& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR century operator--(int) noexcept {
        century temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }

public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const century c) {
        os << c.to_string();
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
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class decade {
private:
    std::uint64_t m_value;
    
public:
    constexpr decade() noexcept : m_value(0) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr decade(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr decade(const decade& d) noexcept : m_value(d.m_value) {}
    constexpr decade(decade&& d) noexcept : m_value(d.m_value) { d.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator=(const decade& d) noexcept {
        if (this != &d) { m_value = d.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator=(decade&& d) noexcept {
        if (this != &d) { 
            m_value = d.m_value; 
            d.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR decade& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }

public:
    constexpr bool operator==(const decade d) const noexcept { return m_value == d.m_value; }
    constexpr bool operator!=(const decade d) const noexcept { return m_value != d.m_value; }
    constexpr bool operator<=(const decade d) const noexcept { return m_value <= d.m_value; }
    constexpr bool operator>=(const decade d) const noexcept { return m_value >= d.m_value; }
    constexpr bool operator<(const decade d) const noexcept { return m_value < d.m_value; }
    constexpr bool operator>(const decade d) const noexcept { return m_value > d.m_value; }
    
public:
    constexpr decade operator+(const decade d) const noexcept { 
        if (d.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - d.m_value) { return decade(std::numeric_limits<std::uint64_t>::max()); }
        return decade(m_value + d.m_value); 
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator+=(const decade d) noexcept {
        if (d.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - d.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += d.m_value;
        }
        return *this;
    }
    
    constexpr decade operator-(const decade d) const noexcept { return m_value >= d.m_value ? decade(m_value - d.m_value) : decade(0); }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator-=(const decade d) noexcept {
        if (m_value >= d.m_value) {
            m_value -= d.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade operator++(int) noexcept {
        decade temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decade operator--(int) noexcept {
        decade temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }

public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const decade d) {
        os << d.to_string();
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
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class year {
private:
    std::uint64_t m_value;
    
public:
    constexpr year() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr year(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr year(const year& y) noexcept : m_value(y.m_value) {}
    constexpr year(year&& y) noexcept : m_value(y.m_value) { y.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator=(const year& y) noexcept {
        if (this != &y) { m_value = y.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator=(year&& y) noexcept {
        if (this != &y) {
            m_value = y.m_value;
            y.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR year& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const year y) const noexcept { return m_value == y.m_value; }
    constexpr bool operator!=(const year y) const noexcept { return m_value != y.m_value; }
    constexpr bool operator<=(const year y) const noexcept { return m_value <= y.m_value; }
    constexpr bool operator>=(const year y) const noexcept { return m_value >= y.m_value; }
    constexpr bool operator<(const year y) const noexcept { return m_value < y.m_value; }
    constexpr bool operator>(const year y) const noexcept { return m_value > y.m_value; }
    
public:
    constexpr year operator+(const year y) const noexcept {
        if (y.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - y.m_value) { return year(std::numeric_limits<std::uint64_t>::max()); }
        return year(m_value + y.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator+=(const year y) noexcept {
        if (y.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - y.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += y.m_value;
        }
        return *this;
    }
    
    constexpr year operator-(const year y) const noexcept { return m_value >= y.m_value ? year(m_value - y.m_value) : year(0); }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator-=(const year y) noexcept {
        if (m_value >= y.m_value) {
            m_value -= y.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR year operator++(int) noexcept {
        year temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR year& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR year operator--(int) noexcept {
        year temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const year y) {
        os << y.to_string();
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
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_YEAR_TIME_CLASS_HPP