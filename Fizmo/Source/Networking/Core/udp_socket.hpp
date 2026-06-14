#ifndef FIZMO_UDP_SOCKET_HPP
#define FIZMO_UDP_SOCKET_HPP

#include "socket_base.hpp"
#include "socket_options.hpp"
#include <string>
#include <utility>

namespace fizmo {
namespace networking {
namespace core {

class UDPSocket : public SocketBase {
public:
    explicit UDPSocket(AddressFamily family = AddressFamily::IPv4) noexcept : SocketBase(SocketType::UDP, family) {}
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&& other) noexcept : SocketBase(std::move(other)) {}

    UDPSocket& operator=(UDPSocket&& other) noexcept {
        SocketBase::operator=(std::move(other));
        return *this;
    }

    ~UDPSocket() override = default;

public:
    bool connect(const NetworkAddress& address) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return false;
        SocketState::State prev = m_state.state();
        m_state = SocketState::State::Connecting;
        const bool result = m_impl->connect(address);

        if (result) {
            m_state = SocketState::State::Connected;
        } else {
            m_state = prev;

        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("connect"));
        #endif
        }

        return result;
    }

    bool disconnect() noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (m_state.state() != SocketState::State::Connected) return false;
        struct sockaddr_storage storage;
        std::memset(&storage, 0, sizeof(storage));
        storage.ss_family = AF_UNSPEC;

        int result = ::connect(
            static_cast<detail::WinsockImpl*>(m_impl.get())->get_raw_socket(),
            reinterpret_cast<struct sockaddr*>(&storage),
            static_cast<int>(sizeof(storage))
        );

        if (result == 0 || WSAGetLastError() == WSAEAFNOSUPPORT) {
            m_state = SocketState::State::Bound;   
            return true;
        }

        add_error(WinsockErrorConverter::get_last_error("disconnect"));
        return false;
    #else
        return false;
    #endif
    }

    bool is_connected() const noexcept { return m_state.state() == SocketState::State::Connected; }

public:
    int send_connected(const void* data, const std::size_t length) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_send()) return -1;
        const int result = m_impl->send(data, length);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("send"));
        #endif
        }

        return result;
    }

    int receive_connected(void* buffer, const std::size_t length) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;
        const int result = m_impl->receive(buffer, length);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("receive"));
        #endif
        }

        return result;
    }

public:
    int send_string(const std::string& msg, const NetworkAddress& dest) noexcept { return send(msg.data(), msg.size(), dest); }

    std::pair<std::string, NetworkAddress> receive_string(std::size_t max_length = 65507) noexcept {
        std::string buf(max_length, '\0');
        NetworkAddress source;
        int n = receive(&buf[0], max_length, source);
        if (n <= 0) return {{}, {}};
        buf.resize(static_cast<std::size_t>(n));
        return {std::move(buf), source};
    }

    int send_string_connected(const std::string& msg) noexcept { return send_connected(msg.data(), msg.size()); }

    std::string receive_string_connected(std::size_t max_length = 65507) noexcept {
        std::string buf(max_length, '\0');
        int n = receive_connected(&buf[0], max_length);
        if (n <= 0) return {};
        buf.resize(static_cast<std::size_t>(n));
        return buf;
    }

public:
    int send(const void* data, const std::size_t length, const NetworkAddress& dest) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        const int result = m_impl->sendto(data, length, dest);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("sendto"));
        #endif
        }

        return result;
    }

    int receive(void* buffer, const std::size_t length, NetworkAddress& source) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        const int result = m_impl->recvfrom(buffer, length, source);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("recvfrom"));
        #endif
        }

        return result;
    }

public:
    int broadcast(const void* data, std::size_t length, std::uint16_t port) noexcept {
    #ifdef OS_WINDOWS
        options(*this).set(BooleanOption::Broadcast, true);
        NetworkAddress dest("255.255.255.255", port);
        return send(data, length, dest);
    #else
        return -1;
    #endif
    }

    int broadcast_string(const std::string& msg, std::uint16_t port) noexcept { return broadcast(msg.data(), msg.size(), port); }

