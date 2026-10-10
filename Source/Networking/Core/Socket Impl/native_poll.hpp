#ifndef FIZMO_NATIVE_POLL_HPP
#define FIZMO_NATIVE_POLL_HPP

#include "native_impl.hpp"
#include <vector>
#include <chrono>
#include <limits>
#include <cstring>

#ifdef OS_LINUX
#include <poll.h>
#include <cerrno>
#endif

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

#ifdef OS_WINDOWS
    using native_pollfd = WSAPOLLFD;
    using poll_event_t  = SHORT;
    using poll_nfds_t   = ULONG;
#else
    using native_pollfd = struct pollfd;
    using poll_event_t  = short;
    using poll_nfds_t   = nfds_t;
#endif

struct PollMask {
#ifdef OS_WINDOWS
    static constexpr poll_event_t Read  = static_cast<poll_event_t>(POLLRDNORM);
    static constexpr poll_event_t Write = static_cast<poll_event_t>(POLLWRNORM);
#else
    static constexpr poll_event_t Read  = static_cast<poll_event_t>(POLLIN);
    static constexpr poll_event_t Write = static_cast<poll_event_t>(POLLOUT);
#endif
    static constexpr poll_event_t Error   = static_cast<poll_event_t>(POLLERR);
    static constexpr poll_event_t Hangup  = static_cast<poll_event_t>(POLLHUP);
    static constexpr poll_event_t Invalid = static_cast<poll_event_t>(POLLNVAL);
};

class PollSet {
private:
    std::vector<native_pollfd> m_fds;

    static int do_poll(native_pollfd* fds, poll_nfds_t count, int timeout_ms) noexcept;

public:
    PollSet() = default;
    ~PollSet() = default;
    PollSet(const PollSet&) = delete;
    PollSet& operator=(const PollSet&) = delete;
    PollSet(PollSet&&) = default;
    PollSet& operator=(PollSet&&) = default;

public:
    void clear() noexcept { m_fds.clear(); }
    void reserve(std::size_t n) { m_fds.reserve(n); }
    std::size_t size() const noexcept { return m_fds.size(); }
    bool empty() const noexcept { return m_fds.empty(); }

    bool add(native_handle_t handle, bool want_read, bool want_write);

    int wait(int timeout_ms) noexcept;

public:
    poll_event_t revents(std::size_t index) const noexcept {
        if (index >= m_fds.size()) return 0;
        return m_fds[index].revents;
    }

    bool readable(std::size_t index) const noexcept {
        return (revents(index) & (PollMask::Read | PollMask::Hangup)) != 0;
    }

    bool writable(std::size_t index) const noexcept {
        return (revents(index) & PollMask::Write) != 0;
    }

    bool errored(std::size_t index) const noexcept {
        return (revents(index) & (PollMask::Error | PollMask::Invalid)) != 0;
    }

    bool hangup(std::size_t index) const noexcept {
        return (revents(index) & PollMask::Hangup) != 0;
    }

    bool signalled(std::size_t index) const noexcept { return revents(index) != 0; }

public:
    static int duration_to_ms(std::chrono::steady_clock::duration d) noexcept;
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NATIVE_POLL_HPP