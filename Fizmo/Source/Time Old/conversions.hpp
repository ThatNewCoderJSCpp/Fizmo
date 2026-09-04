#ifndef FIZMO_CHRONO_TIME_CONVERTERS_HPP
#define FIZMO_CHRONO_TIME_CONVERTERS_HPP

#include "calendar.hpp"

namespace fizmo {
namespace time {

class conversions {
public:
    // nanosecond conversions
    static constexpr microsecond nanoseconds_to_microseconds(const nanosecond ns) noexcept { return microsecond(ns.value() / 1000); }
    static constexpr millisecond nanoseconds_to_milliseconds(const nanosecond ns) noexcept { return millisecond(ns.value() / 1000000); }
    static constexpr centisecond nanoseconds_to_centiseconds(const nanosecond ns) noexcept { return centisecond(ns.value() / 10000000); }
    static constexpr decisecond nanoseconds_to_deciseconds(const nanosecond ns) noexcept { return decisecond(ns.value() / 100000000); }
    static constexpr second nanoseconds_to_seconds(const nanosecond ns) noexcept { return second(ns.value() / 1000000000); }
    static constexpr minute nanoseconds_to_minutes(const nanosecond ns) noexcept { return minute(ns.value() / 60000000000); }
    static constexpr hour nanoseconds_to_hours(const nanosecond ns) noexcept { return hour(ns.value() / 3600000000000); }
    static constexpr day nanoseconds_to_days(const nanosecond ns) noexcept { return day(ns.value() / 86400000000000); }
    static constexpr week nanoseconds_to_weeks(const nanosecond ns) noexcept { return week(ns.value() / 604800000000000); }
    static constexpr year nanoseconds_to_years(const nanosecond ns) noexcept { return year(ns.value() / 31536000000000000); }
    static constexpr year nanoseconds_to_years(const nanosecond ns, const year starting_year) noexcept { return days_to_years(nanoseconds_to_days(ns), starting_year); }
    static constexpr decade nanoseconds_to_decades(const nanosecond ns) noexcept { return decade(ns.value() / 315360000000000000); }
    static constexpr decade nanoseconds_to_decades(const nanosecond ns, const year starting_year) noexcept { return days_to_decades(nanoseconds_to_days(ns), starting_year); }
    static constexpr century nanoseconds_to_centuries(const nanosecond ns) noexcept { return century(ns.value() / 3153600000000000000); }
    static constexpr century nanoseconds_to_centuries(const nanosecond ns, const year starting_year) noexcept { return days_to_centuries(nanoseconds_to_days(ns), starting_year); }
    static constexpr millennium nanoseconds_to_millennia(const nanosecond ns) noexcept { return millennium(0); }
    static constexpr millennium nanoseconds_to_millennia(const nanosecond ns, const year starting_year) noexcept { return days_to_millennia(nanoseconds_to_days(ns), starting_year); }

public:
    // microsecond conversions
    static constexpr nanosecond microseconds_to_nanoseconds(const microsecond us) noexcept { 
        return us.value() > std::numeric_limits<std::uint64_t>::max() / 1000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(us.value() * 1000); 
    }
    static constexpr millisecond microseconds_to_milliseconds(const microsecond us) noexcept { return millisecond(us.value() / 1000); }
    static constexpr centisecond microseconds_to_centiseconds(const microsecond us) noexcept { return centisecond(us.value() / 10000); }
    static constexpr decisecond microseconds_to_deciseconds(const microsecond us) noexcept { return decisecond(us.value() / 100000); }
    static constexpr second microseconds_to_seconds(const microsecond us) noexcept { return second(us.value() / 1000000); }
    static constexpr minute microseconds_to_minutes(const microsecond us) noexcept { return minute(us.value() / 60000000); }
    static constexpr hour microseconds_to_hours(const microsecond us) noexcept { return hour(us.value() / 3600000000); }
    static constexpr day microseconds_to_days(const microsecond us) noexcept { return day(us.value() / 86400000000); }
    static constexpr week microseconds_to_weeks(const microsecond us) noexcept { return week(us.value() / 604800000000); }
    static constexpr year microseconds_to_years(const microsecond us) noexcept { return year(us.value() / 31536000000000); }
    static constexpr year microseconds_to_years(const microsecond us, const year starting_year) noexcept { return days_to_years(microseconds_to_days(us), starting_year); }
    static constexpr decade microseconds_to_decades(const microsecond us) noexcept { return decade(us.value() / 315360000000000); }
    static constexpr decade microseconds_to_decades(const microsecond us, const year starting_year) noexcept { return days_to_decades(microseconds_to_days(us), starting_year); }
    static constexpr century microseconds_to_centuries(const microsecond us) noexcept { return century(us.value() / 3153600000000000); }
    static constexpr century microseconds_to_centuries(const microsecond us, const year starting_year) noexcept { return days_to_centuries(microseconds_to_days(us), starting_year); }
    static constexpr millennium microseconds_to_millennia(const microsecond us) noexcept { return millennium(us.value() / 31536000000000000); }
    static constexpr millennium microseconds_to_millennia(const microsecond us, const year starting_year) noexcept { return days_to_millennia(microseconds_to_days(us), starting_year); }

public:
    // millisecond conversions
    static constexpr nanosecond milliseconds_to_nanoseconds(const millisecond ms) noexcept { 
        return ms.value() > std::numeric_limits<std::uint64_t>::max() / 1000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(ms.value() * 1000000); 
    }
    
    static constexpr microsecond milliseconds_to_microseconds(const millisecond ms) noexcept { 
        return ms.value() > std::numeric_limits<std::uint64_t>::max() / 1000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(ms.value() * 1000); 
    }
    
    static constexpr centisecond milliseconds_to_centiseconds(const millisecond ms) noexcept { return centisecond(ms.value() / 10); }
    static constexpr decisecond milliseconds_to_deciseconds(const millisecond ms) noexcept { return decisecond(ms.value() / 100); }
    static constexpr second milliseconds_to_seconds(const millisecond ms) noexcept { return second(ms.value() / 1000); }
    static constexpr minute milliseconds_to_minutes(const millisecond ms) noexcept { return minute(ms.value() / 60000); }
    static constexpr hour milliseconds_to_hours(const millisecond ms) noexcept { return hour(ms.value() / 3600000); }
    static constexpr day milliseconds_to_days(const millisecond ms) noexcept { return day(ms.value() / 86400000); }
    static constexpr week milliseconds_to_weeks(const millisecond ms) noexcept { return week(ms.value() / 604800000); }
    static constexpr year milliseconds_to_years(const millisecond ms) noexcept { return year(ms.value() / 31536000000); }
    static constexpr year milliseconds_to_years(const millisecond ms, const year starting_year) noexcept { return days_to_years(milliseconds_to_days(ms), starting_year); }
    static constexpr decade milliseconds_to_decades(const millisecond ms) noexcept { return decade(ms.value() / 315360000000); }
    static constexpr decade milliseconds_to_decades(const millisecond ms, const year starting_year) noexcept { return days_to_decades(milliseconds_to_days(ms), starting_year); }
    static constexpr century milliseconds_to_centuries(const millisecond ms) noexcept { return century(ms.value() / 3153600000000); }
    static constexpr century milliseconds_to_centuries(const millisecond ms, const year starting_year) noexcept { return days_to_centuries(milliseconds_to_days(ms), starting_year); }
    static constexpr millennium milliseconds_to_millennia(const millisecond ms) noexcept { return millennium(ms.value() / 31536000000000); }
    static constexpr millennium milliseconds_to_millennia(const millisecond ms, const year starting_year) noexcept { return days_to_millennia(milliseconds_to_days(ms), starting_year); }

public:
    // centisecond conversions
    static constexpr nanosecond centiseconds_to_nanoseconds(const centisecond cs) noexcept { 
        return cs.value() > std::numeric_limits<std::uint64_t>::max() / 10000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(cs.value() * 10000000); 
    }
    
    static constexpr microsecond centiseconds_to_microseconds(const centisecond cs) noexcept { 
        return cs.value() > std::numeric_limits<std::uint64_t>::max() / 10000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(cs.value() * 10000); 
    }
    
    static constexpr millisecond centiseconds_to_milliseconds(const centisecond cs) noexcept { 
        return cs.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(cs.value() * 10); 
    }
    
