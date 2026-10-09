#include "fizmo_library.hpp"
#include "datagram.hpp"

namespace fizmo {
namespace networking {
namespace core {

Datagram::Datagram(const void* buf, std::size_t len, const NetworkAddress& addr) : data(static_cast<const std::uint8_t*>(buf), static_cast<const std::uint8_t*>(buf) + len), address(addr) {}

} // namespace core
} // namespace networking
} // namespace fizmo
