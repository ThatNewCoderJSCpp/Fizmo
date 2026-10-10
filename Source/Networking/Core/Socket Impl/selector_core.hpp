#ifndef FIZMO_SELECTOR_CORE_HPP
#define FIZMO_SELECTOR_CORE_HPP

#include "../socket_event.hpp"
#include "native_poll.hpp"
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

class SelectorCore {
public:
    using Duration     = std::chrono::steady_clock::duration;
    using Milliseconds = std::chrono::milliseconds;

    struct SocketState {
        bool readable  = false;
        bool writable  = false;
        bool has_error = false;
    };

protected:
    enum class EntryKind { Raw, Buffered, BufferedUDP };

    struct Entry {
        EntryKind kind;
        void*     socket;
    };

    std::unordered_map<SocketBase*       , SocketEvent::Type> m_raw_sockets;
    std::unordered_map<BufferedSocket*   , SocketEvent::Type> m_buffered_sockets;
    std::unordered_map<BufferedUDPSocket*, SocketEvent::Type> m_buffered_udp_sockets;

    std::unordered_map<SocketBase*       , SocketState> m_raw_results;
    std::unordered_map<BufferedSocket*   , SocketState> m_buffered_results;
    std::unordered_map<BufferedUDPSocket*, SocketState> m_buffered_udp_results;

    mutable std::mutex m_selector_mutex;

    PollSet            m_poll_set;
    std::vector<Entry> m_poll_index;

public:
    SelectorCore()  = default;
    ~SelectorCore() = default;
    SelectorCore(const SelectorCore&) = delete;
    SelectorCore& operator=(const SelectorCore&) = delete;

public:
    void add(SocketBase* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error);

    void add(BufferedSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error);

    void add(BufferedUDPSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error);

    void modify(SocketBase* socket, SocketEvent::Type events);

    void modify(BufferedSocket* socket, SocketEvent::Type events);

    void modify(BufferedUDPSocket* socket, SocketEvent::Type events);

    void remove(SocketBase* socket);

    void remove(BufferedSocket* socket);

    void remove(BufferedUDPSocket* socket);

    void clear();

public:
    int wait(const Duration& timeout = Milliseconds(0));

public:
    bool is_readable(SocketBase* socket) const;

    bool is_writable(SocketBase* socket) const;

    bool has_error(SocketBase* socket) const;

    bool is_readable(BufferedSocket* socket) const;

    bool is_writable(BufferedSocket* socket) const;

    bool has_error(BufferedSocket* socket) const;

    bool is_readable(BufferedUDPSocket* socket) const;

    bool is_writable(BufferedUDPSocket* socket) const;

    bool has_error(BufferedUDPSocket* socket) const;

public:
    std::unordered_map<SocketBase*, SocketState> get_raw_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_raw_results;
    }

    std::unordered_map<BufferedSocket*, SocketState> get_buffered_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_buffered_results;
    }

    std::unordered_map<BufferedUDPSocket*, SocketState> get_buffered_udp_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_buffered_udp_results;
    }

    std::size_t size() const;

    bool empty() const { return size() == 0; }

private:
    static bool wants(SocketEvent::Type events, SocketEvent::Type flag) noexcept {
        return (events & flag) != SocketEvent::Type::None;
    }

    void build_poll_set();

    void harvest_results();
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SELECTOR_CORE_HPP