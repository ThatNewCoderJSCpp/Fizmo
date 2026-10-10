#ifndef FIZMO_VULKAN_TYPES_HPP
#define FIZMO_VULKAN_TYPES_HPP

#include "compat.hpp"
#include <type_traits>
#include <initializer_list>
#include <vector>
#include <array>

namespace fizmo {
namespace vulkan {

enum class Result : std::int32_t {
    Success             = VK_SUCCESS,
    NotReady            = VK_NOT_READY,
    Timeout             = VK_TIMEOUT,
    Incomplete          = VK_INCOMPLETE,
    Suboptimal          = VK_SUBOPTIMAL_KHR,
    OutOfHostMemory     = VK_ERROR_OUT_OF_HOST_MEMORY,
    OutOfDeviceMemory   = VK_ERROR_OUT_OF_DEVICE_MEMORY,
    InitializationFailed= VK_ERROR_INITIALIZATION_FAILED,
    DeviceLost          = VK_ERROR_DEVICE_LOST,
    MemoryMapFailed     = VK_ERROR_MEMORY_MAP_FAILED,
    LayerNotPresent     = VK_ERROR_LAYER_NOT_PRESENT,
    ExtensionNotPresent = VK_ERROR_EXTENSION_NOT_PRESENT,
    FeatureNotPresent   = VK_ERROR_FEATURE_NOT_PRESENT,
    IncompatibleDriver  = VK_ERROR_INCOMPATIBLE_DRIVER,
    TooManyObjects      = VK_ERROR_TOO_MANY_OBJECTS,
    FormatNotSupported  = VK_ERROR_FORMAT_NOT_SUPPORTED,
    SurfaceLost         = VK_ERROR_SURFACE_LOST_KHR,
    OutOfDate           = VK_ERROR_OUT_OF_DATE_KHR,
    Unknown             = VK_ERROR_UNKNOWN,

    LibraryNotFound     = -0x7F000001,
    MissingEntryPoint   = -0x7F000002,
    NoSuitableDevice    = -0x7F000003,
    NoSuitableMemory    = -0x7F000004,
    InvalidArgument     = -0x7F000005,
    NotInitialized      = -0x7F000006,
    FileNotFound        = -0x7F000007,
};

inline constexpr Result to_result(VkResult r) noexcept { return static_cast<Result>(r); }
inline constexpr bool   succeeded(Result r) noexcept { return static_cast<std::int32_t>(r) >= 0; }
inline constexpr bool   failed(Result r)    noexcept { return static_cast<std::int32_t>(r) < 0; }

inline constexpr_string to_string(Result r) noexcept {
    switch (r) {
        case Result::Success:              return "Success";
        case Result::NotReady:             return "NotReady";
        case Result::Timeout:              return "Timeout";
        case Result::Incomplete:           return "Incomplete";
        case Result::Suboptimal:           return "Suboptimal";
        case Result::OutOfHostMemory:      return "OutOfHostMemory";
        case Result::OutOfDeviceMemory:    return "OutOfDeviceMemory";
        case Result::InitializationFailed: return "InitializationFailed";
        case Result::DeviceLost:           return "DeviceLost";
        case Result::MemoryMapFailed:      return "MemoryMapFailed";
        case Result::LayerNotPresent:      return "LayerNotPresent";
        case Result::ExtensionNotPresent:  return "ExtensionNotPresent";
        case Result::FeatureNotPresent:    return "FeatureNotPresent";
        case Result::IncompatibleDriver:   return "IncompatibleDriver";
        case Result::TooManyObjects:       return "TooManyObjects";
        case Result::FormatNotSupported:   return "FormatNotSupported";
        case Result::SurfaceLost:          return "SurfaceLost";
        case Result::OutOfDate:            return "OutOfDate";
        case Result::Unknown:              return "Unknown";
        case Result::LibraryNotFound:      return "LibraryNotFound";
        case Result::MissingEntryPoint:    return "MissingEntryPoint";
        case Result::NoSuitableDevice:     return "NoSuitableDevice";
        case Result::NoSuitableMemory:     return "NoSuitableMemory";
        case Result::InvalidArgument:      return "InvalidArgument";
        case Result::NotInitialized:       return "NotInitialized";
        case Result::FileNotFound:         return "FileNotFound";
    }
    return "Unrecognized";
}

template <typename E> struct enable_bitmask : std::false_type {};

#define FIZMO_VK_BITMASK(E) template <> struct enable_bitmask<E> : std::true_type {};

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr E operator|(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr E operator&(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
}

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr E operator~(E a) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(a));
}

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr E& operator|=(E& a, E b) noexcept { return a = a | b; }

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr E& operator&=(E& a, E b) noexcept { return a = a & b; }

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr bool has_flag(E set, E flag) noexcept {
    using U = std::underlying_type_t<E>;
    return (static_cast<U>(set) & static_cast<U>(flag)) == static_cast<U>(flag);
}

template <typename E, typename = std::enable_if_t<enable_bitmask<E>::value>>
constexpr bool any_flag(E set) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<U>(set) != 0;
}

