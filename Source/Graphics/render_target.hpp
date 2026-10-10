#ifndef FIZMO_RENDER_TARGET_HPP
#define FIZMO_RENDER_TARGET_HPP

#include "texture.hpp"
#include <atomic>
#include <cstdint>
#include <memory>

namespace fizmo {
namespace graphics {

namespace detail {

std::uint64_t next_render_target_id() noexcept;

struct RenderTargetSlot {
    std::uint64_t id         = next_render_target_id();
    unsigned int  width      = 0;
    unsigned int  height     = 0;
    std::uint64_t generation = 1;
    SampleFilter  filter     = SampleFilter::Bilinear;
    WrapMode      wrap       = WrapMode::Clamp;
};

} // namespace detail

class RenderTarget {
private:
    std::shared_ptr<detail::RenderTargetSlot> m_slot;

public:
    RenderTarget() = default;

    RenderTarget(unsigned int width, unsigned int height, SampleFilter filter = SampleFilter::Bilinear, WrapMode wrap = WrapMode::Clamp)
;

    bool valid() const noexcept { return m_slot && m_slot->width > 0 && m_slot->height > 0; }
    explicit operator bool() const noexcept { return valid(); }
    unsigned int width() const noexcept { return m_slot ? m_slot->width : 0; }
    unsigned int height() const noexcept { return m_slot ? m_slot->height : 0; }
    std::uint64_t id() const noexcept { return m_slot ? m_slot->id : 0; }
    std::uint64_t generation() const noexcept { return m_slot ? m_slot->generation : 0; }
    SampleFilter filter() const noexcept { return m_slot ? m_slot->filter : SampleFilter::Bilinear; }
    WrapMode wrap() const noexcept { return m_slot ? m_slot->wrap : WrapMode::Clamp; }
    TextureRect full_rect() const noexcept { return TextureRect(0, 0, width(), height()); }

    void set_filter(SampleFilter f) noexcept { if (m_slot) m_slot->filter = f; }
    void set_wrap(WrapMode w) noexcept { if (m_slot) m_slot->wrap = w; }

    void resize(unsigned int width, unsigned int height);

    const std::shared_ptr<detail::RenderTargetSlot>& slot() const noexcept { return m_slot; }

    friend bool operator==(const RenderTarget& a, const RenderTarget& b) noexcept { return a.m_slot == b.m_slot; }
    friend bool operator!=(const RenderTarget& a, const RenderTarget& b) noexcept { return a.m_slot != b.m_slot; }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_RENDER_TARGET_HPP
