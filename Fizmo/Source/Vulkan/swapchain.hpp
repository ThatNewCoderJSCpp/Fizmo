#ifndef FIZMO_VULKAN_SWAPCHAIN_HPP
#define FIZMO_VULKAN_SWAPCHAIN_HPP

#include "commands.hpp"

namespace fizmo {
namespace vulkan {

struct SwapchainDesc {
    Extent2D      size;                           
    bool          vsync       = true;             
    bool          srgb        = false;            
    std::uint32_t image_count = 3;                
    ImageUsage    usage       = ImageUsage::ColorAttachment | ImageUsage::TransferDst;
};

class Swapchain {
private:
    const detail::DeviceState* m_device  = nullptr;
    const Surface*             m_surface = nullptr;
    native::Swapchain          m_handle  = VK_NULL_HANDLE;
    SwapchainDesc              m_desc;
    Format                     m_format  = Format::Undefined;
    PresentMode                m_present_mode = PresentMode::Fifo;
    Extent2D                   m_extent;
    std::vector<native::Image> m_images;
    std::vector<ImageView>     m_views;
    std::vector<Semaphore>     m_present_semaphores;  

    void release_images() noexcept {
        m_views.clear();
        m_present_semaphores.clear();
        m_images.clear();
    }

public:
    Swapchain() noexcept = default;
    ~Swapchain() noexcept { destroy(); }

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain(Swapchain&& o) noexcept { *this = std::move(o); }

    Swapchain& operator=(Swapchain&& o) noexcept {
        if (this != &o) {
            destroy();
            m_device  = std::exchange(o.m_device, nullptr);
            m_surface = std::exchange(o.m_surface, nullptr);
            m_handle  = std::exchange(o.m_handle, VK_NULL_HANDLE);
            m_desc    = o.m_desc;
            m_format  = o.m_format;
            m_present_mode = o.m_present_mode;
            m_extent  = o.m_extent;
            m_images  = std::move(o.m_images);
            m_views   = std::move(o.m_views);
            m_present_semaphores = std::move(o.m_present_semaphores);
        }

        return *this;
    }

    Result create(const Device& device, const Surface& surface, const SwapchainDesc& desc) {
        if (!device.valid() || !surface.valid()) return Result::NotInitialized;
        if (!device.features().swapchain) return Result::ExtensionNotPresent;
        m_device  = device.state();
        m_surface = &surface;
        m_desc    = desc;
        return rebuild(device);
    }

    Result recreate(const Device& device, Extent2D new_size) {
        m_desc.size = new_size;
        return rebuild(device);
    }

    void destroy() noexcept {
        if (!m_device) return;

        if (m_handle) {
            m_device->fn.vkDeviceWaitIdle(m_device->handle);
            release_images();
            m_device->fn.vkDestroySwapchainKHR(m_device->handle, m_handle, nullptr);
        }

        m_handle = VK_NULL_HANDLE;
        m_device = nullptr;
    }

    Result acquire(const Semaphore& signal, std::uint32_t& image_index, std::uint64_t timeout_ns = kNoTimeout) const noexcept {
        if (!m_handle) return Result::NotInitialized;
        return to_result(m_device->fn.vkAcquireNextImageKHR(m_device->handle, m_handle, timeout_ns, signal.handle(), VK_NULL_HANDLE, &image_index));
    }

    Result present(const Queue& queue, std::uint32_t image_index) const noexcept {
        if (!m_handle) return Result::NotInitialized;
        native::Semaphore wait = m_present_semaphores[image_index].handle();
        auto info = detail::make<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores    = &wait;
        info.swapchainCount     = 1;
        info.pSwapchains        = &m_handle;
        info.pImageIndices      = &image_index;
        return to_result(m_device->fn.vkQueuePresentKHR(queue.handle(), &info));
    }

