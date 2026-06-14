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
    
    SocketState& operator=(const SocketState& ss) noexcept {
        if (this != &ss) { m_state.store(ss.m_state.load(std::memory_order_acquire), std::memory_order_release); }
        return *this;
    }

    SocketState& operator=(SocketState&& ss) noexcept {
        if (this != &ss) { m_state.store(ss.m_state.load(std::memory_order_acquire), std::memory_order_release); }
        return *this;
    }

    SocketState& operator=(State state) noexcept {
        m_state.store(state, std::memory_order_release);
        return *this;
    }

public:
    static std::string socket_state_to_string(State state) noexcept {
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

public:
    State state() const noexcept { return m_state.load(std::memory_order_acquire); }

public:
    bool can_send() const noexcept { return m_state.load(std::memory_order_acquire) == State::Connected; }
    bool can_receive() const noexcept { return m_state.load(std::memory_order_acquire) == State::Connected; }
    bool can_accept() const noexcept { return m_state.load(std::memory_order_acquire) == State::Listening; } 

    bool is_operational() const noexcept {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Listening || s == State::Connected || s == State::Bound;
    }

    bool is_error() const noexcept { return m_state.load(std::memory_order_acquire) == State::Error; }
    bool is_initialized() const noexcept { return m_state.load(std::memory_order_acquire) != State::Uninitialized; }

    bool can_sendto() const noexcept {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Initialized || s == State::Bound || s == State::Connected;
    }

    bool can_recvfrom() const noexcept {
        State s = m_state.load(std::memory_order_acquire);
        return s == State::Initialized || s == State::Bound || s == State::Connected;
    }

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