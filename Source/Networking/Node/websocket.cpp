#include "fizmo_library.hpp"
#include "websocket.hpp"
#include "../../Crypto/encoding.hpp"
#include "../../Crypto/hash.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace ws {

namespace {

const char* kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::atomic<std::uint64_t> g_ws_ids{0};

std::error_code err(async::error e) { return async::make_error_code(e); }

bool valid_close_code(std::uint16_t c) noexcept {
    if (c >= 1000 && c <= 1014) return c != 1004 && c != 1005 && c != 1006;
    return c >= 3000 && c <= 4999;
}

Bytes make_frame(std::uint8_t opcode, const std::uint8_t* data, std::size_t len, bool mask) {
    Bytes f;
    f.reserve(len + 14);
    f.push_back(static_cast<std::uint8_t>(0x80 | opcode));
    const std::uint8_t mbit = mask ? 0x80 : 0x00;

    if (len < 126) {
        f.push_back(static_cast<std::uint8_t>(mbit | len));
    } else if (len <= 0xFFFF) {
        f.push_back(static_cast<std::uint8_t>(mbit | 126));
        f.push_back(static_cast<std::uint8_t>(len >> 8));
        f.push_back(static_cast<std::uint8_t>(len));
    } else {
        f.push_back(static_cast<std::uint8_t>(mbit | 127));
        const std::uint64_t l = len;
        for (int i = 7; i >= 0; --i) f.push_back(static_cast<std::uint8_t>(l >> (8 * i)));
    }

    if (!mask) {
        f.insert(f.end(), data, data + len);
        return f;
    }

    std::uint8_t key[4];
    fizmo::random_bytes(key, sizeof(key));
    f.insert(f.end(), key, key + 4);
    const std::size_t off = f.size();
    f.resize(off + len);
    for (std::size_t i = 0; i < len; ++i) f[off + i] = static_cast<std::uint8_t>(data[i] ^ key[i & 3]);
    return f;
}

std::string trim(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split_list(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;

    while (i <= s.size()) {
        std::size_t c = s.find(',', i);
        if (c == std::string::npos) c = s.size();
        std::string t = trim(s.substr(i, c - i));
        if (!t.empty()) out.push_back(t);
        i = c + 1;
    }

    return out;
}

} // namespace

bool is_valid_utf8(const std::uint8_t* s, std::size_t len) noexcept {
    std::size_t i = 0;

    while (i < len) {
        const std::uint8_t c = s[i];

        if (c < 0x80) {
            ++i;
            continue;
        }

        std::size_t n;
        std::uint32_t cp;

        if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; if (c < 0xC2) return false; }
        else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; if (c > 0xF4) return false; }
        else return false;

        if (i + n >= len) return false;

        for (std::size_t k = 1; k <= n; ++k) {
            const std::uint8_t cc = s[i + k];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }

        if (n == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
        if (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
        i += n + 1;
    }

    return true;
}

std::string accept_key(const std::string& client_key) {
    const auto digest = crypto::SHA1::hash(client_key + kGuid);
    return crypto::base64_encode(digest.data(), digest.size());
}

struct WebSocket::State : std::enable_shared_from_this<WebSocket::State> {
    mutable std::mutex           m;
    Events                       ev;
    net::Socket                  sock;
    bool                         server_side = false;
    ReadyState                   rs = ReadyState::Connecting;
    std::string                  protocol;
    std::string                  url;
    std::size_t                  max_payload = 64u << 20;
    bool                         auto_pong = true;
    std::chrono::milliseconds    close_timeout{30000};
    std::uint64_t                id = ++g_ws_ids;
    Bytes                        rbuf;
    bool                         in_message = false;
    std::uint8_t                 msg_opcode = 0;
    Bytes                        msg;
    bool                         close_sent = false;
    bool                         close_received = false;
    bool                         close_emitted = false;
    bool                         failed = false;
    std::uint16_t                code = close_code::Abnormal;
    std::string                  reason;
    Timer                        close_timer;
    Timer                        handshake_timer;
    http::ClientRequest          pending;
    std::any                     user;
    std::function<void(std::uint64_t)> on_closed_hook;

