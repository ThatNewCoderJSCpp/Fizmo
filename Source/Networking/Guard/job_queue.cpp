#include "fizmo_library.hpp"
#include "job_queue.hpp"
#include "../../Crypto/encoding.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace fizmo {
namespace networking {
namespace guard {

namespace {

constexpr std::size_t kMaxErrorBytes = 1024;

std::string clip(std::string s) {
    if (s.size() > kMaxErrorBytes) {
        s.resize(kMaxErrorBytes);
        s += "...";
    }

    return s;
}

std::string ms_text(std::chrono::milliseconds ms) {
    return std::to_string(ms.count()) + " ms";
}

} // namespace

JobRejected::JobRejected(std::string job, std::vector<SchemaIssue> issues)
    : std::invalid_argument("job '" + job + "' was rejected: " + describe(issues)), m_job(std::move(job)), m_issues(std::move(issues)) {}

std::chrono::milliseconds RetryPolicy::delay_for(std::uint32_t attempt, double random01) const {
    if (attempt < 1) attempt = 1;
    double ms = static_cast<double>(initial.count()) * std::pow(multiplier, static_cast<double>(attempt - 1));
    ms = std::min(ms, static_cast<double>(maximum.count()));
    const double spread = ms * jitter;
    ms += (random01 * 2.0 - 1.0) * spread;
    if (ms < 0.0) ms = 0.0;
    return std::chrono::milliseconds(static_cast<std::int64_t>(ms));
}

std::vector<SchemaIssue> RetryPolicy::problems(std::uint16_t attempt_limit, std::chrono::milliseconds backoff_limit) const {
    std::vector<SchemaIssue> out;
    if (attempt_limit < 1 || attempt_limit > kMaxJobAttempts) attempt_limit = kMaxJobAttempts;
    if (max_attempts < 1) out.push_back({ "retry.max_attempts", "must be at least 1" });
    if (max_attempts > attempt_limit) out.push_back({ "retry.max_attempts", "is " + std::to_string(max_attempts) + "; the limit is " + std::to_string(attempt_limit) });
    if (initial.count() < 0) out.push_back({ "retry.initial", "can't be negative" });
    if (maximum.count() < 0) out.push_back({ "retry.maximum", "can't be negative" });
    if (maximum < initial) out.push_back({ "retry.maximum", "must be at least retry.initial" });
    if (maximum > backoff_limit) out.push_back({ "retry.maximum", "is " + ms_text(maximum) + "; the limit is " + ms_text(backoff_limit) });
    if (!(multiplier >= 1.0 && multiplier <= 10.0)) out.push_back({ "retry.multiplier", "must be from 1 to 10" });
    if (!(jitter >= 0.0 && jitter <= 1.0)) out.push_back({ "retry.jitter", "must be from 0 to 1" });
    return out;
}

const char* to_string(JobState state) noexcept {
    switch (state) {
        case JobState::Queued:    return "queued";
        case JobState::Running:   return "running";
        case JobState::Succeeded: return "succeeded";
        case JobState::Dead:      return "dead";
        default:                  return "cancelled";
    }
}

bool valid_job_name(const std::string& name) noexcept {
    if (name.empty() || name.size() > 128) return false;

    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':' || c == '-';
        if (!ok) return false;
    }

    return true;
}

bool JobQueue::Order::operator()(const Item* a, const Item* b) const noexcept {
    if (a->record.priority != b->record.priority) return a->record.priority > b->record.priority;
    if (a->run_at != b->run_at) return a->run_at < b->run_at;
    return a->seq < b->seq;
}

JobQueue::JobQueue(std::size_t workers, std::shared_ptr<Logger> logger)
    : m_workers(workers ? workers : 1), m_logger(std::move(logger)) {}

JobQueue::JobQueue(std::size_t workers, JobQueueLimits limits, std::shared_ptr<Logger> logger)
    : m_workers(workers ? workers : 1), m_logger(std::move(logger)) {
    set_limits(std::move(limits));
}

JobQueue::~JobQueue() {
    stop(false);
}

double JobQueue::random01() {
    thread_local std::mt19937_64 rng{ std::random_device{}() };
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
}

void JobQueue::set_limits(JobQueueLimits limits) {
    std::vector<SchemaIssue> bad;
    if (limits.max_attempts < 1 || limits.max_attempts > kMaxJobAttempts) bad.push_back({ "limits.max_attempts", "must be from 1 to " + std::to_string(kMaxJobAttempts) });
    if (limits.max_queued < 1) bad.push_back({ "limits.max_queued", "must be at least 1" });
    if (limits.max_delay.count() < 0) bad.push_back({ "limits.max_delay", "can't be negative" });
    if (limits.max_backoff.count() < 0) bad.push_back({ "limits.max_backoff", "can't be negative" });
    if (limits.min_priority > limits.max_priority) bad.push_back({ "limits.min_priority", "must not be above limits.max_priority" });
    if (!bad.empty()) throw JobRejected("limits", std::move(bad));
    std::lock_guard<std::mutex> lk(m_mutex);
    m_limits = std::move(limits);
}

JobQueueLimits JobQueue::limits() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_limits;
}

