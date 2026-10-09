#ifndef FIZMO_UI_DATETIME_HPP
#define FIZMO_UI_DATETIME_HPP

#include "ui_controls.hpp"
#include <ctime>

namespace fizmo {
namespace ui {

struct DateNames {
    std::array<std::string, 12> months{ { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" } };
    std::array<std::string, 12> months_short{ { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" } };
    std::array<std::string, 7>  weekdays{ { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" } };
    std::array<std::string, 7>  weekdays_short{ { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" } };
    std::array<std::string, 7>  weekdays_min{ { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" } };
    std::string                 am = "AM", pm = "PM";

    static DateNames& get() { static DateNames names; return names; }
};

inline bool local_time(std::time_t t, std::tm& out) noexcept {
#if defined(_WIN32)
    return localtime_s(&out, &t) == 0;
#else
    return localtime_r(&t, &out) != nullptr;
#endif
}

struct Date {
    int year = 1970, month = 1, day = 1;

    constexpr Date() noexcept = default;
    constexpr Date(int y, int m, int d) noexcept : year(y), month(m), day(d) {}

    static constexpr bool is_leap(int y) noexcept { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
    static constexpr int days_in_month(int y, int m) noexcept { return m == 2 ? (is_leap(y) ? 29 : 28) : (m == 4 || m == 6 || m == 9 || m == 11) ? 30 : 31; }
    constexpr bool valid() const noexcept { return month >= 1 && month <= 12 && day >= 1 && day <= days_in_month(year, month); }

    constexpr long long to_days() const noexcept {
        const long long y = year - (month <= 2 ? 1 : 0);
        const long long era = (y >= 0 ? y : y - 399) / 400;
        const long long yoe = y - era * 400;
        const long long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    static constexpr Date from_days(long long z) noexcept {
        z += 719468;
        const long long era = (z >= 0 ? z : z - 146096) / 146097;
        const long long doe = z - era * 146097;
        const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const long long y = yoe + era * 400;
        const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const long long mp = (5 * doy + 2) / 153;
        const long long d = doy - (153 * mp + 2) / 5 + 1;
        const long long m = mp < 10 ? mp + 3 : mp - 9;
        return Date(static_cast<int>(y + (m <= 2 ? 1 : 0)), static_cast<int>(m), static_cast<int>(d));
    }

    constexpr int weekday() const noexcept { const long long z = to_days(); return static_cast<int>(((z % 7) + 11) % 7); }
    constexpr Date add_days(long long n) const noexcept { return from_days(to_days() + n); }

    Date add_months(int n) const noexcept {
        long long m = static_cast<long long>(year) * 12 + (month - 1) + n;
        const int y = static_cast<int>(m >= 0 ? m / 12 : (m - 11) / 12);
        const int mo = static_cast<int>(m - static_cast<long long>(y) * 12) + 1;
        return Date(y, mo, std::min(day, days_in_month(y, mo)));
    }

    Date add_years(int n) const noexcept { return add_months(n * 12); }
    Date first_of_month() const noexcept { return Date(year, month, 1); }

    constexpr bool operator==(const Date& o) const noexcept { return year == o.year && month == o.month && day == o.day; }
    constexpr bool operator!=(const Date& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const Date& o) const noexcept { return year != o.year ? year < o.year : month != o.month ? month < o.month : day < o.day; }
    constexpr bool operator>(const Date& o) const noexcept { return o < *this; }
    constexpr bool operator<=(const Date& o) const noexcept { return !(o < *this); }
    constexpr bool operator>=(const Date& o) const noexcept { return !(*this < o); }

    static Date today() {
        std::tm tm{};
        if (!local_time(std::time(nullptr), tm)) return Date();
        return Date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }

    std::string format(const std::string& fmt = "YYYY-MM-DD") const {
        const DateNames& n = DateNames::get();
        std::string out;
        char buf[16];
        auto starts = [&](std::size_t i, const char* tok) { return fmt.compare(i, std::strlen(tok), tok) == 0; };
        for (std::size_t i = 0; i < fmt.size();) {
            if (starts(i, "YYYY")) { std::snprintf(buf, sizeof(buf), "%04d", year); out += buf; i += 4; }
            else if (starts(i, "YY")) { std::snprintf(buf, sizeof(buf), "%02d", ((year % 100) + 100) % 100); out += buf; i += 2; }
            else if (starts(i, "MMMM")) { out += n.months[static_cast<std::size_t>(month - 1) % 12]; i += 4; }
            else if (starts(i, "MMM")) { out += n.months_short[static_cast<std::size_t>(month - 1) % 12]; i += 3; }
            else if (starts(i, "MM")) { std::snprintf(buf, sizeof(buf), "%02d", month); out += buf; i += 2; }
            else if (starts(i, "M")) { out += std::to_string(month); i += 1; }
            else if (starts(i, "DD")) { std::snprintf(buf, sizeof(buf), "%02d", day); out += buf; i += 2; }
            else if (starts(i, "D")) { out += std::to_string(day); i += 1; }
            else if (starts(i, "dddd")) { out += n.weekdays[static_cast<std::size_t>(weekday())]; i += 4; }
            else if (starts(i, "ddd")) { out += n.weekdays_short[static_cast<std::size_t>(weekday())]; i += 3; }
            else { out.push_back(fmt[i]); ++i; }
        }
        return out;
    }

    static std::optional<Date> parse(const std::string& text, const std::string& fmt = "YYYY-MM-DD") {
        const DateNames& n = DateNames::get();
        auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
        auto starts = [&](std::size_t i, const char* tok) { return fmt.compare(i, std::strlen(tok), tok) == 0; };
        const std::string lt = lower(text);
        std::size_t p = 0;
        int y = -1, m = -1, d = -1;
        auto skip_space = [&] { while (p < text.size() && text[p] == ' ') ++p; };
        auto number = [&](int max_digits, int& out) {
            skip_space();
            std::size_t st = p;
            while (p < text.size() && p - st < static_cast<std::size_t>(max_digits) && std::isdigit(static_cast<unsigned char>(text[p]))) ++p;
            if (p == st) return false;
            out = std::stoi(text.substr(st, p - st));
            return true;
        };
        auto name = [&](const std::array<std::string, 12>& full, const std::array<std::string, 12>& shrt, int& out) {
            skip_space();
            for (int k = 0; k < 12; ++k) {
                const std::string f = lower(full[static_cast<std::size_t>(k)]), s = lower(shrt[static_cast<std::size_t>(k)]);
                if (lt.compare(p, f.size(), f) == 0) { p += f.size(); out = k + 1; return true; }
                if (lt.compare(p, s.size(), s) == 0) { p += s.size(); out = k + 1; while (p < text.size() && std::isalpha(static_cast<unsigned char>(text[p]))) ++p; return true; }
            }
            return false;
        };
        for (std::size_t i = 0; i < fmt.size();) {
            if (starts(i, "YYYY")) { if (!number(4, y)) return std::nullopt; i += 4; }
            else if (starts(i, "YY")) { int v; if (!number(2, v)) return std::nullopt; y = 2000 + v; i += 2; }
            else if (starts(i, "MMMM")) { if (!name(n.months, n.months_short, m)) return std::nullopt; i += 4; }
            else if (starts(i, "MMM")) { if (!name(n.months, n.months_short, m)) return std::nullopt; i += 3; }
            else if (starts(i, "MM")) { if (!number(2, m)) return std::nullopt; i += 2; }
            else if (starts(i, "M")) { if (!number(2, m)) return std::nullopt; i += 1; }
            else if (starts(i, "DD")) { if (!number(2, d)) return std::nullopt; i += 2; }
            else if (starts(i, "D")) { if (!number(2, d)) return std::nullopt; i += 1; }
            else if (starts(i, "dddd") || starts(i, "ddd")) { skip_space(); while (p < text.size() && std::isalpha(static_cast<unsigned char>(text[p]))) ++p; i += starts(i, "dddd") ? 4 : 3; }
            else {
                const char c = fmt[i];
                if (!std::isalnum(static_cast<unsigned char>(c))) { while (p < text.size() && !std::isalnum(static_cast<unsigned char>(text[p]))) ++p; }
                else { if (p >= text.size() || std::tolower(static_cast<unsigned char>(text[p])) != std::tolower(static_cast<unsigned char>(c))) return std::nullopt; ++p; }
                ++i;
            }
        }
        skip_space();
        if (p != text.size() || y < 0 || m < 0 || d < 0) return std::nullopt;
        const Date out(y, m, d);
        if (!out.valid()) return std::nullopt;
        return out;
    }
};

struct TimeOfDay {
    int hour = 0, minute = 0, second = 0;

    constexpr TimeOfDay() noexcept = default;
    constexpr TimeOfDay(int h, int m, int s = 0) noexcept : hour(h), minute(m), second(s) {}

    constexpr bool valid() const noexcept { return hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60; }
    constexpr int total_seconds() const noexcept { return hour * 3600 + minute * 60 + second; }
    static constexpr TimeOfDay from_seconds(long long s) noexcept { s %= 86400; if (s < 0) s += 86400; return TimeOfDay(static_cast<int>(s / 3600), static_cast<int>((s / 60) % 60), static_cast<int>(s % 60)); }
    constexpr bool operator==(const TimeOfDay& o) const noexcept { return hour == o.hour && minute == o.minute && second == o.second; }
    constexpr bool operator!=(const TimeOfDay& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const TimeOfDay& o) const noexcept { return total_seconds() < o.total_seconds(); }

    static TimeOfDay now() {
        std::tm tm{};
        if (!local_time(std::time(nullptr), tm)) return TimeOfDay();
        return TimeOfDay(tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    std::string format(bool h24 = true, bool seconds = false) const {
        char buf[32];
        if (h24) {
            if (seconds) std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
            else std::snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
            return buf;
        }
        const int h = hour % 12 == 0 ? 12 : hour % 12;
        if (seconds) std::snprintf(buf, sizeof(buf), "%d:%02d:%02d ", h, minute, second);
        else std::snprintf(buf, sizeof(buf), "%d:%02d ", h, minute);
        return std::string(buf) + (hour < 12 ? DateNames::get().am : DateNames::get().pm);
    }

    static std::optional<TimeOfDay> parse(const std::string& text) {
        int h = -1, m = 0, s = 0;
        std::size_t p = 0;
        auto num = [&](int& out) {
            while (p < text.size() && text[p] == ' ') ++p;
            const std::size_t st = p;
            while (p < text.size() && p - st < 2 && std::isdigit(static_cast<unsigned char>(text[p]))) ++p;
            if (p == st) return false;
            out = std::stoi(text.substr(st, p - st));
            return true;
        };
        if (!num(h)) return std::nullopt;
        if (p < text.size() && text[p] == ':') { ++p; if (!num(m)) return std::nullopt; }
        if (p < text.size() && text[p] == ':') { ++p; if (!num(s)) return std::nullopt; }
        while (p < text.size() && text[p] == ' ') ++p;
        std::string rest = text.substr(p);
        for (char& c : rest) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto lower = [](std::string x) { for (char& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return x; };
        if (!rest.empty()) {
            const bool am = rest == lower(DateNames::get().am) || rest == "a", pm = rest == lower(DateNames::get().pm) || rest == "p";
            if ((!am && !pm) || h < 1 || h > 12) return std::nullopt;
            h = h % 12 + (pm ? 12 : 0);
        }
        const TimeOfDay t(h, m, s);
        if (!t.valid()) return std::nullopt;
        return t;
    }
};

struct DateTime {
    Date      date;
    TimeOfDay time;

    std::time_t to_time_t() const {
        std::tm tm{};
        tm.tm_year = date.year - 1900; tm.tm_mon = date.month - 1; tm.tm_mday = date.day;
        tm.tm_hour = time.hour; tm.tm_min = time.minute; tm.tm_sec = time.second;
        tm.tm_isdst = -1;
        return std::mktime(&tm);
    }

    static DateTime from_time_t(std::time_t t) {
        std::tm tm{};
        if (!local_time(t, tm)) return DateTime();
        return DateTime{ Date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday), TimeOfDay(tm.tm_hour, tm.tm_min, tm.tm_sec) };
    }

    static DateTime now() { return from_time_t(std::time(nullptr)); }
    bool operator==(const DateTime& o) const noexcept { return date == o.date && time == o.time; }
    bool operator!=(const DateTime& o) const noexcept { return !(*this == o); }
};

class Calendar : public Widget {
protected:
    Date                             m_selected;
    Date                             m_view;
    Date                             m_cursor;
    std::optional<Date>              m_min, m_max;
    int                              m_first_weekday = 0;
    bool                             m_popup_style = false;
    std::function<void(const Date&)> m_on_change, m_on_pick;

    float pad() const noexcept { return 8.0f; }
    float cell_w() const noexcept { return 32.0f; }
    float cell_h() const noexcept { return 28.0f; }
    float header_h() const noexcept { return theme().row_height + 4.0f; }
    float weekday_h() const noexcept { return 22.0f; }

    Rect btn(int i) const noexcept {
        const float w = 24.0f, h = header_h() - 4.0f, y = pad();
        switch (i) {
            case 0: return Rect(pad(), y, w, h);
            case 1: return Rect(pad() + w + 2.0f, y, w, h);
            case 2: return Rect(width() - pad() - 2.0f * w - 2.0f, y, w, h);
            default: return Rect(width() - pad() - w, y, w, h);
        }
    }

    Date grid_start() const noexcept {
        const Date f = m_view.first_of_month();
        const int off = (f.weekday() - m_first_weekday + 7) % 7;
        return f.add_days(-off);
    }

    Rect cell_rect(int i) const noexcept {
        const float x0 = pad(), y0 = pad() + header_h() + weekday_h();
        return Rect(x0 + (i % 7) * cell_w(), y0 + (i / 7) * cell_h(), cell_w(), cell_h());
    }

    bool allowed(const Date& d) const noexcept { return (!m_min || d >= *m_min) && (!m_max || d <= *m_max); }

    void move_cursor(const Date& d) {
        Date c = d;
        if (m_min && c < *m_min) c = *m_min;
        if (m_max && c > *m_max) c = *m_max;
        m_cursor = c;
        m_view = c.first_of_month();
    }

    void chevron(Painter& p, const Rect& r, int dir, int count, const Color& c) {
        const float s = 3.5f, cy = r.center_y();
        for (int k = 0; k < count; ++k) {
            const float cx = r.center_x() + (count == 2 ? (k == 0 ? -3.0f : 3.0f) : 0.0f);
            p.polyline({ Point{ cx + s * 0.5f * dir, cy - s }, Point{ cx - s * 0.5f * dir, cy }, Point{ cx + s * 0.5f * dir, cy + s } }, c, 1.5f);
        }
    }

public:
    explicit Calendar(const Date& selected = Date::today()) : m_selected(selected), m_view(selected.first_of_month()), m_cursor(selected) { m_focusable = true; }

    const Date& selected() const noexcept { return m_selected; }
    Calendar& set_selected(const Date& d, bool notify = false) {
        const bool ch = d != m_selected;
        m_selected = d;
        m_cursor = d;
        m_view = d.first_of_month();
        if (notify && ch && m_on_change) m_on_change(d);
        return *this;
    }
    const Date& view_month() const noexcept { return m_view; }
    Calendar& show_month(int year, int month) noexcept { m_view = Date(year, month, 1); return *this; }
    const Date& cursor_date() const noexcept { return m_cursor; }
    Calendar& set_range(std::optional<Date> min, std::optional<Date> max) noexcept { m_min = min; m_max = max; return *this; }
    Calendar& set_first_weekday(int wd) noexcept { m_first_weekday = ((wd % 7) + 7) % 7; return *this; }
    Calendar& set_popup_style(bool p) noexcept { m_popup_style = p; return *this; }
    Calendar& on_change(std::function<void(const Date&)> fn) { m_on_change = std::move(fn); return *this; }
    Calendar& on_pick(std::function<void(const Date&)> fn) { m_on_pick = std::move(fn); return *this; }
    bool is_allowed(const Date& d) const noexcept { return allowed(d); }
    void previous_month() { m_view = m_view.add_months(-1); }
    void next_month() { m_view = m_view.add_months(1); }

    void pick(const Date& d) {
        if (!allowed(d)) return;
        const bool ch = d != m_selected;
        m_selected = d;
        m_cursor = d;
        m_view = d.first_of_month();
        if (ch && m_on_change) m_on_change(d);
        if (m_on_pick) m_on_pick(d);
    }

    Rect day_rect(const Date& d) const noexcept {
        const long long i = d.to_days() - grid_start().to_days();
        if (i < 0 || i >= 42) return Rect();
        return cell_rect(static_cast<int>(i));
    }

    Size measure(Ui&) override { return Size{ pad() * 2.0f + cell_w() * 7.0f, pad() * 2.0f + header_h() + weekday_h() + cell_h() * 6.0f }; }

    void draw(Painter& p, Ui& ui) override {
        const Theme& t = theme();
        const DateNames& n = DateNames::get();
        if (m_popup_style) p.box(bounds(), t.radius, t.popup, t.border, t.border_width);
        const Point mp = ui.mouse();
        const Rect sr = screen_rect();
        const float mx = mp.x - sr.x, my = mp.y - sr.y;
        const bool en = enabled();
        for (int i = 0; i < 4; ++i) {
            const Rect b = btn(i);
            if (hovered() && b.contains(mx, my) && en) p.fill_rounded(b, t.radius, t.surface_hover);
            chevron(p, b, i < 2 ? 1 : -1, (i == 0 || i == 3) ? 2 : 1, en ? t.text_muted : t.text_disabled);
        }
        const std::string title = n.months[static_cast<std::size_t>(m_view.month - 1) % 12] + " " + std::to_string(m_view.year);
        const Size ts = text_size(title, 0.0, true);
        p.text(std::round((width() - ts.w) * 0.5f), std::round(pad() + (header_h() - 4.0f - ts.h) * 0.5f), title, text_style(en ? t.text : t.text_disabled, 0.0, true));
        const double small = t.font_size * 0.85;
        for (int c = 0; c < 7; ++c) {
            const std::string& wd = n.weekdays_min[static_cast<std::size_t>((c + m_first_weekday) % 7)];
            const Size ws = text_size(wd, small);
            p.text(std::round(pad() + c * cell_w() + (cell_w() - ws.w) * 0.5f), std::round(pad() + header_h() + (weekday_h() - ws.h) * 0.5f), wd, text_style(t.text_muted, small));
        }
        const Date start = grid_start(), today = Date::today();
        for (int i = 0; i < 42; ++i) {
            const Date d = start.add_days(i);
            const Rect r = cell_rect(i).inset(1.5f);
            const bool in_month = d.month == m_view.month;
            const bool ok = allowed(d) && en;
            const bool sel = d == m_selected;
            if (sel) p.fill_rounded(r, t.radius, ok ? t.accent : t.text_disabled);
            else if (ok && hovered() && cell_rect(i).contains(mx, my)) p.fill_rounded(r, t.radius, t.surface_hover);
            if (d == today && !sel) p.stroke_rounded(r, t.radius, t.accent, 1.0f);
            if (d == m_cursor && focus_visible()) p.stroke_rounded(r.inset(-1.0f), t.radius + 1.0f, t.focus, t.focus_width);
            const std::string s = std::to_string(d.day);
            const Size ds = text_size(s);
            const Color c = sel ? t.accent_text : !ok ? t.text_disabled : in_month ? t.text : t.text_muted;
            p.text(std::round(r.center_x() - ds.w * 0.5f), std::round(r.center_y() - ds.h * 0.5f), s, text_style(c));
        }
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button != input::MouseButton::Left) return true;
        for (int i = 0; i < 4; ++i) {
            if (!btn(i).contains(e.x, e.y)) continue;
            m_view = m_view.add_months(i == 0 ? -12 : i == 1 ? -1 : i == 2 ? 1 : 12);
            return true;
        }
        const Date start = grid_start();
        for (int i = 0; i < 42; ++i) if (cell_rect(i).contains(e.x, e.y)) { pick(start.add_days(i)); return true; }
        return true;
    }

    bool on_wheel(MouseEvent& e) override {
        if (e.wheel_y == 0.0f) return false;
        m_view = m_view.add_months(e.wheel_y > 0.0f ? -1 : 1);
        return true;
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        switch (k.key) {
            case K::Left: move_cursor(m_cursor.add_days(-1)); return true;
            case K::Right: move_cursor(m_cursor.add_days(1)); return true;
            case K::Up: move_cursor(m_cursor.add_days(-7)); return true;
            case K::Down: move_cursor(m_cursor.add_days(7)); return true;
            case K::PageUp: move_cursor(k.shift() ? m_cursor.add_years(-1) : m_cursor.add_months(-1)); return true;
            case K::PageDown: move_cursor(k.shift() ? m_cursor.add_years(1) : m_cursor.add_months(1)); return true;
            case K::Home: move_cursor(m_cursor.first_of_month()); return true;
            case K::End: move_cursor(Date(m_cursor.year, m_cursor.month, Date::days_in_month(m_cursor.year, m_cursor.month))); return true;
            case K::Enter: case K::NumpadEnter: case K::Space: pick(m_cursor); return true;
            default: return false;
        }
    }
};

class DateField : public TextField {
protected:
    Date                             m_date;
    std::string                      m_format = "YYYY-MM-DD";
    std::unique_ptr<Calendar>        m_calendar;
    std::function<void(const Date&)> m_on_date;
    std::optional<Date>              m_min, m_max;

    float extra_right() const override { return 22.0f; }
    Rect button_rect() const noexcept { return Rect(width() - 26.0f, 1.0f, 25.0f, height() - 2.0f); }

    Date clamp(Date d) const noexcept {
        if (m_min && d < *m_min) d = *m_min;
        if (m_max && d > *m_max) d = *m_max;
        return d;
    }

    void committed() override {
        const auto d = Date::parse(m_text, m_format);
        if (d && (!m_min || *d >= *m_min) && (!m_max || *d <= *m_max)) apply(*d, true);
        TextField::set_text(m_date.format(m_format));
        TextField::committed();
    }

    void apply(const Date& d, bool notify) {
        const bool ch = d != m_date;
        m_date = d;
        TextField::set_text(m_date.format(m_format));
        if (notify && ch && m_on_date) m_on_date(m_date);
    }

public:
    explicit DateField(const Date& date = Date::today(), std::string format = "YYYY-MM-DD") : TextField("", ""), m_date(date), m_format(std::move(format)) {
        m_select_on_focus = true;
        m_placeholder = m_format;
        TextField::set_text(m_date.format(m_format));
        m_calendar = std::make_unique<Calendar>(m_date);
        m_calendar->set_popup_style(true);
        m_calendar->on_pick([this](const Date& d) { apply(d, true); close_calendar(); });
    }

    ~DateField() override { if (ui() && m_calendar) ui()->close_popup(*m_calendar); }

    const Date& date() const noexcept { return m_date; }
    DateField& set_date(const Date& d, bool notify = false) { apply(clamp(d), notify); return *this; }
    DateField& set_format(std::string f) { m_format = std::move(f); m_placeholder = m_format; TextField::set_text(m_date.format(m_format)); return *this; }
    const std::string& format() const noexcept { return m_format; }
    DateField& set_range(std::optional<Date> min, std::optional<Date> max) { m_min = min; m_max = max; m_calendar->set_range(min, max); apply(clamp(m_date), false); return *this; }
    DateField& set_first_weekday(int wd) { m_calendar->set_first_weekday(wd); return *this; }
    DateField& on_date_change(std::function<void(const Date&)> fn) { m_on_date = std::move(fn); return *this; }
    Calendar& calendar() noexcept { return *m_calendar; }
    bool calendar_open() const noexcept { return ui() && ui()->popup_open(*m_calendar); }

    void open_calendar() {
        if (!ui() || calendar_open() || !enabled()) return;
        committed_text();
        m_calendar->set_selected(m_date);
        ui()->open_popup(*this, *m_calendar, screen_rect(), Placement::Below);
    }

    void close_calendar() { if (ui() && m_calendar) ui()->close_popup(*m_calendar); }

    void committed_text() {
        const auto d = Date::parse(m_text, m_format);
        if (d) m_date = clamp(*d);
    }

    windows::SystemCursor cursor(float x, float y) const noexcept override {
        return button_rect().contains(x, y) ? windows::SystemCursor::Hand : TextField::cursor(x, y);
    }

    Size measure(Ui& ui) override {
        Size s = TextField::measure(ui);
        s.w = std::max(120.0f, text_width(Date(2026, 12, 28).format(m_format)) + theme().padding * 2.0f + extra_right() + 12.0f);
        return s;
    }

    void draw(Painter& p, Ui& ui) override {
        TextField::draw(p, ui);
        const Theme& t = theme();
        const Rect b = button_rect();
        const Point m = ui.mouse();
        const Rect sr = screen_rect();
        if ((hovered() && b.contains(m.x - sr.x, m.y - sr.y)) || calendar_open()) p.fill_rounded(b.inset(2.0f), t.radius, t.surface_hover);
        const Color c = enabled() ? t.text_muted : t.text_disabled;
        const Rect ic(std::round(b.center_x() - 6.0f), std::round(b.center_y() - 5.0f), 12.0f, 11.0f);
        p.stroke_rounded(ic, 1.5f, c, 1.2f);
        p.fill_rect(Rect(ic.x, ic.y, ic.w, 3.0f), c);
        p.fill_rect(Rect(ic.x + 3.0f, ic.y - 2.0f, 1.2f, 3.0f), c);
        p.fill_rect(Rect(ic.right() - 4.2f, ic.y - 2.0f, 1.2f, 3.0f), c);
    }

    bool on_mouse_down(MouseEvent& e) override {
        if (e.button == input::MouseButton::Left && button_rect().contains(e.x, e.y)) {
            if (calendar_open()) close_calendar(); else open_calendar();
            return true;
        }
        return TextField::on_mouse_down(e);
    }

    bool on_key(const KeyEvent& k) override {
        using K = input::Key;
        if (calendar_open()) {
            if (k.key == K::Escape || k.key == K::Tab) { close_calendar(); return k.key == K::Escape; }
            return m_calendar->on_key(k);
        }
        if ((k.key == K::Down && k.alt()) || k.key == K::F4) { open_calendar(); return true; }
        if (k.key == K::Up || k.key == K::Down) {
            committed_text();
            apply(clamp(m_date.add_days(k.key == K::Up ? 1 : -1)), true);
            select_all();
            return true;
        }
        if (k.key == K::PageUp || k.key == K::PageDown) {
            committed_text();
            apply(clamp(m_date.add_months(k.key == K::PageUp ? 1 : -1)), true);
            select_all();
            return true;
        }
        return TextField::on_key(k);
    }
};

class TimeField : public HStack {
protected:
    IntField*                             m_hour = nullptr;
    IntField*                             m_minute = nullptr;
    IntField*                             m_second = nullptr;
    Label*                                m_sep1 = nullptr;
    Label*                                m_sep2 = nullptr;
    Button*                               m_ampm = nullptr;
    bool                                  m_24h = true;
    bool                                  m_seconds = false;
    bool                                  m_pm = false;
    std::function<void(const TimeOfDay&)> m_on_change;

    static std::string two(long long v) { char b[8]; std::snprintf(b, sizeof(b), "%02lld", v); return b; }

    void configure() {
        m_hour->set_range(m_24h ? 0 : 1, m_24h ? 23 : 12);
        m_second->set_visible(m_seconds);
        m_sep2->set_visible(m_seconds);
        m_ampm->set_visible(!m_24h);
        m_ampm->set_text(m_pm ? DateNames::get().pm : DateNames::get().am);
    }

    void changed() { if (m_on_change) m_on_change(value()); }

    IntField& make_field(long long max) {
        IntField& f = add<IntField>(0, 0, max, 1);
        f.set_wrap(true);
        f.set_formatter(two);
        f.set_width(44.0f);
        f.on_value_change([this](long long) { changed(); });
        return f;
    }

public:
    explicit TimeField(const TimeOfDay& t = TimeOfDay(), bool h24 = true, bool seconds = false) : HStack(2.0f), m_24h(h24), m_seconds(seconds) {
        m_cross = Align::Stretch;
        m_hour = &make_field(23);
        m_sep1 = &add<Label>(":");
        m_minute = &make_field(59);
        m_sep2 = &add<Label>(":");
        m_second = &make_field(59);
        m_ampm = &add<Button>("AM", [this]() { m_pm = !m_pm; configure(); changed(); });
        m_ampm->set_width(44.0f);
        m_ampm->set_style(ButtonStyle::Normal);
        set_value(t);
    }

    TimeOfDay value() const {
        int h = static_cast<int>(m_hour->value());
        if (!m_24h) h = h % 12 + (m_pm ? 12 : 0);
        return TimeOfDay(h, static_cast<int>(m_minute->value()), m_seconds ? static_cast<int>(m_second->value()) : 0);
    }

    TimeField& set_value(const TimeOfDay& t, bool notify = false) {
        const TimeOfDay before = value();
        m_pm = t.hour >= 12;
        configure();
        m_hour->set_value(m_24h ? t.hour : (t.hour % 12 == 0 ? 12 : t.hour % 12));
        m_minute->set_value(t.minute);
        m_second->set_value(t.second);
        if (notify && value() != before) changed();
        return *this;
    }

    TimeField& set_24h(bool h24) { const TimeOfDay t = value(); m_24h = h24; return set_value(t); }
    TimeField& set_show_seconds(bool s) { const TimeOfDay t = value(); m_seconds = s; return set_value(t); }
    TimeField& set_spin_buttons(bool s) {
        for (IntField* f : { m_hour, m_minute, m_second }) { f->set_spin_buttons(s); f->set_width(s ? 58.0f : 44.0f); }
        return *this;
    }
    TimeField& set_minute_step(int step) { m_minute->set_step(std::max(1, step)); return *this; }
    TimeField& on_change(std::function<void(const TimeOfDay&)> fn) { m_on_change = std::move(fn); return *this; }
    IntField& hour_field() noexcept { return *m_hour; }
    IntField& minute_field() noexcept { return *m_minute; }
    IntField& second_field() noexcept { return *m_second; }
    Button& am_pm_button() noexcept { return *m_ampm; }
    bool hit_self(float, float) const noexcept override { return false; }
};

class DateTimeField : public HStack {
protected:
    DateField*                           m_date = nullptr;
    TimeField*                           m_time = nullptr;
    std::function<void(const DateTime&)> m_on_change;

public:
    explicit DateTimeField(const DateTime& dt = DateTime::now(), bool h24 = true, bool seconds = false) : HStack(6.0f) {
        m_date = &add<DateField>(dt.date);
        m_time = &add<TimeField>(dt.time, h24, seconds);
        m_date->on_date_change([this](const Date&) { if (m_on_change) m_on_change(value()); });
        m_time->on_change([this](const TimeOfDay&) { if (m_on_change) m_on_change(value()); });
    }

    DateTime value() const { return DateTime{ m_date->date(), m_time->value() }; }
    DateTimeField& set_value(const DateTime& dt, bool notify = false) {
        const DateTime before = value();
        m_date->set_date(dt.date);
        m_time->set_value(dt.time);
        if (notify && before != value() && m_on_change) m_on_change(value());
        return *this;
    }
    DateTimeField& on_change(std::function<void(const DateTime&)> fn) { m_on_change = std::move(fn); return *this; }
    DateField& date_field() noexcept { return *m_date; }
    TimeField& time_field() noexcept { return *m_time; }
    bool hit_self(float, float) const noexcept override { return false; }
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_DATETIME_HPP
