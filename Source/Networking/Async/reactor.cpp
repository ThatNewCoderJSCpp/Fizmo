#include "fizmo_library.hpp"
#include "reactor.hpp"
#include "io_context.hpp"
#include "socket_ops.hpp"
#include "endpoint.hpp"

#if defined(OS_LINUX) || defined(OS_ANDROID)
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
    #define FIZMO_ASYNC_EPOLL 1
#endif

namespace fizmo {
namespace networking {
namespace async {
namespace detail {

#if defined(FIZMO_ASYNC_EPOLL)

struct Reactor::Impl {
    int epfd = -1;
    int evfd = -1;
    static constexpr bool interrupt_on_start = false;

    Impl() {
        epfd = ::epoll_create1(EPOLL_CLOEXEC);
        evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.u64 = 0;
        ::epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, &ev);
    }

    ~Impl() {
        if (evfd >= 0) ::close(evfd);
        if (epfd >= 0) ::close(epfd);
    }

    bool add(native_socket_t fd, std::uint64_t id, std::error_code& ec) {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLET;
        ev.data.u64 = id;

        if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
            ec = last_socket_error();
            return false;
        }

        return true;
    }

    void remove(native_socket_t fd) {
        epoll_event ev{};
        ::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev);
    }

    void interrupt() noexcept {
        std::uint64_t one = 1;
        ssize_t r = ::write(evfd, &one, sizeof(one));
        (void)r;
    }

    void drain() noexcept {
        std::uint64_t v;
        while (::read(evfd, &v, sizeof(v)) > 0) {}
    }

    void wait(Reactor& r, int timeout_ms, std::vector<UniqueFunction<void()>>& out) {
        epoll_event events[128];
        int n = ::epoll_wait(epfd, events, 128, timeout_ms);
        if (n < 0) return;

        for (int i = 0; i < n; ++i) {
            if (events[i].data.u64 == 0) { drain(); continue; }
            DescriptorPtr d = r.lookup(events[i].data.u64);
            if (!d) continue;
            const std::uint32_t e = events[i].events;
            const bool err = (e & (EPOLLERR | EPOLLHUP)) != 0;
            r.process(d, err || (e & (EPOLLIN | EPOLLPRI)) != 0, err || (e & EPOLLOUT) != 0, out);
        }
    }
};

#else

#if defined(OS_WINDOWS)
    #define FIZMO_POLL_FN ::WSAPoll
    #define FIZMO_POLL_IN POLLRDNORM
    #define FIZMO_POLL_OUT POLLWRNORM
    using fizmo_pollfd = WSAPOLLFD;
#else
    #define FIZMO_POLL_FN ::poll
    #define FIZMO_POLL_IN POLLIN
    #define FIZMO_POLL_OUT POLLOUT
    using fizmo_pollfd = pollfd;
#endif

struct Reactor::Impl {
    native_socket_t read_end  = invalid_socket;
    native_socket_t write_end = invalid_socket;
    static constexpr bool interrupt_on_start = true;

    Impl() {
#if defined(OS_WINDOWS)
        std::error_code ec;
        read_end = open_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, ec);
        write_end = open_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, ec);
        Endpoint lo("127.0.0.1", 0);
        bind(read_end, lo, ec);
        Endpoint bound;
        local_endpoint(read_end, bound, ec);
        start_connect(write_end, bound, ec);
#else
        int fds[2] = { -1, -1 };

        if (::pipe(fds) == 0) {
            read_end = fds[0];
            write_end = fds[1];
            std::error_code ec;
            set_nonblocking(read_end, true, ec);
            set_nonblocking(write_end, true, ec);
            ::fcntl(read_end, F_SETFD, FD_CLOEXEC);
            ::fcntl(write_end, F_SETFD, FD_CLOEXEC);
        }
#endif
    }

    ~Impl() {
#if defined(OS_WINDOWS)
        close_socket(read_end);
        close_socket(write_end);
#else
        if (read_end >= 0) ::close(read_end);
        if (write_end >= 0) ::close(write_end);
#endif
    }

    bool add(native_socket_t, std::uint64_t, std::error_code&) { return true; }
    void remove(native_socket_t) {}

    void interrupt() noexcept {
        const char b = 1;
#if defined(OS_WINDOWS)
        ::send(write_end, &b, 1, 0);
#else
        ssize_t r = ::write(write_end, &b, 1);
        (void)r;
#endif
    }

    void drain() noexcept {
        char buf[64];
#if defined(OS_WINDOWS)
        while (::recv(read_end, buf, sizeof(buf), 0) > 0) {}
#else
        while (::read(read_end, buf, sizeof(buf)) > 0) {}
#endif
    }

    void wait(Reactor& r, int timeout_ms, std::vector<UniqueFunction<void()>>& out) {
        std::vector<fizmo_pollfd> fds;
        std::vector<DescriptorPtr> ds;
        fizmo_pollfd self{};
        self.fd = read_end;
        self.events = FIZMO_POLL_IN;
        fds.push_back(self);
        ds.push_back(nullptr);

        {
            std::lock_guard<std::mutex> lock(r.m_registry_mutex);

            for (auto& kv : r.m_registry) {
                std::lock_guard<std::mutex> dl(kv.second->mutex);
                short ev = 0;
                if (!kv.second->ops[Read].empty()) ev |= FIZMO_POLL_IN;
                if (!kv.second->ops[Write].empty()) ev |= FIZMO_POLL_OUT;
                if (ev == 0) continue;
                fizmo_pollfd p{};
                p.fd = kv.second->fd;
                p.events = ev;
                fds.push_back(p);
                ds.push_back(kv.second);
            }
        }

        const int n = FIZMO_POLL_FN(fds.data(), static_cast<unsigned long>(fds.size()), timeout_ms);
        if (n <= 0) return;
        if (fds[0].revents != 0) drain();

        for (std::size_t i = 1; i < fds.size(); ++i) {
            const short re = fds[i].revents;
            if (re == 0) continue;
            const bool err = (re & (POLLERR | POLLHUP | POLLNVAL)) != 0;
            r.process(ds[i], err || (re & FIZMO_POLL_IN) != 0, err || (re & FIZMO_POLL_OUT) != 0, out);
        }
    }
};

