#include "fizmo_library.hpp"
#include "http_internal.hpp"

#include <atomic>
#include <cstdio>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace http {

namespace {

std::string hex_size(std::size_t n) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%zx", n);
    return buf;
}

} // namespace

struct ServerResponse::State : std::enable_shared_from_this<ServerResponse::State> {
    mutable std::mutex        m;
    Events                    ev;
    int                       status = 200;
    std::string               message;
    Headers                   headers;
    Headers                   trailers;
    bool                      send_date = true;
    bool                      headers_sent = false;
    bool                      ended = false;
    bool                      finished = false;
    bool                      close_emitted = false;
    bool                      chunked = false;
    bool                      no_body = false;
    bool                      head_request = false;
    bool                      req_http11 = true;
    bool                      req_keep_alive = true;
    bool                      final_keep = false;
    std::chrono::milliseconds keep_alive_timeout{5000};
    net::Socket               socket;
    std::function<void(bool)> on_done;

    std::string build_head_locked(bool ending, std::size_t body_size) {
        no_body = head_request || (status >= 100 && status < 200) || status == 204 || status == 304;
        bool close_after = false;
        const bool user_close = headers.has_token("connection", "close");
        const bool te_set = headers.has("transfer-encoding");
        const bool cl_set = headers.has("content-length");

        if (!no_body) {
            if (te_set) {
                chunked = headers.has_token("transfer-encoding", "chunked");

                if (!req_http11) {
                    headers.remove("transfer-encoding");
                    chunked = false;
                    close_after = !cl_set;
                } else if (!chunked) {
                    close_after = true;
                }
            } else if (cl_set) {
                chunked = false;
            } else if (ending) {
                headers.set("Content-Length", std::to_string(body_size));
            } else if (req_http11) {
                headers.set("Transfer-Encoding", "chunked");
                chunked = true;
            } else {
                close_after = true;
            }
        } else if (head_request && ending && body_size > 0 && !cl_set && !te_set) {
            headers.set("Content-Length", std::to_string(body_size));
        }

        const bool keep = req_keep_alive && !user_close && !close_after;

        if (!headers.has("connection")) {
            if (keep) {
                headers.set("Connection", "keep-alive");
                const auto secs = keep_alive_timeout.count() / 1000;
                if (secs > 0 && !headers.has("keep-alive")) headers.set("Keep-Alive", "timeout=" + std::to_string(secs));
            } else {
                headers.set("Connection", "close");
            }
        } else if (!keep && !user_close) {
            headers.set("Connection", "close");
            headers.remove("keep-alive");
        }

        if (send_date && !headers.has("date")) headers.set("Date", http_date());
        final_keep = keep;
        std::string head = "HTTP/1.1 " + std::to_string(status) + " " + (message.empty() ? std::string(status_text(status)) : message) + "\r\n";
        head += headers.serialize();
        head += "\r\n";
        headers_sent = true;
        return head;
    }

    void finish(const std::error_code& ec) {
        bool keep;
        std::function<void(bool)> done;

        {
            std::lock_guard<std::mutex> lk(m);
            if (finished) return;
            finished = !ec;
            keep = final_keep && !ec;
            done = std::move(on_done);
            on_done = nullptr;
        }

        if (!ec) ev.finish.emit();
        emit_close();
        if (done) done(keep);
    }

    void emit_close() {
        {
            std::lock_guard<std::mutex> lk(m);
            if (close_emitted) return;
            close_emitted = true;
        }

        ev.close.emit();
        ev.finish.clear();
        ev.close.clear();
    }
};

int ServerResponse::status_code() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->status;
}

ServerResponse& ServerResponse::set_status_code(int code) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent && code >= 100 && code <= 999) m_state->status = code;
    return *this;
}

std::string ServerResponse::status_message() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->message.empty() ? std::string(status_text(m_state->status)) : m_state->message;
}

ServerResponse& ServerResponse::set_status_message(const std::string& message) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent && Headers::valid_value(message)) m_state->message = message;
    return *this;
}

ServerResponse& ServerResponse::set_header(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent && Headers::valid_name(name) && Headers::valid_value(value)) m_state->headers.set(name, value);
    return *this;
}

