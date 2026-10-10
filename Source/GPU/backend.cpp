#include "fizmo_library.hpp"
#include "backend.hpp"

namespace fizmo {
namespace gpu {
namespace detail {

std::uint32_t PipelineLayoutImpl::array_capacity(std::uint32_t group, std::uint32_t binding) const noexcept {
    if (group >= groups.size() || !groups[group]) return 0;
    const BindGroupLayoutEntry* e = groups[group]->find(binding);
    return e ? e->count : 0;
}

auto PassRecord::attachment_extent() const noexcept -> Extent2D {
    const TextureImpl* t = color_count ? colors[0].texture : depth;
    if (!t) return {};
    const std::uint32_t mip = color_count ? colors[0].mip : 0;
    return { std::max(1u, t->desc.extent.width >> mip), std::max(1u, t->desc.extent.height >> mip) };
}

std::uint32_t CommandStream::store(const void* data, std::size_t size) {
    const std::uint32_t at = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + size);
    if (size) std::memcpy(bytes.data() + at, data, size);
    return at;
}

Extent3D mip_extent(const TextureDesc& d, std::uint32_t mip) noexcept {
    return { std::max(1u, d.extent.width >> mip), std::max(1u, d.extent.height >> mip), d.dimension == TextureDimension::D3 ? std::max(1u, d.extent.depth >> mip) : 1u };
}

std::uint32_t full_mip_count(Extent3D e) noexcept {
    std::uint32_t largest = std::max(e.width, std::max(e.height, e.depth));
    std::uint32_t levels = 1;
    while (largest > 1) { largest >>= 1; ++levels; }
    return levels;
}

Result validate(const TextureDesc& d) noexcept {
    if (d.extent.width == 0 || d.extent.height == 0 || d.extent.depth == 0 || d.format == Format::Undefined) return Result::InvalidArgument;
    if (d.dimension == TextureDimension::Cube && d.extent.width != d.extent.height) return Result::InvalidArgument;
    if (d.dimension != TextureDimension::D3 && d.extent.depth != 1) return Result::InvalidArgument;
    if (d.samples > 1 && (d.dimension != TextureDimension::D2 || d.mip_levels != 1)) return Result::InvalidArgument;
    if (d.samples > 1 && (any(d.usage, TextureUsage::Sampled) || any(d.usage, TextureUsage::Storage))) return Result::Unsupported;
    return Result::Success;
}

} // namespace detail
} // namespace gpu
} // namespace fizmo