public:
    int peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        struct sockaddr_storage storage;
        int addr_len = sizeof(storage);
        std::memset(&storage, 0, sizeof(storage));

        int result = ::recvfrom(
            static_cast<detail::WinsockImpl*>(m_impl.get())->get_raw_socket(),
            static_cast<char*>(buffer),
            static_cast<int>(length),
            MSG_PEEK,
            reinterpret_cast<struct sockaddr*>(&storage),
            &addr_len
        );

        if (result >= 0) {
            source = detail::WinsockImpl::parse_sockaddr(storage);
        } else {
            add_error(WinsockErrorConverter::get_last_error("peek"));
        }

        return result;
    #else
        return -1;
    #endif
    }

    int peek_connected(void* buffer, std::size_t length) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_receive()) return -1;

        int result = ::recv(
            static_cast<detail::WinsockImpl*>(m_impl.get())->get_raw_socket(),
            static_cast<char*>(buffer),
            static_cast<int>(length),
            MSG_PEEK
        );

        if (result < 0) {
            add_error(WinsockErrorConverter::get_last_error("peek_connected"));
        }

        return result;
    #else
        return -1;
    #endif
    }

public:
    bool set_max_hops(int hops) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(hops));
        } else {
            result = m_impl->set_option(IPPROTO_IP, IP_TTL, &hops, sizeof(hops));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_max_hops"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    int get_max_hops() const noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return -1;
        int hops = -1;
        int len  = sizeof(hops);

        if (m_family == AddressFamily::IPv6) {
            m_impl->get_option(IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, &len);
        } else {
            m_impl->get_option(IPPROTO_IP, IP_TTL, &hops, &len);
        }

        return hops;
    #else
        return -1;
    #endif
    }

public:
    bool join_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface = "0.0.0.0"
    ) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            struct group_source_req gsr;
            std::memset(&gsr, 0, sizeof(gsr));
            gsr.gsr_interface = resolve_interface_index(local_interface);
            auto* grp = reinterpret_cast<struct sockaddr_in6*>(&gsr.gsr_group);
            grp->sin6_family = AF_INET6;
            inet_pton(AF_INET6, group_address.c_str(), &grp->sin6_addr);
            auto* src = reinterpret_cast<struct sockaddr_in6*>(&gsr.gsr_source);
            src->sin6_family = AF_INET6;
            inet_pton(AF_INET6, source_address.c_str(), &src->sin6_addr);
            result = m_impl->set_option(IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP, &gsr, sizeof(gsr));
        } else {
            struct ip_mreq_source mreq;
            std::memset(&mreq, 0, sizeof(mreq));
            inet_pton(AF_INET, group_address.c_str(),   &mreq.imr_multiaddr);
            inet_pton(AF_INET, source_address.c_str(),  &mreq.imr_sourceaddr);
            inet_pton(AF_INET, local_interface.c_str(), &mreq.imr_interface);
            result = m_impl->set_option(IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, &mreq, sizeof(mreq));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("join_source_group"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    bool leave_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface = "0.0.0.0"
    ) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            struct group_source_req gsr;
            std::memset(&gsr, 0, sizeof(gsr));
            gsr.gsr_interface = resolve_interface_index(local_interface);
            auto* grp = reinterpret_cast<struct sockaddr_in6*>(&gsr.gsr_group);
            grp->sin6_family = AF_INET6;
            inet_pton(AF_INET6, group_address.c_str(), &grp->sin6_addr);
            auto* src = reinterpret_cast<struct sockaddr_in6*>(&gsr.gsr_source);
            src->sin6_family = AF_INET6;
            inet_pton(AF_INET6, source_address.c_str(), &src->sin6_addr);
            result = m_impl->set_option(IPPROTO_IPV6, MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof(gsr));
        } else {
            struct ip_mreq_source mreq;
            std::memset(&mreq, 0, sizeof(mreq));
            inet_pton(AF_INET, group_address.c_str(),   &mreq.imr_multiaddr);
            inet_pton(AF_INET, source_address.c_str(),  &mreq.imr_sourceaddr);
            inet_pton(AF_INET, local_interface.c_str(), &mreq.imr_interface);
            result = m_impl->set_option(IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof(mreq));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("leave_source_group"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

public:
    int scatter_receive(detail::IOBuffer* buffers, std::size_t count, NetworkAddress& source) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_recvfrom()) return -1;
        int result = m_impl->scatter_recvfrom(buffers, count, source);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("scatter_recvfrom"));
        #endif
        }

        return result;
    }

    int gather_send(const detail::IOBuffer* buffers, std::size_t count, const NetworkAddress& dest) noexcept {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.can_sendto()) return -1;
        int result = m_impl->gather_sendto(buffers, count, dest);

        if (result < 0) {
        #ifdef OS_WINDOWS
            add_error(WinsockErrorConverter::get_last_error("gather_sendto"));
        #endif
        }

        return result;
    }