ServerResponse& ServerResponse::set_header(const std::string& name, const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (m_state->headers_sent || !Headers::valid_name(name)) return *this;
    for (auto& v : values) if (!Headers::valid_value(v)) return *this;
    m_state->headers.set(name, values);
    return *this;
}

ServerResponse& ServerResponse::append_header(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent && Headers::valid_name(name) && Headers::valid_value(value)) m_state->headers.append(name, value);
    return *this;
}

std::string ServerResponse::get_header(const std::string& name) const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers.get(name);
}

bool ServerResponse::has_header(const std::string& name) const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers.has(name);
}

ServerResponse& ServerResponse::remove_header(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent) m_state->headers.remove(name);
    return *this;
}

Headers ServerResponse::get_headers() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers;
}

std::vector<std::string> ServerResponse::get_header_names() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers.names();
}

ServerResponse& ServerResponse::set_send_date(bool send) {
    std::lock_guard<std::mutex> lk(m_state->m);
    m_state->send_date = send;
    return *this;
}

ServerResponse& ServerResponse::add_trailers(const Headers& trailers) {
    std::lock_guard<std::mutex> lk(m_state->m);
    for (auto& e : trailers) if (Headers::valid_name(e.first) && Headers::valid_value(e.second)) m_state->trailers.append(e.first, e.second);
    return *this;
}

ServerResponse& ServerResponse::write_head(int code, const std::string& message, const Headers& headers) {
    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (m_state->headers_sent) return *this;
        if (code >= 100 && code <= 999) m_state->status = code;
        if (!message.empty() && Headers::valid_value(message)) m_state->message = message;
        for (auto& e : headers) if (Headers::valid_name(e.first) && Headers::valid_value(e.second)) m_state->headers.set(e.first, e.second);
    }

    return *this;
}

bool ServerResponse::write(const DataView& chunk, std::function<void(const std::error_code&)> done) {
    std::string out;

    {
        std::lock_guard<std::mutex> lk(m_state->m);

        if (m_state->ended) {
            const auto ec = async::make_error_code(async::error::shut_down);
            if (done) loop().post([done, ec]() { done(ec); });
            return false;
        }

        if (!m_state->headers_sent) out = m_state->build_head_locked(false, 0);

        if (!m_state->no_body && !chunk.empty()) {
            if (m_state->chunked) {
                out += hex_size(chunk.size());
                out += "\r\n";
                out.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
                out += "\r\n";
            } else {
                out.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            }
        }
    }

    if (out.empty()) {
        if (done) loop().post([done]() { done({}); });
        return true;
    }

    return m_state->socket.write(DataView(out), std::move(done));
}

ServerResponse& ServerResponse::end(std::function<void()> done) {
    return end(DataView(), std::move(done));
}

ServerResponse& ServerResponse::end(const DataView& data, std::function<void()> done) {
    std::string out;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (m_state->ended) return *this;
        m_state->ended = true;
        if (!m_state->headers_sent) out = m_state->build_head_locked(true, data.size());

        if (!m_state->no_body) {
            if (m_state->chunked) {
                if (!data.empty()) {
                    out += hex_size(data.size());
                    out += "\r\n";
                    out.append(reinterpret_cast<const char*>(data.data()), data.size());
                    out += "\r\n";
                }

                out += "0\r\n";
                out += m_state->trailers.serialize();
                out += "\r\n";
            } else {
                out.append(reinterpret_cast<const char*>(data.data()), data.size());
            }
        }
    }

    auto st = m_state;
    m_state->socket.write(DataView(out), [st, done](const std::error_code& ec) {
        st->finish(ec);
        if (done && !ec) done();
    });

    return *this;
}

ServerResponse& ServerResponse::send(const DataView& body, const std::string& content_type) {
    if (!has_header("content-type") && !content_type.empty()) set_header("Content-Type", content_type);
    return end(body);
}

ServerResponse& ServerResponse::send(int code, const DataView& body, const std::string& content_type) {
    set_status_code(code);
    return send(body, content_type);
}

ServerResponse& ServerResponse::json(const std::string& json_text, int code) {
    set_status_code(code);
    set_header("Content-Type", "application/json; charset=utf-8");
    return end(DataView(json_text));
}

