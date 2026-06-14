#ifndef FIZMO_REPEATING_TIMER_HPP
#define FIZMO_REPEATING_TIMER_HPP

#include "stopwatch.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace fizmo {
namespace time {

class RepeatingTimer {
public:
    using callback_type = std::function<void()>;
    
    enum class state {
        stopped,
        running,
        paused,
        completed
    };

private:
    callback_type callback_;
    CompleteDuration interval_;
    CompleteDuration total_runtime_;
    CompleteDuration elapsed_runtime_;
    CompleteDuration time_until_next_tick_;
    
    std::atomic<state> current_state_;
    std::atomic<bool> run_forever_;
    std::atomic<bool> should_terminate_;
    
    std::unique_ptr<std::thread> timer_thread_;
    std::mutex state_mutex_;
    std::condition_variable state_cv_;
    
    Stopwatch runtime_watch_;
    Stopwatch interval_watch_;
    
public:
    RepeatingTimer(callback_type callback, const CompleteDuration& interval) 
        : callback_(std::move(callback))
        , interval_(interval)
        , total_runtime_(CompleteDuration())
        , elapsed_runtime_(CompleteDuration())
        , time_until_next_tick_(interval)
        , current_state_(state::stopped)
        , run_forever_(true)
        , should_terminate_(false) {
        if (!callback_) { throw std::invalid_argument("Callback function cannot be null"); }
        if (interval_.to_nanosecond().value() == 0) { throw std::invalid_argument("Interval must be greater than zero"); }
    }
    
    RepeatingTimer(callback_type callback, const CompleteDuration& interval, const CompleteDuration& total_runtime)
        : callback_(std::move(callback))
        , interval_(interval)
        , total_runtime_(total_runtime)
        , elapsed_runtime_(CompleteDuration())
        , time_until_next_tick_(interval)
        , current_state_(state::stopped)
        , run_forever_(false)
        , should_terminate_(false) {
        if (!callback_) { throw std::invalid_argument("Callback function cannot be null"); }
        if (interval_.to_nanosecond().value() == 0) { throw std::invalid_argument("Interval must be greater than zero"); }
        if (total_runtime_.to_nanosecond().value() == 0) { throw std::invalid_argument("Total runtime must be greater than zero for finite timer"); }
    }

    RepeatingTimer(callback_type callback, const Duration& interval) : RepeatingTimer(std::move(callback), CompleteDuration(interval)) {}
    RepeatingTimer(callback_type callback, const Duration& interval, const Duration& total_runtime) : RepeatingTimer(std::move(callback), CompleteDuration(interval), CompleteDuration(total_runtime)) {}
    
    ~RepeatingTimer() {
        stop();
        if (timer_thread_ && timer_thread_->joinable()) { timer_thread_->join(); }
    }
    
    RepeatingTimer(const RepeatingTimer&) = delete;
    RepeatingTimer& operator=(const RepeatingTimer&) = delete;
    
    RepeatingTimer(RepeatingTimer&& other) noexcept
        : callback_(std::move(other.callback_))
        , interval_(other.interval_)
        , total_runtime_(other.total_runtime_)
        , elapsed_runtime_(other.elapsed_runtime_)
        , time_until_next_tick_(other.time_until_next_tick_)
        , current_state_(other.current_state_.load())
        , run_forever_(other.run_forever_.load())
        , should_terminate_(other.should_terminate_.load())
        , timer_thread_(std::move(other.timer_thread_))
        , runtime_watch_(std::move(other.runtime_watch_))
        , interval_watch_(std::move(other.interval_watch_)) {
        other.current_state_ = state::stopped;
        other.should_terminate_ = true;
    }
    
    RepeatingTimer& operator=(RepeatingTimer&& other) noexcept {
        if (this != &other) {
            stop();
            if (timer_thread_ && timer_thread_->joinable()) { timer_thread_->join(); }
            callback_ = std::move(other.callback_);
            interval_ = other.interval_;
            total_runtime_ = other.total_runtime_;
            elapsed_runtime_ = other.elapsed_runtime_;
            time_until_next_tick_ = other.time_until_next_tick_;
            current_state_ = other.current_state_.load();
            run_forever_ = other.run_forever_.load();
            should_terminate_ = other.should_terminate_.load();
            timer_thread_ = std::move(other.timer_thread_);
            runtime_watch_ = std::move(other.runtime_watch_);
            interval_watch_ = std::move(other.interval_watch_);
            other.current_state_ = state::stopped;
            other.should_terminate_ = true;
        }
        return *this;
    }
    
    void start() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::running) { return; }
        
        if (current_state_ == state::completed) {
            elapsed_runtime_ = CompleteDuration();
            time_until_next_tick_ = interval_;
        }
        
