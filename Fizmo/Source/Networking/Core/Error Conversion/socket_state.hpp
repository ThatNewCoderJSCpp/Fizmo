#ifndef FIZMO_SOCKET_STATE_HPP
#define FIZMO_SOCKET_STATE_HPP

#include <string>
#include <ostream>
#include <atomic>
#include "../../Basic/fizmo_defines.hpp"

namespace fizmo {
namespace networking {
namespace core {

class SocketState {
public:
    enum class State {
        Uninitialized = 0, 
        Initialized,    
        Connecting,
        Connected,
        Binding,
        Bound,
        Listening,
        Accepting,
        Closing,        
        Closed,         
        Error           
    };

    SocketState() noexcept : m_state(State::Uninitialized) {}
    SocketState(State state) noexcept : m_state(state) {}
    SocketState(const SocketState& ss) noexcept : m_state(ss.m_state.load(std::memory_order_acquire)) {}
    SocketState(SocketState&& ss) noexcept : m_state(ss.m_state.load(std::memory_order_acquire)) {}
    
    SocketState& operator=(const SocketState& ss) noexcept;

    SocketState& operator=(SocketState&& ss) noexcept;

    SocketState& operator=(State state) noexcept {
        m_state.store(state, std::memory_order_release);
        return *this;
    }

public:
    static std::string socket_state_to_string(State state) noexcept;

public:
    State state() const noexcept { return m_state.load(std::memory_order_acquire); }

public:
    bool can_send() const noexcept { return m_state.load(std::memory_order_acquire) == State::Connected; }
    bool can_receive() const noexcept { return m_state.load(std::memory_order_acquire) == State::Connected; }
    bool can_accept() const noexcept { return m_state.load(std::memory_order_acquire) == State::Listening; } 

    bool is_operational() const noexcept;

    bool is_error() const noexcept { return m_state.load(std::memory_order_acquire) == State::Error; }
    bool is_initialized() const noexcept { return m_state.load(std::memory_order_acquire) != State::Uninitialized; }

    bool can_sendto() const noexcept;

    bool can_recvfrom() const noexcept;

    operator bool() const noexcept { return is_operational(); }
    bool operator!() const noexcept { return !is_operational(); }

public:
    bool operator==(SocketState other) const noexcept { return state() == other.state(); }
    bool operator!=(SocketState other) const noexcept { return state() != other.state(); }
    bool operator==(State s) const noexcept { return state() == s; }
    bool operator!=(State s) const noexcept { return state() != s; }

public:
    std::string to_string() const noexcept { return socket_state_to_string(state()); }
    
    friend std::ostream& operator<<(std::ostream& os, const SocketState& ss) {
        os << SocketState::socket_state_to_string(ss.state());
        return os;
    }

private:
    std::atomic<State> m_state;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_STATE_HPP