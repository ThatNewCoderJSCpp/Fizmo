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

bool local_time(std::time_t t, std::tm& out) noexcept;

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

    Date add_months(int n) const noexcept;

    Date add_years(int n) const noexcept { return add_months(n * 12); }
    Date first_of_month() const noexcept { return Date(year, month, 1); }

    constexpr bool operator==(const Date& o) const noexcept { return year == o.year && month == o.month && day == o.day; }
    constexpr bool operator!=(const Date& o) const noexcept { return !(*this == o); }
    constexpr bool operator<(const Date& o) const noexcept { return year != o.year ? year < o.year : month != o.month ? month < o.month : day < o.day; }
    constexpr bool operator>(const Date& o) const noexcept { return o < *this; }
    constexpr bool operator<=(const Date& o) const noexcept { return !(o < *this); }
    constexpr bool operator>=(const Date& o) const noexcept { return !(*this < o); }

    static Date today();

    std::string format(const std::string& fmt = "YYYY-MM-DD") const;

    static std::optional<Date> parse(const std::string& text, const std::string& fmt = "YYYY-MM-DD");
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

    static TimeOfDay now();

    std::string format(bool h24 = true, bool seconds = false) const;

    static std::optional<TimeOfDay> parse(const std::string& text);
};

struct DateTime {
    Date      date;
    TimeOfDay time;

    std::time_t to_time_t() const;

    static DateTime from_time_t(std::time_t t);

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

    Rect btn(int i) const noexcept;

    Date grid_start() const noexcept;

    Rect cell_rect(int i) const noexcept;

    bool allowed(const Date& d) const noexcept { return (!m_min || d >= *m_min) && (!m_max || d <= *m_max); }

    void move_cursor(const Date& d);

    void chevron(Painter& p, const Rect& r, int dir, int count, const Color& c);

public:
    explicit Calendar(const Date& selected = Date::today());

    const Date& selected() const noexcept { return m_selected; }
    Calendar& set_selected(const Date& d, bool notify = false);
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

    void pick(const Date& d);

    Rect day_rect(const Date& d) const noexcept;

    Size measure(Ui&) override { return Size{ pad() * 2.0f + cell_w() * 7.0f, pad() * 2.0f + header_h() + weekday_h() + cell_h() * 6.0f }; }

    void draw(Painter& p, Ui& ui) override;

    bool on_mouse_down(MouseEvent& e) override;

    bool on_wheel(MouseEvent& e) override;

    bool on_key(const KeyEvent& k) override;
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

    void committed() override;

    void apply(const Date& d, bool notify);

public:
    explicit DateField(const Date& date = Date::today(), std::string format = "YYYY-MM-DD");

    ~DateField() override { if (ui() && m_calendar) ui()->close_popup(*m_calendar); }

    const Date& date() const noexcept { return m_date; }
    DateField& set_date(const Date& d, bool notify = false) { apply(clamp(d), notify); return *this; }
    DateField& set_format(std::string f);
    const std::string& format() const noexcept { return m_format; }
    DateField& set_range(std::optional<Date> min, std::optional<Date> max);
    DateField& set_first_weekday(int wd) { m_calendar->set_first_weekday(wd); return *this; }
    DateField& on_date_change(std::function<void(const Date&)> fn) { m_on_date = std::move(fn); return *this; }
    Calendar& calendar() noexcept { return *m_calendar; }
    bool calendar_open() const noexcept { return ui() && ui()->popup_open(*m_calendar); }

    void open_calendar();

    void close_calendar() { if (ui() && m_calendar) ui()->close_popup(*m_calendar); }

    void committed_text() {
        const auto d = Date::parse(m_text, m_format);
        if (d) m_date = clamp(*d);
    }

    windows::SystemCursor cursor(float x, float y) const noexcept override {
        return button_rect().contains(x, y) ? windows::SystemCursor::Hand : TextField::cursor(x, y);
    }

    Size measure(Ui& ui) override;

    void draw(Painter& p, Ui& ui) override;

    bool on_mouse_down(MouseEvent& e) override;

    bool on_key(const KeyEvent& k) override;
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

    void configure();

    void changed() { if (m_on_change) m_on_change(value()); }

    IntField& make_field(long long max);

public:
    explicit TimeField(const TimeOfDay& t = TimeOfDay(), bool h24 = true, bool seconds = false);

    TimeOfDay value() const;

    TimeField& set_value(const TimeOfDay& t, bool notify = false);

    TimeField& set_24h(bool h24) { const TimeOfDay t = value(); m_24h = h24; return set_value(t); }
    TimeField& set_show_seconds(bool s) { const TimeOfDay t = value(); m_seconds = s; return set_value(t); }
    TimeField& set_spin_buttons(bool s);
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
    explicit DateTimeField(const DateTime& dt = DateTime::now(), bool h24 = true, bool seconds = false);

    DateTime value() const { return DateTime{ m_date->date(), m_time->value() }; }
    DateTimeField& set_value(const DateTime& dt, bool notify = false);
    DateTimeField& on_change(std::function<void(const DateTime&)> fn) { m_on_change = std::move(fn); return *this; }
    DateField& date_field() noexcept { return *m_date; }
    TimeField& time_field() noexcept { return *m_time; }
    bool hit_self(float, float) const noexcept override { return false; }
};

} // namespace ui
} // namespace fizmo

#endif // FIZMO_UI_DATETIME_HPP
