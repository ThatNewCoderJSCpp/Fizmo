#ifndef FIZMO_NODE_HTTP_HPP
#define FIZMO_NODE_HTTP_HPP

#include "net.hpp"
#include "tls.hpp"
#include "url.hpp"

#include <any>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {
namespace http {

const std::vector<std::string>& methods();
const char* status_text(int code) noexcept;

class Headers {
public:
    using Entry = std::pair<std::string, std::string>;

    Headers() = default;
    Headers(std::initializer_list<Entry> entries);

    Headers& set(const std::string& name, const std::string& value);
    Headers& set(const std::string& name, const std::vector<std::string>& values);
    Headers& append(const std::string& name, const std::string& value);
    std::string get(const std::string& name, const std::string& fallback = {}) const;
    std::vector<std::string> get_all(const std::string& name) const;
    bool has(const std::string& name) const;
    bool has_token(const std::string& name, const std::string& token) const;
    bool remove(const std::string& name);
    void clear() { m_entries.clear(); }

    std::size_t size() const noexcept { return m_entries.size(); }
    bool empty() const noexcept { return m_entries.empty(); }
    const std::vector<Entry>& entries() const noexcept { return m_entries; }
    std::vector<Entry>::const_iterator begin() const noexcept { return m_entries.begin(); }
    std::vector<Entry>::const_iterator end() const noexcept { return m_entries.end(); }
    std::vector<std::string> names() const;

    std::string serialize() const;

    static bool valid_name(const std::string& name) noexcept;
    static bool valid_value(const std::string& value) noexcept;
    static bool iequals(const std::string& a, const std::string& b) noexcept;

private:
    std::vector<Entry> m_entries;
};

namespace detail {

struct ParserLimits {
    std::size_t max_head_size = 16384;
    std::size_t max_headers = 2000;
    std::size_t max_line = 8192;
};

class Parser {
public:
    enum class Mode { Request, Response };
    enum class State { Idle, Head, Body, Complete, Upgrade, Error };
    using Limits = ParserLimits;

    explicit Parser(Mode mode, Limits limits = Limits());

    std::size_t feed(const std::uint8_t* data, std::size_t len);
    bool finish_eof();
    void reset();

    State state() const noexcept { return m_state; }
    bool error() const noexcept { return m_state == State::Error; }
    int error_status() const noexcept { return m_error_status; }
    const std::string& error_message() const noexcept { return m_error_message; }
    bool started() const noexcept { return m_state != State::Idle; }

    const std::string& method() const noexcept { return m_method; }
    const std::string& target() const noexcept { return m_target; }
    int version_major() const noexcept { return m_major; }
    int version_minor() const noexcept { return m_minor; }
    std::string version() const { return std::to_string(m_major) + "." + std::to_string(m_minor); }
    int status() const noexcept { return m_status; }
    const std::string& reason() const noexcept { return m_reason; }
    const Headers& headers() const noexcept { return m_headers; }
    const Headers& trailers() const noexcept { return m_trailers; }
    bool keep_alive() const noexcept { return m_keep_alive; }
    bool upgrade() const noexcept { return m_upgrade; }
    bool chunked() const noexcept { return m_chunked; }
    bool has_content_length() const noexcept { return m_has_length; }
    std::uint64_t content_length() const noexcept { return m_length; }
    bool reads_until_eof() const noexcept { return m_until_eof; }

    void set_skip_body(bool skip) noexcept { m_skip_body = skip; }
    void set_upgrade_enabled(bool enabled) noexcept { m_upgrade_enabled = enabled; }

    std::function<void()>                                on_headers;
    std::function<void(const std::uint8_t*, std::size_t)> on_body;
    std::function<void()>                                on_complete;

private:
    enum class BodyMode { None, Length, Chunked, UntilEof };
    enum class ChunkState { Size, Data, DataEnd, Trailers };

