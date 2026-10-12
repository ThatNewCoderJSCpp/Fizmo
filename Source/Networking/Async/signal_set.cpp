#include "fizmo_library.hpp"
#include "signal_set.hpp"
#include "platform.hpp"

#include <algorithm>
#include <map>
#include <thread>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

namespace {

class SignalHub {
public:
    static SignalHub& instance() {
        static SignalHub* hub = new SignalHub();
        return *hub;
    }

    void attach(const std::shared_ptr<SignalSet::Impl>& impl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sets.push_back(impl);
    }

    void detach(const SignalSet::Impl* impl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sets.erase(std::remove_if(m_sets.begin(), m_sets.end(), [impl](const std::weak_ptr<SignalSet::Impl>& w) {
            auto p = w.lock();
            return !p || p.get() == impl;
        }), m_sets.end());
    }

    void install(int sig) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_refs[sig]++ > 0) return;
        start_locked();
#if defined(OS_WINDOWS)
        if (sig != SIGINT && sig != SIGTERM && sig != SIGBREAK) std::signal(sig, &SignalHub::on_signal);
#else
        struct sigaction sa{};
        sa.sa_handler = &SignalHub::on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        ::sigaction(sig, &sa, nullptr);
#endif
    }

    void uninstall(int sig) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_refs.find(sig);
        if (it == m_refs.end() || it->second == 0) return;
        if (--it->second > 0) return;
        m_refs.erase(it);
#if defined(OS_WINDOWS)
        if (sig != SIGINT && sig != SIGTERM && sig != SIGBREAK) std::signal(sig, SIG_DFL);
#else
        struct sigaction sa{};
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        ::sigaction(sig, &sa, nullptr);
#endif
    }

    bool deliver(int sig) {
        std::vector<std::shared_ptr<SignalSet::Impl>> targets;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            for (auto& w : m_sets) {
                if (auto p = w.lock()) targets.push_back(std::move(p));
            }
        }

        bool handled = false;

        for (auto& impl : targets) {
            SignalSet::SignalHandler h;

            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                if (impl->signals.count(sig) == 0) continue;
                handled = true;

                if (impl->waiters.empty()) {
                    impl->pending.push_back(sig);
                    continue;
                }

                h = std::move(impl->waiters.front());
                impl->waiters.pop_front();
            }

            impl->ctx.post_completed([h = std::move(h), sig]() mutable { h(std::error_code(), sig); });
        }

        return handled;
    }

private:
    std::mutex                                  m_mutex;
    std::vector<std::weak_ptr<SignalSet::Impl>> m_sets;
    std::map<int, int>                          m_refs;
    bool                                        m_started = false;

#if defined(OS_WINDOWS)
    static BOOL WINAPI console_handler(DWORD type) {
        int sig = 0;

        switch (type) {
            case CTRL_C_EVENT:        sig = SIGINT; break;
            case CTRL_BREAK_EVENT:    sig = SIGBREAK; break;
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT: sig = SIGTERM; break;
            default:                  return FALSE;
        }

        bool installed = false;

        {
            SignalHub& hub = instance();
            std::lock_guard<std::mutex> lock(hub.m_mutex);
            auto it = hub.m_refs.find(sig);
            installed = it != hub.m_refs.end() && it->second > 0;
        }

        if (!installed) return FALSE;
        return instance().deliver(sig) ? TRUE : FALSE;
    }

    static void on_signal(int sig) {
        std::signal(sig, &SignalHub::on_signal);
        std::thread([sig] { instance().deliver(sig); }).detach();
    }

    void start_locked() {
        if (m_started) return;
        m_started = true;
        ::SetConsoleCtrlHandler(&SignalHub::console_handler, TRUE);
    }
#else
    int m_pipe[2] = { -1, -1 };
    static int s_write_fd;

    static void on_signal(int sig) {
        const int saved = errno;
        const unsigned char b = static_cast<unsigned char>(sig);
        if (s_write_fd >= 0) {
            ssize_t r = ::write(s_write_fd, &b, 1);
            (void)r;
        }
        errno = saved;
    }

    void start_locked() {
        if (m_started) return;
        if (::pipe(m_pipe) != 0) return;
        ::fcntl(m_pipe[0], F_SETFD, FD_CLOEXEC);
        ::fcntl(m_pipe[1], F_SETFD, FD_CLOEXEC);
        ::fcntl(m_pipe[1], F_SETFL, ::fcntl(m_pipe[1], F_GETFL, 0) | O_NONBLOCK);
        s_write_fd = m_pipe[1];
        m_started = true;
        const int rfd = m_pipe[0];

        std::thread([rfd] {
            for (;;) {
                unsigned char b;
                const ssize_t n = ::read(rfd, &b, 1);
                if (n == 1) { instance().deliver(static_cast<int>(b)); continue; }
                if (n < 0 && errno == EINTR) continue;
                return;
            }
        }).detach();
    }
#endif
};

#if !defined(OS_WINDOWS)
int SignalHub::s_write_fd = -1;
#endif

} // namespace

SignalSet::SignalSet(IOContext& ctx) : m_impl(std::make_shared<Impl>(ctx)) {
    SignalHub::instance().attach(m_impl);
}

SignalSet::SignalSet(IOContext& ctx, std::initializer_list<int> signals) : SignalSet(ctx) {
    for (int s : signals) add(s);
}

SignalSet::~SignalSet() {
    cancel();
    clear();
    SignalHub::instance().detach(m_impl.get());
}

void SignalSet::add(int signal_number) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (!m_impl->signals.insert(signal_number).second) return;
    }

    SignalHub::instance().install(signal_number);
}

void SignalSet::remove(int signal_number) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->signals.erase(signal_number) == 0) return;
        m_impl->pending.erase(std::remove(m_impl->pending.begin(), m_impl->pending.end(), signal_number), m_impl->pending.end());
    }

    SignalHub::instance().uninstall(signal_number);
}

void SignalSet::clear() {
    std::set<int> sigs;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        sigs.swap(m_impl->signals);
        m_impl->pending.clear();
    }

    for (int s : sigs) SignalHub::instance().uninstall(s);
}

void SignalSet::async_wait(SignalHandler handler) {
    m_impl->ctx.work_started();
    int sig = 0;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        if (m_impl->pending.empty()) {
            m_impl->waiters.push_back(std::move(handler));
            return;
        }

        sig = m_impl->pending.front();
        m_impl->pending.pop_front();
    }

    m_impl->ctx.post_completed([h = std::move(handler), sig]() mutable { h(std::error_code(), sig); });
}

std::size_t SignalSet::cancel() {
    std::deque<SignalHandler> waiters;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        waiters.swap(m_impl->waiters);
    }

    const std::size_t n = waiters.size();

    for (auto& h : waiters) {
        m_impl->ctx.post_completed([h = std::move(h)]() mutable { h(make_error_code(error::operation_aborted), 0); });
    }

    return n;
}

} // namespace async
} // namespace networking
} // namespace fizmo
