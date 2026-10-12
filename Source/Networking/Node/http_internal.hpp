#ifndef FIZMO_NODE_HTTP_INTERNAL_HPP
#define FIZMO_NODE_HTTP_INTERNAL_HPP

#include "http.hpp"

#include <deque>
#include <mutex>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace http {

struct IncomingMessage::State : std::enable_shared_from_this<IncomingMessage::State> {
    mutable std::mutex m;
    Events             ev;
    std::string        method;
    std::string        url;
    std::string        version = "1.1";
    int                status = 0;
    std::string        reason;
    Headers            headers;
    Headers            trailers;
    bool               keep_alive = true;
    bool               complete = false;
    bool               aborted = false;
    bool               end_emitted = false;
    bool               end_pending = false;
    bool               close_emitted = false;
    bool               paused_for_backlog = false;
    bool               flush_scheduled = false;
    std::vector<Bytes> backlog;
    std::size_t        backlog_bytes = 0;
    std::size_t        backlog_limit = 1u << 20;
    net::Socket        socket;
    std::any           user;

    void push_body(const std::uint8_t* data, std::size_t n);
    void mark_complete();
    void mark_aborted(const std::error_code& ec);
    void schedule_flush();
    void flush();
    void emit_close();
    void clear_events();
};

std::string http_date();

struct Agent::State : std::enable_shared_from_this<Agent::State> {
    struct Spec {
        std::string                           key;
        net::ConnectOptions                   connect;
        std::shared_ptr<security::TLSContext> tls;
        std::string                           servername;
    };

    using Ready = std::function<void(net::Socket, bool reused)>;

    struct Idle {
        net::Socket                           socket;
        ListenerId                            close_id = 0;
        ListenerId                            data_id = 0;
        ListenerId                            error_id = 0;
        ListenerId                            timeout_id = 0;
        std::chrono::steady_clock::time_point expires;
    };

    struct Waiter {
        std::uint64_t id = 0;
        Spec          spec;
        Ready         ready;
    };

    struct Pool {
        std::vector<Idle>   free;
        std::size_t         active = 0;
        std::deque<Waiter>  waiting;
    };

    mutable std::mutex                                                     m;
    AgentOptions                                                           opt;
    std::unordered_map<std::string, Pool>                                  pools;
    std::unordered_map<std::string, std::shared_ptr<security::TLSContext>> tls_cache;
    std::uint64_t                                                          next_waiter = 0;

    std::shared_ptr<security::TLSContext> tls_context(const RequestOptions& o, std::string& key, std::string& error);
    std::uint64_t acquire(const Spec& spec, Ready ready, bool force_new);
    void release(const std::string& key, net::Socket socket, std::chrono::milliseconds idle_hint);
    void forget(const std::string& key);
    void cancel(const std::string& key, std::uint64_t waiter);
    void remove_idle(const std::string& key, std::uint64_t socket_id);
    void serve(const std::string& key);
    static void detach(Idle& idle);
};

} // namespace http
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_HTTP_INTERNAL_HPP
