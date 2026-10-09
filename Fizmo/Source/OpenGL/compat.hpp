#ifndef FIZMO_OPENGL_COMPAT_HPP
#define FIZMO_OPENGL_COMPAT_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstddef>
#include <cstdint>

#if defined(OS_LINUX)
    #include <dlfcn.h>
    #include "../x11_compat.hpp"
#endif

#if defined(OS_WINDOWS)
    #define FIZMO_GL_APIENTRY __stdcall
#else
    #define FIZMO_GL_APIENTRY
#endif

namespace fizmo {
namespace opengl {
namespace native {

using Enum     = unsigned int;
using Boolean  = unsigned char;
using Bitfield = unsigned int;
using Byte     = signed char;
using Ubyte    = unsigned char;
using Short    = short;
using Ushort   = unsigned short;
using Int      = int;
using Uint     = unsigned int;
using Sizei    = int;
using Float    = float;
using Double   = double;
using Char     = char;
using Intptr   = std::intptr_t;
using Sizeiptr = std::intptr_t;
using Int64    = std::int64_t;
using Uint64   = std::uint64_t;
using Sync     = struct __GLsync*;

using DebugProc = void (FIZMO_GL_APIENTRY*)(Enum source, Enum type, Uint id, Enum severity, Sizei length, const Char* message, const void* user);

#if defined(OS_WINDOWS)
using LibraryHandle = ::HMODULE;
#else
using LibraryHandle = void*;
#endif

} // namespace native

namespace gl {

using native::Enum;

inline constexpr Enum NONE                                  = 0;
inline constexpr Enum ZERO                                  = 0;
inline constexpr Enum ONE                                   = 1;
inline constexpr Enum OUT_OF_MEMORY                         = 0x0505;

inline constexpr Enum POINTS                                = 0x0000;
inline constexpr Enum LINES                                 = 0x0001;
inline constexpr Enum TRIANGLES                             = 0x0004;

inline constexpr Enum BYTE                                  = 0x1400;
inline constexpr Enum UNSIGNED_BYTE                         = 0x1401;
inline constexpr Enum SHORT                                 = 0x1402;
inline constexpr Enum UNSIGNED_SHORT                        = 0x1403;
inline constexpr Enum INT                                   = 0x1404;
inline constexpr Enum UNSIGNED_INT                          = 0x1405;
inline constexpr Enum FLOAT                                 = 0x1406;
inline constexpr Enum UNSIGNED_INT_24_8                     = 0x84FA;

inline constexpr Enum DEPTH_BUFFER_BIT                      = 0x00000100;
inline constexpr Enum STENCIL_BUFFER_BIT                    = 0x00000400;
inline constexpr Enum COLOR_BUFFER_BIT                      = 0x00004000;

inline constexpr Enum CULL_FACE                             = 0x0B44;
inline constexpr Enum DEPTH_TEST                            = 0x0B71;
inline constexpr Enum BLEND                                 = 0x0BE2;
inline constexpr Enum SCISSOR_TEST                          = 0x0C11;
inline constexpr Enum POLYGON_OFFSET_FILL                   = 0x8037;
inline constexpr Enum MULTISAMPLE                           = 0x809D;
inline constexpr Enum DEPTH_CLAMP                           = 0x864F;
inline constexpr Enum TEXTURE_CUBE_MAP_SEAMLESS             = 0x884F;
inline constexpr Enum FRAMEBUFFER_SRGB                      = 0x8DB9;
inline constexpr Enum DEBUG_OUTPUT_SYNCHRONOUS              = 0x8242;
inline constexpr Enum DEBUG_OUTPUT                          = 0x92E0;

inline constexpr Enum FRONT                                 = 0x0404;
inline constexpr Enum BACK                                  = 0x0405;
inline constexpr Enum FRONT_AND_BACK                        = 0x0408;
inline constexpr Enum CW                                    = 0x0900;
inline constexpr Enum CCW                                   = 0x0901;

inline constexpr Enum NEVER                                 = 0x0200;
inline constexpr Enum LESS                                  = 0x0201;
inline constexpr Enum EQUAL                                 = 0x0202;
inline constexpr Enum LEQUAL                                = 0x0203;
inline constexpr Enum GREATER                               = 0x0204;
inline constexpr Enum NOTEQUAL                              = 0x0205;
inline constexpr Enum GEQUAL                                = 0x0206;
inline constexpr Enum ALWAYS                                = 0x0207;

inline constexpr Enum SRC_COLOR                             = 0x0300;
inline constexpr Enum ONE_MINUS_SRC_COLOR                   = 0x0301;
inline constexpr Enum SRC_ALPHA                             = 0x0302;
inline constexpr Enum ONE_MINUS_SRC_ALPHA                   = 0x0303;
inline constexpr Enum DST_ALPHA                             = 0x0304;
inline constexpr Enum ONE_MINUS_DST_ALPHA                   = 0x0305;
inline constexpr Enum DST_COLOR                             = 0x0306;
inline constexpr Enum ONE_MINUS_DST_COLOR                   = 0x0307;
inline constexpr Enum FUNC_ADD                              = 0x8006;

inline constexpr Enum ARRAY_BUFFER                          = 0x8892;
inline constexpr Enum ELEMENT_ARRAY_BUFFER                  = 0x8893;
inline constexpr Enum PIXEL_PACK_BUFFER                     = 0x88EB;
inline constexpr Enum PIXEL_UNPACK_BUFFER                   = 0x88EC;
inline constexpr Enum UNIFORM_BUFFER                        = 0x8A11;
inline constexpr Enum COPY_READ_BUFFER                      = 0x8F36;
inline constexpr Enum COPY_WRITE_BUFFER                     = 0x8F37;
inline constexpr Enum DRAW_INDIRECT_BUFFER                  = 0x8F3F;
inline constexpr Enum STREAM_DRAW                           = 0x88E0;
inline constexpr Enum STREAM_READ                           = 0x88E1;
inline constexpr Enum STATIC_DRAW                           = 0x88E4;
inline constexpr Enum DYNAMIC_DRAW                          = 0x88E8;

inline constexpr Enum TEXTURE_2D                            = 0x0DE1;
inline constexpr Enum TEXTURE_3D                            = 0x806F;
inline constexpr Enum TEXTURE_CUBE_MAP                      = 0x8513;
inline constexpr Enum TEXTURE_CUBE_MAP_POSITIVE_X           = 0x8515;
inline constexpr Enum TEXTURE_2D_MULTISAMPLE                = 0x9100;
inline constexpr Enum TEXTURE0                              = 0x84C0;
inline constexpr Enum TEXTURE_MAG_FILTER                    = 0x2800;
inline constexpr Enum TEXTURE_MIN_FILTER                    = 0x2801;
inline constexpr Enum TEXTURE_WRAP_S                        = 0x2802;
inline constexpr Enum TEXTURE_WRAP_T                        = 0x2803;
inline constexpr Enum TEXTURE_WRAP_R                        = 0x8072;
inline constexpr Enum TEXTURE_MIN_LOD                       = 0x813A;
inline constexpr Enum TEXTURE_MAX_LOD                       = 0x813B;
inline constexpr Enum TEXTURE_BASE_LEVEL                    = 0x813C;
inline constexpr Enum TEXTURE_MAX_LEVEL                     = 0x813D;
inline constexpr Enum TEXTURE_COMPARE_MODE                  = 0x884C;
inline constexpr Enum TEXTURE_COMPARE_FUNC                  = 0x884D;
inline constexpr Enum COMPARE_REF_TO_TEXTURE                = 0x884E;
inline constexpr Enum NEAREST                               = 0x2600;
inline constexpr Enum LINEAR                                = 0x2601;
inline constexpr Enum REPEAT                                = 0x2901;
inline constexpr Enum CLAMP_TO_EDGE                         = 0x812F;
inline constexpr Enum MIRRORED_REPEAT                       = 0x8370;

inline constexpr Enum DEPTH_COMPONENT                       = 0x1902;
inline constexpr Enum RED                                   = 0x1903;
inline constexpr Enum RGBA                                  = 0x1908;
inline constexpr Enum DEPTH_STENCIL                         = 0x84F9;
inline constexpr Enum RGBA8                                 = 0x8058;
inline constexpr Enum R8                                    = 0x8229;
inline constexpr Enum R32F                                  = 0x822E;
inline constexpr Enum RGBA32F                               = 0x8814;
inline constexpr Enum RGBA16F                               = 0x881A;
inline constexpr Enum DEPTH_COMPONENT16                     = 0x81A5;
inline constexpr Enum DEPTH_COMPONENT24                     = 0x81A6;
inline constexpr Enum DEPTH_COMPONENT32F                    = 0x8CAC;
inline constexpr Enum DEPTH24_STENCIL8                      = 0x88F0;

inline constexpr Enum UNPACK_ROW_LENGTH                     = 0x0CF2;
inline constexpr Enum UNPACK_SKIP_ROWS                      = 0x0CF3;
inline constexpr Enum UNPACK_SKIP_PIXELS                    = 0x0CF4;
inline constexpr Enum UNPACK_ALIGNMENT                      = 0x0CF5;
inline constexpr Enum PACK_ROW_LENGTH                       = 0x0D02;
inline constexpr Enum PACK_ALIGNMENT                        = 0x0D05;

inline constexpr Enum FRAMEBUFFER                           = 0x8D40;
inline constexpr Enum READ_FRAMEBUFFER                      = 0x8CA8;
inline constexpr Enum DRAW_FRAMEBUFFER                      = 0x8CA9;
inline constexpr Enum RENDERBUFFER                          = 0x8D41;
inline constexpr Enum COLOR_ATTACHMENT0                     = 0x8CE0;
inline constexpr Enum DEPTH_ATTACHMENT                      = 0x8D00;
inline constexpr Enum DEPTH_STENCIL_ATTACHMENT              = 0x821A;
inline constexpr Enum FRAMEBUFFER_COMPLETE                  = 0x8CD5;
inline constexpr Enum BACK_LEFT                             = 0x0402;

inline constexpr Enum FRAGMENT_SHADER                       = 0x8B30;
inline constexpr Enum VERTEX_SHADER                         = 0x8B31;
inline constexpr Enum COMPUTE_SHADER                        = 0x91B9;
inline constexpr Enum COMPILE_STATUS                        = 0x8B81;
inline constexpr Enum LINK_STATUS                           = 0x8B82;
inline constexpr Enum INFO_LOG_LENGTH                       = 0x8B84;
inline constexpr native::Uint INVALID_INDEX                 = 0xFFFFFFFFu;

inline constexpr Enum VENDOR                                = 0x1F00;
inline constexpr Enum RENDERER                              = 0x1F01;
inline constexpr Enum VERSION                               = 0x1F02;
inline constexpr Enum EXTENSIONS                            = 0x1F03;
inline constexpr Enum MAJOR_VERSION                         = 0x821B;
inline constexpr Enum MINOR_VERSION                         = 0x821C;
inline constexpr Enum NUM_EXTENSIONS                        = 0x821D;
inline constexpr Enum MAX_TEXTURE_SIZE                      = 0x0D33;
inline constexpr Enum MAX_TEXTURE_IMAGE_UNITS               = 0x8872;
inline constexpr Enum MAX_COMBINED_TEXTURE_IMAGE_UNITS      = 0x8B4D;
inline constexpr Enum MAX_SAMPLES                           = 0x8D57;
inline constexpr Enum MAX_UNIFORM_BLOCK_SIZE                = 0x8A30;
inline constexpr Enum UNIFORM_BUFFER_OFFSET_ALIGNMENT       = 0x8A34;
inline constexpr Enum CONTEXT_FLAGS                         = 0x821E;

inline constexpr Enum QUERY_RESULT                          = 0x8866;
inline constexpr Enum QUERY_RESULT_AVAILABLE                = 0x8867;
inline constexpr Enum TIMESTAMP                             = 0x8E28;

inline constexpr Enum SYNC_GPU_COMMANDS_COMPLETE            = 0x9117;
inline constexpr Enum SYNC_FLUSH_COMMANDS_BIT               = 0x00000001;

inline constexpr Enum LOWER_LEFT                            = 0x8CA1;
inline constexpr Enum UPPER_LEFT                            = 0x8CA2;
inline constexpr Enum NEGATIVE_ONE_TO_ONE                   = 0x935E;
inline constexpr Enum ZERO_TO_ONE                           = 0x935F;

inline constexpr Enum READ_ONLY                             = 0x88B8;
inline constexpr Enum WRITE_ONLY                            = 0x88B9;
inline constexpr Enum READ_WRITE                            = 0x88BA;
inline constexpr Enum TEXTURE_FETCH_BARRIER_BIT             = 0x00000008;
inline constexpr Enum SHADER_IMAGE_ACCESS_BARRIER_BIT       = 0x00000020;

inline constexpr Enum DEBUG_SOURCE_APPLICATION             = 0x824A;
inline constexpr Enum DEBUG_TYPE_ERROR                      = 0x824C;
inline constexpr Enum DEBUG_SEVERITY_HIGH                   = 0x9146;
inline constexpr Enum DEBUG_SEVERITY_MEDIUM                 = 0x9147;
inline constexpr Enum DEBUG_SEVERITY_LOW                    = 0x9148;
inline constexpr Enum DEBUG_SEVERITY_NOTIFICATION           = 0x826B;
inline constexpr Enum BUFFER_OBJECT                         = 0x82E0;
inline constexpr Enum SHADER_OBJECT                         = 0x82E1;
inline constexpr Enum PROGRAM_OBJECT                        = 0x82E2;
inline constexpr Enum QUERY_OBJECT                          = 0x82E3;
inline constexpr Enum SAMPLER_OBJECT                        = 0x82E6;
inline constexpr Enum TEXTURE_OBJECT                        = 0x1702;
inline constexpr Enum FRAMEBUFFER_OBJECT                    = 0x8D40;
inline constexpr Enum RENDERBUFFER_OBJECT                   = 0x8D41;
inline constexpr Enum VERTEX_ARRAY_OBJECT                   = 0x8074;
inline constexpr Enum CONTEXT_FLAG_DEBUG_BIT                = 0x00000002;

inline constexpr Enum GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX  = 0x9047;
inline constexpr Enum GPU_MEMORY_INFO_TOTAL_AVAILABLE_NVX   = 0x9048;
inline constexpr Enum GPU_MEMORY_INFO_CURRENT_AVAILABLE_NVX = 0x9049;
inline constexpr Enum TEXTURE_FREE_MEMORY_ATI               = 0x87FC;

inline constexpr Enum BGRA                                  = 0x80E1;
inline constexpr Enum RG                                    = 0x8227;
inline constexpr Enum RGB_FORMAT                            = 0x1907;
inline constexpr Enum RED_INTEGER                           = 0x8D94;
inline constexpr Enum RGB_INTEGER                           = 0x8D98;
inline constexpr Enum HALF_FLOAT                            = 0x140B;
inline constexpr Enum R16F                                  = 0x822D;
inline constexpr Enum R32UI                                 = 0x8236;
inline constexpr Enum RG32F                                 = 0x8230;
inline constexpr Enum RGB32F                                = 0x8815;
inline constexpr Enum RGB32I                                = 0x8D83;
inline constexpr Enum SRGB8_ALPHA8                          = 0x8C43;
inline constexpr Enum COLOR                                 = 0x1800;
inline constexpr Enum DEPTH                                 = 0x1801;
inline constexpr Enum NEAREST_MIPMAP_NEAREST                = 0x2700;
inline constexpr Enum LINEAR_MIPMAP_NEAREST                 = 0x2701;
inline constexpr Enum NEAREST_MIPMAP_LINEAR                 = 0x2702;
inline constexpr Enum LINEAR_MIPMAP_LINEAR                  = 0x2703;
inline constexpr Enum TEXTURE_MAX_ANISOTROPY                = 0x84FE;
inline constexpr Enum UNPACK_IMAGE_HEIGHT                   = 0x806E;
inline constexpr Enum MAX_COLOR_TEXTURE_SAMPLES             = 0x910E;
inline constexpr Enum MAX_DEPTH_TEXTURE_SAMPLES             = 0x910F;
inline constexpr Enum MAX_FRAGMENT_UNIFORM_BLOCKS           = 0x8A2D;
inline constexpr Enum SHADER_STORAGE_BUFFER                 = 0x90D2;
inline constexpr Enum SHADER_STORAGE_BLOCK                  = 0x92E6;
inline constexpr Enum DYNAMIC_READ                          = 0x88E9;
inline constexpr native::Bitfield ALL_BARRIER_BITS                = 0xFFFFFFFFu;

} // namespace gl

namespace glx {

inline constexpr int USE_GL                          = 1;
inline constexpr int RGBA                            = 4;
inline constexpr int DOUBLEBUFFER                    = 5;
inline constexpr int RED_SIZE                        = 8;
inline constexpr int GREEN_SIZE                      = 9;
inline constexpr int BLUE_SIZE                       = 10;
inline constexpr int ALPHA_SIZE                      = 11;
inline constexpr int DEPTH_SIZE                      = 12;
inline constexpr int STENCIL_SIZE                    = 13;
inline constexpr int X_VISUAL_TYPE                   = 0x22;
inline constexpr int DRAWABLE_TYPE                   = 0x8010;
inline constexpr int RENDER_TYPE                     = 0x8011;
inline constexpr int X_RENDERABLE                    = 0x8012;
inline constexpr int VISUAL_ID                       = 0x800B;
inline constexpr int TRUE_COLOR                      = 0x8002;
inline constexpr int RGBA_TYPE                       = 0x8014;
inline constexpr int WINDOW_BIT                      = 0x00000001;
inline constexpr int RGBA_BIT                        = 0x00000001;
inline constexpr int SAMPLE_BUFFERS                  = 100000;
inline constexpr int SAMPLES                         = 100001;
inline constexpr int CONTEXT_MAJOR_VERSION_ARB       = 0x2091;
inline constexpr int CONTEXT_MINOR_VERSION_ARB       = 0x2092;
inline constexpr int CONTEXT_FLAGS_ARB               = 0x2094;
inline constexpr int CONTEXT_PROFILE_MASK_ARB        = 0x9126;
inline constexpr int CONTEXT_CORE_PROFILE_BIT_ARB    = 0x00000001;
inline constexpr int CONTEXT_DEBUG_BIT_ARB           = 0x00000001;
inline constexpr int CONTEXT_FORWARD_COMPATIBLE_BIT  = 0x00000002;

} // namespace glx

namespace wgl {

inline constexpr int DRAW_TO_WINDOW_ARB              = 0x2001;
inline constexpr int ACCELERATION_ARB                = 0x2003;
inline constexpr int SUPPORT_OPENGL_ARB              = 0x2010;
inline constexpr int DOUBLE_BUFFER_ARB               = 0x2011;
inline constexpr int PIXEL_TYPE_ARB                  = 0x2013;
inline constexpr int COLOR_BITS_ARB                  = 0x2014;
inline constexpr int ALPHA_BITS_ARB                  = 0x201B;
inline constexpr int DEPTH_BITS_ARB                  = 0x2022;
inline constexpr int STENCIL_BITS_ARB                = 0x2023;
inline constexpr int FULL_ACCELERATION_ARB           = 0x2027;
inline constexpr int TYPE_RGBA_ARB                   = 0x202B;
inline constexpr int CONTEXT_MAJOR_VERSION_ARB       = 0x2091;
inline constexpr int CONTEXT_MINOR_VERSION_ARB       = 0x2092;
inline constexpr int CONTEXT_FLAGS_ARB               = 0x2094;
inline constexpr int CONTEXT_PROFILE_MASK_ARB        = 0x9126;
inline constexpr int CONTEXT_CORE_PROFILE_BIT_ARB    = 0x00000001;
inline constexpr int CONTEXT_DEBUG_BIT_ARB           = 0x00000001;
inline constexpr int CONTEXT_FORWARD_COMPATIBLE_BIT  = 0x00000002;

} // namespace wgl

inline constexpr std::uint32_t make_version(std::uint32_t major, std::uint32_t minor) noexcept { return major * 100u + minor * 10u; }

} // namespace opengl
} // namespace fizmo

#endif // FIZMO_OPENGL_COMPAT_HPP