void JobQueue::register_handler(const std::string& name, JobHandler handler) {
    if (!valid_job_name(name)) throw JobRejected(name, { { "name", "must be 1 to 128 letters, digits, '_', '.', ':' or '-'" } });
    if (!handler) throw JobRejected(name, { { "handler", "is empty" } });
    std::lock_guard<std::mutex> lk(m_mutex);
    m_handlers[name] = std::move(handler);
}

void JobQueue::register_handler(const std::string& name, JobHandler handler, JobSchema schema) {
    register_handler(name, std::move(handler));
    set_schema(name, std::move(schema));
}

void JobQueue::set_schema(const std::string& name, JobSchema schema) {
    if (!valid_job_name(name)) throw JobRejected(name, { { "name", "must be 1 to 128 letters, digits, '_', '.', ':' or '-'" } });
    std::lock_guard<std::mutex> lk(m_mutex);
    m_schemas[name] = std::make_shared<const JobSchema>(std::move(schema));
}

bool JobQueue::remove_schema(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_schemas.erase(name) > 0;
}

bool JobQueue::has_schema(const std::string& name) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_schemas.count(name) > 0;
}

std::vector<SchemaIssue> JobQueue::check_locked(const std::string& name, const std::string& payload, const JobOptions& options, std::shared_ptr<const JobSchema>& schema) const {
    std::vector<SchemaIssue> out;
    const JobQueueLimits& l = m_limits;

    if (!valid_job_name(name)) {
        out.push_back({ "name", "must be 1 to 128 letters, digits, '_', '.', ':' or '-'" });
        return out;
    }

    if (l.require_handler && !m_handlers.count(name)) out.push_back({ "name", "no handler is registered for '" + name + "'" });

    for (auto& p : options.retry.problems(l.max_attempts, l.max_backoff)) out.push_back(std::move(p));
    if (options.delay.count() < 0) out.push_back({ "delay", "can't be negative" });
    if (options.delay > l.max_delay) out.push_back({ "delay", "is " + ms_text(options.delay) + "; the limit is " + ms_text(l.max_delay) });
    if (options.priority < l.min_priority || options.priority > l.max_priority) out.push_back({ "priority", "must be from " + std::to_string(l.min_priority) + " to " + std::to_string(l.max_priority) });

    if (options.unique_key.size() > l.max_unique_key_bytes) out.push_back({ "unique_key", "is longer than " + std::to_string(l.max_unique_key_bytes) + " bytes" });
    else if (!valid_utf8(options.unique_key)) out.push_back({ "unique_key", "is not valid UTF-8" });
    else {
        for (unsigned char c : options.unique_key) {
            if (c < 0x20 || c == 0x7f) { out.push_back({ "unique_key", "has control characters" }); break; }
        }
    }

    if (payload.size() > l.max_payload_bytes) out.push_back({ "$", "payload is " + std::to_string(payload.size()) + " bytes; the queue's limit is " + std::to_string(l.max_payload_bytes) });

    const auto s = m_schemas.find(name);
    if (s != m_schemas.end()) schema = s->second;
    else if (l.require_schema) out.push_back({ "name", "no payload schema is set for '" + name + "'" });
    return out;
}

std::vector<SchemaIssue> JobQueue::check(const std::string& name, const std::string& payload, const JobOptions& options) const {
    std::shared_ptr<const JobSchema> schema;
    std::vector<SchemaIssue> issues;
    std::size_t limit = 0;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        issues = check_locked(name, payload, options, schema);
        limit = m_limits.max_payload_bytes;
    }

    if (schema && payload.size() <= limit) {
        for (auto& i : schema->validate(payload)) issues.push_back(std::move(i));
    }

    return issues;
}