public:
    bool join_multicast_group(const std::string& group_address, const std::string& local_interface = "0.0.0.0") noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            struct ipv6_mreq mreq;
            inet_pton(AF_INET6, group_address.c_str(), &mreq.ipv6mr_multiaddr);
            mreq.ipv6mr_interface = 0;
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq));
        } else {
            struct ip_mreq mreq;
            inet_pton(AF_INET, group_address.c_str(), &mreq.imr_multiaddr);
            inet_pton(AF_INET, local_interface.c_str(), &mreq.imr_interface);
            result = m_impl->set_option(IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("join_multicast_group"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    bool leave_multicast_group(const std::string& group_address, const std::string& local_interface = "0.0.0.0") noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized() || m_state.state() == SocketState::State::Closed) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            struct ipv6_mreq mreq;
            inet_pton(AF_INET6, group_address.c_str(), &mreq.ipv6mr_multiaddr);
            mreq.ipv6mr_interface = 0;
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq, sizeof(mreq));
        } else {
            struct ip_mreq mreq;
            inet_pton(AF_INET, group_address.c_str(), &mreq.imr_multiaddr);
            inet_pton(AF_INET, local_interface.c_str(), &mreq.imr_interface);
            result = m_impl->set_option(IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("leave_multicast_group"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    bool set_multicast_ttl(int ttl) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl));
        } else {
            result = m_impl->set_option(IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_multicast_ttl"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    bool set_multicast_loopback(bool enable) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int val = enable ? 1 : 0;
        int result;

        if (m_family == AddressFamily::IPv6) {
            result = m_impl->set_option(IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val));
        } else {
            result = m_impl->set_option(IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_multicast_loopback"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

    bool set_multicast_interface(const std::string& local_interface) noexcept {
    #ifdef OS_WINDOWS
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if (!m_state.is_initialized()) return false;
        int result;

        if (m_family == AddressFamily::IPv6) {
            unsigned int idx = resolve_interface_index(local_interface);

            if (idx == 0 && local_interface != "0.0.0.0" && local_interface != "::") {
                add_error(
                    SocketError(ErrorCode::InvalidArgument,
                    "Could not resolve interface \"" + local_interface + "\" to an adapter index",
                    0,
                    "set_multicast_interface"
                ));

                return false;
            }

            result = m_impl->set_option(IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx));
        } else {
            struct in_addr addr;
            inet_pton(AF_INET, local_interface.c_str(), &addr);
            result = m_impl->set_option(IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr));
        }

        if (result != 0) {
            add_error(WinsockErrorConverter::get_last_error("set_multicast_interface"));
            return false;
        }

        return true;
    #else
        return false;
    #endif
    }

public:
    int send_buffer_connected(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer_connected(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int send_buffer(Buffer& buffer, const NetworkAddress& dest, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer(Buffer& buffer, NetworkAddress& source, const std::size_t max_length = 0) noexcept;

private:
    static unsigned int resolve_interface_index(const std::string& local_interface) noexcept {
        if (local_interface.empty() || local_interface == "0.0.0.0" || local_interface == "::") { return 0; }

    #ifdef OS_WINDOWS
        unsigned int idx = if_nametoindex(local_interface.c_str());
        if (idx != 0) return idx;
        ULONG buf_size = 15000;
        std::unique_ptr<std::uint8_t[]> buf;
        ULONG retries = 0;
        DWORD ret;

        do {
            buf = std::make_unique<std::uint8_t[]>(buf_size);
            ret = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get()), &buf_size);
        } while (ret == ERROR_BUFFER_OVERFLOW && ++retries < 3);

        if (ret != NO_ERROR) return 0;

        for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get()); adapter; adapter = adapter->Next) {
            for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                char addr_str[INET6_ADDRSTRLEN];
                auto* sa = unicast->Address.lpSockaddr;

                if (sa->sa_family == AF_INET) {
                    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr, addr_str, sizeof(addr_str));
                } else if (sa->sa_family == AF_INET6) {
                    inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr, addr_str, sizeof(addr_str));
                } else {
                    continue;
                }

                if (local_interface == addr_str) { return adapter->IfIndex; }
            }
        }
    #endif

        return 0;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_UDP_SOCKET_HPP