    static constexpr decisecond centiseconds_to_deciseconds(const centisecond cs) noexcept { return decisecond(cs.value() / 10); }
    static constexpr second centiseconds_to_seconds(const centisecond cs) noexcept { return second(cs.value() / 100); }
    static constexpr minute centiseconds_to_minutes(const centisecond cs) noexcept { return minute(cs.value() / 6000); }
    static constexpr hour centiseconds_to_hours(const centisecond cs) noexcept { return hour(cs.value() / 360000); }
    static constexpr day centiseconds_to_days(const centisecond cs) noexcept { return day(cs.value() / 8640000); }
    static constexpr week centiseconds_to_weeks(const centisecond cs) noexcept { return week(cs.value() / 60480000); }
    static constexpr year centiseconds_to_years(const centisecond cs) noexcept { return year(cs.value() / 3153600000); }
    static constexpr year centiseconds_to_years(const centisecond cs, const year starting_year) noexcept { return days_to_years(centiseconds_to_days(cs), starting_year); }
    static constexpr decade centiseconds_to_decades(const centisecond cs) noexcept { return decade(cs.value() / 31536000000); }
    static constexpr decade centiseconds_to_decades(const centisecond cs, const year starting_year) noexcept { return days_to_decades(centiseconds_to_days(cs), starting_year); }
    static constexpr century centiseconds_to_centuries(const centisecond cs) noexcept { return century(cs.value() / 315360000000); }
    static constexpr century centiseconds_to_centuries(const centisecond cs, const year starting_year) noexcept { return days_to_centuries(centiseconds_to_days(cs), starting_year); }
    static constexpr millennium centiseconds_to_millennia(const centisecond cs) noexcept { return millennium(cs.value() / 3153600000000); }
    static constexpr millennium centiseconds_to_millennia(const centisecond cs, const year starting_year) noexcept { return days_to_millennia(centiseconds_to_days(cs), starting_year); }

public:
    // decisecond conversions
    static constexpr nanosecond deciseconds_to_nanoseconds(const decisecond ds) noexcept { 
        return ds.value() > std::numeric_limits<std::uint64_t>::max() / 100000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(ds.value() * 100000000); 
    }
    
    static constexpr microsecond deciseconds_to_microseconds(const decisecond ds) noexcept { 
        return ds.value() > std::numeric_limits<std::uint64_t>::max() / 100000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(ds.value() * 100000); 
    }
    
    static constexpr millisecond deciseconds_to_milliseconds(const decisecond ds) noexcept { 
        return ds.value() > std::numeric_limits<std::uint64_t>::max() / 100 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(ds.value() * 100); 
    }
    
    static constexpr centisecond deciseconds_to_centiseconds(const decisecond ds) noexcept { 
        return ds.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(ds.value() * 10); 
    }

    static constexpr second deciseconds_to_seconds(const decisecond ds) noexcept { return second(ds.value() / 10); }
    static constexpr minute deciseconds_to_minutes(const decisecond ds) noexcept { return minute(ds.value() / 600); }
    static constexpr hour deciseconds_to_hours(const decisecond ds) noexcept { return hour(ds.value() / 36000); }
    static constexpr day deciseconds_to_days(const decisecond ds) noexcept { return day(ds.value() / 864000); }
    static constexpr week deciseconds_to_weeks(const decisecond ds) noexcept { return week(ds.value() / 6048000); }
    static constexpr year deciseconds_to_years(const decisecond ds) noexcept { return year(ds.value() / 315360000); }
    static constexpr year deciseconds_to_years(const decisecond ds, const year starting_year) noexcept { return days_to_years(deciseconds_to_days(ds), starting_year); }
    static constexpr decade deciseconds_to_decades(const decisecond ds) noexcept { return decade(ds.value() / 3153600000); }
    static constexpr decade deciseconds_to_decades(const decisecond ds, const year starting_year) noexcept { return days_to_decades(deciseconds_to_days(ds), starting_year); }
    static constexpr century deciseconds_to_centuries(const decisecond ds) noexcept { return century(ds.value() / 31536000000); }
    static constexpr century deciseconds_to_centuries(const decisecond ds, const year starting_year) noexcept { return days_to_centuries(deciseconds_to_days(ds), starting_year); }
    static constexpr millennium deciseconds_to_millennia(const decisecond ds) noexcept { return millennium(ds.value() / 315360000000); }
    static constexpr millennium deciseconds_to_millennia(const decisecond ds, const year starting_year) noexcept { return days_to_millennia(deciseconds_to_days(ds), starting_year); }

public:
    // second conversions
    static constexpr nanosecond seconds_to_nanoseconds(const second s) noexcept { 
        return s.value() > std::numeric_limits<std::uint64_t>::max() / 1000000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(s.value() * 1000000000); 
    }
    
    static constexpr microsecond seconds_to_microseconds(const second s) noexcept { 
        return s.value() > std::numeric_limits<std::uint64_t>::max() / 1000000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(s.value() * 1000000); 
    }
    
    static constexpr millisecond seconds_to_milliseconds(const second s) noexcept { 
        return s.value() > std::numeric_limits<std::uint64_t>::max() / 1000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(s.value() * 1000); 
    }
    
    static constexpr centisecond seconds_to_centiseconds(const second s) noexcept { 
        return s.value() > std::numeric_limits<std::uint64_t>::max() / 100 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(s.value() * 100); 
    }

    static constexpr decisecond seconds_to_deciseconds(const second s) noexcept { 
        return s.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(s.value() * 10); 
    }

    static constexpr minute seconds_to_minutes(const second s) noexcept { return minute(s.value() / 60); }
    static constexpr hour seconds_to_hours(const second s) noexcept { return hour(s.value() / 3600); }
    static constexpr day seconds_to_days(const second s) noexcept { return day(s.value() / 86400); }
    static constexpr week seconds_to_weeks(const second s) noexcept { return week(s.value() / 604800); }
    static constexpr year seconds_to_years(const second s) noexcept { return year(s.value() / 31536000); }
    static constexpr year seconds_to_years(const second s, const year starting_year) noexcept { return days_to_years(seconds_to_days(s), starting_year); }
    static constexpr decade seconds_to_decades(const second s) noexcept { return decade(s.value() / 315360000); }
    static constexpr decade seconds_to_decades(const second s, const year starting_year) noexcept { return days_to_decades(seconds_to_days(s), starting_year); }
    static constexpr century seconds_to_centuries(const second s) noexcept { return century(s.value() / 3153600000); }
    static constexpr century seconds_to_centuries(const second s, const year starting_year) noexcept { return days_to_centuries(seconds_to_days(s), starting_year); }
    static constexpr millennium seconds_to_millennia(const second s) noexcept { return millennium(s.value() / 31536000000); }
    static constexpr millennium seconds_to_millennia(const second s, const year starting_year) noexcept { return days_to_millennia(seconds_to_days(s), starting_year); }

public:
    // Minute conversion
    static constexpr nanosecond minutes_to_nanoseconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 60000000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(m.value() * 60000000000); 
    }
    
    static constexpr microsecond minutes_to_microseconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 60000000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(m.value() * 60000000); 
    }
    
    static constexpr millisecond minutes_to_milliseconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 60000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(m.value() * 60000); 
    }
    
    static constexpr centisecond minutes_to_centiseconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 6000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(m.value() * 6000); 
    }

    static constexpr decisecond minutes_to_deciseconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 600 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(m.value() * 600); 
    }

    static constexpr second minutes_to_seconds(const minute m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 60 ? second(std::numeric_limits<std::uint64_t>::max()) : second(m.value() * 60); 
    }

    static constexpr hour minutes_to_hours(const minute m) noexcept { return hour(m.value() / 60); }
    static constexpr day minutes_to_days(const minute m) noexcept { return day(m.value() / 1440); }
    static constexpr week minutes_to_weeks(const minute m) noexcept { return week(m.value() / 10080); }
    static constexpr year minutes_to_years(const minute m) noexcept { return year(m.value() / 525600); }
    static constexpr decade minutes_to_decades(const minute m) noexcept { return decade(m.value() / 5256000); }
    static constexpr century minutes_to_centuries(const minute m) noexcept { return century(m.value() / 52560000); }
    static constexpr millennium minutes_to_millennia(const minute m) noexcept { return millennium(m.value() / 525600000); }
    static constexpr year minutes_to_years(const minute m, const year starting_year) noexcept { return days_to_years(minutes_to_days(m), starting_year); }
    static constexpr decade minutes_to_decades(const minute m, const year starting_year) noexcept { return days_to_decades(minutes_to_days(m), starting_year); }
    static constexpr century minutes_to_centuries(const minute m, const year starting_year) noexcept { return days_to_centuries(minutes_to_days(m), starting_year); }
    static constexpr millennium minutes_to_millennia(const minute m, const year starting_year) noexcept { return days_to_millennia(minutes_to_days(m), starting_year); }

