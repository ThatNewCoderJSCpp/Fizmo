#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "socket_options.hpp"

namespace fizmo {
namespace networking {
namespace core {

auto SocketOptions::resolve(OptionCode code) noexcept -> NativeOption {
    #ifdef OS_WINDOWS
        return WindowsOptions::resolve(code);
    #elif defined(OS_LINUX)
        return LinuxOptions::resolve(code);
    #else
        return NativeOption();
    #endif
    }

auto SocketOptions::set(BooleanOption opt, bool enable) noexcept -> SocketOptions& {
        auto native = resolve(opt.code());

        if (native.supported) {
            int val = enable ? 1 : 0;
            m_socket.set_option(native.level, native.option_name, val);
        }

        return *this;
    }

auto SocketOptions::get(BooleanOption opt) const noexcept -> bool {
        auto native = resolve(opt.code());
        if (!native.supported) return false;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return val != 0;
    }

auto SocketOptions::set(IntegerOption opt, int value) noexcept -> SocketOptions& {
        auto native = resolve(opt.code());
        if (native.supported) { m_socket.set_option(native.level, native.option_name, value); }
        return *this;
    }

auto SocketOptions::get(IntegerOption opt) const noexcept -> int {
        auto native = resolve(opt.code());
        if (!native.supported) return 0;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return val;
    }

auto SocketOptions::set(SizeOption opt, std::size_t value) noexcept -> SocketOptions& {
        auto native = resolve(opt.code());

        if (native.supported) {
            int val = static_cast<int>(value);
            m_socket.set_option(native.level, native.option_name, val);
        }

        return *this;
    }

auto SocketOptions::get(SizeOption opt) const noexcept -> std::size_t {
        auto native = resolve(opt.code());
        if (!native.supported) return 0;
        int val = 0;
        int len = sizeof(val);
        get_opt(native.level, native.option_name, &val, &len);
        return static_cast<std::size_t>(val);
    }

auto SocketOptions::set(TimevalOption opt, unsigned long seconds, unsigned long microseconds) noexcept -> SocketOptions& {
        auto native = resolve(opt.code());

        if (native.supported) {
            struct timeval tv;
            tv.tv_sec  = seconds;
            tv.tv_usec = microseconds;
            m_socket.set_option(native.level, native.option_name, tv);
        }

        return *this;
    }

auto SocketOptions::get(TimevalOption opt) const noexcept -> std::pair<unsigned long, unsigned long> {
        auto native = resolve(opt.code());
        if (!native.supported) return {0, 0};
        struct timeval tv = {0, 0};
        int len = sizeof(tv);
        get_opt(native.level, native.option_name, &tv, &len);
        return {static_cast<unsigned long>(tv.tv_sec), static_cast<unsigned long>(tv.tv_usec)};
    }

auto SocketOptions::linger(bool enable, unsigned short timeout_seconds) noexcept -> SocketOptions& {
        auto native = resolve(OptionCode::Linger);
        if (native.supported) {

            struct linger lg;
            lg.l_onoff  = enable ? 1 : 0;
            lg.l_linger = timeout_seconds;
            m_socket.set_option(native.level, native.option_name, lg);
        }

        return *this;
    }

auto SocketOptions::get_linger() const noexcept -> std::pair<bool, unsigned short> {
        auto native = resolve(OptionCode::Linger);
        if (!native.supported) return {false, 0};
        struct linger lg = {0, 0};
        int len = sizeof(lg);
        get_opt(native.level, native.option_name, &lg, &len);
        return {lg.l_onoff != 0, static_cast<unsigned short>(lg.l_linger)};
    }

auto SocketOptions::get_pending_error() const noexcept -> int {
        auto native = resolve(OptionCode::PendingError);
        if (!native.supported) return 0;
        int err = 0;
        int len = sizeof(err);
        get_opt(native.level, native.option_name, &err, &len);
        return err;
    }

auto SocketOptions::apply_server_defaults() noexcept -> SocketOptions& {
        return set(BooleanOption::ReuseAddress, true)
              .set(BooleanOption::KeepAlive, true)
              .linger(true, 5);
    }

auto SocketOptions::apply_low_latency() noexcept -> SocketOptions& {
        set(BooleanOption::TCPNoDelay, true);
    #ifdef OS_LINUX
        set_quick_ack(true);
    #endif
        return *this;
    }

auto SocketOptions::set_buffer_sizes(std::size_t send_bytes, std::size_t receive_bytes) noexcept -> SocketOptions& {
        return set(SizeOption::SendBufferSize, send_bytes)
              .set(SizeOption::ReceiveBufferSize, receive_bytes);
    }

auto SocketOptions::apply_high_throughput() noexcept -> SocketOptions& {
        return set(BooleanOption::TCPNoDelay, false)
              .set(SizeOption::SendBufferSize, 262144)
              .set(SizeOption::ReceiveBufferSize, 262144);
    }

auto SocketOptions::apply_udp_defaults() noexcept -> SocketOptions& {
        return set(BooleanOption::ReuseAddress, true)
              .set(SizeOption::SendBufferSize, 65536)
              .set(SizeOption::ReceiveBufferSize, 65536);
    }

auto SocketOptions::apply_realtime() noexcept -> SocketOptions& {
        return set(BooleanOption::TCPNoDelay, true)
              .set(BooleanOption::KeepAlive, true)
              .set_keepalive_params(5000, 1000)       
              .set(SizeOption::SendBufferSize, 8192)
              .set(SizeOption::ReceiveBufferSize, 8192);
    }

auto SocketOptions::apply_bulk_transfer() noexcept -> SocketOptions& {
        return set(BooleanOption::TCPNoDelay, false)
              .set(SizeOption::SendBufferSize, 524288)
              .set(SizeOption::ReceiveBufferSize, 524288)
              .linger(true, 30);
    }

auto SocketOptions::set_quick_ack(bool enable) noexcept -> bool {
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

auto SocketOptions::enable_fast_open_listener(int queue_length) noexcept -> bool {
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

auto SocketOptions::enable_fast_open_client() noexcept -> bool {
    #if defined(OS_LINUX) && defined(TCP_FASTOPEN_CONNECT)
        int on = 1;
        return m_socket.set_option(IPPROTO_TCP, TCP_FASTOPEN_CONNECT, on) == 0;
    #else
        return false;
    #endif
    }

auto SocketOptions::get_opt(int level, int option, void* value, int* length) const noexcept -> void {
        auto* impl = const_cast<SocketBase&>(m_socket).get_impl();
        if (impl) { impl->get_option(level, option, value, length); }
    }

} // namespace core
} // namespace networking
} // namespace fizmo