enum class DeviceType : std::uint32_t {
    Other      = VK_PHYSICAL_DEVICE_TYPE_OTHER,
    Integrated = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
    Discrete   = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
    Virtual    = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
    Cpu        = VK_PHYSICAL_DEVICE_TYPE_CPU,
};

enum class DebugSeverity : std::uint32_t {
    Verbose = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
    Info    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
    Warning = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
    Error   = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};

enum class QueueType : std::uint8_t { Graphics, Compute, Transfer };

enum class Format : std::uint32_t {
    Undefined        = VK_FORMAT_UNDEFINED,
    R8Unorm          = VK_FORMAT_R8_UNORM,
    RG8Unorm         = VK_FORMAT_R8G8_UNORM,
    RGBA8Unorm       = VK_FORMAT_R8G8B8A8_UNORM,
    RGBA8Srgb        = VK_FORMAT_R8G8B8A8_SRGB,
    BGRA8Unorm       = VK_FORMAT_B8G8R8A8_UNORM,
    BGRA8Srgb        = VK_FORMAT_B8G8R8A8_SRGB,
    A2BGR10Unorm     = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
    R16Float         = VK_FORMAT_R16_SFLOAT,
    RG16Float        = VK_FORMAT_R16G16_SFLOAT,
    RGBA16Float      = VK_FORMAT_R16G16B16A16_SFLOAT,
    R32Uint          = VK_FORMAT_R32_UINT,
    R32Float         = VK_FORMAT_R32_SFLOAT,
    RG32Float        = VK_FORMAT_R32G32_SFLOAT,
    RGB32Float       = VK_FORMAT_R32G32B32_SFLOAT,
    RGBA32Float      = VK_FORMAT_R32G32B32A32_SFLOAT,
    RGB32Sint        = VK_FORMAT_R32G32B32_SINT,
    D16Unorm         = VK_FORMAT_D16_UNORM,
    D32Float         = VK_FORMAT_D32_SFLOAT,
    D24UnormS8Uint   = VK_FORMAT_D24_UNORM_S8_UINT,
    D32FloatS8Uint   = VK_FORMAT_D32_SFLOAT_S8_UINT,
};

inline constexpr bool is_depth_format(Format f) noexcept {
    return f == Format::D16Unorm || f == Format::D32Float || f == Format::D24UnormS8Uint || f == Format::D32FloatS8Uint;
}

inline constexpr bool has_stencil(Format f) noexcept {
    return f == Format::D24UnormS8Uint || f == Format::D32FloatS8Uint;
}

enum class PresentMode : std::uint32_t {
    Immediate   = VK_PRESENT_MODE_IMMEDIATE_KHR,
    Mailbox     = VK_PRESENT_MODE_MAILBOX_KHR,
    Fifo        = VK_PRESENT_MODE_FIFO_KHR,        // vsync, always supported
    FifoRelaxed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
};

enum class MemoryUsage : std::uint8_t {
    GpuOnly = 0,    
    CpuToGpu,   
    GpuToCpu   
};

enum class BufferUsage : std::uint32_t {
    None          = 0,
    TransferSrc   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst   = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    Uniform       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    Storage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    Index         = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    Vertex        = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Indirect      = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    DeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
};

FIZMO_VK_BITMASK(BufferUsage)

enum class ImageUsage : std::uint32_t {
    None            = 0,
    TransferSrc     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst     = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    Sampled         = VK_IMAGE_USAGE_SAMPLED_BIT,
    Storage         = VK_IMAGE_USAGE_STORAGE_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
};

FIZMO_VK_BITMASK(ImageUsage)

enum class ImageAspect : std::uint32_t {
    None    = 0,
    Color   = VK_IMAGE_ASPECT_COLOR_BIT,
    Depth   = VK_IMAGE_ASPECT_DEPTH_BIT,
    Stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
};

FIZMO_VK_BITMASK(ImageAspect)

