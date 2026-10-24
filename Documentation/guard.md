# Guard: server safety

`#include <networking.hpp>` gives `fizmo::networking::guard`, a set of safety pieces for servers. `Guard` ties them into an `http::Server`; each piece also works on its own.

```cpp
using namespace fizmo::networking;
namespace g = fizmo::networking::guard;

g::GuardOptions opts;
opts.route_limits.push_back({ "POST", "/login", g::RateLimitPolicy::per_minute(5, 5) });
g::Guard guard(opts);
guard.logger().add_sink(std::make_shared<g::StreamSink>());

auto server = https::create_server(tls, guard.wrap([&](http::IncomingMessage req, http::ServerResponse res) {
    if (!guard.require_auth(req, res) || !guard.require_csrf(req, res)) return;
    if (!guard.authorize(req, res, "recipe", id, g::Action::Update)) return;
    res.json("{\"ok\":true}");
}));
guard.start_maintenance();
```

## What `wrap` does for every request

- gives it a request ID (`X-Request-Id`) and a `RequestContext` (`Guard::context(req)`)
- finds the client IP (`X-Forwarded-For` only from trusted proxies)
- adds security headers: `nosniff`, frame denial, referrer policy, CSP, COOP, HSTS on HTTPS, and `Cache-Control: no-store`
- takes the JA4 TLS fingerprint and scores it against the User-Agent; blocked fingerprints get 403, and with `block_high_risk` so do likely bots
- rate limits per IP (token bucket) and per route, answering 429 with `Retry-After` and `RateLimit-*` headers
- locks out IPs whose 4xx rate is too high, with lockouts that grow each time
- catches exceptions, groups them in the `ErrorTracker` and answers 500 with only the request ID
- writes one structured access-log line when the response finishes, with secrets redacted

## Pieces

| Type | Does |
|---|---|
| `RateLimiter`, `TokenBucket` | token buckets: `capacity`, `initial_tokens`, `refill_per_second`, per-key overrides |
| `SlidingWindowLimiter` | at most N hits per window |
| `FailureTracker` | failure counts and ratios per key, slow-downs, escalating lockouts |
| `fingerprint(ClientHello)`, `FingerprintRegistry` | JA4 hashes, browser-vs-tool checks, block and trust lists |
| `DeviceRegistry` | known devices per user via a device cookie; new or changed devices are reported |
| `SessionStore` | hashed tokens, idle and absolute timeouts, rotation, CSRF tokens, logout everywhere |
| `make_set_cookie`, `clear_site_data_header` | safe cookies (`__Host-`, `Secure`, `HttpOnly`, `SameSite`) and `Clear-Site-Data` |
| `OwnershipGuard`, `IdSigner` | IDOR checks that answer 404, and signed IDs that can't be guessed |
| `Statement`, `QueryRecorder` | parameterized SQL that rejects literals and stacked statements; N+1 and slow-query warnings |
| `KeyedMutex`, `Versioned<T>`, `SingleFlight<V>` | per-key locks, compare-and-set, and one run for identical concurrent work |
| `IdempotencyStore` | `Idempotency-Key` replay, conflict and mismatch detection |
| `Logger` | JSON or text logs, sinks for streams, rotating files and memory, redaction |
| `ErrorTracker` | groups errors, alerts on new groups, spikes and regressions |
| `JobQueue`, `JobSchema` | background jobs that are checked before they're queued, with bounded retries, backoff, unique keys and a dead-letter queue |
| `BudgetMonitor` | budgets with warning and exceeded alerts, hard stops, gauges |
| `Debouncer`, `Throttler`, `KeyedDebouncer` | debouncing on the event loop |
| `EventStream`, `Broadcaster`, `ChangeSignal` | Server-Sent Events with replay, and waits that wake on change instead of polling |

## Logging in and out

`check_login(req, account)` applies an IP window and account lockouts; `login_failed` records a miss. `login(req, res, user, roles)` revokes any old session, recognizes the device, sets the session and device cookies and returns the CSRF token in `X-CSRF-Token`. `logout(req, res, everywhere, clear)` revokes the session on the server, expires its cookie and sends `Clear-Site-Data` (cookies and storage by default; pass `ClearSiteData::Storage` to keep the device cookie).

## Background jobs

