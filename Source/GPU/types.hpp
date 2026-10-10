#ifndef FIZMO_GPU_TYPES_HPP
#define FIZMO_GPU_TYPES_HPP

#include "../Basic/fizmo_defines.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

#ifdef OS_LINUX
#endif

namespace fizmo {
namespace gpu {

enum class Result : std::int32_t {
    Success         = 0,
    NotReady        = 1,
    InvalidArgument = -1,
    NotInitialized  = -2,
    Unsupported     = -3,
    OutOfMemory     = -4,
    DeviceLost      = -5,
    BackendFailed   = -6,
    ShaderFailed    = -7,
    NoBackend       = -8,
};

inline constexpr bool succeeded(Result r) noexcept { return static_cast<std::int32_t>(r) >= 0; }
inline constexpr bool failed(Result r)    noexcept { return static_cast<std::int32_t>(r) < 0; }

inline constexpr const char* to_string(Result r) noexcept {
    switch (r) {
        case Result::Success:         return "Success";
        case Result::NotReady:        return "NotReady";
        case Result::InvalidArgument: return "InvalidArgument";
        case Result::NotInitialized:  return "NotInitialized";
        case Result::Unsupported:     return "Unsupported";
        case Result::OutOfMemory:     return "OutOfMemory";
        case Result::DeviceLost:      return "DeviceLost";
        case Result::BackendFailed:   return "BackendFailed";
        case Result::ShaderFailed:    return "ShaderFailed";
        case Result::NoBackend:       return "NoBackend";
    }
    return "Unrecognized";
}

template <typename E> struct is_flags : std::false_type {};

#define FIZMO_GPU_FLAGS(E) template <> struct is_flags<E> : std::true_type {};

template <typename E, typename = std::enable_if_t<is_flags<E>::value>>
constexpr E operator|(E a, E b) noexcept { using U = std::underlying_type_t<E>; return static_cast<E>(static_cast<U>(a) | static_cast<U>(b)); }

template <typename E, typename = std::enable_if_t<is_flags<E>::value>>
constexpr E operator&(E a, E b) noexcept { using U = std::underlying_type_t<E>; return static_cast<E>(static_cast<U>(a) & static_cast<U>(b)); }

template <typename E, typename = std::enable_if_t<is_flags<E>::value>>
constexpr E& operator|=(E& a, E b) noexcept { return a = a | b; }

template <typename E, typename = std::enable_if_t<is_flags<E>::value>>
constexpr bool any(E set, E flag) noexcept { using U = std::underlying_type_t<E>; return (static_cast<U>(set) & static_cast<U>(flag)) != 0; }

enum class Backend : std::uint8_t { Auto = 0, Vulkan, OpenGL };

inline constexpr const char* backend_name(Backend b) noexcept {
    return b == Backend::Vulkan ? "vulkan" : b == Backend::OpenGL ? "opengl" : "auto";
}

enum class Format : std::uint8_t {
    Undefined = 0,
    R8Unorm,
    RGBA8Unorm,
    RGBA8Srgb,
    BGRA8Unorm,
    R16Float,
    RGBA16Float,
    R32Uint,
    R32Float,
    RG32Float,
    RGB32Float,
    RGBA32Float,
    RGB32Sint,
    D16Unorm,
    D24UnormS8Uint,
    D32Float,
};

inline constexpr bool is_depth(Format f) noexcept { return f == Format::D16Unorm || f == Format::D24UnormS8Uint || f == Format::D32Float; }
inline constexpr bool has_stencil(Format f) noexcept { return f == Format::D24UnormS8Uint; }

inline constexpr std::uint32_t texel_size(Format f) noexcept {
    switch (f) {
        case Format::R8Unorm:        return 1;
        case Format::R16Float:
        case Format::D16Unorm:       return 2;
        case Format::RGBA8Unorm:
        case Format::RGBA8Srgb:
        case Format::BGRA8Unorm:
        case Format::R32Uint:
        case Format::R32Float:
        case Format::D24UnormS8Uint:
        case Format::D32Float:       return 4;
        case Format::RGBA16Float:
        case Format::RG32Float:      return 8;
        case Format::RGB32Float:
        case Format::RGB32Sint:      return 12;
        case Format::RGBA32Float:    return 16;
        default:                     return 0;
    }
}

enum class TextureDimension : std::uint8_t { D2 = 0, Cube, D3 };

enum class TextureUsage : std::uint32_t {
    None         = 0,
    Sampled      = 1u << 0,
    RenderTarget = 1u << 1,
    DepthStencil = 1u << 2,
    Storage      = 1u << 3,
    CopySrc      = 1u << 4,
    CopyDst      = 1u << 5,
};

FIZMO_GPU_FLAGS(TextureUsage)

enum class BufferUsage : std::uint32_t {
    None     = 0,
    Vertex   = 1u << 0,
    Index    = 1u << 1,
    Uniform  = 1u << 2,
    Storage  = 1u << 3,
    Indirect = 1u << 4,
    CopySrc  = 1u << 5,
    CopyDst  = 1u << 6,
};

FIZMO_GPU_FLAGS(BufferUsage)

enum class MemoryAccess : std::uint8_t { GpuOnly = 0, Upload, Readback };

enum class ShaderStage : std::uint32_t {
    None     = 0,
    Vertex   = 1u << 0,
    Fragment = 1u << 1,
    Compute  = 1u << 2,
    Graphics = Vertex | Fragment,
    All      = Vertex | Fragment | Compute,
};

FIZMO_GPU_FLAGS(ShaderStage)

enum class Filter : std::uint8_t { Nearest = 0, Linear };
enum class AddressMode : std::uint8_t { ClampToEdge = 0, Repeat, MirroredRepeat };
enum class CompareOp : std::uint8_t { Never = 0, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
enum class BlendFactor : std::uint8_t { Zero = 0, One, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor, SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha };
enum class CullMode : std::uint8_t { None = 0, Front, Back };
enum class FrontFace : std::uint8_t { CounterClockwise = 0, Clockwise };
enum class Topology : std::uint8_t { TriangleList = 0, LineList, PointList };
enum class IndexType : std::uint8_t { UInt16 = 0, UInt32 };
enum class VertexRate : std::uint8_t { PerVertex = 0, PerInstance };
enum class LoadOp : std::uint8_t { Load = 0, Clear, DontCare };
enum class StoreOp : std::uint8_t { Store = 0, DontCare };
enum class BindingType : std::uint8_t { UniformBuffer = 0, StorageBuffer, SampledTexture, StorageTexture };
enum class StorageAccess : std::uint8_t { ReadOnly = 0, WriteOnly, ReadWrite };
enum class BindGroupLifetime : std::uint8_t { Persistent = 0, Frame };

template <typename T>
class Span {
private:
    const T*    m_data = nullptr;
    std::size_t m_size = 0;

public:
    constexpr Span() noexcept = default;
    constexpr Span(const T* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    constexpr Span(const T& one) noexcept : m_data(&one), m_size(1) {}
#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Winit-list-lifetime"
#endif
    constexpr Span(std::initializer_list<T> list) noexcept : m_data(list.begin()), m_size(list.size()) {}
#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
#endif
    Span(const std::vector<T>& v) noexcept : m_data(v.data()), m_size(v.size()) {}

    template <std::size_t N>
    constexpr Span(const std::array<T, N>& a) noexcept : m_data(a.data()), m_size(N) {}

    template <std::size_t N>
    constexpr Span(const T (&a)[N]) noexcept : m_data(a), m_size(N) {}

    constexpr const T*    data()  const noexcept { return m_data; }
    constexpr std::size_t size()  const noexcept { return m_size; }
    constexpr bool        empty() const noexcept { return m_size == 0; }
    constexpr const T*    begin() const noexcept { return m_data; }
    constexpr const T*    end()   const noexcept { return m_data + m_size; }
    constexpr const T& operator[](std::size_t i) const noexcept { return m_data[i]; }
};

struct Offset2D { std::int32_t  x = 0, y = 0; };
struct Offset3D { std::int32_t  x = 0, y = 0, z = 0; };
struct Extent2D { std::uint32_t width = 0, height = 0; };
struct Extent3D { std::uint32_t width = 1, height = 1, depth = 1; };
struct Rect2D   { Offset2D offset; Extent2D extent; };

struct Viewport {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
    float min_depth = 0.0f, max_depth = 1.0f;
};

struct ClearColor { float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; };

struct TextureRegion {
    Offset3D      origin;
    Extent3D      extent;
    std::uint32_t mip   = 0;
    std::uint32_t layer = 0;
};

enum class AdapterType : std::uint8_t { Unknown = 0, Integrated, Discrete, Virtual, Cpu };

const char* adapter_type_name(AdapterType type) noexcept;

std::uint32_t vendor_id_from_name(const std::string& vendor) noexcept;

struct Caps {
    Backend       backend             = Backend::Auto;
    std::string   device_name;
    std::string   api_version;
    std::uint32_t vendor_id           = 0;
    std::uint32_t device_id           = 0;
    AdapterType   adapter_type        = AdapterType::Unknown;
    bool          luid_valid          = false;
    std::uint64_t luid                = 0;
    std::string   pci_bus;
    std::uint32_t max_samples         = 1;
    std::uint32_t max_texture_size    = 4096;
    std::uint32_t max_textures        = 16;
    std::uint64_t uniform_alignment   = 256;
    std::uint64_t max_uniform_range   = 16384;
    std::uint32_t max_push_constants  = 128;
    bool          compute             = false;
    bool          storage_buffers     = false;
    bool          multi_draw_indirect = false;
    bool          base_instance       = false;
    bool          depth_clamp         = false;
    bool          timestamps          = false;
    bool          debug_labels        = false;
};

struct MemoryStats {
    bool          valid        = false;
    bool          measured     = false;
    std::uint64_t used         = 0;
    std::uint64_t budget       = 0;
    std::uint64_t total        = 0;
    std::uint64_t shared_used  = 0;
    std::uint64_t shared_total = 0;
};

struct BufferDesc {
    std::uint64_t size   = 0;
    BufferUsage   usage  = BufferUsage::Vertex;
    MemoryAccess  memory = MemoryAccess::GpuOnly;
    const char*   name   = nullptr;
};

struct TextureDesc {
    TextureDimension dimension  = TextureDimension::D2;
    Extent3D         extent;
    Format           format     = Format::RGBA8Unorm;
    std::uint32_t    mip_levels = 1;
    std::uint32_t    samples    = 1;
    TextureUsage     usage      = TextureUsage::Sampled | TextureUsage::CopyDst;
    const char*      name       = nullptr;
};

struct SamplerDesc {
    Filter      mag_filter     = Filter::Linear;
    Filter      min_filter     = Filter::Linear;
    Filter      mip_filter     = Filter::Nearest;
    AddressMode address_u      = AddressMode::ClampToEdge;
    AddressMode address_v      = AddressMode::ClampToEdge;
    AddressMode address_w      = AddressMode::ClampToEdge;
    bool        compare        = false;
    CompareOp   compare_op     = CompareOp::LessEqual;
    float       max_lod        = 0.0f;
    float       max_anisotropy = 0.0f;
    const char* name           = nullptr;
};

enum class GlslResource : std::uint8_t { Texture = 0, UniformBlock, PushBlock, StorageImage, StorageBlock };

struct GlslBinding {
    const char*   name    = nullptr;
    GlslResource  kind    = GlslResource::Texture;
    std::uint32_t set     = 0;
    std::uint32_t binding = 0;
    std::uint32_t count   = 1;
};

struct ShaderDesc {
    ShaderStage         stage         = ShaderStage::Vertex;
    Span<std::uint32_t> spirv;
    const char* const*  glsl          = nullptr;
    std::size_t         glsl_pieces   = 0;
    int                 glsl_version  = 330;
    Span<GlslBinding>   glsl_bindings;
    const char*         name          = nullptr;
};

struct BindGroupLayoutEntry {
    std::uint32_t binding        = 0;
    BindingType   type           = BindingType::SampledTexture;
    ShaderStage   stages         = ShaderStage::Graphics;
    std::uint32_t count          = 1;
    Format        storage_format = Format::Undefined;
    StorageAccess access         = StorageAccess::ReadWrite;
};

struct BindGroupLayoutDesc {
    Span<BindGroupLayoutEntry> entries;
    const char*                name = nullptr;
};

struct VertexBinding {
    std::uint32_t binding = 0;
    std::uint32_t stride  = 0;
    VertexRate    rate    = VertexRate::PerVertex;
};

struct VertexAttribute {
    std::uint32_t location = 0;
    std::uint32_t binding  = 0;
    Format        format   = Format::RG32Float;
    std::uint32_t offset   = 0;
};

struct BlendState {
    bool        enable    = false;
    BlendFactor src_color = BlendFactor::One;
    BlendFactor dst_color = BlendFactor::Zero;
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::Zero;
    bool        write     = true;

    static BlendState opaque() noexcept { return {}; }

    static BlendState make(BlendFactor sc, BlendFactor dc, BlendFactor sa, BlendFactor da) noexcept;

    static BlendState alpha() noexcept;
    static BlendState premultiplied() noexcept;
    static BlendState additive() noexcept { return make(BlendFactor::One, BlendFactor::One, BlendFactor::One, BlendFactor::One); }
};

struct DepthState {
    bool      test          = false;
    bool      write         = false;
    CompareOp compare       = CompareOp::LessEqual;
    bool      clamp         = false;
    bool      bias          = false;
    float     bias_constant = 0.0f;
    float     bias_slope    = 0.0f;
};

struct FrameInfo {
    std::uint32_t index  = 0;
    std::uint64_t serial = 0;
    Extent2D      extent;
};

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_TYPES_HPP
