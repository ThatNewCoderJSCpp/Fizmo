#include "fizmo_library.hpp"
#include "io_service.hpp"

namespace fizmo {
namespace networking {
namespace core {

void IOService::start(std::size_t thread_count) {
    if (m_running.exchange(true)) return;
    m_stop_requested = false;
    for (std::size_t i = 0; i < thread_count; ++i) { m_threads.emplace_back(&IOService::worker_thread, this); }
}

void IOService::stop() {
    m_stop_requested = true;
    { std::lock_guard<std::mutex> lock(m_task_mutex);  m_task_condition.notify_all(); }
    { std::lock_guard<std::mutex> lock(m_timer_mutex); m_timer_condition.notify_all(); }

    if (m_running.exchange(false)) {
        for (auto& t : m_threads) { if (t.joinable()) t.join(); }
        m_threads.clear();
    }
}

void IOService::post(Task task) {
    std::lock_guard<std::mutex> lock(m_task_mutex);
    m_tasks.push(std::move(task));
    m_task_condition.notify_one();
}

auto IOService::schedule_timer_at(TimePoint tp, Task task) -> TimerID {
    std::lock_guard<std::mutex> lock(m_timer_mutex);
    TimerID id = m_next_timer_id++;
    auto it = m_timers.emplace(tp, std::make_pair(id, std::move(task)));
    m_timer_lookup[id] = it;
    m_timer_condition.notify_one();
    return id;
}

void IOService::cancel_timer(TimerID id) {
    std::lock_guard<std::mutex> lock(m_timer_mutex);
    auto it = m_timer_lookup.find(id);

    if (it != m_timer_lookup.end()) {
        m_timers.erase(it->second);
        m_timer_lookup.erase(it);
    }
}

unsigned int IOService::poll_one() {
    if (!m_running) return 0;
    if (process_tasks())  return 1;
    if (process_timers()) return 1;
    return poll_sockets(0) ? 1u : 0u;
}

std::uint32_t IOService::poll() {
    std::uint32_t count = 0;
    while (poll_one() > 0 && count < std::numeric_limits<std::uint32_t>::max() / 2) { ++count; }
    return count;
}

void IOService::run() {
    m_running = true;
    m_stop_requested = false;

    while (!m_stop_requested) {
        bool did_work = process_tasks();
        did_work |= process_timers();
        if (!did_work) { poll_sockets(next_poll_timeout_ms()); }
    }

    m_running = false;
}

bool IOService::run_one() {
    if (process_tasks())  return true;
    if (process_timers()) return true;
    return poll_sockets(next_poll_timeout_ms());
}

void IOService::register_socket(SocketBase* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_raw_sockets[socket] = events;
}

void IOService::register_socket(BufferedSocket* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_buffered_sockets[socket] = events;
}

void IOService::register_socket(BufferedUDPSocket* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_buffered_udp_sockets[socket] = events;
}

void IOService::unregister_socket(SocketBase* socket) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_raw_sockets.erase(socket);
    m_event_handler.unregister_all_callbacks(socket);
}

void IOService::unregister_socket(BufferedSocket* socket) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_buffered_sockets.erase(socket);
    m_event_handler.unregister_all_callbacks(socket);
}

void IOService::unregister_socket(BufferedUDPSocket* socket) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    m_buffered_udp_sockets.erase(socket);
    m_event_handler.unregister_all_callbacks(socket);
}

void IOService::modify_socket_events(SocketBase* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    auto it = m_raw_sockets.find(socket);
    if (it != m_raw_sockets.end()) { it->second = events; }
}

void IOService::modify_socket_events(BufferedSocket* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    auto it = m_buffered_sockets.find(socket);
    if (it != m_buffered_sockets.end()) { it->second = events; }
}

void IOService::modify_socket_events(BufferedUDPSocket* socket, SocketEvent::Type events) {
    if (!socket) return;
    std::lock_guard<std::mutex> lock(m_socket_set_mutex);
    auto it = m_buffered_udp_sockets.find(socket);
    if (it != m_buffered_udp_sockets.end()) { it->second = events; }
}

void IOService::worker_thread() {
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

int IOService::next_poll_timeout_ms() {
    Duration remaining = get_next_timer_duration();
    if (remaining == Duration::max()) return kDefaultPollTimeoutMs;
    int ms = detail::PollSet::duration_to_ms(remaining);
    if (ms < 0 || ms > kDefaultPollTimeoutMs) return kDefaultPollTimeoutMs;
    return ms;
}

bool IOService::poll_sockets(int timeout_ms) {
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

void IOService::build_poll_set() {
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

void IOService::dispatch_events() {
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

void IOService::dispatch_raw(SocketBase* socket, bool readable, bool writable, bool errored) {
    if (!socket) return;
    if (readable) { m_event_handler.trigger_event(socket, SocketEvent::Type::Read); }
    if (writable) { m_event_handler.trigger_event(socket, SocketEvent::Type::Write); }
    if (errored)  { m_event_handler.trigger_error_event(socket, NativeErrorConverter::get_last_error("poll")); }
}

void IOService::dispatch_buffered(BufferedSocket* socket, bool readable, bool writable, bool errored) {
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

void IOService::dispatch_buffered_udp(BufferedUDPSocket* socket, bool readable, bool writable, bool errored) {
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

bool IOService::process_tasks() {
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

bool IOService::process_timers() {
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

auto IOService::get_next_timer_duration() -> Duration {
    std::lock_guard<std::mutex> lock(m_timer_mutex);
    if (m_timers.empty()) return Duration::max();
    TimePoint now  = std::chrono::steady_clock::now();
    TimePoint next = m_timers.begin()->first;
    if (next <= now) return Duration::zero();
    return next - now;
}

auto IOService::get_next_timer_point() -> TimePoint {
    std::lock_guard<std::mutex> lock(m_timer_mutex);
    if (m_timers.empty()) return TimePoint::max();
    return m_timers.begin()->first;
}

} // namespace core
} // namespace networking
} // namespace fizmo
