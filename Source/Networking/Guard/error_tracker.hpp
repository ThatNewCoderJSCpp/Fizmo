#ifndef FIZMO_GUARD_ERROR_TRACKER_HPP
#define FIZMO_GUARD_ERROR_TRACKER_HPP

#include "guard_types.hpp"
#include "logger.hpp"

#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

struct ErrorEvent {
    std::string                        type;
    std::string                        message;
    std::string                        where;
    std::string                        request_id;
    std::map<std::string, std::string> tags;
};

struct ErrorGroup {
    std::string                        id;
    std::string                        type;
    std::string                        message;
    std::string                        where;
    std::size_t                        count = 0;
    std::size_t                        recent = 0;
    WallTime                           first_seen{};
    WallTime                           last_seen{};
    std::string                        last_request_id;
    std::map<std::string, std::string> last_tags;
    bool                               resolved = false;
};

enum class ErrorAlertKind {
    NewGroup,
    Spike,
    Regression
};

const char* to_string(ErrorAlertKind kind) noexcept;

struct ErrorTrackerPolicy {
    std::chrono::seconds window          = std::chrono::minutes(5);
    std::size_t          spike_threshold = 20;
    std::chrono::seconds spike_cooldown  = std::chrono::minutes(30);
    std::size_t          max_groups      = 2000;
};

class ErrorTracker {
public:
    using AlertCallback = std::function<void(ErrorAlertKind kind, const ErrorGroup& group)>;

    explicit ErrorTracker(ErrorTrackerPolicy policy = ErrorTrackerPolicy(), NowFn now = default_now(), WallNowFn wall = default_wall_now());

    std::string capture(const ErrorEvent& event);
    std::string capture_exception(const std::exception& e, const std::string& where = {}, const std::string& request_id = {});
    std::string capture_current(const std::string& where = {}, const std::string& request_id = {});

    std::vector<ErrorGroup> groups() const;
    std::optional<ErrorGroup> group(const std::string& id) const;
    bool resolve(const std::string& id);
    void clear();

    void on_alert(AlertCallback cb);
    void set_logger(std::shared_ptr<Logger> logger);

    static std::string normalize_message(const std::string& message);
    static std::string group_id(const std::string& type, const std::string& message, const std::string& where);

private:
    struct Entry {
        ErrorGroup             group;
        std::deque<SteadyTime> hits;
        SteadyTime             last_spike{};
        bool                   spiking = false;
    };

    ErrorTrackerPolicy                     m_policy;
    NowFn                                  m_now;
    WallNowFn                              m_wall;
    mutable std::mutex                     m_mutex;
    std::unordered_map<std::string, Entry> m_entries;
    AlertCallback                          m_on_alert;
    std::shared_ptr<Logger>                m_logger;
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_ERROR_TRACKER_HPP
