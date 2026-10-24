#include "fizmo_library.hpp"
#include "event_stream.hpp"
#include "../Node/loop.hpp"

#include <algorithm>
#include <atomic>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::atomic<std::uint64_t> g_stream_ids{0};

std::string one_line(const std::string& s) {
    std::string out;

    for (char c : s) {
        if (c != '\r' && c != '\n') out += c;
    }

    return out;
}

} // namespace

std::string ServerEvent::serialize() const {
    std::string out;
    if (!id.empty()) out += "id: " + one_line(id) + "\n";
    if (!event.empty()) out += "event: " + one_line(event) + "\n";
    if (retry.count() > 0) out += "retry: " + std::to_string(retry.count()) + "\n";
    std::size_t pos = 0;

    for (;;) {
        std::size_t nl = data.find('\n', pos);
        std::string line = data.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out += "data: " + line + "\n";
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }

    out += "\n";
    return out;
}

struct EventStream::State {
    std::mutex                         m;
    http::ServerResponse               res;
    bool                               closed = false;
    Timer                              heartbeat;
    std::string                        last_id;
    std::uint64_t                      id = 0;
    std::vector<std::function<void()>> close_fns;

    bool write(const std::string& text) {
        http::ServerResponse r;

        {
            std::lock_guard<std::mutex> lk(m);
            if (closed) return false;
            r = res;
        }

        if (!r || r.writable_ended()) return false;
        r.write(DataView(text));
        return true;
    }

    void mark_closed() {
        std::vector<std::function<void()>> fns;

        {
            std::lock_guard<std::mutex> lk(m);
            if (closed) return;
            closed = true;
            heartbeat.cancel();
            fns.swap(close_fns);
        }

        for (auto& f : fns) {
            try { f(); } catch (...) {}
        }
    }
};

EventStream EventStream::open(http::IncomingMessage& req, http::ServerResponse& res, std::chrono::milliseconds heartbeat) {
    auto s = std::make_shared<State>();
    s->res = res;
    s->last_id = req.header("last-event-id");
    s->id = ++g_stream_ids;

    res.set_header("Content-Type", "text/event-stream; charset=utf-8");
    res.set_header("Cache-Control", "no-cache, no-transform");
    res.set_header("Connection", "keep-alive");
    res.set_header("X-Accel-Buffering", "no");
    res.write_head(200);
    res.write(DataView(": open\n\n"));

    std::weak_ptr<State> weak = s;
    res.on_close([weak] { if (auto st = weak.lock()) st->mark_closed(); });
    res.on_finish([weak] { if (auto st = weak.lock()) st->mark_closed(); });

    if (heartbeat.count() > 0) {
        s->heartbeat = set_interval([weak] {
            auto st = weak.lock();
            if (st && !st->write(": ping\n\n")) st->mark_closed();
        }, heartbeat);
    }

    return EventStream(s);
}

bool EventStream::send(const ServerEvent& event) {
    if (!m_state) return false;
    return m_state->write(event.serialize());
}

bool EventStream::send(const std::string& event, const std::string& data, const std::string& id) {
    ServerEvent e;
    e.event = event;
    e.data = data;
    e.id = id;
    return send(e);
}

bool EventStream::comment(const std::string& text) {
    if (!m_state) return false;
    return m_state->write(": " + one_line(text) + "\n\n");
}

void EventStream::close() {
    if (!m_state) return;
    http::ServerResponse r;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        r = m_state->res;
    }

    m_state->mark_closed();
    if (r && !r.writable_ended()) r.end();
}

bool EventStream::closed() const {
    if (!m_state) return true;
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->closed;
}

std::string EventStream::last_event_id() const {
    if (!m_state) return {};
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->last_id;
}

std::uint64_t EventStream::id() const noexcept {
    return m_state ? m_state->id : 0;
}

void EventStream::on_close(std::function<void()> fn) {
    if (!m_state || !fn) return;
    bool now = false;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (m_state->closed) now = true;
        else m_state->close_fns.push_back(fn);
    }

    if (now) fn();
}

