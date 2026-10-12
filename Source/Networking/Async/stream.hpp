#ifndef FIZMO_ASYNC_STREAM_HPP
#define FIZMO_ASYNC_STREAM_HPP

#include "buffer.hpp"
#include "endpoint.hpp"
#include "function.hpp"
#include "io_context.hpp"

#include <chrono>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace async {

using IoHandler    = UniqueFunction<void(std::error_code, std::size_t)>;
using ErrorHandler = UniqueFunction<void(std::error_code)>;

class AsyncStream {
public:
    virtual ~AsyncStream() = default;

    virtual IOContext& context() const noexcept = 0;

    virtual void async_read_some(MutableBuffer buffer, IoHandler handler) = 0;
    virtual void async_write_some(ConstBuffer buffer, IoHandler handler) = 0;
    virtual void async_shutdown(ErrorHandler handler) = 0;

    virtual void close() = 0;
    virtual void cancel() = 0;
    virtual bool is_open() const noexcept = 0;

    virtual Endpoint local_endpoint() const = 0;
    virtual Endpoint remote_endpoint() const = 0;

    virtual void set_timeout(std::chrono::milliseconds timeout) = 0;
    virtual void clear_timeout() = 0;

    virtual bool        is_secure() const noexcept { return false; }
    virtual std::string description() const { return remote_endpoint().to_string(); }
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_STREAM_HPP
