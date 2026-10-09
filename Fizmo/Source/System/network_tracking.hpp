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

    void finish(NetworkSample& s, double wall) {
        for (NetworkInterfaceSample& i : s.interfaces) {
            const auto it = m_prev.find(i.name);
            if (it != m_prev.end() && wall > 0.0) {
                i.rx_bytes_per_sec   = rate(i.rx_bytes, it->second.rx_bytes, wall);
                i.tx_bytes_per_sec   = rate(i.tx_bytes, it->second.tx_bytes, wall);
                i.rx_packets_per_sec = rate(m_packets_rx[i.name], it->second.rx_packets, wall);
                i.tx_packets_per_sec = rate(m_packets_tx[i.name], it->second.tx_packets, wall);
            }
            if (i.link_mbps && *i.link_mbps > 0.0) i.utilization_percent = detail::clamp_percent(100.0 * std::max(i.rx_bytes_per_sec, i.tx_bytes_per_sec) * 8.0 / (*i.link_mbps * 1.0e6));
            if (!i.loopback) {
                s.rx_bytes_per_sec += i.rx_bytes_per_sec;
                s.tx_bytes_per_sec += i.tx_bytes_per_sec;
            }
        }

        m_prev.clear();
        for (const NetworkInterfaceSample& i : s.interfaces) m_prev[i.name] = { i.rx_bytes, i.tx_bytes, m_packets_rx[i.name], m_packets_tx[i.name] };
        m_packets_rx.clear();
        m_packets_tx.clear();

        const std::uint64_t sent = m_app_sent.load(), recv = m_app_received.load();
        s.app_sent = sent;
        s.app_received = recv;
        s.app_sent_per_sec = rate(sent, m_prev_app_sent, wall);
        s.app_received_per_sec = rate(recv, m_prev_app_received, wall);
        m_prev_app_sent = sent;
        m_prev_app_received = recv;
    }

    std::map<std::string, std::uint64_t> m_packets_rx;
    std::map<std::string, std::uint64_t> m_packets_tx;

#if defined(OS_LINUX)
    using Socket = int;
    static constexpr Socket kBadSocket = -1;
    static void close_socket(Socket s) { ::close(s); }
    static bool set_nonblocking(Socket s) {
        const int flags = ::fcntl(s, F_GETFL, 0);
        return flags >= 0 && ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
    }
    static bool in_progress() { return errno == EINPROGRESS; }

    static bool wait_writable(Socket s, int timeout_ms) {
        pollfd p{};
        p.fd = s;
        p.events = POLLOUT;
        return ::poll(&p, 1, timeout_ms) == 1 && (p.revents & POLLOUT);
    }

    std::map<std::string, std::vector<std::string>> addresses() const {
        std::map<std::string, std::vector<std::string>> out;
        ifaddrs* list = nullptr;
        if (::getifaddrs(&list) != 0) return out;

        for (ifaddrs* a = list; a; a = a->ifa_next) {
            if (!a->ifa_addr || !a->ifa_name) continue;
            char buf[INET6_ADDRSTRLEN] = {};
            if (a->ifa_addr->sa_family == AF_INET) ::inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr, buf, sizeof(buf));
            else if (a->ifa_addr->sa_family == AF_INET6) ::inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(a->ifa_addr)->sin6_addr, buf, sizeof(buf));
            else continue;
            out[a->ifa_name].push_back(buf);
        }

        ::freeifaddrs(list);
        return out;
    }

    std::map<std::string, double> signals() const {
        std::map<std::string, double> out;
        std::string text;
        if (!detail::lnx::read_text("/proc/net/wireless", text)) return out;
        for (const std::string& line : detail::split_lines(text)) {
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            const std::vector<std::string> f = detail::split_ws(line.substr(colon + 1));
            if (f.size() < 3) continue;
            std::string level = f[2];
            if (!level.empty() && level.back() == '.') level.pop_back();
            if (const auto v = detail::parse_double(level)) out[detail::trim(line.substr(0, colon))] = *v;
        }
        return out;
    }

    void discover() {
        NetworkSample s;
        read_interfaces(s, false);
        finish(s, 0.0);
    }

    void read_interfaces(NetworkSample& s, bool details) {
        std::string text;
        if (!detail::lnx::read_text("/proc/net/dev", text)) return;
        std::map<std::string, std::vector<std::string>> addrs;
        std::map<std::string, double> sig;
        if (details) { addrs = addresses(); sig = signals(); }

        for (const std::string& line : detail::split_lines(text)) {
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) continue;
            const std::vector<std::string> f = detail::split_ws(line.substr(colon + 1));
            if (f.size() < 16) continue;
            auto at = [&](std::size_t i) { return detail::parse_u64(f[i]).value_or(0); };
            NetworkInterfaceSample i;
            i.name       = detail::trim(line.substr(0, colon));
            i.rx_bytes   = at(0);
            i.rx_errors  = at(2);
            i.rx_dropped = at(3);
            i.tx_bytes   = at(8);
            i.tx_errors  = at(10);
            i.tx_dropped = at(11);
            m_packets_rx[i.name] = at(1);
            m_packets_tx[i.name] = at(9);

            if (details) {
                const std::string dir = "/sys/class/net/" + i.name;
                const std::string oper = detail::lnx::read_line(dir + "/operstate");
                i.loopback = detail::lnx::read_u64(dir + "/type").value_or(0) == 772;
                i.up       = oper == "up" || (oper == "unknown" && (detail::lnx::read_hex(dir + "/flags").value_or(0) & 0x1u) != 0);
                i.wireless = detail::lnx::exists(dir + "/wireless") || detail::lnx::exists(dir + "/phy80211");
                i.mac      = detail::lnx::read_line(dir + "/address");
                i.mtu      = static_cast<std::uint32_t>(detail::lnx::read_u64(dir + "/mtu").value_or(0));
                if (i.up) if (const auto sp = detail::lnx::read_i64(dir + "/speed")) if (*sp > 0 && *sp < 10000000) i.link_mbps = static_cast<double>(*sp);
                const auto a = addrs.find(i.name);
                if (a != addrs.end()) i.addresses = a->second;
                const auto g = sig.find(i.name);
                if (g != sig.end()) i.signal_dbm = g->second;
                const std::string driver = detail::lnx::link_name(dir + "/device/driver");
                if (!driver.empty()) i.description = driver;
            }

            s.interfaces.push_back(std::move(i));
        }
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

    bool resolve(const std::string& host, const std::string& port) {
        m_addr.clear();
#if defined(OS_LINUX) || defined(OS_WINDOWS)
        addrinfo hints{};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) return false;
        m_addr.assign(reinterpret_cast<const unsigned char*>(res->ai_addr), reinterpret_cast<const unsigned char*>(res->ai_addr) + res->ai_addrlen);
        m_family = res->ai_family;
        ::freeaddrinfo(res);
        return true;