inline constexpr ImageAspect aspect_of(Format f) noexcept {
    if (!is_depth_format(f)) return ImageAspect::Color;
    return has_stencil(f) ? (ImageAspect::Depth | ImageAspect::Stencil) : ImageAspect::Depth;
}

enum class ImageType : std::uint8_t { Image2D = 0, Image3D, Cube };

enum class ImageLayout : std::uint32_t {
    Undefined        = VK_IMAGE_LAYOUT_UNDEFINED,
    General          = VK_IMAGE_LAYOUT_GENERAL,
    ColorAttachment  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    DepthAttachment  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    DepthReadOnly    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    ShaderReadOnly   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    TransferSrc      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    TransferDst      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    Present          = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};

enum class SampleCount : std::uint32_t {
    X1  = VK_SAMPLE_COUNT_1_BIT,
    X2  = VK_SAMPLE_COUNT_2_BIT,
    X4  = VK_SAMPLE_COUNT_4_BIT,
    X8  = VK_SAMPLE_COUNT_8_BIT,
    X16 = VK_SAMPLE_COUNT_16_BIT,
};

enum class ShaderStage : std::uint32_t {
    None     = 0,
    Vertex   = VK_SHADER_STAGE_VERTEX_BIT,
    Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute  = VK_SHADER_STAGE_COMPUTE_BIT,
    Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
    AllGraphics = VK_SHADER_STAGE_ALL_GRAPHICS,
    All      = VK_SHADER_STAGE_ALL,
};

FIZMO_VK_BITMASK(ShaderStage)

enum class PipelineStage : std::uint64_t {
    None                 = VK_PIPELINE_STAGE_2_NONE,
    TopOfPipe            = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    DrawIndirect         = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
    VertexInput          = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
    VertexShader         = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    FragmentShader       = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    EarlyFragmentTests   = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    LateFragmentTests    = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    ColorAttachmentOutput= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    ComputeShader        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    Transfer             = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
    BottomOfPipe         = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    Host                 = VK_PIPELINE_STAGE_2_HOST_BIT,
    AllGraphics          = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    AllCommands          = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
};

FIZMO_VK_BITMASK(PipelineStage)

enum class Access : std::uint64_t {
    None                 = VK_ACCESS_2_NONE,
    IndirectRead         = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
    IndexRead            = VK_ACCESS_2_INDEX_READ_BIT,
    VertexRead           = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
    UniformRead          = VK_ACCESS_2_UNIFORM_READ_BIT,
    ShaderRead           = VK_ACCESS_2_SHADER_READ_BIT,
    ShaderWrite          = VK_ACCESS_2_SHADER_WRITE_BIT,
    ColorAttachmentRead  = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
    ColorAttachmentWrite = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    DepthAttachmentRead  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    DepthAttachmentWrite = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    TransferRead         = VK_ACCESS_2_TRANSFER_READ_BIT,
    TransferWrite        = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    HostRead             = VK_ACCESS_2_HOST_READ_BIT,
    HostWrite            = VK_ACCESS_2_HOST_WRITE_BIT,
    MemoryRead           = VK_ACCESS_2_MEMORY_READ_BIT,
    MemoryWrite          = VK_ACCESS_2_MEMORY_WRITE_BIT,
};

FIZMO_VK_BITMASK(Access)

enum class LoadOp : std::uint32_t {
    Load     = VK_ATTACHMENT_LOAD_OP_LOAD,
    Clear    = VK_ATTACHMENT_LOAD_OP_CLEAR,
    DontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
};

enum class StoreOp : std::uint32_t {
    Store    = VK_ATTACHMENT_STORE_OP_STORE,
    DontCare = VK_ATTACHMENT_STORE_OP_DONT_CARE,
};

enum class Topology : std::uint32_t {
    PointList     = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    LineList      = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    LineStrip     = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    TriangleList  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    TriangleFan   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
};

enum class PolygonMode : std::uint32_t {
    Fill  = VK_POLYGON_MODE_FILL,
    Line  = VK_POLYGON_MODE_LINE,
    Point = VK_POLYGON_MODE_POINT,
};

enum class CullMode : std::uint32_t {
    None         = VK_CULL_MODE_NONE,
    Front        = VK_CULL_MODE_FRONT_BIT,
    Back         = VK_CULL_MODE_BACK_BIT,
    FrontAndBack = VK_CULL_MODE_FRONT_AND_BACK,
};

enum class FrontFace : std::uint32_t {
    CounterClockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    Clockwise        = VK_FRONT_FACE_CLOCKWISE,
};

