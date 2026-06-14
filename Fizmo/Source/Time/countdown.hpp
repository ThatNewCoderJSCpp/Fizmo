#ifndef FIZMO_COUNTDOWN_TIMER_HPP
#define FIZMO_COUNTDOWN_TIMER_HPP

#include "repeat.hpp"

namespace fizmo {
namespace time {

class CountdownTimer {
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
    CompleteDuration target_duration_;
    CompleteDuration remaining_duration_;
    
    std::atomic<state> current_state_;
    std::atomic<bool> should_terminate_;
    
    std::unique_ptr<std::thread> timer_thread_;
    std::mutex state_mutex_;
    std::condition_variable state_cv_;
    
    Stopwatch countdown_watch_;
    
public:
    explicit CountdownTimer(const CompleteDuration& countdown_duration) 
        : callback_(nullptr)
        , target_duration_(countdown_duration)
        , remaining_duration_(countdown_duration)
        , current_state_(state::stopped)
        , should_terminate_(false) {
        if (countdown_duration.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Countdown duration must be greater than zero"); 
        }
    }
    
    CountdownTimer(callback_type callback, const CompleteDuration& countdown_duration)
        : callback_(std::move(callback))
        , target_duration_(countdown_duration)
        , remaining_duration_(countdown_duration)
        , current_state_(state::stopped)
        , should_terminate_(false) 
    {
        if (countdown_duration.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Countdown duration must be greater than zero"); 
        }
    }

    explicit CountdownTimer(DURATION_PARAM countdown_duration) : CountdownTimer(CompleteDuration(countdown_duration)) {}
    CountdownTimer(callback_type callback, DURATION_PARAM countdown_duration) : CountdownTimer(std::move(callback), CompleteDuration(countdown_duration)) {}
    
    ~CountdownTimer() {
        stop();
        if (timer_thread_ && timer_thread_->joinable()) { timer_thread_->join(); }
    }
    
    CountdownTimer(const CountdownTimer&) = delete;
    CountdownTimer& operator=(const CountdownTimer&) = delete;
    
    CountdownTimer(CountdownTimer&& other) noexcept
        : callback_(std::move(other.callback_))
        , target_duration_(other.target_duration_)
        , remaining_duration_(other.remaining_duration_)
        , current_state_(other.current_state_.load())
        , should_terminate_(other.should_terminate_.load())
        , timer_thread_(std::move(other.timer_thread_))
        , countdown_watch_(std::move(other.countdown_watch_)) 
    {
        other.current_state_ = state::stopped;
        other.should_terminate_ = true;
    }
    
    CountdownTimer& operator=(CountdownTimer&& other) noexcept {
        if (this != &other) {
            stop();
            if (timer_thread_ && timer_thread_->joinable()) { timer_thread_->join(); }
            callback_ = std::move(other.callback_);
            target_duration_ = other.target_duration_;
            remaining_duration_ = other.remaining_duration_;
            current_state_ = other.current_state_.load();
            should_terminate_ = other.should_terminate_.load();
            timer_thread_ = std::move(other.timer_thread_);
            countdown_watch_ = std::move(other.countdown_watch_);
            other.current_state_ = state::stopped;
            other.should_terminate_ = true;
        }
        return *this;
    }
    
    void start() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::running) { return; }
        if (current_state_ == state::completed) { remaining_duration_ = target_duration_; }
        current_state_ = state::running;
        should_terminate_ = false;
        countdown_watch_.restart();
        
        if (!timer_thread_ || !timer_thread_->joinable()) {
            timer_thread_ = std::make_unique<std::thread>(&CountdownTimer::timer_loop, this);
        }
        
        state_cv_.notify_all();
    }
    