    void attach(net::Socket s, const DataView& head) {
        {
            std::lock_guard<std::mutex> lk(m);
            sock = s;
            rs = ReadyState::Open;
        }

        auto self = shared_from_this();
        s.on_data([self](const DataView& d) { self->on_data(d); });
        s.on_close([self](bool) { self->on_socket_close(); });
        s.on_error([self](const std::error_code& ec) { self->on_socket_error(ec); });
        s.set_no_delay(true);

        if (!head.empty()) {
            auto copy = std::make_shared<Bytes>(head.bytes());
            s.pause();

            loop().post([self, copy, s]() mutable {
                self->on_data(DataView(*copy));
                s.resume();
            });
        }
    }

    void emit_error(const std::error_code& ec) {
        if (ev.error.emit(ec) == 0 && ec != async::error::connection_reset && ec != std::errc::connection_reset) report_unhandled_error("ws::WebSocket", ec);
    }

    bool send_frame(std::uint8_t opcode, const std::uint8_t* data, std::size_t len, std::function<void(const std::error_code&)> done = {}) {
        net::Socket s;

        {
            std::lock_guard<std::mutex> lk(m);
            s = sock;
        }

        if (!s) {
            if (done) loop().post([done]() { done(err(async::error::not_connected)); });
            return false;
        }

        const Bytes f = make_frame(opcode, data, len, !server_side);
        return s.write(DataView(f), std::move(done));
    }

    void send_close_frame(std::uint16_t c, const std::string& r) {
        Bytes payload;

        if (c != close_code::NoStatus) {
            payload.push_back(static_cast<std::uint8_t>(c >> 8));
            payload.push_back(static_cast<std::uint8_t>(c));
            payload.insert(payload.end(), r.begin(), r.end());
        }

        send_frame(0x8, payload.data(), payload.size());
    }

    void arm_close_timer() {
        std::weak_ptr<State> weak = shared_from_this();
        auto t = set_timeout([weak]() { if (auto s = weak.lock()) { if (s->sock) s->sock.destroy(); } }, close_timeout);
        std::lock_guard<std::mutex> lk(m);
        close_timer.cancel();
        close_timer = t;
    }

    void fail(std::uint16_t ws_code, const std::error_code& ec) {
        bool send;

        {
            std::lock_guard<std::mutex> lk(m);
            if (failed) return;
            failed = true;
            send = !close_sent;
            close_sent = true;
            if (!close_received) code = ws_code;
            rs = ReadyState::Closing;
        }

        emit_error(ec);
        if (send) send_close_frame(ws_code, {});
        sock.end();
        arm_close_timer();
    }

    void on_data(const DataView& d) {
        {
            std::lock_guard<std::mutex> lk(m);
            if (failed || rs == ReadyState::Closed) return;
        }

        rbuf.insert(rbuf.end(), d.begin(), d.end());
        parse();
    }

    void parse() {
        std::size_t pos = 0;

        while (true) {
            const std::size_t avail = rbuf.size() - pos;
            if (avail < 2) break;
            const std::uint8_t* p = rbuf.data() + pos;
            const bool fin = (p[0] & 0x80) != 0;
            const std::uint8_t rsv = p[0] & 0x70;
            const std::uint8_t opcode = p[0] & 0x0F;
            const bool masked = (p[1] & 0x80) != 0;
            std::uint64_t len = p[1] & 0x7F;
            std::size_t header = 2;

            if (rsv) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }

            if (len == 126) {
                if (avail < 4) break;
                len = (std::uint64_t(p[2]) << 8) | p[3];
                header = 4;
                if (len < 126) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }
            } else if (len == 127) {
                if (avail < 10) break;
                len = 0;
                for (int i = 0; i < 8; ++i) len = (len << 8) | p[2 + i];
                header = 10;
                if ((len >> 63) != 0 || len < 65536) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }
            }

