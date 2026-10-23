#include "app.hpp"

#if defined(OS_ANDROID)

#include <android/native_window.h>

#include <algorithm>
#include <cstring>

namespace fizmo {
namespace android {

void Surface::attach(ANativeWindow* window) noexcept {
    if (m_window == window) return;
    if (m_window) ANativeWindow_release(m_window);
    m_window = window;
    if (m_window) ANativeWindow_acquire(m_window);
    ++m_generation;
    m_width = 0;
    m_height = 0;
    refresh_size();
    m_redraw = true;
}

void Surface::detach() noexcept {
    if (m_window) ANativeWindow_release(m_window);
    m_window = nullptr;
    m_width = 0;
    m_height = 0;
    m_redraw = false;
}

bool Surface::refresh_size() noexcept {
    if (!m_window) return false;
    const int w = ANativeWindow_getWidth(m_window);
    const int h = ANativeWindow_getHeight(m_window);
    const unsigned int nw = w > 0 ? static_cast<unsigned int>(w) : 0u;
    const unsigned int nh = h > 0 ? static_cast<unsigned int>(h) : 0u;
    if (nw == m_width && nh == m_height) return false;
    m_width = nw;
    m_height = nh;
    m_redraw = true;
    return true;
}

void Surface::notify(SurfaceChange change) noexcept {
    const std::vector<Listener> copy = m_listeners;
    for (const Listener& l : copy) {
        try {
            if (l.change) l.change(change);
            if (change == SurfaceChange::Resized && l.resize) l.resize(m_width, m_height);
        } catch (...) {}
    }
}

void Surface::paint() noexcept {
    if (!m_paint || !m_window) return;
    try { m_paint(m_window); } catch (...) {}
    m_redraw = false;
}

std::uint64_t Surface::add_listener(ResizeCallback on_resize, ChangeCallback on_change) {
    Listener l;
    l.id = m_next_listener++;
    l.resize = std::move(on_resize);
    l.change = std::move(on_change);
    m_listeners.push_back(std::move(l));
    return m_listeners.back().id;
}

void Surface::remove_listener(std::uint64_t id) noexcept {
    m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(), [id](const Listener& l) { return l.id == id; }), m_listeners.end());
}

bool present_pixels(ANativeWindow* window, const std::uint32_t* bgra, unsigned int width, unsigned int height, unsigned int stride_bytes) noexcept {
    if (!window || !bgra || width == 0 || height == 0) return false;
    if (ANativeWindow_setBuffersGeometry(window, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), WINDOW_FORMAT_RGBX_8888) != 0) return false;
    ANativeWindow_Buffer buf{};
    if (ANativeWindow_lock(window, &buf, nullptr) != 0 || !buf.bits) return false;
    const unsigned int rows = std::min(height, static_cast<unsigned int>(std::max(0, buf.height)));
    const unsigned int cols = std::min(width, static_cast<unsigned int>(std::max(0, buf.width)));
    const std::size_t src_stride = stride_bytes / 4u;
    if (buf.format == WINDOW_FORMAT_RGB_565) {
        auto* dst = static_cast<std::uint16_t*>(buf.bits);
        for (unsigned int y = 0; y < rows; ++y) {
            const std::uint32_t* s = bgra + static_cast<std::size_t>(y) * src_stride;
            std::uint16_t* d = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(buf.stride);
            for (unsigned int x = 0; x < cols; ++x) {
                const std::uint32_t v = s[x];
                d[x] = static_cast<std::uint16_t>(((v >> 8) & 0xF800u) | ((v >> 5) & 0x07E0u) | ((v >> 3) & 0x001Fu));
            }
        }
    } else {
        auto* dst = static_cast<std::uint32_t*>(buf.bits);
        for (unsigned int y = 0; y < rows; ++y) {
            const std::uint32_t* s = bgra + static_cast<std::size_t>(y) * src_stride;
            std::uint32_t* d = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(buf.stride);
            for (unsigned int x = 0; x < cols; ++x) {
                const std::uint32_t v = s[x];
                d[x] = 0xFF000000u | ((v & 0xFFu) << 16) | (v & 0xFF00u) | ((v >> 16) & 0xFFu);
            }
        }
    }
    ANativeWindow_unlockAndPost(window);
    return true;
}

} // namespace android
} // namespace fizmo

#endif // OS_ANDROID