#endif

Reactor::Reactor(IOContext& ctx) : m_ctx(ctx), m_impl(new Impl()) {
    ensure_socket_runtime();
}

Reactor::~Reactor() {
    shutdown();
}

std::size_t Reactor::shutdown() {
    std::vector<DescriptorPtr> descriptors;

    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        for (auto& kv : m_registry) descriptors.push_back(kv.second);
    }

    std::vector<ReactorOpPtr> doomed;

    for (auto& d : descriptors) {
        std::lock_guard<std::mutex> dl(d->mutex);

        for (auto& q : d->ops) {
            for (auto& op : q) doomed.push_back(std::move(op));
            q.clear();
        }
    }

    const std::size_t n = doomed.size();
    doomed.clear();
    return n;
}

DescriptorPtr Reactor::register_descriptor(native_socket_t fd, std::error_code& ec) {
    ec.clear();
    auto d = std::make_shared<Descriptor>();
    d->fd = fd;

    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        d->id = m_next_id++;
        m_registry.emplace(d->id, d);
    }

    if (!m_impl->add(fd, d->id, ec)) {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        m_registry.erase(d->id);
        return nullptr;
    }

    return d;
}

void Reactor::deregister_descriptor(DescriptorPtr& d) {
    if (!d) return;

    {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        m_registry.erase(d->id);
    }

    m_impl->remove(d->fd);
    std::vector<UniqueFunction<void()>> aborted;

    {
        std::lock_guard<std::mutex> dl(d->mutex);
        d->shutdown = true;

        for (auto& q : d->ops) {
            for (auto& op : q) {
                op->ec = make_error_code(error::operation_aborted);
                op->bytes = 0;
                aborted.push_back(make_completion(std::move(op)));
            }

            q.clear();
        }
    }

    if (!aborted.empty()) m_ctx.post_completed_batch(aborted);
    d.reset();
}

void Reactor::start_op(const DescriptorPtr& d, OpType type, ReactorOpPtr op, bool speculative) {
    if (!d) {
        op->ec = make_error_code(error::bad_descriptor);
        m_ctx.post_completed(make_completion(std::move(op)));
        return;
    }

    {
        std::unique_lock<std::mutex> dl(d->mutex);

        if (d->shutdown) {
            op->ec = make_error_code(error::bad_descriptor);
            dl.unlock();
            m_ctx.post_completed(make_completion(std::move(op)));
            return;
        }

        if (speculative && d->ops[type].empty() && op->perform(op->ec, op->bytes)) {
            dl.unlock();
            m_ctx.post_completed(make_completion(std::move(op)));
            return;
        }

        d->ops[type].push_back(std::move(op));
    }

    if (Impl::interrupt_on_start) m_impl->interrupt();
}

std::size_t Reactor::cancel_ops(const DescriptorPtr& d, std::error_code reason) {
    if (!d) return 0;
    std::vector<UniqueFunction<void()>> aborted;

    {
        std::lock_guard<std::mutex> dl(d->mutex);

        for (auto& q : d->ops) {
            for (auto& op : q) {
                op->ec = reason;
                op->bytes = 0;
                aborted.push_back(make_completion(std::move(op)));
            }

            q.clear();
        }
    }

    const std::size_t n = aborted.size();
    if (n > 0) m_ctx.post_completed_batch(aborted);
    return n;
}

void Reactor::run(int timeout_ms, std::vector<UniqueFunction<void()>>& completions) {
    m_impl->wait(*this, timeout_ms, completions);
}

void Reactor::interrupt() noexcept {
    m_impl->interrupt();
}

UniqueFunction<void()> Reactor::make_completion(ReactorOpPtr op) {
    return [op = std::move(op)]() mutable {
        auto h = std::move(op->handler);
        const std::error_code ec = op->ec;
        const std::size_t n = op->bytes;
        op.reset();
        if (h) h(ec, n);
    };
}

void Reactor::process(const DescriptorPtr& d, bool readable, bool writable, std::vector<UniqueFunction<void()>>& completions) {
    std::lock_guard<std::mutex> dl(d->mutex);
    if (d->shutdown) return;
    const bool flags[2] = { readable, writable };

    for (int t = 0; t < 2; ++t) {
        if (!flags[t]) continue;
        auto& q = d->ops[t];

        while (!q.empty()) {
            ReactorOp& op = *q.front();
            if (!op.perform(op.ec, op.bytes)) break;
            completions.push_back(make_completion(std::move(q.front())));
            q.pop_front();
        }
    }
}

DescriptorPtr Reactor::lookup(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(m_registry_mutex);
    auto it = m_registry.find(id);
    return it == m_registry.end() ? nullptr : it->second;
}

} // namespace detail
} // namespace async
} // namespace networking
} // namespace fizmo
