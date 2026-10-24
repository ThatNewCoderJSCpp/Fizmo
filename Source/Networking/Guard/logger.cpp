#include "fizmo_library.hpp"
#include "logger.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string value_text(const LogValue& v, bool json) {
    switch (v.index()) {
        case 0: return json ? "\"" + json_escape(std::get<std::string>(v)) + "\"" : std::get<std::string>(v);
        case 1: return std::to_string(std::get<std::int64_t>(v));
        case 2: {
            const double d = std::get<double>(v);
            if (!std::isfinite(d)) return json ? "null" : "nan";
            char buf[40];
            std::snprintf(buf, sizeof(buf), "%.6g", d);
            return buf;
        }
        default: return std::get<bool>(v) ? "true" : "false";
    }
}

} // namespace

const char* to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Fatal: return "fatal";
        default:              return "off";
    }
}

bool parse_log_level(const std::string& text, LogLevel& out) noexcept {
    static const LogLevel all[] = { LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error, LogLevel::Fatal, LogLevel::Off };

    for (LogLevel l : all) {
        if (lower(text) == to_string(l)) { out = l; return true; }
    }

    if (lower(text) == "warning") { out = LogLevel::Warn; return true; }
    return false;
}

LogField field(const std::string& key, const std::string& value) { return { key, LogValue(value) }; }
LogField field(const std::string& key, const char* value) { return { key, LogValue(std::string(value ? value : "")) }; }
LogField field(const std::string& key, std::int64_t value) { return { key, LogValue(value) }; }
LogField field(const std::string& key, int value) { return { key, LogValue(static_cast<std::int64_t>(value)) }; }
LogField field(const std::string& key, unsigned value) { return { key, LogValue(static_cast<std::int64_t>(value)) }; }
LogField field(const std::string& key, std::uint64_t value) { return { key, LogValue(static_cast<std::int64_t>(value)) }; }
LogField field(const std::string& key, double value) { return { key, LogValue(value) }; }
LogField field(const std::string& key, bool value) { return { key, LogValue(value) }; }

void StreamSink::write(const LogEntry&, const std::string& line) {
    if (!m_stream) return;
    std::fwrite(line.data(), 1, line.size(), m_stream);
    std::fputc('\n', m_stream);
}

void StreamSink::flush() {
    if (m_stream) std::fflush(m_stream);
}

FileSink::FileSink(std::string path, std::size_t max_bytes, std::size_t max_files)
    : m_path(std::move(path)), m_max_bytes(max_bytes), m_max_files(max_files ? max_files : 1) {
    open();
}

FileSink::~FileSink() {
    if (m_file) std::fclose(m_file);
}

void FileSink::open() {
    m_file = std::fopen(m_path.c_str(), "ab");
    m_size = 0;

    if (m_file) {
        std::fseek(m_file, 0, SEEK_END);
        const long pos = std::ftell(m_file);
        m_size = pos > 0 ? static_cast<std::size_t>(pos) : 0;
    }
}

void FileSink::rotate() {
    if (m_file) {
        std::fclose(m_file);
        m_file = nullptr;
    }

    for (std::size_t i = m_max_files; i > 1; --i) {
        const std::string from = m_path + "." + std::to_string(i - 1);
        const std::string to = m_path + "." + std::to_string(i);
        std::remove(to.c_str());
        std::rename(from.c_str(), to.c_str());
    }

    const std::string first = m_path + ".1";
    std::remove(first.c_str());
    std::rename(m_path.c_str(), first.c_str());
    open();
}

void FileSink::write(const LogEntry&, const std::string& line) {
    if (m_max_bytes && m_size + line.size() + 1 > m_max_bytes && m_size > 0) rotate();
    if (!m_file) return;
    std::fwrite(line.data(), 1, line.size(), m_file);
    std::fputc('\n', m_file);
    m_size += line.size() + 1;
}

void FileSink::flush() {
    if (m_file) std::fflush(m_file);
}

void MemorySink::write(const LogEntry& entry, const std::string& line) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_items.emplace_back(entry, line);
    while (m_capacity && m_items.size() > m_capacity) m_items.pop_front();
}

std::vector<std::string> MemorySink::lines() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::string> out;
    for (const auto& i : m_items) out.push_back(i.second);
    return out;
}

std::vector<LogEntry> MemorySink::entries() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<LogEntry> out;
    for (const auto& i : m_items) out.push_back(i.first);
    return out;
}

void MemorySink::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_items.clear();
}

struct Logger::Core {
    mutable std::mutex                    m;
    mutable std::mutex                    write_m;
    LogLevel                              level;
    LogFormat                             format;
    std::vector<std::shared_ptr<LogSink>> sinks;
    std::vector<std::string>              redacted;
    WallNowFn                             now = default_wall_now();
};

