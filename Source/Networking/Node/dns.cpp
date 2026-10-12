#include "fizmo_library.hpp"
#include "dns.hpp"

#include <unordered_set>

namespace fizmo {
namespace networking {
namespace dns {

namespace {

async::ResolveFamily to_resolve_family(net::Family f) {
    if (f == net::Family::IPv4) return async::ResolveFamily::V4;
    if (f == net::Family::IPv6) return async::ResolveFamily::V6;
    return async::ResolveFamily::Any;
}

std::vector<LookupAddress> collect(const async::Resolver::Results& results) {
    std::vector<LookupAddress> out;
    std::unordered_set<std::string> seen;

    for (auto& r : results) {
        const async::Endpoint ep = r.endpoint;
        std::string a = ep.address();
        if (!seen.insert(a).second) continue;
        out.push_back(LookupAddress{ a, ep.is_v6() ? net::Family::IPv6 : net::Family::IPv4 });
    }

    return out;
}

void resolve_family(const std::string& hostname, net::Family family, std::function<void(const std::error_code&, const std::vector<LookupAddress>&)> cb) {
    const int numeric = net::is_ip(hostname);

    if (numeric != 0) {
        std::vector<LookupAddress> one;
        const net::Family f = numeric == 6 ? net::Family::IPv6 : net::Family::IPv4;
        if (family == net::Family::Any || family == f) one.push_back(LookupAddress{ hostname, f });
        const std::error_code ec = one.empty() ? async::make_error_code(async::error::host_not_found) : std::error_code();
        loop().post([cb, ec, one]() { cb(ec, one); });
        return;
    }

    networking::detail::default_resolver().async_resolve(hostname, std::uint16_t(0), [cb](std::error_code ec, async::Resolver::Results results) {
        auto list = ec ? std::vector<LookupAddress>() : collect(results);
        if (!ec && list.empty()) ec = async::make_error_code(async::error::host_not_found);
        cb(ec, list);
    }, to_resolve_family(family));
}

} // namespace

void lookup(const std::string& hostname, std::function<void(const std::error_code&, const std::string&, net::Family)> callback) {
    lookup(hostname, LookupOptions(), std::move(callback));
}

void lookup(const std::string& hostname, const LookupOptions& options, std::function<void(const std::error_code&, const std::string&, net::Family)> callback) {
    if (!callback) return;
    const std::string host = hostname.empty() ? std::string("localhost") : hostname;

    resolve_family(host, options.family, [callback, options](const std::error_code& ec, const std::vector<LookupAddress>& list) {
        if (ec || list.empty()) { callback(ec, std::string(), net::Family::Any); return; }
        const LookupAddress* pick = &list.front();

        if (!options.verbatim) {
            for (auto& a : list) {
                if (a.family == net::Family::IPv4) { pick = &a; break; }
            }
        }

        callback(ec, pick->address, pick->family);
    });
}

void lookup_all(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<LookupAddress>&)> callback, net::Family family) {
    if (!callback) return;
    resolve_family(hostname.empty() ? std::string("localhost") : hostname, family, std::move(callback));
}

namespace {

void resolve_strings(const std::string& hostname, net::Family family, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback) {
    if (!callback) return;

    resolve_family(hostname, family, [callback](const std::error_code& ec, const std::vector<LookupAddress>& list) {
        std::vector<std::string> out;
        for (auto& a : list) out.push_back(a.address);
        callback(ec, out);
    });
}

} // namespace

void resolve4(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback) {
    resolve_strings(hostname, net::Family::IPv4, std::move(callback));
}

void resolve6(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback) {
    resolve_strings(hostname, net::Family::IPv6, std::move(callback));
}

void reverse(const std::string& ip, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback) {
    if (!callback) return;
    async::Endpoint ep;

    if (!async::Endpoint::parse(ip, 0, ep)) {
        loop().post([callback]() { callback(async::make_error_code(async::error::invalid_argument), {}); });
        return;
    }

    networking::detail::default_resolver().async_reverse(ep, [callback](std::error_code ec, std::string host, std::string) {
        std::vector<std::string> out;
        if (!ec && !host.empty()) out.push_back(host);
        callback(ec, out);
    });
}

void lookup_service(const std::string& address, std::uint16_t port, std::function<void(const std::error_code&, const std::string&, const std::string&)> callback) {
    if (!callback) return;
    async::Endpoint ep;

    if (!async::Endpoint::parse(address, port, ep)) {
        loop().post([callback]() { callback(async::make_error_code(async::error::invalid_argument), {}, {}); });
        return;
    }

    networking::detail::default_resolver().async_reverse(ep, [callback](std::error_code ec, std::string host, std::string service) {
        callback(ec, host, service);
    });
}

std::vector<LookupAddress> lookup_sync(const std::string& hostname, std::error_code& ec, net::Family family) {
    ec.clear();
    const int numeric = net::is_ip(hostname);
    if (numeric != 0) return { LookupAddress{ hostname, numeric == 6 ? net::Family::IPv6 : net::Family::IPv4 } };
    auto results = async::Resolver::resolve(hostname, "0", ec, to_resolve_family(family));
    if (ec) return {};
    auto list = collect(results);
    if (list.empty()) ec = async::make_error_code(async::error::host_not_found);
    return list;
}

} // namespace dns
} // namespace networking
} // namespace fizmo
