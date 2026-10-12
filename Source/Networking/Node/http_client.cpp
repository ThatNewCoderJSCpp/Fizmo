#include "fizmo_library.hpp"
#include "http_internal.hpp"
#include "../../Crypto/encoding.hpp"

#include <cctype>
#include <cstdio>

namespace fizmo {
namespace networking {
namespace http {

namespace {

std::string hex_size(std::size_t n) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%zx", n);
    return buf;
}

bool method_expects_body(const std::string& m) {
    return m == "POST" || m == "PUT" || m == "PATCH";
}

bool is_secure_protocol(const std::string& p) {
    return p == "https:" || p == "wss:" || p == "https" || p == "wss";
}

} // namespace

RequestOptions RequestOptions::from_url(const URL& url) {
    RequestOptions o;
    o.protocol = url.protocol();
    o.host = url.bare_hostname();
    o.port = url.port().empty() ? 0 : url.port_number();
    o.path = url.path_and_query();

    if (!url.username().empty() || !url.password().empty()) {
        o.auth = decode_uri_component(url.username());
        if (!url.password().empty()) o.auth += ":" + decode_uri_component(url.password());
    }

    return o;
}

RequestOptions RequestOptions::from_url(const std::string& url) { return from_url(URL(url)); }

struct ClientRequest::State : std::enable_shared_from_this<ClientRequest::State> {
    using WriteDone = std::function<void(const std::error_code&)>;

    mutable std::mutex                       m;
    Events                                   ev;
    RequestOptions                           opt;
    Headers                                  headers;
    std::string                              method;
    std::string                              path;
    std::string                              host;
    std::string                              host_header;
    net::Socket                              sock;
    detail::Parser                           parser;
    bool                                     headers_sent = false;
    bool                                     ended = false;
    bool                                     destroyed = false;
    bool                                     chunked = false;
    bool                                     finished = false;
    bool                                     close_emitted = false;
    bool                                     upgraded = false;
    bool                                     got_response = false;
    bool                                     response_complete = false;
    bool                                     error_emitted = false;
    std::shared_ptr<IncomingMessage::State>  res;
    std::shared_ptr<Agent::State>            agent;
    Agent::State::Spec                       spec;
    bool                                     assigned = false;
    bool                                     reused = false;
    bool                                     counted = false;
    bool                                     released = false;
    bool                                     got_byte = false;
    bool                                     replayable = true;
    int                                      retries = 0;
    std::uint64_t                            waiter = 0;
    std::string                              replay;
    std::vector<std::pair<std::string, WriteDone>> preout;
    std::chrono::milliseconds                timeout{0};
    bool                                     no_delay = true;
    ListenerId                               id_data = 0;
    ListenerId                               id_close = 0;
    ListenerId                               id_error = 0;
    ListenerId                               id_timeout = 0;

    explicit State(const RequestOptions& o) : opt(o), parser(detail::Parser::Mode::Response, limits(o)) {}

    static detail::Parser::Limits limits(const RequestOptions& o) {
        detail::Parser::Limits l;
        l.max_head_size = o.max_header_size;
        l.max_line = o.max_header_size;
        return l;
    }

    bool keep_alive_enabled() const { return agent && agent->opt.keep_alive; }

    bool idempotent() const {
        return method == "GET" || method == "HEAD" || method == "OPTIONS" || method == "TRACE" || method == "PUT" || method == "DELETE";
    }

    std::string build_head_locked(bool ending, std::size_t body_size) {
        Headers h = headers;
        std::string head = method + " " + path + " HTTP/1.1\r\n";
        if (opt.set_host && !h.has("host")) head += "Host: " + host_header + "\r\n";
        if (!opt.auth.empty() && !h.has("authorization")) h.set("Authorization", "Basic " + crypto::base64_encode(opt.auth));

        if (!h.has("content-length") && !h.has("transfer-encoding")) {
            if (ending) {
                if (body_size > 0 || method_expects_body(method)) h.set("Content-Length", std::to_string(body_size));
                chunked = false;
            } else {
                h.set("Transfer-Encoding", "chunked");
                chunked = true;
            }
        } else {
            chunked = h.has_token("transfer-encoding", "chunked");
        }

        if (!h.has("connection")) h.set("Connection", keep_alive_enabled() ? "keep-alive" : "close");
        head += h.serialize();
        head += "\r\n";
        headers_sent = true;
        return head;
    }

