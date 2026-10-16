#ifndef FIZMO_TIME_CIVIL_HPP
#define FIZMO_TIME_CIVIL_HPP

#include "duration.hpp"
#include "wait_sleep.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

namespace fizmo {
namespace time {

struct CivilDate {
    std::int64_t year  = 1970;
    int          month = 1;
    int          day   = 1;
};

struct CivilTime {
    int          hour       = 0;
    int          minute     = 0;
    int          second     = 0;
    std::int64_t nanosecond = 0;
};

struct IsoWeek {
    std::int64_t year    = 1970;
    int          week    = 1;
    int          weekday = 1;
};

struct HoursMinutesSeconds {
    bool         negative    = false;
    std::int64_t hours       = 0;
    int          minutes     = 0;
    int          seconds     = 0;
    std::int64_t nanoseconds = 0;

    std::int64_t total_nanoseconds() const noexcept {
        const std::int64_t total = ((hours * 60 + minutes) * 60 + seconds) * 1000000000LL + nanoseconds;
        return negative ? -total : total;
    }
};

namespace civil {

constexpr std::int64_t nanos_per_second = 1000000000LL;
constexpr std::int64_t nanos_per_minute = 60LL * nanos_per_second;
constexpr std::int64_t nanos_per_hour   = 60LL * nanos_per_minute;
constexpr std::int64_t nanos_per_day    = 24LL * nanos_per_hour;

constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) noexcept {
    const std::int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

constexpr std::int64_t floor_mod(std::int64_t a, std::int64_t b) noexcept { return a - floor_div(a, b) * b; }

constexpr bool is_leap_year(std::int64_t y) noexcept { return (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0)); }

constexpr int days_in_month(std::int64_t y, int m) noexcept {
    constexpr int table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    return (m == 2 && is_leap_year(y)) ? 29 : table[m - 1];
}

constexpr int days_in_year(std::int64_t y) noexcept { return is_leap_year(y) ? 366 : 365; }

constexpr bool valid_date(std::int64_t y, int m, int d) noexcept { return m >= 1 && m <= 12 && d >= 1 && d <= days_in_month(y, m); }

constexpr std::int64_t days_from_civil(std::int64_t y, int m, int d) noexcept {
    y -= m <= 2 ? 1 : 0;
    const std::int64_t era = floor_div(y, 400);
    const std::int64_t yoe = y - era * 400;
    const std::int64_t mp  = (m + 9) % 12;
    const std::int64_t doy = (153 * mp + 2) / 5 + d - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

constexpr CivilDate civil_from_days(std::int64_t z) noexcept {
    z += 719468;
    const std::int64_t era = floor_div(z, 146097);
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp  = (5 * doy + 2) / 153;
    const int          d   = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int          m   = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    CivilDate out;
    out.year  = yoe + era * 400 + (m <= 2 ? 1 : 0);
    out.month = m;
    out.day   = d;
    return out;
}

constexpr int iso_weekday_from_days(std::int64_t z) noexcept { return static_cast<int>(floor_mod(z + 3, 7)) + 1; }

constexpr int weekday_from_days(std::int64_t z) noexcept { return static_cast<int>(floor_mod(z + 4, 7)); }

constexpr int day_of_year(std::int64_t y, int m, int d) noexcept {
    return static_cast<int>(days_from_civil(y, m, d) - days_from_civil(y, 1, 1)) + 1;
}

constexpr int quarter(int m) noexcept { return (m - 1) / 3 + 1; }

constexpr IsoWeek iso_week(std::int64_t y, int m, int d) noexcept {
    const std::int64_t days    = days_from_civil(y, m, d);
    const int          wd      = iso_weekday_from_days(days);
    const std::int64_t thursday = days + (4 - wd);
    const CivilDate    t        = civil_from_days(thursday);
    const std::int64_t first    = days_from_civil(t.year, 1, 1);
    IsoWeek out;
    out.year    = t.year;
    out.week    = static_cast<int>((thursday - first) / 7) + 1;
    out.weekday = wd;
    return out;
}

constexpr int iso_weeks_in_year(std::int64_t y) noexcept { return iso_week(y, 12, 28).week; }

constexpr int nth_weekday_of_month(std::int64_t y, int m, int iso_weekday, int n) noexcept {
    if (n < 1 || n > 5) return 0;
    const int first_wd = iso_weekday_from_days(days_from_civil(y, m, 1));
    const int offset   = ((iso_weekday - first_wd) % 7 + 7) % 7;
    const int day      = 1 + offset + (n - 1) * 7;
    return day <= days_in_month(y, m) ? day : 0;
}

constexpr int last_weekday_of_month(std::int64_t y, int m, int iso_weekday) noexcept {
    const int last    = days_in_month(y, m);
    const int last_wd = iso_weekday_from_days(days_from_civil(y, m, last));
    return last - ((last_wd - iso_weekday) % 7 + 7) % 7;
}

constexpr bool is_am(int hour) noexcept { return hour >= 0 && hour < 12; }
constexpr bool is_pm(int hour) noexcept { return hour >= 12 && hour < 24; }
constexpr int  make12(int hour) noexcept { return hour % 12 == 0 ? 12 : hour % 12; }
constexpr int  make24(int hour12, bool pm) noexcept { return (hour12 % 12) + (pm ? 12 : 0); }

constexpr HoursMinutesSeconds split(std::int64_t ns) noexcept {
    HoursMinutesSeconds out;
    out.negative = ns < 0;
    const std::uint64_t mag = out.negative ? static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(ns) : static_cast<std::uint64_t>(ns);
    out.hours       = static_cast<std::int64_t>(mag / static_cast<std::uint64_t>(nanos_per_hour));
    out.minutes     = static_cast<int>((mag / static_cast<std::uint64_t>(nanos_per_minute)) % 60);
    out.seconds     = static_cast<int>((mag / static_cast<std::uint64_t>(nanos_per_second)) % 60);
    out.nanoseconds = static_cast<std::int64_t>(mag % static_cast<std::uint64_t>(nanos_per_second));
    return out;
}

constexpr CivilTime time_from_nanos_of_day(std::int64_t ns) noexcept {
    ns = floor_mod(ns, nanos_per_day);
    CivilTime out;
    out.hour       = static_cast<int>(ns / nanos_per_hour);
    out.minute     = static_cast<int>((ns / nanos_per_minute) % 60);
    out.second     = static_cast<int>((ns / nanos_per_second) % 60);
    out.nanosecond = ns % nanos_per_second;
    return out;
}

struct LeapSecond {
    std::int64_t unix_seconds;
    int          tai_minus_utc;
};

inline const LeapSecond* leap_second_table(std::size_t& count) noexcept {
    static const LeapSecond table[] = {
        {63072000, 10},   {78796800, 11},   {94694400, 12},   {126230400, 13},  {157766400, 14},  {189302400, 15},
        {220924800, 16},  {252460800, 17},  {283996800, 18},  {315532800, 19},  {362793600, 20},  {394329600, 21},
        {425865600, 22},  {489024000, 23},  {567993600, 24},  {631152000, 25},  {662688000, 26},  {709948800, 27},
        {741484800, 28},  {773020800, 29},  {820454400, 30},  {867715200, 31},  {915148800, 32},  {1136073600, 33},
        {1230768000, 34}, {1341100800, 35}, {1435708800, 36}, {1483228800, 37},
    };
    count = sizeof(table) / sizeof(table[0]);
    return table;
}

inline int tai_minus_utc(std::int64_t unix_seconds) noexcept {
    std::size_t       n     = 0;
    const LeapSecond* table = leap_second_table(n);
    int               out   = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (unix_seconds >= table[i].unix_seconds) out = table[i].tai_minus_utc;
    }
    return out;
}

inline int gps_minus_utc(std::int64_t unix_seconds) noexcept {
    const int tai = tai_minus_utc(unix_seconds);
    return tai >= 19 ? tai - 19 : 0;
}

inline bool is_leap_second_day(std::int64_t unix_seconds) noexcept {
    std::size_t       n     = 0;
    const LeapSecond* table = leap_second_table(n);
    const std::int64_t next_day = floor_div(unix_seconds, 86400) * 86400 + 86400;
    for (std::size_t i = 0; i < n; ++i) {
        if (table[i].unix_seconds == next_day && i > 0) return true;
    }
    return false;
}

inline std::int64_t system_unix_nanoseconds() noexcept {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

inline std::int64_t steady_nanoseconds() noexcept {
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline int local_utc_offset_seconds(std::int64_t unix_seconds) noexcept {
    const std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm           local{};
    std::tm           utc{};
#if defined(_WIN32)
    if (localtime_s(&local, &t) != 0 || gmtime_s(&utc, &t) != 0) return 0;
#else
    if (localtime_r(&t, &local) == nullptr || gmtime_r(&t, &utc) == nullptr) return 0;
#endif
    const std::int64_t ld = days_from_civil(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    const std::int64_t ud = days_from_civil(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);
    const std::int64_t ls = ld * 86400 + local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    const std::int64_t us = ud * 86400 + utc.tm_hour * 3600 + utc.tm_min * 60 + utc.tm_sec;
    return static_cast<int>(ls - us);
}

inline std::string local_zone_name(std::int64_t unix_seconds) {
    const std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm           local{};
#if defined(_WIN32)
    if (localtime_s(&local, &t) != 0) return std::string();
#else
    if (localtime_r(&t, &local) == nullptr) return std::string();
#endif
    char buf[64] = {0};
    const std::size_t n = std::strftime(buf, sizeof(buf), "%Z", &local);
    return std::string(buf, n);
}

inline bool sleep_for_nanoseconds(std::int64_t ns) noexcept {
    if (ns <= 0) return true;
    const std::uint64_t u = static_cast<std::uint64_t>(ns);
    return impl::do_sleep(u / 1000000ULL, u);
}

inline bool wait_for_nanoseconds(std::int64_t ns) noexcept {
    if (ns <= 0) return true;
    return impl::do_wait(static_cast<std::uint64_t>(ns));
}

inline bool precise_wait_for_nanoseconds(std::int64_t ns) noexcept {
    if (ns <= 0) return true;
    return impl::do_precise_wait(static_cast<std::uint64_t>(ns));
}

inline const char* month_full_name(int m) noexcept {
    static const char* names[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "";
}

inline const char* month_short_name(int m) noexcept {
    static const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return (m >= 1 && m <= 12) ? names[m - 1] : "";
}

inline const char* weekday_full_name(int iso_weekday) noexcept {
    static const char* names[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    return (iso_weekday >= 1 && iso_weekday <= 7) ? names[iso_weekday - 1] : "";
}

inline const char* weekday_short_name(int iso_weekday) noexcept {
    static const char* names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    return (iso_weekday >= 1 && iso_weekday <= 7) ? names[iso_weekday - 1] : "";
}

namespace fdetail {

inline void pad(std::string& out, std::int64_t v, int width, char fill = '0') {
    const bool  neg = v < 0;
    std::string digits = std::to_string(neg ? -v : v);
    while (static_cast<int>(digits.size()) < width) digits.insert(digits.begin(), fill);
    if (neg) out += '-';
    out += digits;
}

inline void offset(std::string& out, int seconds, bool colon) {
    out += seconds < 0 ? '-' : '+';
    const int mag = seconds < 0 ? -seconds : seconds;
    pad(out, mag / 3600, 2);
    if (colon) out += ':';
    pad(out, (mag / 60) % 60, 2);
}

inline bool digits(const std::string& s, std::size_t& i, int min_count, int max_count, std::int64_t& out) {
    std::size_t start = i;
    bool        neg   = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+') && max_count > 4) {
        neg = s[i] == '-';
        ++i;
        start = i;
    }
    std::int64_t v = 0;
    while (i < s.size() && static_cast<int>(i - start) < max_count && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        ++i;
    }
    if (static_cast<int>(i - start) < min_count) return false;
    out = neg ? -v : v;
    return true;
}

inline bool name_match(const std::string& s, std::size_t& i, const char* const* names, int count, int& out) {
    for (int k = 0; k < count; ++k) {
        const std::string n = names[k];
        if (s.size() - i >= n.size()) {
            bool same = true;
            for (std::size_t j = 0; j < n.size(); ++j) {
                const char a = s[i + j];
                const char b = n[j];
                const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
                const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
                if (la != lb) { same = false; break; }
            }
            if (same) {
                i += n.size();
                out = k;
                return true;
            }
        }
    }
    return false;
}

} // namespace fdetail

inline std::string format(const std::string& pattern, const CivilDate& date, const CivilTime& t, int offset_seconds = 0, const std::string& zone = "UTC") {
    std::string out;
    const std::int64_t days = days_from_civil(date.year, date.month, date.day);
    const int          wd   = iso_weekday_from_days(days);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        if (c != '%' || i + 1 >= pattern.size()) {
            out += c;
            continue;
        }
        const char f = pattern[++i];
        switch (f) {
            case 'Y': fdetail::pad(out, date.year, 4); break;
            case 'y': fdetail::pad(out, floor_mod(date.year, 100), 2); break;
            case 'C': fdetail::pad(out, floor_div(date.year, 100), 2); break;
            case 'm': fdetail::pad(out, date.month, 2); break;
            case 'd': fdetail::pad(out, date.day, 2); break;
            case 'e': fdetail::pad(out, date.day, 2, ' '); break;
            case 'j': fdetail::pad(out, day_of_year(date.year, date.month, date.day), 3); break;
            case 'H': fdetail::pad(out, t.hour, 2); break;
            case 'I': fdetail::pad(out, make12(t.hour), 2); break;
            case 'M': fdetail::pad(out, t.minute, 2); break;
            case 'S': fdetail::pad(out, t.second, 2); break;
            case 'L': fdetail::pad(out, t.nanosecond / 1000000, 3); break;
            case 'f': fdetail::pad(out, t.nanosecond / 1000, 6); break;
            case 'N': fdetail::pad(out, t.nanosecond, 9); break;
            case 'p': out += is_am(t.hour) ? "AM" : "PM"; break;
            case 'P': out += is_am(t.hour) ? "am" : "pm"; break;
            case 'A': out += weekday_full_name(wd); break;
            case 'a': out += weekday_short_name(wd); break;
            case 'B': out += month_full_name(date.month); break;
            case 'b': case 'h': out += month_short_name(date.month); break;
            case 'u': fdetail::pad(out, wd, 1); break;
            case 'w': fdetail::pad(out, wd % 7, 1); break;
            case 'V': fdetail::pad(out, iso_week(date.year, date.month, date.day).week, 2); break;
            case 'G': fdetail::pad(out, iso_week(date.year, date.month, date.day).year, 4); break;
            case 'q': fdetail::pad(out, quarter(date.month), 1); break;
            case 'z': fdetail::offset(out, offset_seconds, false); break;
            case 'Z': out += zone; break;
            case 'F': out += format("%Y-%m-%d", date, t, offset_seconds, zone); break;
            case 'T': out += format("%H:%M:%S", date, t, offset_seconds, zone); break;
            case 'R': out += format("%H:%M", date, t, offset_seconds, zone); break;
            case 'D': out += format("%m/%d/%y", date, t, offset_seconds, zone); break;
            case 's': fdetail::pad(out, days * 86400 + t.hour * 3600 + t.minute * 60 + t.second - offset_seconds, 1); break;
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case '%': out += '%'; break;
            case ':':
                if (i + 1 < pattern.size() && pattern[i + 1] == 'z') {
                    ++i;
                    fdetail::offset(out, offset_seconds, true);
                    break;
                }
                out += "%:";
                break;
            default: out += '%'; out += f; break;
        }
    }
    return out;
}

struct ParsedCivil {
    bool         ok             = false;
    CivilDate    date;
    CivilTime    time;
    int          offset_seconds = 0;
    bool         has_offset     = false;
    std::string  error;
};

inline ParsedCivil parse(const std::string& text, const std::string& pattern) {
    static const char* const months_long[]  = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    static const char* const months_short[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* const days_long[]    = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    static const char* const days_short[]   = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    ParsedCivil out;
    std::size_t i  = 0;
    int         pm = -1;
    auto fail = [&](const std::string& why) {
        out.ok    = false;
        out.error = why;
        return out;
    };
    for (std::size_t p = 0; p < pattern.size(); ++p) {
        const char c = pattern[p];
        if (c != '%' || p + 1 >= pattern.size()) {
            if (c == ' ') {
                while (i < text.size() && text[i] == ' ') ++i;
                continue;
            }
            if (i >= text.size() || text[i] != c) return fail(std::string("expected '") + c + "' at position " + std::to_string(i));
            ++i;
            continue;
        }
        const char   f = pattern[++p];
        std::int64_t v = 0;
        int          k = 0;
        switch (f) {
            case 'Y': if (!fdetail::digits(text, i, 1, 9, v)) return fail("expected a year"); out.date.year = v; break;
            case 'y': if (!fdetail::digits(text, i, 2, 2, v)) return fail("expected a two-digit year"); out.date.year = v < 69 ? 2000 + v : 1900 + v; break;
            case 'm': if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected a month"); out.date.month = static_cast<int>(v); break;
            case 'd': case 'e':
                while (i < text.size() && text[i] == ' ') ++i;
                if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected a day");
                out.date.day = static_cast<int>(v);
                break;
            case 'H': if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected an hour"); out.time.hour = static_cast<int>(v); break;
            case 'I': if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected an hour"); out.time.hour = static_cast<int>(v); if (pm < 0) pm = 2; break;
            case 'M': if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected minutes"); out.time.minute = static_cast<int>(v); break;
            case 'S': if (!fdetail::digits(text, i, 1, 2, v)) return fail("expected seconds"); out.time.second = static_cast<int>(v); break;
            case 'L': case 'f': case 'N': {
                const std::size_t start = i;
                if (!fdetail::digits(text, i, 1, 9, v)) return fail("expected a fraction of a second");
                std::size_t count = i - start;
                while (count < 9) { v *= 10; ++count; }
                out.time.nanosecond = v;
                break;
            }
            case 'p': case 'P':
                if (text.size() - i >= 2) {
                    const char a = static_cast<char>(text[i] | 0x20);
                    if ((a == 'a' || a == 'p') && (text[i + 1] | 0x20) == 'm') {
                        pm = a == 'p' ? 1 : 0;
                        i += 2;
                        break;
                    }
                }
                return fail("expected AM or PM");
            case 'B': case 'b': case 'h':
                if (fdetail::name_match(text, i, months_long, 12, k) || fdetail::name_match(text, i, months_short, 12, k)) { out.date.month = k + 1; break; }
                return fail("expected a month name");
            case 'A': case 'a':
                if (fdetail::name_match(text, i, days_long, 7, k) || fdetail::name_match(text, i, days_short, 7, k)) break;
                return fail("expected a weekday name");
            case 'z': {
                if (i < text.size() && (text[i] == 'Z' || text[i] == 'z')) { ++i; out.has_offset = true; out.offset_seconds = 0; break; }
                if (i >= text.size() || (text[i] != '+' && text[i] != '-')) return fail("expected a UTC offset");
                const int sign = text[i] == '-' ? -1 : 1;
                ++i;
                std::int64_t hh = 0;
                std::int64_t mm = 0;
                if (!fdetail::digits(text, i, 2, 2, hh)) return fail("expected offset hours");
                if (i < text.size() && text[i] == ':') ++i;
                if (!fdetail::digits(text, i, 2, 2, mm)) mm = 0;
                out.has_offset     = true;
                out.offset_seconds = sign * static_cast<int>(hh * 3600 + mm * 60);
                break;
            }
            case 'F': {
                const ParsedCivil sub = parse(text.substr(i), "%Y-%m-%d");
                if (!sub.ok) return fail(sub.error);
                out.date = sub.date;
                i += 10 + (sub.date.year >= 10000 || sub.date.year < 0 ? 1 : 0);
                break;
            }
            case 'T': {
                std::int64_t hh = 0;
                std::int64_t mm = 0;
                std::int64_t ss = 0;
                if (!fdetail::digits(text, i, 1, 2, hh) || i >= text.size() || text[i++] != ':' || !fdetail::digits(text, i, 1, 2, mm) || i >= text.size() || text[i++] != ':' ||
                    !fdetail::digits(text, i, 1, 2, ss)) {
                    return fail("expected a time as HH:MM:SS");
                }
                out.time.hour   = static_cast<int>(hh);
                out.time.minute = static_cast<int>(mm);
                out.time.second = static_cast<int>(ss);
                break;
            }
            case '%':
                if (i >= text.size() || text[i] != '%') return fail("expected '%'");
                ++i;
                break;
            default: return fail(std::string("unsupported pattern field %") + f);
        }
    }
    if (i != text.size()) return fail("unexpected text after position " + std::to_string(i));
    if (pm == 1 && out.time.hour < 12) out.time.hour += 12;
    if (pm == 0 && out.time.hour == 12) out.time.hour = 0;
    if (!valid_date(out.date.year, out.date.month, out.date.day)) return fail("the date isn't a real calendar date");
    if (out.time.hour > 23 || out.time.minute > 59 || out.time.second > 60) return fail("the time is out of range");
    out.ok = true;
    return out;
}

} // namespace civil

template <typename To, Unit F, typename FV>
constexpr To duration_floor(const Duration<F, FV>& d) noexcept {
    To t = duration_cast<To>(d);
    if (duration_cast<Duration<F, FV>>(t) > d) t = t - To(1);
    return t;
}

template <typename To, Unit F, typename FV>
constexpr To duration_ceil(const Duration<F, FV>& d) noexcept {
    To t = duration_cast<To>(d);
    if (duration_cast<Duration<F, FV>>(t) < d) t = t + To(1);
    return t;
}

template <typename To, Unit F, typename FV>
constexpr To duration_round(const Duration<F, FV>& d) noexcept {
    const To lo = duration_floor<To>(d);
    const To hi = lo + To(1);
    const Duration<F, FV> a = d - duration_cast<Duration<F, FV>>(lo);
    const Duration<F, FV> b = duration_cast<Duration<F, FV>>(hi) - d;
    if (a < b) return lo;
    if (b < a) return hi;
    return (lo.count() % typename To::value_type(2) == typename To::value_type(0)) ? lo : hi;
}

template <Unit F, typename FV>
constexpr Duration<F, FV> duration_abs(const Duration<F, FV>& d) noexcept {
    return d < Duration<F, FV>(0) ? Duration<F, FV>(0) - d : d;
}

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_CIVIL_HPP
