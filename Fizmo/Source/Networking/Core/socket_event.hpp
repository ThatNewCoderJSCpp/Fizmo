#ifndef FIZMO_SOCKET_EVENT_HPP
#define FIZMO_SOCKET_EVENT_HPP

#include "buffered_socket.hpp"
#include "buffered_udp_socket.hpp"
#include <functional>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace core {

class SocketEvent {
public:
    enum class Type {
        None    = 0,
        Read    = 1 << 0,
        Write   = 1 << 1,
        Connect = 1 << 2,
        Accept  = 1 << 3,
        Close   = 1 << 4,
        Error   = 1 << 5,
        Timeout = 1 << 6
    };

    constexpr friend Type operator|(Type a, Type b) noexcept { return static_cast<Type>(static_cast<int>(a) | static_cast<int>(b)); }
    constexpr friend Type operator&(Type a, Type b) noexcept { return static_cast<Type>(static_cast<int>(a) & static_cast<int>(b)); }
    constexpr friend bool operator!(Type a) noexcept { return static_cast<int>(a) == 0; }

protected:
    Type m_event_type;
    SocketError m_error;
    SocketEvent(Type type) noexcept : m_event_type(type) {}
    SocketEvent(Type type, const SocketError& err) noexcept : m_event_type(type), m_error(err) {}

public:
    virtual ~SocketEvent() = default;
    Type event_type() const noexcept { return m_event_type; }
    const SocketError& error() const noexcept { return m_error; }
};

class RawSocketEvent : public SocketEvent {
private:
    const SocketBase& m_socket;

public:
    RawSocketEvent(const SocketBase& socket, Type type) noexcept : SocketEvent(type), m_socket(socket) {}
    RawSocketEvent(const SocketBase& socket, Type type, const SocketError& err) noexcept : SocketEvent(type, err), m_socket(socket) {}
    const SocketBase& socket() const noexcept { return m_socket; }
};

class BufferedSocketEvent : public SocketEvent {
private:
    const BufferedSocket& m_socket;

public:
    BufferedSocketEvent(const BufferedSocket& socket, Type type) noexcept : SocketEvent(type), m_socket(socket) {}
    BufferedSocketEvent(const BufferedSocket& socket, Type type, const SocketError& err) noexcept : SocketEvent(type, err), m_socket(socket) {}
    const BufferedSocket& socket() const { return m_socket; }
};

class BufferedUDPSocketEvent : public SocketEvent {
private:
    const BufferedUDPSocket& m_socket;

public:
    BufferedUDPSocketEvent(const BufferedUDPSocket& socket, Type type) noexcept : SocketEvent(type), m_socket(socket) {}
    BufferedUDPSocketEvent(const BufferedUDPSocket& socket, Type type, const SocketError& err) noexcept : SocketEvent(type, err), m_socket(socket) {}
    const BufferedUDPSocket& socket() const noexcept { return m_socket; }
};

namespace detail {

template<typename SocketType, typename EventType, typename CallbackType>
class EventHandler {
private:
    std::unordered_map<SocketType*, std::unordered_map<SocketEvent::Type, std::vector<CallbackType>>> m_callbacks;
    mutable std::mutex m_callback_mutex;

public:
    EventHandler() = default;
    virtual ~EventHandler() = default;

    void register_callback(SocketType* socket, SocketEvent::Type event_type, CallbackType callback) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_callbacks[socket][event_type].push_back(callback);
    }

    void unregister_all_callbacks(SocketType* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_callbacks.erase(socket);
    }

    void trigger_event(SocketType* socket, SocketEvent::Type event_type) {
        if (!socket) return;
        std::vector<CallbackType> cbs;

        {
            std::lock_guard<std::mutex> lock(m_callback_mutex);
            auto it = m_callbacks.find(socket);

            if (it != m_callbacks.end()) {
                for (const auto& entry : it->second) {
                    if ((entry.first & event_type) != SocketEvent::Type::None) {
                        cbs.insert(cbs.end(), entry.second.begin(), entry.second.end());
                    }
                }
            }
        }

        if (!cbs.empty()) {
            EventType event(*socket, event_type);
            for (const auto& cb : cbs) { cb(event); }
        }
    }

    void trigger_error_event(SocketType* socket, const SocketError& error) {
        if (!socket) return;
        std::vector<CallbackType> cbs;

        {
            std::lock_guard<std::mutex> lock(m_callback_mutex);
            auto it = m_callbacks.find(socket);

            if (it != m_callbacks.end()) {
                auto eit = it->second.find(SocketEvent::Type::Error);
                if (eit != it->second.end()) { cbs = eit->second; }
            }
        }

        if (!cbs.empty()) {
            EventType event(*socket, SocketEvent::Type::Error, error);
            for (const auto& cb : cbs) { cb(event); }
        }
    }

