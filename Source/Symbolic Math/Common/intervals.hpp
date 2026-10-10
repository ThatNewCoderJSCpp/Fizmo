#ifndef FIZMO_MATH_INTERVALS_HPP
#define FIZMO_MATH_INTERVALS_HPP

#include <limits>
#include <vector>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>

namespace fizmo {
namespace math {

enum class InequalityType {
    LessThan = 0,
    GreaterThan,
    LEQ,
    GEQ
};

struct Endpoint {
    double value;
    bool is_closed; 
    constexpr Endpoint(double point, bool is_cls) noexcept : value(point), is_closed(std::isfinite(point) ? is_cls : false) {} 
    constexpr Endpoint() noexcept : value(0.0), is_closed(true) {}
    
public:
    constexpr static Endpoint negative_infinity() noexcept { return {-std::numeric_limits<double>::infinity(), false}; }
    constexpr static Endpoint positive_infinity() noexcept { return {std::numeric_limits<double>::infinity(), false}; }
    constexpr static Endpoint open(double v) noexcept { return {v, false}; }
    constexpr static Endpoint closed(double v) noexcept { return {v, true}; }
    
public:
    constexpr bool is_negative_infinity() const noexcept { return value == -std::numeric_limits<double>::infinity(); }
    constexpr bool is_positive_infinity() const noexcept { return value == std::numeric_limits<double>::infinity(); }
    constexpr bool is_infinite() const noexcept { return is_negative_infinity() || is_positive_infinity(); }
    
    constexpr bool is_valid() const noexcept {
        if (value != value) { return false; }
        return !(is_infinite() && is_closed);
    }
};

struct Interval {
    Endpoint lower;
    Endpoint upper;
    constexpr Interval(Endpoint start, Endpoint end) noexcept : lower(start), upper(end) {}
    constexpr Interval() noexcept : lower(), upper() {}
    
public:
    constexpr static Interval all() noexcept { return {Endpoint::negative_infinity(), Endpoint::positive_infinity()}; }
    constexpr static Interval empty() noexcept { return {Endpoint::open(1.0), Endpoint::open(0.0)}; }
    constexpr static Interval point(double v) noexcept { return {Endpoint::closed(v), Endpoint::closed(v)}; }
    constexpr static Interval less_than(double v) noexcept { return {Endpoint::negative_infinity(), Endpoint::open(v)}; }
    constexpr static Interval less_or_equal(double v) noexcept { return {Endpoint::negative_infinity(), Endpoint::closed(v)}; }
    constexpr static Interval greater_than(double v) noexcept { return {Endpoint::open(v), Endpoint::positive_infinity()}; }
    constexpr static Interval greater_or_equal(double v) noexcept { return {Endpoint::closed(v), Endpoint::positive_infinity()}; }
    constexpr static Interval between(double lo, double hi, bool lo_closed = true, bool hi_closed = true) noexcept { return {Endpoint{lo, lo_closed}, Endpoint{hi, hi_closed}}; }
    constexpr static Interval unit_interval() noexcept { return {Endpoint::closed(0.0), Endpoint::closed(1.0)}; }
    constexpr static Interval symmetric(double center = 0.0, double half_width = 0.5, bool lo_closed = true, bool hi_closed = true) noexcept { return {Endpoint{center - half_width, lo_closed}, Endpoint{center + half_width, hi_closed}}; }

public:
    constexpr bool is_empty() const noexcept {
        if (lower.value > upper.value) return true;
        if (lower.value == upper.value) { return !(lower.is_closed && upper.is_closed); }
        return false;
    }
    
    constexpr bool contains(double x) const noexcept {
        if (is_empty()) return false;
        bool aboveLower = (x > lower.value) || (x == lower.value && lower.is_closed);
        bool belowUpper = (x < upper.value) || (x == upper.value && upper.is_closed);
        return aboveLower && belowUpper;
    }
    
    constexpr bool overlaps_or_touches(const Interval& other) const noexcept {
        if (is_empty() || other.is_empty()) return false;
        bool thisBeforeOther = (upper.value < other.lower.value) || (upper.value == other.lower.value && !upper.is_closed && !other.lower.is_closed);
        bool otherBeforeThis = (other.upper.value < lower.value) || (other.upper.value == lower.value && !other.upper.is_closed && !lower.is_closed);
        return !thisBeforeOther && !otherBeforeThis;
    }
    
    constexpr Interval merge(const Interval& other) const noexcept {
        Endpoint newLower{0.0, false};
        Endpoint newUpper{0.0, false};
        
        if (lower.value < other.lower.value) {
            newLower = lower;
        } else if (lower.value > other.lower.value) {
            newLower = other.lower;
        } else {
            newLower = {lower.value, lower.is_closed || other.lower.is_closed};
        }
        
        if (upper.value > other.upper.value) {
            newUpper = upper;
        } else if (upper.value < other.upper.value) {
            newUpper = other.upper;
        } else {
            newUpper = {upper.value, upper.is_closed || other.upper.is_closed};
        }
        
        return {newLower, newUpper};
    }