ServerResponse& ServerResponse::redirect(const std::string& location, int code) {
    set_status_code(code);
    set_header("Location", location);
    return end();
}

void ServerResponse::write_continue() {
    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (m_state->headers_sent) return;
    }

    m_state->socket.write(DataView("HTTP/1.1 100 Continue\r\n\r\n"));
}

bool ServerResponse::headers_sent() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers_sent;
}

bool ServerResponse::writable_ended() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->ended;
}

bool ServerResponse::writable_finished() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->finished;
}

ServerResponse& ServerResponse::on_finish(std::function<void()> fn) { m_state->ev.finish.add(std::move(fn)); return *this; }
ServerResponse& ServerResponse::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
ServerResponse::Events& ServerResponse::events() { return m_state->ev; }
net::Socket ServerResponse::socket() const { return m_state->socket; }

namespace server_detail {

struct Conn;

} // namespace server_detail

using server_detail::Conn;

struct Server::State : std::enable_shared_from_this<Server::State> {
    mutable std::mutex                                 m;
    Events                                             ev;
    ServerOptions                                      opt;
    net::Server                                        net;
    std::unordered_map<std::uint64_t, std::shared_ptr<Conn>> conns;
    std::atomic<bool>                                  closing{false};
    std::shared_ptr<State>                             keep_alive;

    void hold() {
        std::lock_guard<std::mutex> lk(m);
        keep_alive = shared_from_this();
    }

    void release() {
        std::shared_ptr<State> doomed;
        std::lock_guard<std::mutex> lk(m);
        doomed.swap(keep_alive);
    }

    void add(std::uint64_t id, const std::shared_ptr<Conn>& c) {
        std::lock_guard<std::mutex> lk(m);
        conns[id] = c;
    }

    void remove(std::uint64_t id) {
        std::lock_guard<std::mutex> lk(m);
        conns.erase(id);
    }

    std::vector<std::shared_ptr<Conn>> snapshot() const {
        std::vector<std::shared_ptr<Conn>> out;
        std::lock_guard<std::mutex> lk(m);
        for (auto& kv : conns) out.push_back(kv.second);
        return out;
    }
};

namespace server_detail {

enum Phase : int { kIdle = 0, kHead = 1, kActive = 2 };

struct Conn : std::enable_shared_from_this<Conn> {
    std::weak_ptr<Server::State>                server;
    ServerOptions                               opt;
    net::Socket                                 sock;
    detail::Parser                              parser;
    Bytes                                       inbuf;
    std::shared_ptr<IncomingMessage::State>     req;
    std::shared_ptr<ServerResponse::State>      res;
    std::atomic<int>                            phase{kIdle};
    std::atomic<std::uint64_t>                  timer_token{0};
    Timer                                       timer;
    std::mutex                                  timer_mutex;
    std::mutex                                  res_mutex;
    bool                                        response_done = true;
    bool                                        request_complete = false;
    bool                                        waiting = false;
    bool                                        upgraded = false;
    bool                                        closing = false;
    std::atomic<bool>                           closed{false};
    std::size_t                                 served = 0;
    ListenerId                                  id_data = 0;
    ListenerId                                  id_close = 0;
    ListenerId                                  id_error = 0;

    Conn(const std::shared_ptr<Server::State>& srv, net::Socket s)
        : server(srv), opt(srv->opt), sock(std::move(s)), parser(detail::Parser::Mode::Request, make_limits(srv->opt)) {}

    static detail::Parser::Limits make_limits(const ServerOptions& o) {
        detail::Parser::Limits l;
        l.max_head_size = o.max_header_size;
        l.max_headers = o.max_headers_count;
        l.max_line = o.max_header_size;
        return l;
    }

    void attach() {
        std::weak_ptr<Conn> weak = shared_from_this();
        parser.on_headers = [this]() { on_headers(); };
        parser.on_body = [this](const std::uint8_t* d, std::size_t n) { if (req) req->push_body(d, n); };
        refresh_upgrade_flag();
        id_data = sock.events().data.add([weak](const DataView& d) { if (auto c = weak.lock()) c->on_data(d); });
        id_close = sock.events().close.add([weak](bool) { if (auto c = weak.lock()) c->on_close(); });
        id_error = sock.events().error.add([](const std::error_code&) {});
        arm(kIdle, opt.headers_timeout);
    }