    void clear_all_callbacks() {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_callbacks.clear();
    }

    void trigger_event(const EventType& event) {
        SocketType* socket_ptr = const_cast<SocketType*>(&event.socket());
        std::vector<CallbackType> cbs;

        {
            std::lock_guard<std::mutex> lock(m_callback_mutex);
            auto it = m_callbacks.find(socket_ptr);

            if (it != m_callbacks.end()) {
                for (const auto& entry : it->second) {
                    if ((entry.first & event.event_type()) != SocketEvent::Type::None) {
                        cbs.insert(cbs.end(), entry.second.begin(), entry.second.end());
                    }
                }
            }
        }

        for (const auto& cb : cbs) { cb(event); }
    }

    bool is_monitored(SocketType* socket) const {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        return m_callbacks.find(socket) != m_callbacks.end();
    }
};

using RawSocketEventHandler      = EventHandler<SocketBase, RawSocketEvent, std::function<void(const RawSocketEvent&)>>;
using BufferedSocketEventHandler  = EventHandler<BufferedSocket, BufferedSocketEvent, std::function<void(const BufferedSocketEvent&)>>;
using BufferedUDPSocketEventHandler = EventHandler<BufferedUDPSocket, BufferedUDPSocketEvent, std::function<void(const BufferedUDPSocketEvent&)>>;

} // namespace detail

class SocketEventHandler {
private:
    detail::EventHandler<SocketBase, RawSocketEvent, std::function<void(const RawSocketEvent&)>>                        m_raw_handler;
    detail::EventHandler<BufferedSocket, BufferedSocketEvent, std::function<void(const BufferedSocketEvent&)>>          m_buffered_handler;
    detail::EventHandler<BufferedUDPSocket, BufferedUDPSocketEvent, std::function<void(const BufferedUDPSocketEvent&)>> m_buffered_udp_handler;
    mutable std::mutex m_handler_mutex;

public:
    SocketEventHandler() = default;
    virtual ~SocketEventHandler() = default;

    void register_callback(SocketBase* socket, SocketEvent::Type event_type, std::function<void(const RawSocketEvent&)> callback) { m_raw_handler.register_callback(socket, event_type, callback); }
    void register_callback(BufferedSocket* socket, SocketEvent::Type event_type, std::function<void(const BufferedSocketEvent&)> callback) { m_buffered_handler.register_callback(socket, event_type, callback); }
    void register_callback(BufferedUDPSocket* socket, SocketEvent::Type event_type, std::function<void(const BufferedUDPSocketEvent&)> callback) { m_buffered_udp_handler.register_callback(socket, event_type, callback); }

    void unregister_all_callbacks(SocketBase* socket) { m_raw_handler.unregister_all_callbacks(socket); }
    void unregister_all_callbacks(BufferedSocket* socket) { m_buffered_handler.unregister_all_callbacks(socket); }
    void unregister_all_callbacks(BufferedUDPSocket* socket) { m_buffered_udp_handler.unregister_all_callbacks(socket); }

    void trigger_event(SocketBase* socket, SocketEvent::Type event_type) { m_raw_handler.trigger_event(socket, event_type); }
    void trigger_event(BufferedSocket* socket, SocketEvent::Type event_type) { m_buffered_handler.trigger_event(socket, event_type); }
    void trigger_event(BufferedUDPSocket* socket, SocketEvent::Type event_type) { m_buffered_udp_handler.trigger_event(socket, event_type); }

    void trigger_error_event(SocketBase* socket, const SocketError& error) { m_raw_handler.trigger_error_event(socket, error); }
    void trigger_error_event(BufferedSocket* socket, const SocketError& error) { m_buffered_handler.trigger_error_event(socket, error); }
    void trigger_error_event(BufferedUDPSocket* socket, const SocketError& error) { m_buffered_udp_handler.trigger_error_event(socket, error); }

    void trigger_event(const RawSocketEvent& event) { m_raw_handler.trigger_event(event); }
    void trigger_event(const BufferedSocketEvent& event) { m_buffered_handler.trigger_event(event); }
    void trigger_event(const BufferedUDPSocketEvent& event) { m_buffered_udp_handler.trigger_event(event); }

    bool is_monitored(SocketBase* socket) const { return m_raw_handler.is_monitored(socket); }
    bool is_monitored(BufferedSocket* socket) const { return m_buffered_handler.is_monitored(socket); }
    bool is_monitored(BufferedUDPSocket* socket) const { return m_buffered_udp_handler.is_monitored(socket); }

    void clear_all_callbacks() {
        std::lock_guard<std::mutex> lock(m_handler_mutex);
        m_raw_handler.clear_all_callbacks();
        m_buffered_handler.clear_all_callbacks();
        m_buffered_udp_handler.clear_all_callbacks();
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_EVENT_HPP