    static constexpr Interval merge(const Interval& a, const Interval& b) noexcept { return a.merge(b); }
    
    constexpr Interval intersect(const Interval& other) const noexcept {
        if (is_empty() || other.is_empty()) return empty();
        Endpoint newLower{0.0, false};
        Endpoint newUpper{0.0, false};
        
        if (lower.value > other.lower.value) {
            newLower = lower;
        } else if (lower.value < other.lower.value) {
            newLower = other.lower;
        } else {
            newLower = {lower.value, lower.is_closed && other.lower.is_closed};
        }
        
        if (upper.value < other.upper.value) {
            newUpper = upper;
        } else if (upper.value > other.upper.value) {
            newUpper = other.upper;
        } else {
            newUpper = {upper.value, upper.is_closed && other.upper.is_closed};
        }
        
        return {newLower, newUpper};
    }

    static constexpr Interval intersect(const Interval& a, const Interval& b) noexcept { return a.intersect(b); }

    constexpr double width() const noexcept { return upper.value - lower.value; }

    std::string to_string() const;

    std::string to_inequality_string(const std::string& var) const;
};

class IntervalSet {
private:
    std::vector<Interval> m_intervals;
    
    void normalize();

public:
    IntervalSet() = default;
    explicit IntervalSet(const Interval& interval) { if (!interval.is_empty()) { m_intervals.push_back(interval); }}
    explicit IntervalSet(std::vector<Interval> intervals) : m_intervals(std::move(intervals)) { normalize(); }
    static IntervalSet all() { return IntervalSet(Interval::all()); }
    static IntervalSet empty() { return IntervalSet(); }
    static IntervalSet point(double v) { return IntervalSet(Interval::point(v)); }
    
    bool is_empty() const noexcept { return m_intervals.empty(); }

    bool is_all() const noexcept;
    
    const std::vector<Interval>& intervals() const noexcept { return m_intervals; }
    
    bool contains(double x) const;
    
    IntervalSet union_with(const IntervalSet& other) const;
    
    IntervalSet intersect_with(const IntervalSet& other) const;
    
    IntervalSet complement() const;
    
    IntervalSet symmetric_difference(const IntervalSet& other) const;
    
    IntervalSet operator|(const IntervalSet& other) const { return union_with(other); }
    IntervalSet operator&(const IntervalSet& other) const { return intersect_with(other); }
    IntervalSet operator~() const { return complement(); }
    IntervalSet operator^(const IntervalSet& other) const { return symmetric_difference(other); }

    std::string to_string() const {
        std::stringstream ss;
        for (Interval i : m_intervals) { ss << i.to_string() << "\n"; }
        return ss.str();
    }

    std::string to_inequality_string(const std::string& var) const;
};

struct Constraint2D {
    std::function<bool(double, double)> predicate;
    std::string label; // Optional human-readable label
    Constraint2D() = default;
    Constraint2D(std::function<bool(double, double)> p, std::string lbl = {}) : predicate(std::move(p)), label(std::move(lbl)) {}
    bool operator()(double x, double y) const { return predicate(x, y); }
    explicit operator bool() const { return static_cast<bool>(predicate); }
};

struct Region {
    Interval x_range;
    Interval y_range;
    std::function<bool(double, double)> constraint;
    Region() : x_range(), y_range() {}
    Region(Interval xr, Interval yr) : x_range(std::move(xr)), y_range(std::move(yr)) {}
    Region(Interval xr, Interval yr, std::function<bool(double, double)> c) : x_range(std::move(xr)), y_range(std::move(yr)) { if (c) add_constraint(std::move(c)); }
    Region(double x0, double x1, double y0, double y1) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)) {}
    Region(double x0, double x1, double y0, double y1, std::function<bool(double, double)> c) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)) { if (c) add_constraint(std::move(c)); }

    bool contains(double x, double y) const;

    bool is_empty() const noexcept { return x_range.is_empty() || y_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool has_constraints() const noexcept  { return !m_constraints.empty(); }
    const std::vector<Constraint2D>& constraints() const noexcept { return m_constraints; }

    Region& add_constraint(std::function<bool(double, double)> pred, std::string label = {});

    Region& where(std::function<bool(double, double)> pred, std::string label = {}) {
        return add_constraint(std::move(pred), std::move(label));
    }

    Region& where_all(std::initializer_list<Constraint2D> list) {
        for (auto& c : list) m_constraints.push_back(c);
        rebuild_legacy_constraint();
        return *this;
    }

    Region& clear_constraints() {
        m_constraints.clear();
        constraint = nullptr;
        return *this;
    }

    static Region rectangle(double x0, double x1, double y0, double y1) { return { x0, x1, y0, y1 }; }
    static Region square(double cx, double cy, double half) { return { cx - half, cx + half, cy - half, cy + half }; }

    static Region disk(double cx, double cy, double r);

    static Region annulus(double cx, double cy, double r_inner, double r_outer);

    static Region half_plane_x_positive(double y0, double y1, double x_min = 0.0, double x_max = 1e300) { return { x_min, x_max, y0, y1 }; }

    static Region from_predicate(double x0, double x1, double y0, double y1, std::function<bool(double, double)> pred) {
        Region reg(x0, x1, y0, y1);
        reg.add_constraint(std::move(pred));
        return reg;
    }

    static Region all() { return { Interval::all(), Interval::all() }; }
    static Region first_quadrant(double x_max = 1e300, double y_max = 1e300) { return rectangle(0.0, x_max, 0.0, y_max); }

private:
    std::vector<Constraint2D> m_constraints;

    void rebuild_legacy_constraint();

    static Interval make_iv(double lo, double hi) noexcept;
};

