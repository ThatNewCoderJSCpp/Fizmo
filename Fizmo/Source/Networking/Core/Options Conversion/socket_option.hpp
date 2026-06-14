#ifndef FIZMO_SOCKET_OPTION_HPP
#define FIZMO_SOCKET_OPTION_HPP

#include <string>
#include <vector>
#include "../../../Basic/fizmo_defines.hpp"

namespace fizmo {
namespace networking {
namespace core {

enum class OptionType {
    Boolean = 0,
    Integer,
    Size,
    LingerValue,
    Timeval,
    String
};

enum class OptionCategory {
    Common = 0,
    Windows
};

#define FIZMO_COMMON_OPTIONS(X)                          \
    X(ReuseAddress,      Common,   Boolean)              \
    X(ReusePort,         Common,   Boolean)              \
    X(TCPNoDelay,        Common,   Boolean)              \
    X(KeepAlive,         Common,   Boolean)              \
    X(Linger,            Common,   LingerValue)          \
    X(SendBufferSize,    Common,   Size)                 \
    X(ReceiveBufferSize, Common,   Size)                 \
    X(Broadcast,         Common,   Boolean)              \
    X(ReceiveTimeout,    Common,   Timeval)              \
    X(SendTimeout,       Common,   Timeval)              \
    X(IPv6Only,          Common,   Boolean)              \
    X(TTL,               Common,   Integer)              \
    X(TCPFastOpen,       Common,   Integer)              \
    X(KeepAliveIdle,     Common,   Integer)              \
    X(KeepAliveInterval, Common,   Integer)              \
    X(KeepAliveCount,    Common,   Integer)              \
    X(TypeOfService,     Common,   Integer)              \
    X(PendingError,      Common,   Integer)              \
    X(ReceivePacketInfo, Common,   Boolean)              \
    X(OOBInline,         Common,   Boolean)

#define FIZMO_WINDOWS_OPTIONS(X)                         \
    X(ExclusiveAddress,  Windows,  Boolean)              \
    X(DontFragment,      Windows,  Boolean)

#define FIZMO_ALL_OPTIONS(X)                             \
    FIZMO_COMMON_OPTIONS(X)                              \
    FIZMO_WINDOWS_OPTIONS(X)

#ifdef OS_WINDOWS
    #define FIZMO_PLATFORM_EXPAND(M)      \
            FIZMO_COMMON_OPTIONS(M)        \
            FIZMO_WINDOWS_OPTIONS(M)
#else
    #define FIZMO_PLATFORM_EXPAND(M)   \
            FIZMO_COMMON_OPTIONS(M)
#endif

// Boolean
#define FIZMO_MATCH_Boolean_Boolean(X, name)      X(name)
#define FIZMO_MATCH_Boolean_Integer(X, name)
#define FIZMO_MATCH_Boolean_Size(X, name)
#define FIZMO_MATCH_Boolean_LingerValue(X, name)
#define FIZMO_MATCH_Boolean_Timeval(X, name)
#define FIZMO_MATCH_Boolean_String(X, name)

// Integer
#define FIZMO_MATCH_Integer_Boolean(X, name)
#define FIZMO_MATCH_Integer_Integer(X, name)      X(name)
#define FIZMO_MATCH_Integer_Size(X, name)
#define FIZMO_MATCH_Integer_LingerValue(X, name)
#define FIZMO_MATCH_Integer_Timeval(X, name)
#define FIZMO_MATCH_Integer_String(X, name)

// Size
#define FIZMO_MATCH_Size_Boolean(X, name)
#define FIZMO_MATCH_Size_Integer(X, name)
#define FIZMO_MATCH_Size_Size(X, name)            X(name)
#define FIZMO_MATCH_Size_LingerValue(X, name)
#define FIZMO_MATCH_Size_Timeval(X, name)
#define FIZMO_MATCH_Size_String(X, name)

// LingerValue
#define FIZMO_MATCH_Linger_Boolean(X, name)
#define FIZMO_MATCH_Linger_Integer(X, name)
#define FIZMO_MATCH_Linger_Size(X, name)
#define FIZMO_MATCH_Linger_LingerValue(X, name)   X(name)
#define FIZMO_MATCH_Linger_Timeval(X, name)
#define FIZMO_MATCH_Linger_String(X, name)

// Timeval
#define FIZMO_MATCH_Timeval_Boolean(X, name)
#define FIZMO_MATCH_Timeval_Integer(X, name)
#define FIZMO_MATCH_Timeval_Size(X, name)
#define FIZMO_MATCH_Timeval_LingerValue(X, name)
#define FIZMO_MATCH_Timeval_Timeval(X, name)      X(name)
#define FIZMO_MATCH_Timeval_String(X, name)

// String
#define FIZMO_MATCH_String_Boolean(X, name)
#define FIZMO_MATCH_String_Integer(X, name)
#define FIZMO_MATCH_String_Size(X, name)
#define FIZMO_MATCH_String_LingerValue(X, name)
#define FIZMO_MATCH_String_Timeval(X, name)
#define FIZMO_MATCH_String_String(X, name)        X(name)

#define FIZMO_MATCH_Common_Common(X, name)        X(name)
#define FIZMO_MATCH_Common_Windows(X, name)
#define FIZMO_MATCH_Windows_Common(X, name)
#define FIZMO_MATCH_Windows_Windows(X, name)      X(name)

#define FIZMO_BY_TYPE(target, X, name, cat, type)  FIZMO_MATCH_##target##_##type(X, name)
#define FIZMO_BY_CAT(target, X, name, cat, type)   FIZMO_MATCH_##target##_##cat(X, name)


#define FIZMO_FILTER_BOOL(name, cat, type)    FIZMO_BY_TYPE(Boolean, FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_INT(name, cat, type)     FIZMO_BY_TYPE(Integer, FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_SIZE(name, cat, type)    FIZMO_BY_TYPE(Size,    FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_LINGER(name, cat, type)  FIZMO_BY_TYPE(Linger,  FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_TIMEVAL(name, cat, type) FIZMO_BY_TYPE(Timeval, FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_STRING(name, cat, type)  FIZMO_BY_TYPE(String,  FIZMO_EMIT_NAME, name, cat, type)

#define FIZMO_FILTER_COMMON(name, cat, type)  FIZMO_BY_CAT(Common,  FIZMO_EMIT_NAME, name, cat, type)
#define FIZMO_FILTER_WINDOWS(name, cat, type) FIZMO_BY_CAT(Windows, FIZMO_EMIT_NAME, name, cat, type)

#define FIZMO_EMIT_NAME(name) FIZMO_CURRENT_X(name)

#define FIZMO_FOR_EACH_BOOLEAN(X)                        \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_BOOL)                 \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_INTEGER(X)                        \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_INT)                  \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_SIZE(X)                           \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_SIZE)                 \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_LINGER(X)                         \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_LINGER)               \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_TIMEVAL(X)                        \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_TIMEVAL)              \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_STRING(X)                         \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_STRING)               \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_COMMON(X)                         \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_COMMON)               \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#define FIZMO_FOR_EACH_WINDOWS_OPT(X)                    \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")")           \
    FIZMO_UNDEF_CURRENT_X                                \
    FIZMO_DEFINE_CURRENT_X(X)                            \
    FIZMO_ALL_OPTIONS(FIZMO_FILTER_WINDOWS)              \
    _Pragma("pop_macro(\"FIZMO_CURRENT_X\")")

#ifdef FIZMO_CURRENT_X
#define FIZMO_UNDEF_CURRENT_X
#else
#define FIZMO_UNDEF_CURRENT_X
#endif
#undef  FIZMO_CURRENT_X
#define FIZMO_DEFINE_CURRENT_X(X) \
    _Pragma("push_macro(\"FIZMO_CURRENT_X\")") 

#undef FIZMO_FOR_EACH_BOOLEAN
#undef FIZMO_FOR_EACH_INTEGER
#undef FIZMO_FOR_EACH_SIZE
#undef FIZMO_FOR_EACH_LINGER
#undef FIZMO_FOR_EACH_TIMEVAL
#undef FIZMO_FOR_EACH_STRING
#undef FIZMO_FOR_EACH_COMMON
#undef FIZMO_FOR_EACH_WINDOWS_OPT
#undef FIZMO_EMIT_NAME
#undef FIZMO_DEFINE_CURRENT_X
#undef FIZMO_UNDEF_CURRENT_X

enum class OptionCode {
#define FIZMO_ENUM_ENTRY(name, cat, type) name,
    FIZMO_ALL_OPTIONS(FIZMO_ENUM_ENTRY)
#undef FIZMO_ENUM_ENTRY
};

// Helper macro emits a static constexpr member inside a typed struct 
#define FIZMO_TYPED_CONST(name) \
    static constexpr TypedOption name { OptionCode::name };

struct BooleanOption {
private:
    using TypedOption = BooleanOption;
    OptionCode m_code;
public:
    constexpr explicit BooleanOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

struct IntegerOption {
private:
    using TypedOption = IntegerOption;
    OptionCode m_code;
public:
    constexpr explicit IntegerOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

struct SizeOption {
private:
    using TypedOption = SizeOption;
    OptionCode m_code;
public:
    constexpr explicit SizeOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

struct LingerOption {
private:
    using TypedOption = LingerOption;
    OptionCode m_code;
public:
    constexpr explicit LingerOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

struct TimevalOption {
private:
    using TypedOption = TimevalOption;
    OptionCode m_code;
public:
    constexpr explicit TimevalOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

struct CommonOption {
private:
    using TypedOption = CommonOption;
    OptionCode m_code;
public:
    constexpr explicit CommonOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

#ifdef OS_WINDOWS

struct WindowsOption {
private:
    using TypedOption = WindowsOption;
    OptionCode m_code;
public:
    constexpr explicit WindowsOption(OptionCode c) noexcept : m_code(c) {}
    constexpr operator OptionCode() const noexcept { return m_code; }
    constexpr OptionCode code()     const noexcept { return m_code; }