    void refresh_upgrade_flag() {
        auto srv = server.lock();
        parser.set_upgrade_enabled(srv && !srv->ev.upgrade.empty());
    }

    void arm(int kind, std::chrono::milliseconds ms) {
        const std::uint64_t token = ++timer_token;
        std::lock_guard<std::mutex> lk(timer_mutex);
        timer.cancel();
        if (ms.count() <= 0 || closed) return;
        std::weak_ptr<Conn> weak = shared_from_this();

        timer = set_timeout([weak, token, kind]() {
            auto c = weak.lock();
            if (!c || c->timer_token != token || c->closed) return;
            c->on_timer(kind);
        }, ms);
    }

    void disarm() {
        ++timer_token;
        std::lock_guard<std::mutex> lk(timer_mutex);
        timer.cancel();
    }

    void on_timer(int kind) {
        if (kind == kIdle && phase == kIdle) {
            sock.destroy();
            return;
        }

        std::shared_ptr<ServerResponse::State> r;

        {
            std::lock_guard<std::mutex> lk(res_mutex);
            r = res;
        }

        bool started = false;

        if (r) {
            std::lock_guard<std::mutex> lk(r->m);
            started = r->headers_sent;
        }

        if (!started && (phase == kHead || phase == kIdle || !r)) {
            sock.end(DataView("HTTP/1.1 408 Request Timeout\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"));
            return;
        }

        if (!started && r) {
            {
                std::lock_guard<std::mutex> lk(r->m);
                r->req_keep_alive = false;
            }

            ServerResponse(r).set_status_code(408).end();
            return;
        }

        sock.destroy();
    }

    void on_data(const DataView& d) {
        if (upgraded || closing) return;
        inbuf.insert(inbuf.end(), d.begin(), d.end());
        process();
    }

    void process() {
        while (!inbuf.empty() && !waiting && !upgraded && !closing && !closed) {
            if (parser.state() == detail::Parser::State::Idle && phase == kIdle) {
                phase = kHead;
                arm(kHead, opt.headers_timeout);
            }

            const std::size_t n = parser.feed(inbuf.data(), inbuf.size());
            inbuf.erase(inbuf.begin(), inbuf.begin() + static_cast<std::ptrdiff_t>(n));

            if (parser.error()) {
                protocol_error();
                return;
            }

            if (parser.state() == detail::Parser::State::Upgrade) {
                do_upgrade();
                return;
            }

            if (parser.state() == detail::Parser::State::Complete) {
                on_message_complete();
                continue;
            }

            if (n == 0) break;
        }

        if (parser.state() == detail::Parser::State::Idle && phase == kHead && inbuf.empty() && response_done) {
            phase = kIdle;
            arm(kIdle, opt.keep_alive_timeout);
        }
    }

    void on_headers() {
        auto srv = server.lock();
        auto r = std::make_shared<IncomingMessage::State>();
        r->method = parser.method();
        r->url = parser.target();
        r->version = parser.version();
        r->headers = parser.headers();
        r->keep_alive = parser.keep_alive();
        r->socket = sock;
        r->backlog_limit = opt.max_body_backlog;

        auto s = std::make_shared<ServerResponse::State>();
        s->socket = sock;
        s->head_request = parser.method() == "HEAD";
        s->req_http11 = parser.version_minor() >= 1;
        const bool server_closing = !srv || srv->closing;
        s->req_keep_alive = parser.keep_alive() && !server_closing && (opt.max_requests_per_socket == 0 || served + 1 < opt.max_requests_per_socket);
        s->keep_alive_timeout = opt.keep_alive_timeout;
        std::weak_ptr<Conn> weak = shared_from_this();
        s->on_done = [weak](bool keep) { if (auto c = weak.lock()) c->response_finished(keep); };

        if (req) req->emit_close();

        {
            std::lock_guard<std::mutex> lk(res_mutex);
            req = r;
            res = s;
        }

        response_done = false;
        request_complete = false;
        phase = kActive;
        arm(kActive, opt.request_timeout);

        if (parser.upgrade() && srv && !srv->ev.upgrade.empty()) return;
        if (!srv) { sock.destroy(); return; }

        const std::string expect = r->headers.get("expect");

        if (!expect.empty()) {
            if (Headers::iequals(expect, "100-continue")) {
                if (!srv->ev.check_continue.empty()) {
                    srv->ev.check_continue.emit(IncomingMessage(r), ServerResponse(s));
                    return;
                }

                ServerResponse(s).write_continue();
            } else {
                {
                    std::lock_guard<std::mutex> lk(s->m);
                    s->req_keep_alive = false;
                }

                ServerResponse(s).set_status_code(417).end();
                return;
            }
        }

        if (srv->ev.request.emit(IncomingMessage(r), ServerResponse(s)) == 0) {
            ServerResponse(s).set_status_code(404).end();
        }
    }

