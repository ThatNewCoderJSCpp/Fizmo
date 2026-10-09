#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "socket_state.hpp"

namespace fizmo {
namespace networking {
namespace core {

auto SocketState::operator=(const SocketState& ss) noexcept -> SocketState& {
        if (this != &ss) { m_state.store(ss.m_state.load(std::memory_order_acquire), std::memory_order_release); }
        return *this;
    }

auto SocketState::operator=(SocketState&& ss) noexcept -> SocketState& {
        if (this != &ss) { m_state.store(ss.m_state.load(std::memory_order_acquire), std::memory_order_release); }
        return *this;
    }

auto SocketState::socket_state_to_string(State state) noexcept -> std::string {
        switch (state) {
            case State::Uninitialized: return "Uninitialized";
            case State::Initialized: return "Initialized";
            case State::Connecting: return "Connecting";
            case State::Connected: return "Connected";
            case State::Binding: return "Binding";
            case State::Bound: return "Bound";
            case State::Listening: return "Listening";
            case State::Accepting: return "Accepting";
            case State::Closing: return "Closing";
            case State::Closed: return "Closed";
            case State::Error: return "Error";
            default: return "Unknown";
        }
    }

auto SocketState::is_operational() const noexcept -> bool {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Listening || s == State::Connected || s == State::Bound;
    }

auto SocketState::can_sendto() const noexcept -> bool {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Initialized || s == State::Bound || s == State::Connected;
    }

auto SocketState::can_recvfrom() const noexcept -> bool {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Initialized || s == State::Bound || s == State::Connected;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
