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
    void start(std::size_t thread_count = 1);

    void stop();

    bool is_running() const noexcept { return m_running; }

    void post(Task task);

public:
    TimerID schedule_timer(Duration duration, Task task) {
        return schedule_timer_at(std::chrono::steady_clock::now() + duration, std::move(task));
    }

    TimerID schedule_timer_at(TimePoint tp, Task task);

    void cancel_timer(TimerID id);

public:
    unsigned int poll_one();

    std::uint32_t poll();

public:
    void run();

    bool run_one();

public:
    void register_socket(SocketBase* socket, SocketEvent::Type events);

    void register_socket(BufferedSocket* socket, SocketEvent::Type events);

    void register_socket(BufferedUDPSocket* socket, SocketEvent::Type events);

    void unregister_socket(SocketBase* socket);

    void unregister_socket(BufferedSocket* socket);

    void unregister_socket(BufferedUDPSocket* socket);

    void modify_socket_events(SocketBase* socket, SocketEvent::Type events);

    void modify_socket_events(BufferedSocket* socket, SocketEvent::Type events);

    void modify_socket_events(BufferedUDPSocket* socket, SocketEvent::Type events);

public:
    template<typename Func>
    auto with_event_handler(Func func) -> decltype(func(std::declval<SocketEventHandler&>())) {
        return func(m_event_handler);
    }

private:
    void worker_thread();

private:
    static bool wants(SocketEvent::Type events, SocketEvent::Type flag) noexcept {
        return (events & flag) != SocketEvent::Type::None;
    }

    int next_poll_timeout_ms();

    bool poll_sockets(int timeout_ms);

    void build_poll_set();

    void dispatch_events();

    void dispatch_raw(SocketBase* socket, bool readable, bool writable, bool errored);

    void dispatch_buffered(BufferedSocket* socket, bool readable, bool writable, bool errored);

    void dispatch_buffered_udp(BufferedUDPSocket* socket, bool readable, bool writable, bool errored);

private:
    bool process_tasks();

    bool process_timers();

    Duration get_next_timer_duration();

    TimePoint get_next_timer_point();
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_IO_SERVICE_HPP