#ifndef FIZMO_DNS_RESOLVER_CLASSES_HPP
#define FIZMO_DNS_RESOLVER_CLASSES_HPP

#include "io_service.hpp"
#include "Socket Impl/native_resolver.hpp"
#include <future>

namespace fizmo {
namespace networking {
namespace core {

enum class DNSQueryType {
    A = 0,  // IPv4 address
    AAAA,   // IPv6 address
    CNAME,  // Canonical name
    MX,     // Mail exchange
    TXT,    // Text record
    PTR     // Pointer 
};

class DNSResult {
public:
    struct AddressInfo {
        std::string address;
        AddressFamily family;
        unsigned int port;

        AddressInfo(const std::string& addr, AddressFamily fam, const unsigned int p = 0)
            : address(addr), family(fam), port(p) {}
    };

private:
    std::vector<AddressInfo> m_addresses;
    std::string m_hostname;
    SocketError m_error;
    std::chrono::steady_clock::time_point m_resolve_time;
    std::chrono::milliseconds m_duration;

public:
    DNSResult() : m_error(ErrorCode::Success), m_duration(0) {}

    DNSResult(const std::string& hostname, const std::vector<AddressInfo>& addresses) : m_addresses(addresses), m_hostname(hostname), m_error(ErrorCode::Success), m_duration(0) {
        m_resolve_time = std::chrono::steady_clock::now();
    }

    DNSResult(const std::string& hostname, const SocketError& error) : m_hostname(hostname), m_error(error), m_duration(0) {
        m_resolve_time = std::chrono::steady_clock::now();
    }

public:
    bool is_success() const noexcept { return m_error.is_success() && !m_addresses.empty(); }
    bool has_error() const noexcept { return m_error.is_error(); }
    const SocketError& error() const noexcept { return m_error; }
    const std::vector<AddressInfo>& addresses() const noexcept { return m_addresses; }
    const std::string& hostname() const noexcept { return m_hostname; }
    std::chrono::milliseconds duration() const noexcept { return m_duration; }
    std::chrono::steady_clock::time_point resolve_time() const noexcept { return m_resolve_time; }

    bool has_ipv4() const noexcept {
        return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv4; });
    }

    bool has_ipv6() const noexcept {
        return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv6; });
    }

    std::vector<std::string> get_ipv4_addresses() const {
        std::vector<std::string> ipv4_addrs;
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { ipv4_addrs.push_back(addr.address); } }
        return ipv4_addrs;
    }

    std::vector<std::string> get_ipv6_addresses() const {
        std::vector<std::string> ipv6_addrs;
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { ipv6_addrs.push_back(addr.address); } }
        return ipv6_addrs;
    }

    std::string first_ipv4() const {
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { return addr.address; } }
        return "";
    }

    std::string first_ipv6() const {
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { return addr.address; } }
        return "";
    }

    std::string first_address() const { return m_addresses.empty() ? "" : m_addresses[0].address; }

    std::vector<NetworkAddress> to_network_addresses(const std::uint16_t port = 0) const {
        std::vector<NetworkAddress> network_addrs;
        for (const auto& addr : m_addresses) { network_addrs.emplace_back(addr.address, port > 0 ? port : static_cast<std::uint16_t>(addr.port)); }
        return network_addrs;
    }

public:
    void set_duration(const std::chrono::milliseconds dur) { m_duration = dur; }
    void add_address(const std::string& address, AddressFamily family, const unsigned int port = 0) { m_addresses.emplace_back(address, family, port); }
    void set_error(const SocketError& error) { m_error = error; }
};

class DNSResolver {
private:
    mutable std::mutex m_resolver_mutex;
    IOService* m_io_service;
    bool m_owns_io_service;
    mutable std::mutex m_cache_mutex;
    mutable std::map<std::string, DNSResult> m_cache;
    std::chrono::seconds m_cache_ttl;
    mutable std::map<std::string, std::chrono::steady_clock::time_point> m_cache_timestamps;
    std::chrono::seconds m_timeout;
    AddressFamily m_preferred_family;
    bool m_use_cache;
    std::vector<std::string> m_dns_servers;

public:
    DNSResolver()
        : m_io_service(nullptr), m_owns_io_service(false),
          m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any),
          m_use_cache(true) {
        detail::NameResolver::ensure_initialized();
    }

    explicit DNSResolver(IOService* io_service)
        : m_io_service(io_service), m_owns_io_service(false),
          m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any),
          m_use_cache(true) {
        detail::NameResolver::ensure_initialized();
    }

    ~DNSResolver() {
        if (m_owns_io_service && m_io_service) {
            m_io_service->stop();
            delete m_io_service;
            m_io_service = nullptr;
        }
    }

    DNSResolver(const DNSResolver&) = delete;
    DNSResolver& operator=(const DNSResolver&) = delete;

    DNSResolver(DNSResolver&& other) noexcept
        : m_io_service(other.m_io_service), m_owns_io_service(other.m_owns_io_service),
          m_cache(std::move(other.m_cache)), m_cache_ttl(other.m_cache_ttl),
          m_cache_timestamps(std::move(other.m_cache_timestamps)),
          m_timeout(other.m_timeout), m_preferred_family(other.m_preferred_family),
          m_use_cache(other.m_use_cache), m_dns_servers(std::move(other.m_dns_servers)) {
        other.m_io_service = nullptr;
        other.m_owns_io_service = false;
    }

