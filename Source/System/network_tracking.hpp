#ifndef FIZMO_SYSTEM_NETWORK_TRACKING_HPP
#define FIZMO_SYSTEM_NETWORK_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "win_library.hpp"

#if defined(OS_LINUX)
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <cerrno>
#endif

namespace fizmo {
namespace system {

struct NetworkInterfaceSample {
    std::string              name;
    std::string              description;
    std::string              mac;
    std::vector<std::string> addresses;
    bool                     up       = false;
    bool                     loopback = false;
    bool                     wireless = false;
    std::optional<double>    link_mbps;
    std::uint32_t            mtu      = 0;

    double        rx_bytes_per_sec   = 0.0;
    double        tx_bytes_per_sec   = 0.0;
    double        rx_packets_per_sec = 0.0;
    double        tx_packets_per_sec = 0.0;
    std::uint64_t rx_bytes   = 0;
    std::uint64_t tx_bytes   = 0;
    std::uint64_t rx_errors  = 0;
    std::uint64_t tx_errors  = 0;
    std::uint64_t rx_dropped = 0;
    std::uint64_t tx_dropped = 0;
    std::optional<double> utilization_percent;
    std::optional<double> signal_dbm;
};

struct NetworkSample {
    double time       = 0.0;
    double collect_ms = 0.0;

    std::vector<NetworkInterfaceSample> interfaces;
    double                rx_bytes_per_sec = 0.0;
    double                tx_bytes_per_sec = 0.0;

    std::optional<double> latency_ms;
    std::string           latency_target;
    bool                  latency_failed = false;

    double        app_sent_per_sec     = 0.0;
    double        app_received_per_sec = 0.0;
    std::uint64_t app_sent             = 0;
    std::uint64_t app_received         = 0;
};

class NetworkTracking : public detail::Tracker<NetworkTracking, NetworkSample> {
private:
    friend class detail::Tracker<NetworkTracking, NetworkSample>;

    struct Counters {
        std::uint64_t rx_bytes = 0, tx_bytes = 0, rx_packets = 0, tx_packets = 0;
    };

    std::map<std::string, Counters> m_prev;
    detail::Clock::time_point       m_prev_time = detail::Clock::now();
    std::atomic<std::uint64_t>      m_app_sent{ 0 };
    std::atomic<std::uint64_t>      m_app_received{ 0 };
    std::uint64_t                   m_prev_app_sent     = 0;
    std::uint64_t                   m_prev_app_received = 0;

    std::mutex                      m_target_mutex;
    std::string                     m_host;
    std::string                     m_port;
    std::chrono::milliseconds       m_probe_every{ 2000 };
    std::chrono::milliseconds       m_probe_timeout{ 1000 };
    detail::Clock::time_point       m_last_probe{};
    bool                            m_probed = false;
    std::optional<double>           m_last_latency;
    bool                            m_last_failed = false;
    std::vector<unsigned char>      m_addr;
    int                             m_family = 0;
    std::string                     m_resolved_for;

    static double rate(std::uint64_t now, std::uint64_t before, double wall) { return detail::per_second(now, before, wall); }

    void finish(NetworkSample& s, double wall);

    std::map<std::string, std::uint64_t> m_packets_rx;
    std::map<std::string, std::uint64_t> m_packets_tx;

#if defined(OS_LINUX)
    using Socket = int;
    static constexpr Socket kBadSocket = -1;
    static void close_socket(Socket s) { ::close(s); }
    static bool set_nonblocking(Socket s);
    static bool in_progress() { return errno == EINPROGRESS; }

    static bool wait_writable(Socket s, int timeout_ms);

    std::map<std::string, std::vector<std::string>> addresses() const;

    std::map<std::string, double> signals() const;

    void discover() {
        NetworkSample s;
        read_interfaces(s, false);
        finish(s, 0.0);
    }

    void read_interfaces(NetworkSample& s, bool details);

    NetworkSample collect();

#elif defined(OS_WINDOWS)
    using Socket = std::uintptr_t;
    static constexpr Socket kBadSocket = ~Socket(0);
    static void close_socket(Socket s);
    static bool set_nonblocking(Socket s);
    static bool in_progress();

    static bool wait_writable(Socket s, int timeout_ms);

    struct WinState;
    detail::Opaque<WinState> m_win;

    std::map<unsigned long, std::vector<std::string>> addresses() const;

    void discover();

    void read_interfaces(NetworkSample& s, bool details);

    NetworkSample collect();

#else
    using Socket = int;
    static constexpr Socket kBadSocket = -1;
    void discover() {}
    NetworkSample collect() { NetworkSample s; probe(s); return s; }
#endif

    bool resolve(const std::string& host, const std::string& port);

    std::optional<double> connect_once(int timeout_ms);

    void probe(NetworkSample& s);

public:
    explicit NetworkTracking(std::chrono::milliseconds interval = std::chrono::milliseconds(1000), std::size_t history = 240)
        : Tracker(interval, history) {
        discover();
    }

    ~NetworkTracking();

    void set_latency_target(const std::string& host, std::uint16_t port = 443, std::chrono::milliseconds every = std::chrono::milliseconds(2000), std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    void clear_latency_target() {
        std::lock_guard<std::mutex> lk(m_target_mutex);
        m_host.clear();
    }

    void record_sent(std::uint64_t bytes) noexcept { m_app_sent.fetch_add(bytes, std::memory_order_relaxed); }
    void record_received(std::uint64_t bytes) noexcept { m_app_received.fetch_add(bytes, std::memory_order_relaxed); }

    std::string report() const;
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_NETWORK_TRACKING_HPP
