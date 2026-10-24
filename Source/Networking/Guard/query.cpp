#include "fizmo_library.hpp"
#include "query.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

bool ident_start(char c) noexcept {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool ident_char(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

std::string describe(const SqlValue& v, bool redact) {
    switch (v.index()) {
        case 0: return "NULL";
        case 1: return std::get<bool>(v) ? "TRUE" : "FALSE";
        case 2: return redact ? "<int>" : std::to_string(std::get<std::int64_t>(v));
        case 3: {
            if (redact) return "<real>";
            char buf[40];
            std::snprintf(buf, sizeof(buf), "%.17g", std::get<double>(v));
            return buf;
        }
        case 4: return redact ? "<text:" + std::to_string(std::get<std::string>(v).size()) + ">" : "'" + std::get<std::string>(v) + "'";
        default: return "<blob:" + std::to_string(std::get<SqlBlob>(v).size()) + ">";
    }
}

} // namespace

std::size_t Statement::count_placeholders(const std::string& sql, StatementMode mode) {
    const bool strict = mode == StatementMode::Strict;
    std::size_t count = 0;
    const std::size_t n = sql.size();

    for (std::size_t i = 0; i < n; ++i) {
        const char c = sql[i];

        if (c == '"' || c == '`') {
            const std::size_t end = sql.find(c, i + 1);
            if (end == std::string::npos) throw QueryError("unterminated quoted identifier");
            i = end;
            continue;
        }

        if (c == '\'') {
            if (strict) throw QueryError("string literals are not allowed in strict statements; bind the value with ? instead");
            std::size_t j = i + 1;

            for (;;) {
                if (j >= n) throw QueryError("unterminated string literal");
                if (sql[j] == '\'') {
                    if (j + 1 < n && sql[j + 1] == '\'') { j += 2; continue; }
                    break;
                }
                ++j;
            }

            i = j;
            continue;
        }

        if (c == '-' && i + 1 < n && sql[i + 1] == '-') {
            if (strict) throw QueryError("SQL comments are not allowed in strict statements");
            const std::size_t end = sql.find('\n', i);
            i = end == std::string::npos ? n : end;
            continue;
        }

        if (c == '/' && i + 1 < n && sql[i + 1] == '*') {
            if (strict) throw QueryError("SQL comments are not allowed in strict statements");
            const std::size_t end = sql.find("*/", i + 2);
            if (end == std::string::npos) throw QueryError("unterminated comment");
            i = end + 1;
            continue;
        }

        if (c == ';' && strict) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (!std::isspace(static_cast<unsigned char>(sql[j]))) throw QueryError("multiple statements are not allowed in strict statements");
            }
        }

        if (c == '?') ++count;
    }

    return count;
}

Statement::Statement(std::string sql, StatementMode mode)
    : m_sql(std::move(sql)), m_mode(mode) {
    m_placeholders = count_placeholders(m_sql, m_mode);
}

Statement::Statement(std::string sql, std::vector<SqlValue> params, StatementMode mode)
    : Statement(std::move(sql), mode) {
    bind_all(params);
}

Statement& Statement::bind(SqlValue value) {
    if (m_params.size() >= m_placeholders) throw QueryError("more values bound than the statement has placeholders");
    m_params.push_back(std::move(value));
    return *this;
}

Statement& Statement::bind_all(const std::vector<SqlValue>& values) {
    for (const auto& v : values) bind(v);
    return *this;
}

Statement& Statement::append(const std::string& fragment, const std::vector<SqlValue>& values) {
    if (!complete()) throw QueryError("bind every placeholder before appending");
    const std::size_t extra = count_placeholders(fragment, m_mode);
    if (extra != values.size()) throw QueryError("fragment has " + std::to_string(extra) + " placeholders but " + std::to_string(values.size()) + " values");
    const std::string joined = m_sql.empty() ? fragment : m_sql + " " + fragment;
    count_placeholders(joined, m_mode);
    m_sql = joined;
    m_placeholders += extra;
    m_params.insert(m_params.end(), values.begin(), values.end());
    return *this;
}

Statement& Statement::append_in(const std::string& column, const std::vector<SqlValue>& values) {
    const std::string col = identifier(column);
    if (values.empty()) return append("1 = 0");
    return append(col + " IN (" + placeholders(values.size()) + ")", values);
}

Statement& Statement::append_identifier(const std::string& name) {
    return append(identifier(name));
}

void Statement::require_complete() const {
    if (!complete()) throw QueryError("statement has " + std::to_string(m_placeholders) + " placeholders but " + std::to_string(m_params.size()) + " bound values");
}

std::string Statement::shape() const {
    return normalize_query_shape(m_sql);
}

std::string Statement::debug_string(bool redact) const {
    std::string out;
    std::size_t k = 0;
    const std::size_t n = m_sql.size();

    for (std::size_t i = 0; i < n; ++i) {
        const char c = m_sql[i];

        if (c == '"' || c == '`' || c == '\'') {
            const std::size_t end = m_sql.find(c, i + 1);
            const std::size_t stop = end == std::string::npos ? n - 1 : end;
            out.append(m_sql, i, stop - i + 1);
            i = stop;
            continue;
        }

        if (c == '?') {
            out += k < m_params.size() ? describe(m_params[k], redact) : std::string("?");
            ++k;
            continue;
        }

        out += c;
    }

    return out;
}