Logger::Logger(LogLevel level, LogFormat format) : m_core(std::make_shared<Core>()) {
    m_core->level = level;
    m_core->format = format;
    m_core->redacted = { "password", "passwd", "secret", "token", "authorization", "cookie", "api_key", "apikey",
                         "session", "csrf", "private_key", "credit_card", "card_number", "cvv", "ssn", "otp" };
}

Logger Logger::child(LogFields context) const {
    Logger l(*this);
    l.m_context.insert(l.m_context.end(), context.begin(), context.end());
    return l;
}

void Logger::add_sink(std::shared_ptr<LogSink> sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->sinks.push_back(std::move(sink));
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->sinks.clear();
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->level = level;
}

LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lk(m_core->m);
    return m_core->level;
}

bool Logger::enabled(LogLevel level) const {
    if (level == LogLevel::Off) return false;
    std::lock_guard<std::mutex> lk(m_core->m);
    return level >= m_core->level && !m_core->sinks.empty();
}

void Logger::set_format(LogFormat format) {
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->format = format;
}

void Logger::add_redacted_key(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->redacted.push_back(lower(key));
}

void Logger::set_wall_clock(WallNowFn now) {
    std::lock_guard<std::mutex> lk(m_core->m);
    m_core->now = now ? std::move(now) : default_wall_now();
}

bool Logger::is_sensitive_key(const std::string& key) const {
    const std::string k = lower(key);
    std::lock_guard<std::mutex> lk(m_core->m);

    for (const auto& r : m_core->redacted) {
        if (k.find(r) != std::string::npos) return true;
    }

    return false;
}

std::string Logger::redact_value(const std::string& key, const std::string& value) const {
    if (is_sensitive_key(key)) return "[REDACTED]";
    const std::string l = lower(value.substr(0, 7));
    if (l.rfind("bearer ", 0) == 0 || l.rfind("basic ", 0) == 0) return "[REDACTED]";

    if (value.size() > 30 && value.rfind("eyJ", 0) == 0 && std::count(value.begin(), value.end(), '.') == 2) return "[REDACTED]";
    return value;
}

std::string Logger::format(const LogEntry& entry) const {
    LogFormat fmt;

    {
        std::lock_guard<std::mutex> lk(m_core->m);
        fmt = m_core->format;
    }

    auto safe = [&](const LogField& f) -> LogValue {
        if (f.second.index() == 0) return LogValue(redact_value(f.first, std::get<std::string>(f.second)));
        if (is_sensitive_key(f.first)) return LogValue(std::string("[REDACTED]"));
        return f.second;
    };

    std::string out;

    if (fmt == LogFormat::Json) {
        out = "{\"time\":\"" + format_iso8601(entry.time) + "\",\"level\":\"" + to_string(entry.level) + "\",\"msg\":\"" + json_escape(entry.message) + "\"";

        for (const auto& f : entry.fields) {
            out += ",\"" + json_escape(f.first) + "\":" + value_text(safe(f), true);
        }

        out += "}";
    } else {
        std::string lvl = to_string(entry.level);
        for (auto& c : lvl) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        out = format_iso8601(entry.time) + " " + lvl + " " + entry.message;

        for (const auto& f : entry.fields) {
            std::string v = value_text(safe(f), false);
            for (auto& c : v) if (c == '\n' || c == '\r') c = ' ';
            out += " " + f.first + "=" + v;
        }
    }

    return out;
}

void Logger::log(LogLevel level, const std::string& message, LogFields fields) const {
    if (level == LogLevel::Off) return;
    std::vector<std::shared_ptr<LogSink>> sinks;
    WallNowFn now;

    {
        std::lock_guard<std::mutex> lk(m_core->m);
        if (level < m_core->level || m_core->sinks.empty()) return;
        sinks = m_core->sinks;
        now = m_core->now;
    }

    LogEntry entry;
    entry.time = now();
    entry.level = level;
    entry.message = message;
    entry.fields = m_context;
    entry.fields.insert(entry.fields.end(), fields.begin(), fields.end());
    const std::string line = format(entry);
    std::lock_guard<std::mutex> wl(m_core->write_m);

    for (auto& s : sinks) {
        try {
            s->write(entry, line);
            if (level >= LogLevel::Error) s->flush();
        } catch (...) {
        }
    }
}

void Logger::flush() const {
    std::vector<std::shared_ptr<LogSink>> sinks;

    {
        std::lock_guard<std::mutex> lk(m_core->m);
        sinks = m_core->sinks;
    }

    std::lock_guard<std::mutex> wl(m_core->write_m);
    for (auto& s : sinks) s->flush();
}

std::string Logger::new_request_id() {
    return crypto::random_token_base64url(12);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
