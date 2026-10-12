#include "fizmo_library.hpp"
#include "http_internal.hpp"

#include <sstream>

namespace fizmo {
namespace networking {
namespace http {

namespace {

std::string tls_fingerprint(const tls::SecureContextOptions& s) {
    std::ostringstream os;
    os << s.cert.size() << ':' << std::hash<std::string>()(s.cert) << '|' << std::hash<std::string>()(s.key) << '|' << s.cert_file << '|' << s.key_file << '|' << s.pfx_file << '|'
       << s.ca_file << '|' << s.ciphers << '|' << static_cast<std::uint32_t>(s.min_version) << '|' << static_cast<std::uint32_t>(s.max_version) << '|'
       << s.reject_unauthorized << s.check_revocation << s.session_tickets << '|';
    for (auto& c : s.crl_files) os << c << ',';
    os << '|';
    for (auto& a : s.alpn_protocols) os << a << ',';
    return os.str();
}

} // namespace

std::shared_ptr<security::TLSContext> Agent::State::tls_context(const RequestOptions& o, std::string& key, std::string& error) {
    if (o.secure_context) {
        std::ostringstream os;
        os << "ctx@" << static_cast<const void*>(o.secure_context.get());
        key = os.str();
        return o.secure_context;
    }

    key = tls_fingerprint(o.secure);

    {
        std::lock_guard<std::mutex> lk(m);
        auto it = tls_cache.find(key);
        if (it != tls_cache.end()) return it->second;
    }

    auto ctx = tls::create_secure_context(o.secure, false, &error);
    if (!ctx) return nullptr;
    std::lock_guard<std::mutex> lk(m);
    auto it = tls_cache.find(key);
    if (it != tls_cache.end()) return it->second;
    tls_cache[key] = ctx;
    return ctx;
}

void Agent::State::detach(Idle& idle) {
    if (!idle.socket) return;
    auto& ev = idle.socket.events();
    ev.close.remove(idle.close_id);
    ev.data.remove(idle.data_id);
    ev.error.remove(idle.error_id);
    ev.timeout.remove(idle.timeout_id);
}

std::uint64_t Agent::State::acquire(const Spec& spec, Ready ready, bool force_new) {
    std::vector<Idle> stale;
    Idle chosen;
    bool have = false;
    bool create = false;
    std::uint64_t waiter = 0;

    {
        std::lock_guard<std::mutex> lk(m);
        Pool& p = pools[spec.key];

        const auto now = std::chrono::steady_clock::now();

        for (auto it = p.free.begin(); it != p.free.end();) {
            if (it->expires <= now) { stale.push_back(*it); it = p.free.erase(it); }
            else ++it;
        }

        while (!force_new && !p.free.empty()) {
            Idle i = p.free.back();
            p.free.pop_back();
            if (i.socket.destroyed() || !i.socket.writable()) { stale.push_back(i); continue; }
            chosen = i;
            have = true;
            break;
        }

        if (have) {
            ++p.active;
        } else if (opt.max_sockets == 0 || p.active + p.free.size() < opt.max_sockets) {
            ++p.active;
            create = true;
        } else {
            waiter = ++next_waiter;
            p.waiting.push_back(Waiter{ waiter, spec, ready });
        }
    }

    for (auto& s : stale) {
        detach(s);
        s.socket.destroy();
    }

    if (have) {
        detach(chosen);
        chosen.socket.ref();
        net::Socket s = chosen.socket;
        loop().post([ready, s]() { ready(s, true); });
        return 0;
    }

    if (create) {
        net::Socket s = net::detail::make_client(spec.connect, spec.tls, spec.servername);
        loop().post([ready, s]() { ready(s, false); });
        return 0;
    }

    return waiter;
}

void Agent::State::release(const std::string& key, net::Socket socket, std::chrono::milliseconds idle_hint) {
    Waiter w;
    bool give = false;

    {
        std::lock_guard<std::mutex> lk(m);
        Pool& p = pools[key];
        if (p.active) --p.active;

        if (!p.waiting.empty() && !socket.destroyed()) {
            w = std::move(p.waiting.front());
            p.waiting.pop_front();
            ++p.active;
            give = true;
        }
    }

    if (give) {
        auto ready = w.ready;
        loop().post([ready, socket]() { ready(socket, true); });
        return;
    }

    bool full;

    {
        std::lock_guard<std::mutex> lk(m);
        full = !opt.keep_alive || pools[key].free.size() >= opt.max_free_sockets;
    }

    if (full || socket.destroyed()) {
        socket.end();
        serve(key);
        return;
    }

    Idle idle;
    idle.socket = socket;
    std::weak_ptr<State> weak = shared_from_this();
    std::weak_ptr<net::Socket::State> weak_sock = socket.state();
    const std::uint64_t id = socket.id();
    idle.close_id = socket.events().close.add([weak, key, id](bool) { if (auto s = weak.lock()) s->remove_idle(key, id); });
    idle.data_id = socket.events().data.add([weak_sock](const DataView&) { if (auto s = weak_sock.lock()) net::Socket(s).destroy(); });
    idle.error_id = socket.events().error.add([](const std::error_code&) {});
    std::chrono::milliseconds t = opt.free_socket_timeout;
    if (idle_hint.count() > 0 && idle_hint < t) t = idle_hint;
    idle.expires = std::chrono::steady_clock::now() + t;
    socket.unref();

    {
        std::lock_guard<std::mutex> lk(m);
        pools[key].free.push_back(idle);
    }

    serve(key);
}

void Agent::State::forget(const std::string& key) {
    {
        std::lock_guard<std::mutex> lk(m);
        Pool& p = pools[key];
        if (p.active) --p.active;
    }

    serve(key);
}

void Agent::State::serve(const std::string& key) {
    Waiter w;

    {
        std::lock_guard<std::mutex> lk(m);
        auto it = pools.find(key);
        if (it == pools.end() || it->second.waiting.empty()) return;
        Pool& p = it->second;
        const bool capacity = opt.max_sockets == 0 || p.active + p.free.size() < opt.max_sockets;
        if (!capacity && p.free.empty()) return;
        w = std::move(p.waiting.front());
        p.waiting.pop_front();
    }

    acquire(w.spec, w.ready, false);
}

void Agent::State::cancel(const std::string& key, std::uint64_t waiter) {
    std::lock_guard<std::mutex> lk(m);
    auto it = pools.find(key);
    if (it == pools.end()) return;
    auto& q = it->second.waiting;

    for (auto w = q.begin(); w != q.end(); ++w) {
        if (w->id != waiter) continue;
        q.erase(w);
        return;
    }
}

void Agent::State::remove_idle(const std::string& key, std::uint64_t socket_id) {
    std::lock_guard<std::mutex> lk(m);
    auto it = pools.find(key);
    if (it == pools.end()) return;
    auto& f = it->second.free;

    for (auto i = f.begin(); i != f.end(); ++i) {
        if (i->socket.id() != socket_id) continue;
        f.erase(i);
        return;
    }
}

Agent::Agent(const AgentOptions& options) : m_state(std::make_shared<State>()) {
    m_state->opt = options;
}

Agent Agent::global() {
    static Agent g{ AgentOptions() };
    return g;
}

AgentOptions Agent::options() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->opt;
}

std::size_t Agent::free_socket_count() const {
    std::size_t n = 0;
    std::lock_guard<std::mutex> lk(m_state->m);
    for (auto& kv : m_state->pools) n += kv.second.free.size();
    return n;
}

std::size_t Agent::active_socket_count() const {
    std::size_t n = 0;
    std::lock_guard<std::mutex> lk(m_state->m);
    for (auto& kv : m_state->pools) n += kv.second.active;
    return n;
}

std::size_t Agent::pending_request_count() const {
    std::size_t n = 0;
    std::lock_guard<std::mutex> lk(m_state->m);
    for (auto& kv : m_state->pools) n += kv.second.waiting.size();
    return n;
}

void Agent::close_idle_sockets() {
    std::vector<State::Idle> idle;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        for (auto& kv : m_state->pools) {
            for (auto& i : kv.second.free) idle.push_back(i);
            kv.second.free.clear();
        }
    }

    for (auto& i : idle) {
        State::detach(i);
        i.socket.destroy();
    }
}

void Agent::destroy() {
    close_idle_sockets();
    std::vector<State::Waiter> waiters;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        for (auto& kv : m_state->pools) {
            for (auto& w : kv.second.waiting) waiters.push_back(std::move(w));
            kv.second.active += kv.second.waiting.size();
            kv.second.waiting.clear();
        }
    }

    for (auto& w : waiters) {
        net::Socket failed = net::detail::make_failed(async::make_error_code(async::error::operation_aborted), "Agent destroyed", false);
        auto ready = w.ready;
        loop().post([ready, failed]() { ready(failed, false); });
    }
}

} // namespace http
} // namespace networking
} // namespace fizmo
