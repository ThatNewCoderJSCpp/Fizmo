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

    static NativeOption resolve(OptionCode code) noexcept {
    #ifdef OS_WINDOWS
        return WindowsOptions::resolve(code);
    #elif defined(OS_LINUX)
        return LinuxOptions::resolve(code);
    #else
        return NativeOption();
    #endif
    }

public:
    explicit SocketOptions(SocketBase& socket) noexcept : m_socket(socket) {}

public:
    SocketOptions& set(BooleanOption opt, bool enable) noexcept {
        auto native = resolve(opt.code());

        if (native.supported) {
            int val = enable ? 1 : 0;
            m_socket.set_option(native.level, native.option_name, val);
        }

        return *this;
    }

    bool get(BooleanOption opt) const noexcept {
        auto native = resolve(opt.code());
        if (!native.supported) return false;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return val != 0;
    }

    SocketOptions& set(IntegerOption opt, int value) noexcept {
        auto native = resolve(opt.code());
        if (native.supported) { m_socket.set_option(native.level, native.option_name, value); }
        return *this;
    }

    int get(IntegerOption opt) const noexcept {
        auto native = resolve(opt.code());
        if (!native.supported) return 0;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return val;
    }

    SocketOptions& set(SizeOption opt, std::size_t value) noexcept {
        auto native = resolve(opt.code());

        if (native.supported) {
            int val = static_cast<int>(value);
            m_socket.set_option(native.level, native.option_name, val);
        }

        return *this;
    }

    std::size_t get(SizeOption opt) const noexcept {
        auto native = resolve(opt.code());
        if (!native.supported) return 0;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return static_cast<std::size_t>(val);
    }

    SocketOptions& set(TimevalOption opt, unsigned long seconds, unsigned long microseconds = 0) noexcept {
        auto native = resolve(opt.code());

        if (native.supported) {
            struct timeval tv;
            tv.tv_sec  = seconds;
            tv.tv_usec = microseconds;
            m_socket.set_option(native.level, native.option_name, tv);
        }

        return *this;
    }

    std::pair<unsigned long, unsigned long> get(TimevalOption opt) const noexcept {
        auto native = resolve(opt.code());
        if (!native.supported) return {0, 0};
        struct timeval tv = {0, 0};
        int len = sizeof(tv);
        get_opt(native.level, native.option_name, &tv, &len);
        return {static_cast<unsigned long>(tv.tv_sec), static_cast<unsigned long>(tv.tv_usec)};
    }

public:
    SocketOptions& linger(bool enable, unsigned short timeout_seconds = 0) noexcept {
        auto native = resolve(OptionCode::Linger);
        if (native.supported) {

            struct linger lg;
            lg.l_onoff  = enable ? 1 : 0;
            lg.l_linger = timeout_seconds;
            m_socket.set_option(native.level, native.option_name, lg);
        }

        return *this;
    }

    std::pair<bool, unsigned short> get_linger() const noexcept {
        auto native = resolve(OptionCode::Linger);
        if (!native.supported) return {false, 0};
        struct linger lg = {0, 0};
        int len = sizeof(lg);
        get_opt(native.level, native.option_name, &lg, &len);
        return {lg.l_onoff != 0, static_cast<unsigned short>(lg.l_linger)};
    }

public:
    SocketOptions& set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept {
        m_socket.set_keepalive_params(idle_ms, interval_ms);
        return *this;
    }

    int get_pending_error() const noexcept {
        auto native = resolve(OptionCode::PendingError);
        if (!native.supported) return 0;
        int err = 0;
        int len = sizeof(err);
        get_opt(native.level, native.option_name, &err, &len);
        return err;
    }

public:
    template <typename TypedOpt, typename = decltype(std::declval<TypedOpt>().code())>
    bool is_supported(TypedOpt opt) const noexcept { return resolve(opt.code()).supported; }

public: // TCP presets
    SocketOptions& apply_server_defaults() noexcept {
        return set(BooleanOption::ReuseAddress, true)
              .set(BooleanOption::KeepAlive, true)
              .linger(true, 5);
    }

    SocketOptions& apply_client_defaults() noexcept {
        return set(BooleanOption::TCPNoDelay, true)
              .set(BooleanOption::KeepAlive, true);
    }

    SocketOptions& apply_low_latency() noexcept {
        set(BooleanOption::TCPNoDelay, true);
    #ifdef OS_LINUX
        set_quick_ack(true);
    #endif
        return *this;
    }

    SocketOptions& set_buffer_sizes(std::size_t send_bytes, std::size_t receive_bytes) noexcept {
        return set(SizeOption::SendBufferSize, send_bytes)
              .set(SizeOption::ReceiveBufferSize, receive_bytes);
    }

    SocketOptions& apply_high_throughput() noexcept {
        return set(BooleanOption::TCPNoDelay, false)
              .set(SizeOption::SendBufferSize, 262144)
              .set(SizeOption::ReceiveBufferSize, 262144);
    }

public: // UDP presets
    SocketOptions& apply_udp_defaults() noexcept {
        return set(BooleanOption::ReuseAddress, true)
              .set(SizeOption::SendBufferSize, 65536)
              .set(SizeOption::ReceiveBufferSize, 65536);
    }

    SocketOptions& apply_multicast_defaults() noexcept {
        return apply_udp_defaults().set(BooleanOption::Broadcast, false);
    }

public: // Cross-protocol presets
    SocketOptions& apply_realtime() noexcept {
        return set(BooleanOption::TCPNoDelay, true)
              .set(BooleanOption::KeepAlive, true)
              .set_keepalive_params(5000, 1000)       
              .set(SizeOption::SendBufferSize, 8192)
              .set(SizeOption::ReceiveBufferSize, 8192);
    }

    SocketOptions& apply_bulk_transfer() noexcept {
        return set(BooleanOption::TCPNoDelay, false)
              .set(SizeOption::SendBufferSize, 524288)
              .set(SizeOption::ReceiveBufferSize, 524288)
              .linger(true, 30);
    }

public:
    bool set_quick_ack(bool enable) noexcept {
    #if defined(OS_LINUX) && defined(TCP_QUICKACK)
        auto native = resolve(OptionCode::QuickAck);
        if (!native.supported) return false;
        int val = enable ? 1 : 0;
        return m_socket.set_option(native.level, native.option_name, val) == 0;
    #else
        (void)enable;
        return false;
    #endif
    }

    bool enable_fast_open_listener(int queue_length = 16) noexcept {
        auto native = resolve(OptionCode::TCPFastOpen);
        if (!native.supported) return false;
    #if defined(OS_WINDOWS)
        int val = 1;                        
        return m_socket.set_option(native.level, native.option_name, val) == 0;
    #else
        if (queue_length < 1) queue_length = 1;
        return m_socket.set_option(native.level, native.option_name, queue_length) == 0;
    #endif
    }

    bool enable_fast_open_client() noexcept {
    #if defined(OS_LINUX) && defined(TCP_FASTOPEN_CONNECT)
        int on = 1;
        return m_socket.set_option(IPPROTO_TCP, TCP_FASTOPEN_CONNECT, on) == 0;
    #else
        return false;
    #endif
    }

private:
    void get_opt(int level, int option, void* value, int* length) const noexcept {
        auto* impl = const_cast<SocketBase&>(m_socket).get_impl();
        if (impl) { impl->get_option(level, option, value, length); }
    }
};

inline SocketOptions options(SocketBase& socket) noexcept { return SocketOptions(socket); }

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_OPTIONS_HPP