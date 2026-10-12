#ifndef FIZMO_ASYNC_REACTOR_HPP
#define FIZMO_ASYNC_REACTOR_HPP

#include "platform.hpp"
#include "function.hpp"
#include "error.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

class IOContext;

namespace detail {

struct ReactorOp {
    UniqueFunction<bool(std::error_code&, std::size_t&)> perform;
    UniqueFunction<void(std::error_code, std::size_t)>   handler;
    std::error_code ec;
    std::size_t     bytes = 0;
};

using ReactorOpPtr = std::unique_ptr<ReactorOp>;

struct Descriptor {
    native_socket_t           fd = invalid_socket;
    std::uint64_t             id = 0;
    std::mutex                mutex;
    std::deque<ReactorOpPtr>  ops[2];
    bool                      shutdown = false;
};

using DescriptorPtr = std::shared_ptr<Descriptor>;

class Reactor {
public:
    enum OpType { Read = 0, Write = 1 };

    explicit Reactor(IOContext& ctx);
    ~Reactor();

    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    DescriptorPtr register_descriptor(native_socket_t fd, std::error_code& ec);
    void deregister_descriptor(DescriptorPtr& d);

    void start_op(const DescriptorPtr& d, OpType type, ReactorOpPtr op, bool speculative);
    std::size_t cancel_ops(const DescriptorPtr& d, std::error_code reason = make_error_code(error::operation_aborted));

    std::size_t shutdown();

    void run(int timeout_ms, std::vector<UniqueFunction<void()>>& completions);
    void interrupt() noexcept;

    static UniqueFunction<void()> make_completion(ReactorOpPtr op);

private:
    struct Impl;

    IOContext&            m_ctx;
    std::unique_ptr<Impl> m_impl;
    std::mutex            m_registry_mutex;
    std::unordered_map<std::uint64_t, DescriptorPtr> m_registry;
    std::uint64_t         m_next_id = 1;

    void process(const DescriptorPtr& d, bool readable, bool writable, std::vector<UniqueFunction<void()>>& completions);
    DescriptorPtr lookup(std::uint64_t id);

    friend struct Impl;
};

} // namespace detail
} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_REACTOR_HPP