        current_state_ = state::running;
        should_terminate_ = false;
        runtime_watch_.restart();
        interval_watch_.restart();
        if (!timer_thread_ || !timer_thread_->joinable()) { timer_thread_ = std::make_unique<std::thread>(&RepeatingTimer::timer_loop, this); }
        state_cv_.notify_all();
    }

    void start_blocking() {
        start();
        wait_until_complete();
    }

    void wait_until_complete() { 
        while (get_state() != state::completed && get_state() != state::stopped) { 
            precise_wait(millisecond(1)); 
        }
    }
    
    void pause() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::running) { return; }
        current_state_ = state::paused;
        
        if (runtime_watch_.is_running()) {
            elapsed_runtime_ = elapsed_runtime_ + runtime_watch_.elapsed();
            runtime_watch_.stop();
        }
        
        if (interval_watch_.is_running()) {
            CompleteDuration elapsed_in_interval = interval_watch_.elapsed();

            if (elapsed_in_interval < interval_) {
                time_until_next_tick_ = interval_ - elapsed_in_interval;
            } else {
                time_until_next_tick_ = interval_;
            }

            interval_watch_.stop();
        }
        
        state_cv_.notify_all();
    }
    
    void resume() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::paused) { return; }
        current_state_ = state::running;
        runtime_watch_.start();
        interval_watch_.restart();
        state_cv_.notify_all();
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (current_state_ == state::stopped) { return; }
            current_state_ = state::stopped;
            should_terminate_ = true;
            elapsed_runtime_ = CompleteDuration();
            time_until_next_tick_ = interval_;
            runtime_watch_.reset();
            interval_watch_.reset();
            state_cv_.notify_all();
        }
        
        if (timer_thread_ && timer_thread_->joinable()) {
            timer_thread_->join();
            timer_thread_.reset();
        }
    }
    
    void reset() {
        stop();
        start();
    }
    
    bool set_interval(const CompleteDuration& new_interval) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::stopped) { return false; }
        if (new_interval.to_nanosecond().value() == 0) { return false; }
        interval_ = new_interval;
        time_until_next_tick_ = new_interval;
        return true;
    }
    
    bool set_interval(DURATION_PARAM new_interval) noexcept { 
        return set_interval(CompleteDuration(new_interval)); 
    }
    
    bool set_total_runtime(const CompleteDuration& new_runtime) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::stopped) { return false; }

        if (new_runtime.to_nanosecond().value() == 0) {
            run_forever_ = true;
            total_runtime_ = CompleteDuration();
        } else {
            run_forever_ = false;
            total_runtime_ = new_runtime;
        }
        
        elapsed_runtime_ = CompleteDuration();
        return true;
    }
    
    bool set_total_runtime(DURATION_PARAM new_runtime) noexcept { return set_total_runtime(CompleteDuration(new_runtime)); }
    
    bool set_infinite() noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::stopped) { return false; }
        run_forever_ = true;
        total_runtime_ = CompleteDuration();
        elapsed_runtime_ = CompleteDuration();
        return true;
    }

    bool set_callback(callback_type new_callback) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::running) { return false; }
        callback_ = std::move(new_callback);
        return true;
    }

    void restart(const CompleteDuration& new_interval) {
        stop();
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (new_interval.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Interval must be greater than zero"); 
        }
        interval_ = new_interval;
        time_until_next_tick_ = new_interval;
    }
    
    void restart(DURATION_PARAM new_interval) { restart(CompleteDuration(new_interval)); }

    void restart(const CompleteDuration& new_interval, const CompleteDuration& new_runtime) {
        stop();
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (new_interval.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Interval must be greater than zero"); 
        }
        if (new_runtime.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Total runtime must be greater than zero for finite timer"); 
        }
        interval_ = new_interval;
        time_until_next_tick_ = new_interval;
        total_runtime_ = new_runtime;
        run_forever_ = false;
    }
    
    void restart(DURATION_PARAM new_interval, DURATION_PARAM new_runtime) { restart(CompleteDuration(new_interval), CompleteDuration(new_runtime)); }

    void add_runtime(const CompleteDuration& additional_time) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::completed || current_state_ == state::stopped) { return; }
        if (run_forever_) { return; }
        total_runtime_ = total_runtime_ + additional_time;
    }
    
    void add_runtime(DURATION_PARAM additional_time) { add_runtime(CompleteDuration(additional_time)); }
    
    void subtract_runtime(const CompleteDuration& time_to_subtract) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::completed || current_state_ == state::stopped) { return; }
        if (run_forever_) { return; }
        
        CompleteDuration current_elapsed = elapsed_runtime_;
        if (current_state_ == state::running && runtime_watch_.is_running()) {
            current_elapsed = current_elapsed + runtime_watch_.elapsed();
        }
        
        if (time_to_subtract >= total_runtime_) {
            total_runtime_ = current_elapsed;
            if (current_state_ == state::running) {
                current_state_ = state::completed;
                state_cv_.notify_all();
            }
        } else {
            total_runtime_ = total_runtime_ - time_to_subtract;
            if (current_elapsed >= total_runtime_ && current_state_ == state::running) {
                current_state_ = state::completed;
                state_cv_.notify_all();
            }
        }
    }
    
    void subtract_runtime(DURATION_PARAM time_to_subtract) { subtract_runtime(CompleteDuration(time_to_subtract)); }
    state get_state() const noexcept { return current_state_.load(); }
    bool is_running() const noexcept { return current_state_ == state::running; }
    bool is_paused() const noexcept { return current_state_ == state::paused; }
    bool is_stopped() const noexcept { return current_state_ == state::stopped; }
    bool is_completed() const noexcept { return current_state_ == state::completed; }
    
    CompleteDuration elapsed_runtime() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        if (current_state_ == state::running && runtime_watch_.is_running()) { 
            return elapsed_runtime_ + runtime_watch_.elapsed(); 
        }
        return elapsed_runtime_;
    }
    
    CompleteDuration remaining_runtime() const {
        if (run_forever_) { return CompleteDuration(); }
        CompleteDuration elapsed = elapsed_runtime();
        if (elapsed >= total_runtime_) { return CompleteDuration(); }
        return total_runtime_ - elapsed;
    }
    
    CompleteDuration get_interval() const noexcept { return interval_; }
    CompleteDuration get_total_runtime() const noexcept { return total_runtime_; }
    bool is_infinite() const noexcept { return run_forever_; }

    CompleteDuration time_until_next_tick() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        
        if (current_state_ != state::running) {
            return time_until_next_tick_;
        }
        
        if (interval_watch_.is_running()) {
            CompleteDuration elapsed_in_interval = interval_watch_.elapsed();
            if (elapsed_in_interval >= interval_) { 
                return CompleteDuration(); 
            }
            return interval_ - elapsed_in_interval;
        }
        
        return time_until_next_tick_;
    }

    double get_progress() const {
        if (run_forever_) { return 0.0; }
        if (total_runtime_ == CompleteDuration()) { return 1.0; }
        
        const CompleteDuration& elapsed = elapsed_runtime();
        
        if (total_runtime_.total_millennia() >= 1.0) {
            return elapsed.total_millennia() / total_runtime_.total_millennia();
        } else if (total_runtime_.total_years() >= 1.0) {
            return elapsed.total_years() / total_runtime_.total_years();
        } else if (total_runtime_.total_days() >= 1.0) {
            return elapsed.total_days() / total_runtime_.total_days();
        } else if (total_runtime_.total_seconds() >= 1.0) {
            return elapsed.total_seconds() / total_runtime_.total_seconds();
        } else {
            return elapsed.total_nanoseconds() / total_runtime_.total_nanoseconds();
        }
    }

    bool has_callback() const noexcept {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        return callback_ != nullptr;
    }
    
    operator CompleteDuration() const { return remaining_runtime(); }
    operator Duration() const { return remaining_runtime().to_duration(); }
    
