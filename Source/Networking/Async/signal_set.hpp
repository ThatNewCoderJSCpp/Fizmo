#ifndef FIZMO_ASYNC_SIGNAL_SET_HPP
#define FIZMO_ASYNC_SIGNAL_SET_HPP

#include "io_context.hpp"

#include <csignal>
#include <deque>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <set>

namespace fizmo {
namespace networking {
namespace async {

class SignalSet {
public:
    using SignalHandler = UniqueFunction<void(std::error_code, int)>;

    explicit SignalSet(IOContext& ctx);
    SignalSet(IOContext& ctx, std::initializer_list<int> signals);
    ~SignalSet();

    SignalSet(const SignalSet&) = delete;
    SignalSet& operator=(const SignalSet&) = delete;

    void add(int signal_number);
    void remove(int signal_number);
    void clear();

    void async_wait(SignalHandler handler);
    std::size_t cancel();

    IOContext& context() const noexcept { return m_impl->ctx; }

    struct Impl {
        explicit Impl(IOContext& c) : ctx(c) {}
        IOContext&                ctx;
        std::mutex                mutex;
        std::set<int>             signals;
        std::deque<int>           pending;
        std::deque<SignalHandler> waiters;
    };

private:
    std::shared_ptr<Impl> m_impl;
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_SIGNAL_SET_HPP