            if (server_side != masked) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }

            const bool control = (opcode & 0x8) != 0;

            if (control) {
                if (!fin || len > 125 || (opcode != 0x8 && opcode != 0x9 && opcode != 0xA)) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }
            } else {
                if (opcode > 0x2) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return; }
                if (len > max_payload || (opcode == 0x0 && msg.size() + len > max_payload)) { fail(close_code::MessageTooBig, err(async::error::limit_exceeded)); return; }
            }

            const std::size_t mask_len = masked ? 4 : 0;
            if (avail < header + mask_len + len) break;
            const std::uint8_t* key = p + header;
            const std::uint8_t* src = p + header + mask_len;
            Bytes payload(src, src + static_cast<std::size_t>(len));
            if (masked) for (std::size_t i = 0; i < payload.size(); ++i) payload[i] ^= key[i & 3];
            pos += header + mask_len + static_cast<std::size_t>(len);
            if (!handle_frame(fin, opcode, std::move(payload))) { rbuf.clear(); return; }
        }

        rbuf.erase(rbuf.begin(), rbuf.begin() + static_cast<std::ptrdiff_t>(pos));
    }

    bool handle_frame(bool fin, std::uint8_t opcode, Bytes payload) {
        {
            std::lock_guard<std::mutex> lk(m);
            if (failed || close_received) return false;
        }

        switch (opcode) {
            case 0x0:
                if (!in_message) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return false; }
                msg.insert(msg.end(), payload.begin(), payload.end());
                if (fin) return deliver();
                return true;

            case 0x1:
            case 0x2:
                if (in_message) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return false; }
                msg_opcode = opcode;
                msg.swap(payload);
                in_message = !fin;
                if (fin) return deliver();
                return true;

            case 0x9: {
                bool open;
                { std::lock_guard<std::mutex> lk(m); open = rs == ReadyState::Open; }
                if (auto_pong && open) send_frame(0xA, payload.data(), payload.size());
                ev.ping.emit(DataView(payload));
                return true;
            }

            case 0xA:
                ev.pong.emit(DataView(payload));
                return true;

            case 0x8:
                return handle_close(payload);

            default:
                fail(close_code::ProtocolError, err(async::error::protocol_error));
                return false;
        }
    }

    bool deliver() {
        in_message = false;
        Bytes m_out;
        m_out.swap(msg);
        const bool binary = msg_opcode == 0x2;

        if (!binary && !is_valid_utf8(m_out.data(), m_out.size())) {
            fail(close_code::InvalidPayload, err(async::error::protocol_error));
            return false;
        }

        ev.message.emit(DataView(m_out), binary);
        return true;
    }

    bool handle_close(const Bytes& payload) {
        std::uint16_t c = close_code::NoStatus;
        std::string r;

        if (payload.size() == 1) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return false; }

        if (payload.size() >= 2) {
            c = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
            if (!valid_close_code(c)) { fail(close_code::ProtocolError, err(async::error::protocol_error)); return false; }
            if (!is_valid_utf8(payload.data() + 2, payload.size() - 2)) { fail(close_code::InvalidPayload, err(async::error::protocol_error)); return false; }
            r.assign(reinterpret_cast<const char*>(payload.data() + 2), payload.size() - 2);
        }

        bool reply;

        {
            std::lock_guard<std::mutex> lk(m);
            close_received = true;
            code = c;
            reason = r;
            reply = !close_sent;
            close_sent = true;
            rs = ReadyState::Closing;
        }

        if (reply) send_close_frame(c == close_code::NoStatus ? close_code::NoStatus : c, {});

        if (server_side) sock.end();
        else arm_close_timer();
        return false;
    }

    void on_socket_error(const std::error_code& ec) {
        bool quiet;

        {
            std::lock_guard<std::mutex> lk(m);
            quiet = close_received || failed;
        }

        if (!quiet) emit_error(ec);
    }

    void on_socket_close() {
        finish();
    }

    void finish() {
        std::uint16_t c;
        std::string r;
        std::function<void(std::uint64_t)> hook;

        {
            std::lock_guard<std::mutex> lk(m);
            if (close_emitted) return;
            close_emitted = true;
            rs = ReadyState::Closed;
            c = code;
            r = reason;
            hook = std::move(on_closed_hook);
            on_closed_hook = nullptr;
            close_timer.cancel();
            handshake_timer.cancel();
        }

        ev.close.emit(c, r);
        if (hook) hook(id);
        ev.open.clear();
        ev.message.clear();
        ev.close.clear();
        ev.error.clear();
        ev.ping.clear();
        ev.pong.clear();
    }
};

WebSocket& WebSocket::on_open(std::function<void()> fn) { m_state->ev.open.add(std::move(fn)); return *this; }
WebSocket& WebSocket::on_message(std::function<void(const DataView&, bool)> fn) { m_state->ev.message.add(std::move(fn)); return *this; }