    bool send_raw(std::string out, WriteDone done) {
        net::Socket s;

        {
            std::lock_guard<std::mutex> lk(m);

            if (agent && !got_byte && replayable) {
                if (replay.size() + out.size() <= agent->opt.max_replay_bytes) replay += out;
                else { replayable = false; replay.clear(); }
            }

            if (!assigned) {
                preout.emplace_back(std::move(out), std::move(done));
                return true;
            }

            s = sock;
        }

        return s.write(DataView(out), std::move(done));
    }

    void mark_finished() {
        {
            std::lock_guard<std::mutex> lk(m);
            if (finished) return;
            finished = true;
        }

        ev.finish.emit();
    }

    void assign(net::Socket s, bool was_reused) {
        std::vector<std::pair<std::string, WriteDone>> flush;
        bool dead;
        std::chrono::milliseconds t;

        {
            std::lock_guard<std::mutex> lk(m);
            dead = destroyed;
            sock = s;
            assigned = true;
            reused = was_reused;
            counted = agent != nullptr;
            waiter = 0;
            flush.swap(preout);
            t = timeout;
        }

        if (dead) {
            if (agent) {
                counted = false;
                if (was_reused) agent->release(spec.key, s, std::chrono::milliseconds(0));
                else { s.destroy(); agent->forget(spec.key); }
            } else {
                s.destroy();
            }

            for (auto& w : flush) if (w.second) w.second(async::make_error_code(async::error::operation_aborted));
            emit_close();
            return;
        }

        auto self = shared_from_this();
        id_data = s.events().data.add([self](const DataView& d) { self->on_data(d); });
        id_close = s.events().close.add([self](bool) { self->on_close(); });
        id_error = s.events().error.add([](const std::error_code&) {});
        id_timeout = s.events().timeout.add([self]() { self->ev.timeout.emit(); });
        if (t.count() > 0) s.set_timeout(t);
        if (!no_delay) s.set_no_delay(false);
        ev.socket.emit(s);

        for (auto& w : flush) {
            if (w.first.empty()) {
                if (w.second) s.write(DataView(), std::move(w.second));
                continue;
            }

            s.write(DataView(w.first), std::move(w.second));
        }
    }

    void detach_socket() {
        auto& e = sock.events();
        e.data.remove(id_data);
        e.close.remove(id_close);
        e.error.remove(id_error);
        e.timeout.remove(id_timeout);
    }

    void emit_error(const std::error_code& ec) {
        {
            std::lock_guard<std::mutex> lk(m);
            if (error_emitted) return;
            error_emitted = true;
        }

        if (ev.error.emit(ec) == 0) report_unhandled_error("http::ClientRequest", ec);
    }

    void on_headers() {
        const int status = parser.status();
        if (status >= 100 && status < 200 && status != 101) return;
        auto r = std::make_shared<IncomingMessage::State>();
        r->status = status;
        r->reason = parser.reason();
        r->version = parser.version();
        r->headers = parser.headers();
        r->keep_alive = parser.keep_alive();
        r->socket = sock;
        r->method = method;
        r->url = path;

        {
            std::lock_guard<std::mutex> lk(m);
            res = r;
            if (status != 101) got_response = true;
        }

        if (status == 101) return;
        if (ev.response.emit(IncomingMessage(r)) == 0) IncomingMessage(r).discard();
    }