#else
        (void)host; (void)port;
        return false;
#endif
    }

    std::optional<double> connect_once(int timeout_ms) {
#if defined(OS_LINUX) || defined(OS_WINDOWS)
        if (m_addr.empty()) return std::nullopt;
        const Socket s = ::socket(m_family, SOCK_STREAM, IPPROTO_TCP);
        if (s == kBadSocket) return std::nullopt;
        if (!set_nonblocking(s)) { close_socket(s); return std::nullopt; }
        const detail::Clock::time_point t0 = detail::Clock::now();
        const int r = ::connect(s, reinterpret_cast<const sockaddr*>(m_addr.data()), static_cast<int>(m_addr.size()));
        bool ok = r == 0;

        if (!ok && in_progress() && wait_writable(s, timeout_ms)) {
            int err = 0;
            socklen_t len = sizeof(err);
            ok = ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len) == 0 && err == 0;
        }

        const detail::Clock::time_point t1 = detail::Clock::now();
        close_socket(s);
        if (!ok) return std::nullopt;
        return detail::seconds_between(t0, t1) * 1000.0;
#else
        (void)timeout_ms;
        return std::nullopt;
#endif
    }

    void probe(NetworkSample& s) {
        std::string host, port;
        std::chrono::milliseconds every, timeout;
        {
            std::lock_guard<std::mutex> lk(m_target_mutex);
            host = m_host;
            port = m_port;
            every = m_probe_every;
            timeout = m_probe_timeout;
        }

        if (host.empty()) { m_last_latency.reset(); m_last_failed = false; return; }
        s.latency_target = host + ":" + port;
        const detail::Clock::time_point now = detail::Clock::now();

        if (!m_probed || now - m_last_probe >= every || m_resolved_for != s.latency_target) {
            m_probed = true;
            m_last_probe = now;
            if (m_resolved_for != s.latency_target || m_addr.empty()) {
                m_resolved_for = s.latency_target;
                resolve(host, port);
            }
            m_last_latency = connect_once(static_cast<int>(timeout.count()));
            m_last_failed = !m_last_latency;
            if (m_last_failed) m_addr.clear();
        }

        s.latency_ms = m_last_latency;
        s.latency_failed = m_last_failed;
    }

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

    void set_latency_target(const std::string& host, std::uint16_t port = 443, std::chrono::milliseconds every = std::chrono::milliseconds(2000), std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        std::lock_guard<std::mutex> lk(m_target_mutex);
        m_host = host;
        m_port = std::to_string(port);
        m_probe_every = every;
        m_probe_timeout = timeout;
    }

    void clear_latency_target() {
        std::lock_guard<std::mutex> lk(m_target_mutex);
        m_host.clear();
    }

    void record_sent(std::uint64_t bytes) noexcept { m_app_sent.fetch_add(bytes, std::memory_order_relaxed); }
    void record_received(std::uint64_t bytes) noexcept { m_app_received.fetch_add(bytes, std::memory_order_relaxed); }

    std::string report() const {
        const NetworkSample s = latest();
        std::string r = "Network  down " + format_bitrate(s.rx_bytes_per_sec) + ", up " + format_bitrate(s.tx_bytes_per_sec);
        if (!s.latency_target.empty()) r += ", latency " + (s.latency_ms ? detail::fixed(*s.latency_ms, 1) + " ms" : std::string("unreachable")) + " (" + s.latency_target + ")";
        r += "\n";

        for (const NetworkInterfaceSample& i : s.interfaces) {
            if (i.loopback) continue;
            r += "  " + i.name + (i.up ? " up" : " down");
            if (i.link_mbps) r += " " + detail::fixed(*i.link_mbps, 0) + " Mbit/s";
            if (i.wireless) r += " wifi";
            if (i.signal_dbm) r += " " + detail::fixed(*i.signal_dbm, 0) + " dBm";
            r += "  down " + format_bitrate(i.rx_bytes_per_sec) + "  up " + format_bitrate(i.tx_bytes_per_sec);
            if (!i.addresses.empty()) r += "  " + i.addresses.front();
            r += "\n";
        }

        if (s.app_sent || s.app_received) r += "  app      sent " + format_rate(s.app_sent_per_sec) + ", received " + format_rate(s.app_received_per_sec) + "\n";
        return r;
    }
};

} // namespace system
} // namespace fizmo

#endif // FIZMO_SYSTEM_NETWORK_TRACKING_HPP