WebSocket& WebSocket::on_text(std::function<void(const std::string&)> fn) {
    if (!fn) return *this;
    m_state->ev.message.add([fn](const DataView& d, bool binary) { if (!binary) fn(d.str()); });
    return *this;
}

WebSocket& WebSocket::on_binary(std::function<void(const DataView&)> fn) {
    if (!fn) return *this;
    m_state->ev.message.add([fn](const DataView& d, bool binary) { if (binary) fn(d); });
    return *this;
}

WebSocket& WebSocket::on_close(std::function<void(std::uint16_t, const std::string&)> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
WebSocket& WebSocket::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
WebSocket& WebSocket::on_ping(std::function<void(const DataView&)> fn) { m_state->ev.ping.add(std::move(fn)); return *this; }
WebSocket& WebSocket::on_pong(std::function<void(const DataView&)> fn) { m_state->ev.pong.add(std::move(fn)); return *this; }
WebSocket::Events& WebSocket::events() { return m_state->ev; }

bool WebSocket::send(const std::string& text, std::function<void(const std::error_code&)> done) {
    return send(DataView(text), false, std::move(done));
}

bool WebSocket::send(const DataView& data, bool binary, std::function<void(const std::error_code&)> done) {
    {
        std::lock_guard<std::mutex> lk(m_state->m);

        if (m_state->rs != ReadyState::Open) {
            const auto ec = err(m_state->rs == ReadyState::Connecting ? async::error::not_connected : async::error::closed);
            if (done) loop().post([done, ec]() { done(ec); });
            return false;
        }
    }

    return m_state->send_frame(binary ? 0x2 : 0x1, data.data(), data.size(), std::move(done));
}

bool WebSocket::ping(const DataView& data) {
    if (data.size() > 125 || ready_state() != ReadyState::Open) return false;
    return m_state->send_frame(0x9, data.data(), data.size());
}

bool WebSocket::pong(const DataView& data) {
    if (data.size() > 125 || ready_state() != ReadyState::Open) return false;
    return m_state->send_frame(0xA, data.data(), data.size());
}

WebSocket& WebSocket::close(std::uint16_t code, const std::string& reason) {
    auto st = m_state;
    std::string r = reason.size() > 123 ? reason.substr(0, 123) : reason;
    if (!valid_close_code(code)) code = close_code::Normal;
    if (!is_valid_utf8(reinterpret_cast<const std::uint8_t*>(r.data()), r.size())) r.clear();
    bool abort_handshake = false;
    bool already_received = false;

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->rs == ReadyState::Closed || st->close_sent) return *this;

        if (st->rs == ReadyState::Connecting) {
            abort_handshake = true;
            st->code = close_code::Abnormal;
        } else {
            st->close_sent = true;
            st->rs = ReadyState::Closing;
            st->code = code;
            st->reason = r;
            already_received = st->close_received;
        }
    }

    if (abort_handshake) {
        http::ClientRequest req = st->pending;
        if (req) req.destroy();
        loop().post([st]() { st->finish(); });
        return *this;
    }

    st->send_close_frame(code, r);
    if (already_received && st->server_side) st->sock.end();
    else st->arm_close_timer();
    return *this;
}

WebSocket& WebSocket::terminate() {
    net::Socket s;
    http::ClientRequest req;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        s = m_state->sock;
        req = m_state->pending;
    }

    if (s) s.destroy();
    else if (req) req.destroy();

    if (!s) {
        auto st = m_state;
        loop().post([st]() { st->finish(); });
    }

    return *this;
}

WebSocket& WebSocket::pause() { auto s = socket(); if (s) s.pause(); return *this; }
WebSocket& WebSocket::resume() { auto s = socket(); if (s) s.resume(); return *this; }

ReadyState WebSocket::ready_state() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->rs;
}

std::string WebSocket::protocol() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->protocol;
}

std::string WebSocket::url() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->url;
}

bool WebSocket::is_server() const noexcept { return m_state->server_side; }
std::size_t WebSocket::buffered_amount() const noexcept { auto s = socket(); return s ? s.buffered_amount() : 0; }
std::string WebSocket::remote_address() const { auto s = socket(); return s ? s.remote_address() : std::string(); }
std::uint16_t WebSocket::remote_port() const { auto s = socket(); return s ? s.remote_port() : 0; }

