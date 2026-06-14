#ifndef FIZMO_SMALL_TIME_CLASSES_HPP
#define FIZMO_SMALL_TIME_CLASSES_HPP

#include "hour_minute_second.hpp"

namespace fizmo {
namespace time {

class decisecond {
private:
    std::uint64_t m_value;
    
public:
    constexpr decisecond() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr decisecond(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr decisecond(const decisecond& ds) noexcept : m_value(ds.m_value) {}
    constexpr decisecond(decisecond&& ds) noexcept : m_value(ds.m_value) { ds.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator=(const decisecond& ds) noexcept {
        if (this != &ds) { m_value = ds.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator=(decisecond&& ds) noexcept {
        if (this != &ds) {
            m_value = ds.m_value;
            ds.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const decisecond ds) const noexcept { return m_value == ds.m_value; }
    constexpr bool operator!=(const decisecond ds) const noexcept { return m_value != ds.m_value; }
    constexpr bool operator<=(const decisecond ds) const noexcept { return m_value <= ds.m_value; }
    constexpr bool operator>=(const decisecond ds) const noexcept { return m_value >= ds.m_value; }
    constexpr bool operator<(const decisecond ds) const noexcept { return m_value < ds.m_value; }
    constexpr bool operator>(const decisecond ds) const noexcept { return m_value > ds.m_value; }

    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const decisecond ds) {
        os << ds.to_string();
        return os;
    }
    
public:
    constexpr decisecond operator+(const decisecond ds) const noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) { 
            return decisecond(std::numeric_limits<std::uint64_t>::max()); 
        }
        return decisecond(m_value + ds.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator+=(const decisecond ds) noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += ds.m_value;
        }
        return *this;
    }
    
    constexpr decisecond operator-(const decisecond ds) const noexcept { return m_value >= ds.m_value ? decisecond(m_value - ds.m_value) : decisecond(0); }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator-=(const decisecond ds) noexcept {
        if (m_value >= ds.m_value) {
            m_value -= ds.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond operator++(int) noexcept {
        decisecond temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR decisecond operator--(int) noexcept {
        decisecond temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator centisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
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

class centisecond {
private:
    std::uint64_t m_value;
    
public:
    constexpr centisecond() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr centisecond(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr centisecond(const centisecond& ds) noexcept : m_value(ds.m_value) {}
    constexpr centisecond(centisecond&& ds) noexcept : m_value(ds.m_value) { ds.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator=(const centisecond& ds) noexcept {
        if (this != &ds) { m_value = ds.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator=(centisecond&& ds) noexcept {
        if (this != &ds) {
            m_value = ds.m_value;
            ds.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const centisecond ds) const noexcept { return m_value == ds.m_value; }
    constexpr bool operator!=(const centisecond ds) const noexcept { return m_value != ds.m_value; }
    constexpr bool operator<=(const centisecond ds) const noexcept { return m_value <= ds.m_value; }
    constexpr bool operator>=(const centisecond ds) const noexcept { return m_value >= ds.m_value; }
    constexpr bool operator<(const centisecond ds) const noexcept { return m_value < ds.m_value; }
    constexpr bool operator>(const centisecond ds) const noexcept { return m_value > ds.m_value; }

    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const centisecond cs) {
        os << cs.to_string();
        return os;
    }
    
public:
    constexpr centisecond operator+(const centisecond ds) const noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) { 
            return centisecond(std::numeric_limits<std::uint64_t>::max()); 
        }
        return centisecond(m_value + ds.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator+=(const centisecond ds) noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += ds.m_value;
        }
        return *this;
    }
    
    constexpr centisecond operator-(const centisecond ds) const noexcept { return m_value >= ds.m_value ? centisecond(m_value - ds.m_value) : centisecond(0); }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator-=(const centisecond ds) noexcept {
        if (m_value >= ds.m_value) {
            m_value -= ds.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond operator++(int) noexcept {
        centisecond temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR centisecond operator--(int) noexcept {
        centisecond temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
    constexpr explicit operator millisecond() const noexcept;
    constexpr explicit operator decisecond() const noexcept;
    constexpr explicit operator second() const noexcept;
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

class millisecond {
private:
    std::uint64_t m_value;
    
public:
    constexpr millisecond() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr millisecond(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr millisecond(const millisecond& ds) noexcept : m_value(ds.m_value) {}
    constexpr millisecond(millisecond&& ds) noexcept : m_value(ds.m_value) { ds.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator=(const millisecond& ds) noexcept {
        if (this != &ds) { m_value = ds.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator=(millisecond&& ds) noexcept {
        if (this != &ds) {
            m_value = ds.m_value;
            ds.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const millisecond ds) const noexcept { return m_value == ds.m_value; }
    constexpr bool operator!=(const millisecond ds) const noexcept { return m_value != ds.m_value; }
    constexpr bool operator<=(const millisecond ds) const noexcept { return m_value <= ds.m_value; }
    constexpr bool operator>=(const millisecond ds) const noexcept { return m_value >= ds.m_value; }
    constexpr bool operator<(const millisecond ds) const noexcept { return m_value < ds.m_value; }
    constexpr bool operator>(const millisecond ds) const noexcept { return m_value > ds.m_value; }

    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const millisecond ms) {
        os << ms.to_string();
        return os;
    }
    
public:
    constexpr millisecond operator+(const millisecond ds) const noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) { 
            return millisecond(std::numeric_limits<std::uint64_t>::max()); 
        }
        return millisecond(m_value + ds.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator+=(const millisecond ds) noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += ds.m_value;
        }
        return *this;
    }
    
    constexpr millisecond operator-(const millisecond ds) const noexcept { return m_value >= ds.m_value ? millisecond(m_value - ds.m_value) : millisecond(0); }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator-=(const millisecond ds) noexcept {
        if (m_value >= ds.m_value) {
            m_value -= ds.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond operator++(int) noexcept {
        millisecond temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR millisecond operator--(int) noexcept {
        millisecond temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    constexpr explicit operator nanosecond() const noexcept;
    constexpr explicit operator microsecond() const noexcept;
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
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class microsecond {
private:
    std::uint64_t m_value;
    
public:
    constexpr microsecond() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr microsecond(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr microsecond(const microsecond& ds) noexcept : m_value(ds.m_value) {}
    constexpr microsecond(microsecond&& ds) noexcept : m_value(ds.m_value) { ds.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator=(const microsecond& ds) noexcept {
        if (this != &ds) { m_value = ds.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator=(microsecond&& ds) noexcept {
        if (this != &ds) {
            m_value = ds.m_value;
            ds.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const microsecond ds) const noexcept { return m_value == ds.m_value; }
    constexpr bool operator!=(const microsecond ds) const noexcept { return m_value != ds.m_value; }
    constexpr bool operator<=(const microsecond ds) const noexcept { return m_value <= ds.m_value; }
    constexpr bool operator>=(const microsecond ds) const noexcept { return m_value >= ds.m_value; }
    constexpr bool operator<(const microsecond ds) const noexcept { return m_value < ds.m_value; }
    constexpr bool operator>(const microsecond ds) const noexcept { return m_value > ds.m_value; }

    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const microsecond us) {
        os << us.to_string();
        return os;
    }
    
public:
    constexpr microsecond operator+(const microsecond ds) const noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) { 
            return microsecond(std::numeric_limits<std::uint64_t>::max()); 
        }
        return microsecond(m_value + ds.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator+=(const microsecond ds) noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += ds.m_value;
        }
        return *this;
    }
    
    constexpr microsecond operator-(const microsecond ds) const noexcept { return m_value >= ds.m_value ? microsecond(m_value - ds.m_value) : microsecond(0); }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator-=(const microsecond ds) noexcept {
        if (m_value >= ds.m_value) {
            m_value -= ds.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond operator++(int) noexcept {
        microsecond temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR microsecond operator--(int) noexcept {
        microsecond temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
    constexpr explicit operator nanosecond() const noexcept;
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
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

class nanosecond {
private:
    std::uint64_t m_value;
    
public:
    constexpr nanosecond() noexcept : m_value(0) {}
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr nanosecond(const T v) noexcept : m_value(static_cast<std::uint64_t>(fizmo::abs_constexpr(v))) {}
    
    constexpr nanosecond(const nanosecond& ds) noexcept : m_value(ds.m_value) {}
    constexpr nanosecond(nanosecond&& ds) noexcept : m_value(ds.m_value) { ds.m_value = 0; }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator=(const nanosecond& ds) noexcept {
        if (this != &ds) { m_value = ds.m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator=(nanosecond&& ds) noexcept {
        if (this != &ds) {
            m_value = ds.m_value;
            ds.m_value = 0;
        }
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator=(const T v) noexcept {
        m_value = v;
        return *this;
    }

    constexpr std::uint64_t value() const noexcept { return m_value; }
    constexpr std::uint64_t& value() noexcept { return m_value; }
    
public:
    constexpr bool operator==(const nanosecond ds) const noexcept { return m_value == ds.m_value; }
    constexpr bool operator!=(const nanosecond ds) const noexcept { return m_value != ds.m_value; }
    constexpr bool operator<=(const nanosecond ds) const noexcept { return m_value <= ds.m_value; }
    constexpr bool operator>=(const nanosecond ds) const noexcept { return m_value >= ds.m_value; }
    constexpr bool operator<(const nanosecond ds) const noexcept { return m_value < ds.m_value; }
    constexpr bool operator>(const nanosecond ds) const noexcept { return m_value > ds.m_value; }

    std::string to_string() const { return std::to_string(m_value); }

    friend std::ostream& operator<<(std::ostream& os, const nanosecond ns) {
        os << ns.to_string();
        return os;
    }
    
public:
    constexpr nanosecond operator+(const nanosecond ds) const noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) { 
            return nanosecond(std::numeric_limits<std::uint64_t>::max()); 
        }
        return nanosecond(m_value + ds.m_value);
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator+=(const nanosecond ds) noexcept {
        if (ds.m_value > 0 && m_value > std::numeric_limits<std::uint64_t>::max() - ds.m_value) {
            m_value = std::numeric_limits<std::uint64_t>::max();
        } else {
            m_value += ds.m_value;
        }
        return *this;
    }
    
    constexpr nanosecond operator-(const nanosecond ds) const noexcept { return m_value >= ds.m_value ? nanosecond(m_value - ds.m_value) : nanosecond(0); }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator-=(const nanosecond ds) noexcept {
        if (m_value >= ds.m_value) {
            m_value -= ds.m_value;
        } else {
            m_value = 0;
        }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator++() noexcept {
        ++m_value;
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond operator++(int) noexcept {
        nanosecond temp(*this);
        ++m_value;
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond& operator--() noexcept {
        if (m_value > 0) { --m_value; }
        return *this;
    }
    
    OPTIONAL_CPP14_CONSTEXPR nanosecond operator--(int) noexcept {
        nanosecond temp(*this);
        if (m_value > 0) { --m_value; }
        return temp;
    }
    
public:
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
    constexpr explicit operator millennium() const noexcept;
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    constexpr explicit operator T() const noexcept { return static_cast<T>(m_value); }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_SMALL_TIME_CLASSES_HPP