private:
    void timer_loop() {
        while (!should_terminate_) {
            std::unique_lock<std::mutex> lock(state_mutex_);
            state_cv_.wait(lock, [this] { return current_state_ == state::running || should_terminate_; });
            if (should_terminate_) { break; }
            Duration wait_time = time_until_next_tick_.to_duration(Duration::unit::nanosecond);
            
            if (!run_forever_) {
                CompleteDuration total_elapsed = elapsed_runtime_ + runtime_watch_.elapsed();
                CompleteDuration remaining = (total_runtime_ > total_elapsed) ? (total_runtime_ - total_elapsed) : CompleteDuration();
                
                if (remaining == CompleteDuration()) {
                    current_state_ = state::completed;
                    continue;
                }
                
                Duration remaining_duration = remaining.to_duration(Duration::unit::nanosecond);
                if (wait_time > remaining_duration) { wait_time = remaining_duration; }
            }
            
            lock.unlock();
            precise_wait(wait_time);
            lock.lock();
            if (current_state_ != state::running || should_terminate_) { continue; }
            
            if (!run_forever_) {
                CompleteDuration total_elapsed = elapsed_runtime_ + runtime_watch_.elapsed();

                if (total_elapsed >= total_runtime_) {
                    current_state_ = state::completed;
                    continue;
                }
            }
            
            lock.unlock();
            
            try {
                callback_();
            } catch (...) {
                // Silently catch exceptions from callback
            }
            
            lock.lock();
            time_until_next_tick_ = interval_;
            interval_watch_.restart();
        }
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_REPEATING_TIMER_HPP