#include "fizmo_library.hpp"
#include "content_version.hpp"

namespace fizmo {

std::uint64_t next_content_version() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

} // namespace fizmo
