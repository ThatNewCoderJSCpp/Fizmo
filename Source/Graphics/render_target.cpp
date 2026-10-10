#include "fizmo_library.hpp"
#include "render_target.hpp"

namespace fizmo {
namespace graphics {
namespace detail {

std::uint64_t next_render_target_id() noexcept {
    static std::atomic<std::uint64_t> counter{ 0 };
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

} // namespace detail
} // namespace graphics
} // namespace fizmo

namespace fizmo {
namespace graphics {

RenderTarget::RenderTarget(unsigned int width, unsigned int height, SampleFilter filter, WrapMode wrap) : m_slot(std::make_shared<detail::RenderTargetSlot>()) {
    m_slot->width  = width;
    m_slot->height = height;
    m_slot->filter = filter;
    m_slot->wrap   = wrap;
}

void RenderTarget::resize(unsigned int width, unsigned int height) {
    if (!m_slot) m_slot = std::make_shared<detail::RenderTargetSlot>();
    if (m_slot->width == width && m_slot->height == height) return;
    m_slot->width  = width;
    m_slot->height = height;
    ++m_slot->generation;
}

} // namespace graphics
} // namespace fizmo
