#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "network_tracking.hpp"

namespace fizmo {
namespace system {

auto NetworkTracking::finish(NetworkSample& s, double wall) -> void {
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

#if defined(OS_LINUX)
auto NetworkTracking::set_nonblocking(Socket s) -> bool {
        const int flags = ::fcntl(s, F_GETFL, 0);
        return flags >= 0 && ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
    }
#endif

#if defined(OS_LINUX)
auto NetworkTracking::wait_writable(Socket s, int timeout_ms) -> bool {
        pollfd p{};
        p.fd = s;
        p.events = POLLOUT;
        return ::poll(&p, 1, timeout_ms) == 1 && (p.revents & POLLOUT);
    }
#endif

#if defined(OS_LINUX)
auto NetworkTracking::addresses() const -> std::map<std::string, std::vector<std::string>> {
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
#endif

#if defined(OS_LINUX)
auto NetworkTracking::signals() const -> std::map<std::string, double> {
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
#endif

#if defined(OS_LINUX)
auto NetworkTracking::read_interfaces(NetworkSample& s, bool details) -> void {
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
#endif

#if defined(OS_LINUX)
auto NetworkTracking::collect() -> NetworkSample {
        NetworkSample s;
        read_interfaces(s, true);
        const detail::Clock::time_point now = detail::Clock::now();
        finish(s, detail::seconds_between(m_prev_time, now));
        m_prev_time = now;
        probe(s);
        return s;
    }
#endif

auto NetworkTracking::resolve(const std::string& host, const std::string& port) -> bool {
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

auto NetworkTracking::connect_once(int timeout_ms) -> std::optional<double> {
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

auto NetworkTracking::probe(NetworkSample& s) -> void {
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

auto NetworkTracking::set_latency_target(const std::string& host, std::uint16_t port, std::chrono::milliseconds every, std::chrono::milliseconds timeout) -> void {
        std::lock_guard<std::mutex> lk(m_target_mutex);
        m_host = host;
        m_port = std::to_string(port);
        m_probe_every = every;
        m_probe_timeout = timeout;
    }

auto NetworkTracking::report() const -> std::string {
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

} // namespace system
} // namespace fizmo
