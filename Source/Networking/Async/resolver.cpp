#include "fizmo_library.hpp"
#include "resolver.hpp"
#include "socket_ops.hpp"

namespace fizmo {
namespace networking {
namespace async {

Resolver::Resolver(IOContext& ctx) : m_ctx(ctx), m_state(std::make_shared<State>()) {}

Resolver::~Resolver() {
    cancel();

    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->stopping = true;
    }

    m_state->cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void Resolver::submit(UniqueFunction<void()> job) {
    std::call_once(m_started, [this] { m_worker = std::thread(&Resolver::worker_loop, m_state); });

    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        m_state->jobs.push_back(std::move(job));
    }

    m_state->cv.notify_one();
}

void Resolver::worker_loop(std::shared_ptr<State> state) {
    for (;;) {
        UniqueFunction<void()> job;

        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->cv.wait(lock, [&] { return state->stopping || !state->jobs.empty(); });
            if (state->jobs.empty()) return;
            job = std::move(state->jobs.front());
            state->jobs.pop_front();
        }

        job();
    }
}

void Resolver::cancel() {
    ++m_state->generation;
}

Resolver::Results Resolver::resolve(const std::string& host, const std::string& service, std::error_code& ec, ResolveFamily family, ResolveFlags flags) {
    detail::ensure_socket_runtime();
    ec.clear();
    Results results;
    addrinfo hints{};
    hints.ai_family = family == ResolveFamily::V4 ? AF_INET : (family == ResolveFamily::V6 ? AF_INET6 : AF_UNSPEC);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    if (flags.passive)            hints.ai_flags |= AI_PASSIVE;
    if (flags.numeric_host)       hints.ai_flags |= AI_NUMERICHOST;
    if (flags.address_configured) hints.ai_flags |= AI_ADDRCONFIG;
#if defined(AI_NUMERICSERV)
    if (flags.numeric_service)    hints.ai_flags |= AI_NUMERICSERV;
#endif
#if defined(AI_V4MAPPED)
    if (flags.v4_mapped && family == ResolveFamily::V6) hints.ai_flags |= AI_V4MAPPED;
#endif
    addrinfo* list = nullptr;
    const char* h = host.empty() ? nullptr : host.c_str();
    const char* s = service.empty() ? nullptr : service.c_str();
    int rc = ::getaddrinfo(h, s, &hints, &list);

    if (rc != 0 && (hints.ai_flags & AI_ADDRCONFIG)) {
        hints.ai_flags &= ~AI_ADDRCONFIG;
        rc = ::getaddrinfo(h, s, &hints, &list);
    }

    if (rc != 0) {
        ec = resolver_error_code(rc);
        return results;
    }

    for (addrinfo* p = list; p; p = p->ai_next) {
        if (p->ai_family != AF_INET && p->ai_family != AF_INET6) continue;
        ResolveResult r;
        r.endpoint = Endpoint(p->ai_addr, static_cast<std::size_t>(p->ai_addrlen));
        r.host_name = host;
        r.service_name = service;
        bool duplicate = false;

        for (const auto& existing : results) {
            if (existing.endpoint == r.endpoint) { duplicate = true; break; }
        }

        if (!duplicate) results.push_back(std::move(r));
    }

    ::freeaddrinfo(list);
    if (results.empty()) ec = error::host_not_found;
    return results;
}

Resolver::Results Resolver::resolve(const std::string& host, const std::string& service) {
    std::error_code ec;
    Results r = resolve(host, service, ec);
    if (ec) throw std::system_error(ec, "Resolver::resolve(" + host + ")");
    return r;
}

bool Resolver::reverse(const Endpoint& endpoint, std::string& host, std::string& service, std::error_code& ec) {
    detail::ensure_socket_runtime();
    ec.clear();
    char hbuf[NI_MAXHOST] = {};
    char sbuf[NI_MAXSERV] = {};
    const int rc = ::getnameinfo(endpoint.data(), endpoint.size(), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), 0);
    if (rc != 0) { ec = resolver_error_code(rc); return false; }
    host = hbuf;
    service = sbuf;
    return true;
}

std::vector<Endpoint> Resolver::endpoints(const Results& results) {
    std::vector<Endpoint> out;
    out.reserve(results.size());
    for (const auto& r : results) out.push_back(r.endpoint);
    return out;
}

void Resolver::async_resolve(const std::string& host, const std::string& service, ResolveHandler handler, ResolveFamily family, ResolveFlags flags) {
    IOContext* ctx = &m_ctx;
    ctx->work_started();
    auto state = m_state;
    const std::uint64_t gen = state->generation.load();

    submit([ctx, state, gen, host, service, family, flags, h = std::move(handler)]() mutable {
        std::error_code ec;
        Results results;

        if (state->generation.load() != gen) ec = error::operation_aborted;
        else results = resolve(host, service, ec, family, flags);

        if (!ec && state->generation.load() != gen) {
            ec = error::operation_aborted;
            results.clear();
        }

        ctx->post_completed([h = std::move(h), ec, results = std::move(results)]() mutable { h(ec, std::move(results)); });
    });
}

void Resolver::async_resolve(const std::string& host, std::uint16_t port, ResolveHandler handler, ResolveFamily family, ResolveFlags flags) {
    flags.numeric_service = true;
    async_resolve(host, std::to_string(port), std::move(handler), family, flags);
}

void Resolver::async_reverse(const Endpoint& endpoint, ReverseHandler handler) {
    IOContext* ctx = &m_ctx;
    ctx->work_started();
    auto state = m_state;
    const std::uint64_t gen = state->generation.load();

    submit([ctx, state, gen, endpoint, h = std::move(handler)]() mutable {
        std::error_code ec;
        std::string host, service;
        if (state->generation.load() != gen) ec = error::operation_aborted;
        else reverse(endpoint, host, service, ec);
        if (!ec && state->generation.load() != gen) ec = error::operation_aborted;
        ctx->post_completed([h = std::move(h), ec, host, service]() mutable { h(ec, host, service); });
    });
}

} // namespace async
} // namespace networking
} // namespace fizmo
