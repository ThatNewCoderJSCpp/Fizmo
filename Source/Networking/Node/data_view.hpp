#ifndef FIZMO_NODE_DATA_VIEW_HPP
#define FIZMO_NODE_DATA_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace networking {

using Bytes = std::vector<std::uint8_t>;

class DataView {
public:
    DataView() noexcept = default;
    DataView(const void* data, std::size_t size) noexcept : m_data(static_cast<const std::uint8_t*>(data)), m_size(data ? size : 0) {}
    DataView(const char* text) noexcept : m_data(reinterpret_cast<const std::uint8_t*>(text)), m_size(text ? std::strlen(text) : 0) {}
    DataView(const std::string& s) noexcept : m_data(reinterpret_cast<const std::uint8_t*>(s.data())), m_size(s.size()) {}
    DataView(std::string_view s) noexcept : m_data(reinterpret_cast<const std::uint8_t*>(s.data())), m_size(s.size()) {}
    DataView(const Bytes& v) noexcept : m_data(v.data()), m_size(v.size()) {}

    const std::uint8_t* data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }
    std::size_t length() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }

    const std::uint8_t* begin() const noexcept { return m_data; }
    const std::uint8_t* end() const noexcept { return m_data + m_size; }
    std::uint8_t operator[](std::size_t i) const noexcept { return m_data[i]; }

    std::string_view view() const noexcept { return std::string_view(reinterpret_cast<const char*>(m_data), m_size); }
    std::string str() const { return std::string(reinterpret_cast<const char*>(m_data), m_size); }
    std::string to_string() const { return str(); }
    Bytes bytes() const { return Bytes(m_data, m_data + m_size); }

    DataView slice(std::size_t offset, std::size_t count = static_cast<std::size_t>(-1)) const noexcept {
        if (offset >= m_size) return DataView();
        const std::size_t n = (count > m_size - offset) ? (m_size - offset) : count;
        return DataView(m_data + offset, n);
    }

    friend bool operator==(const DataView& a, const DataView& b) noexcept {
        return a.m_size == b.m_size && (a.m_size == 0 || std::memcmp(a.m_data, b.m_data, a.m_size) == 0);
    }

    friend bool operator!=(const DataView& a, const DataView& b) noexcept { return !(a == b); }

private:
    const std::uint8_t* m_data = nullptr;
    std::size_t         m_size = 0;
};

} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_DATA_VIEW_HPP
