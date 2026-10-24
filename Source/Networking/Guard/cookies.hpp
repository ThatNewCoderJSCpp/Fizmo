#ifndef FIZMO_GUARD_COOKIES_HPP
#define FIZMO_GUARD_COOKIES_HPP

#include "guard_types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace guard {

enum class SameSite {
    Strict,
    Lax,
    None
};

struct CookieOptions {
    std::string                         path      = "/";
    std::string                         domain;
    bool                                secure    = true;
    bool                                http_only = true;
    SameSite                            same_site = SameSite::Lax;
    std::optional<std::chrono::seconds> max_age;
    bool                                partitioned = false;
};

bool valid_cookie_name(const std::string& name) noexcept;
bool valid_cookie_value(const std::string& value) noexcept;
std::string make_set_cookie(const std::string& name, const std::string& value, const CookieOptions& options = CookieOptions());
std::string make_expired_cookie(const std::string& name, const CookieOptions& options = CookieOptions());
std::map<std::string, std::string> parse_cookie_header(const std::string& header);
std::string cookie_value(const std::string& header, const std::string& name);

enum class ClearSiteData : unsigned {
    None              = 0,
    Cookies           = 1u << 0,
    Storage           = 1u << 1,
    Cache             = 1u << 2,
    ExecutionContexts = 1u << 3,
    All               = Cookies | Storage | Cache | ExecutionContexts
};

inline ClearSiteData operator|(ClearSiteData a, ClearSiteData b) noexcept {
    return static_cast<ClearSiteData>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline bool has_flag(ClearSiteData set, ClearSiteData flag) noexcept {
    return (static_cast<unsigned>(set) & static_cast<unsigned>(flag)) != 0;
}

std::string clear_site_data_header(ClearSiteData what);

} // namespace guard
} // namespace networking
} // namespace fizmo

#endif // FIZMO_GUARD_COOKIES_HPP
