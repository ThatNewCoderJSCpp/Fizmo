#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "selector_core.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

auto SelectorCore::add(SocketBase* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets[socket] = events;
    }

auto SelectorCore::add(BufferedSocket* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets[socket] = events;
    }

auto SelectorCore::add(BufferedUDPSocket* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets[socket] = events;
    }

auto SelectorCore::modify(SocketBase* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_sockets.find(socket);
        if (it != m_raw_sockets.end()) { it->second = events; }
    }

auto SelectorCore::modify(BufferedSocket* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_sockets.find(socket);
        if (it != m_buffered_sockets.end()) { it->second = events; }
    }

auto SelectorCore::modify(BufferedUDPSocket* socket, SocketEvent::Type events) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_sockets.find(socket);
        if (it != m_buffered_udp_sockets.end()) { it->second = events; }
    }

auto SelectorCore::remove(SocketBase* socket) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets.erase(socket);
        m_raw_results.erase(socket);
    }

auto SelectorCore::remove(BufferedSocket* socket) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets.erase(socket);
        m_buffered_results.erase(socket);
    }

auto SelectorCore::remove(BufferedUDPSocket* socket) -> void {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets.erase(socket);
        m_buffered_udp_results.erase(socket);
    }

auto SelectorCore::clear() -> void {
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

auto SelectorCore::wait(const Duration& timeout) -> int {
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

auto SelectorCore::is_readable(SocketBase* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.readable);
    }

auto SelectorCore::is_writable(SocketBase* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.writable);
    }

auto SelectorCore::has_error(SocketBase* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.has_error);
    }

auto SelectorCore::is_readable(BufferedSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.readable);
    }

auto SelectorCore::is_writable(BufferedSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.writable);
    }

auto SelectorCore::has_error(BufferedSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.has_error);
    }

auto SelectorCore::is_readable(BufferedUDPSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.readable);
    }

auto SelectorCore::is_writable(BufferedUDPSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.writable);
    }

auto SelectorCore::has_error(BufferedUDPSocket* socket) const -> bool {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.has_error);
    }

auto SelectorCore::size() const -> std::size_t {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_raw_sockets.size() + m_buffered_sockets.size() + m_buffered_udp_sockets.size();
    }

auto SelectorCore::build_poll_set() -> void {
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

auto SelectorCore::harvest_results() -> void {
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

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