public:
    // Hour conversion
    static constexpr nanosecond hours_to_nanoseconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 3600000000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(h.value() * 3600000000000); 
    }
    
    static constexpr microsecond hours_to_microseconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 3600000000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(h.value() * 3600000000); 
    }
    
    static constexpr millisecond hours_to_milliseconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 3600000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(h.value() * 3600000); 
    }
    
    static constexpr centisecond hours_to_centiseconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 360000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(h.value() * 360000); 
    }

    static constexpr decisecond hours_to_deciseconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 36000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(h.value() * 36000); 
    }

    static constexpr second hours_to_seconds(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 3600 ? second(std::numeric_limits<std::uint64_t>::max()) : second(h.value() * 3600); 
    }

    static constexpr minute hours_to_minutes(const hour h) noexcept { 
        return h.value() > std::numeric_limits<std::uint64_t>::max() / 60 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(h.value() * 60); 
    }

    static constexpr day hours_to_days(const hour h) noexcept { return day(h.value() / 24); }
    static constexpr week hours_to_weeks(const hour h) noexcept { return week((h.value() / 24) / 7); }
    static constexpr year hours_to_years(const hour h) noexcept { return year((h.value() / 24) / 365); }
    static constexpr year hours_to_years(const hour h, const year starting_year) noexcept { return days_to_years(hours_to_days(h), starting_year); }
    static constexpr decade hours_to_decades(const hour h) noexcept { return decade((h.value() / 24) / 3650); }
    static constexpr decade hours_to_decades(const hour h, const year starting_year) noexcept { return days_to_decades(hours_to_days(h), starting_year); }
    static constexpr century hours_to_centuries(const hour h) noexcept { return century((h.value() / 24) / 36500); }
    static constexpr century hours_to_centuries(const hour h, const year starting_year) noexcept { return days_to_centuries(hours_to_days(h), starting_year); }
    static constexpr millennium hours_to_millennia(const hour h) noexcept { return millennium((h.value() / 24) / 365000); }
    static constexpr millennium hours_to_millennia(const hour h, const year starting_year) noexcept { return days_to_millennia(hours_to_days(h), starting_year); }

public:
    // Day conversions
    static constexpr nanosecond days_to_nanoseconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 86400000000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(d.value() * 86400000000000); 
    }
    
    static constexpr microsecond days_to_microseconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 86400000000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(d.value() * 86400000000); 
    }
    
    static constexpr millisecond days_to_milliseconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 86400000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(d.value() * 86400000); 
    }
    
    static constexpr centisecond days_to_centiseconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 8640000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(d.value() * 8640000); 
    }

    static constexpr decisecond days_to_deciseconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 864000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(d.value() * 864000); 
    }

    static constexpr second days_to_seconds(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 86400 ? second(std::numeric_limits<std::uint64_t>::max()) : second(d.value() * 86400); 
    }

    static constexpr minute days_to_minutes(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 1440 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(d.value() * 1440); 
    }

    static constexpr hour days_to_hours(const day d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 24 ? hour(std::numeric_limits<std::uint64_t>::max()) : hour(d.value() * 24); 
    }
    
    static constexpr week day_to_week(const day d) noexcept { return week(d.value() / 7); }
    static constexpr year days_to_years(const day d) noexcept { return year(d.value() / 365); }

    static constexpr year days_to_years(const day d, year starting_year) noexcept {
        std::uint64_t days_remaining = d.value();
        std::uint64_t years_count = 0;
        
        while (days_remaining >= Calendar::days_in_year(starting_year)) {
            days_remaining -= Calendar::days_in_year(starting_year);
            starting_year = year(starting_year.value() + 1);
            years_count++;
        }
        
        return year(years_count);
    }

    static constexpr decade days_to_decades(const day d) noexcept { return decade(d.value() / 3650); }
    static constexpr decade days_to_decades(const day d, const year starting_year) noexcept { return decade(days_to_years(d, starting_year).value() / 10); }
    static constexpr century days_to_centuries(const day d) noexcept { return century(d.value() / 36500); }
    static constexpr century days_to_centuries(const day d, const year starting_year) noexcept { return century(days_to_years(d, starting_year).value() / 100); }
    static constexpr millennium days_to_millennia(const day d) noexcept { return millennium(d.value() / 365000); }
    static constexpr millennium days_to_millennia(const day d, const year starting_year) noexcept { return millennium(days_to_years(d, starting_year).value() / 1000); }

public:
    // Week conversions
    static constexpr nanosecond weeks_to_nanoseconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 604800000000000 ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(w.value() * 604800000000000); 
    }
    
    static constexpr microsecond weeks_to_microseconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 604800000000 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(w.value() * 604800000000); 
    }
    
    static constexpr millisecond weeks_to_milliseconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 604800000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(w.value() * 604800000); 
    }
    
    static constexpr centisecond weeks_to_centiseconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 60480000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(w.value() * 60480000); 
    }

    static constexpr decisecond weeks_to_deciseconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 6048000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(w.value() * 6048000); 
    }

    static constexpr second weeks_to_seconds(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 604800 ? second(std::numeric_limits<std::uint64_t>::max()) : second(w.value() * 604800); 
    }

    static constexpr minute weeks_to_minutes(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 10080 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(w.value() * 10080); 
    }

    static constexpr hour weeks_to_hours(const week w) noexcept { 
        if (w.value() > std::numeric_limits<std::uint64_t>::max() / 7) { return hour(std::numeric_limits<std::uint64_t>::max()); }
        std::uint64_t days_value = w.value() * 7;
        return days_value > std::numeric_limits<std::uint64_t>::max() / 24 ? hour(std::numeric_limits<std::uint64_t>::max()) : hour(days_value * 24);
    }
    
    static constexpr day weeks_to_days(const week w) noexcept { 
        return w.value() > std::numeric_limits<std::uint64_t>::max() / 7 ? day(std::numeric_limits<std::uint64_t>::max()) : day(w.value() * 7); 
    }

    static constexpr year weeks_to_years(const week w) noexcept { return year(w.value() / 52); }
    static constexpr year weeks_to_years(const week w, const year starting_year) noexcept { return days_to_years(weeks_to_days(w), starting_year); }
    static constexpr decade weeks_to_decades(const week w) noexcept { return decade(w.value() / 520); }
    static constexpr decade weeks_to_decades(const week w, const year starting_year) noexcept { return days_to_decades(weeks_to_days(w), starting_year); }
    static constexpr century weeks_to_centuries(const week w) noexcept { return century(w.value() / 5200); }
    static constexpr century weeks_to_centuries(const week w, const year starting_year) noexcept { return days_to_centuries(weeks_to_days(w), starting_year); }
    static constexpr millennium weeks_to_millennia(const week w) noexcept { return millennium(w.value() / 52000); }
    static constexpr millennium weeks_to_millennia(const week w, const year starting_year) noexcept { return days_to_millennia(weeks_to_days(w), starting_year); }