    void on_message_complete() {
        if (req) req->mark_complete();
        ++served;
        parser.reset();
        refresh_upgrade_flag();
        request_complete = true;

        if (!response_done) {
            waiting = true;
            sock.pause();
            return;
        }

        next_request();
    }

    void response_finished(bool keep) {
        auto srv = server.lock();
        if (!srv || srv->closing) keep = false;
        response_done = true;

        if (!keep) {
            closing = true;
            disarm();
            if (req && request_complete) req->emit_close();
            sock.end();
            return;
        }

        if (request_complete) next_request();
    }

    void next_request() {
        if (req) req->emit_close();
        request_complete = false;
        waiting = false;
        phase = kIdle;
        arm(kIdle, opt.keep_alive_timeout);
        sock.resume();
        if (!inbuf.empty()) process();
    }

    void protocol_error() {
        const int status = parser.error_status() ? parser.error_status() : 400;
        const auto ec = async::make_error_code(async::error::protocol_error);
        closing = true;
        disarm();
        auto srv = server.lock();

        if (srv && !srv->ev.client_error.empty()) {
            srv->ev.client_error.emit(ec, sock);
            return;
        }

        if (response_done || !res || !ServerResponse(res).headers_sent()) {
            const std::string text = status_text(status);
            sock.end(DataView("HTTP/1.1 " + std::to_string(status) + " " + text + "\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"));
        } else {
            sock.destroy();
        }
    }

    void do_upgrade() {
        upgraded = true;
        disarm();
        sock.events().data.remove(id_data);
        sock.events().close.remove(id_close);
        sock.events().error.remove(id_error);
        Bytes head;
        head.swap(inbuf);
        auto srv = server.lock();
        if (srv) srv->remove(sock.id());
        auto r = req;
        auto s = sock;

        if (!srv || srv->ev.upgrade.emit(IncomingMessage(r), s, DataView(head)) == 0) {
            s.destroy();
        }
    }

    void on_close() {
        closed = true;
        disarm();
        if (req && !request_complete && !upgraded) req->mark_aborted(async::make_error_code(async::error::connection_reset));
        if (res) res->emit_close();
        if (req) req->emit_close();
        auto srv = server.lock();
        if (srv) srv->remove(sock.id());
        parser.on_headers = nullptr;
        parser.on_body = nullptr;
    }
};

} // namespace server_detail

using server_detail::kIdle;

namespace detail {

Server make_server(const ServerOptions& options, std::shared_ptr<security::TLSContext> tls) {
    auto st = std::make_shared<Server::State>();
    st->opt = options;
    st->net = net::detail::make_server(options, std::move(tls));
    std::weak_ptr<Server::State> weak = st;

    st->net.on_connection([weak](net::Socket s) {
        auto srv = weak.lock();
        if (!srv) { s.destroy(); return; }
        auto c = std::make_shared<Conn>(srv, s);
        srv->add(s.id(), c);
        c->attach();
        srv->ev.connection.emit(s);
    });

    st->net.on_listening([weak]() { if (auto s = weak.lock()) s->ev.listening.emit(); });

    st->net.on_close([weak]() {
        auto s = weak.lock();
        if (!s) return;
        s->ev.close.emit();
        s->release();
    });

    st->net.on_error([weak](const std::error_code& ec) {
        auto s = weak.lock();
        if (!s) return;
        if (!s->net.listening()) s->release();
        if (s->ev.error.emit(ec) == 0) report_unhandled_error("http::Server", ec);
    });

    st->net.on_tls_client_error([weak](const std::error_code& ec, const std::string&, const async::Endpoint&) {
        auto s = weak.lock();
        if (s) s->ev.client_error.emit(ec, net::Socket());
    });

    return Server(st);
}

} // namespace detail