net::Socket WebSocket::socket() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->sock;
}

std::uint64_t WebSocket::id() const noexcept { return m_state->id; }
std::any& WebSocket::data() { return m_state->user; }

WebSocket connect(const std::string& url, std::function<void()> on_open) {
    return connect(url, ClientOptions(), std::move(on_open));
}

WebSocket connect(const std::string& url, const ClientOptions& options, std::function<void()> on_open) {
    auto st = std::make_shared<WebSocket::State>();
    st->server_side = false;
    st->url = url;
    st->max_payload = options.max_payload;
    st->auto_pong = options.auto_pong;
    st->close_timeout = options.close_timeout;
    WebSocket ws(st);
    if (on_open) ws.on_open(std::move(on_open));
    URL u;

    if (!URL::parse(url, u) || (u.scheme() != "ws" && u.scheme() != "wss" && u.scheme() != "http" && u.scheme() != "https")) {
        loop().post([st]() {
            st->emit_error(err(async::error::invalid_argument));
            st->finish();
        });

        return ws;
    }

    std::uint8_t raw[16];
    fizmo::random_bytes(raw, sizeof(raw));
    const std::string key = crypto::base64_encode(raw, sizeof(raw));
    const std::string expected = accept_key(key);

    http::RequestOptions ro = http::RequestOptions::from_url(u);
    ro.protocol = u.secure() ? "https:" : "http:";
    ro.method = "GET";
    ro.secure = options.secure;
    ro.connect_timeout = options.handshake_timeout;
    for (auto& h : options.headers) ro.headers.append(h.first, h.second);
    ro.headers.set("Upgrade", "websocket");
    ro.headers.set("Connection", "Upgrade");
    ro.headers.set("Sec-WebSocket-Key", key);
    ro.headers.set("Sec-WebSocket-Version", "13");

    if (!options.protocols.empty()) {
        std::string joined;
        for (auto& p : options.protocols) { if (!joined.empty()) joined += ", "; joined += p; }
        ro.headers.set("Sec-WebSocket-Protocol", joined);
    }

    if (!options.origin.empty()) ro.headers.set("Origin", options.origin);
    const std::vector<std::string> offered = options.protocols;
    std::weak_ptr<WebSocket::State> weak = st;
    http::ClientRequest req = http::request(ro);

    req.on_upgrade([st, expected, offered](http::IncomingMessage res, net::Socket sock, const DataView& head) {
        auto s = st;
        const bool upgrade_ok = res.headers().has_token("upgrade", "websocket") && res.headers().has_token("connection", "upgrade");
        const bool accept_ok = res.header("sec-websocket-accept") == expected;
        const std::string proto = res.header("sec-websocket-protocol");
        bool proto_ok = proto.empty();
        for (auto& p : offered) if (p == proto) proto_ok = true;
        const bool ext_ok = res.header("sec-websocket-extensions").empty();

        if (!upgrade_ok || !accept_ok || !proto_ok || !ext_ok) {
            sock.destroy();
            s->emit_error(err(async::error::handshake_failed));
            s->finish();
            return;
        }

        {
            std::lock_guard<std::mutex> lk(s->m);
            s->protocol = proto;
            s->pending = http::ClientRequest();
            s->handshake_timer.cancel();
        }

        s->attach(sock, head);
        s->ev.open.emit();
    });

    req.on_response([st](http::IncomingMessage res) {
        res.discard();
        auto s = st;
        s->emit_error(err(async::error::handshake_failed));
        auto sk = res.socket();
        if (sk) sk.destroy();
        s->finish();
    });

    req.on_error([st](const std::error_code& ec) {
        auto s = st;
        s->emit_error(ec);
        s->finish();
    });

    {
        std::lock_guard<std::mutex> lk(st->m);
        st->pending = req;
    }

    if (options.handshake_timeout.count() > 0) {
        st->handshake_timer = set_timeout([weak]() {
            auto s = weak.lock();
            if (!s) return;
            http::ClientRequest r;

            {
                std::lock_guard<std::mutex> lk(s->m);
                if (s->rs != ReadyState::Connecting) return;
                r = s->pending;
            }

            if (r) r.destroy();
            s->emit_error(err(async::error::timed_out));
            s->finish();
        }, options.handshake_timeout);
    }

    req.end();
    return ws;
}