public:
    // year conversions
    static constexpr nanosecond years_to_nanoseconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 31622400000000000ULL : 31536000000000000ULL;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(y.value() * mult); 
    }
    
    static constexpr microsecond years_to_microseconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 31622400000000ULL : 31536000000000ULL;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(y.value() * mult); 
    }
    
    static constexpr millisecond years_to_milliseconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 31622400000 : 31536000000;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(y.value() * mult); 
    }
    
    static constexpr centisecond years_to_centiseconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 3162240000 : 3153600000;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(y.value() * mult); 
    }

    static constexpr decisecond years_to_deciseconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 316224000 : 315360000;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(y.value() * mult); 
    }

    static constexpr second years_to_seconds(const year y) noexcept { 
        const std::uint64_t mult = Calendar::is_leap_year(y) ? 31622400 : 31536000;
        return y.value() > std::numeric_limits<std::uint64_t>::max() / mult ? second(std::numeric_limits<std::uint64_t>::max()) : second(y.value() * mult); 
    }

    static constexpr minute years_to_minutes(const year y) noexcept { return days_to_minutes(years_to_days(y)); }
    static constexpr hour years_to_hours(const year y) noexcept { return days_to_hours(years_to_days(y)); }

    static constexpr day years_to_days(const year y) noexcept { 
        return y.value() > std::numeric_limits<std::uint64_t>::max() / 365 ? day(std::numeric_limits<std::uint64_t>::max()) : day(y.value() * 365); 
    }

    static constexpr week years_to_weeks(const year y) noexcept { 
        return y.value() > std::numeric_limits<std::uint64_t>::max() / 52 ? week(std::numeric_limits<std::uint64_t>::max()) : week(y.value() * 52); 
    }

    static constexpr decade years_to_decades(const year y) noexcept { return decade(y.value() / 10); }
    static constexpr century years_to_centuries(const year y) noexcept { return century(y.value() / 100); }
    static constexpr millennium years_to_millennia(const year y) noexcept { return millennium(y.value() / 1000); }
    
public:
    // Decades conversions
    static constexpr nanosecond decades_to_nanoseconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 315360000000000000ULL ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(d.value() * 315360000000000000ULL); 
    }
    
    static constexpr nanosecond decades_to_nanoseconds(const decade d, const year starting_year) noexcept { return days_to_nanoseconds(decades_to_days(d, starting_year)); }
    
    static constexpr microsecond decades_to_microseconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 315360000000000ULL ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(d.value() * 315360000000000ULL); 
    }
    
    static constexpr microsecond decades_to_microseconds(const decade d, const year starting_year) noexcept { return days_to_microseconds(decades_to_days(d, starting_year)); }
    
    static constexpr millisecond decades_to_milliseconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 315360000000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(d.value() * 315360000000); 
    }
    
    static constexpr millisecond decades_to_milliseconds(const decade d, const year starting_year) noexcept { return days_to_milliseconds(decades_to_days(d, starting_year)); }
    
    static constexpr centisecond decades_to_centiseconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 31536000000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(d.value() * 31536000000); 
    }

    static constexpr centisecond decades_to_centiseconds(const decade d, const year starting_year) noexcept { return days_to_centiseconds(decades_to_days(d, starting_year)); }

    static constexpr decisecond decades_to_deciseconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(d.value() * 3153600000); 
    }
    
    static constexpr decisecond decades_to_deciseconds(const decade d, const year starting_year) noexcept { return days_to_deciseconds(decades_to_days(d, starting_year)); }
    
    static constexpr second decades_to_seconds(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 315360000 ? second(std::numeric_limits<std::uint64_t>::max()) : second(d.value() * 315360000); 
    }

    static constexpr second decades_to_seconds(const decade d, const year starting_year) noexcept { return days_to_seconds(decades_to_days(d, starting_year)); }

    static constexpr minute decades_to_minutes(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 5256000 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(d.value() * 5256000); 
    }

    static constexpr minute decades_to_minutes(const decade d, const year starting_year) noexcept { return days_to_minutes(decades_to_days(d, starting_year)); }
    static constexpr hour decades_to_hours(const decade d) noexcept { return days_to_hours(decades_to_days(d)); }
    static constexpr hour decades_to_hours(const decade d, const year start_year) noexcept { return days_to_hours(decades_to_days(d, start_year)); }

    static constexpr day decades_to_days(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 3650 ? day(std::numeric_limits<std::uint64_t>::max()) : day(d.value() * 3650); 
    }

    static constexpr day decades_to_days(const decade d, const year start_year) noexcept {
        std::uint64_t days = 0;
        const std::uint64_t total_years = d.value() * 10;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            const year current_year = start_year + year(i);
            days += Calendar::days_in_year(current_year);
        }
        
        return day(days);
    }

    static constexpr week decades_to_weeks(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 520 ? week(std::numeric_limits<std::uint64_t>::max()) : week(d.value() * 520); 
    }

    static constexpr week decades_to_weeks(const decade d, const year start_year) noexcept { return day_to_week(decades_to_days(d, start_year)); }

    static constexpr year decades_to_years(const decade d) noexcept { 
        return d.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? year(std::numeric_limits<std::uint64_t>::max()) : year(d.value() * 10); 
    }

    static constexpr century decades_to_centuries(const decade d) noexcept { return century(d.value() / 10); }
    static constexpr millennium decades_to_millennia(const decade d) noexcept { return millennium(d.value() / 100); }

public:
    // Century conversions
    static constexpr nanosecond centuries_to_nanoseconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000000000000ULL ? nanosecond(std::numeric_limits<std::uint64_t>::max()) : nanosecond(c.value() * 3153600000000000000ULL); 
    }
    
    static constexpr nanosecond centuries_to_nanoseconds(const century c, const year starting_year) noexcept { return days_to_nanoseconds(centuries_to_days(c, starting_year)); }
    
    static constexpr microsecond centuries_to_microseconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000000000ULL ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(c.value() * 3153600000000000ULL); 
    }
    
    static constexpr microsecond centuries_to_microseconds(const century c, const year starting_year) noexcept { return days_to_microseconds(centuries_to_days(c, starting_year)); }
    
    static constexpr millisecond centuries_to_milliseconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(c.value() * 3153600000000); 
    }
    
    static constexpr millisecond centuries_to_milliseconds(const century c, const year starting_year) noexcept { return days_to_milliseconds(centuries_to_days(c, starting_year)); }
    
    static constexpr centisecond centuries_to_centiseconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 315360000000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(c.value() * 315360000000); 
    }
    
    static constexpr centisecond centuries_to_centiseconds(const century c, const year starting_year) noexcept { return days_to_centiseconds(centuries_to_days(c, starting_year)); }
    
    static constexpr decisecond centuries_to_deciseconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 31536000000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(c.value() * 31536000000); 
    }
    
    static constexpr decisecond centuries_to_deciseconds(const century c, const year starting_year) noexcept { return days_to_deciseconds(centuries_to_days(c, starting_year)); }
    
    static constexpr second centuries_to_seconds(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000 ? second(std::numeric_limits<std::uint64_t>::max()) : second(c.value() * 3153600000); 
    }

    static constexpr second centuries_to_seconds(const century c, const year starting_year) noexcept { return days_to_seconds(centuries_to_days(c, starting_year)); }

    static constexpr minute centuries_to_minutes(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 52560000 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(c.value() * 52560000); 
    }

    static constexpr minute centuries_to_minutes(const century c, const year starting_year) noexcept { return days_to_minutes(centuries_to_days(c, starting_year)); }
    static constexpr hour centuries_to_hours(const century c) noexcept { return days_to_hours(centuries_to_days(c)); }
    static constexpr hour centuries_to_hours(const century c, const year start_year) noexcept { return days_to_hours(centuries_to_days(c, start_year)); }

    static constexpr day centuries_to_days(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 36500 ? day(std::numeric_limits<std::uint64_t>::max()) : day(c.value() * 36500); 
    }

    static constexpr day centuries_to_days(const century c, const year start_year) noexcept {
        std::uint64_t days = 0;
        const std::uint64_t total_years = c.value() * 100;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            const year current_year = start_year + year(i);
            days += Calendar::days_in_year(current_year);
        }
        
        return day(days);
    }

    static constexpr week centuries_to_weeks(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 5200 ? week(std::numeric_limits<std::uint64_t>::max()) : week(c.value() * 5200); 
    }

    static constexpr week centuries_to_weeks(const century c, const year start_year) noexcept { return day_to_week(centuries_to_days(c, start_year)); }

    static constexpr year centuries_to_years(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 100 ? year(std::numeric_limits<std::uint64_t>::max()) : year(c.value() * 100); 
    }

    static constexpr decade centuries_to_decades(const century c) noexcept { 
        return c.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? decade(std::numeric_limits<std::uint64_t>::max()) : decade(c.value() * 10); 
    }

    static constexpr millennium centuries_to_millennia(const century c) noexcept { return millennium(c.value() / 10); }

