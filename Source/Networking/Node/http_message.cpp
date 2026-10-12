#include "fizmo_library.hpp"
#include "http_internal.hpp"

#include <cstdio>
#include <ctime>

namespace fizmo {
namespace networking {
namespace http {

std::string http_date() {
    static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT", days[tm.tm_wday % 7], tm.tm_mday, months[tm.tm_mon % 12], tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

void IncomingMessage::State::push_body(const std::uint8_t* data, std::size_t n) {
    if (n == 0) return;
    bool emit_now = false;
    bool pause = false;

    {
        std::lock_guard<std::mutex> lk(m);

        if (ev.data.empty() || !backlog.empty() || flush_scheduled) {
            backlog.emplace_back(data, data + n);
            backlog_bytes += n;

            if (backlog_bytes > backlog_limit && !paused_for_backlog) {
                paused_for_backlog = true;
                pause = true;
            }
        } else {
            emit_now = true;
        }
    }

    if (pause && socket) socket.pause();
    if (emit_now) ev.data.emit(DataView(data, n));
}

void IncomingMessage::State::mark_complete() {
    bool emit = false;

    {
        std::lock_guard<std::mutex> lk(m);
        if (complete) return;
        complete = true;

        if (!backlog.empty() || flush_scheduled) {
            end_pending = true;
        } else {
            end_emitted = true;
            emit = true;
        }
    }

    if (emit) ev.end.emit();
}

void IncomingMessage::State::mark_aborted(const std::error_code& ec) {
    {
        std::lock_guard<std::mutex> lk(m);
        if (complete || aborted) return;
        aborted = true;
    }

    ev.aborted.emit();
    ev.error.emit(ec);
}

void IncomingMessage::State::schedule_flush() {
    {
        std::lock_guard<std::mutex> lk(m);
        if (flush_scheduled || (backlog.empty() && !end_pending)) return;
        flush_scheduled = true;
    }

    auto self = shared_from_this();
    loop().post([self]() { self->flush(); });
}

void IncomingMessage::State::flush() {
    while (true) {
        std::vector<Bytes> chunks;
        bool resume = false;
        bool end = false;

        {
            std::lock_guard<std::mutex> lk(m);

            if (backlog.empty()) {
                flush_scheduled = false;
                backlog_bytes = 0;
                resume = paused_for_backlog;
                paused_for_backlog = false;

                if (end_pending) {
                    end_pending = false;
                    end_emitted = true;
                    end = true;
                }
            } else {
                chunks.swap(backlog);
                backlog_bytes = 0;
            }
        }

        if (chunks.empty()) {
            if (resume && socket) socket.resume();
            if (end) ev.end.emit();
            return;
        }

        for (auto& c : chunks) ev.data.emit(DataView(c));
    }
}

void IncomingMessage::State::emit_close() {
    {
        std::lock_guard<std::mutex> lk(m);
        if (close_emitted) return;
        close_emitted = true;
    }

    ev.close.emit();
    clear_events();
}

void IncomingMessage::State::clear_events() {
    ev.data.clear();
    ev.end.clear();
    ev.close.clear();
    ev.aborted.clear();
    ev.error.clear();
}

const std::string& IncomingMessage::method() const { return m_state->method; }
const std::string& IncomingMessage::url() const { return m_state->url; }
const std::string& IncomingMessage::http_version() const { return m_state->version; }
int IncomingMessage::status_code() const { return m_state->status; }
const std::string& IncomingMessage::status_message() const { return m_state->reason; }
const Headers& IncomingMessage::headers() const { return m_state->headers; }
const Headers& IncomingMessage::trailers() const { return m_state->trailers; }
std::string IncomingMessage::header(const std::string& name, const std::string& fallback) const { return m_state->headers.get(name, fallback); }

std::string IncomingMessage::path() const {
    const std::string& u = m_state->url;

    if (!u.empty() && u[0] != '/' && u != "*") {
        URL parsed;
        if (URL::parse(u, parsed)) return parsed.pathname();
    }

    const std::size_t q = u.find_first_of("?#");
    return q == std::string::npos ? u : u.substr(0, q);
}

URLSearchParams IncomingMessage::query() const {
    const std::string& u = m_state->url;
    const std::size_t q = u.find('?');
    if (q == std::string::npos) return URLSearchParams();
    const std::size_t h = u.find('#', q);
    return URLSearchParams(u.substr(q + 1, h == std::string::npos ? std::string::npos : h - q - 1));
}

bool IncomingMessage::complete() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->complete;
}

bool IncomingMessage::aborted() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->aborted;
}

bool IncomingMessage::keep_alive() const noexcept { return m_state->keep_alive; }

net::Socket IncomingMessage::socket() const { return m_state->socket; }

IncomingMessage& IncomingMessage::on_data(std::function<void(const DataView&)> fn) {
    m_state->ev.data.add(std::move(fn));
    m_state->schedule_flush();
    return *this;
}

IncomingMessage& IncomingMessage::on_end(std::function<void()> fn) { m_state->ev.end.add(std::move(fn)); return *this; }
IncomingMessage& IncomingMessage::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
IncomingMessage& IncomingMessage::on_aborted(std::function<void()> fn) { m_state->ev.aborted.add(std::move(fn)); return *this; }
IncomingMessage& IncomingMessage::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
IncomingMessage::Events& IncomingMessage::events() { return m_state->ev; }

IncomingMessage& IncomingMessage::pause() {
    if (m_state->socket) m_state->socket.pause();
    return *this;
}

IncomingMessage& IncomingMessage::resume() {
    if (m_state->socket) m_state->socket.resume();
    return *this;
}

IncomingMessage& IncomingMessage::set_timeout(std::chrono::milliseconds timeout, std::function<void()> fn) {
    if (m_state->socket) m_state->socket.set_timeout(timeout, std::move(fn));
    return *this;
}

void IncomingMessage::body(std::function<void(const std::error_code&, std::string)> callback, std::size_t limit) {
    if (!callback) return;

    struct Collector {
        std::string acc;
        bool        done = false;
        std::mutex  m;
    };

    auto c = std::make_shared<Collector>();
    auto st = m_state;
    bool already_ended;

    {
        std::lock_guard<std::mutex> lk(st->m);
        already_ended = st->end_emitted;
    }

    if (already_ended) {
        loop().post([callback]() { callback({}, std::string()); });
        return;
    }

    auto finish = [c, callback](const std::error_code& ec) {
        std::string out;

        {
            std::lock_guard<std::mutex> lk(c->m);
            if (c->done) return;
            c->done = true;
            out.swap(c->acc);
        }

        callback(ec, std::move(out));
    };

    st->ev.end.add([finish]() { finish({}); });
    st->ev.aborted.add([finish]() { finish(async::make_error_code(async::error::connection_reset)); });

    on_data([c, limit, finish](const DataView& d) {
        bool over = false;

        {
            std::lock_guard<std::mutex> lk(c->m);
            if (c->done) return;
            if (c->acc.size() + d.size() > limit) over = true;
            else c->acc.append(reinterpret_cast<const char*>(d.data()), d.size());
        }

        if (over) finish(async::make_error_code(async::error::limit_exceeded));
    });
}

void IncomingMessage::discard() {
    on_data([](const DataView&) {});
}

std::any& IncomingMessage::data() { return m_state->user; }

} // namespace http
} // namespace networking
} // namespace fizmo
