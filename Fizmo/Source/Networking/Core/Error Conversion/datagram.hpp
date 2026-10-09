#ifndef FIZMO_DATAGRAM_HPP
#define FIZMO_DATAGRAM_HPP

#include "addresses.hpp"
#include <vector>
#include <cstring>

namespace fizmo {
namespace networking {
namespace core {

struct Datagram {
    std::vector<std::uint8_t> data;
    NetworkAddress address;

    Datagram() = default;
    Datagram(const void* buf, std::size_t len, const NetworkAddress& addr);
    Datagram(std::vector<std::uint8_t> d, const NetworkAddress& addr) : data(std::move(d)), address(addr) {}
    Datagram(const std::string& str, const NetworkAddress& addr) : data(str.begin(), str.end()), address(addr) {}
    Datagram(const Datagram& other) = default;
    Datagram(Datagram&& other) noexcept = default;
    Datagram& operator=(const Datagram& other) = default;
    Datagram& operator=(Datagram&& other) noexcept = default;

    std::size_t size() const noexcept { return data.size(); }
    bool empty() const noexcept { return data.empty(); }
    const std::uint8_t* raw() const noexcept { return data.data(); }
    std::uint8_t* raw() noexcept { return data.data(); }
    std::string to_string() const { return std::string(reinterpret_cast<const char*>(data.data()), data.size()); }

    void append(const void* buf, std::size_t len) {
        const auto* p = static_cast<const std::uint8_t*>(buf);
        data.insert(data.end(), p, p + len);
    }

    void clear() noexcept { data.clear(); }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_DATAGRAM_HPP