public:
    // millennium conversions
    static constexpr nanosecond millennia_to_nanoseconds(const millennium m) noexcept { 
        // This would always overflow a uint64_t, so we just return max
        return nanosecond(std::numeric_limits<std::uint64_t>::max());
    }
    
    static constexpr nanosecond millennia_to_nanoseconds(const millennium m, const year starting_year) noexcept { return days_to_nanoseconds(millennia_to_days(m, starting_year)); }
    
    static constexpr microsecond millennia_to_microseconds(const millennium m) noexcept { 
        // This would almost always overflow a uint64_t for reasonable values
        return m.value() > 0 ? microsecond(std::numeric_limits<std::uint64_t>::max()) : microsecond(0);
    }
    
    static constexpr microsecond millennia_to_microseconds(const millennium m, const year starting_year) noexcept { return days_to_microseconds(millennia_to_days(m, starting_year)); }
    
    static constexpr millisecond millennia_to_milliseconds(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 31536000000000 ? millisecond(std::numeric_limits<std::uint64_t>::max()) : millisecond(m.value() * 31536000000000); 
    }
    
    static constexpr millisecond millennia_to_milliseconds(const millennium m, const year starting_year) noexcept { return days_to_milliseconds(millennia_to_days(m, starting_year)); }
    
    static constexpr centisecond millennia_to_centiseconds(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 3153600000000 ? centisecond(std::numeric_limits<std::uint64_t>::max()) : centisecond(m.value() * 3153600000000); 
    }
    
    static constexpr centisecond millennia_to_centiseconds(const millennium m, const year starting_year) noexcept { return days_to_centiseconds(millennia_to_days(m, starting_year)); }
    
    static constexpr decisecond millennia_to_deciseconds(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 315360000000 ? decisecond(std::numeric_limits<std::uint64_t>::max()) : decisecond(m.value() * 315360000000); 
    }
    
    static constexpr decisecond millennia_to_deciseconds(const millennium m, const year starting_year) noexcept { return days_to_deciseconds(millennia_to_days(m, starting_year)); }
    
    static constexpr second millennia_to_seconds(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 31536000000 ? second(std::numeric_limits<std::uint64_t>::max()) : second(m.value() * 31536000000); 
    }

    static constexpr second millennia_to_seconds(const millennium m, const year starting_year) noexcept { return days_to_seconds(millennia_to_days(m, starting_year)); }

    static constexpr minute millennia_to_minutes(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 525600000 ? minute(std::numeric_limits<std::uint64_t>::max()) : minute(m.value() * 525600000); 
    }

    static constexpr minute millennia_to_minutes(const millennium m, const year starting_year) noexcept { return days_to_minutes(millennia_to_days(m, starting_year)); }
    static constexpr hour millennia_to_hours(const millennium m) noexcept { return days_to_hours(millennia_to_days(m)); }
    static constexpr hour millennia_to_hours(const millennium m, const year start_year) noexcept { return days_to_hours(millennia_to_days(m, start_year)); }

    static constexpr day millennia_to_days(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 365000 ? day(std::numeric_limits<std::uint64_t>::max()) : day(m.value() * 365000); 
    }

    static constexpr day millennia_to_days(const millennium m, const year start_year) noexcept {
        std::uint64_t days = 0;
        const std::uint64_t total_years = m.value() * 1000;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            const year current_year = start_year + year(i);
            days += Calendar::days_in_year(current_year);
        }
        
        return day(days);
    }

    static constexpr week millennia_to_weeks(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 52000 ? week(std::numeric_limits<std::uint64_t>::max()) : week(m.value() * 52000); 
    }

    static constexpr week millennia_to_weeks(const millennium m, const year start_year) noexcept { return day_to_week(millennia_to_days(m, start_year)); }

    static constexpr year millennia_to_years(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 1000 ? year(std::numeric_limits<std::uint64_t>::max()) : year(m.value() * 1000); 
    }

    static constexpr decade millennia_to_decades(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 100 ? decade(std::numeric_limits<std::uint64_t>::max()) : decade(m.value() * 100); 
    }

    static constexpr century millennia_to_centuries(const millennium m) noexcept { 
        return m.value() > std::numeric_limits<std::uint64_t>::max() / 10 ? century(std::numeric_limits<std::uint64_t>::max()) : century(m.value() * 10); 
    }
};

class exact_conversions {
public:
    // nanosecond conversions
    static constexpr double nanoseconds_to_microseconds(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 1000.0; }
    static constexpr double nanoseconds_to_milliseconds(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 1000000.0; }
    static constexpr double nanoseconds_to_centiseconds(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 10000000.0; }
    static constexpr double nanoseconds_to_deciseconds(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 100000000.0; }
    static constexpr double nanoseconds_to_seconds(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 1000000000.0; }
    static constexpr double nanoseconds_to_minutes(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 60000000000.0; }
    static constexpr double nanoseconds_to_hours(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 3600000000000.0; }
    static constexpr double nanoseconds_to_days(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 86400000000000.0; }
    static constexpr double nanoseconds_to_weeks(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 604800000000000.0; }
    static constexpr double nanoseconds_to_years(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 31536000000000000.0; }
    