struct Server::State : std::enable_shared_from_this<Server::State> {
    mutable std::mutex                           m;
    Events                                       ev;
    ServerOptions                                opt;
    http::Server                                 http;
    bool                                         own_http = false;
    bool                                         closing = false;
    std::unordered_map<std::uint64_t, WebSocket> clients;

    void abort(net::Socket sock, int code, const std::string& message, const http::Headers& extra = http::Headers()) {
        const std::string body = message.empty() ? std::string(http::status_text(code)) : message;
        std::string out = "HTTP/1.1 " + std::to_string(code) + " " + http::status_text(code) + "\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
        out += extra.serialize();
        out += "\r\n" + body;
        sock.end(DataView(out));
    }
};

bool Server::should_handle(const http::IncomingMessage& request) const {
    if (m_state->opt.path.empty()) return true;
    return request.path() == m_state->opt.path;
}

void Server::handle_upgrade(http::IncomingMessage req, net::Socket sock, const DataView& head, std::function<void(WebSocket)> on_ready) {
    auto st = m_state;
    const http::Headers& h = req.headers();
    const std::string key = h.get("sec-websocket-key");
    std::vector<std::uint8_t> decoded;

    if (req.method() != "GET" || req.http_version() != "1.1") { st->abort(sock, 405, "WebSocket handshakes must use GET over HTTP/1.1"); return; }
    if (!h.has_token("upgrade", "websocket")) { st->abort(sock, 400, "Missing or invalid Upgrade header"); return; }
    if (!h.has_token("connection", "upgrade")) { st->abort(sock, 400, "Missing or invalid Connection header"); return; }
    if (key.empty() || !crypto::base64_decode(key, decoded) || decoded.size() != 16) { st->abort(sock, 400, "Missing or invalid Sec-WebSocket-Key header"); return; }

    if (trim(h.get("sec-websocket-version")) != "13") {
        http::Headers extra;
        extra.set("Sec-WebSocket-Version", "13");
        st->abort(sock, 426, "Unsupported WebSocket version", extra);
        return;
    }

    if (!should_handle(req)) { st->abort(sock, 400, "Invalid WebSocket path"); return; }
    if (st->opt.verify_client && !st->opt.verify_client(req)) { st->abort(sock, 401, {}); return; }

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->closing) { st->abort(sock, 503, "Server is closing"); return; }
    }

    const std::vector<std::string> offered = split_list(h.get("sec-websocket-protocol"));
    std::string chosen;

    if (!offered.empty()) {
        if (st->opt.handle_protocols) {
            chosen = st->opt.handle_protocols(offered, req);
            bool ok = chosen.empty();
            for (auto& p : offered) if (p == chosen) ok = true;
            if (!ok) chosen.clear();
        } else {
            chosen = offered.front();
        }
    }

    std::string resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept_key(key) + "\r\n";
    if (!chosen.empty()) resp += "Sec-WebSocket-Protocol: " + chosen + "\r\n";
    resp += "\r\n";
    sock.write(DataView(resp));

    auto ws_state = std::make_shared<WebSocket::State>();
    ws_state->server_side = true;
    ws_state->protocol = chosen;
    ws_state->url = req.url();
    ws_state->max_payload = st->opt.max_payload;
    ws_state->auto_pong = st->opt.auto_pong;
    ws_state->close_timeout = st->opt.close_timeout;
    WebSocket ws(ws_state);

    if (st->opt.client_tracking) {
        std::weak_ptr<State> weak = st;

        {
            std::lock_guard<std::mutex> lk(st->m);
            st->clients[ws_state->id] = ws;
        }

        ws_state->on_closed_hook = [weak](std::uint64_t id) {
            auto s = weak.lock();
            if (!s) return;
            std::lock_guard<std::mutex> lk(s->m);
            s->clients.erase(id);
        };
    }

    ws_state->attach(sock, head);
    if (on_ready) on_ready(ws);
}