void JobQueue::reject(const std::string& name, const std::vector<SchemaIssue>& issues) {
    RejectCallback cb;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        ++m_stats.rejected;
        cb = m_on_rejected;
    }

    if (m_logger) m_logger->warn("job rejected", { field("job", name), field("problems", describe(issues)) });

    if (cb) {
        try { cb(name, issues); } catch (...) {}
    }
}

EnqueueResult JobQueue::try_enqueue(const std::string& name, const std::string& payload, const JobOptions& options) {
    EnqueueResult r;
    r.issues = check(name, payload, options);

    if (!r.issues.empty()) {
        reject(name, r.issues);
        return r;
    }

    {
        std::lock_guard<std::mutex> lk(m_mutex);

        if (!options.unique_key.empty()) {
            const auto u = m_unique.find(options.unique_key);

            if (u != m_unique.end()) {
                r.ok = true;
                r.duplicate = true;
                r.id = u->second;
                return r;
            }
        }

        if (m_ready.size() + m_delayed.size() >= m_limits.max_queued) {
            r.issues.push_back({ "", "the queue is full (" + std::to_string(m_limits.max_queued) + " jobs waiting)" });
        } else {
            auto item = std::make_unique<Item>();
            r.id = crypto::uuid_v4();
            item->record.id = r.id;
            item->record.name = name;
            item->record.payload = payload;
            item->record.unique_key = options.unique_key;
            item->record.priority = options.priority;
            item->record.max_attempts = options.retry.max_attempts;
            item->record.enqueued = WallClock::now();
            item->retry = options.retry;
            item->run_at = SteadyClock::now() + options.delay;
            item->seq = ++m_seq;
            if (!options.unique_key.empty()) m_unique[options.unique_key] = r.id;
            Item* raw = item.get();
            m_items.emplace(r.id, std::move(item));
            schedule_locked(raw);
            r.ok = true;
        }
    }

    if (!r.ok) {
        reject(name, r.issues);
        return r;
    }

    m_cv.notify_one();
    return r;
}

std::string JobQueue::enqueue(const std::string& name, const std::string& payload, const JobOptions& options) {
    EnqueueResult r = try_enqueue(name, payload, options);
    if (!r.ok) throw JobRejected(name, std::move(r.issues));
    return r.id;
}

void JobQueue::schedule_locked(Item* item) {
    if (item->run_at <= SteadyClock::now()) m_ready.insert(item);
    else m_delayed.emplace(item->run_at, item);
}

void JobQueue::unschedule_locked(Item* item) {
    if (m_ready.erase(item)) return;
    auto range = m_delayed.equal_range(item->run_at);

    for (auto it = range.first; it != range.second; ++it) {
        if (it->second == item) { m_delayed.erase(it); return; }
    }
}

void JobQueue::start() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_running.load()) return;
    m_stopping = false;
    m_drain = false;
    m_running = true;

    for (std::size_t i = 0; i < m_workers; ++i) {
        m_threads.emplace_back([this] { worker(); });
    }
}

void JobQueue::stop(bool drain) {
    std::vector<std::thread> threads;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_running.load()) return;
        m_drain = drain;
        m_stopping = true;
        threads.swap(m_threads);
    }

    m_cv.notify_all();

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    m_running = false;
    m_idle_cv.notify_all();
}

bool JobQueue::wait_idle(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lk(m_mutex);

    return m_idle_cv.wait_for(lk, timeout, [&] { return m_active == 0 && m_ready.empty() && m_delayed.empty(); });
}

JobQueue::Item* JobQueue::next_ready(SteadyTime now, SteadyTime& wake) {
    while (!m_delayed.empty() && m_delayed.begin()->first <= now) {
        m_ready.insert(m_delayed.begin()->second);
        m_delayed.erase(m_delayed.begin());
    }

    wake = m_delayed.empty() ? SteadyTime::max() : m_delayed.begin()->first;
    return m_ready.empty() ? nullptr : *m_ready.begin();
}