A `JobQueue` runs jobs on worker threads. Every job is checked before it goes into the queue, so a job that can never run is refused at the door instead of failing later.

```cpp
g::JobSchema file;
file.field("name", g::FieldRule::string().non_empty())
    .field("bytes", g::FieldRule::integer().between(1, 1e6));

g::JobSchema email;
email.field("to", g::FieldRule::string().format(g::StringFormat::Email))
     .field("subject", g::FieldRule::string().length(1, 200))
     .field("files", g::FieldRule::array(g::FieldRule::object(file)).max_items(3).optional());

g::JobQueue jobs(2);
jobs.register_handler("email", send_email, email);
jobs.start();

g::JobOptions o;
o.retry.max_attempts = 3;
auto r = jobs.try_enqueue("email", R"({"to":"ann@example.com","subject":"Hi"})", o);
if (!r) log(r.message());        // "$.to: must be a valid email address; ..."
```

`enqueue` does the same checks and throws `JobRejected` (with `issues()`); `try_enqueue` returns an `EnqueueResult` instead, and `check` only reports. A job is refused when:

- its name isn't 1 to 128 letters, digits, `_`, `.`, `:` or `-`, or no handler is registered for it (`require_handler`)
- `retry.max_attempts` is 0 or above the limit. It is a `std::uint16_t`, and the limit is `JobQueueLimits::max_attempts` (at most `kMaxJobAttempts`, 100), so a job can't retry forever
- the retry backoff, delay or priority is out of range, or the unique key is too long
- the payload is bigger than `max_payload_bytes` (64 KB), or doesn't match the job's schema (`require_schema` refuses jobs that have none)
- the queue already holds `max_queued` jobs

### Payload schemas

A `JobSchema` describes a JSON object. Fields are required unless marked `optional()`, and fields that aren't listed are refused unless `allow_extra_fields()` is set. All problems are reported together, each with a path such as `$.files[0].bytes`.

| Rule | Checks |
|---|---|
| `FieldRule::string()`, `integer()`, `number()`, `boolean()`, `any()` | the type; `integer()` also needs a whole number that a double holds exactly |
| `FieldRule::array(items)`, `FieldRule::object(schema)` | each item, and nested fields |
| `min`, `max`, `between`, `positive` | number ranges |
| `min_length`, `max_length`, `length`, `non_empty` | string length in characters |
| `min_items`, `max_items`, `unique_items` | array sizes |
| `one_of({...})` | allowed values |
| `format(StringFormat::...)` | `Email`, `Uuid`, `Url` (http and https only, no `user@`), `Date`, `DateTime` (ISO 8601), `Identifier`, `Hex`, `Base64`, `Digits` |
| `nullable()`, `allow_control_chars()` | relaxations; control characters other than tab and newlines are refused by default |
| `check(fn)` | your own rule: return an error message, or an empty string |

On the schema itself, `at_least_one_of`, `only_one_of` and `all_or_none_of` relate fields, and `check(fn)` sees the whole payload. Every payload is also held to limits whatever its fields: valid UTF-8, no repeated keys, nesting up to `max_depth` (16), strings up to `max_string_length` (16,384 characters), arrays up to `max_array_items` (1,000) and the whole payload up to `max_bytes` (64 KB). `JobSchema::text(max_bytes)` is for plain-text payloads and `JobSchema::empty()` for jobs that take none.

### Retries and dead letters

A handler returns `JobResult::ok()`, `retry(error, after)` or `fail(error)`; a thrown exception counts as a retry. Retries wait with exponential backoff and jitter, capped by `max_backoff`. When the attempts run out, or on `fail`, the job moves to the dead-letter queue. That queue is separate from the main one, so workers never see dead jobs again and keep taking new work.

Each dead job keeps its payload and the error from every attempt (`JobRecord::errors`). `dead_letters()` lists them, newest first. `retry_dead(id)` checks the payload again and puts it back with fresh attempts, `retry_all_dead()` does all of them, and `discard_dead(id)` and `clear_dead()` remove them. The dead-letter queue holds `dead_letter_capacity` jobs (10,000); past that, the oldest are dropped and counted in `JobStats::dead_evicted`. `on_dead`, `on_success` and `on_rejected` report each outcome.

`wait_idle(timeout)` waits until nothing is running or waiting, including jobs waiting to retry.