    void on_data(const DataView& d) {
        if (upgraded || response_complete || released) return;

        {
            std::lock_guard<std::mutex> lk(m);
            got_byte = true;
            replay.clear();
        }

        std::size_t off = 0;

        while (off < d.size()) {
            const std::size_t n = parser.feed(d.data() + off, d.size() - off);
            off += n;

            if (parser.error()) {
                const auto ec = async::make_error_code(async::error::protocol_error);
                if (res && got_response) res->mark_aborted(ec);
                else emit_error(ec);
                sock.destroy();
                return;
            }

            if (parser.state() == detail::Parser::State::Upgrade) {
                handle_upgrade(d.slice(off));
                return;
            }

            if (parser.state() == detail::Parser::State::Complete) {
                const int status = parser.status();

                if (status >= 100 && status < 200 && status != 101) {
                    const Headers info = parser.headers();
                    if (status == 100) ev.continue_.emit();
                    ev.information.emit(status, info);
                    parser.reset();
                    parser.set_skip_body(method == "HEAD");
                    continue;
                }

                complete_response(off == d.size());
                return;
            }

            if (n == 0) break;
        }
    }

    std::chrono::milliseconds keep_alive_hint() const {
        const std::string ka = parser.headers().get("keep-alive");
        const std::size_t p = ka.find("timeout=");
        if (p == std::string::npos) return std::chrono::milliseconds(0);
        long secs = 0;
        for (std::size_t i = p + 8; i < ka.size() && ka[i] >= '0' && ka[i] <= '9'; ++i) secs = secs * 10 + (ka[i] - '0');
        if (secs <= 0) return std::chrono::milliseconds(0);
        return std::chrono::milliseconds(secs >= 2 ? secs * 1000 - 1000 : 500);
    }

    void complete_response(bool clean_boundary) {
        response_complete = true;
        bool keep = keep_alive_enabled() && parser.keep_alive() && !upgraded && clean_boundary && method != "CONNECT";

        {
            std::lock_guard<std::mutex> lk(m);
            keep = keep && finished && !destroyed;
        }

        const auto hint = keep ? keep_alive_hint() : std::chrono::milliseconds(0);

        if (res) {
            res->trailers = parser.trailers();
            res->mark_complete();
        }

        if (!keep) {
            sock.end();
            return;
        }

        detach_socket();
        released = true;
        counted = false;
        sock.resume();
        agent->release(spec.key, sock, hint);
        if (res) res->emit_close();
        emit_close();
    }

    void handle_upgrade(const DataView& head) {
        upgraded = true;
        Bytes copy = head.bytes();
        auto r = res;
        if (!r) r = std::make_shared<IncomingMessage::State>();
        r->socket = sock;
        net::Socket s = sock;
        s.events().data.clear();
        s.events().end.clear();
        s.events().close.clear();
        s.events().error.clear();
        s.events().timeout.clear();

        if (agent && counted) {
            counted = false;
            agent->forget(spec.key);
        }

        if (ev.upgrade.emit(IncomingMessage(r), s, DataView(copy)) == 0) s.destroy();
        emit_close();
    }

    bool try_retry() {
        if (!agent || retries > 0 || upgraded || response_complete || !idempotent()) return false;

        {
            std::lock_guard<std::mutex> lk(m);
            if (!reused || got_byte || destroyed || !ended || !replayable || replay.empty()) return false;
            ++retries;
            assigned = false;
            reused = false;
            sock = net::Socket();
            preout.clear();
            preout.emplace_back(replay, nullptr);
            auto self = shared_from_this();
            preout.emplace_back(std::string(), [self](const std::error_code& ec) { if (!ec) self->mark_finished(); });
        }

        parser.reset();
        parser.set_skip_body(method == "HEAD");
        auto self = shared_from_this();
        const std::uint64_t w = agent->acquire(spec, [self](net::Socket s, bool r) { self->assign(s, r); }, true);
        std::lock_guard<std::mutex> lk(m);
        waiter = w;
        return true;
    }

