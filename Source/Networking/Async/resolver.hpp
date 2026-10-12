#ifndef FIZMO_ASYNC_RESOLVER_HPP
#define FIZMO_ASYNC_RESOLVER_HPP

#include "endpoint.hpp"
#include "io_context.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

enum class ResolveFamily {
    Any,
    V4,
    V6
};

struct ResolveFlags {
    bool passive          = false;
    bool numeric_host     = false;
    bool numeric_service  = false;
    bool address_configured = true;
    bool v4_mapped        = false;
};

struct ResolveResult {
    Endpoint    endpoint;
    std::string host_name;
    std::string service_name;
};

class Resolver {
public:
    using Results        = std::vector<ResolveResult>;
    using ResolveHandler = UniqueFunction<void(std::error_code, Results)>;
    using ReverseHandler = UniqueFunction<void(std::error_code, std::string, std::string)>;

    explicit Resolver(IOContext& ctx);
    ~Resolver();

    Resolver(const Resolver&) = delete;
    Resolver& operator=(const Resolver&) = delete;

    IOContext& context() const noexcept { return m_ctx; }

    void async_resolve(const std::string& host, const std::string& service, ResolveHandler handler,
                       ResolveFamily family = ResolveFamily::Any, ResolveFlags flags = ResolveFlags());
    void async_resolve(const std::string& host, std::uint16_t port, ResolveHandler handler,
                       ResolveFamily family = ResolveFamily::Any, ResolveFlags flags = ResolveFlags());
    void async_reverse(const Endpoint& endpoint, ReverseHandler handler);

    static Results resolve(const std::string& host, const std::string& service, std::error_code& ec,
                           ResolveFamily family = ResolveFamily::Any, ResolveFlags flags = ResolveFlags());
    static Results resolve(const std::string& host, const std::string& service);
    static bool reverse(const Endpoint& endpoint, std::string& host, std::string& service, std::error_code& ec);

    static std::vector<Endpoint> endpoints(const Results& results);

    void cancel();

private:
    struct State {
        std::mutex                         mutex;
        std::condition_variable            cv;
        std::deque<UniqueFunction<void()>> jobs;
        std::atomic<std::uint64_t>         generation{0};
        bool                               stopping = false;
    };

    IOContext&             m_ctx;
    std::shared_ptr<State> m_state;
    std::thread            m_worker;
    std::once_flag         m_started;

    void submit(UniqueFunction<void()> job);
    static void worker_loop(std::shared_ptr<State> state);
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_RESOLVER_HPP
