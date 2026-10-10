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

    DNSResult(const std::string& hostname, const std::vector<AddressInfo>& addresses);

    DNSResult(const std::string& hostname, const SocketError& error);

public:
    bool is_success() const noexcept { return m_error.is_success() && !m_addresses.empty(); }
    bool has_error() const noexcept { return m_error.is_error(); }
    const SocketError& error() const noexcept { return m_error; }
    const std::vector<AddressInfo>& addresses() const noexcept { return m_addresses; }
    const std::string& hostname() const noexcept { return m_hostname; }
    std::chrono::milliseconds duration() const noexcept { return m_duration; }
    std::chrono::steady_clock::time_point resolve_time() const noexcept { return m_resolve_time; }

    bool has_ipv4() const noexcept;

    bool has_ipv6() const noexcept;

    std::vector<std::string> get_ipv4_addresses() const;

    std::vector<std::string> get_ipv6_addresses() const;

    std::string first_ipv4() const;

    std::string first_ipv6() const;

    std::string first_address() const { return m_addresses.empty() ? "" : m_addresses[0].address; }

    std::vector<NetworkAddress> to_network_addresses(const std::uint16_t port = 0) const;

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
;

    explicit DNSResolver(IOService* io_service)
;

    ~DNSResolver();

    DNSResolver(const DNSResolver&) = delete;
    DNSResolver& operator=(const DNSResolver&) = delete;

    DNSResolver(DNSResolver&& other) noexcept
;

public:
    DNSResult resolve(const std::string& hostname, AddressFamily family = AddressFamily::Any);

    void resolve_async(const std::string& hostname, std::function<void(const DNSResult&)> callback, AddressFamily family = AddressFamily::Any);

    std::future<DNSResult> resolve_async(const std::string& hostname, AddressFamily family = AddressFamily::Any);

    DNSResult reverse_lookup(const std::string& ip_address);

    void reverse_lookup_async(const std::string& ip_address, std::function<void(const DNSResult&)> callback);

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

    void clear_cache();

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
    IOService* ensure_io_service();

    DNSResult perform_resolution(const std::string& hostname, AddressFamily family);

    DNSResult perform_reverse_lookup(const std::string& ip_address);

    DNSResult get_from_cache(const std::string& hostname) const;

    void add_to_cache(const std::string& hostname, const DNSResult& result);
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_DNS_RESOLVER_CLASSES_HPP