enum class CompareOp : std::uint32_t {
    Never        = VK_COMPARE_OP_NEVER,
    Less         = VK_COMPARE_OP_LESS,
    Equal        = VK_COMPARE_OP_EQUAL,
    LessEqual    = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater      = VK_COMPARE_OP_GREATER,
    NotEqual     = VK_COMPARE_OP_NOT_EQUAL,
    GreaterEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always       = VK_COMPARE_OP_ALWAYS,
};

enum class BlendFactor : std::uint32_t {
    Zero             = VK_BLEND_FACTOR_ZERO,
    One              = VK_BLEND_FACTOR_ONE,
    SrcColor         = VK_BLEND_FACTOR_SRC_COLOR,
    OneMinusSrcColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    DstColor         = VK_BLEND_FACTOR_DST_COLOR,
    OneMinusDstColor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    SrcAlpha         = VK_BLEND_FACTOR_SRC_ALPHA,
    OneMinusSrcAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    DstAlpha         = VK_BLEND_FACTOR_DST_ALPHA,
    OneMinusDstAlpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
};

enum class BlendOp : std::uint32_t {
    Add             = VK_BLEND_OP_ADD,
    Subtract        = VK_BLEND_OP_SUBTRACT,
    ReverseSubtract = VK_BLEND_OP_REVERSE_SUBTRACT,
    Min             = VK_BLEND_OP_MIN,
    Max             = VK_BLEND_OP_MAX,
};

enum class ColorWrite : std::uint32_t {
    None = 0,
    R    = VK_COLOR_COMPONENT_R_BIT,
    G    = VK_COLOR_COMPONENT_G_BIT,
    B    = VK_COLOR_COMPONENT_B_BIT,
    A    = VK_COLOR_COMPONENT_A_BIT,
    All  = R | G | B | A,
};

FIZMO_VK_BITMASK(ColorWrite)

enum class Filter : std::uint32_t {
    Nearest = VK_FILTER_NEAREST,
    Linear  = VK_FILTER_LINEAR,
};

enum class MipmapMode : std::uint32_t {
    Nearest = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    Linear  = VK_SAMPLER_MIPMAP_MODE_LINEAR,
};

enum class AddressMode : std::uint32_t {
    Repeat         = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    MirroredRepeat = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    ClampToEdge    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    ClampToBorder  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
};

enum class DescriptorType : std::uint32_t {
    Sampler              = VK_DESCRIPTOR_TYPE_SAMPLER,
    CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    SampledImage         = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    StorageImage         = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    UniformBuffer        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    StorageBuffer        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    UniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    StorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
};

enum class IndexType : std::uint32_t {
    UInt16 = VK_INDEX_TYPE_UINT16,
    UInt32 = VK_INDEX_TYPE_UINT32,
};

enum class VertexRate : std::uint32_t {
    PerVertex   = VK_VERTEX_INPUT_RATE_VERTEX,
    PerInstance = VK_VERTEX_INPUT_RATE_INSTANCE,
};

enum class BindPoint : std::uint32_t {
    Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    Compute  = VK_PIPELINE_BIND_POINT_COMPUTE,
};

