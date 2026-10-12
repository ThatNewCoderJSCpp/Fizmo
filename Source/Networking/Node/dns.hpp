#ifndef FIZMO_NODE_DNS_HPP
#define FIZMO_NODE_DNS_HPP

#include "loop.hpp"
#include "net.hpp"

#include <functional>
#include <string>
#include <system_error>
#include <vector>

namespace fizmo {
namespace networking {
namespace dns {

struct LookupAddress {
    std::string address;
    net::Family family = net::Family::IPv4;
};

struct LookupOptions {
    net::Family family = net::Family::Any;
    bool        all = false;
    bool        verbatim = true;
};

void lookup(const std::string& hostname, std::function<void(const std::error_code&, const std::string& address, net::Family family)> callback);
void lookup(const std::string& hostname, const LookupOptions& options, std::function<void(const std::error_code&, const std::string& address, net::Family family)> callback);
void lookup_all(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<LookupAddress>&)> callback, net::Family family = net::Family::Any);
void resolve4(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback);
void resolve6(const std::string& hostname, std::function<void(const std::error_code&, const std::vector<std::string>&)> callback);
void reverse(const std::string& ip, std::function<void(const std::error_code&, const std::vector<std::string>& hostnames)> callback);
void lookup_service(const std::string& address, std::uint16_t port, std::function<void(const std::error_code&, const std::string& hostname, const std::string& service)> callback);

std::vector<LookupAddress> lookup_sync(const std::string& hostname, std::error_code& ec, net::Family family = net::Family::Any);

} // namespace dns
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_DNS_HPP
