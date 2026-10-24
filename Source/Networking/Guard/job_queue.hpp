#ifndef FIZMO_GUARD_JOB_QUEUE_HPP
#define FIZMO_GUARD_JOB_QUEUE_HPP

#include "guard_types.hpp"
#include "job_schema.hpp"
#include "logger.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

constexpr std::uint16_t kMaxJobAttempts = 100;

class JobRejected : public std::invalid_argument {
public:
    JobRejected(std::string job, std::vector<SchemaIssue> issues);

    const std::string& job() const noexcept { return m_job; }
    const std::vector<SchemaIssue>& issues() const noexcept { return m_issues; }

private:
    std::string              m_job;
    std::vector<SchemaIssue> m_issues;
};

struct RetryPolicy {
    std::uint16_t             max_attempts = 5;
    std::chrono::milliseconds initial{1000};
    std::chrono::milliseconds maximum{std::chrono::minutes(5)};
    double                    multiplier = 2.0;
    double                    jitter = 0.2;

    std::chrono::milliseconds delay_for(std::uint32_t attempt, double random01) const;
    std::vector<SchemaIssue> problems(std::uint16_t attempt_limit = kMaxJobAttempts, std::chrono::milliseconds backoff_limit = std::chrono::hours(24)) const;
};

struct JobOptions {
    RetryPolicy               retry;
    std::chrono::milliseconds delay{0};
    int                       priority = 0;
    std::string               unique_key;
};

struct JobQueueLimits {
    std::uint16_t             max_attempts = kMaxJobAttempts;
    std::size_t               max_queued = 100000;
    std::size_t               dead_letter_capacity = 10000;
    std::size_t               max_payload_bytes = 64 * 1024;
    std::size_t               max_unique_key_bytes = 256;
    std::chrono::milliseconds max_delay{std::chrono::hours(24 * 7)};
    std::chrono::milliseconds max_backoff{std::chrono::hours(24)};
    int                       min_priority = -1000;
    int                       max_priority = 1000;
    bool                      require_handler = true;
    bool                      require_schema = false;
};

enum class JobState {
    Queued,
    Running,
    Succeeded,
    Dead,
    Cancelled
};

const char* to_string(JobState state) noexcept;

struct JobContext {
    std::string   id;
    std::string   name;
    std::string   payload;
    std::uint16_t attempt = 1;
    std::uint16_t max_attempts = 1;
    std::string   last_error;
    const std::atomic<bool>* stopping = nullptr;

    bool should_stop() const noexcept { return stopping && stopping->load(); }
    bool last_attempt() const noexcept { return attempt >= max_attempts; }
};

enum class JobOutcome {
    Success,
    Retry,
    Fail
};

struct JobResult {
    JobOutcome                outcome = JobOutcome::Success;
    std::string               error;
    std::chrono::milliseconds retry_after{0};

    static JobResult ok() { return JobResult(); }
    static JobResult retry(std::string error, std::chrono::milliseconds after = std::chrono::milliseconds(0)) { return { JobOutcome::Retry, std::move(error), after }; }
    static JobResult fail(std::string error) { return { JobOutcome::Fail, std::move(error), std::chrono::milliseconds(0) }; }
};

using JobHandler = std::function<JobResult(const JobContext&)>;

struct JobRecord {
    std::string                id;
    std::string                name;
    std::string                payload;
    std::string                unique_key;
    int                        priority = 0;
    std::uint16_t              attempts = 0;
    std::uint16_t              max_attempts = 0;
    JobState                   state = JobState::Queued;
    std::string                last_error;
    std::vector<std::string>   errors;
    WallTime                   enqueued{};
    WallTime                   finished{};
};

struct JobStats {
    std::size_t queued = 0;
    std::size_t running = 0;
    std::size_t succeeded = 0;
    std::size_t retried = 0;
    std::size_t dead = 0;
    std::size_t cancelled = 0;
    std::size_t rejected = 0;
    std::size_t dead_evicted = 0;
};

struct EnqueueResult {
    bool                     ok = false;
    bool                     duplicate = false;
    std::string              id;
    std::vector<SchemaIssue> issues;

    explicit operator bool() const noexcept { return ok; }
    std::string message() const { return describe(issues); }
};