struct Constraint3D {
    std::function<bool(double, double, double)> predicate;
    std::string label;
    Constraint3D() = default;
    Constraint3D(std::function<bool(double, double, double)> p, std::string lbl = {}) : predicate(std::move(p)), label(std::move(lbl)) {}
    bool operator()(double x, double y, double z) const { return predicate(x, y, z); }
    explicit operator bool() const { return static_cast<bool>(predicate); }
};

struct Region3D {
    Interval x_range;
    Interval y_range;
    Interval z_range;
    std::function<bool(double, double, double)> constraint;

    Region3D() : x_range(), y_range(), z_range() {}
    Region3D(Interval xr, Interval yr, Interval zr) : x_range(std::move(xr)), y_range(std::move(yr)), z_range(std::move(zr)) {}
    Region3D(Interval xr, Interval yr, Interval zr, std::function<bool(double, double, double)> c);
    Region3D(double x0, double x1, double y0, double y1, double z0, double z1) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)), z_range(make_iv(z0, z1)) {}
    Region3D(double x0, double x1, double y0, double y1, double z0, double z1, std::function<bool(double, double, double)> c);

public:
    bool contains(double x, double y, double z) const;

    bool is_empty() const noexcept { return x_range.is_empty() || y_range.is_empty() || z_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool has_constraints() const noexcept { return !m_constraints.empty(); }
    const std::vector<Constraint3D>& constraints() const noexcept { return m_constraints; }

public:
    Region3D& add_constraint(std::function<bool(double, double, double)> pred, std::string label = {});

    Region3D& where(std::function<bool(double, double, double)> pred, std::string label = {}) {
        return add_constraint(std::move(pred), std::move(label));
    }

    Region3D& where_all(std::initializer_list<Constraint3D> list) {
        for (auto& c : list) m_constraints.push_back(c);
        rebuild_legacy_constraint();
        return *this;
    }

    Region3D& clear_constraints() {
        m_constraints.clear();
        constraint = nullptr;
        return *this;
    }

public:
    static Region3D box(double x0, double x1, double y0, double y1, double z0, double z1) { return { x0, x1, y0, y1, z0, z1 }; }

    static Region3D cube(double cx, double cy, double cz, double half) {
        return { cx - half, cx + half, cy - half, cy + half, cz - half, cz + half };
    }

    static Region3D ball(double cx, double cy, double cz, double r);

    static Region3D spherical_shell(double cx, double cy, double cz, double r_inner, double r_outer);

    static Region3D cylinder_z(double cx, double cy, double r, double z0, double z1);

    static Region3D cylinder_x(double cy, double cz, double r, double x0, double x1);

    static Region3D cylinder_y(double cx, double cz, double r, double y0, double y1);

    static Region3D from_predicate(double x0, double x1, double y0, double y1, double z0, double z1, std::function<bool(double, double, double)> pred) {
        Region3D reg(x0, x1, y0, y1, z0, z1);
        reg.add_constraint(std::move(pred));
        return reg;
    }

    static Region3D all() { return { Interval::all(), Interval::all(), Interval::all() }; }
    static Region3D first_octant(double x_max = 1e300, double y_max = 1e300, double z_max = 1e300) { return box(0.0, x_max, 0.0, y_max, 0.0, z_max); }

private:
    std::vector<Constraint3D> m_constraints;

    void rebuild_legacy_constraint();

    static Interval make_iv(double lo, double hi) noexcept;
};

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTERVALS_HPP