inline constexpr VkFormat              to_vk(Format v)         noexcept { return static_cast<VkFormat>(v); }
inline constexpr VkPresentModeKHR      to_vk(PresentMode v)    noexcept { return static_cast<VkPresentModeKHR>(v); }
inline constexpr VkBufferUsageFlags    to_vk(BufferUsage v)    noexcept { return static_cast<VkBufferUsageFlags>(v); }
inline constexpr VkImageUsageFlags     to_vk(ImageUsage v)     noexcept { return static_cast<VkImageUsageFlags>(v); }
inline constexpr VkImageAspectFlags    to_vk(ImageAspect v)    noexcept { return static_cast<VkImageAspectFlags>(v); }
inline constexpr VkImageLayout         to_vk(ImageLayout v)    noexcept { return static_cast<VkImageLayout>(v); }
inline constexpr VkSampleCountFlagBits to_vk(SampleCount v)    noexcept { return static_cast<VkSampleCountFlagBits>(v); }
inline constexpr VkShaderStageFlags    to_vk(ShaderStage v)    noexcept { return static_cast<VkShaderStageFlags>(v); }
inline constexpr VkPipelineStageFlags2 to_vk(PipelineStage v)  noexcept { return static_cast<VkPipelineStageFlags2>(v); }
inline constexpr VkAccessFlags2        to_vk(Access v)         noexcept { return static_cast<VkAccessFlags2>(v); }
inline constexpr VkAttachmentLoadOp    to_vk(LoadOp v)         noexcept { return static_cast<VkAttachmentLoadOp>(v); }
inline constexpr VkAttachmentStoreOp   to_vk(StoreOp v)        noexcept { return static_cast<VkAttachmentStoreOp>(v); }
inline constexpr VkPrimitiveTopology   to_vk(Topology v)       noexcept { return static_cast<VkPrimitiveTopology>(v); }
inline constexpr VkPolygonMode         to_vk(PolygonMode v)    noexcept { return static_cast<VkPolygonMode>(v); }
inline constexpr VkCullModeFlags       to_vk(CullMode v)       noexcept { return static_cast<VkCullModeFlags>(v); }
inline constexpr VkFrontFace           to_vk(FrontFace v)      noexcept { return static_cast<VkFrontFace>(v); }
inline constexpr VkCompareOp           to_vk(CompareOp v)      noexcept { return static_cast<VkCompareOp>(v); }
inline constexpr VkBlendFactor         to_vk(BlendFactor v)    noexcept { return static_cast<VkBlendFactor>(v); }
inline constexpr VkBlendOp             to_vk(BlendOp v)        noexcept { return static_cast<VkBlendOp>(v); }
inline constexpr VkColorComponentFlags to_vk(ColorWrite v)     noexcept { return static_cast<VkColorComponentFlags>(v); }
inline constexpr VkFilter              to_vk(Filter v)         noexcept { return static_cast<VkFilter>(v); }
inline constexpr VkSamplerMipmapMode   to_vk(MipmapMode v)     noexcept { return static_cast<VkSamplerMipmapMode>(v); }
inline constexpr VkSamplerAddressMode  to_vk(AddressMode v)    noexcept { return static_cast<VkSamplerAddressMode>(v); }
inline constexpr VkDescriptorType      to_vk(DescriptorType v) noexcept { return static_cast<VkDescriptorType>(v); }
inline constexpr VkIndexType           to_vk(IndexType v)      noexcept { return static_cast<VkIndexType>(v); }
inline constexpr VkVertexInputRate     to_vk(VertexRate v)     noexcept { return static_cast<VkVertexInputRate>(v); }
inline constexpr VkPipelineBindPoint   to_vk(BindPoint v)      noexcept { return static_cast<VkPipelineBindPoint>(v); }

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

    constexpr const T*    data()  const noexcept { return m_data; }
    constexpr std::size_t size()  const noexcept { return m_size; }
    constexpr bool        empty() const noexcept { return m_size == 0; }
    constexpr const T*    begin() const noexcept { return m_data; }
    constexpr const T*    end()   const noexcept { return m_data + m_size; }
    constexpr const T& operator[](std::size_t i) const noexcept { return m_data[i]; }
    constexpr std::uint32_t count() const noexcept { return static_cast<std::uint32_t>(m_size); }
};

struct Extent2D { std::uint32_t width = 0, height = 0; };
struct Extent3D { std::uint32_t width = 0, height = 0, depth = 1; };
struct Offset2D { std::int32_t x = 0, y = 0; };
struct Rect2D   { Offset2D offset; Extent2D extent; };

struct Viewport {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
    float min_depth = 0.0f, max_depth = 1.0f;
};

struct ClearColor { float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; };
struct ClearDepth { float depth = 1.0f; std::uint32_t stencil = 0; };

inline constexpr VkExtent2D to_vk(Extent2D e) noexcept { return { e.width, e.height }; }
inline constexpr VkExtent3D to_vk(Extent3D e) noexcept { return { e.width, e.height, e.depth }; }
inline constexpr VkOffset2D to_vk(Offset2D o) noexcept { return { o.x, o.y }; }
inline constexpr VkRect2D   to_vk(Rect2D r)   noexcept { return { to_vk(r.offset), to_vk(r.extent) }; }
inline constexpr VkViewport to_vk(const Viewport& v) noexcept { return { v.x, v.y, v.width, v.height, v.min_depth, v.max_depth }; }

inline VkClearValue to_vk(const ClearColor& c) noexcept {
    VkClearValue v{};
    v.color.float32[0] = c.r; v.color.float32[1] = c.g;
    v.color.float32[2] = c.b; v.color.float32[3] = c.a;
    return v;
}

inline VkClearValue to_vk(const ClearDepth& d) noexcept {
    VkClearValue v{};
    v.depthStencil.depth   = d.depth;
    v.depthStencil.stencil = d.stencil;
    return v;
}

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_TYPES_HPP