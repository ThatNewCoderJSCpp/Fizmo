#include "fizmo_library.hpp"
#include "http.hpp"

#include <algorithm>
#include <cctype>

namespace fizmo {
namespace networking {
namespace http {

namespace {

bool is_tchar(unsigned char c) noexcept {
    if (std::isalnum(c)) return true;
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-': case '.': case '^': case '_': case '`': case '|': case '~':
            return true;
        default:
            return false;
    }
}

char lower(char c) noexcept { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

std::string trim_ows(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split_tokens(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;

    while (i <= s.size()) {
        std::size_t c = s.find(',', i);
        if (c == std::string::npos) c = s.size();
        std::string t = trim_ows(s.substr(i, c - i));
        for (auto& ch : t) ch = lower(ch);
        if (!t.empty()) out.push_back(t);
        i = c + 1;
    }

    return out;
}

} // namespace

const std::vector<std::string>& methods() {
    static const std::vector<std::string> list = {
        "ACL", "BIND", "CHECKOUT", "CONNECT", "COPY", "DELETE", "GET", "HEAD", "LINK", "LOCK", "M-SEARCH", "MERGE", "MKACTIVITY",
        "MKCALENDAR", "MKCOL", "MOVE", "NOTIFY", "OPTIONS", "PATCH", "POST", "PROPFIND", "PROPPATCH", "PURGE", "PUT", "QUERY",
        "REBIND", "REPORT", "SEARCH", "SOURCE", "SUBSCRIBE", "TRACE", "UNBIND", "UNLINK", "UNLOCK", "UNSUBSCRIBE"
    };
    return list;
}

const char* status_text(int code) noexcept {
    switch (code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        case 103: return "Early Hints";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 208: return "Already Reported";
        case 226: return "IM Used";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a Teapot";
        case 421: return "Misdirected Request";
        case 422: return "Unprocessable Entity";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 425: return "Too Early";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 451: return "Unavailable For Legal Reasons";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";
        case 507: return "Insufficient Storage";
        case 508: return "Loop Detected";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";
        default: return "Unknown";
    }
}

Headers::Headers(std::initializer_list<Entry> entries) {
    for (auto& e : entries) append(e.first, e.second);
}

bool Headers::iequals(const std::string& a, const std::string& b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (lower(a[i]) != lower(b[i])) return false;
    return true;
}

bool Headers::valid_name(const std::string& name) noexcept {
    if (name.empty()) return false;
    for (unsigned char c : name) if (!is_tchar(c)) return false;
    return true;
}

bool Headers::valid_value(const std::string& value) noexcept {
    for (unsigned char c : value) {
        if (c == '\t') continue;
        if (c < 0x20 || c == 0x7F) return false;
    }

    return true;
}

Headers& Headers::set(const std::string& name, const std::string& value) {
    remove(name);
    m_entries.emplace_back(name, value);
    return *this;
}

Headers& Headers::set(const std::string& name, const std::vector<std::string>& values) {
    remove(name);
    for (auto& v : values) m_entries.emplace_back(name, v);
    return *this;
}

Headers& Headers::append(const std::string& name, const std::string& value) {
    m_entries.emplace_back(name, value);
    return *this;
}

std::string Headers::get(const std::string& name, const std::string& fallback) const {
    std::string out;
    bool found = false;
    const char* sep = iequals(name, "cookie") ? "; " : ", ";

    for (auto& e : m_entries) {
        if (!iequals(e.first, name)) continue;
        if (found) out += sep;
        out += e.second;
        found = true;
    }

    return found ? out : fallback;
}

std::vector<std::string> Headers::get_all(const std::string& name) const {
    std::vector<std::string> out;
    for (auto& e : m_entries) if (iequals(e.first, name)) out.push_back(e.second);
    return out;
}

bool Headers::has(const std::string& name) const {
    for (auto& e : m_entries) if (iequals(e.first, name)) return true;
    return false;
}

bool Headers::has_token(const std::string& name, const std::string& token) const {
    for (auto& e : m_entries) {
        if (!iequals(e.first, name)) continue;
        for (auto& t : split_tokens(e.second)) if (iequals(t, token)) return true;
    }

    return false;
}

bool Headers::remove(const std::string& name) {
    const std::size_t before = m_entries.size();
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry& e) { return iequals(e.first, name); }), m_entries.end());
    return m_entries.size() != before;
}

std::vector<std::string> Headers::names() const {
    std::vector<std::string> out;

    for (auto& e : m_entries) {
        std::string l = e.first;
        for (auto& c : l) c = lower(c);
        if (std::find(out.begin(), out.end(), l) == out.end()) out.push_back(l);
    }

    return out;
}

std::string Headers::serialize() const {
    std::string out;

    for (auto& e : m_entries) {
        if (!valid_name(e.first) || !valid_value(e.second)) continue;
        out += e.first;
        out += ": ";
        out += e.second;
        out += "\r\n";
    }

    return out;
}

namespace detail {

Parser::Parser(Mode mode, Limits limits) : m_mode(mode), m_limits(limits) {}

void Parser::reset() {
    m_state = State::Idle;
    m_error_status = 0;
    m_error_message.clear();
    m_line.clear();
    m_head_size = 0;
    m_first_line = true;
    m_method.clear();
    m_target.clear();
    m_major = 1;
    m_minor = 1;
    m_status = 0;
    m_reason.clear();
    m_headers.clear();
    m_trailers.clear();
    m_keep_alive = true;
    m_upgrade = false;
    m_chunked = false;
    m_has_length = false;
    m_until_eof = false;
    m_skip_body = false;
    m_length = 0;
    m_body = BodyMode::None;
    m_chunk_state = ChunkState::Size;
    m_remaining = 0;
    m_trailer_size = 0;
}

bool Parser::fail(int status, const std::string& message) {
    m_state = State::Error;
    m_error_status = status;
    m_error_message = message;
    return false;
}

bool Parser::take_line(const std::uint8_t* data, std::size_t len, std::size_t& i, std::string& out, bool& complete) {
    complete = false;
    const bool in_head = m_state == State::Head;

    while (i < len) {
        const char c = static_cast<char>(data[i++]);

        if (in_head && ++m_head_size > m_limits.max_head_size) {
            return fail(m_mode == Mode::Request ? 431 : 502, "Header section too large");
        }

        if (c == '\n') {
            if (!m_line.empty() && m_line.back() == '\r') m_line.pop_back();
            if (m_line.find('\r') != std::string::npos) return fail(400, "Bare CR in header section");
            out.swap(m_line);
            m_line.clear();
            complete = true;
            return true;
        }

        m_line.push_back(c);

        if (m_line.size() > m_limits.max_line) {
            return fail(m_mode == Mode::Request ? (m_first_line && in_head ? 414 : 431) : 502, "Line too long");
        }
    }

    return true;
}

bool Parser::parse_start_line(const std::string& line) {
    auto parse_version = [this](const std::string& v) {
        if (v.size() != 8 || v.compare(0, 5, "HTTP/") != 0 || !std::isdigit(static_cast<unsigned char>(v[5])) || v[6] != '.' || !std::isdigit(static_cast<unsigned char>(v[7]))) return false;
        m_major = v[5] - '0';
        m_minor = v[7] - '0';
        return true;
    };

    if (m_mode == Mode::Request) {
        const std::size_t sp1 = line.find(' ');
        if (sp1 == std::string::npos || sp1 == 0) return fail(400, "Malformed request line");
        const std::size_t sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos || sp2 == sp1 + 1) return fail(400, "Malformed request line");
        m_method = line.substr(0, sp1);
        m_target = line.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string version = line.substr(sp2 + 1);
        for (unsigned char c : m_method) if (!is_tchar(c)) return fail(400, "Invalid method");
        for (unsigned char c : m_target) if (c <= 0x20 || c == 0x7F) return fail(400, "Invalid request target");
        if (!parse_version(version)) return fail(400, "Invalid HTTP version");
        if (m_major != 1) return fail(505, "HTTP version not supported");
        return true;
    }

    if (line.size() < 12 || !parse_version(line.substr(0, 8)) || line[8] != ' ') return fail(502, "Malformed status line");
    if (!std::isdigit(static_cast<unsigned char>(line[9])) || !std::isdigit(static_cast<unsigned char>(line[10])) || !std::isdigit(static_cast<unsigned char>(line[11]))) return fail(502, "Malformed status code");
    m_status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + (line[11] - '0');
    if (m_status < 100) return fail(502, "Malformed status code");
    if (line.size() > 12 && line[12] != ' ') return fail(502, "Malformed status line");
    m_reason = line.size() > 13 ? line.substr(13) : std::string();
    if (m_major != 1) return fail(502, "Unsupported HTTP version");
    return true;
}

bool Parser::parse_header_line(const std::string& line, Headers& into) {
    if (line[0] == ' ' || line[0] == '\t') return fail(m_mode == Mode::Request ? 400 : 502, "Obsolete header line folding");
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) return fail(m_mode == Mode::Request ? 400 : 502, "Malformed header line");
    const std::string name = line.substr(0, colon);
    if (!Headers::valid_name(name)) return fail(m_mode == Mode::Request ? 400 : 502, "Invalid header name");
    const std::string value = trim_ows(line.substr(colon + 1));
    if (!Headers::valid_value(value)) return fail(m_mode == Mode::Request ? 400 : 502, "Invalid header value");
    if (into.size() >= m_limits.max_headers) return fail(m_mode == Mode::Request ? 431 : 502, "Too many headers");
    into.append(name, value);
    return true;
}

bool Parser::headers_done() {
    const bool is_request = m_mode == Mode::Request;
    const int bad = is_request ? 400 : 502;

    if (m_major == 1 && m_minor >= 1) m_keep_alive = !m_headers.has_token("connection", "close");
    else m_keep_alive = m_headers.has_token("connection", "keep-alive");

    if (is_request) m_upgrade = (m_headers.has_token("connection", "upgrade") && m_headers.has("upgrade")) || m_method == "CONNECT";
    else m_upgrade = m_status == 101;

    const auto te_values = m_headers.get_all("transfer-encoding");
    const auto cl_values = m_headers.get_all("content-length");

    if (!te_values.empty()) {
        if (is_request && !cl_values.empty()) return fail(400, "Both Transfer-Encoding and Content-Length present");
        if (is_request && m_minor == 0) return fail(400, "Transfer-Encoding in an HTTP/1.0 request");
        std::vector<std::string> codings;
        for (auto& v : te_values) for (auto& t : split_tokens(v)) codings.push_back(t);
        const bool chunked_last = !codings.empty() && codings.back() == "chunked";
        const auto chunked_count = std::count(codings.begin(), codings.end(), std::string("chunked"));
        if (chunked_count > 1) return fail(bad, "Repeated chunked coding");

        if (chunked_last) {
            m_chunked = true;
        } else if (is_request) {
            return fail(chunked_count ? 400 : 501, "Unsupported Transfer-Encoding");
        } else {
            m_until_eof = true;
        }
    } else if (!cl_values.empty()) {
        std::uint64_t value = 0;
        bool have = false;

        for (auto& v : cl_values) {
            std::size_t start = 0;

            while (start <= v.size()) {
                std::size_t comma = v.find(',', start);
                if (comma == std::string::npos) comma = v.size();
                const std::string t = trim_ows(v.substr(start, comma - start));
                if (t.empty()) return fail(bad, "Invalid Content-Length");
                std::uint64_t n = 0;

                for (char c : t) {
                    if (!std::isdigit(static_cast<unsigned char>(c))) return fail(bad, "Invalid Content-Length");
                    if (n > (UINT64_MAX - 9) / 10) return fail(bad, "Content-Length too large");
                    n = n * 10 + static_cast<std::uint64_t>(c - '0');
                }

                if (have && n != value) return fail(bad, "Conflicting Content-Length values");
                value = n;
                have = true;
                start = comma + 1;
            }
        }

        m_has_length = true;
        m_length = value;
    }

    m_state = State::Body;
    if (on_headers) on_headers();
    if (m_state == State::Error) return false;

    if (m_upgrade && (!is_request || m_upgrade_enabled)) {
        m_state = State::Upgrade;
        return true;
    }

    const bool no_body = !is_request && (m_skip_body || (m_status >= 100 && m_status < 200) || m_status == 204 || m_status == 304);

    if (no_body || (is_request && m_method == "CONNECT")) {
        complete();
        return true;
    }

    if (m_chunked) {
        m_body = BodyMode::Chunked;
        m_chunk_state = ChunkState::Size;
        return true;
    }

    if (m_has_length) {
        if (m_length == 0) { complete(); return true; }
        m_body = BodyMode::Length;
        m_remaining = m_length;
        return true;
    }

    if (is_request) {
        complete();
        return true;
    }

    m_until_eof = true;
    m_keep_alive = false;
    m_body = BodyMode::UntilEof;
    return true;
}

void Parser::complete() {
    m_state = State::Complete;
    if (on_complete) on_complete();
}

bool Parser::finish_eof() {
    if (m_state == State::Idle || m_state == State::Complete || m_state == State::Upgrade) return true;

    if (m_state == State::Body && m_body == BodyMode::UntilEof) {
        complete();
        return true;
    }

    return false;
}

std::size_t Parser::feed(const std::uint8_t* data, std::size_t len) {
    std::size_t i = 0;

    while (i < len) {
        if (m_state == State::Complete || m_state == State::Upgrade || m_state == State::Error) return i;

        if (m_state == State::Idle) {
            if (m_line.empty() && (data[i] == '\r' || data[i] == '\n')) {
                ++i;
                continue;
            }

            m_state = State::Head;
            m_first_line = true;
            m_head_size = 0;
        }

        if (m_state == State::Head) {
            std::string line;
            bool done = false;
            if (!take_line(data, len, i, line, done)) return i;
            if (!done) return i;

            if (m_first_line) {
                m_first_line = false;
                if (!parse_start_line(line)) return i;
                continue;
            }

            if (line.empty()) {
                headers_done();
                continue;
            }

            if (!parse_header_line(line, m_headers)) return i;
            continue;
        }

        if (m_body == BodyMode::Length) {
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(m_remaining, len - i));
            if (on_body && n) on_body(data + i, n);
            i += n;
            m_remaining -= n;
            if (m_remaining == 0) complete();
            continue;
        }

        if (m_body == BodyMode::UntilEof) {
            if (on_body) on_body(data + i, len - i);
            i = len;
            continue;
        }

        if (m_chunk_state == ChunkState::Data) {
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(m_remaining, len - i));
            if (on_body && n) on_body(data + i, n);
            i += n;
            m_remaining -= n;
            if (m_remaining == 0) m_chunk_state = ChunkState::DataEnd;
            continue;
        }

        std::string line;
        bool done = false;
        if (!take_line(data, len, i, line, done)) return i;
        if (!done) return i;

        if (m_chunk_state == ChunkState::Size) {
            std::string size = line.substr(0, line.find(';'));
            size = trim_ows(size);
            if (size.empty() || size.size() > 16) { fail(400, "Invalid chunk size"); return i; }
            std::uint64_t n = 0;

            for (char c : size) {
                int v;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                else { fail(400, "Invalid chunk size"); return i; }
                n = (n << 4) | static_cast<std::uint64_t>(v);
            }

            if (n == 0) {
                m_chunk_state = ChunkState::Trailers;
            } else {
                m_remaining = n;
                m_chunk_state = ChunkState::Data;
            }

            continue;
        }

        if (m_chunk_state == ChunkState::DataEnd) {
            if (!line.empty()) { fail(400, "Missing CRLF after chunk data"); return i; }
            m_chunk_state = ChunkState::Size;
            continue;
        }

        if (line.empty()) {
            complete();
            continue;
        }

        m_trailer_size += line.size();
        if (m_trailer_size > m_limits.max_head_size) { fail(431, "Trailers too large"); return i; }
        if (!parse_header_line(line, m_trailers)) return i;
    }

    return i;
}

} // namespace detail

} // namespace http
} // namespace networking
} // namespace fizmo