void JobQueue::finish_locked(Item& item, JobState state) {
    item.record.state = state;
    item.record.finished = WallClock::now();
    if (!item.record.unique_key.empty()) m_unique.erase(item.record.unique_key);

    if (state == JobState::Succeeded || state == JobState::Cancelled) {
        m_finished.push_back(item.record.id);

        while (m_finished.size() > 1000) {
            m_items.erase(m_finished.front());
            m_finished.pop_front();
        }
    }
}

void JobQueue::bury_locked(const std::string& id) {
    auto it = m_items.find(id);
    if (it == m_items.end()) return;
    std::unique_ptr<Item> item = std::move(it->second);
    m_items.erase(it);
    finish_locked(*item, JobState::Dead);
    m_dead.push_back(std::move(item));
    m_dead_index[id] = std::prev(m_dead.end());

    while (m_limits.dead_letter_capacity && m_dead.size() > m_limits.dead_letter_capacity) {
        const std::string old = m_dead.front()->record.id;
        if (m_logger) m_logger->warn("dead letter dropped; the dead-letter queue is full", { field("job_id", old), field("job", m_dead.front()->record.name) });
        m_dead_index.erase(old);
        m_dead.pop_front();
        ++m_stats.dead_evicted;
    }

    if (!m_limits.dead_letter_capacity) {
        m_dead_index.erase(id);
        m_dead.pop_back();
        ++m_stats.dead_evicted;
    }
}

void JobQueue::worker() {
    std::unique_lock<std::mutex> lk(m_mutex);

    for (;;) {
        const SteadyTime now = SteadyClock::now();
        SteadyTime wake;
        Item* item = next_ready(now, wake);

        if (m_stopping.load() && (!m_drain || !item)) break;

        if (!item) {
            if (wake == SteadyTime::max()) m_cv.wait(lk);
            else m_cv.wait_until(lk, wake);
            continue;
        }

        m_ready.erase(m_ready.begin());
        item->record.state = JobState::Running;
        ++item->record.attempts;
        ++m_active;

        JobContext ctx;
        ctx.id = item->record.id;
        ctx.name = item->record.name;
        ctx.payload = item->record.payload;
        ctx.attempt = item->record.attempts;
        ctx.max_attempts = item->record.max_attempts;
        ctx.last_error = item->record.last_error;
        ctx.stopping = &m_stopping;

        const auto h = m_handlers.find(item->record.name);
        JobHandler handler = h == m_handlers.end() ? JobHandler() : h->second;
        const std::string id = item->record.id;
        lk.unlock();

        JobResult result;

        if (!handler) {
            result = JobResult::fail("no handler registered for job '" + ctx.name + "'");
        } else {
            try {
                result = handler(ctx);
            } catch (const std::exception& e) {
                result = JobResult::retry(std::string("exception: ") + e.what());
            } catch (...) {
                result = JobResult::retry("unknown exception");
            }
        }

        lk.lock();
        --m_active;
        const auto found = m_items.find(id);

        if (found != m_items.end()) {
            Item& it = *found->second;
            JobCallback cb;
            JobRecord copy;

            if (result.outcome == JobOutcome::Success) {
                it.record.last_error.clear();
                finish_locked(it, JobState::Succeeded);
                ++m_stats.succeeded;
                cb = m_on_success;
                copy = it.record;
            } else {
                it.record.last_error = clip(result.error);
                it.record.errors.push_back(it.record.last_error);

                if (result.outcome == JobOutcome::Retry && it.record.attempts < it.record.max_attempts) {
                    it.record.state = JobState::Queued;
                    auto backoff = result.retry_after.count() > 0 ? result.retry_after : it.retry.delay_for(it.record.attempts, random01());
                    backoff = std::min(backoff, m_limits.max_backoff);
                    it.run_at = SteadyClock::now() + backoff;
                    it.seq = ++m_seq;
                    schedule_locked(&it);
                    ++m_stats.retried;
                    if (m_logger) m_logger->warn("job retry scheduled", { field("job_id", it.record.id), field("job", it.record.name), field("attempt", static_cast<std::int64_t>(it.record.attempts)), field("max_attempts", static_cast<std::int64_t>(it.record.max_attempts)), field("error", it.record.last_error), field("retry_in_ms", static_cast<std::int64_t>(backoff.count())) });
                } else {
                    if (m_logger) m_logger->error("job moved to dead letters", { field("job_id", it.record.id), field("job", it.record.name), field("attempts", static_cast<std::int64_t>(it.record.attempts)), field("error", it.record.last_error) });
                    ++m_stats.dead;
                    cb = m_on_dead;
                    copy = it.record;
                    copy.state = JobState::Dead;
                    copy.finished = WallClock::now();
                    bury_locked(id);
                }
            }

            if (cb) {
                lk.unlock();
                try { cb(copy); } catch (...) {}
                lk.lock();
            }
        }

        m_idle_cv.notify_all();
        m_cv.notify_one();
    }
}

