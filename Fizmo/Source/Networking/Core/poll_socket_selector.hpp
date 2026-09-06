#ifndef FIZMO_POLL_SOCKET_SELECTOR_HPP
#define FIZMO_POLL_SOCKET_SELECTOR_HPP

#include "Socket Impl/selector_core.hpp"

namespace fizmo {
namespace networking {
namespace core {

class PollSocketSelector : public detail::SelectorCore {
public:
    using Duration     = detail::SelectorCore::Duration;
    using Milliseconds = detail::SelectorCore::Milliseconds;
    using SocketState  = detail::SelectorCore::SocketState;

public:
    PollSocketSelector()  = default;
    ~PollSocketSelector() = default;
    PollSocketSelector(const PollSocketSelector&) = delete;
    PollSocketSelector& operator=(const PollSocketSelector&) = delete;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_POLL_SOCKET_SELECTOR_HPP