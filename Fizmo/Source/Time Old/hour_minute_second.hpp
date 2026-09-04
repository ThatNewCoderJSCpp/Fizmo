#ifndef FIZMO_HOUR_MINUTE_SECOND_CLASSES_HPP
#define FIZMO_HOUR_MINUTE_SECOND_CLASSES_HPP

#include "month_day_week.hpp"

namespace fizmo {
namespace time {

class hour {
private:
    std::uint64_t m_value;
    
public:
    constexpr hour() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr hour(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr hour(const hour& h) noexcept : m_value(h.m_value) {}
    constexpr hour(hour&& h) noexcept : m_value(h.m_value) { h.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator=(const hour& h) noexcept {
        if (this != &h) { m_value = h.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator=(hour&& h) noexcept {
        if (this != &h) {
            m_value = h.m_value;
            h.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR hour& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const hour h) const noexcept { return m_value == h.m_value; }
    constexpr bool operator!=(const hour h) const noexcept { return m_value != h.m_value; }
    constexpr bool operator<=(const hour h) const noexcept { return m_value <= h.m_value; }
    constexpr bool operator>=(const hour h) const noexcept { return m_value >= h.m_value; }
    constexpr bool operator<(const hour h) const noexcept { return m_value < h.m_value; }
    constexpr bool operator>(const hour h) const noexcept { return m_value > h.m_value; }
    
public:
    constexpr hour operator+(const hour h) const noexcept {
        if (h.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - h.m_value) { 
            return hour(std::numeric_limits<std::uint64_t>::max()); 
        }
        return hour(m_value + h.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator+=(const hour h) noexcept {
        if (h.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - h.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += h.m_value;
        }
        return *this;
    }
    
    constexpr hour operator-(const hour h) const noexcept { return m_value >= h.m_value ? hour(m_value - h.m_value) : hour(0); }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator-=(const hour h) noexcept {
        if (m_value >= h.m_value) {
            m_value -= h.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour operator++(int) noexcept {
        hour temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR hour operator--(int) noexcept {
        hour temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const hour h) {
        os << h.to_string();
        return os;
    }

    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
    constexpr explicit operator minute() const noexcept;
    constexpr explicit operator day() const noexcept;
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class minute {
private:
    std::uint64_t m_value;
    
public:
    constexpr minute() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr minute(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr minute(const minute& m) noexcept : m_value(m.m_value) {}
    constexpr minute(minute&& m) noexcept : m_value(m.m_value) { m.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator=(const minute& m) noexcept {
        if (this != &m) { m_value = m.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator=(minute&& m) noexcept {
        if (this != &m) {
            m_value = m.m_value;
            m.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR minute& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const minute m) const noexcept { return m_value == m.m_value; }
    constexpr bool operator!=(const minute m) const noexcept { return m_value != m.m_value; }
    constexpr bool operator<=(const minute m) const noexcept { return m_value <= m.m_value; }
    constexpr bool operator>=(const minute m) const noexcept { return m_value >= m.m_value; }
    constexpr bool operator<(const minute m) const noexcept { return m_value < m.m_value; }
    constexpr bool operator>(const minute m) const noexcept { return m_value > m.m_value; }
    
public:
    constexpr minute operator+(const minute m) const noexcept {
        if (m.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - m.m_value) { 
            return minute(std::numeric_limits<std::uint64_t>::max()); 
        }
        return minute(m_value + m.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator+=(const minute m) noexcept {
        if (m.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - m.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += m.m_value;
        }
        return *this;
    }
    
    constexpr minute operator-(const minute m) const noexcept { return m_value >= m.m_value ? minute(m_value - m.m_value) : minute(0); }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator-=(const minute m) noexcept {
        if (m_value >= m.m_value) {
            m_value -= m.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute operator++(int) noexcept {
        minute temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR minute operator--(int) noexcept {
        minute temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const minute m) {
        os << m.to_string();
        return os;
    }

   constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
    constexpr explicit operator hour() const noexcept;
    constexpr explicit operator day() const noexcept;
    constexpr explicit operator week() const noexcept;
    constexpr explicit operator year() const noexcept;
    constexpr explicit operator decade() const noexcept;
    constexpr explicit operator century() const noexcept;
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class second {
private:
    std::uint64_t m_value;
    
public:
    constexpr second() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr second(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr second(const second& s) noexcept : m_value(s.m_value) {}
    constexpr second(second&& s) noexcept : m_value(s.m_value) { s.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator=(const second& s) noexcept {
        if (this != &s) { m_value = s.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator=(second&& s) noexcept {
        if (this != &s) {
            m_value = s.m_value;
            s.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR second& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const second s) const noexcept { return m_value == s.m_value; }
    constexpr bool operator!=(const second s) const noexcept { return m_value != s.m_value; }
    constexpr bool operator<=(const second s) const noexcept { return m_value <= s.m_value; }
    constexpr bool operator>=(const second s) const noexcept { return m_value >= s.m_value; }
    constexpr bool operator<(const second s) const noexcept { return m_value < s.m_value; }
    constexpr bool operator>(const second s) const noexcept { return m_value > s.m_value; }
    
public:
    constexpr second operator+(const second s) const noexcept {
        if (s.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - s.m_value) { 
            return second(std::numeric_limits<std::uint64_t>::max()); 
        }
        return second(m_value + s.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator+=(const second s) noexcept {
        if (s.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - s.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += s.m_value;
        }
        return *this;
    }
    
    constexpr second operator-(const second s) const noexcept { return m_value >= s.m_value ? second(m_value - s.m_value) : second(0); }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator-=(const second s) noexcept {
        if (m_value >= s.m_value) {
            m_value -= s.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR second operator++(int) noexcept {
        second temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR second& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR second operator--(int) noexcept {
        second temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const second s) {
        os << s.to_string();
        return os;
    }

    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator minute() const noexcept;
    constexpr explicit operator hour() const noexcept;
    constexpr explicit operator day() const noexcept;
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

#endif // FIZMO_HOUR_MINUTE_SECOND_CLASSES_HPP