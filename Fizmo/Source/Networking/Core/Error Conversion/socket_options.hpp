#ifndef FIZMO_SOCKET_OPTIONS_HPP
#define FIZMO_SOCKET_OPTIONS_HPP

#include "socket_base.hpp"
#include "Options Conversion/socket_option.hpp"

#ifdef OS_WINDOWS
#include "Options Conversion/winsock_options.hpp"
#elif defined(OS_LINUX)
#include "Options Conversion/posix_options.hpp"
#endif

namespace fizmo {
namespace networking {
namespace core {

class SocketOptions {
private:
    SocketBase& m_socket;

    static NativeOption resolve(OptionCode code) noexcept;

public:
    explicit SocketOptions(SocketBase& socket) noexcept : m_socket(socket) {}

public:
    SocketOptions& set(BooleanOption opt, bool enable) noexcept;

    bool get(BooleanOption opt) const noexcept;

    SocketOptions& set(IntegerOption opt, int value) noexcept;

    int get(IntegerOption opt) const noexcept;

    SocketOptions& set(SizeOption opt, std::size_t value) noexcept;

    std::size_t get(SizeOption opt) const noexcept;

    SocketOptions& set(TimevalOption opt, unsigned long seconds, unsigned long microseconds = 0) noexcept;

    std::pair<unsigned long, unsigned long> get(TimevalOption opt) const noexcept;

public:
    SocketOptions& linger(bool enable, unsigned short timeout_seconds = 0) noexcept;

    std::pair<bool, unsigned short> get_linger() const noexcept;

public:
    SocketOptions& set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept {
        m_socket.set_keepalive_params(idle_ms, interval_ms);
        return *this;
    }

    int get_pending_error() const noexcept;

public:
    template <typename TypedOpt, typename = decltype(std::declval<TypedOpt>().code())>
    bool is_supported(TypedOpt opt) const noexcept { return resolve(opt.code()).supported; }

public: // TCP presets
    SocketOptions& apply_server_defaults() noexcept;

    SocketOptions& apply_client_defaults() noexcept {
        return set(BooleanOption::TCPNoDelay, true)
              .set(BooleanOption::KeepAlive, true);
    }

    SocketOptions& apply_low_latency() noexcept;

    SocketOptions& set_buffer_sizes(std::size_t send_bytes, std::size_t receive_bytes) noexcept;

    SocketOptions& apply_high_throughput() noexcept;

public: // UDP presets
    SocketOptions& apply_udp_defaults() noexcept;

    SocketOptions& apply_multicast_defaults() noexcept {
        return apply_udp_defaults().set(BooleanOption::Broadcast, false);
    }

public: // Cross-protocol presets
    SocketOptions& apply_realtime() noexcept;

    SocketOptions& apply_bulk_transfer() noexcept;

public:
    bool set_quick_ack(bool enable) noexcept;

    bool enable_fast_open_listener(int queue_length = 16) noexcept;

    bool enable_fast_open_client() noexcept;

private:
    void get_opt(int level, int option, void* value, int* length) const noexcept;
};

inline SocketOptions options(SocketBase& socket) noexcept { return SocketOptions(socket); }

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_OPTIONS_HPP