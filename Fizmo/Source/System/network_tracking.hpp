#ifndef FIZMO_SYSTEM_NETWORK_TRACKING_HPP
#define FIZMO_SYSTEM_NETWORK_TRACKING_HPP

#include "tracker.hpp"
#include "platform_linux.hpp"
#include "platform_windows.hpp"

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
    using Socket = SOCKET;
    static constexpr Socket kBadSocket = INVALID_SOCKET;
    static void close_socket(Socket s) { ::closesocket(s); }
    static bool set_nonblocking(Socket s) {
        u_long on = 1;
        return ::ioctlsocket(s, FIONBIO, &on) == 0;
    }
    static bool in_progress() { return WSAGetLastError() == WSAEWOULDBLOCK; }

    static bool wait_writable(Socket s, int timeout_ms) {
        fd_set w, e;
        FD_ZERO(&w);
        FD_ZERO(&e);
        FD_SET(s, &w);
        FD_SET(s, &e);
        timeval tv{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        return ::select(0, nullptr, &w, &e, &tv) > 0 && FD_ISSET(s, &w);
    }

    using GetIfTable2Fn  = DWORD (WINAPI*)(PMIB_IF_TABLE2*);
    using FreeMibTableFn = void (WINAPI*)(PVOID);
    using AdaptersFn     = ULONG (WINAPI*)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);

    detail::win::Library m_iphlp{ L"iphlpapi.dll" };
    GetIfTable2Fn        m_get_table = nullptr;
    FreeMibTableFn       m_free_table = nullptr;
    AdaptersFn           m_adapters = nullptr;
    bool                 m_wsa = false;

    std::map<NET_IFINDEX, std::vector<std::string>> addresses() const {
        std::map<NET_IFINDEX, std::vector<std::string>> out;
        if (!m_adapters) return out;
        ULONG size = 16384;
        std::vector<unsigned char> buf;
        ULONG r = ERROR_BUFFER_OVERFLOW;

        for (int attempt = 0; attempt < 3 && r == ERROR_BUFFER_OVERFLOW; ++attempt) {
            buf.resize(size);
            r = m_adapters(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()), &size);
        }

        if (r != NO_ERROR) return out;

        for (auto* a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()); a; a = a->Next) {
            for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                char text[INET6_ADDRSTRLEN] = {};
                const sockaddr* sa = u->Address.lpSockaddr;
                if (!sa) continue;
                if (sa->sa_family == AF_INET) ::inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr, text, sizeof(text));
                else if (sa->sa_family == AF_INET6) ::inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr, text, sizeof(text));
                else continue;
                out[a->IfIndex ? a->IfIndex : a->Ipv6IfIndex].push_back(text);
            }
        }

        return out;
    }

    void discover() {
        WSADATA data;
        m_wsa = WSAStartup(MAKEWORD(2, 2), &data) == 0;
        m_get_table  = m_iphlp.get<GetIfTable2Fn>("GetIfTable2");
        m_free_table = m_iphlp.get<FreeMibTableFn>("FreeMibTable");
        m_adapters   = m_iphlp.get<AdaptersFn>("GetAdaptersAddresses");
        NetworkSample s;
        read_interfaces(s, false);
        finish(s, 0.0);
    }

    void read_interfaces(NetworkSample& s, bool details) {
        if (!m_get_table || !m_free_table) return;
        PMIB_IF_TABLE2 table = nullptr;
        if (m_get_table(&table) != NO_ERROR || !table) return;
        std::map<NET_IFINDEX, std::vector<std::string>> addrs;
        if (details) addrs = addresses();

        for (ULONG k = 0; k < table->NumEntries; ++k) {
            const MIB_IF_ROW2& row = table->Table[k];
            if (row.InterfaceAndOperStatusFlags.FilterInterface) continue;
            const bool loopback = row.Type == IF_TYPE_SOFTWARE_LOOPBACK;
            const bool up = row.OperStatus == IfOperStatusUp;
            if (!row.InterfaceAndOperStatusFlags.HardwareInterface && !loopback && !(up && row.Type != IF_TYPE_TUNNEL)) continue;
            NetworkInterfaceSample i;
            i.name       = detail::win::narrow(row.Alias);
            if (i.name.empty()) i.name = "if" + std::to_string(row.InterfaceIndex);
            i.description = detail::win::narrow(row.Description);
            i.up         = up;
            i.loopback   = loopback;
            i.wireless   = row.Type == IF_TYPE_IEEE80211;
            i.mtu        = row.Mtu;
            i.rx_bytes   = row.InOctets;
            i.tx_bytes   = row.OutOctets;
            i.rx_errors  = row.InErrors;
            i.tx_errors  = row.OutErrors;
            i.rx_dropped = row.InDiscards;
            i.tx_dropped = row.OutDiscards;
            m_packets_rx[i.name] = row.InUcastPkts + row.InNUcastPkts;
            m_packets_tx[i.name] = row.OutUcastPkts + row.OutNUcastPkts;
            const std::uint64_t speed = std::max<std::uint64_t>(row.ReceiveLinkSpeed, row.TransmitLinkSpeed);
            if (up && speed > 0 && speed < (1ull << 50)) i.link_mbps = static_cast<double>(speed) / 1.0e6;

            if (row.PhysicalAddressLength > 0) {
                char mac[3 * IF_MAX_PHYS_ADDRESS_LENGTH + 1] = {};
                std::size_t at = 0;
                for (ULONG b = 0; b < row.PhysicalAddressLength && b < IF_MAX_PHYS_ADDRESS_LENGTH; ++b) {
                    at += static_cast<std::size_t>(std::snprintf(mac + at, sizeof(mac) - at, b ? ":%02x" : "%02x", row.PhysicalAddress[b]));
                }
                i.mac = mac;
            }

            const auto a = addrs.find(row.InterfaceIndex);
            if (a != addrs.end()) i.addresses = a->second;
            s.interfaces.push_back(std::move(i));
        }

        m_free_table(table);
    }

    NetworkSample collect() {
        NetworkSample s;
        read_interfaces(s, true);
        const detail::Clock::time_point now = detail::Clock::now();
        finish(s, detail::seconds_between(m_prev_time, now));
        m_prev_time = now;
        probe(s);
        return s;
    }

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

    ~NetworkTracking() {
        shutdown();
#if defined(OS_WINDOWS)
        if (m_wsa) WSACleanup();
#endif
    }

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