bool valid_job_name(const std::string& name) noexcept;

class JobQueue {
public:
    using JobCallback    = std::function<void(const JobRecord& job)>;
    using RejectCallback = std::function<void(const std::string& job, const std::vector<SchemaIssue>& issues)>;

    explicit JobQueue(std::size_t workers = 1, std::shared_ptr<Logger> logger = nullptr);
    JobQueue(std::size_t workers, JobQueueLimits limits, std::shared_ptr<Logger> logger = nullptr);
    ~JobQueue();

    JobQueue(const JobQueue&) = delete;
    JobQueue& operator=(const JobQueue&) = delete;

    void set_limits(JobQueueLimits limits);
    JobQueueLimits limits() const;

    void register_handler(const std::string& name, JobHandler handler);
    void register_handler(const std::string& name, JobHandler handler, JobSchema schema);
    void set_schema(const std::string& name, JobSchema schema);
    bool remove_schema(const std::string& name);
    bool has_schema(const std::string& name) const;

    std::vector<SchemaIssue> check(const std::string& name, const std::string& payload = {}, const JobOptions& options = JobOptions()) const;
    EnqueueResult try_enqueue(const std::string& name, const std::string& payload = {}, const JobOptions& options = JobOptions());
    std::string enqueue(const std::string& name, const std::string& payload = {}, const JobOptions& options = JobOptions());

    void start();
    void stop(bool drain = false);
    bool running() const noexcept { return m_running.load(); }
    bool wait_idle(std::chrono::milliseconds timeout);

    bool cancel(const std::string& id);
    std::optional<JobRecord> find(const std::string& id) const;
    void purge_finished(std::size_t keep = 1000);
    JobStats stats() const;

    std::vector<JobRecord> dead_letters() const;
    std::size_t dead_count() const;
    bool retry_dead(const std::string& id);
    std::size_t retry_all_dead();
    bool discard_dead(const std::string& id);
    std::size_t clear_dead();

    void on_dead(JobCallback cb);
    void on_success(JobCallback cb);
    void on_rejected(RejectCallback cb);

private:
    struct Item {
        JobRecord     record;
        RetryPolicy   retry;
        SteadyTime    run_at{};
        std::uint64_t seq = 0;
    };

    struct Order {
        bool operator()(const Item* a, const Item* b) const noexcept;
    };

    using DeadList = std::list<std::unique_ptr<Item>>;

    std::size_t                                             m_workers;
    std::shared_ptr<Logger>                                 m_logger;
    JobQueueLimits                                          m_limits;
    mutable std::mutex                                      m_mutex;
    std::condition_variable                                 m_cv;
    std::condition_variable                                 m_idle_cv;
    std::unordered_map<std::string, JobHandler>             m_handlers;
    std::unordered_map<std::string, std::shared_ptr<const JobSchema>> m_schemas;
    std::unordered_map<std::string, std::unique_ptr<Item>>  m_items;
    std::set<Item*, Order>                                  m_ready;
    std::multimap<SteadyTime, Item*>                        m_delayed;
    DeadList                                                m_dead;
    std::unordered_map<std::string, DeadList::iterator>     m_dead_index;
    std::unordered_map<std::string, std::string>            m_unique;
    std::deque<std::string>                                 m_finished;
    std::vector<std::thread>                                m_threads;
    std::atomic<bool>                                       m_running{false};
    std::atomic<bool>                                       m_stopping{false};
    bool                                                    m_drain = false;
    std::size_t                                             m_active = 0;
    std::uint64_t                                           m_seq = 0;
    JobStats                                                m_stats;
    JobCallback                                             m_on_dead;
    JobCallback                                             m_on_success;
    RejectCallback                                          m_on_rejected;

    void worker();
    Item* next_ready(SteadyTime now, SteadyTime& wake);
    void schedule_locked(Item* item);
    void unschedule_locked(Item* item);
    void finish_locked(Item& item, JobState state);
    void bury_locked(const std::string& id);
    std::vector<SchemaIssue> check_locked(const std::string& name, const std::string& payload, const JobOptions& options, std::shared_ptr<const JobSchema>& schema) const;
    void reject(const std::string& name, const std::vector<SchemaIssue>& issues);
    double random01();
};

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_JOB_QUEUE_HPP