Broadcaster::Broadcaster(std::size_t replay) : m_shared(std::make_shared<Shared>()) {
    m_shared->replay = replay;
}

void Broadcaster::subscribe(const std::string& topic, EventStream stream) {
    if (!stream) return;
    std::vector<ServerEvent> missed;
    const std::string last = stream.last_event_id();

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        Topic& t = m_shared->topics[topic];
        t.streams.push_back(stream);

        if (!last.empty()) {
            auto it = std::find_if(t.history.begin(), t.history.end(), [&](const ServerEvent& e) { return e.id == last; });
            if (it != t.history.end()) missed.assign(it + 1, t.history.end());
        }
    }

    for (const auto& e : missed) stream.send(e);

    const std::uint64_t sid = stream.id();
    std::weak_ptr<Shared> weak = m_shared;

    stream.on_close([weak, topic, sid] {
        auto sh = weak.lock();
        if (!sh) return;
        std::lock_guard<std::mutex> lk(sh->m);
        const auto it = sh->topics.find(topic);
        if (it == sh->topics.end()) return;
        auto& v = it->second.streams;
        v.erase(std::remove_if(v.begin(), v.end(), [&](const EventStream& s) { return s.id() == sid; }), v.end());
    });
}

void Broadcaster::unsubscribe(const std::string& topic, const EventStream& stream) {
    std::lock_guard<std::mutex> lk(m_shared->m);
    const auto it = m_shared->topics.find(topic);
    if (it == m_shared->topics.end()) return;
    auto& v = it->second.streams;
    const std::uint64_t sid = stream.id();
    v.erase(std::remove_if(v.begin(), v.end(), [&](const EventStream& s) { return s.id() == sid; }), v.end());
}

std::size_t Broadcaster::publish(const std::string& topic, const std::string& event, const std::string& data) {
    std::vector<EventStream> targets;
    ServerEvent e;

    {
        std::lock_guard<std::mutex> lk(m_shared->m);
        Topic& t = m_shared->topics[topic];
        e.id = std::to_string(t.next_id++);
        e.event = event;
        e.data = data;

        if (m_shared->replay) {
            t.history.push_back(e);
            while (t.history.size() > m_shared->replay) t.history.pop_front();
        }

        targets = t.streams;
    }

    std::size_t sent = 0;

    for (auto& s : targets) {
        if (s.send(e)) ++sent;
    }

    return sent;
}

std::size_t Broadcaster::subscribers(const std::string& topic) const {
    std::lock_guard<std::mutex> lk(m_shared->m);
    const auto it = m_shared->topics.find(topic);
    return it == m_shared->topics.end() ? 0 : it->second.streams.size();
}

std::size_t Broadcaster::prune() {
    std::lock_guard<std::mutex> lk(m_shared->m);
    std::size_t removed = 0;

    for (auto& kv : m_shared->topics) {
        auto& v = kv.second.streams;
        const auto before = v.size();
        v.erase(std::remove_if(v.begin(), v.end(), [](const EventStream& s) { return s.closed(); }), v.end());
        removed += before - v.size();
    }

    return removed;
}

std::uint64_t ChangeSignal::version() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_version;
}

std::uint64_t ChangeSignal::notify() {
    std::vector<Listener> listeners;
    std::uint64_t v;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        v = ++m_version;
        for (const auto& kv : m_listeners) listeners.push_back(kv.second);
    }

    m_cv.notify_all();

    for (auto& l : listeners) {
        try { l(v); } catch (...) {}
    }

    return v;
}

bool ChangeSignal::wait_for_change(std::uint64_t seen, std::chrono::milliseconds timeout, std::uint64_t* now_version) const {
    std::unique_lock<std::mutex> lk(m_mutex);
    const bool changed = m_cv.wait_for(lk, timeout, [&] { return m_version != seen; });
    if (now_version) *now_version = m_version;
    return changed;
}

std::uint64_t ChangeSignal::listen(Listener fn) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const std::uint64_t id = m_next_listener++;
    m_listeners.emplace(id, std::move(fn));
    return id;
}

void ChangeSignal::unlisten(std::uint64_t id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_listeners.erase(id);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