    Mode          m_mode;
    Limits        m_limits;
    State         m_state = State::Idle;
    int           m_error_status = 0;
    std::string   m_error_message;
    std::string   m_line;
    std::size_t   m_head_size = 0;
    bool          m_first_line = true;
    std::string   m_method;
    std::string   m_target;
    int           m_major = 1;
    int           m_minor = 1;
    int           m_status = 0;
    std::string   m_reason;
    Headers       m_headers;
    Headers       m_trailers;
    bool          m_keep_alive = true;
    bool          m_upgrade = false;
    bool          m_chunked = false;
    bool          m_has_length = false;
    bool          m_until_eof = false;
    bool          m_skip_body = false;
    bool          m_upgrade_enabled = true;
    std::uint64_t m_length = 0;
    BodyMode      m_body = BodyMode::None;
    ChunkState    m_chunk_state = ChunkState::Size;
    std::uint64_t m_remaining = 0;
    std::size_t   m_trailer_size = 0;

    bool fail(int status, const std::string& message);
    bool take_line(const std::uint8_t* data, std::size_t len, std::size_t& i, std::string& out, bool& complete);
    bool parse_start_line(const std::string& line);
    bool parse_header_line(const std::string& line, Headers& into);
    bool headers_done();
    void complete();
};

} // namespace detail

class IncomingMessage {
public:
    struct State;

    struct Events {
        Event<const DataView&>         data;
        Event<>                        end;
        Event<>                        close;
        Event<>                        aborted;
        Event<const std::error_code&>  error;
    };

