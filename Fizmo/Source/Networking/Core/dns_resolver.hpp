#ifndef FIZMO_DNS_RESOLVER_CLASSES_HPP
#define FIZMO_DNS_RESOLVER_CLASSES_HPP

#include "io_service.hpp"
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
    PTR     // Pointer (reverse lookup)
};

class DNSResult {
public:
    struct AddressInfo {
        std::string address;
        AddressFamily family;
        unsigned int port;
        
        AddressInfo(const std::string& addr, AddressFamily fam, const unsigned int p = 0) : address(addr), family(fam), port(p) {}
    };

private:
    std::vector<AddressInfo> m_addresses;
    std::string m_hostname;
    SocketError m_error;
    std::chrono::steady_clock::time_point m_resolve_time;
    std::chrono::milliseconds m_duration;

public:
    DNSResult() : m_error(ErrorCode::Success) {}
    
    DNSResult(const std::string& hostname, const std::vector<AddressInfo>& addresses)
        : m_addresses(addresses), m_hostname(hostname), m_error(ErrorCode::Success) {
        m_resolve_time = std::chrono::steady_clock::now();
    }
    
    DNSResult(const std::string& hostname, const SocketError& error)
        : m_hostname(hostname), m_error(error) {
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
    bool has_ipv4() const noexcept { return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv4; }); }
    bool has_ipv6() const noexcept { return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv6; }); }
    
    std::vector<std::string> get_ipv4_addresses() const {
        std::vector<std::string> ipv4_addrs;
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { ipv4_addrs.push_back(addr.address); }}
        return ipv4_addrs;
    }
    
    std::vector<std::string> get_ipv6_addresses() const {
        std::vector<std::string> ipv6_addrs;
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { ipv6_addrs.push_back(addr.address); }}
        return ipv6_addrs;
    }
    
    std::string first_ipv4() const {
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { return addr.address; }}
        return "";
    }
    
    std::string first_ipv6() const {
        for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { return addr.address; }}
        return "";
    }
    
    std::string first_address() const { return m_addresses.empty() ? "" : m_addresses[0].address; }
    
    std::vector<NetworkAddress> to_network_addresses(const std::uint16_t port = 0) const {
        std::vector<NetworkAddress> network_addrs;
        for (const auto& addr : m_addresses) { network_addrs.emplace_back(addr.address, port > 0 ? port : addr.port); }
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

#ifdef OS_WINDOWS
    static bool initialize_winsock() {
        static bool result = [] {
            WSADATA wsaData;
            return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
        }();

        return result;
    }
#endif

public:
    DNSResolver() 
        : m_io_service(nullptr), m_owns_io_service(false), 
          m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any), 
          m_use_cache(true) {
#ifdef OS_WINDOWS
        initialize_winsock();
#endif
    }
    
    explicit DNSResolver(IOService* io_service) 
        : m_io_service(io_service), m_owns_io_service(false),
          m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any),
          m_use_cache(true) {
#ifdef OS_WINDOWS
        initialize_winsock();
#endif
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
        
        if (m_use_cache) {
            auto cached = get_from_cache(hostname);
            if (cached.is_success()) { return cached; }
        }
        
        auto start_time = std::chrono::steady_clock::now();
        DNSResult result = perform_resolution(hostname, family);
        auto end_time = std::chrono::steady_clock::now();
        result.set_duration(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
        if (m_use_cache && result.is_success()) { add_to_cache(hostname, result); }
        return result;
    }
    
    void resolve_async(const std::string& hostname, std::function<void(const DNSResult&)> callback, AddressFamily family = AddressFamily::Any) {
        {
            std::lock_guard<std::mutex> lock(m_resolver_mutex);

            if (!m_io_service) {
                m_io_service = new IOService();
                m_owns_io_service = true;
                m_io_service->start(1);
            }
        }

        m_io_service->post([this, hostname, callback, family]() {
            DNSResult result = resolve(hostname, family);
            callback(result);
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
        if (!m_io_service) {
            std::lock_guard<std::mutex> lock(m_resolver_mutex);

            if (!m_io_service) {
                m_io_service = new IOService();
                m_owns_io_service = true;
                m_io_service->start(1);
            }
        }
        
        m_io_service->post([this, ip_address, callback]() {
            DNSResult result = reverse_lookup(ip_address);
            callback(result);
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

private:
    DNSResult perform_resolution(const std::string& hostname, AddressFamily family) {
#ifdef OS_WINDOWS
        struct addrinfo hints, *result = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = static_cast<int>(family);
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_ADDRCONFIG;
        int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
        
        if (status != 0) { return DNSResult(hostname, WinsockErrorConverter::convert(WSAGetLastError(), "getaddrinfo")); }
        
        std::vector<DNSResult::AddressInfo> addresses;

        for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            char addr_str[INET6_ADDRSTRLEN];
            
            if (ptr->ai_family == AF_INET) {
                struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(ptr->ai_addr);
                inet_ntop(AF_INET, &(ipv4->sin_addr), addr_str, INET_ADDRSTRLEN);
                addresses.emplace_back(addr_str, AddressFamily::IPv4, ntohs(ipv4->sin_port));
            } else if (ptr->ai_family == AF_INET6) {
                struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(ptr->ai_addr);
                inet_ntop(AF_INET6, &(ipv6->sin6_addr), addr_str, INET6_ADDRSTRLEN);
                addresses.emplace_back(addr_str, AddressFamily::IPv6, ntohs(ipv6->sin6_port));
            }
        }
        
        freeaddrinfo(result);
        if (addresses.empty()) { return DNSResult(hostname, SocketError(ErrorCode::NameResolutionFailed, "No addresses found")); }
        return DNSResult(hostname, addresses);
#endif
    }
    
    DNSResult perform_reverse_lookup(const std::string& ip_address) {
#ifdef OS_WINDOWS
        struct sockaddr_storage addr;
        std::memset(&addr, 0, sizeof(addr));
        struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(&addr);

        if (inet_pton(AF_INET, ip_address.c_str(), &(ipv4->sin_addr)) == 1) {
            ipv4->sin_family = AF_INET;
        } else {
            struct sockaddr_in6* ipv6 = reinterpret_cast<struct sockaddr_in6*>(&addr);

            if (inet_pton(AF_INET6, ip_address.c_str(), &(ipv6->sin6_addr)) == 1) {
                ipv6->sin6_family = AF_INET6;
            } else {
                return DNSResult("", SocketError(ErrorCode::InvalidArgument, "Invalid IP address format"));
            }
        }
        
        char hostname[NI_MAXHOST];

        int status = getnameinfo(
            reinterpret_cast<struct sockaddr*>(&addr), 
            addr.ss_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
            hostname, 
            NI_MAXHOST, 
            nullptr, 
            0, 
            NI_NAMEREQD
        );
        
        if (status != 0) { return DNSResult("", WinsockErrorConverter::convert(WSAGetLastError(), "getnameinfo")); }
        std::vector<DNSResult::AddressInfo> addresses;
        AddressFamily family = addr.ss_family == AF_INET ? AddressFamily::IPv4 : AddressFamily::IPv6;
        addresses.emplace_back(ip_address, family);
        return DNSResult(hostname, addresses);
#endif
    }
    
    bool is_ip_address(const std::string& str) const { return is_ipv4(str) || is_ipv6(str); }
    
    bool is_ipv4(const std::string& str) const {
#ifdef OS_WINDOWS
        struct sockaddr_in sa;
        return inet_pton(AF_INET, str.c_str(), &(sa.sin_addr)) == 1;
#endif
    }
    
    bool is_ipv6(const std::string& str) const {
#ifdef OS_WINDOWS
        struct sockaddr_in6 sa;
        return inet_pton(AF_INET6, str.c_str(), &(sa.sin6_addr)) == 1;
#endif
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