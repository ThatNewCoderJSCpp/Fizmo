#include "fizmo_library.hpp"
#include "error_tracker.hpp"
#include "../../Crypto/hash.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <typeinfo>

#if defined(__GNUG__)
    #include <cxxabi.h>
#endif // defined(__GNUG__)

namespace fizmo {
namespace networking {
namespace guard {

namespace {

std::string type_name(const std::type_info& info) {
#if defined(__GNUG__)
    int status = 0;
    char* demangled = abi::__cxa_demangle(info.name(), nullptr, nullptr, &status);

    if (status == 0 && demangled) {
        std::string out(demangled);
        std::free(demangled);
        return out;
    }

    std::free(demangled);
#endif // defined(__GNUG__)
    return info.name();
}

} // namespace

const char* to_string(ErrorAlertKind kind) noexcept {
    switch (kind) {
        case ErrorAlertKind::NewGroup:   return "new";
        case ErrorAlertKind::Spike:      return "spike";
        default:                         return "regression";
    }
}

ErrorTracker::ErrorTracker(ErrorTrackerPolicy policy, NowFn now, WallNowFn wall)
    : m_policy(policy), m_now(now ? std::move(now) : default_now()), m_wall(wall ? std::move(wall) : default_wall_now()) {}

std::string ErrorTracker::normalize_message(const std::string& message) {
    std::string out;
    out.reserve(message.size());
    const std::size_t n = message.size();

    for (std::size_t i = 0; i < n;) {
        const unsigned char c = static_cast<unsigned char>(message[i]);

        if (std::isxdigit(c)) {
            std::size_t j = i;
            bool digit = false;
            while (j < n && (std::isxdigit(static_cast<unsigned char>(message[j])) || message[j] == '-')) {
                if (std::isdigit(static_cast<unsigned char>(message[j]))) digit = true;
                ++j;
            }

            const bool word_start = i == 0 || !std::isalnum(static_cast<unsigned char>(message[i - 1]));
            const bool word_end = j >= n || !std::isalnum(static_cast<unsigned char>(message[j]));

            if (digit && word_start && word_end) {
                out += '#';
                i = j;
                continue;
            }
        }

        if (c == '\'' || c == '"') {
            const std::size_t end = message.find(static_cast<char>(c), i + 1);

            if (end != std::string::npos && end - i <= 200) {
                out += static_cast<char>(c);
                out += '*';
                out += static_cast<char>(c);
                i = end + 1;
                continue;
            }
        }

        out += static_cast<char>(c);
        ++i;
    }

    if (out.size() > 300) out.resize(300);
    return out;
}

std::string ErrorTracker::group_id(const std::string& type, const std::string& message, const std::string& where) {
    crypto::SHA256 h;
    const std::uint8_t sep = 0;
    h.update(type);
    h.update(&sep, 1);
    h.update(normalize_message(message));
    h.update(&sep, 1);
    h.update(where);
    const auto d = h.finish();
    return crypto::to_hex(d.data(), 8);
}

std::string ErrorTracker::capture(const ErrorEvent& event) {
    const SteadyTime now = m_now();
    const WallTime wall = m_wall();
    const std::string type = event.type.empty() ? std::string("error") : event.type;
    const std::string id = group_id(type, event.message, event.where);
    std::vector<std::pair<ErrorAlertKind, ErrorGroup>> alerts;
    AlertCallback cb;
    std::shared_ptr<Logger> logger;
    ErrorGroup snapshot;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_entries.find(id);

        if (it == m_entries.end()) {
            if (m_policy.max_groups && m_entries.size() >= m_policy.max_groups) {
                auto victim = m_entries.begin();

                for (auto e = m_entries.begin(); e != m_entries.end(); ++e) {
                    if (e->second.group.last_seen < victim->second.group.last_seen) victim = e;
                }

                if (victim != m_entries.end()) m_entries.erase(victim);
            }

            Entry e;
            e.group.id = id;
            e.group.type = type;
            e.group.message = event.message;
            e.group.where = event.where;
            e.group.first_seen = wall;
            it = m_entries.emplace(id, std::move(e)).first;
            alerts.emplace_back(ErrorAlertKind::NewGroup, ErrorGroup());
        } else if (it->second.group.resolved) {
            it->second.group.resolved = false;
            alerts.emplace_back(ErrorAlertKind::Regression, ErrorGroup());
        }

        Entry& e = it->second;
        e.hits.push_back(now);
        while (!e.hits.empty() && now - e.hits.front() >= m_policy.window) e.hits.pop_front();
        ++e.group.count;
        e.group.recent = e.hits.size();
        e.group.last_seen = wall;
        e.group.last_request_id = event.request_id;
        e.group.last_tags = event.tags;

        if (m_policy.spike_threshold && e.hits.size() >= m_policy.spike_threshold) {
            if (!e.spiking || now - e.last_spike >= m_policy.spike_cooldown) {
                e.spiking = true;
                e.last_spike = now;
                alerts.emplace_back(ErrorAlertKind::Spike, ErrorGroup());
            }
        } else if (e.hits.size() < m_policy.spike_threshold / 2) {
            e.spiking = false;
        }

        for (auto& a : alerts) a.second = e.group;
        snapshot = e.group;
        cb = m_on_alert;
        logger = m_logger;
    }

