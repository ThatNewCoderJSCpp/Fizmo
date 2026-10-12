#ifndef FIZMO_ASYNC_STRAND_HPP
#define FIZMO_ASYNC_STRAND_HPP

#include "io_context.hpp"

#include <deque>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>

namespace fizmo {
namespace networking {
namespace async {

class Strand {
public:
    explicit Strand(IOContext& ctx);

    IOContext& context() const noexcept { return m_impl->ctx; }

    template <typename F>
    void post(F&& f) { enqueue(m_impl, UniqueFunction<void()>(std::forward<F>(f))); }

    template <typename F>
    void dispatch(F&& f) {
        if (running_in_this_thread()) {
            f();
            return;
        }

        post(std::forward<F>(f));
    }

    template <typename F>
    void defer(F&& f) { post(std::forward<F>(f)); }

    bool running_in_this_thread() const noexcept;

    template <typename F>
    auto wrap(F f) const {
        auto impl = m_impl;
        return [impl, f](auto&&... args) {
            auto tup = std::make_tuple(std::forward<decltype(args)>(args)...);
            enqueue(impl, UniqueFunction<void()>([f, tup]() mutable { std::apply(f, tup); }));
        };
    }

    friend bool operator==(const Strand& a, const Strand& b) noexcept { return a.m_impl == b.m_impl; }
    friend bool operator!=(const Strand& a, const Strand& b) noexcept { return a.m_impl != b.m_impl; }

private:
    struct Impl {
        explicit Impl(IOContext& c) : ctx(c) {}
        IOContext&                         ctx;
        std::mutex                         mutex;
        std::deque<UniqueFunction<void()>> queue;
        bool                               scheduled = false;
    };

    std::shared_ptr<Impl> m_impl;

    static void enqueue(const std::shared_ptr<Impl>& impl, UniqueFunction<void()> fn);
    static void drain(const std::shared_ptr<Impl>& impl);
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_STRAND_HPP
