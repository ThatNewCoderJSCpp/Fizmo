#include "fizmo_library.hpp"
#include "socket_impl_base.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

SocketImplBase::SocketImplBase(SocketType type, AddressFamily family) noexcept : m_connected(false), m_type(type), m_family(family),
          m_connect_timeout_ms(0), m_send_timeout_ms(0), m_receive_timeout_ms(0) {}

SocketImplBase::SocketImplBase(SocketType type, AddressFamily family, bool connected) noexcept : m_connected(connected), m_type(type), m_family(family),
          m_connect_timeout_ms(0), m_send_timeout_ms(0), m_receive_timeout_ms(0) {}

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
