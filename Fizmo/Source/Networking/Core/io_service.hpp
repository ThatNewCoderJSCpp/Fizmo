#ifndef FIZMO_IO_SERVICE_HPP
#define FIZMO_IO_SERVICE_HPP

#include "socket_event.hpp"
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

#ifdef OS_WINDOWS
    fd_set m_read_set;
    fd_set m_write_set;
    fd_set m_error_set;
#endif

    std::mutex m_socket_set_mutex;
    std::map<SocketBase*       , SocketEvent::Type> m_raw_sockets;
    std::map<BufferedSocket*   , SocketEvent::Type> m_buffered_sockets;
    std::map<BufferedUDPSocket*, SocketEvent::Type> m_buffered_udp_sockets;

public:
    IOService() : m_running(false), m_stop_requested(false), m_next_timer_id(1) {
    #ifdef OS_WINDOWS
        FD_ZERO(&m_read_set);
        FD_ZERO(&m_write_set);
        FD_ZERO(&m_error_set);
    #endif
    }

    ~IOService() { stop(); }
    IOService(const IOService&) = delete;
    IOService& operator=(const IOService&) = delete;

public:
    void start(size_t thread_count = 1) {
        if (m_running.exchange(true)) return;
        m_stop_requested = false;
        for (size_t i = 0; i < thread_count; ++i) { m_threads.emplace_back(&IOService::worker_thread, this); }
    }

    void stop() {
        m_stop_requested = true;
        { std::lock_guard<std::mutex> lock(m_task_mutex); m_task_condition.notify_all(); }
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
        if (process_tasks()) return 1;
        if (process_timers()) return 1;
        { std::lock_guard<std::mutex> lock(m_socket_set_mutex); prepare_sets(); }

    #ifdef OS_WINDOWS
        struct timeval tv = {0, 0};
        int ready = select(0, &m_read_set, &m_write_set, &m_error_set, &tv);
        if (ready > 0) { std::lock_guard<std::mutex> lock(m_socket_set_mutex); process_events(); return 1; }
    #endif

        return 0;
    }

    std::uint32_t poll() {
        std::uint32_t count = 0;
        while (poll_one() > 0 && count < std::numeric_limits<std::uint32_t>::max() / 2) { ++count; }
        return count;
    }

public:
    void run() {
        if (!m_running.exchange(true)) { m_stop_requested = false; }

        while (!m_stop_requested) {
            bool did_work = process_tasks();
            did_work |= process_timers();

            if (!did_work) {
                { std::lock_guard<std::mutex> lock(m_socket_set_mutex); prepare_sets(); }

            #ifdef OS_WINDOWS
                Duration td = get_next_timer_duration();
                struct timeval tv;

                if (td != Duration::max()) {
                    auto us = std::chrono::duration_cast<std::chrono::microseconds>(td).count();
                    tv.tv_sec  = static_cast<long>(us / 1000000);
                    tv.tv_usec = static_cast<long>(us % 1000000);
                } else {
                    tv.tv_sec = 0; tv.tv_usec = 100000;
                }

                int ready = select(0, &m_read_set, &m_write_set, &m_error_set, &tv);
                if (ready > 0) { std::lock_guard<std::mutex> lock(m_socket_set_mutex); process_events(); }
            #endif
            }
        }
        m_running = false;
    }

    void run_one() {
        if (!m_running.exchange(true)) { m_stop_requested = false; }
        bool did_work = process_tasks();
        did_work |= process_timers();

        if (!did_work) {
            { std::lock_guard<std::mutex> lock(m_socket_set_mutex); prepare_sets(); }

        #ifdef OS_WINDOWS
            Duration td = get_next_timer_duration();
            struct timeval tv;

            if (td != Duration::max()) {
                auto us = std::chrono::duration_cast<std::chrono::microseconds>(td).count();
                tv.tv_sec  = static_cast<long>(us / 1000000);
                tv.tv_usec = static_cast<long>(us % 1000000);
            } else {
                tv.tv_sec = 0; tv.tv_usec = 100000;
            }

            int ready = select(0, &m_read_set, &m_write_set, &m_error_set, &tv);
            if (ready > 0) { std::lock_guard<std::mutex> lock(m_socket_set_mutex); process_events(); }
        #endif
        }
        m_running = false;
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
                        m_task_condition.wait_for(lock, std::chrono::milliseconds(100), [this] { return !m_tasks.empty() || m_stop_requested; });
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

    void prepare_sets() {
    #ifdef OS_WINDOWS
        FD_ZERO(&m_read_set);
        FD_ZERO(&m_write_set);
        FD_ZERO(&m_error_set);

        for (const auto& entry : m_raw_sockets) {
            SocketBase* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            BufferedSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if (((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && socket->has_data_to_send()) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            BufferedUDPSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if (((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && socket->has_data_to_send()) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }
    #endif
    }

    void process_events() {
    #ifdef OS_WINDOWS
        for (const auto& entry : m_raw_sockets) {
            SocketBase* socket = entry.first;
            SOCKET raw = socket->get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if (FD_ISSET(raw, &m_read_set))  { m_event_handler.trigger_event(socket, SocketEvent::Type::Read); }
                if (FD_ISSET(raw, &m_write_set)) { m_event_handler.trigger_event(socket, SocketEvent::Type::Write); }
                if (FD_ISSET(raw, &m_error_set)) { m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("select")); }
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            BufferedSocket* socket = entry.first;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if (FD_ISSET(raw, &m_read_set)) {
                    int received = socket->receive();

                    if (received > 0) {
                        m_event_handler.trigger_event(socket, SocketEvent::Type::Read);
                    } else if (received == 0) {
                        m_event_handler.trigger_event(socket, SocketEvent::Type::Close);
                    } else {
                        if (!WinsockErrorConverter::would_block()) {
                            m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("receive"));
                        }
                    }
                }

                if (FD_ISSET(raw, &m_write_set)) {
                    int sent = socket->flush();

                    if (sent > 0) {
                        m_event_handler.trigger_event(socket, SocketEvent::Type::Write);
                    } else if (sent < 0 && !WinsockErrorConverter::would_block()) {
                        m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("send"));
                    }
                }

                if (FD_ISSET(raw, &m_error_set)) {
                    m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("select"));
                }
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            BufferedUDPSocket* socket = entry.first;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if (FD_ISSET(raw, &m_read_set)) {
                    int received = socket->receive_one();

                    if (received > 0) {
                        m_event_handler.trigger_event(socket, SocketEvent::Type::Read);
                    } else if (received < 0) {
                        if (!WinsockErrorConverter::would_block()) {
                            m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("recvfrom"));
                        }
                    }
                }

                if (FD_ISSET(raw, &m_write_set)) {
                    int sent = socket->flush_one();

                    if (sent > 0) {
                        m_event_handler.trigger_event(socket, SocketEvent::Type::Write);
                    } else if (sent < 0 && !WinsockErrorConverter::would_block()) {
                        m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("sendto"));
                    }
                }

                if (FD_ISSET(raw, &m_error_set)) {
                    m_event_handler.trigger_error_event(socket, WinsockErrorConverter::get_last_error("select"));
                }
            }
        }
    #endif
    }

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
        TimePoint now = std::chrono::steady_clock::now();
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