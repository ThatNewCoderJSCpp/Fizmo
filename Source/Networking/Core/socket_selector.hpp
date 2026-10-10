#ifndef FIZMO_SOCKET_SELECTOR_HPP
#define FIZMO_SOCKET_SELECTOR_HPP

#include "Socket Impl/selector_core.hpp"

namespace fizmo {
namespace networking {
namespace core {

class SocketSelector : public detail::SelectorCore {
public:
    using Duration     = detail::SelectorCore::Duration;
    using Milliseconds = detail::SelectorCore::Milliseconds;
    using SocketState  = detail::SelectorCore::SocketState;

public:
    SocketSelector()  = default;
    ~SocketSelector() = default;
    SocketSelector(const SocketSelector&) = delete;
    SocketSelector& operator=(const SocketSelector&) = delete;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_SELECTOR_HPP