    void on_close() {
        if (released) return;

        if (agent && counted) {
            counted = false;
            agent->forget(spec.key);
        }

        if (try_retry()) return;

        if (!response_complete && !upgraded) {
            if (got_response && res) {
                if (parser.finish_eof() && parser.state() == detail::Parser::State::Complete) {
                    response_complete = true;
                    res->trailers = parser.trailers();
                    res->mark_complete();
                } else {
                    res->mark_aborted(async::make_error_code(async::error::connection_reset));
                }
            } else {
                bool user_destroyed;

                {
                    std::lock_guard<std::mutex> lk(m);
                    user_destroyed = destroyed;
                }

                std::error_code ec = sock ? sock.last_error() : std::error_code();
                if (!ec) ec = async::make_error_code(async::error::connection_reset);
                if (!user_destroyed) emit_error(ec);
            }
        }

        if (res) res->emit_close();
        emit_close();
    }

    void emit_close() {
        {
            std::lock_guard<std::mutex> lk(m);
            if (close_emitted) return;
            close_emitted = true;
        }

        ev.close.emit();
        ev.response.clear();
        ev.upgrade.clear();
        ev.information.clear();
        ev.continue_.clear();
        ev.socket.clear();
        ev.finish.clear();
        ev.timeout.clear();
        ev.close.clear();
        ev.error.clear();
        parser.on_headers = nullptr;
        parser.on_body = nullptr;
    }
};

ClientRequest& ClientRequest::set_header(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent && Headers::valid_name(name) && Headers::valid_value(value)) m_state->headers.set(name, value);
    return *this;
}

std::string ClientRequest::get_header(const std::string& name) const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers.get(name);
}

bool ClientRequest::has_header(const std::string& name) const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers.has(name);
}

ClientRequest& ClientRequest::remove_header(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->headers_sent) m_state->headers.remove(name);
    return *this;
}

bool ClientRequest::write(const DataView& chunk, std::function<void(const std::error_code&)> done) {
    std::string out;

    {
        std::lock_guard<std::mutex> lk(m_state->m);

        if (m_state->ended || m_state->destroyed) {
            const auto ec = async::make_error_code(async::error::shut_down);
            if (done) loop().post([done, ec]() { done(ec); });
            return false;
        }

        if (!m_state->headers_sent) out = m_state->build_head_locked(false, 0);

        if (!chunk.empty()) {
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

    return m_state->send_raw(std::move(out), std::move(done));
}

ClientRequest& ClientRequest::end(std::function<void()> done) { return end(DataView(), std::move(done)); }

ClientRequest& ClientRequest::end(const DataView& data, std::function<void()> done) {
    std::string out;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (m_state->ended || m_state->destroyed) return *this;
        m_state->ended = true;
        if (!m_state->headers_sent) out = m_state->build_head_locked(true, data.size());

        if (m_state->chunked) {
            if (!data.empty()) {
                out += hex_size(data.size());
                out += "\r\n";
                out.append(reinterpret_cast<const char*>(data.data()), data.size());
                out += "\r\n";
            }

            out += "0\r\n\r\n";
        } else {
            out.append(reinterpret_cast<const char*>(data.data()), data.size());
        }
    }

    auto st = m_state;

    m_state->send_raw(std::move(out), [st, done](const std::error_code& ec) {
        if (ec) return;
        st->mark_finished();
        if (done) done();
    });

    return *this;
}

ClientRequest& ClientRequest::destroy(const std::error_code& reason) {
    auto st = m_state;
    net::Socket s;
    bool assigned;
    std::uint64_t w;
    std::vector<std::pair<std::string, State::WriteDone>> dropped;

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->destroyed) return *this;
        st->destroyed = true;
        s = st->sock;
        assigned = st->assigned;
        w = st->waiter;
        if (!assigned) dropped.swap(st->preout);
    }

    if (reason) st->emit_error(reason);

    if (assigned) {
        if (s) s.destroy();
        return *this;
    }

    if (st->agent && w) st->agent->cancel(st->spec.key, w);
    for (auto& d : dropped) if (d.second) d.second(async::make_error_code(async::error::operation_aborted));
    loop().post([st]() { st->emit_close(); });
    return *this;
}