    static constexpr double nanoseconds_to_years(const nanosecond ns, const year starting_year) noexcept {
        double days = static_cast<double>(ns.value()) / 86400000000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double nanoseconds_to_decades(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 315360000000000000.0; }
    
    static constexpr double nanoseconds_to_decades(const nanosecond ns, const year starting_year) noexcept {
        double days = static_cast<double>(ns.value()) / 86400000000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double nanoseconds_to_centuries(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 3153600000000000000.0; }
    
    static constexpr double nanoseconds_to_centuries(const nanosecond ns, const year starting_year) noexcept {
        double days = static_cast<double>(ns.value()) / 86400000000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double nanoseconds_to_millennia(const nanosecond ns) noexcept { return static_cast<double>(ns.value()) / 31536000000000000000.0; }
    
    static constexpr double nanoseconds_to_millennia(const nanosecond ns, const year starting_year) noexcept {
        double days = static_cast<double>(ns.value()) / 86400000000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // microsecond conversions
    static constexpr double microseconds_to_nanoseconds(const microsecond us) noexcept { return static_cast<double>(us.value()) * 1000.0; }
    static constexpr double microseconds_to_milliseconds(const microsecond us) noexcept { return static_cast<double>(us.value()) / 1000.0; }
    static constexpr double microseconds_to_centiseconds(const microsecond us) noexcept { return static_cast<double>(us.value()) / 10000.0; }
    static constexpr double microseconds_to_deciseconds(const microsecond us) noexcept { return static_cast<double>(us.value()) / 100000.0; }
    static constexpr double microseconds_to_seconds(const microsecond us) noexcept { return static_cast<double>(us.value()) / 1000000.0; }
    static constexpr double microseconds_to_minutes(const microsecond us) noexcept { return static_cast<double>(us.value()) / 60000000.0; }
    static constexpr double microseconds_to_hours(const microsecond us) noexcept { return static_cast<double>(us.value()) / 3600000000.0; }
    static constexpr double microseconds_to_days(const microsecond us) noexcept { return static_cast<double>(us.value()) / 86400000000.0; }
    static constexpr double microseconds_to_weeks(const microsecond us) noexcept { return static_cast<double>(us.value()) / 604800000000.0; }
    static constexpr double microseconds_to_years(const microsecond us) noexcept { return static_cast<double>(us.value()) / 31536000000000.0; }
    
    static constexpr double microseconds_to_years(const microsecond us, const year starting_year) noexcept {
        double days = static_cast<double>(us.value()) / 86400000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double microseconds_to_decades(const microsecond us) noexcept { return static_cast<double>(us.value()) / 315360000000000.0; }
    
    static constexpr double microseconds_to_decades(const microsecond us, const year starting_year) noexcept {
        double days = static_cast<double>(us.value()) / 86400000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double microseconds_to_centuries(const microsecond us) noexcept { return static_cast<double>(us.value()) / 3153600000000000.0; }
    
    static constexpr double microseconds_to_centuries(const microsecond us, const year starting_year) noexcept {
        double days = static_cast<double>(us.value()) / 86400000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double microseconds_to_millennia(const microsecond us) noexcept { return static_cast<double>(us.value()) / 31536000000000000.0; }
    
    static constexpr double microseconds_to_millennia(const microsecond us, const year starting_year) noexcept {
        double days = static_cast<double>(us.value()) / 86400000000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // millisecond conversions
    static constexpr double milliseconds_to_nanoseconds(const millisecond ms) noexcept { return static_cast<double>(ms.value()) * 1000000.0; }
    static constexpr double milliseconds_to_microseconds(const millisecond ms) noexcept { return static_cast<double>(ms.value()) * 1000.0; }
    static constexpr double milliseconds_to_centiseconds(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 10.0; }
    static constexpr double milliseconds_to_deciseconds(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 100.0; }
    static constexpr double milliseconds_to_seconds(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 1000.0; }
    static constexpr double milliseconds_to_minutes(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 60000.0; }
    static constexpr double milliseconds_to_hours(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 3600000.0; }
    static constexpr double milliseconds_to_days(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 86400000.0; }
    static constexpr double milliseconds_to_weeks(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 604800000.0; }
    static constexpr double milliseconds_to_years(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 31536000000.0; }
    
    static constexpr double milliseconds_to_years(const millisecond ms, const year starting_year) noexcept {
        double days = static_cast<double>(ms.value()) / 86400000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double milliseconds_to_decades(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 315360000000.0; }
    
    static constexpr double milliseconds_to_decades(const millisecond ms, const year starting_year) noexcept {
        double days = static_cast<double>(ms.value()) / 86400000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double milliseconds_to_centuries(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 3153600000000.0; }
    
    static constexpr double milliseconds_to_centuries(const millisecond ms, const year starting_year) noexcept {
        double days = static_cast<double>(ms.value()) / 86400000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double milliseconds_to_millennia(const millisecond ms) noexcept { return static_cast<double>(ms.value()) / 31536000000000.0; }
    
    static constexpr double milliseconds_to_millennia(const millisecond ms, const year starting_year) noexcept {
        double days = static_cast<double>(ms.value()) / 86400000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // centisecond conversions
    static constexpr double centiseconds_to_nanoseconds(const centisecond cs) noexcept { return static_cast<double>(cs.value()) * 10000000.0; }
    static constexpr double centiseconds_to_microseconds(const centisecond cs) noexcept { return static_cast<double>(cs.value()) * 10000.0; }
    static constexpr double centiseconds_to_milliseconds(const centisecond cs) noexcept { return static_cast<double>(cs.value()) * 10.0; }
    static constexpr double centiseconds_to_deciseconds(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 10.0; }
    static constexpr double centiseconds_to_seconds(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 100.0; }
    static constexpr double centiseconds_to_minutes(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 6000.0; }
    static constexpr double centiseconds_to_hours(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 360000.0; }
    static constexpr double centiseconds_to_days(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 8640000.0; }
    static constexpr double centiseconds_to_weeks(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 60480000.0; }
    static constexpr double centiseconds_to_years(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 3153600000.0; }
    
    static constexpr double centiseconds_to_years(const centisecond cs, const year starting_year) noexcept {
        double days = static_cast<double>(cs.value()) / 8640000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double centiseconds_to_decades(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 31536000000.0; }
    
    static constexpr double centiseconds_to_decades(const centisecond cs, const year starting_year) noexcept {
        double days = static_cast<double>(cs.value()) / 8640000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double centiseconds_to_centuries(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 315360000000.0; }
    
    static constexpr double centiseconds_to_centuries(const centisecond cs, const year starting_year) noexcept {
        double days = static_cast<double>(cs.value()) / 8640000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double centiseconds_to_millennia(const centisecond cs) noexcept { return static_cast<double>(cs.value()) / 3153600000000.0; }
    
    static constexpr double centiseconds_to_millennia(const centisecond cs, const year starting_year) noexcept {
        double days = static_cast<double>(cs.value()) / 8640000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // decisecond conversions
    static constexpr double deciseconds_to_nanoseconds(const decisecond ds) noexcept { return static_cast<double>(ds.value()) * 100000000.0; }
    static constexpr double deciseconds_to_microseconds(const decisecond ds) noexcept { return static_cast<double>(ds.value()) * 100000.0; }
    static constexpr double deciseconds_to_milliseconds(const decisecond ds) noexcept { return static_cast<double>(ds.value()) * 100.0; }
    static constexpr double deciseconds_to_centiseconds(const decisecond ds) noexcept { return static_cast<double>(ds.value()) * 10.0; }
    static constexpr double deciseconds_to_seconds(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 10.0; }
    static constexpr double deciseconds_to_minutes(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 600.0; }
    static constexpr double deciseconds_to_hours(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 36000.0; }
    static constexpr double deciseconds_to_days(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 864000.0; }
    static constexpr double deciseconds_to_weeks(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 6048000.0; }
    static constexpr double deciseconds_to_years(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 315360000.0; }
    
    static constexpr double deciseconds_to_years(const decisecond ds, const year starting_year) noexcept {
        double days = static_cast<double>(ds.value()) / 864000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double deciseconds_to_decades(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 3153600000.0; }
    
    static constexpr double deciseconds_to_decades(const decisecond ds, const year starting_year) noexcept {
        double days = static_cast<double>(ds.value()) / 864000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double deciseconds_to_centuries(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 31536000000.0; }
    
    static constexpr double deciseconds_to_centuries(const decisecond ds, const year starting_year) noexcept {
        double days = static_cast<double>(ds.value()) / 864000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double deciseconds_to_millennia(const decisecond ds) noexcept { return static_cast<double>(ds.value()) / 315360000000.0; }
    
    static constexpr double deciseconds_to_millennia(const decisecond ds, const year starting_year) noexcept {
        double days = static_cast<double>(ds.value()) / 864000.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // second conversions
    static constexpr double seconds_to_nanoseconds(const second s) noexcept { return static_cast<double>(s.value()) * 1000000000.0; }
    static constexpr double seconds_to_microseconds(const second s) noexcept { return static_cast<double>(s.value()) * 1000000.0; }
    static constexpr double seconds_to_milliseconds(const second s) noexcept { return static_cast<double>(s.value()) * 1000.0; }
    static constexpr double seconds_to_centiseconds(const second s) noexcept { return static_cast<double>(s.value()) * 100.0; }
    static constexpr double seconds_to_deciseconds(const second s) noexcept { return static_cast<double>(s.value()) * 10.0; }
    static constexpr double seconds_to_minutes(const second s) noexcept { return static_cast<double>(s.value()) / 60.0; }
    static constexpr double seconds_to_hours(const second s) noexcept { return static_cast<double>(s.value()) / 3600.0; }
    static constexpr double seconds_to_days(const second s) noexcept { return static_cast<double>(s.value()) / 86400.0; }
    static constexpr double seconds_to_weeks(const second s) noexcept { return static_cast<double>(s.value()) / 604800.0; }
    static constexpr double seconds_to_years(const second s) noexcept { return static_cast<double>(s.value()) / 31536000.0; }
    
    static constexpr double seconds_to_years(const second s, const year starting_year) noexcept {
        double days = static_cast<double>(s.value()) / 86400.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double seconds_to_decades(const second s) noexcept { return static_cast<double>(s.value()) / 315360000.0; }
    
    static constexpr double seconds_to_decades(const second s, const year starting_year) noexcept {
        double days = static_cast<double>(s.value()) / 86400.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double seconds_to_centuries(const second s) noexcept { return static_cast<double>(s.value()) / 3153600000.0; }
    
    static constexpr double seconds_to_centuries(const second s, const year starting_year) noexcept {
        double days = static_cast<double>(s.value()) / 86400.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double seconds_to_millennia(const second s) noexcept { return static_cast<double>(s.value()) / 31536000000.0; }
    
    static constexpr double seconds_to_millennia(const second s, const year starting_year) noexcept {
        double days = static_cast<double>(s.value()) / 86400.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // Minute conversion
    static constexpr double minutes_to_nanoseconds(const minute m) noexcept { return static_cast<double>(m.value()) * 60000000000.0; }
    static constexpr double minutes_to_microseconds(const minute m) noexcept { return static_cast<double>(m.value()) * 60000000.0; }
    static constexpr double minutes_to_milliseconds(const minute m) noexcept { return static_cast<double>(m.value()) * 60000.0; }
    static constexpr double minutes_to_centiseconds(const minute m) noexcept { return static_cast<double>(m.value()) * 6000.0; }
    static constexpr double minutes_to_deciseconds(const minute m) noexcept { return static_cast<double>(m.value()) * 600.0; }
    static constexpr double minutes_to_seconds(const minute m) noexcept { return static_cast<double>(m.value()) * 60.0; }
    static constexpr double minutes_to_hours(const minute m) noexcept { return static_cast<double>(m.value()) / 60.0; }
    static constexpr double minutes_to_days(const minute m) noexcept { return static_cast<double>(m.value()) / 1440.0; }
    static constexpr double minutes_to_weeks(const minute m) noexcept { return static_cast<double>(m.value()) / 10080.0; }
    static constexpr double minutes_to_years(const minute m) noexcept { return static_cast<double>(m.value()) / 525600.0; }
    
    static constexpr double minutes_to_years(const minute m, const year starting_year) noexcept {
        double days = static_cast<double>(m.value()) / 1440.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double minutes_to_decades(const minute m) noexcept { return static_cast<double>(m.value()) / 5256000.0; }
    
    static constexpr double minutes_to_decades(const minute m, const year starting_year) noexcept {
        double days = static_cast<double>(m.value()) / 1440.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double minutes_to_centuries(const minute m) noexcept { return static_cast<double>(m.value()) / 52560000.0; }
    
    static constexpr double minutes_to_centuries(const minute m, const year starting_year) noexcept {
        double days = static_cast<double>(m.value()) / 1440.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double minutes_to_millennia(const minute m) noexcept { return static_cast<double>(m.value()) / 525600000.0; }
    
    static constexpr double minutes_to_millennia(const minute m, const year starting_year) noexcept {
        double days = static_cast<double>(m.value()) / 1440.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // Hour conversion
    static constexpr double hours_to_nanoseconds(const hour h) noexcept { return static_cast<double>(h.value()) * 3600000000000.0; }
    static constexpr double hours_to_microseconds(const hour h) noexcept { return static_cast<double>(h.value()) * 3600000000.0; }
    static constexpr double hours_to_milliseconds(const hour h) noexcept { return static_cast<double>(h.value()) * 3600000.0; }
    static constexpr double hours_to_centiseconds(const hour h) noexcept { return static_cast<double>(h.value()) * 360000.0; }
    static constexpr double hours_to_deciseconds(const hour h) noexcept { return static_cast<double>(h.value()) * 36000.0; }
    static constexpr double hours_to_seconds(const hour h) noexcept { return static_cast<double>(h.value()) * 3600.0; }
    static constexpr double hours_to_minutes(const hour h) noexcept { return static_cast<double>(h.value()) * 60.0; }
    static constexpr double hours_to_days(const hour h) noexcept { return static_cast<double>(h.value()) / 24.0; }
    static constexpr double hours_to_weeks(const hour h) noexcept { return static_cast<double>(h.value()) / 168.0; } // 24 * 7 = 168
    static constexpr double hours_to_years(const hour h) noexcept { return static_cast<double>(h.value()) / 8760.0; } // 24 * 365 = 8760
    
    static constexpr double hours_to_years(const hour h, const year starting_year) noexcept {
        double days = static_cast<double>(h.value()) / 24.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double hours_to_decades(const hour h) noexcept { return static_cast<double>(h.value()) / 87600.0; } // 24 * 365 * 10 = 87600
    
    static constexpr double hours_to_decades(const hour h, const year starting_year) noexcept {
        double days = static_cast<double>(h.value()) / 24.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double hours_to_centuries(const hour h) noexcept { return static_cast<double>(h.value()) / 876000.0; } // 24 * 365 * 100 = 876000
    
    static constexpr double hours_to_centuries(const hour h, const year starting_year) noexcept {
        double days = static_cast<double>(h.value()) / 24.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double hours_to_millennia(const hour h) noexcept { return static_cast<double>(h.value()) / 8760000.0; } // 24 * 365 * 1000 = 8760000
    
    static constexpr double hours_to_millennia(const hour h, const year starting_year) noexcept {
        double days = static_cast<double>(h.value()) / 24.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // Day conversions
    static constexpr double days_to_nanoseconds(const day d) noexcept { return static_cast<double>(d.value()) * 86400000000000.0; }
    static constexpr double days_to_microseconds(const day d) noexcept { return static_cast<double>(d.value()) * 86400000000.0; }
    static constexpr double days_to_milliseconds(const day d) noexcept { return static_cast<double>(d.value()) * 86400000.0; }
    static constexpr double days_to_centiseconds(const day d) noexcept { return static_cast<double>(d.value()) * 8640000.0; }
    static constexpr double days_to_deciseconds(const day d) noexcept { return static_cast<double>(d.value()) * 864000.0; }
    static constexpr double days_to_seconds(const day d) noexcept { return static_cast<double>(d.value()) * 86400.0; }
    static constexpr double days_to_minutes(const day d) noexcept { return static_cast<double>(d.value()) * 1440.0; }
    static constexpr double days_to_hours(const day d) noexcept { return static_cast<double>(d.value()) * 24.0; }
    static constexpr double days_to_weeks(const day d) noexcept { return static_cast<double>(d.value()) / 7.0; }
    static constexpr double days_to_years(const day d) noexcept { return static_cast<double>(d.value()) / 365.2422; } // Average days in a year
    
    static constexpr double days_to_years(const day d, const year starting_year) noexcept {
        double days_remaining = static_cast<double>(d.value());
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days_remaining >= Calendar::exact_days_in_year(current_year)) {
            days_remaining -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days_remaining > 0.0) { years_count += days_remaining / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double days_to_decades(const day d) noexcept { return static_cast<double>(d.value()) / 3652.422; } // 365.2422 * 10
    
    static constexpr double days_to_decades(const day d, const year starting_year) noexcept {
        double days_remaining = static_cast<double>(d.value());
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days_remaining >= Calendar::exact_days_in_year(current_year)) {
            days_remaining -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days_remaining > 0.0) { years_count += days_remaining / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double days_to_centuries(const day d) noexcept { return static_cast<double>(d.value()) / 36524.22; } // 365.2422 * 100
    
    static constexpr double days_to_centuries(const day d, const year starting_year) noexcept {
        double days_remaining = static_cast<double>(d.value());
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days_remaining >= Calendar::exact_days_in_year(current_year)) {
            days_remaining -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days_remaining > 0.0) { years_count += days_remaining / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double days_to_millennia(const day d) noexcept { return static_cast<double>(d.value()) / 365242.2; } // 365.2422 * 1000
    
    static constexpr double days_to_millennia(const day d, const year starting_year) noexcept {
        double days_remaining = static_cast<double>(d.value());
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days_remaining >= Calendar::exact_days_in_year(current_year)) {
            days_remaining -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days_remaining > 0.0) { years_count += days_remaining / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // Week conversions
    static constexpr double weeks_to_nanoseconds(const week w) noexcept { return static_cast<double>(w.value()) * 604800000000000.0; }
    static constexpr double weeks_to_microseconds(const week w) noexcept { return static_cast<double>(w.value()) * 604800000000.0; }
    static constexpr double weeks_to_milliseconds(const week w) noexcept { return static_cast<double>(w.value()) * 604800000.0; }
    static constexpr double weeks_to_centiseconds(const week w) noexcept { return static_cast<double>(w.value()) * 60480000.0; }
    static constexpr double weeks_to_deciseconds(const week w) noexcept { return static_cast<double>(w.value()) * 6048000.0; }
    static constexpr double weeks_to_seconds(const week w) noexcept { return static_cast<double>(w.value()) * 604800.0; }
    static constexpr double weeks_to_minutes(const week w) noexcept { return static_cast<double>(w.value()) * 10080.0; }
    static constexpr double weeks_to_hours(const week w) noexcept { return static_cast<double>(w.value()) * 168.0; }
    static constexpr double weeks_to_days(const week w) noexcept { return static_cast<double>(w.value()) * 7.0; }
    static constexpr double weeks_to_years(const week w) noexcept { return static_cast<double>(w.value()) * 7.0 / 365.2422; }
    
    static constexpr double weeks_to_years(const week w, const year starting_year) noexcept {
        double days = static_cast<double>(w.value()) * 7.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count;
    }
    
    static constexpr double weeks_to_decades(const week w) noexcept { return static_cast<double>(w.value()) * 7.0 / 3652.422; }
    
    static constexpr double weeks_to_decades(const week w, const year starting_year) noexcept {
        double days = static_cast<double>(w.value()) * 7.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 10.0;
    }
    
    static constexpr double weeks_to_centuries(const week w) noexcept { return static_cast<double>(w.value()) * 7.0 / 36524.22; }
    
    static constexpr double weeks_to_centuries(const week w, const year starting_year) noexcept {
        double days = static_cast<double>(w.value()) * 7.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 100.0;
    }
    
    static constexpr double weeks_to_millennia(const week w) noexcept { return static_cast<double>(w.value()) * 7.0 / 365242.2; }
    
    static constexpr double weeks_to_millennia(const week w, const year starting_year) noexcept {
        double days = static_cast<double>(w.value()) * 7.0;
        double years_count = 0.0;
        year current_year = starting_year;
        
        while (days >= Calendar::exact_days_in_year(current_year)) {
            days -= Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
            years_count += 1.0;
        }
        
        if (days > 0.0) { years_count += days / Calendar::exact_days_in_year(current_year); }
        return years_count / 1000.0;
    }

public:
    // year conversions
    static constexpr double years_to_nanoseconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 31622400000000000.0 : 31536000000000000.0); }
    static constexpr double years_to_microseconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 31622400000000.0 : 31536000000000.0); }
    static constexpr double years_to_milliseconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 31622400000.0 : 31536000000.0); }
    static constexpr double years_to_centiseconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 3162240000.0 : 3153600000.0); }
    static constexpr double years_to_deciseconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 316224000.0 : 315360000.0); }
    static constexpr double years_to_seconds(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 31622400.0 : 31536000.0); }
    static constexpr double years_to_minutes(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 527040.0 : 525600.0); }
    static constexpr double years_to_hours(const year y) noexcept { return static_cast<double>(y.value()) * (Calendar::is_leap_year(y) ? 8784.0 : 8760.0); }
    static constexpr double years_to_days(const year y) noexcept { return static_cast<double>(y.value()) * Calendar::exact_days_in_year(y); }
    static constexpr double years_to_weeks(const year y) noexcept { return static_cast<double>(y.value()) * Calendar::exact_days_in_year(y) / 7.0; }
    static constexpr double years_to_decades(const year y) noexcept { return static_cast<double>(y.value()) / 10.0; }
    static constexpr double years_to_centuries(const year y) noexcept { return static_cast<double>(y.value()) / 100.0; }
    static constexpr double years_to_millennia(const year y) noexcept { return static_cast<double>(y.value()) / 1000.0; }
    
public:
    // Decades conversions
    static constexpr double decades_to_nanoseconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 31536000000000000.0; }
    static constexpr double decades_to_microseconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 31536000000000.0; }
    static constexpr double decades_to_milliseconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 31536000000.0; }
    static constexpr double decades_to_centiseconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 3153600000.0; }
    static constexpr double decades_to_deciseconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 315360000.0; }
    static constexpr double decades_to_seconds(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 31536000.0; }
    static constexpr double decades_to_minutes(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 525600.0; }
    static constexpr double decades_to_hours(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 8760.0; }
    static constexpr double decades_to_days(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 365.2422; }
    static constexpr double decades_to_weeks(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0 * 365.2422 / 7.0; }
    static constexpr double decades_to_years(const decade d) noexcept { return static_cast<double>(d.value()) * 10.0; }
    static constexpr double decades_to_centuries(const decade d) noexcept { return static_cast<double>(d.value()) / 10.0; }
    static constexpr double decades_to_millennia(const decade d) noexcept { return static_cast<double>(d.value()) / 100.0; }

    static constexpr double decades_to_days(const decade d, const year starting_year) noexcept {
        double days = 0.0;
        const std::uint64_t total_years = d.value() * 10;
        year current_year = starting_year;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            days += Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
        }
        
        return days;
    }

    static constexpr double decades_to_weeks(const decade d, const year starting_year) noexcept { return decades_to_days(d, starting_year) / 7.0; }
    static constexpr double decades_to_years(const decade d, const year starting_year) noexcept { return static_cast<double>(d.value()) * 10.0; }

public:
    // Century conversions
    static constexpr double centuries_to_nanoseconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 31536000000000000.0; }
    static constexpr double centuries_to_microseconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 31536000000000.0; }
    static constexpr double centuries_to_milliseconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 31536000000.0; }
    static constexpr double centuries_to_centiseconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 3153600000.0; }
    static constexpr double centuries_to_deciseconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 315360000.0; }
    static constexpr double centuries_to_seconds(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 31536000.0; }
    static constexpr double centuries_to_minutes(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 525600.0; }
    static constexpr double centuries_to_hours(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 8760.0; }
    static constexpr double centuries_to_days(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 365.2422; }
    static constexpr double centuries_to_weeks(const century c) noexcept { return static_cast<double>(c.value()) * 100.0 * 365.2422 / 7.0; }
    static constexpr double centuries_to_years(const century c) noexcept { return static_cast<double>(c.value()) * 100.0; }
    static constexpr double centuries_to_decades(const century c) noexcept { return static_cast<double>(c.value()) * 10.0; }
    static constexpr double centuries_to_millennia(const century c) noexcept { return static_cast<double>(c.value()) / 10.0; }

    static constexpr double centuries_to_days(const century c, const year starting_year) noexcept {
        double days = 0.0;
        const std::uint64_t total_years = c.value() * 100;
        year current_year = starting_year;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            days += Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
        }
        
        return days;
    }
    
    static constexpr double centuries_to_weeks(const century c, const year starting_year) noexcept { return centuries_to_days(c, starting_year) / 7.0; }
    static constexpr double centuries_to_years(const century c, const year starting_year) noexcept { return static_cast<double>(c.value()) * 100.0; }
    static constexpr double centuries_to_decades(const century c, const year starting_year) noexcept { return static_cast<double>(c.value()) * 10.0; }

public:
    // millennium conversions
    static constexpr double millennia_to_nanoseconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 31536000000000000.0; }
    static constexpr double millennia_to_microseconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 31536000000000.0; }
    static constexpr double millennia_to_milliseconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 31536000000.0; }
    static constexpr double millennia_to_centiseconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 3153600000.0; }
    static constexpr double millennia_to_deciseconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 315360000.0; }
    static constexpr double millennia_to_seconds(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 31536000.0; }
    static constexpr double millennia_to_minutes(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 525600.0; }
    static constexpr double millennia_to_hours(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 8760.0; }
    static constexpr double millennia_to_days(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 365.2422; }
    static constexpr double millennia_to_weeks(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0 * 365.2422 / 7.0; }
    static constexpr double millennia_to_years(const millennium m) noexcept { return static_cast<double>(m.value()) * 1000.0; }
    static constexpr double millennia_to_decades(const millennium m) noexcept { return static_cast<double>(m.value()) * 100.0; }
    static constexpr double millennia_to_centuries(const millennium m) noexcept { return static_cast<double>(m.value()) * 10.0; }

    static constexpr double millennia_to_days(const millennium m, const year starting_year) noexcept {
        double days = 0.0;
        const std::uint64_t total_years = m.value() * 1000;
        year current_year = starting_year;
        
        for (std::uint64_t i = 0; i < total_years; ++i) {
            days += Calendar::exact_days_in_year(current_year);
            current_year = year(current_year.value() + 1);
        }
        
        return days;
    }
    
    static constexpr double millennia_to_weeks(const millennium m, const year starting_year) noexcept { return millennia_to_days(m, starting_year) / 7.0; }
    static constexpr double millennia_to_years(const millennium m, const year starting_year) noexcept { return static_cast<double>(m.value()) * 1000.0; }
    static constexpr double millennia_to_decades(const millennium m, const year starting_year) noexcept { return static_cast<double>(m.value()) * 100.0; }
    static constexpr double millennia_to_centuries(const millennium m, const year starting_year) noexcept { return static_cast<double>(m.value()) * 10.0; }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_CHRONO_TIME_CONVERTERS_HPP