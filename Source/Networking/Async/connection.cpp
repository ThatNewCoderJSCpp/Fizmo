#include "fizmo_library.hpp"
#include "connection.hpp"
#include "composed.hpp"

namespace fizmo {
namespace networking {
namespace async {

namespace {

std::atomic<std::uint64_t> g_connection_ids{0};

long long now_ticks() noexcept {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

} // namespace

Connection::Connection(std::unique_ptr<AsyncStream> stream)
    : m_stream(std::move(stream)),
      m_strand(m_stream->context()),
      m_idle_timer(m_stream->context()),
      m_id(++g_connection_ids),
      m_secure(m_stream->is_secure()),
      m_remote(m_stream->remote_endpoint()),
      m_local(m_stream->local_endpoint()),
      m_read_buf(16384) {
    touch();
}

Connection::Ptr Connection::create(std::unique_ptr<AsyncStream> stream) {
    return Ptr(new Connection(std::move(stream)));
}

Connection::~Connection() {
    m_idle_timer.cancel();
    if (m_stream) m_stream->close();
}

void Connection::touch() noexcept {
    m_last_activity = now_ticks();
}

std::chrono::steady_clock::time_point Connection::last_activity() const noexcept {
    return std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(m_last_activity.load()));
}

void Connection::start() {
    auto self = shared_from_this();

    m_strand.dispatch([self]() {
        if (self->m_started || self->m_closed) return;
        self->m_started = true;
        self->arm_idle_timer();
        self->read_next();
    });
}

Connection& Connection::on_data(DataHandler h) { m_on_data = std::move(h); return *this; }

Connection& Connection::on_line(LineHandler h, std::size_t max_line) {
    m_on_line = std::move(h);
    m_framing = Framing::Line;
    m_frame_limit = max_line;
    return *this;
}

Connection& Connection::on_message(MessageHandler h, std::size_t max_message) {
    m_on_message = std::move(h);
    m_framing = Framing::LengthPrefixed;
    m_frame_limit = max_message;
    return *this;
}

Connection& Connection::on_close(CloseHandler h) { m_on_close = std::move(h); return *this; }
Connection& Connection::on_error(ErrorHandler h) { m_on_error = std::move(h); return *this; }
Connection& Connection::on_drain(EventHandler h) { m_on_drain = std::move(h); return *this; }
Connection& Connection::on_end(EventHandler h) { m_on_end = std::move(h); return *this; }
Connection& Connection::on_timeout(EventHandler h) { m_on_timeout = std::move(h); return *this; }

void Connection::set_read_buffer_size(std::size_t bytes) {
    auto self = shared_from_this();
    m_strand.dispatch([self, bytes]() { if (!self->m_reading && bytes > 0) self->m_read_buf.resize(bytes); });
}

void Connection::read_next() {
    if (m_closed || m_paused || m_reading) return;
    m_reading = true;
    auto self = shared_from_this();

    m_stream->async_read_some(MutableBuffer(m_read_buf.data(), m_read_buf.size()), [self](std::error_code ec, std::size_t n) {
        if (self->m_read_discounted.exchange(false)) self->context().work_started();
        self->m_strand.post([self, ec, n]() { self->on_read(ec, n); });
    });

    if (m_unref && m_reading && !m_read_discounted.exchange(true)) context().work_finished();
}

void Connection::set_ref(bool ref) {
    auto self = shared_from_this();

    m_strand.dispatch([self, ref]() {
        self->m_unref = !ref;

        if (ref) {
            if (self->m_read_discounted.exchange(false)) self->context().work_started();
            return;
        }

        if (self->m_reading && !self->m_closed && !self->m_read_discounted.exchange(true)) self->context().work_finished();
    });
}

void Connection::on_read(std::error_code ec, std::size_t n) {
    m_reading = false;
    if (m_closed) return;
    auto self = shared_from_this();

    if (n > 0) {
        m_bytes_read += n;
        touch();
        deliver(m_read_buf.data(), n);
        if (m_closed) return;
    }

    if (ec) {
        if (ec == error::eof) {
            if (m_on_end) m_on_end(self);
            if (m_closed) return;
            if (m_shutdown_started) finish({});
            else end();
            return;
        }

        if (ec != error::operation_aborted && m_on_error) m_on_error(self, ec);
        finish(ec);
        return;
    }

    read_next();
}

void Connection::deliver(const std::uint8_t* data, std::size_t n) {
    auto self = shared_from_this();
    if (m_on_data) m_on_data(self, data, n);
    if (m_closed) return;

    if (m_framing == Framing::Line && m_on_line) {
        m_frame_buf.insert(m_frame_buf.end(), data, data + n);
        std::size_t start = 0;

        for (std::size_t i = 0; i < m_frame_buf.size(); ++i) {
            if (m_frame_buf[i] != '\n') continue;
            std::size_t end = i;
            if (end > start && m_frame_buf[end - 1] == '\r') --end;
            const std::string line(reinterpret_cast<const char*>(m_frame_buf.data() + start), end - start);
            start = i + 1;
            m_on_line(self, line);
            if (m_closed) return;
        }

        m_frame_buf.erase(m_frame_buf.begin(), m_frame_buf.begin() + static_cast<std::ptrdiff_t>(start));

        if (m_frame_buf.size() > m_frame_limit) {
            if (m_on_error) m_on_error(self, make_error_code(error::limit_exceeded));
            finish(make_error_code(error::limit_exceeded));
        }

        return;
    }

    if (m_framing == Framing::LengthPrefixed && m_on_message) {
        m_frame_buf.insert(m_frame_buf.end(), data, data + n);
        std::size_t pos = 0;

        while (m_frame_buf.size() - pos >= 4) {
            const std::uint8_t* p = m_frame_buf.data() + pos;
            const std::size_t len = (std::size_t(p[0]) << 24) | (std::size_t(p[1]) << 16) | (std::size_t(p[2]) << 8) | p[3];

            if (len > m_frame_limit) {
                if (m_on_error) m_on_error(self, make_error_code(error::limit_exceeded));
                finish(make_error_code(error::limit_exceeded));
                return;
            }

            if (m_frame_buf.size() - pos - 4 < len) break;
            std::vector<std::uint8_t> msg(p + 4, p + 4 + len);
            pos += 4 + len;
            m_on_message(self, std::move(msg));
            if (m_closed) return;
        }

        m_frame_buf.erase(m_frame_buf.begin(), m_frame_buf.begin() + static_cast<std::ptrdiff_t>(pos));
    }
}

bool Connection::write(const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    return write(std::vector<std::uint8_t>(p, p + len));
}

bool Connection::write(std::string data) {
    return write(std::vector<std::uint8_t>(data.begin(), data.end()));
}

bool Connection::write(std::vector<std::uint8_t> data) {
    return write(std::move(data), WriteCallback());
}

bool Connection::write(std::vector<std::uint8_t> data, WriteCallback done) {
    if (m_closed) {
        if (done) {
            auto self = shared_from_this();
            m_strand.post([done]() { done(make_error_code(error::closed)); });
        }

        return false;
    }

    m_buffered += data.size();
    auto self = shared_from_this();
    m_strand.dispatch([self, d = std::move(data), cb = std::move(done)]() mutable { self->queue_write(std::move(d), std::move(cb)); });
    return m_buffered.load() < m_high_watermark;
}

bool Connection::write_line(const std::string& line) {
    return write(line + "\n");
}

bool Connection::write_message(const void* data, std::size_t len) {
    std::vector<std::uint8_t> framed(4 + len);
    framed[0] = static_cast<std::uint8_t>(len >> 24);
    framed[1] = static_cast<std::uint8_t>(len >> 16);
    framed[2] = static_cast<std::uint8_t>(len >> 8);
    framed[3] = static_cast<std::uint8_t>(len);
    if (len) std::memcpy(framed.data() + 4, data, len);
    return write(std::move(framed));
}

void Connection::queue_write(std::vector<std::uint8_t> data, WriteCallback done) {
    if (m_closed || m_ending) {
        m_buffered -= data.size();
        if (done) done(make_error_code(m_closed ? error::closed : error::shut_down));
        return;
    }

    if (data.empty()) {
        if (done) {
            if (!m_writing && m_write_queue.empty()) { done({}); return; }
        } else {
            return;
        }
    }

    m_write_queue.push_back(WriteEntry{ std::move(data), std::move(done) });
    if (!m_writing) write_next();
}

void Connection::write_next() {
    auto self = shared_from_this();

    if (m_write_queue.empty()) {
        m_writing = false;
        if (m_on_drain) m_on_drain(self);
        if (m_ending && !m_closed) begin_shutdown();
        return;
    }

    m_writing = true;

    if (m_write_queue.front().data.empty()) {
        m_strand.post([self]() { self->on_write({}, 0); });
        return;
    }

    const auto& front = m_write_queue.front().data;

    async_write(*m_stream, ConstBuffer(front.data(), front.size()), [self](std::error_code ec, std::size_t n) {
        self->m_strand.post([self, ec, n]() { self->on_write(ec, n); });
    });
}

void Connection::on_write(std::error_code ec, std::size_t n) {
    WriteCallback done;

    if (!m_write_queue.empty()) {
        m_buffered -= m_write_queue.front().data.size();
        done = std::move(m_write_queue.front().done);
        m_write_queue.pop_front();
    }

    m_bytes_written += n;
    touch();
    if (done) done(ec);
    if (m_closed) return;

    if (ec) {
        auto self = shared_from_this();
        if (ec != error::operation_aborted && m_on_error) m_on_error(self, ec);
        finish(ec);
        return;
    }

    write_next();
}

void Connection::end() {
    auto self = shared_from_this();

    m_strand.dispatch([self]() {
        if (self->m_closed || self->m_ending) return;
        self->m_ending = true;
        if (!self->m_writing && self->m_write_queue.empty()) self->begin_shutdown();
    });
}

void Connection::begin_shutdown() {
    if (m_shutdown_started || m_closed) return;
    m_shutdown_started = true;
    auto self = shared_from_this();

    m_stream->async_shutdown([self](std::error_code) {
        self->m_strand.post([self]() {
            if (self->m_closed) return;
            if (!self->m_reading) { self->finish({}); return; }
            self->m_idle_timer.expires_after(std::chrono::seconds(5));
            std::weak_ptr<Connection> weak = self;

            self->m_idle_timer.async_wait([weak](std::error_code ec) {
                if (ec) return;
                if (auto s = weak.lock()) s->m_strand.post([s]() { s->finish({}); });
            });
        });
    });
}

void Connection::close() {
    auto self = shared_from_this();
    m_strand.dispatch([self]() { self->finish({}); });
}

void Connection::destroy(std::error_code reason) {
    auto self = shared_from_this();
    m_strand.dispatch([self, reason]() { self->finish(reason); });
}

void Connection::pause() {
    m_paused = true;
}

void Connection::resume() {
    auto self = shared_from_this();

    m_strand.dispatch([self]() {
        self->m_paused = false;
        if (self->m_started) self->read_next();
    });
}

void Connection::set_idle_timeout(std::chrono::milliseconds timeout) {
    auto self = shared_from_this();

    m_strand.dispatch([self, timeout]() {
        self->m_idle_timeout = timeout;
        self->touch();
        self->arm_idle_timer();
    });
}

void Connection::arm_idle_timer() {
    if (m_closed || m_shutdown_started) return;

    if (m_idle_timeout.count() <= 0) {
        m_idle_timer.cancel();
        return;
    }

    const auto deadline = last_activity() + m_idle_timeout;
    m_idle_timer.expires_at(deadline);
    std::weak_ptr<Connection> weak = shared_from_this();

    m_idle_timer.async_wait([weak](std::error_code ec) {
        if (ec) return;
        auto self = weak.lock();
        if (!self) return;

        self->m_strand.post([self]() {
            if (self->m_closed || self->m_idle_timeout.count() <= 0) return;

            if (std::chrono::steady_clock::now() - self->last_activity() < self->m_idle_timeout) {
                self->arm_idle_timer();
                return;
            }

            if (self->m_on_timeout) {
                self->m_on_timeout(self);
                self->touch();
                self->arm_idle_timer();
            } else {
                self->finish(make_error_code(error::timed_out));
            }
        });
    });
}

void Connection::finish(std::error_code ec) {
    if (m_close_fired) return;
    m_close_fired = true;
    m_closed = true;
    m_idle_timer.cancel();
    m_stream->close();
    std::deque<WriteEntry> pending;
    pending.swap(m_write_queue);
    m_buffered = 0;
    auto self = shared_from_this();

    for (auto& w : pending) {
        if (w.done) w.done(ec ? ec : make_error_code(error::operation_aborted));
    }

    if (m_on_close) m_on_close(self, ec);
    if (m_close_hook) m_close_hook(self);
    m_on_data = nullptr;
    m_on_line = nullptr;
    m_on_message = nullptr;
    m_on_close = nullptr;
    m_on_error = nullptr;
    m_on_drain = nullptr;
    m_on_end = nullptr;
    m_on_timeout = nullptr;
    m_close_hook = nullptr;
}

std::string Connection::to_string() const {
    std::string s = "Connection#" + std::to_string(m_id) + "[" + (m_secure ? "tls " : "tcp ") + m_remote.to_string();
    s += is_open() ? " open" : " closed";
    s += " rx=" + std::to_string(m_bytes_read.load()) + " tx=" + std::to_string(m_bytes_written.load()) + "]";
    return s;
}

} // namespace async
} // namespace networking
} // namespace fizmo
