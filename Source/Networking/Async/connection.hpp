#ifndef FIZMO_ASYNC_CONNECTION_HPP
#define FIZMO_ASYNC_CONNECTION_HPP

#include "stream.hpp"
#include "strand.hpp"
#include "timer.hpp"

#include <any>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;

    using DataHandler    = std::function<void(const Ptr&, const std::uint8_t*, std::size_t)>;
    using CloseHandler   = std::function<void(const Ptr&, std::error_code)>;
    using ErrorHandler   = std::function<void(const Ptr&, std::error_code)>;
    using EventHandler   = std::function<void(const Ptr&)>;
    using LineHandler    = std::function<void(const Ptr&, const std::string&)>;
    using MessageHandler = std::function<void(const Ptr&, std::vector<std::uint8_t>)>;
    using WriteCallback  = std::function<void(std::error_code)>;

    enum class Framing {
        None,
        Line,
        LengthPrefixed
    };

    static Ptr create(std::unique_ptr<AsyncStream> stream);

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

public:
    void start();

    Connection& on_data(DataHandler h);
    Connection& on_line(LineHandler h, std::size_t max_line = 1u << 16);
    Connection& on_message(MessageHandler h, std::size_t max_message = 16u << 20);
    Connection& on_close(CloseHandler h);
    Connection& on_error(ErrorHandler h);
    Connection& on_drain(EventHandler h);
    Connection& on_end(EventHandler h);
    Connection& on_timeout(EventHandler h);

    bool write(const void* data, std::size_t len);
    bool write(std::string data);
    bool write(std::vector<std::uint8_t> data);
    bool write(std::vector<std::uint8_t> data, WriteCallback done);
    bool write_line(const std::string& line);
    bool write_message(const void* data, std::size_t len);
    bool write_message(const std::string& data) { return write_message(data.data(), data.size()); }

    void end();
    void close();
    void destroy(std::error_code reason);

    void pause();
    void resume();
    void set_ref(bool ref);
    bool has_ref() const noexcept { return !m_unref.load(); }

    void set_idle_timeout(std::chrono::milliseconds timeout);
    void set_high_watermark(std::size_t bytes) noexcept { m_high_watermark = bytes; }
    void set_read_buffer_size(std::size_t bytes);

public:
    std::uint64_t id() const noexcept { return m_id; }
    bool is_open() const noexcept { return !m_closed.load(); }
    bool is_secure() const noexcept { return m_secure; }
    bool is_paused() const noexcept { return m_paused.load(); }
    Endpoint remote_endpoint() const noexcept { return m_remote; }
    Endpoint local_endpoint() const noexcept { return m_local; }
    std::size_t buffered_amount() const noexcept { return m_buffered.load(); }
    std::uint64_t bytes_read() const noexcept { return m_bytes_read.load(); }
    std::uint64_t bytes_written() const noexcept { return m_bytes_written.load(); }
    std::chrono::steady_clock::time_point last_activity() const noexcept;

    AsyncStream& stream() noexcept { return *m_stream; }
    IOContext& context() noexcept { return m_stream->context(); }
    Strand& strand() noexcept { return m_strand; }

    std::any& user_data() noexcept { return m_user_data; }
    template <typename T> T* data() { return std::any_cast<T>(&m_user_data); }

    void set_close_hook(std::function<void(const Ptr&)> hook) { m_close_hook = std::move(hook); }

    std::string to_string() const;

private:
    explicit Connection(std::unique_ptr<AsyncStream> stream);

    std::unique_ptr<AsyncStream>         m_stream;
    Strand                               m_strand;
    SteadyTimer                          m_idle_timer;
    std::uint64_t                        m_id;
    bool                                 m_secure;
    Endpoint                             m_remote;
    Endpoint                             m_local;
    std::vector<std::uint8_t>            m_read_buf;
    struct WriteEntry {
        std::vector<std::uint8_t> data;
        WriteCallback             done;
    };

    std::deque<WriteEntry>               m_write_queue;
    std::atomic<std::size_t>             m_buffered{0};
    std::atomic<std::uint64_t>           m_bytes_read{0};
    std::atomic<std::uint64_t>           m_bytes_written{0};
    std::atomic<bool>                    m_closed{false};
    std::atomic<bool>                    m_paused{false};
    std::atomic<bool>                    m_unref{false};
    std::atomic<bool>                    m_read_discounted{false};
    std::atomic<long long>               m_last_activity{0};
    std::size_t                          m_high_watermark = 1u << 20;
    std::chrono::milliseconds            m_idle_timeout{0};
    bool                                 m_started = false;
    bool                                 m_reading = false;
    bool                                 m_writing = false;
    bool                                 m_ending = false;
    bool                                 m_shutdown_started = false;
    bool                                 m_close_fired = false;
    Framing                              m_framing = Framing::None;
    std::size_t                          m_frame_limit = 0;
    std::vector<std::uint8_t>            m_frame_buf;
    std::any                             m_user_data;

    DataHandler                          m_on_data;
    LineHandler                          m_on_line;
    MessageHandler                       m_on_message;
    CloseHandler                         m_on_close;
    ErrorHandler                         m_on_error;
    EventHandler                         m_on_drain;
    EventHandler                         m_on_end;
    EventHandler                         m_on_timeout;
    std::function<void(const Ptr&)>      m_close_hook;

    void touch() noexcept;
    void read_next();
    void on_read(std::error_code ec, std::size_t n);
    void deliver(const std::uint8_t* data, std::size_t n);
    void queue_write(std::vector<std::uint8_t> data, WriteCallback done);
    void write_next();
    void on_write(std::error_code ec, std::size_t n);
    void begin_shutdown();
    void arm_idle_timer();
    void finish(std::error_code ec);
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_CONNECTION_HPP
