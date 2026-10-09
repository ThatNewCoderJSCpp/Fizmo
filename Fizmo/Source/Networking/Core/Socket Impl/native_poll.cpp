#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "native_poll.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

auto PollSet::do_poll(native_pollfd* fds, poll_nfds_t count, int timeout_ms) noexcept -> int {
    #ifdef OS_WINDOWS
        return ::WSAPoll(fds, count, timeout_ms);
    #else
        int result;
        do { result = ::poll(fds, count, timeout_ms); }
        while (result < 0 && errno == EINTR);
        return result;
    #endif
    }

auto PollSet::add(native_handle_t handle, bool want_read, bool want_write) -> bool {
        if (handle == kInvalidHandle) return false;
        native_pollfd pfd;
        std::memset(&pfd, 0, sizeof(pfd));
        pfd.fd = handle;
        poll_event_t requested = 0;
        if (want_read)  requested = static_cast<poll_event_t>(requested | PollMask::Read);
        if (want_write) requested = static_cast<poll_event_t>(requested | PollMask::Write);
        pfd.events  = requested;
        pfd.revents = 0;
        m_fds.push_back(pfd);
        return true;
    }

auto PollSet::wait(int timeout_ms) noexcept -> int {
        if (m_fds.empty()) return 0;
        return do_poll(m_fds.data(), static_cast<poll_nfds_t>(m_fds.size()), timeout_ms);
    }

auto PollSet::duration_to_ms(std::chrono::steady_clock::duration d) noexcept -> int {
        if (d == std::chrono::steady_clock::duration::max()) return -1;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
        if (ms < 0) return 0;
        
        if (ms > static_cast<decltype(ms)>(std::numeric_limits<int>::max())) {
            return std::numeric_limits<int>::max();
        }
        
        return static_cast<int>(ms);
    }

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
