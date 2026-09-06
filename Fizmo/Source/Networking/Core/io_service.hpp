#ifndef FIZMO_IO_SERVICE_HPP
#define FIZMO_IO_SERVICE_HPP

#include "socket_event.hpp"
#include "Socket Impl/native_poll.hpp"
#include "Error Conversion/native_errors.hpp"
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace fizmo {
namespace networking {
namespace core {

class IOService {
public:
    using Task      = std::function<void()>;
    using TimerID   = std::uint64_t;
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration  = std::chrono::steady_clock::duration;

private:
    enum class EntryKind { Raw, Buffered, BufferedUDP };

    struct PollEntry {
        EntryKind kind;
        void*     socket;
    };

    std::atomic<bool> m_running;
    std::atomic<bool> m_stop_requested;

    std::mutex m_task_mutex;
    std::condition_variable m_task_condition;
    std::queue<Task> m_tasks;

    std::vector<std::thread> m_threads;
    SocketEventHandler m_event_handler;

    std::mutex m_timer_mutex;
    TimerID m_next_timer_id;
    std::multimap<TimePoint, std::pair<TimerID, Task>> m_timers;
    std::map<TimerID, std::multimap<TimePoint, std::pair<TimerID, Task>>::iterator> m_timer_lookup;
    std::condition_variable m_timer_condition;

    std::mutex m_socket_set_mutex;
    std::map<SocketBase*       , SocketEvent::Type> m_raw_sockets;
    std::map<BufferedSocket*   , SocketEvent::Type> m_buffered_sockets;
    std::map<BufferedUDPSocket*, SocketEvent::Type> m_buffered_udp_sockets;

    std::mutex m_poll_mutex;
    detail::PollSet m_poll_set;
    std::vector<PollEntry> m_poll_index;

    static constexpr int kDefaultPollTimeoutMs = 100;

public:
    IOService() : m_running(false), m_stop_requested(false), m_next_timer_id(1) {}

    ~IOService() { stop(); }
    IOService(const IOService&) = delete;
    IOService& operator=(const IOService&) = delete;

public:
    void start(std::size_t thread_count = 1) {
        if (m_running.exchange(true)) return;
        m_stop_requested = false;
        for (std::size_t i = 0; i < thread_count; ++i) { m_threads.emplace_back(&IOService::worker_thread, this); }
    }

    void stop() {
        m_stop_requested = true;
        { std::lock_guard<std::mutex> lock(m_task_mutex);  m_task_condition.notify_all(); }
        { std::lock_guard<std::mutex> lock(m_timer_mutex); m_timer_condition.notify_all(); }

        if (m_running.exchange(false)) {
            for (auto& t : m_threads) { if (t.joinable()) t.join(); }
            m_threads.clear();
        }
    }

    bool is_running() const noexcept { return m_running; }

    void post(Task task) {
        std::lock_guard<std::mutex> lock(m_task_mutex);
        m_tasks.push(std::move(task));
        m_task_condition.notify_one();
    }

public:
    TimerID schedule_timer(Duration duration, Task task) {
        return schedule_timer_at(std::chrono::steady_clock::now() + duration, std::move(task));
    }

    TimerID schedule_timer_at(TimePoint tp, Task task) {
        std::lock_guard<std::mutex> lock(m_timer_mutex);
        TimerID id = m_next_timer_id++;
        auto it = m_timers.emplace(tp, std::make_pair(id, std::move(task)));
        m_timer_lookup[id] = it;
        m_timer_condition.notify_one();
        return id;
    }

    void cancel_timer(TimerID id) {
        std::lock_guard<std::mutex> lock(m_timer_mutex);
        auto it = m_timer_lookup.find(id);

        if (it != m_timer_lookup.end()) {
            m_timers.erase(it->second);
            m_timer_lookup.erase(it);
        }
    }

public:
    unsigned int poll_one() {
        if (!m_running) return 0;
        if (process_tasks())  return 1;
        if (process_timers()) return 1;
        return poll_sockets(0) ? 1u : 0u;
    }

    std::uint32_t poll() {
        std::uint32_t count = 0;
        while (poll_one() > 0 && count < std::numeric_limits<std::uint32_t>::max() / 2) { ++count; }
        return count;
    }

public:
    void run() {
        m_running = true;
        m_stop_requested = false;

        while (!m_stop_requested) {
            bool did_work = process_tasks();
            did_work |= process_timers();
            if (!did_work) { poll_sockets(next_poll_timeout_ms()); }
        }

        m_running = false;
    }

    bool run_one() {
        if (process_tasks())  return true;
        if (process_timers()) return true;
        return poll_sockets(next_poll_timeout_ms());
    }

public:
    void register_socket(SocketBase* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_raw_sockets[socket] = events;
    }

    void register_socket(BufferedSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_buffered_sockets[socket] = events;
    }

    void register_socket(BufferedUDPSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_buffered_udp_sockets[socket] = events;
    }

    void unregister_socket(SocketBase* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_raw_sockets.erase(socket);
        m_event_handler.unregister_all_callbacks(socket);
    }

    void unregister_socket(BufferedSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_buffered_sockets.erase(socket);
        m_event_handler.unregister_all_callbacks(socket);
    }

    void unregister_socket(BufferedUDPSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        m_buffered_udp_sockets.erase(socket);
        m_event_handler.unregister_all_callbacks(socket);
    }

    void modify_socket_events(SocketBase* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        auto it = m_raw_sockets.find(socket);
        if (it != m_raw_sockets.end()) { it->second = events; }
    }

    void modify_socket_events(BufferedSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        auto it = m_buffered_sockets.find(socket);
        if (it != m_buffered_sockets.end()) { it->second = events; }
    }

    void modify_socket_events(BufferedUDPSocket* socket, SocketEvent::Type events) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        auto it = m_buffered_udp_sockets.find(socket);
        if (it != m_buffered_udp_sockets.end()) { it->second = events; }
    }

public:
    template<typename Func>
    auto with_event_handler(Func func) -> decltype(func(std::declval<SocketEventHandler&>())) {
        return func(m_event_handler);
    }

private:
    void worker_thread() {
        while (m_running && !m_stop_requested) {
            Task task;
            bool have_task = false;

            {
                std::unique_lock<std::mutex> lock(m_task_mutex);

                if (m_tasks.empty()) {
                    TimePoint next = get_next_timer_point();

                    if (next != TimePoint::max()) {
                        m_task_condition.wait_until(lock, next, [this] { return !m_tasks.empty() || m_stop_requested; });
                    } else {
                        m_task_condition.wait_for(lock, std::chrono::milliseconds(kDefaultPollTimeoutMs), [this] { return !m_tasks.empty() || m_stop_requested; });
                    }
                }

                if (!m_tasks.empty() && !m_stop_requested) {
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                    have_task = true;
                }
            }

            if (have_task) { task(); } else { run_one(); }
        }
    }

private:
    static bool wants(SocketEvent::Type events, SocketEvent::Type flag) noexcept {
        return (events & flag) != SocketEvent::Type::None;
    }

    int next_poll_timeout_ms() {
        Duration remaining = get_next_timer_duration();
        if (remaining == Duration::max()) return kDefaultPollTimeoutMs;
        int ms = detail::PollSet::duration_to_ms(remaining);
        if (ms < 0 || ms > kDefaultPollTimeoutMs) return kDefaultPollTimeoutMs;
        return ms;
    }

    bool poll_sockets(int timeout_ms) {
        std::lock_guard<std::mutex> poll_lock(m_poll_mutex);

        {
            std::lock_guard<std::mutex> lock(m_socket_set_mutex);
            build_poll_set();
        }

        if (m_poll_set.empty()) return false;
        int ready = m_poll_set.wait(timeout_ms);
        if (ready <= 0) return false;
        std::lock_guard<std::mutex> lock(m_socket_set_mutex);
        dispatch_events();
        return true;
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
                m_poll_index.push_back(PollEntry{EntryKind::Raw, entry.first});
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            const bool want_read  = wants(entry.second, SocketEvent::Type::Read);
            const bool want_write = wants(entry.second, SocketEvent::Type::Write) && entry.first->has_data_to_send();
            
            if (m_poll_set.add(entry.first->socket().get_raw_socket(), want_read, want_write)) {
                m_poll_index.push_back(PollEntry{EntryKind::Buffered, entry.first});
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            const bool want_read  = wants(entry.second, SocketEvent::Type::Read);
            const bool want_write = wants(entry.second, SocketEvent::Type::Write) && entry.first->has_data_to_send();
            
            if (m_poll_set.add(entry.first->socket().get_raw_socket(), want_read, want_write)) {
                m_poll_index.push_back(PollEntry{EntryKind::BufferedUDP, entry.first});
            }
        }
    }

    void dispatch_events() {
        for (std::size_t i = 0; i < m_poll_index.size(); ++i) {
            if (!m_poll_set.signalled(i)) continue;
            const bool readable = m_poll_set.readable(i);
            const bool writable = m_poll_set.writable(i);
            const bool errored  = m_poll_set.errored(i);

            switch (m_poll_index[i].kind) {
                case EntryKind::Raw:
                    dispatch_raw(static_cast<SocketBase*>(m_poll_index[i].socket), readable, writable, errored);
                    break;
                case EntryKind::Buffered:
                    dispatch_buffered(static_cast<BufferedSocket*>(m_poll_index[i].socket), readable, writable, errored);
                    break;
                case EntryKind::BufferedUDP:
                    dispatch_buffered_udp(static_cast<BufferedUDPSocket*>(m_poll_index[i].socket), readable, writable, errored);
                    break;
            }
        }
    }

    void dispatch_raw(SocketBase* socket, bool readable, bool writable, bool errored) {
        if (!socket) return;
        if (readable) { m_event_handler.trigger_event(socket, SocketEvent::Type::Read); }
        if (writable) { m_event_handler.trigger_event(socket, SocketEvent::Type::Write); }
        if (errored)  { m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("poll")); }
    }

    void dispatch_buffered(BufferedSocket* socket, bool readable, bool writable, bool errored) {
        if (!socket) return;

        if (readable) {
            int received = socket->receive();

            if (received > 0) {
                m_event_handler.trigger_event(socket, SocketEvent::Type::Read);
            } else if (received == 0) {
                m_event_handler.trigger_event(socket, SocketEvent::Type::Close);
            } else if (!NativeErrorConverter::would_block()) {
                m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("receive"));
            }
        }

        if (writable) {
            int sent = socket->flush();

            if (sent > 0) {
                m_event_handler.trigger_event(socket, SocketEvent::Type::Write);
            } else if (sent < 0 && !NativeErrorConverter::would_block()) {
                m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("send"));
            }
        }

        if (errored) {
            m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("poll"));
        }
    }

    void dispatch_buffered_udp(BufferedUDPSocket* socket, bool readable, bool writable, bool errored) {
        if (!socket) return;

        if (readable) {
            int received = socket->receive_one();

            if (received > 0) {
                m_event_handler.trigger_event(socket, SocketEvent::Type::Read);
            } else if (received < 0 && !NativeErrorConverter::would_block()) {
                m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("recvfrom"));
            }
        }

        if (writable) {
            int sent = socket->flush_one();

            if (sent > 0) {
                m_event_handler.trigger_event(socket, SocketEvent::Type::Write);
            } else if (sent < 0 && !NativeErrorConverter::would_block()) {
                m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("sendto"));
            }
        }

        if (errored) {
            m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("poll"));
        }
    }

