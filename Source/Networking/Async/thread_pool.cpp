#include "fizmo_library.hpp"
#include "thread_pool.hpp"

namespace fizmo {
namespace networking {
namespace async {

IOThreadPool::IOThreadPool(std::size_t threads) : m_guard(new WorkGuard(m_ctx)) {
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
        if (threads == 0) threads = 2;
    }

    m_threads.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) m_threads.emplace_back([this] { worker(); });
}

IOThreadPool::~IOThreadPool() {
    stop();
    join();
}

void IOThreadPool::worker() {
    for (;;) {
        try {
            m_ctx.run();
            return;
        } catch (...) {
            if (m_on_exception) m_on_exception(std::current_exception());
        }
    }
}

void IOThreadPool::stop() {
    m_ctx.stop();
}

void IOThreadPool::wait() {
    if (m_guard) m_guard->reset();
    join();
}

void IOThreadPool::join() {
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
}

} // namespace async
} // namespace networking
} // namespace fizmo