    if (logger) {
        LogFields f = { field("error_group", id), field("error_type", type), field("count", static_cast<std::uint64_t>(snapshot.count)) };
        if (!event.where.empty()) f.push_back(field("where", event.where));
        if (!event.request_id.empty()) f.push_back(field("request_id", event.request_id));
        for (const auto& t : event.tags) f.push_back(field(t.first, t.second));
        logger->error(event.message, std::move(f));

        for (const auto& a : alerts) {
            if (a.first == ErrorAlertKind::NewGroup) continue;
            logger->warn(std::string("error ") + to_string(a.first), { field("error_group", id), field("recent", static_cast<std::uint64_t>(a.second.recent)) });
        }
    }

    if (cb) {
        for (const auto& a : alerts) cb(a.first, a.second);
    }

    return id;
}

std::string ErrorTracker::capture_exception(const std::exception& e, const std::string& where, const std::string& request_id) {
    ErrorEvent ev;
    ev.type = type_name(typeid(e));
    ev.message = e.what();
    ev.where = where;
    ev.request_id = request_id;
    return capture(ev);
}

std::string ErrorTracker::capture_current(const std::string& where, const std::string& request_id) {
    const std::exception_ptr p = std::current_exception();

    if (!p) {
        ErrorEvent ev;
        ev.type = "unknown";
        ev.message = "capture_current called with no active exception";
        ev.where = where;
        ev.request_id = request_id;
        return capture(ev);
    }

    try {
        std::rethrow_exception(p);
    } catch (const std::exception& e) {
        return capture_exception(e, where, request_id);
    } catch (...) {
        ErrorEvent ev;
        ev.type = "non-standard exception";
        ev.message = "unknown exception";
        ev.where = where;
        ev.request_id = request_id;
        return capture(ev);
    }
}

std::vector<ErrorGroup> ErrorTracker::groups() const {
    const SteadyTime now = m_now();
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<ErrorGroup> out;

    for (const auto& kv : m_entries) {
        ErrorGroup g = kv.second.group;
        g.recent = static_cast<std::size_t>(std::count_if(kv.second.hits.begin(), kv.second.hits.end(), [&](SteadyTime t) { return now - t < m_policy.window; }));
        out.push_back(std::move(g));
    }

    std::sort(out.begin(), out.end(), [](const ErrorGroup& a, const ErrorGroup& b) { return a.last_seen > b.last_seen; });
    return out;
}

std::optional<ErrorGroup> ErrorTracker::group(const std::string& id) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(id);
    if (it == m_entries.end()) return std::nullopt;
    return it->second.group;
}

bool ErrorTracker::resolve(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_entries.find(id);
    if (it == m_entries.end()) return false;
    it->second.group.resolved = true;
    it->second.hits.clear();
    it->second.spiking = false;
    return true;
}

void ErrorTracker::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_entries.clear();
}

void ErrorTracker::on_alert(AlertCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_alert = std::move(cb);
}

void ErrorTracker::set_logger(std::shared_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_logger = std::move(logger);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
