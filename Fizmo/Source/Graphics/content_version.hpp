#ifndef FIZMO_CONTENT_VERSION_HPP
#define FIZMO_CONTENT_VERSION_HPP

#include <atomic>
#include <cstdint>

namespace fizmo {

inline std::uint64_t next_content_version() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

class ContentVersion {
private:
    mutable std::uint64_t m_version = 0;
    mutable bool          m_dirty   = true;

public:
    void touch() noexcept { m_dirty = true; }

    std::uint64_t get() const noexcept {
        if (m_dirty) {
            m_version = next_content_version();
            m_dirty   = false;
        }

        return m_version;
    }
};

} // namespace fizmo

#endif // FIZMO_CONTENT_VERSION_HPP