private:
    bool process_tasks() {
        Task task;
        bool processed = false;

        {
            std::lock_guard<std::mutex> lock(m_task_mutex);

            if (!m_tasks.empty()) {
                task = std::move(m_tasks.front());
                m_tasks.pop();
                processed = true;
            }
        }

        if (processed) { task(); }
        return processed;
    }

    bool process_timers() {
        std::vector<Task> ready;

        {
            std::lock_guard<std::mutex> lock(m_timer_mutex);
            if (m_timers.empty()) return false;
            TimePoint now = std::chrono::steady_clock::now();
            bool processed = false;

            while (!m_timers.empty()) {
                auto it = m_timers.begin();
                if (it->first > now) break;
                ready.push_back(std::move(it->second.second));
                m_timer_lookup.erase(it->second.first);
                m_timers.erase(it);
                processed = true;
            }

            if (!processed) return false;
        }

        for (auto& t : ready) { t(); }
        return true;
    }

    Duration get_next_timer_duration() {
        std::lock_guard<std::mutex> lock(m_timer_mutex);
        if (m_timers.empty()) return Duration::max();
        TimePoint now  = std::chrono::steady_clock::now();
        TimePoint next = m_timers.begin()->first;
        if (next <= now) return Duration::zero();
        return next - now;
    }

    TimePoint get_next_timer_point() {
        std::lock_guard<std::mutex> lock(m_timer_mutex);
        if (m_timers.empty()) return TimePoint::max();
        return m_timers.begin()->first;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_IO_SERVICE_HPP