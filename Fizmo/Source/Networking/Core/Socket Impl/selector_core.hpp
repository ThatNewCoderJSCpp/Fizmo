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
    void add(SocketBase* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets[socket] = events;
    }

    void add(BufferedSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets[socket] = events;
    }

    void add(BufferedUDPSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets[socket] = events;
    }

    void modify(SocketBase* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_sockets.find(socket);
        if (it != m_raw_sockets.end()) { it->second = events; }
    }

    void modify(BufferedSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_sockets.find(socket);
        if (it != m_buffered_sockets.end()) { it->second = events; }
    }

    void modify(BufferedUDPSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_sockets.find(socket);
        if (it != m_buffered_udp_sockets.end()) { it->second = events; }
    }

    void remove(SocketBase* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets.erase(socket);
        m_raw_results.erase(socket);
    }

    void remove(BufferedSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets.erase(socket);
        m_buffered_results.erase(socket);
    }

    void remove(BufferedUDPSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets.erase(socket);
        m_buffered_udp_results.erase(socket);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets.clear();
        m_buffered_sockets.clear();
        m_buffered_udp_sockets.clear();
        m_raw_results.clear();
        m_buffered_results.clear();
        m_buffered_udp_results.clear();
        m_poll_set.clear();
        m_poll_index.clear();
    }

public:
    int wait(const Duration& timeout = Milliseconds(0)) {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_results.clear();
        m_buffered_results.clear();
        m_buffered_udp_results.clear();
        build_poll_set();
        if (m_poll_set.empty()) return 0;
        int result = m_poll_set.wait(PollSet::duration_to_ms(timeout));
        if (result > 0) { harvest_results(); }
        return result;
    }

public:
    bool is_readable(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.readable);
    }

    bool is_writable(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.writable);
    }

    bool has_error(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.has_error);
    }

    bool is_readable(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.readable);
    }

    bool is_writable(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.writable);
    }

    bool has_error(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.has_error);
    }

    bool is_readable(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.readable);
    }

    bool is_writable(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.writable);
    }

    bool has_error(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.has_error);
    }

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

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_raw_sockets.size() + m_buffered_sockets.size() + m_buffered_udp_sockets.size();
    }

    bool empty() const { return size() == 0; }

private:
    static bool wants(SocketEvent::Type events, SocketEvent::Type flag) noexcept {
        return (events & flag) != SocketEvent::Type::None;
    }

    void build_poll_set() {
        m_poll_set.clear();
        m_poll_index.clear();
        const std::size_t total = m_raw_sockets.size() + m_buffered_sockets.size() + m_buffered_udp_sockets.size();
        m_poll_set.reserve(total);
        m_poll_index.reserve(total);

        for (const auto& entry : m_raw_sockets) {
            const bool want_read  = wants(entry.second, SocketEvent::Type::Read);
            const bool want_write = wants(entry.second, SocketEvent::Type::Write);
            
            if (m_poll_set.add(entry.first->get_raw_socket(), want_read, want_write)) {
                m_poll_index.push_back(Entry{EntryKind::Raw, entry.first});
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            const bool want_read  = wants(entry.second, SocketEvent::Type::Read);
            const bool want_write = wants(entry.second, SocketEvent::Type::Write) && entry.first->has_data_to_send();
            
            if (m_poll_set.add(entry.first->socket().get_raw_socket(), want_read, want_write)) {
                m_poll_index.push_back(Entry{EntryKind::Buffered, entry.first});
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            const bool want_read  = wants(entry.second, SocketEvent::Type::Read);
            const bool want_write = wants(entry.second, SocketEvent::Type::Write) && entry.first->has_data_to_send();
            
            if (m_poll_set.add(entry.first->socket().get_raw_socket(), want_read, want_write)) {
                m_poll_index.push_back(Entry{EntryKind::BufferedUDP, entry.first});
            }
        }
    }

    void harvest_results() {
        for (std::size_t i = 0; i < m_poll_index.size(); ++i) {
            if (!m_poll_set.signalled(i)) continue;
            SocketState state;
            state.readable  = m_poll_set.readable(i);
            state.writable  = m_poll_set.writable(i);
            state.has_error = m_poll_set.errored(i);
            if (!state.readable && !state.writable && !state.has_error) continue;

            switch (m_poll_index[i].kind) {
                case EntryKind::Raw:
                    m_raw_results[static_cast<SocketBase*>(m_poll_index[i].socket)] = state;
                    break;
                case EntryKind::Buffered:
                    m_buffered_results[static_cast<BufferedSocket*>(m_poll_index[i].socket)] = state;
                    break;
                case EntryKind::BufferedUDP:
                    m_buffered_udp_results[static_cast<BufferedUDPSocket*>(m_poll_index[i].socket)] = state;
                    break;
            }
        }
    }
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SELECTOR_CORE_HPP