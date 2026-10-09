#ifndef FIZMO_SYSTEM_WIN_LIBRARY_HPP
#define FIZMO_SYSTEM_WIN_LIBRARY_HPP

#include "../Basic/fizmo_defines.hpp"
#include <cstring>

#if defined(OS_WINDOWS)

namespace fizmo {
namespace system {
namespace detail {
namespace win {

class Library {
private:
    void* m_handle = nullptr;

    void* symbol(const char* name) const noexcept;

public:
    Library() noexcept = default;

    explicit Library(const wchar_t* name, bool system_only = true) noexcept;

    ~Library();
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    Library(Library&& o) noexcept;
    Library& operator=(Library&& o) noexcept;

    bool valid() const noexcept;

    template <typename F>
    F get(const char* name) const noexcept {
        void* p = symbol(name);
        F f = nullptr;
        static_assert(sizeof(f) == sizeof(p), "function pointer size");
        std::memcpy(&f, &p, sizeof(f));
        return f;
    }
};

} // namespace win
} // namespace detail
} // namespace system
} // namespace fizmo

#endif

#endif // FIZMO_SYSTEM_WIN_LIBRARY_HPP
