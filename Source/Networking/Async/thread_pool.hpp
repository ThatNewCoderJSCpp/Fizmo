#ifndef FIZMO_ASYNC_THREAD_POOL_HPP
#define FIZMO_ASYNC_THREAD_POOL_HPP

#include "io_context.hpp"

#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

class IOThreadPool {
public:
    explicit IOThreadPool(std::size_t threads = 0);
    ~IOThreadPool();

    IOThreadPool(const IOThreadPool&) = delete;
    IOThreadPool& operator=(const IOThreadPool&) = delete;

    IOContext& context() noexcept { return m_ctx; }
    std::size_t size() const noexcept { return m_threads.size(); }

    template <typename F>
    void post(F&& f) { m_ctx.post(std::forward<F>(f)); }

    void set_exception_handler(std::function<void(std::exception_ptr)> handler) { m_on_exception = std::move(handler); }

    void stop();
    void wait();
    void join();

private:
    IOContext                                m_ctx;
    std::unique_ptr<WorkGuard>               m_guard;
    std::vector<std::thread>                 m_threads;
    std::function<void(std::exception_ptr)>  m_on_exception;

    void worker();
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_THREAD_POOL_HPP
