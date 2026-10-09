#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "timer.hpp"

namespace fizmo {
namespace time {
namespace detail {

auto TimerBase::shutdown() noexcept -> void {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_requested_ = true;
            paused_ = false;
        }

        cv_.notify_all();

        if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
            worker_.join();
        }
    }

auto TimerBase::pause() noexcept -> void {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!running_ || paused_) return;
            paused_ = true;
        }

        cv_.notify_all();
    }

auto TimerBase::resume() noexcept -> void {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!paused_) return;
            paused_ = false;
        }

        cv_.notify_all();
    }

auto TimerBase::rethrow_if_error() const -> void {
        std::exception_ptr e;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            e = error_;
        }

        if (e) std::rethrow_exception(e);
    }

} // namespace detail
} // namespace time
} // namespace fizmo