    bool              valid()                            const noexcept { return m_handle != VK_NULL_HANDLE; }
    native::Swapchain handle()                           const noexcept { return m_handle; }
    Format            format()                           const noexcept { return m_format; }
    PresentMode       present_mode()                     const noexcept { return m_present_mode; }
    Extent2D          extent()                           const noexcept { return m_extent; }
    std::uint32_t     image_count()                      const noexcept { return static_cast<std::uint32_t>(m_images.size()); }
    native::Image     image(std::uint32_t i)             const noexcept { return m_images[i]; }
    const ImageView&  view(std::uint32_t i)              const noexcept { return m_views[i]; }
    const Semaphore&  present_semaphore(std::uint32_t i) const noexcept { return m_present_semaphores[i]; }
    const SwapchainDesc& desc()                          const noexcept { return m_desc; }

private:
    Result rebuild(const Device& device) {
        const PhysicalDevice gpu = device.physical_device();
        const VkSurfaceCapabilitiesKHR caps = m_surface->capabilities(gpu);
        Extent2D extent = m_desc.size;
        if (caps.currentExtent.width != UINT32_MAX) extent = { caps.currentExtent.width, caps.currentExtent.height };
        extent.width  = std::max(caps.minImageExtent.width,  std::min(caps.maxImageExtent.width,  extent.width));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
        if (extent.width == 0 || extent.height == 0) return Result::NotReady;   // minimized
        const auto formats = m_surface->formats(gpu);
        if (formats.empty()) return Result::FormatNotSupported;
        const Format wanted[] = { m_desc.srgb ? Format::BGRA8Srgb : Format::BGRA8Unorm, m_desc.srgb ? Format::RGBA8Srgb : Format::RGBA8Unorm };
        VkSurfaceFormatKHR chosen = formats[0];
        bool found = false;

        for (Format w : wanted) {
            for (const auto& f : formats) {
                if (f.format == to_vk(w) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { chosen = f; found = true; break; }
            }

            if (found) break;
        }

        PresentMode mode = PresentMode::Fifo;
        
        if (!m_desc.vsync) {
            const auto modes = m_surface->present_modes(gpu);
            auto has = [&](PresentMode m) { for (auto x : modes) if (x == m) return true; return false; };
            if (has(PresentMode::Mailbox))        mode = PresentMode::Mailbox;
            else if (has(PresentMode::Immediate)) mode = PresentMode::Immediate;
        }

        std::uint32_t count = std::max(m_desc.image_count, caps.minImageCount);
        if (caps.maxImageCount > 0) count = std::min(count, caps.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        if (!(caps.supportedCompositeAlpha & alpha)) {
            for (VkCompositeAlphaFlagBitsKHR a : { VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR }) {
                if (caps.supportedCompositeAlpha & a) { alpha = a; break; }
            }
        }

        const VkImageUsageFlags usage = to_vk(m_desc.usage) & caps.supportedUsageFlags;
        auto info = detail::make<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        info.surface          = m_surface->handle();
        info.minImageCount    = count;
        info.imageFormat      = chosen.format;
        info.imageColorSpace  = chosen.colorSpace;
        info.imageExtent      = to_vk(extent);
        info.imageArrayLayers = 1;
        info.imageUsage       = usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.preTransform     = caps.currentTransform;
        info.compositeAlpha   = alpha;
        info.presentMode      = to_vk(mode);
        info.clipped          = VK_TRUE;
        info.oldSwapchain     = m_handle;
        const std::uint32_t families[] = { m_device->graphics.family, m_device->present.family };
        
        if (families[0] != families[1]) {
            info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices   = families;
        } else {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        if (m_handle) m_device->fn.vkDeviceWaitIdle(m_device->handle);
        native::Swapchain fresh = VK_NULL_HANDLE;
        VkResult r = m_device->fn.vkCreateSwapchainKHR(m_device->handle, &info, nullptr, &fresh);
        if (r != VK_SUCCESS) return to_result(r);
        release_images();
        if (m_handle) m_device->fn.vkDestroySwapchainKHR(m_device->handle, m_handle, nullptr);
        m_handle       = fresh;
        m_format       = static_cast<Format>(chosen.format);
        m_present_mode = mode;
        m_extent       = extent;
        std::uint32_t n = 0;
        m_device->fn.vkGetSwapchainImagesKHR(m_device->handle, m_handle, &n, nullptr);
        m_images.resize(n);
        m_device->fn.vkGetSwapchainImagesKHR(m_device->handle, m_handle, &n, m_images.data());
        m_views.resize(n);
        m_present_semaphores.resize(n);

        for (std::uint32_t i = 0; i < n; ++i) {
            Result res = m_views[i].create(m_device, m_images[i], VK_IMAGE_VIEW_TYPE_2D, m_format, ImageAspect::Color, 0, 1, 0, 1);
            if (failed(res)) return res;
            res = m_present_semaphores[i].create(device);
            if (failed(res)) return res;
        }

        return Result::Success;
    }
};

struct Frame {
    CommandBuffer    cmd;                   
    std::uint32_t    image_index = 0;       
    std::uint32_t    frame_index = 0;       
    native::Image    target      = VK_NULL_HANDLE;
    const ImageView* target_view = nullptr;
    Extent2D         extent;
    Format           format      = Format::Undefined;
};

class FrameRing {
private:
    struct PerFrame {
        CommandPool   pool;
        CommandBuffer cmd;
        Fence         in_flight;
        Semaphore     image_acquired;
    };

    Device*               m_device    = nullptr;
    Swapchain*            m_swapchain = nullptr;
    std::vector<PerFrame> m_frames;
    std::uint32_t         m_current   = 0;
    std::uint32_t         m_image     = 0;
    ImageLayout           m_target_layout = ImageLayout::Undefined;
    bool                  m_in_frame  = false;
    bool                  m_needs_resize = false;
    Extent2D              m_pending_size;

public:
    FrameRing() noexcept = default;
    ~FrameRing() noexcept { destroy(); }

    FrameRing(const FrameRing&) = delete;
    FrameRing& operator=(const FrameRing&) = delete;
    FrameRing(FrameRing&&) noexcept = default;
    FrameRing& operator=(FrameRing&&) noexcept = default;

    Result create(Device& device, Swapchain& swapchain, std::uint32_t frames_in_flight = 2) {
        destroy();
        if (!device.valid() || !swapchain.valid() || frames_in_flight == 0) return Result::InvalidArgument;
        m_device    = &device;
        m_swapchain = &swapchain;
        m_frames.resize(frames_in_flight);

        for (auto& f : m_frames) {
            Result r = f.pool.create(device, QueueType::Graphics, true);
            if (failed(r)) return r;
            r = f.pool.allocate(f.cmd);
            if (failed(r)) return r;
            r = f.in_flight.create(device, true);
            if (failed(r)) return r;
            r = f.image_acquired.create(device);
            if (failed(r)) return r;
        }

        m_current = 0;
        return Result::Success;
    }

    void destroy() noexcept {
        if (m_device && m_device->valid()) m_device->wait_idle();
        m_frames.clear();
        m_device = nullptr;
        m_swapchain = nullptr;
        m_in_frame = false;
    }

    void resize(std::uint32_t width, std::uint32_t height) noexcept {
        m_pending_size = { width, height };
        m_needs_resize = true;
    }

    std::uint32_t frames_in_flight() const noexcept { return static_cast<std::uint32_t>(m_frames.size()); }
    std::uint32_t frame_index()      const noexcept { return m_current; }

    Result begin_frame(Frame& out) {
        if (!m_device || m_in_frame) return Result::NotInitialized;

        if (m_needs_resize) {
            Result r = m_swapchain->recreate(*m_device, m_pending_size);
            if (r == Result::NotReady) return r;   
            m_needs_resize = false;
            if (failed(r)) return r;
        }

        PerFrame& f = m_frames[m_current];
        Result r = f.in_flight.wait();
        if (failed(r)) return r;
        r = m_swapchain->acquire(f.image_acquired, m_image);

        if (r == Result::OutOfDate) {
            m_swapchain->recreate(*m_device, m_swapchain->desc().size);
            return Result::OutOfDate;
        }

        if (failed(r)) return r;
        f.in_flight.reset();
        f.cmd.reset();
        f.cmd.begin(true);
        ImageBarrier to_color;
        to_color.image      = m_swapchain->image(m_image);
        to_color.old_layout = ImageLayout::Undefined;
        to_color.new_layout = ImageLayout::ColorAttachment;
        to_color.src_stage  = PipelineStage::ColorAttachmentOutput;
        to_color.src_access = Access::None;
        to_color.dst_stage  = PipelineStage::ColorAttachmentOutput;
        to_color.dst_access = Access::ColorAttachmentRead | Access::ColorAttachmentWrite;
        f.cmd.barrier(to_color);
        m_target_layout = ImageLayout::ColorAttachment;
        out.cmd         = f.cmd;
        out.image_index = m_image;
        out.frame_index = m_current;
        out.target      = m_swapchain->image(m_image);
        out.target_view = &m_swapchain->view(m_image);
        out.extent      = m_swapchain->extent();
        out.format      = m_swapchain->format();
        m_in_frame = true;
        return Result::Success;
    }

    void transition_target(ImageLayout to) noexcept {
        if (!m_in_frame || to == m_target_layout) return;
        m_frames[m_current].cmd.transition(m_swapchain->image(m_image), m_target_layout, to);
        m_target_layout = to;
    }

    Result end_frame() {
        if (!m_in_frame) return Result::NotInitialized;
        m_in_frame = false;
        PerFrame& f = m_frames[m_current];
        f.cmd.transition(m_swapchain->image(m_image), m_target_layout, ImageLayout::Present);
        Result r = f.cmd.end();
        if (failed(r)) return r;
        const Queue queue = m_device->graphics_queue();
        const SemaphoreWait   wait  { &f.image_acquired, PipelineStage::ColorAttachmentOutput, 0 };
        const SemaphoreSignal signal{ &m_swapchain->present_semaphore(m_image), PipelineStage::AllCommands, 0 };
        r = queue.submit(f.cmd, wait, signal, &f.in_flight);
        if (failed(r)) return r;
        r = m_swapchain->present(m_device->present_queue(), m_image);
        m_current = (m_current + 1) % static_cast<std::uint32_t>(m_frames.size());

        if (r == Result::OutOfDate || r == Result::Suboptimal) {
            if (!m_needs_resize) { m_pending_size = m_swapchain->desc().size; m_needs_resize = true; }
            return Result::Success;
        }
        
        return r;
    }
};

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_SWAPCHAIN_HPP