public:
    DNSResult resolve(const std::string& hostname, AddressFamily family = AddressFamily::Any) {
        if (hostname.empty()) { return DNSResult(hostname, SocketError(ErrorCode::InvalidArgument, "Empty hostname")); }

        if (is_ip_address(hostname)) {
            DNSResult result;
            result.add_address(hostname, is_ipv4(hostname) ? AddressFamily::IPv4 : AddressFamily::IPv6);
            return result;
        }

        if (cache_enabled()) {
            auto cached = get_from_cache(hostname);
            if (cached.is_success()) { return cached; }
        }

        auto start_time = std::chrono::steady_clock::now();
        DNSResult result = perform_resolution(hostname, family);
        auto end_time = std::chrono::steady_clock::now();
        result.set_duration(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
        if (cache_enabled() && result.is_success()) { add_to_cache(hostname, result); }
        return result;
    }

    void resolve_async(const std::string& hostname, std::function<void(const DNSResult&)> callback, AddressFamily family = AddressFamily::Any) {
        IOService* service = ensure_io_service();
        if (!service) return;

        service->post([this, hostname, callback, family]() {
            DNSResult result = resolve(hostname, family);
            if (callback) { callback(result); }
        });
    }

    std::future<DNSResult> resolve_async(const std::string& hostname, AddressFamily family = AddressFamily::Any) {
        auto promise = std::make_shared<std::promise<DNSResult>>();
        auto future = promise->get_future();
        resolve_async(hostname, [promise](const DNSResult& result) { promise->set_value(result); }, family);
        return future;
    }

    DNSResult reverse_lookup(const std::string& ip_address) {
        if (ip_address.empty()) { return DNSResult("", SocketError(ErrorCode::InvalidArgument, "Empty IP address")); }
        auto start_time = std::chrono::steady_clock::now();
        DNSResult result = perform_reverse_lookup(ip_address);
        auto end_time = std::chrono::steady_clock::now();
        result.set_duration(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
        return result;
    }

    void reverse_lookup_async(const std::string& ip_address, std::function<void(const DNSResult&)> callback) {
        IOService* service = ensure_io_service();
        if (!service) return;

        service->post([this, ip_address, callback]() {
            DNSResult result = reverse_lookup(ip_address);
            if (callback) { callback(result); }
        });
    }

public:
    void set_timeout(const std::chrono::seconds timeout) {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        m_timeout = timeout;
    }

    void set_preferred_family(AddressFamily family) {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        m_preferred_family = family;
    }

    void set_cache_enabled(const bool enabled) {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        m_use_cache = enabled;
    }

    void set_cache_ttl(const std::chrono::seconds ttl) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_cache_ttl = ttl;
    }

    void clear_cache() {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_cache.clear();
        m_cache_timestamps.clear();
    }

    void add_dns_server(const std::string& server) {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        m_dns_servers.push_back(server);
    }

    void clear_dns_servers() {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        m_dns_servers.clear();
    }

public:
    std::chrono::seconds timeout() const {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        return m_timeout;
    }

    AddressFamily preferred_family() const {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        return m_preferred_family;
    }

    bool cache_enabled() const {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);
        return m_use_cache;
    }

    std::size_t cache_size() const {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        return m_cache.size();
    }

public:
    bool is_ip_address(const std::string& str) const { return detail::NameResolver::is_ip_address(str); }
    bool is_ipv4(const std::string& str) const { return detail::NameResolver::is_ipv4(str); }
    bool is_ipv6(const std::string& str) const { return detail::NameResolver::is_ipv6(str); }

private:
    IOService* ensure_io_service() {
        std::lock_guard<std::mutex> lock(m_resolver_mutex);

        if (!m_io_service) {
            m_io_service = new (std::nothrow) IOService();
            if (!m_io_service) return nullptr;
            m_owns_io_service = true;
            m_io_service->start(1);
        }

        return m_io_service;
    }

    DNSResult perform_resolution(const std::string& hostname, AddressFamily family) {
        auto outcome = detail::NameResolver::forward(hostname, family);

        if (!outcome.ok) {
            if (outcome.addresses.empty() && outcome.native_code == 0) { return DNSResult(hostname, SocketError(ErrorCode::NameResolutionFailed, "No addresses found")); }
            return DNSResult(hostname, detail::NameResolver::to_error(outcome, "getaddrinfo"));
        }

        std::vector<DNSResult::AddressInfo> addresses;
        addresses.reserve(outcome.addresses.size());
        for (const auto& entry : outcome.addresses) { addresses.emplace_back(entry.address, entry.family, entry.port); }
        return DNSResult(hostname, addresses);
    }

    DNSResult perform_reverse_lookup(const std::string& ip_address) {
        auto outcome = detail::NameResolver::reverse(ip_address);

        if (!outcome.ok) {
            if (outcome.native_code == 0) { return DNSResult("", SocketError(ErrorCode::InvalidArgument, "Invalid IP address format")); }
            return DNSResult("", detail::NameResolver::to_error(outcome, "getnameinfo"));
        }

        std::vector<DNSResult::AddressInfo> addresses;
        for (const auto& entry : outcome.addresses) { addresses.emplace_back(entry.address, entry.family, entry.port); }
        return DNSResult(outcome.canonical_name, addresses);
    }

    DNSResult get_from_cache(const std::string& hostname) const {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_cache.find(hostname);
        if (it == m_cache.end()) { return DNSResult(); }
        auto timestamp_it = m_cache_timestamps.find(hostname);
        if (timestamp_it == m_cache_timestamps.end()) { return DNSResult(); }
        auto now = std::chrono::steady_clock::now();
        auto age = now - timestamp_it->second;

        if (age > m_cache_ttl) {
            m_cache.erase(it);
            m_cache_timestamps.erase(timestamp_it);
            return DNSResult();
        }

        return it->second;
    }

    void add_to_cache(const std::string& hostname, const DNSResult& result) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_cache[hostname] = result;
        m_cache_timestamps[hostname] = std::chrono::steady_clock::now();
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_DNS_RESOLVER_CLASSES_HPP