bool JobQueue::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_items.find(id);
    if (it == m_items.end() || it->second->record.state != JobState::Queued) return false;
    unschedule_locked(it->second.get());
    finish_locked(*it->second, JobState::Cancelled);
    ++m_stats.cancelled;
    m_idle_cv.notify_all();
    return true;
}

bool JobQueue::retry_dead(const std::string& id) {
    std::shared_ptr<const JobSchema> schema;
    std::string name, payload;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto d = m_dead_index.find(id);
        if (d == m_dead_index.end()) return false;
        const Item& item = **d->second;
        name = item.record.name;
        payload = item.record.payload;
        if (m_limits.require_handler && !m_handlers.count(name)) return false;
        const auto s = m_schemas.find(name);
        if (s != m_schemas.end()) schema = s->second;
    }

    if (schema && !schema->accepts(payload)) return false;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto d = m_dead_index.find(id);
        if (d == m_dead_index.end()) return false;
        if (m_ready.size() + m_delayed.size() >= m_limits.max_queued) return false;
        Item& item = **d->second;

        if (!item.record.unique_key.empty()) {
            if (m_unique.count(item.record.unique_key)) return false;
            m_unique[item.record.unique_key] = id;
        }

        std::unique_ptr<Item> owned = std::move(*d->second);
        m_dead.erase(d->second);
        m_dead_index.erase(d);
        owned->record.state = JobState::Queued;
        owned->record.attempts = 0;
        owned->record.finished = WallTime{};
        owned->run_at = SteadyClock::now();
        owned->seq = ++m_seq;
        Item* raw = owned.get();
        m_items[id] = std::move(owned);
        schedule_locked(raw);
    }

    m_cv.notify_one();
    return true;
}

std::size_t JobQueue::retry_all_dead() {
    std::vector<std::string> ids;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (const auto& d : m_dead) ids.push_back(d->record.id);
    }

    std::size_t n = 0;
    for (const auto& id : ids) if (retry_dead(id)) ++n;
    return n;
}

bool JobQueue::discard_dead(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto d = m_dead_index.find(id);
    if (d == m_dead_index.end()) return false;
    m_dead.erase(d->second);
    m_dead_index.erase(d);
    return true;
}

std::size_t JobQueue::clear_dead() {
    std::lock_guard<std::mutex> lk(m_mutex);
    const std::size_t n = m_dead.size();
    m_dead.clear();
    m_dead_index.clear();
    return n;
}

std::size_t JobQueue::dead_count() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_dead.size();
}

std::optional<JobRecord> JobQueue::find(const std::string& id) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_items.find(id);
    if (it != m_items.end()) return it->second->record;
    const auto d = m_dead_index.find(id);
    if (d != m_dead_index.end()) return (*d->second)->record;
    return std::nullopt;
}

std::vector<JobRecord> JobQueue::dead_letters() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<JobRecord> out;
    out.reserve(m_dead.size());
    for (auto it = m_dead.rbegin(); it != m_dead.rend(); ++it) out.push_back((*it)->record);
    return out;
}

void JobQueue::purge_finished(std::size_t keep) {
    std::lock_guard<std::mutex> lk(m_mutex);

    while (m_finished.size() > keep) {
        m_items.erase(m_finished.front());
        m_finished.pop_front();
    }
}

JobStats JobQueue::stats() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    JobStats s = m_stats;
    s.queued = m_ready.size() + m_delayed.size();
    s.running = m_active;
    s.dead = m_dead.size();
    return s;
}

void JobQueue::on_dead(JobCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_dead = std::move(cb);
}

void JobQueue::on_success(JobCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_success = std::move(cb);
}

void JobQueue::on_rejected(RejectCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_on_rejected = std::move(cb);
}

} // namespace guard
} // namespace networking
} // namespace fizmo