    #define FIZMO_GEN(name, cat, type) static const TypedOption name;
        FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
    #undef FIZMO_GEN
};

#endif // OS_WINDOWS

// Out-of-class definitions
#define FIZMO_GEN(name, cat, type) \
    constexpr BooleanOption BooleanOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#define FIZMO_GEN(name, cat, type) \
    constexpr IntegerOption IntegerOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#define FIZMO_GEN(name, cat, type) \
    constexpr SizeOption SizeOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#define FIZMO_GEN(name, cat, type) \
    constexpr LingerOption LingerOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#define FIZMO_GEN(name, cat, type) \
    constexpr TimevalOption TimevalOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#define FIZMO_GEN(name, cat, type) \
    constexpr CommonOption CommonOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN

#ifdef OS_WINDOWS
#define FIZMO_GEN(name, cat, type) \
    constexpr WindowsOption WindowsOption::name { OptionCode::name };
    FIZMO_PLATFORM_EXPAND(FIZMO_GEN)
#undef FIZMO_GEN
#endif // OS_WINDOWS

// Clean up the helper
#undef FIZMO_TYPED_CONST

class SocketOption {
private:
    OptionCode m_code;
    OptionCategory m_category;
    OptionType m_type;

public:
    SocketOption(
        OptionCode code = OptionCode::ReuseAddress,
        OptionCategory category = OptionCategory::Common,
        OptionType type = OptionType::Boolean
    ) noexcept : m_code(code), m_category(category), m_type(type) {}