ClientRequest& ClientRequest::set_timeout(std::chrono::milliseconds timeout, std::function<void()> fn) {
    if (fn) m_state->ev.timeout.once(std::move(fn));
    net::Socket s;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        m_state->timeout = timeout;
        if (m_state->assigned) s = m_state->sock;
    }

    if (s) s.set_timeout(timeout);
    return *this;
}

ClientRequest& ClientRequest::set_no_delay(bool enable) {
    net::Socket s;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        m_state->no_delay = enable;
        if (m_state->assigned) s = m_state->sock;
    }

    if (s) s.set_no_delay(enable);
    return *this;
}

ClientRequest& ClientRequest::on_response(std::function<void(IncomingMessage)> fn) { m_state->ev.response.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_upgrade(UpgradeHandler fn) { m_state->ev.upgrade.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_information(std::function<void(int, const Headers&)> fn) { m_state->ev.information.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_continue(std::function<void()> fn) { m_state->ev.continue_.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_socket(std::function<void(net::Socket)> fn) { m_state->ev.socket.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_finish(std::function<void()> fn) { m_state->ev.finish.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_timeout(std::function<void()> fn) { m_state->ev.timeout.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
ClientRequest& ClientRequest::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
ClientRequest::Events& ClientRequest::events() { return m_state->ev; }

bool ClientRequest::headers_sent() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->headers_sent;
}

bool ClientRequest::destroyed() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->destroyed;
}

const std::string& ClientRequest::method() const { return m_state->method; }
const std::string& ClientRequest::path() const { return m_state->path; }
const std::string& ClientRequest::host() const { return m_state->host; }
net::Socket ClientRequest::socket() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->sock;
}

bool ClientRequest::reused_socket() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->reused;
}

ClientRequest request(const RequestOptions& options, std::function<void(IncomingMessage)> on_response) {
    auto st = std::make_shared<ClientRequest::State>(options);
    ClientRequest req(st);
    if (on_response) req.on_response(std::move(on_response));

    std::string method = options.method.empty() ? std::string("GET") : options.method;
    for (auto& c : method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    st->method = method;
    st->path = options.path.empty() ? std::string("/") : options.path;
    st->host = options.host.empty() ? std::string("localhost") : options.host;
    if (st->host.size() > 2 && st->host.front() == '[' && st->host.back() == ']') st->host = st->host.substr(1, st->host.size() - 2);
    for (auto& e : options.headers) if (Headers::valid_name(e.first) && Headers::valid_value(e.second)) st->headers.append(e.first, e.second);

    const bool secure = is_secure_protocol(options.protocol);
    const std::uint16_t def_port = secure ? 443 : 80;
    const std::uint16_t port = options.port ? options.port : def_port;
    const std::string bracketed = net::is_ipv6(st->host) ? "[" + st->host + "]" : st->host;
    st->host_header = port == def_port ? bracketed : bracketed + ":" + std::to_string(port);

    bool valid = Headers::valid_name(method);
    for (unsigned char c : st->path) if (c <= 0x20 || c == 0x7F) valid = false;

    net::ConnectOptions co;
    co.host = st->host;
    co.port = port;
    co.family = options.family;
    co.connect_timeout = options.connect_timeout;
    st->timeout = options.timeout;

    st->parser.set_skip_body(method == "HEAD");
    st->parser.on_headers = [raw = st.get()]() { raw->on_headers(); };
    st->parser.on_body = [raw = st.get()](const std::uint8_t* d, std::size_t n) { if (raw->res) raw->res->push_body(d, n); };

    const std::string servername = options.servername.empty() ? (net::is_ip(st->host) ? std::string() : st->host) : options.servername;

    if (!valid) {
        st->assign(net::detail::make_failed(async::make_error_code(async::error::invalid_argument), "Invalid method or path", false), false);
        return req;
    }

    if (!options.use_agent) {
        if (secure) {
            std::shared_ptr<security::TLSContext> ctx = options.secure_context;
            std::string why;
            if (!ctx) ctx = tls::create_secure_context(options.secure, false, &why);
            if (ctx) st->assign(net::detail::make_client(co, ctx, servername), false);
            else st->assign(net::detail::make_failed(async::make_error_code(async::error::tls_error), why, true), false);
        } else {
            st->assign(net::detail::make_client(co, nullptr, {}), false);
        }

        return req;
    }

    auto agent = options.agent ? options.agent.state() : Agent::global().state();
    std::string tls_key;
    std::shared_ptr<security::TLSContext> ctx;

    if (secure) {
        std::string why;
        ctx = agent->tls_context(options, tls_key, why);

        if (!ctx) {
            st->assign(net::detail::make_failed(async::make_error_code(async::error::tls_error), why, true), false);
            return req;
        }
    }

    st->agent = agent;
    st->spec.key = std::string(secure ? "https://" : "http://") + bracketed + ":" + std::to_string(port) + "|" + std::to_string(static_cast<int>(options.family)) + "|" + servername + "|" + tls_key;
    st->spec.connect = co;
    st->spec.tls = ctx;
    st->spec.servername = servername;
    const std::uint64_t w = agent->acquire(st->spec, [st](net::Socket s, bool reused) { st->assign(s, reused); }, false);

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (!st->assigned) st->waiter = w;
    }

    return req;
}

ClientRequest request(const std::string& url, std::function<void(IncomingMessage)> on_response) {
    URL u;
    std::string why;

    if (!URL::parse(url, u, &why)) {
        RequestOptions bad;
        bad.method = "INVALID URL";
        return request(bad, std::move(on_response));
    }

    return request(RequestOptions::from_url(u), std::move(on_response));
}

ClientRequest request(const std::string& url, RequestOptions options, std::function<void(IncomingMessage)> on_response) {
    URL u;

    if (URL::parse(url, u)) {
        const RequestOptions from = RequestOptions::from_url(u);
        options.protocol = from.protocol;
        options.host = from.host;
        options.port = from.port;
        options.path = from.path;
        if (options.auth.empty()) options.auth = from.auth;
    } else {
        options.method = "INVALID URL";
    }

    return request(options, std::move(on_response));
}

ClientRequest get(const RequestOptions& options, std::function<void(IncomingMessage)> on_response) {
    RequestOptions o = options;
    o.method = "GET";
    ClientRequest r = request(o, std::move(on_response));
    r.end();
    return r;
}

ClientRequest get(const std::string& url, std::function<void(IncomingMessage)> on_response) {
    ClientRequest r = request(url, std::move(on_response));
    r.end();
    return r;
}

namespace {

struct FetchState : std::enable_shared_from_this<FetchState> {
    FetchOptions  opt;
    FetchCallback cb;
    URL           url;
    std::string   method;
    std::string   body;
    Headers       headers;
    int           redirects = 0;
    bool          done = false;
    std::mutex    m;
    Timer         timer;
    ClientRequest current;

    void finish(const std::error_code& ec, FetchResponse resp) {
        FetchCallback f;
        ClientRequest r;

        {
            std::lock_guard<std::mutex> lk(m);
            if (done) return;
            done = true;
            f = std::move(cb);
            cb = nullptr;
            r = current;
        }

        timer.cancel();
        if (ec && r) r.destroy();
        if (f) f(ec, std::move(resp));
    }

    void start() {
        if (url.scheme() != "http" && url.scheme() != "https") {
            finish(async::make_error_code(async::error::unsupported), FetchResponse());
            return;
        }

        RequestOptions ro = RequestOptions::from_url(url);
        ro.method = method;
        ro.headers = headers;
        ro.secure = opt.secure;
        ro.agent = opt.agent;
        ro.use_agent = opt.use_agent;
        if (!ro.headers.has("user-agent")) ro.headers.set("User-Agent", "fizmo-networking");
        if (!ro.headers.has("accept")) ro.headers.set("Accept", "*/*");
        auto self = shared_from_this();

        ClientRequest req = request(ro, [self](IncomingMessage res) { self->on_response(res); });
        req.on_error([self](const std::error_code& ec) { self->finish(ec, FetchResponse()); });

        {
            std::lock_guard<std::mutex> lk(m);
            current = req;
        }

        req.end(DataView(body));
    }

    void on_response(IncomingMessage res) {
        const int status = res.status_code();
        const bool redirect = status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
        const std::string location = res.header("location");

        if (opt.follow_redirects && redirect && !location.empty()) {
            res.discard();

            if (++redirects > opt.max_redirects) {
                finish(async::make_error_code(async::error::limit_exceeded), FetchResponse());
                return;
            }

            URL next;

            if (!URL::parse(location, url, next)) {
                finish(async::make_error_code(async::error::protocol_error), FetchResponse());
                return;
            }

            if (status == 303 || ((status == 301 || status == 302) && method == "POST")) {
                if (method != "HEAD") method = "GET";
                body.clear();
                headers.remove("content-length");
                headers.remove("content-type");
                headers.remove("transfer-encoding");
            }

            if (next.origin() != url.origin()) {
                headers.remove("authorization");
                headers.remove("cookie");
                headers.remove("proxy-authorization");
            }

            ClientRequest old;

            {
                std::lock_guard<std::mutex> lk(m);
                old = current;
            }

            url = next;
            if (old) old.destroy();
            start();
            return;
        }

        auto self = shared_from_this();
        FetchResponse base;
        base.status = status;
        base.status_text = res.status_message();
        base.headers = res.headers();
        base.url = url.href();
        base.redirected = redirects > 0;
        base.redirect_count = redirects;

        res.body([self, base](const std::error_code& ec, std::string text) mutable {
            base.body = std::move(text);
            self->finish(ec, std::move(base));
        }, opt.max_body);
    }
};

} // namespace

void fetch(const std::string& url, FetchCallback callback) { fetch(url, FetchOptions(), std::move(callback)); }

void fetch(const std::string& url, const FetchOptions& options, FetchCallback callback) {
    auto st = std::make_shared<FetchState>();
    st->opt = options;
    st->cb = std::move(callback);
    st->method = options.method.empty() ? std::string("GET") : options.method;
    for (auto& c : st->method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    st->body = options.body;
    st->headers = options.headers;

    if (!URL::parse(url, st->url)) {
        loop().post([st]() { st->finish(async::make_error_code(async::error::invalid_argument), FetchResponse()); });
        return;
    }

    if (options.timeout.count() > 0) {
        std::weak_ptr<FetchState> weak = st;
        st->timer = set_timeout([weak]() { if (auto s = weak.lock()) s->finish(async::make_error_code(async::error::timed_out), FetchResponse()); }, options.timeout);
    }

    loop().post([st]() { st->start(); });
}

} // namespace http

namespace https {

http::ClientRequest request(const http::RequestOptions& options, std::function<void(http::IncomingMessage)> on_response) {
    http::RequestOptions o = options;
    if (o.protocol != "https:" && o.protocol != "wss:") o.protocol = "https:";
    return http::request(o, std::move(on_response));
}

http::ClientRequest request(const std::string& url, std::function<void(http::IncomingMessage)> on_response) {
    return http::request(url, std::move(on_response));
}

http::ClientRequest get(const std::string& url, std::function<void(http::IncomingMessage)> on_response) {
    return http::get(url, std::move(on_response));
}

http::ClientRequest get(const http::RequestOptions& options, std::function<void(http::IncomingMessage)> on_response) {
    http::RequestOptions o = options;
    if (o.protocol != "https:" && o.protocol != "wss:") o.protocol = "https:";
    return http::get(o, std::move(on_response));
}

} // namespace https
} // namespace networking
} // namespace fizmo
