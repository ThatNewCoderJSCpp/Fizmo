#ifndef FIZMO_GUARD_EVENT_STREAM_HPP
#define FIZMO_GUARD_EVENT_STREAM_HPP

#include "guard_types.hpp"
#include "../Node/http.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct ServerEvent {
    std::string id;
    std::string event;
    std::string data;
    std::chrono::milliseconds retry{0};

    std::string serialize() const;
};

class EventStream {
public:
    EventStream() noexcept = default;

    static EventStream open(http::IncomingMessage& req, http::ServerResponse& res, std::chrono::milliseconds heartbeat = std::chrono::seconds(15));

    bool send(const ServerEvent& event);
    bool send(const std::string& event, const std::string& data, const std::string& id = {});
    bool comment(const std::string& text);
    void close();
    bool closed() const;
    std::string last_event_id() const;
    std::uint64_t id() const noexcept;
    void on_close(std::function<void()> fn);

    explicit operator bool() const noexcept { return m_state != nullptr; }

    struct State;

private:
    explicit EventStream(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    std::shared_ptr<State> m_state;
};

class Broadcaster {
public:
    explicit Broadcaster(std::size_t replay = 100);

    void subscribe(const std::string& topic, EventStream stream);
    void unsubscribe(const std::string& topic, const EventStream& stream);
    std::size_t publish(const std::string& topic, const std::string& event, const std::string& data);
    std::size_t subscribers(const std::string& topic) const;
    std::size_t prune();

private:
    struct Topic {
        std::vector<EventStream> streams;
        std::deque<ServerEvent>  history;
        std::uint64_t            next_id = 1;
    };

    struct Shared {
        std::size_t                            replay = 100;
        std::mutex                             m;
        std::unordered_map<std::string, Topic> topics;
    };

    std::shared_ptr<Shared> m_shared;
};

class ChangeSignal {
public:
    using Listener = std::function<void(std::uint64_t version)>;

    std::uint64_t version() const;
    std::uint64_t notify();
    bool wait_for_change(std::uint64_t seen, std::chrono::milliseconds timeout, std::uint64_t* now_version = nullptr) const;
    std::uint64_t listen(Listener fn);
    void unlisten(std::uint64_t id);

private:
    mutable std::mutex                           m_mutex;
    mutable std::condition_variable              m_cv;
    std::uint64_t                                m_version = 0;
    std::uint64_t                                m_next_listener = 1;
    std::unordered_map<std::uint64_t, Listener>  m_listeners;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_EVENT_STREAM_HPP