std::string Statement::placeholders(std::size_t count) {
    std::string out;

    for (std::size_t i = 0; i < count; ++i) {
        if (i) out += ", ";
        out += '?';
    }

    return out;
}

std::string Statement::identifier(const std::string& name) {
    if (name.empty() || name.size() > 128) throw QueryError("invalid identifier");
    bool start = true;

    for (char c : name) {
        if (c == '.') {
            if (start) throw QueryError("invalid identifier: " + name);
            start = true;
            continue;
        }

        if (start ? !ident_start(c) : !ident_char(c)) throw QueryError("invalid identifier: " + name);
        start = false;
    }

    if (start) throw QueryError("invalid identifier: " + name);
    return name;
}

std::string normalize_query_shape(const std::string& sql) {
    std::string tmp;
    tmp.reserve(sql.size());
    const std::size_t n = sql.size();
    bool space = false;

    for (std::size_t i = 0; i < n; ++i) {
        const char c = sql[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            space = true;
            continue;
        }

        if (space && !tmp.empty()) tmp += ' ';
        space = false;

        if (c == '"' || c == '`') {
            const std::size_t end = sql.find(c, i + 1);
            const std::size_t stop = end == std::string::npos ? n - 1 : end;
            tmp.append(sql, i, stop - i + 1);
            i = stop;
            continue;
        }

        if (c == '\'') {
            std::size_t j = i + 1;

            while (j < n) {
                if (sql[j] == '\'') {
                    if (j + 1 < n && sql[j + 1] == '\'') { j += 2; continue; }
                    break;
                }
                ++j;
            }

            tmp += '?';
            i = j;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) && (tmp.empty() || !ident_char(tmp.back()))) {
            std::size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(sql[j])) || sql[j] == '.')) ++j;
            tmp += '?';
            i = j - 1;
            continue;
        }

        tmp += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string out;
    out.reserve(tmp.size());

    for (std::size_t i = 0; i < tmp.size(); ++i) {
        if (tmp[i] != '?') { out += tmp[i]; continue; }
        std::size_t j = i + 1;
        bool list = false;

        for (;;) {
            std::size_t k = j;
            while (k < tmp.size() && tmp[k] == ' ') ++k;
            if (k >= tmp.size() || tmp[k] != ',') break;
            ++k;
            while (k < tmp.size() && tmp[k] == ' ') ++k;
            if (k >= tmp.size() || tmp[k] != '?') break;
            j = k + 1;
            list = true;
        }

        out += list ? "?+" : "?";
        i = j - 1;
    }

    while (!out.empty() && (out.back() == ';' || out.back() == ' ')) out.pop_back();
    return out;
}

const char* to_string(QueryWarningKind kind) noexcept {
    switch (kind) {
        case QueryWarningKind::RepeatedShape: return "n_plus_one";
        case QueryWarningKind::Slow:          return "slow_query";
        default:                              return "too_many_queries";
    }
}

QueryRecorder::QueryRecorder(QueryRecorderPolicy policy) : m_policy(policy) {}

void QueryRecorder::record(const Statement& statement, std::chrono::microseconds duration) {
    record_shape(statement.sql(), duration);
}

void QueryRecorder::record_shape(const std::string& sql, std::chrono::microseconds duration) {
    const std::string shape = normalize_query_shape(sql);
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_shapes.find(shape);

    if (it == m_shapes.end()) {
        it = m_shapes.emplace(shape, ShapeStats()).first;
        m_order.push_back(shape);
    }

    ++it->second.count;
    it->second.total += duration;
    it->second.slowest = std::max(it->second.slowest, duration);
    ++m_total;
    m_time += duration;
}

std::vector<QueryWarning> QueryRecorder::warnings() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<QueryWarning> out;

    for (const auto& shape : m_order) {
        const ShapeStats& s = m_shapes.at(shape);

        if (m_policy.repeat_threshold && s.count >= m_policy.repeat_threshold) {
            QueryWarning w;
            w.kind = QueryWarningKind::RepeatedShape;
            w.shape = shape;
            w.count = s.count;
            w.total = s.total;
            w.message = "same query ran " + std::to_string(s.count) + " times in one unit of work; batch it with IN (...) or a join";
            out.push_back(std::move(w));
        }

        if (m_policy.slow_query.count() > 0 && s.slowest >= m_policy.slow_query) {
            QueryWarning w;
            w.kind = QueryWarningKind::Slow;
            w.shape = shape;
            w.count = s.count;
            w.total = s.total;
            w.message = "slowest run took " + std::to_string(s.slowest.count() / 1000) + " ms";
            out.push_back(std::move(w));
        }
    }

    if (m_policy.max_queries && m_total > m_policy.max_queries) {
        QueryWarning w;
        w.kind = QueryWarningKind::TooMany;
        w.count = m_total;
        w.total = m_time;
        w.message = std::to_string(m_total) + " queries in one unit of work";
        out.push_back(std::move(w));
    }

    return out;
}

std::size_t QueryRecorder::total() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_total;
}

std::size_t QueryRecorder::count(const std::string& shape) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_shapes.find(normalize_query_shape(shape));
    return it == m_shapes.end() ? 0 : it->second.count;
}

std::chrono::microseconds QueryRecorder::total_time() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_time;
}

void QueryRecorder::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_shapes.clear();
    m_order.clear();
    m_total = 0;
    m_time = std::chrono::microseconds(0);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