    IncomingMessage() noexcept = default;
    explicit IncomingMessage(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    const std::string& method() const;
    const std::string& url() const;
    const std::string& http_version() const;
    int status_code() const;
    const std::string& status_message() const;
    const Headers& headers() const;
    const Headers& trailers() const;
    std::string header(const std::string& name, const std::string& fallback = {}) const;
    std::string path() const;
    URLSearchParams query() const;
    bool complete() const noexcept;
    bool aborted() const noexcept;
    bool keep_alive() const noexcept;
    net::Socket socket() const;

    IncomingMessage& on_data(std::function<void(const DataView&)> fn);
    IncomingMessage& on_end(std::function<void()> fn);
    IncomingMessage& on_close(std::function<void()> fn);
    IncomingMessage& on_aborted(std::function<void()> fn);
    IncomingMessage& on_error(std::function<void(const std::error_code&)> fn);
    Events& events();

    IncomingMessage& pause();
    IncomingMessage& resume();
    IncomingMessage& set_timeout(std::chrono::milliseconds timeout, std::function<void()> fn = {});

    void body(std::function<void(const std::error_code&, std::string)> callback, std::size_t limit = 16u << 20);
    void discard();

    std::any& data();
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

class ServerResponse {
public:
    struct State;

    struct Events {
        Event<> finish;
        Event<> close;
    };

    ServerResponse() noexcept = default;
    explicit ServerResponse(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    int status_code() const;
    ServerResponse& set_status_code(int code);
    ServerResponse& status(int code) { return set_status_code(code); }
    std::string status_message() const;
    ServerResponse& set_status_message(const std::string& message);

    ServerResponse& set_header(const std::string& name, const std::string& value);
    ServerResponse& set_header(const std::string& name, const std::vector<std::string>& values);
    ServerResponse& append_header(const std::string& name, const std::string& value);
    std::string get_header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    ServerResponse& remove_header(const std::string& name);
    Headers get_headers() const;
    std::vector<std::string> get_header_names() const;
    ServerResponse& set_send_date(bool send);
    ServerResponse& add_trailers(const Headers& trailers);

    ServerResponse& write_head(int code, const std::string& message = {}, const Headers& headers = Headers());
    ServerResponse& write_head(int code, const Headers& headers) { return write_head(code, std::string(), headers); }
    bool write(const DataView& chunk, std::function<void(const std::error_code&)> done = {});
    ServerResponse& end(std::function<void()> done = {});
    ServerResponse& end(const DataView& data, std::function<void()> done = {});
    ServerResponse& send(const DataView& body, const std::string& content_type = "text/plain; charset=utf-8");
    ServerResponse& send(int code, const DataView& body, const std::string& content_type = "text/plain; charset=utf-8");
    ServerResponse& json(const std::string& json_text, int code = 200);
    ServerResponse& redirect(const std::string& location, int code = 302);
    void write_continue();

    bool headers_sent() const noexcept;
    bool writable_ended() const noexcept;
    bool writable_finished() const noexcept;
    bool finished() const noexcept { return writable_ended(); }

    ServerResponse& on_finish(std::function<void()> fn);
    ServerResponse& on_close(std::function<void()> fn);
    Events& events();

    net::Socket socket() const;
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

struct ServerOptions : net::ServerOptions {
    std::chrono::milliseconds keep_alive_timeout{5000};
    std::chrono::milliseconds headers_timeout{60000};
    std::chrono::milliseconds request_timeout{300000};
    std::size_t               max_header_size = 16384;
    std::size_t               max_headers_count = 2000;
    std::size_t               max_requests_per_socket = 0;
    std::size_t               max_body_backlog = 1u << 20;
};

using RequestHandler = std::function<void(IncomingMessage, ServerResponse)>;
using UpgradeHandler = std::function<void(IncomingMessage, net::Socket, const DataView&)>;

class Server {
public:
    struct State;

    struct Events {
        Event<IncomingMessage, ServerResponse>                 request;
        Event<IncomingMessage, net::Socket, const DataView&>   upgrade;
        Event<IncomingMessage, ServerResponse>                 check_continue;
        Event<const std::error_code&, net::Socket>             client_error;
        Event<net::Socket>                                     connection;
        Event<>                                                listening;
        Event<>                                                close;
        Event<const std::error_code&>                          error;
    };

    Server() noexcept = default;
    explicit Server(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    Server& listen(std::uint16_t port, std::function<void()> on_listening = {});
    Server& listen(std::uint16_t port, const std::string& host, std::function<void()> on_listening = {});
    Server& listen(const net::ListenOptions& options, std::function<void()> on_listening = {});
    Server& close(std::function<void(const std::error_code&)> on_closed = {});
    Server& close_all_connections();
    Server& close_idle_connections();

    Server& on_request(RequestHandler fn);
    Server& on_upgrade(UpgradeHandler fn);
    Server& on_check_continue(RequestHandler fn);
    Server& on_client_error(std::function<void(const std::error_code&, net::Socket)> fn);
    Server& on_connection(std::function<void(net::Socket)> fn);
    Server& on_listening(std::function<void()> fn);
    Server& on_close(std::function<void()> fn);
    Server& on_error(std::function<void(const std::error_code&)> fn);
    Events& events();

    Server& set_keep_alive_timeout(std::chrono::milliseconds t);
    Server& set_headers_timeout(std::chrono::milliseconds t);
    Server& set_request_timeout(std::chrono::milliseconds t);

    bool listening() const noexcept;
    net::AddressInfo address() const;
    std::uint16_t port() const;
    std::size_t connection_count() const;
    net::Server& net_server();
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

Server create_server(RequestHandler handler = {}, const ServerOptions& options = ServerOptions());
Server create_server(const ServerOptions& options, RequestHandler handler = {});

struct AgentOptions {
    bool                      keep_alive = true;
    std::size_t               max_sockets = 0;
    std::size_t               max_free_sockets = 256;
    std::chrono::milliseconds free_socket_timeout{4000};
    std::size_t               max_replay_bytes = 1u << 20;
};

class Agent {
public:
    struct State;

    Agent() noexcept = default;
    explicit Agent(const AgentOptions& options);
    explicit Agent(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    static Agent global();

    AgentOptions options() const;
    std::size_t free_socket_count() const;
    std::size_t active_socket_count() const;
    std::size_t pending_request_count() const;
    void close_idle_sockets();
    void destroy();

    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

struct RequestOptions {
    std::string                method = "GET";
    std::string                protocol = "http:";
    std::string                host = "localhost";
    std::uint16_t              port = 0;
    std::string                path = "/";
    Headers                    headers;
    std::string                auth;
    std::chrono::milliseconds  timeout{0};
    std::chrono::milliseconds  connect_timeout{30000};
    net::Family                family = net::Family::Any;
    bool                       set_host = true;
    std::string                servername;
    tls::SecureContextOptions  secure;
    std::shared_ptr<security::TLSContext> secure_context;
    std::size_t                max_header_size = 16384;
    Agent                      agent;
    bool                       use_agent = true;

    static RequestOptions from_url(const URL& url);
    static RequestOptions from_url(const std::string& url);
};

class ClientRequest {
public:
    struct State;

    struct Events {
        Event<IncomingMessage>                                 response;
        Event<IncomingMessage, net::Socket, const DataView&>   upgrade;
        Event<int, const Headers&>                             information;
        Event<>                                                continue_;
        Event<net::Socket>                                     socket;
        Event<>                                                finish;
        Event<>                                                timeout;
        Event<>                                                close;
        Event<const std::error_code&>                          error;
    };

    ClientRequest() noexcept = default;
    explicit ClientRequest(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    ClientRequest& set_header(const std::string& name, const std::string& value);
    std::string get_header(const std::string& name) const;
    bool has_header(const std::string& name) const;
    ClientRequest& remove_header(const std::string& name);

    bool write(const DataView& chunk, std::function<void(const std::error_code&)> done = {});
    ClientRequest& end(std::function<void()> done = {});
    ClientRequest& end(const DataView& data, std::function<void()> done = {});
    ClientRequest& destroy(const std::error_code& reason = {});
    ClientRequest& abort() { return destroy(); }
    ClientRequest& set_timeout(std::chrono::milliseconds timeout, std::function<void()> fn = {});
    ClientRequest& set_no_delay(bool enable = true);

    ClientRequest& on_response(std::function<void(IncomingMessage)> fn);
    ClientRequest& on_upgrade(UpgradeHandler fn);
    ClientRequest& on_information(std::function<void(int, const Headers&)> fn);
    ClientRequest& on_continue(std::function<void()> fn);
    ClientRequest& on_socket(std::function<void(net::Socket)> fn);
    ClientRequest& on_finish(std::function<void()> fn);
    ClientRequest& on_timeout(std::function<void()> fn);
    ClientRequest& on_close(std::function<void()> fn);
    ClientRequest& on_error(std::function<void(const std::error_code&)> fn);
    Events& events();

    bool headers_sent() const noexcept;
    bool destroyed() const noexcept;
    bool reused_socket() const noexcept;
    const std::string& method() const;
    const std::string& path() const;
    const std::string& host() const;
    net::Socket socket() const;
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

ClientRequest request(const RequestOptions& options, std::function<void(IncomingMessage)> on_response = {});
ClientRequest request(const std::string& url, std::function<void(IncomingMessage)> on_response = {});
ClientRequest request(const std::string& url, RequestOptions options, std::function<void(IncomingMessage)> on_response = {});
ClientRequest get(const RequestOptions& options, std::function<void(IncomingMessage)> on_response = {});
ClientRequest get(const std::string& url, std::function<void(IncomingMessage)> on_response = {});

struct FetchOptions {
    std::string                method = "GET";
    Headers                    headers;
    std::string                body;
    std::chrono::milliseconds  timeout{30000};
    bool                       follow_redirects = true;
    int                        max_redirects = 20;
    std::size_t                max_body = 64u << 20;
    tls::SecureContextOptions  secure;
    Agent                      agent;
    bool                       use_agent = true;
};

struct FetchResponse {
    int         status = 0;
    std::string status_text;
    Headers     headers;
    std::string body;
    std::string url;
    bool        redirected = false;
    int         redirect_count = 0;

    bool ok() const noexcept { return status >= 200 && status < 300; }
    const std::string& text() const noexcept { return body; }
};

using FetchCallback = std::function<void(const std::error_code&, FetchResponse)>;

void fetch(const std::string& url, FetchCallback callback);
void fetch(const std::string& url, const FetchOptions& options, FetchCallback callback);

namespace detail {

Server make_server(const ServerOptions& options, std::shared_ptr<security::TLSContext> tls);

} // namespace detail

} // namespace http

namespace https {

struct ServerOptions : http::ServerOptions {
    tls::SecureContextOptions             secure;
    std::shared_ptr<security::TLSContext> secure_context;
};

http::Server create_server(const ServerOptions& options, http::RequestHandler handler = {});
http::Server create_server(const tls::SecureContextOptions& options, http::RequestHandler handler = {});
http::ClientRequest request(const http::RequestOptions& options, std::function<void(http::IncomingMessage)> on_response = {});
http::ClientRequest request(const std::string& url, std::function<void(http::IncomingMessage)> on_response = {});
http::ClientRequest get(const std::string& url, std::function<void(http::IncomingMessage)> on_response = {});
http::ClientRequest get(const http::RequestOptions& options, std::function<void(http::IncomingMessage)> on_response = {});

} // namespace https
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_HTTP_HPP