Server& Server::listen(std::uint16_t port, std::function<void()> cb) { m_state->closing = false; m_state->hold(); m_state->net.listen(port, std::move(cb)); return *this; }
Server& Server::listen(std::uint16_t port, const std::string& host, std::function<void()> cb) { m_state->closing = false; m_state->hold(); m_state->net.listen(port, host, std::move(cb)); return *this; }
Server& Server::listen(const net::ListenOptions& options, std::function<void()> cb) { m_state->closing = false; m_state->hold(); m_state->net.listen(options, std::move(cb)); return *this; }

Server& Server::close(std::function<void(const std::error_code&)> cb) {
    m_state->closing = true;
    m_state->net.close(std::move(cb));
    close_idle_connections();
    return *this;
}

Server& Server::close_all_connections() {
    for (auto& c : m_state->snapshot()) c->sock.destroy();
    return *this;
}

Server& Server::close_idle_connections() {
    for (auto& c : m_state->snapshot()) {
        if (c->phase == kIdle) c->sock.destroy();
    }

    return *this;
}

Server& Server::on_request(RequestHandler fn) { m_state->ev.request.add(std::move(fn)); return *this; }

Server& Server::on_upgrade(UpgradeHandler fn) {
    m_state->ev.upgrade.add(std::move(fn));
    return *this;
}

Server& Server::on_check_continue(RequestHandler fn) { m_state->ev.check_continue.add(std::move(fn)); return *this; }
Server& Server::on_client_error(std::function<void(const std::error_code&, net::Socket)> fn) { m_state->ev.client_error.add(std::move(fn)); return *this; }
Server& Server::on_connection(std::function<void(net::Socket)> fn) { m_state->ev.connection.add(std::move(fn)); return *this; }
Server& Server::on_listening(std::function<void()> fn) { m_state->ev.listening.add(std::move(fn)); return *this; }
Server& Server::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
Server& Server::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
Server::Events& Server::events() { return m_state->ev; }

Server& Server::set_keep_alive_timeout(std::chrono::milliseconds t) { m_state->opt.keep_alive_timeout = t; return *this; }
Server& Server::set_headers_timeout(std::chrono::milliseconds t) { m_state->opt.headers_timeout = t; return *this; }
Server& Server::set_request_timeout(std::chrono::milliseconds t) { m_state->opt.request_timeout = t; return *this; }

bool Server::listening() const noexcept { return m_state->net.listening(); }
net::AddressInfo Server::address() const { return m_state->net.address(); }
std::uint16_t Server::port() const { return m_state->net.port(); }

std::size_t Server::connection_count() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->conns.size();
}

net::Server& Server::net_server() { return m_state->net; }

Server create_server(RequestHandler handler, const ServerOptions& options) {
    Server s = detail::make_server(options, nullptr);
    if (handler) s.on_request(std::move(handler));
    return s;
}

Server create_server(const ServerOptions& options, RequestHandler handler) {
    return create_server(std::move(handler), options);
}

} // namespace http

namespace https {

http::Server create_server(const ServerOptions& options, http::RequestHandler handler) {
    std::shared_ptr<security::TLSContext> ctx = options.secure_context;
    std::string why;
    if (!ctx) ctx = tls::create_secure_context(options.secure, true, &why);

    if (!ctx) {
        http::Server s = http::detail::make_server(options, std::make_shared<security::TLSContext>(security::ContextUse::Server));
        if (handler) s.on_request(std::move(handler));
        auto st = s.state();

        loop().post([st, why]() {
            const auto ec = async::make_error_code(async::error::tls_error);
            if (st->ev.error.emit(ec) == 0) report_unhandled_error("https::create_server: " + why, ec);
        });

        return s;
    }

    http::Server s = http::detail::make_server(options, ctx);
    if (handler) s.on_request(std::move(handler));
    return s;
}

http::Server create_server(const tls::SecureContextOptions& options, http::RequestHandler handler) {
    ServerOptions o;
    o.secure = options;
    return create_server(o, std::move(handler));
}

} // namespace https
} // namespace networking
} // namespace fizmo
