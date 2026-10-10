#include "fizmo_library.hpp"
#include "dns_resolver.hpp"

namespace fizmo {
namespace networking {
namespace core {

DNSResult::DNSResult(const std::string& hostname, const std::vector<AddressInfo>& addresses) : m_addresses(addresses), m_hostname(hostname), m_error(ErrorCode::Success), m_duration(0) {
    m_resolve_time = std::chrono::steady_clock::now();
}

DNSResult::DNSResult(const std::string& hostname, const SocketError& error) : m_hostname(hostname), m_error(error), m_duration(0) {
    m_resolve_time = std::chrono::steady_clock::now();
}

bool DNSResult::has_ipv4() const noexcept {
    return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv4; });
}

bool DNSResult::has_ipv6() const noexcept {
    return std::any_of(m_addresses.begin(), m_addresses.end(), [](const AddressInfo& addr) { return addr.family == AddressFamily::IPv6; });
}

std::vector<std::string> DNSResult::get_ipv4_addresses() const {
    std::vector<std::string> ipv4_addrs;
    for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { ipv4_addrs.push_back(addr.address); } }
    return ipv4_addrs;
}

std::vector<std::string> DNSResult::get_ipv6_addresses() const {
    std::vector<std::string> ipv6_addrs;
    for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { ipv6_addrs.push_back(addr.address); } }
    return ipv6_addrs;
}

std::string DNSResult::first_ipv4() const {
    for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv4) { return addr.address; } }
    return "";
}

std::string DNSResult::first_ipv6() const {
    for (const auto& addr : m_addresses) { if (addr.family == AddressFamily::IPv6) { return addr.address; } }
    return "";
}

auto DNSResult::to_network_addresses(const std::uint16_t port) const -> std::vector<NetworkAddress> {
    std::vector<NetworkAddress> network_addrs;
    for (const auto& addr : m_addresses) { network_addrs.emplace_back(addr.address, port > 0 ? port : static_cast<std::uint16_t>(addr.port)); }
    return network_addrs;
}

DNSResolver::DNSResolver() : m_io_service(nullptr), m_owns_io_service(false),
      m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any),
      m_use_cache(true) {
    detail::NameResolver::ensure_initialized();
}

DNSResolver::DNSResolver(IOService* io_service) : m_io_service(io_service), m_owns_io_service(false),
      m_cache_ttl(300), m_timeout(5), m_preferred_family(AddressFamily::Any),
      m_use_cache(true) {
    detail::NameResolver::ensure_initialized();
}

DNSResolver::~DNSResolver() {
    if (m_owns_io_service && m_io_service) {
        m_io_service->stop();
        delete m_io_service;
        m_io_service = nullptr;
    }
}

DNSResolver::DNSResolver(DNSResolver&& other) noexcept : m_io_service(other.m_io_service), m_owns_io_service(other.m_owns_io_service),
      m_cache(std::move(other.m_cache)), m_cache_ttl(other.m_cache_ttl),
      m_cache_timestamps(std::move(other.m_cache_timestamps)),
      m_timeout(other.m_timeout), m_preferred_family(other.m_preferred_family),
      m_use_cache(other.m_use_cache), m_dns_servers(std::move(other.m_dns_servers)) {
    other.m_io_service = nullptr;
    other.m_owns_io_service = false;
}

auto DNSResolver::resolve(const std::string& hostname, AddressFamily family) -> DNSResult {
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

void DNSResolver::resolve_async(const std::string& hostname, std::function<void(const DNSResult&)> callback, AddressFamily family) {
    IOService* service = ensure_io_service();
    if (!service) return;

    service->post([this, hostname, callback, family]() {
        DNSResult result = resolve(hostname, family);
        if (callback) { callback(result); }
    });
}

auto DNSResolver::resolve_async(const std::string& hostname, AddressFamily family) -> std::future<DNSResult> {
    auto promise = std::make_shared<std::promise<DNSResult>>();
    auto future = promise->get_future();
    resolve_async(hostname, [promise](const DNSResult& result) { promise->set_value(result); }, family);
    return future;
}

auto DNSResolver::reverse_lookup(const std::string& ip_address) -> DNSResult {
    if (ip_address.empty()) { return DNSResult("", SocketError(ErrorCode::InvalidArgument, "Empty IP address")); }
    auto start_time = std::chrono::steady_clock::now();
    DNSResult result = perform_reverse_lookup(ip_address);
    auto end_time = std::chrono::steady_clock::now();
    result.set_duration(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time));
    return result;
}

void DNSResolver::reverse_lookup_async(const std::string& ip_address, std::function<void(const DNSResult&)> callback) {
    IOService* service = ensure_io_service();
    if (!service) return;

    service->post([this, ip_address, callback]() {
        DNSResult result = reverse_lookup(ip_address);
        if (callback) { callback(result); }
    });
}

void DNSResolver::clear_cache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_cache.clear();
    m_cache_timestamps.clear();
}

auto DNSResolver::ensure_io_service() -> IOService* {
    std::lock_guard<std::mutex> lock(m_resolver_mutex);

    if (!m_io_service) {
        m_io_service = new (std::nothrow) IOService();
        if (!m_io_service) return nullptr;
        m_owns_io_service = true;
        m_io_service->start(1);
    }

    return m_io_service;
}

auto DNSResolver::perform_resolution(const std::string& hostname, AddressFamily family) -> DNSResult {
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

auto DNSResolver::perform_reverse_lookup(const std::string& ip_address) -> DNSResult {
    auto outcome = detail::NameResolver::reverse(ip_address);

    if (!outcome.ok) {
        if (outcome.native_code == 0) { return DNSResult("", SocketError(ErrorCode::InvalidArgument, "Invalid IP address format")); }
        return DNSResult("", detail::NameResolver::to_error(outcome, "getnameinfo"));
    }

    std::vector<DNSResult::AddressInfo> addresses;
    for (const auto& entry : outcome.addresses) { addresses.emplace_back(entry.address, entry.family, entry.port); }
    return DNSResult(outcome.canonical_name, addresses);
}

auto DNSResolver::get_from_cache(const std::string& hostname) const -> DNSResult {
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

void DNSResolver::add_to_cache(const std::string& hostname, const DNSResult& result) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_cache[hostname] = result;
    m_cache_timestamps[hostname] = std::chrono::steady_clock::now();
}

} // namespace core
} // namespace networking
} // namespace fizmo