namespace {

void wire_http(const std::shared_ptr<Server::State>& st, http::Server srv, bool standalone) {
    std::weak_ptr<http::Server::State> weak_http = srv.state();

    srv.on_upgrade([st, standalone, weak_http](http::IncomingMessage req, net::Socket sock, const DataView& head) mutable {
        auto s = st;
        std::weak_ptr<Server::State> weak = st;
        Server server(s);

        if (!server.should_handle(req)) {
            auto hs = weak_http.lock();
            if (standalone || !hs || http::Server(hs).events().upgrade.count() <= 1) s->abort(sock, 400, "Invalid WebSocket path");
            return;
        }

        server.handle_upgrade(req, sock, head, [weak, req](WebSocket ws) {
            if (auto s2 = weak.lock()) s2->ev.connection.emit(ws, req);
        });
    });
}

} // namespace

Server& Server::listen(std::uint16_t port, std::function<void()> on_listening) {
    return listen(port, std::string(), std::move(on_listening));
}

Server& Server::listen(std::uint16_t port, const std::string& host, std::function<void()> on_listening) {
    auto st = m_state;
    if (on_listening) st->ev.listening.once(std::move(on_listening));
    http::Server srv;

    {
        std::lock_guard<std::mutex> lk(st->m);

        if (!st->http) {
            st->http = http::create_server(st->opt.http, [](http::IncomingMessage req, http::ServerResponse res) {
                req.discard();
                res.set_header("Upgrade", "websocket").send(426, "Upgrade Required");
            });

            st->own_http = true;
            std::weak_ptr<State> weak = st;
            st->http.on_listening([weak]() { if (auto s = weak.lock()) s->ev.listening.emit(); });
            st->http.on_error([weak](const std::error_code& ec) { auto s = weak.lock(); if (s && s->ev.error.emit(ec) == 0) report_unhandled_error("ws::Server", ec); });
            wire_http(st, st->http, true);
        }

        srv = st->http;
    }

    srv.listen(port, host);
    return *this;
}

Server& Server::attach(http::Server server) {
    {
        std::lock_guard<std::mutex> lk(m_state->m);
        m_state->http = server;
        m_state->own_http = false;
    }

    wire_http(m_state, server, false);
    return *this;
}

Server& Server::close(std::function<void()> on_closed) {
    auto st = m_state;
    std::vector<WebSocket> list;
    bool own;
    http::Server srv;

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->closing) return *this;
        st->closing = true;
        for (auto& kv : st->clients) list.push_back(kv.second);
        own = st->own_http;
        srv = st->http;
    }

    for (auto& c : list) c.close(close_code::GoingAway, "Server closing");

    auto done = [st, on_closed]() {
        st->ev.close.emit();
        if (on_closed) on_closed();
        std::lock_guard<std::mutex> lk(st->m);
        st->http = http::Server();
    };

    if (own && srv) srv.close([done](const std::error_code&) { done(); });
    else loop().post(done);
    return *this;
}

Server& Server::on_connection(std::function<void(WebSocket, http::IncomingMessage)> fn) { m_state->ev.connection.add(std::move(fn)); return *this; }
Server& Server::on_listening(std::function<void()> fn) { m_state->ev.listening.add(std::move(fn)); return *this; }
Server& Server::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
Server& Server::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
Server::Events& Server::events() { return m_state->ev; }

std::vector<WebSocket> Server::clients() const {
    std::vector<WebSocket> out;
    std::lock_guard<std::mutex> lk(m_state->m);
    for (auto& kv : m_state->clients) out.push_back(kv.second);
    return out;
}

std::size_t Server::client_count() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->clients.size();
}

std::size_t Server::broadcast(const std::string& text, const WebSocket& except) {
    return broadcast(DataView(text), false, except);
}

std::size_t Server::broadcast(const DataView& data, bool binary, const WebSocket& except) {
    std::size_t n = 0;

    for (auto& c : clients()) {
        if (c == except || !c.is_open()) continue;
        if (c.send(data, binary)) ++n;
        else if (c.is_open()) ++n;
    }

    return n;
}

std::uint16_t Server::port() const { auto s = http_server(); return s ? s.port() : 0; }
net::AddressInfo Server::address() const { auto s = http_server(); return s ? s.address() : net::AddressInfo(); }

http::Server Server::http_server() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->http;
}

Server create_server(const ServerOptions& options, std::function<void(WebSocket, http::IncomingMessage)> on_connection) {
    auto st = std::make_shared<Server::State>();
    st->opt = options;
    Server s(st);
    if (on_connection) s.on_connection(std::move(on_connection));
    return s;
}

} // namespace ws
} // namespace networking
} // namespace fizmo
