#ifndef FIZMO_WINDOWS_RENDERER_HPP
#define FIZMO_WINDOWS_RENDERER_HPP

#include "../Graphics/Renderer/renderer.hpp"

namespace fizmo {
namespace windows {

using graphics::Renderer;
using graphics::RendererBackend;
using graphics::FrameStats;
using graphics::FrameObserver;
using graphics::GpuPassStage;
using graphics::GpuPassContext;
using graphics::GpuPassCallback;
using graphics::RenderPoint;
using graphics::GpuPass;
using graphics::UpscaleFilter;
using graphics::gpu_pass_name;
using graphics::GpuTimings;
using graphics::GpuMemory;

#ifdef OS_WINDOWS
using graphics::blit_framebuffer;
#endif // OS_WINDOWS

} // namespace windows
} // namespace fizmo

#endif // FIZMO_WINDOWS_RENDERER_HPP
