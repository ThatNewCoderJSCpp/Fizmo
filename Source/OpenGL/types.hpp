#ifndef FIZMO_OPENGL_TYPES_HPP
#define FIZMO_OPENGL_TYPES_HPP

#include "compat.hpp"
#include <cstdint>

namespace fizmo {
namespace opengl {

enum class Result : std::int32_t {
    Success               = 0,
    LibraryNotFound       = -1,
    MissingEntryPoint     = -2,
    NoSuitableConfig      = -3,
    ContextCreationFailed = -4,
    VersionTooLow         = -5,
    CompileFailed         = -6,
    LinkFailed            = -7,
    IncompleteFramebuffer = -8,
    OutOfMemory           = -9,
    InvalidArgument       = -10,
    NotInitialized        = -11,
    Unsupported           = -12,
};

inline constexpr bool succeeded(Result r) noexcept { return static_cast<std::int32_t>(r) >= 0; }
inline constexpr bool failed(Result r)    noexcept { return static_cast<std::int32_t>(r) < 0; }

inline constexpr_string to_string(Result r) noexcept {
    switch (r) {
        case Result::Success:               return "Success";
        case Result::LibraryNotFound:       return "LibraryNotFound";
        case Result::MissingEntryPoint:     return "MissingEntryPoint";
        case Result::NoSuitableConfig:      return "NoSuitableConfig";
        case Result::ContextCreationFailed: return "ContextCreationFailed";
        case Result::VersionTooLow:         return "VersionTooLow";
        case Result::CompileFailed:         return "CompileFailed";
        case Result::LinkFailed:            return "LinkFailed";
        case Result::IncompleteFramebuffer: return "IncompleteFramebuffer";
        case Result::OutOfMemory:           return "OutOfMemory";
        case Result::InvalidArgument:       return "InvalidArgument";
        case Result::NotInitialized:        return "NotInitialized";
        case Result::Unsupported:           return "Unsupported";
    }
    return "Unrecognized";
}

struct Offset2D {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct Extent2D {
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
};

struct Extent3D {
    std::uint32_t width  = 1;
    std::uint32_t height = 1;
    std::uint32_t depth  = 1;
};

struct Rect2D {
    Offset2D offset;
    Extent2D extent;
};

struct Viewport {
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f, min_depth = 0.0f, max_depth = 1.0f;
};

struct ClearColor {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

enum class Format : std::uint8_t {
    Undefined = 0,
    R32Float,
    RG32Float,
    RGB32Float,
    RGBA32Float,
    R32Uint,
    RGB32Sint,
    RGBA8Unorm,
    D16Unorm,
    D24UnormS8Uint,
    D32Float,
};

struct AttributeFormat {
    native::Int  components = 0;
    native::Enum type       = gl::FLOAT;
    bool         normalized = false;
    bool         integer    = false;
};

inline constexpr AttributeFormat attribute_format(Format f) noexcept {
    switch (f) {
        case Format::R32Float:    return { 1, gl::FLOAT, false, false };
        case Format::RG32Float:   return { 2, gl::FLOAT, false, false };
        case Format::RGB32Float:  return { 3, gl::FLOAT, false, false };
        case Format::RGBA32Float: return { 4, gl::FLOAT, false, false };
        case Format::R32Uint:     return { 1, gl::UNSIGNED_INT, false, true };
        case Format::RGB32Sint:   return { 3, gl::INT, false, true };
        case Format::RGBA8Unorm:  return { 4, gl::UNSIGNED_BYTE, true, false };
        default:                  return {};
    }
}

struct TextureFormat {
    native::Enum internal = gl::RGBA8;
    native::Enum format   = gl::RGBA;
    native::Enum type     = gl::UNSIGNED_BYTE;
};

inline constexpr TextureFormat texture_format(Format f) noexcept {
    switch (f) {
        case Format::R32Float:       return { gl::R32F, gl::RED, gl::FLOAT };
        case Format::RGBA32Float:    return { gl::RGBA32F, gl::RGBA, gl::FLOAT };
        case Format::RGBA8Unorm:     return { gl::RGBA8, gl::RGBA, gl::UNSIGNED_BYTE };
        case Format::D16Unorm:       return { gl::DEPTH_COMPONENT16, gl::DEPTH_COMPONENT, gl::UNSIGNED_SHORT };
        case Format::D24UnormS8Uint: return { gl::DEPTH24_STENCIL8, gl::DEPTH_STENCIL, gl::UNSIGNED_INT_24_8 };
        case Format::D32Float:       return { gl::DEPTH_COMPONENT32F, gl::DEPTH_COMPONENT, gl::FLOAT };
        default:                     return {};
    }
}

inline constexpr bool is_depth(Format f) noexcept { return f == Format::D16Unorm || f == Format::D24UnormS8Uint || f == Format::D32Float; }
inline constexpr bool has_stencil(Format f) noexcept { return f == Format::D24UnormS8Uint; }

enum class TextureType : std::uint8_t { Texture2D, Cube, Texture3D };

enum class Filter : std::uint8_t { Nearest, Linear };

enum class AddressMode : std::uint8_t { ClampToEdge, Repeat, MirroredRepeat };

enum class CompareOp : std::uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

enum class BlendFactor : std::uint8_t { Zero, One, SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha, SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor };

enum class CullMode : std::uint8_t { None, Front, Back };

enum class FrontFace : std::uint8_t { CounterClockwise, Clockwise };

enum class IndexType : std::uint8_t { UInt16, UInt32 };

enum class VertexRate : std::uint8_t { PerVertex, PerInstance };

enum class BufferUsage : std::uint8_t { Static, Dynamic, Stream };

enum class BufferTarget : std::uint8_t { Vertex, Index, Uniform, Indirect, Readback };

enum class Topology : std::uint8_t { Triangles, Lines };

inline constexpr native::Enum to_gl(CompareOp op) noexcept {
    switch (op) {
        case CompareOp::Never:        return gl::NEVER;
        case CompareOp::Less:         return gl::LESS;
        case CompareOp::Equal:        return gl::EQUAL;
        case CompareOp::LessEqual:    return gl::LEQUAL;
        case CompareOp::Greater:      return gl::GREATER;
        case CompareOp::NotEqual:     return gl::NOTEQUAL;
        case CompareOp::GreaterEqual: return gl::GEQUAL;
        case CompareOp::Always:       return gl::ALWAYS;
    }
    return gl::ALWAYS;
}

inline constexpr native::Enum to_gl(BlendFactor f) noexcept {
    switch (f) {
        case BlendFactor::Zero:             return gl::ZERO;
        case BlendFactor::One:              return gl::ONE;
        case BlendFactor::SrcAlpha:         return gl::SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return gl::ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return gl::DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return gl::ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor:         return gl::SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return gl::ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return gl::DST_COLOR;
        case BlendFactor::OneMinusDstColor: return gl::ONE_MINUS_DST_COLOR;
    }
    return gl::ONE;
}

inline constexpr native::Enum to_gl(Filter f) noexcept { return f == Filter::Linear ? gl::LINEAR : gl::NEAREST; }

inline constexpr native::Enum to_gl(AddressMode m) noexcept {
    return m == AddressMode::Repeat ? gl::REPEAT : m == AddressMode::MirroredRepeat ? gl::MIRRORED_REPEAT : gl::CLAMP_TO_EDGE;
}

inline constexpr native::Enum to_gl(BufferTarget t) noexcept {
    switch (t) {
        case BufferTarget::Vertex:   return gl::ARRAY_BUFFER;
        case BufferTarget::Index:    return gl::ELEMENT_ARRAY_BUFFER;
        case BufferTarget::Uniform:  return gl::UNIFORM_BUFFER;
        case BufferTarget::Indirect: return gl::DRAW_INDIRECT_BUFFER;
        case BufferTarget::Readback: return gl::PIXEL_PACK_BUFFER;
    }
    return gl::ARRAY_BUFFER;
}

inline constexpr native::Enum to_gl(BufferUsage u) noexcept {
    return u == BufferUsage::Stream ? gl::STREAM_DRAW : u == BufferUsage::Dynamic ? gl::DYNAMIC_DRAW : gl::STATIC_DRAW;
}

inline constexpr native::Enum to_gl(TextureType t) noexcept {
    return t == TextureType::Cube ? gl::TEXTURE_CUBE_MAP : t == TextureType::Texture3D ? gl::TEXTURE_3D : gl::TEXTURE_2D;
}

inline constexpr native::Enum to_gl(Topology t) noexcept { return t == Topology::Lines ? gl::LINES : gl::TRIANGLES; }

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_TYPES_HPP