    void pause() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::running) { return; }
        current_state_ = state::paused;
        
        if (countdown_watch_.is_running()) {
            CompleteDuration elapsed = countdown_watch_.elapsed();
            
            if (elapsed < remaining_duration_) {
                remaining_duration_ = remaining_duration_ - elapsed;
            } else {
                remaining_duration_ = CompleteDuration();
            }

            countdown_watch_.stop();
        }
        
        state_cv_.notify_all();
    }
    
    void resume() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::paused) { return; }
        current_state_ = state::running;
        countdown_watch_.restart();
        state_cv_.notify_all();
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (current_state_ == state::stopped) { return; }
            current_state_ = state::stopped;
            should_terminate_ = true;
            remaining_duration_ = target_duration_;
            countdown_watch_.reset();
            state_cv_.notify_all();
        }
        
        if (timer_thread_ && timer_thread_->joinable()) {
            timer_thread_->join();
            timer_thread_.reset();
        }
    }

    void start_blocking() {
        start();
        wait_until_complete();
    }

    void wait_until_complete() { 
        while (get_state() != state::completed) { 
            precise_wait(millisecond(1)); 
        }
    }
    
    void reset() {
        stop();
        start();
    }
    
    void restart(const CompleteDuration& new_duration) {
        stop();
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (new_duration.to_nanosecond().value() == 0) { 
            throw std::invalid_argument("Countdown duration must be greater than zero"); 
        }
        target_duration_ = new_duration;
        remaining_duration_ = new_duration;
    }
    
    void restart(DURATION_PARAM new_duration) { restart(CompleteDuration(new_duration)); }
    
    bool set_callback(callback_type new_callback) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::running) { return false; }
        callback_ = std::move(new_callback);
        return true;
    }
    
    bool set_duration(const CompleteDuration& new_duration) noexcept {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ != state::stopped) { return false; }
        if (new_duration.to_nanosecond().value() == 0) { return false; }
        target_duration_ = new_duration;
        remaining_duration_ = new_duration;
        return true;
    }
    
    bool set_duration(DURATION_PARAM new_duration) noexcept { return set_duration(CompleteDuration(new_duration)); }
    
    void add_time(const CompleteDuration& additional_time) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::completed || current_state_ == state::stopped) { return; }
        
        if (current_state_ == state::running) {
            CompleteDuration elapsed = countdown_watch_.elapsed();
            if (elapsed < remaining_duration_) {
                remaining_duration_ = (remaining_duration_ - elapsed) + additional_time;
            } else {
                remaining_duration_ = additional_time;
            }
            countdown_watch_.restart();
        } else if (current_state_ == state::paused) {
            remaining_duration_ = remaining_duration_ + additional_time;
        }
    }
    
    void add_time(DURATION_PARAM additional_time) { add_time(CompleteDuration(additional_time)); }
    
    void subtract_time(const CompleteDuration& time_to_subtract) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (current_state_ == state::completed || current_state_ == state::stopped) { return; }
        
        if (current_state_ == state::running) {
            CompleteDuration elapsed = countdown_watch_.elapsed();
            CompleteDuration current_remaining = (elapsed < remaining_duration_) ? (remaining_duration_ - elapsed) : CompleteDuration();
            
            if (time_to_subtract >= current_remaining) {
                remaining_duration_ = CompleteDuration();
                current_state_ = state::completed;
                countdown_watch_.stop();
                state_cv_.notify_all();
            } else {
                remaining_duration_ = current_remaining - time_to_subtract;
                countdown_watch_.restart();
            }
        } else if (current_state_ == state::paused) {
            if (time_to_subtract >= remaining_duration_) {
                remaining_duration_ = CompleteDuration();
                current_state_ = state::completed;
            } else {
                remaining_duration_ = remaining_duration_ - time_to_subtract;
            }
        }
    }
    
    void subtract_time(DURATION_PARAM time_to_subtract) { subtract_time(CompleteDuration(time_to_subtract)); }
    state get_state() const noexcept { return current_state_.load(); }
    bool is_running() const noexcept { return current_state_ == state::running; }
    bool is_paused() const noexcept { return current_state_ == state::paused; }
    bool is_stopped() const noexcept { return current_state_ == state::stopped; }
    bool is_completed() const noexcept { return current_state_ == state::completed; }
    
    CompleteDuration get_remaining_time() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        
        if (current_state_ == state::stopped) {
            return target_duration_;
        } else if (current_state_ == state::completed) {
            return CompleteDuration();
        } else if (current_state_ == state::paused) {
            return remaining_duration_;
        } else if (current_state_ == state::running && countdown_watch_.is_running()) {
            CompleteDuration elapsed = countdown_watch_.elapsed();
            if (elapsed >= remaining_duration_) { return CompleteDuration(); }
            return remaining_duration_ - elapsed;
        }
        
        return remaining_duration_;
    }
    
    CompleteDuration get_elapsed_time() const {
        CompleteDuration remaining = get_remaining_time();
        if (target_duration_ >= remaining) { return target_duration_ - remaining; }
        return CompleteDuration();
    }
    
    CompleteDuration get_target_duration() const noexcept { return target_duration_; }
    
    double get_progress() const {
        if (target_duration_ == CompleteDuration()) { return 1.0; }
        const CompleteDuration& elapsed = get_elapsed_time();
        
        if (target_duration_.total_millennia() >= 1.0) {
            return elapsed.total_millennia() / target_duration_.total_millennia();
        } else if (target_duration_.total_years() >= 1.0) {
            return elapsed.total_years() / target_duration_.total_years();
        } else if (target_duration_.total_days() >= 1.0) {
            return elapsed.total_days() / target_duration_.total_days();
        } else if (target_duration_.total_seconds() >= 1.0) {
            return elapsed.total_seconds() / target_duration_.total_seconds();
        } else {
            return elapsed.total_nanoseconds() / target_duration_.total_nanoseconds();
        }
    }
    
    bool has_callback() const noexcept {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        return callback_ != nullptr;
    }
    
    operator CompleteDuration() const { return get_remaining_time(); }
    operator Duration() const { return get_remaining_time().to_duration(); }
    
private:
    void timer_loop() {
        while (!should_terminate_) {
            std::unique_lock<std::mutex> lock(state_mutex_);
            
            state_cv_.wait(lock, [this] {
                return current_state_ == state::running || should_terminate_;
            });
            
            if (should_terminate_) { break; }
            Duration wait_time = remaining_duration_.to_duration(Duration::unit::nanosecond);
            lock.unlock();
            precise_wait(wait_time);
            lock.lock();
            if (current_state_ != state::running || should_terminate_) { continue; }
            CompleteDuration elapsed = countdown_watch_.elapsed();

            if (elapsed >= remaining_duration_) {
                current_state_ = state::completed;
                countdown_watch_.stop();
                
                if (callback_) {
                    lock.unlock();

                    try {
                        callback_();
                    } catch (...) {
                        // Silently catch exceptions from callback
                    }

                    lock.lock();
                }
            }
        }
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_COUNTDOWN_TIMER_HPP