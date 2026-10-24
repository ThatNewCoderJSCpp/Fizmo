#ifndef FIZMO_GUARD_LOGGER_HPP
#define FIZMO_GUARD_LOGGER_HPP

#include "guard_types.hpp"

#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off
};

const char* to_string(LogLevel level) noexcept;
bool parse_log_level(const std::string& text, LogLevel& out) noexcept;

using LogValue  = std::variant<std::string, std::int64_t, double, bool>;
using LogField  = std::pair<std::string, LogValue>;
using LogFields = std::vector<LogField>;

LogField field(const std::string& key, const std::string& value);
LogField field(const std::string& key, const char* value);
LogField field(const std::string& key, std::int64_t value);
LogField field(const std::string& key, int value);
LogField field(const std::string& key, unsigned value);
LogField field(const std::string& key, std::uint64_t value);
LogField field(const std::string& key, double value);
LogField field(const std::string& key, bool value);

struct LogEntry {
    WallTime    time{};
    LogLevel    level = LogLevel::Info;
    std::string message;
    LogFields   fields;
};

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogEntry& entry, const std::string& line) = 0;
    virtual void flush() {}
};

class StreamSink final : public LogSink {
public:
    explicit StreamSink(std::FILE* stream = stderr) noexcept : m_stream(stream) {}
    void write(const LogEntry& entry, const std::string& line) override;
    void flush() override;

private:
    std::FILE* m_stream;
};

class FileSink final : public LogSink {
public:
    FileSink(std::string path, std::size_t max_bytes = 10u << 20, std::size_t max_files = 5);
    ~FileSink() override;

    void write(const LogEntry& entry, const std::string& line) override;
    void flush() override;
    bool good() const noexcept { return m_file != nullptr; }

private:
    std::string m_path;
    std::size_t m_max_bytes;
    std::size_t m_max_files;
    std::FILE*  m_file = nullptr;
    std::size_t m_size = 0;

    void open();
    void rotate();
};

class MemorySink final : public LogSink {
public:
    explicit MemorySink(std::size_t capacity = 1000) : m_capacity(capacity) {}

    void write(const LogEntry& entry, const std::string& line) override;
    std::vector<std::string> lines() const;
    std::vector<LogEntry> entries() const;
    void clear();

private:
    std::size_t                                    m_capacity;
    mutable std::mutex                             m_mutex;
    std::deque<std::pair<LogEntry, std::string>>   m_items;
};

class CallbackSink final : public LogSink {
public:
    using Callback = std::function<void(const LogEntry&, const std::string&)>;
    explicit CallbackSink(Callback cb) : m_cb(std::move(cb)) {}
    void write(const LogEntry& entry, const std::string& line) override { if (m_cb) m_cb(entry, line); }

private:
    Callback m_cb;
};

enum class LogFormat {
    Json,
    Text
};

class Logger {
public:
    explicit Logger(LogLevel level = LogLevel::Info, LogFormat format = LogFormat::Json);

    Logger child(LogFields context) const;

    void add_sink(std::shared_ptr<LogSink> sink);
    void clear_sinks();
    void set_level(LogLevel level);
    LogLevel level() const;
    bool enabled(LogLevel level) const;
    void set_format(LogFormat format);
    void add_redacted_key(const std::string& key);
    void set_wall_clock(WallNowFn now);

    void log(LogLevel level, const std::string& message, LogFields fields = {}) const;
    void trace(const std::string& message, LogFields fields = {}) const { log(LogLevel::Trace, message, std::move(fields)); }
    void debug(const std::string& message, LogFields fields = {}) const { log(LogLevel::Debug, message, std::move(fields)); }
    void info(const std::string& message, LogFields fields = {}) const { log(LogLevel::Info, message, std::move(fields)); }
    void warn(const std::string& message, LogFields fields = {}) const { log(LogLevel::Warn, message, std::move(fields)); }
    void error(const std::string& message, LogFields fields = {}) const { log(LogLevel::Error, message, std::move(fields)); }
    void fatal(const std::string& message, LogFields fields = {}) const { log(LogLevel::Fatal, message, std::move(fields)); }
    void flush() const;

    bool is_sensitive_key(const std::string& key) const;
    std::string redact_value(const std::string& key, const std::string& value) const;
    std::string format(const LogEntry& entry) const;

    static std::string new_request_id();

private:
    struct Core;
    std::shared_ptr<Core> m_core;
    LogFields             m_context;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_LOGGER_HPP