    template <typename TypedOpt, typename = decltype(static_cast<OptionCode>(std::declval<TypedOpt>()))>
    SocketOption(TypedOpt opt) noexcept : SocketOption(SocketOption::from_code(static_cast<OptionCode>(opt))) {}

public:
    OptionCode code()         const noexcept { return m_code; }
    OptionCategory category() const noexcept { return m_category; }
    OptionType type()         const noexcept { return m_type; }

    bool is_common()  const noexcept { return m_category == OptionCategory::Common; }
    bool is_windows() const noexcept { return m_category == OptionCategory::Windows; }

    bool is_boolean() const noexcept { return m_type == OptionType::Boolean; }
    bool is_integer() const noexcept { return m_type == OptionType::Integer; }
    bool is_size()    const noexcept { return m_type == OptionType::Size; }
    bool is_linger()  const noexcept { return m_type == OptionType::LingerValue; }
    bool is_timeval() const noexcept { return m_type == OptionType::Timeval; }
    bool is_string()  const noexcept { return m_type == OptionType::String; }

    static const char* code_to_string(OptionCode code) noexcept {
        switch (code) {
        #define FIZMO_TO_STRING(name, cat, type) case OptionCode::name: return #name;
            FIZMO_ALL_OPTIONS(FIZMO_TO_STRING)
        #undef FIZMO_TO_STRING
            default: return "UnknownOption";
        }
    }

    static OptionCategory get_category_for_code(OptionCode code) noexcept {
        switch (code) {
        #define FIZMO_CATEGORY(name, cat, type) case OptionCode::name: return OptionCategory::cat;
            FIZMO_ALL_OPTIONS(FIZMO_CATEGORY)
        #undef FIZMO_CATEGORY
            default: return OptionCategory::Common;
        }
    }

    static OptionType get_type_for_code(OptionCode code) noexcept {
        switch (code) {
        #define FIZMO_TYPE(name, cat, type) case OptionCode::name: return OptionType::type;
            FIZMO_ALL_OPTIONS(FIZMO_TYPE)
        #undef FIZMO_TYPE
            default: return OptionType::Boolean;
        }
    }

    static bool is_available_on_current_platform(OptionCode code) noexcept {
        OptionCategory cat = get_category_for_code(code);
        if (cat == OptionCategory::Common) return true;
    #ifdef OS_WINDOWS
        return cat == OptionCategory::Windows;
    #else
        return false;
    #endif
    }

    static SocketOption from_code(OptionCode code) noexcept {
        return SocketOption(code, get_category_for_code(code), get_type_for_code(code));
    }

    static const char* category_to_string(OptionCategory category) noexcept {
        switch (category) {
            case OptionCategory::Common:  return "Common";
            case OptionCategory::Windows: return "Windows";
            default:                      return "Unknown";
        }
    }

public:
    std::string to_string() const noexcept {
        return std::string(code_to_string(m_code)) + " [" + category_to_string(m_category) + "]";
    }

    friend std::ostream& operator<<(std::ostream& os, const SocketOption& opt) {
        os << opt.to_string();
        return os;
    }

    bool operator==(const SocketOption& other) const noexcept { return m_code == other.m_code; }
    bool operator!=(const SocketOption& other) const noexcept { return m_code != other.m_code; }
};

struct NativeOption {
    int level;
    int option_name;
    bool supported;
    NativeOption() noexcept : level(0), option_name(0), supported(false) {}
    NativeOption(int lvl, int opt) noexcept : level(lvl), option_name(opt), supported(true) {}
    explicit operator bool() const noexcept { return supported; }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_OPTION_HPP