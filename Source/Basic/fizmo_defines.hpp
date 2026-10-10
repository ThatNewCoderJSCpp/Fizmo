#ifndef FIZMO_DEFINES_HPP
#define FIZMO_DEFINES_HPP

#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#if defined(_WIN32) || defined(_WIN64)
    #ifndef UNICODE
        #define UNICODE
    #endif
    #ifndef _UNICODE
        #define _UNICODE
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX  
    #endif
    #ifndef SECURITY_WIN32 
        #define SECURITY_WIN32
    #endif
    
    #ifdef __GNUC__
        #ifndef _In_
            #define _In_
        #endif
        #ifndef _In_opt_
            #define _In_opt_
        #endif
        #ifndef _Out_
            #define _Out_
        #endif
        
        #ifndef GWLP_USERDATA
            #ifdef _WIN64
                #define GWLP_USERDATA (-21)
            #else
                #define GWLP_USERDATA (-21)
            #endif
        #endif
    #endif

    #ifndef OS_WINDOWS
        #define OS_WINDOWS
    #endif
    
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_MACOS
#elif defined(__ANDROID__)
    #define OS_ANDROID
#elif defined(__linux__)
    #define OS_LINUX
#elif defined(__unix__) || defined(__unix)
    #define OS_UNIX
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define OS_BSD
#elif defined(__sun) && defined(__SVR4)
    #define OS_SOLARIS
#else
    #error "Unknown operating system"
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64
#elif defined(__arm__) || defined(_M_ARM)
    #define ARCH_ARM32
    #if defined(__ARM_ARCH) && (__ARM_ARCH < 7)
        #define ARCH_ARM_LEGACY
    #else
        #define ARCH_ARMV7_OR_GREATER
    #endif
#elif defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X86_64
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH_X86_32
#endif

#if __cplusplus >= 202002L
    #define CPP20_OR_GREATER 1
#else
    #define CPP20_OR_GREATER 0
#endif

#if __cplusplus >= 201703L
    #define CPP17_OR_GREATER 1
#else
    #define CPP17_OR_GREATER 0
#endif

#if __cplusplus >= 201402L
    #define CPP14_OR_GREATER 1
#else
    #define CPP14_OR_GREATER 0
#endif

#if __cplusplus >= 201103L
    #define CPP11_OR_GREATER 1
#else
    #define CPP11_OR_GREATER 0
#endif

#if CPP14_OR_GREATER
    #define OPTIONAL_CPP14_CONSTEXPR constexpr
    #define OPTIONAL_CPP14_NOEXCEPT noexcept
#else 
    #define OPTIONAL_CPP14_CONSTEXPR
    #define OPTIONAL_CPP14_NOEXCEPT 
#endif

#if CPP17_OR_GREATER
    #include <string_view>
    #define OPTIONAL_CPP17_INLINE inline
    #define OPTIONAL_NODISCARD [[nodiscard]]
    typedef std::string_view constexpr_string;
#else
    #define OPTIONAL_CPP17_INLINE
    #define OPTIONAL_NODISCARD 
    typedef const char* constexpr_string;
#endif

#endif // FIZMO_DEFINES_HPP