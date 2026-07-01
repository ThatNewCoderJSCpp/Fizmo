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

    std::string to_string() const {
        if (is_empty()) { return "empty"; }
        if (!lower.is_valid() || !upper.is_valid()) { return "invalid"; }
        std::stringstream ss;
        ss << std::setprecision(15);
        ss << (lower.is_closed ? "[" : "(");

        if (lower.is_negative_infinity()) {
            ss << "-\u221E";
        } else { ss << lower.value; }

        ss << ", ";

        if (upper.is_positive_infinity()) {
            ss << "\u221E";
        } else { ss << upper.value; }

        ss << (upper.is_closed ? "]" : ")");
        return ss.str();
    }

    std::string to_inequality_string(const std::string& var) const {
        if (is_empty()) { return "empty"; }
        if (!lower.is_valid() || !upper.is_valid()) { return "invalid"; }
        std::stringstream ss;
        ss << std::setprecision(15);
        if (lower.is_negative_infinity() && upper.is_positive_infinity()) { return "\u2200" + var; }

        if (lower.is_negative_infinity()) {
            ss << var << " " << (upper.is_closed ? "\u2264 " : "< ") << upper.value; 
            return ss.str();
        }

        if (upper.is_positive_infinity()) {
            ss << var << " " << (lower.is_closed ? "\u2265 " : "> ") << lower.value; 
            return ss.str();
        }

        ss << lower.value << (lower.is_closed ? " \u2264 " : " < ") << var << (upper.is_closed ? " \u2264 " : " < ") << upper.value;
        return ss.str();
    }
};

class IntervalSet {
private:
    std::vector<Interval> m_intervals;
    
    void normalize() {
        m_intervals.erase(
            std::remove_if(m_intervals.begin(), m_intervals.end(), [](const Interval& i) { return i.is_empty(); }),
            m_intervals.end()
        );
        
        if (m_intervals.empty()) return;
        
        std::sort(m_intervals.begin(), m_intervals.end(),
            [](const Interval& a, const Interval& b) {
                if (a.lower.value != b.lower.value) 
                    return a.lower.value < b.lower.value;
                return a.lower.is_closed && !b.lower.is_closed;  
            });
        
        std::vector<Interval> merged;
        merged.push_back(m_intervals[0]);
        
        for (std::size_t i = 1; i < m_intervals.size(); ++i) {
            if (merged.back().overlaps_or_touches(m_intervals[i])) {
                merged.back() = merged.back().merge(m_intervals[i]);
            } else {
                merged.push_back(m_intervals[i]);
            }
        }
        
        m_intervals = std::move(merged);
    }

public:
    IntervalSet() = default;
    explicit IntervalSet(const Interval& interval) { if (!interval.is_empty()) { m_intervals.push_back(interval); }}
    explicit IntervalSet(std::vector<Interval> intervals) : m_intervals(std::move(intervals)) { normalize(); }
    static IntervalSet all() { return IntervalSet(Interval::all()); }
    static IntervalSet empty() { return IntervalSet(); }
    static IntervalSet point(double v) { return IntervalSet(Interval::point(v)); }
    
    bool is_empty() const noexcept { return m_intervals.empty(); }

    bool is_all() const noexcept {
        return m_intervals.size() == 1 && 
               m_intervals[0].lower.is_negative_infinity() && 
               m_intervals[0].upper.is_positive_infinity();
    }
    
    const std::vector<Interval>& intervals() const noexcept { return m_intervals; }
    
    bool contains(double x) const {
        return std::any_of(m_intervals.begin(), m_intervals.end(), [x](const Interval& i) { return i.contains(x); });
    }
    
    IntervalSet union_with(const IntervalSet& other) const {
        std::vector<Interval> combined;
        combined.reserve(m_intervals.size() + other.m_intervals.size());
        combined.insert(combined.end(), m_intervals.begin(), m_intervals.end());
        combined.insert(combined.end(), other.m_intervals.begin(), other.m_intervals.end());
        return IntervalSet(std::move(combined));
    }
    
    IntervalSet intersect_with(const IntervalSet& other) const {
        std::vector<Interval> result;
        
        for (const auto& a : m_intervals) {
            for (const auto& b : other.m_intervals) {
                Interval intersection = a.intersect(b);
                if (!intersection.is_empty()) { result.push_back(intersection); }
            }
        }
        
        return IntervalSet(std::move(result));
    }
    
    IntervalSet complement() const {
        if (is_empty()) return all();
        if (is_all()) return empty();
        std::vector<Interval> result;
        
        if (!m_intervals[0].lower.is_negative_infinity()) {
            result.push_back({
                Endpoint::negative_infinity(),
                {m_intervals[0].lower.value, !m_intervals[0].lower.is_closed}
            });
        }
        
        for (std::size_t i = 0; i + 1 < m_intervals.size(); ++i) {
            result.push_back({
                {m_intervals[i].upper.value, !m_intervals[i].upper.is_closed},
                {m_intervals[i + 1].lower.value, !m_intervals[i + 1].lower.is_closed}
            });
        }
        
        if (!m_intervals.back().upper.is_positive_infinity()) {
            result.push_back({
                {m_intervals.back().upper.value, !m_intervals.back().upper.is_closed},
                Endpoint::positive_infinity()
            });
        }
        
        return IntervalSet(std::move(result));
    }
    
    IntervalSet symmetric_difference(const IntervalSet& other) const {
        return this->intersect_with(other.complement()).union_with(this->complement().intersect_with(other));
    }
    
    IntervalSet operator|(const IntervalSet& other) const { return union_with(other); }
    IntervalSet operator&(const IntervalSet& other) const { return intersect_with(other); }
    IntervalSet operator~() const { return complement(); }
    IntervalSet operator^(const IntervalSet& other) const { return symmetric_difference(other); }

    std::string to_string() const {
        std::stringstream ss;
        for (Interval i : m_intervals) { ss << i.to_string() << "\n"; }
        return ss.str();
    }

    std::string to_inequality_string(const std::string& var) const {
        std::stringstream ss;
        for (Interval i : m_intervals) { ss << i.to_inequality_string(var) << "\n"; }
        return ss.str();
    }
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

    bool contains(double x, double y) const {
        if (!x_range.contains(x) || !y_range.contains(y)) return false;
        for (const auto& c : m_constraints) if (!c(x, y)) return false;
        return true;
    }

    bool is_empty() const noexcept { return x_range.is_empty() || y_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool has_constraints() const noexcept  { return !m_constraints.empty(); }
    const std::vector<Constraint2D>& constraints() const noexcept { return m_constraints; }

    Region& add_constraint(std::function<bool(double, double)> pred, std::string label = {}) {
        m_constraints.emplace_back(std::move(pred), std::move(label));
        rebuild_legacy_constraint();
        return *this;
    }

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

    static Region disk(double cx, double cy, double r) {
        Region reg(cx - r, cx + r, cy - r, cy + r);
        reg.add_constraint(
            [cx, cy, r2 = r * r](double x, double y) {
                double dx = x - cx, dy = y - cy;
                return dx * dx + dy * dy <= r2;
            },
            "disk(r=" + std::to_string(r) + ")"
        );
        return reg;
    }

    static Region annulus(double cx, double cy, double r_inner, double r_outer) {
        Region reg(cx - r_outer, cx + r_outer, cy - r_outer, cy + r_outer);
        reg.add_constraint(
            [cx, cy, ri2 = r_inner * r_inner, ro2 = r_outer * r_outer](double x, double y) {
                double dx = x - cx, dy = y - cy;
                double d2 = dx * dx + dy * dy;
                return d2 >= ri2 && d2 <= ro2;
            },
            "annulus"
        );
        return reg;
    }

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

    void rebuild_legacy_constraint() {
        if (m_constraints.empty()) {
            constraint = nullptr;
            return;
        }
        auto cs = m_constraints;
        constraint = [cs](double x, double y) -> bool {
            for (const auto& c : cs) if (!c(x, y)) return false;
            return true;
        };
    }

    static Interval make_iv(double lo, double hi) noexcept {
        if (lo > hi) std::swap(lo, hi);
        Endpoint ep_lo = std::isinf(lo) ? Endpoint::negative_infinity() : Endpoint::closed(lo);
        Endpoint ep_hi = std::isinf(hi) ? Endpoint::positive_infinity() : Endpoint::closed(hi);
        return { ep_lo, ep_hi };
    }
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
    Region3D(Interval xr, Interval yr, Interval zr, std::function<bool(double, double, double)> c) : x_range(std::move(xr)), y_range(std::move(yr)), z_range(std::move(zr)) { if (c) add_constraint(std::move(c)); }
    Region3D(double x0, double x1, double y0, double y1, double z0, double z1) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)), z_range(make_iv(z0, z1)) {}
    Region3D(double x0, double x1, double y0, double y1, double z0, double z1, std::function<bool(double, double, double)> c) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)), z_range(make_iv(z0, z1)) { if (c) add_constraint(std::move(c)); }

public:
    bool contains(double x, double y, double z) const {
        if (!x_range.contains(x) || !y_range.contains(y) || !z_range.contains(z)) return false;
        for (const auto& c : m_constraints) if (!c(x, y, z)) return false;
        return true;
    }

    bool is_empty() const noexcept { return x_range.is_empty() || y_range.is_empty() || z_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool has_constraints() const noexcept { return !m_constraints.empty(); }
    const std::vector<Constraint3D>& constraints() const noexcept { return m_constraints; }

public:
    Region3D& add_constraint(std::function<bool(double, double, double)> pred, std::string label = {}) {
        m_constraints.emplace_back(std::move(pred), std::move(label));
        rebuild_legacy_constraint();
        return *this;
    }

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

    static Region3D ball(double cx, double cy, double cz, double r) {
        Region3D reg(cx - r, cx + r, cy - r, cy + r, cz - r, cz + r);
        reg.add_constraint(
            [cx, cy, cz, r2 = r * r](double x, double y, double z) {
                double dx = x - cx, dy = y - cy, dz = z - cz;
                return dx * dx + dy * dy + dz * dz <= r2;
            },
            "ball(r=" + std::to_string(r) + ")"
        );
        return reg;
    }

    static Region3D spherical_shell(double cx, double cy, double cz, double r_inner, double r_outer) {
        Region3D reg(cx - r_outer, cx + r_outer, cy - r_outer, cy + r_outer, cz - r_outer, cz + r_outer);
        reg.add_constraint(
            [cx, cy, cz, ri2 = r_inner * r_inner, ro2 = r_outer * r_outer](double x, double y, double z) {
                double dx = x - cx, dy = y - cy, dz = z - cz;
                double d2 = dx * dx + dy * dy + dz * dz;
                return d2 >= ri2 && d2 <= ro2;
            },
            "spherical_shell"
        );
        return reg;
    }

    static Region3D cylinder_z(double cx, double cy, double r, double z0, double z1) {
        Region3D reg(cx - r, cx + r, cy - r, cy + r, z0, z1);
        reg.add_constraint(
            [cx, cy, r2 = r * r](double x, double y, double) {
                double dx = x - cx, dy = y - cy;
                return dx * dx + dy * dy <= r2;
            },
            "cylinder_z(r=" + std::to_string(r) + ")"
        );
        return reg;
    }

    static Region3D cylinder_x(double cy, double cz, double r, double x0, double x1) {
        Region3D reg(x0, x1, cy - r, cy + r, cz - r, cz + r);
        reg.add_constraint(
            [cy, cz, r2 = r * r](double, double y, double z) {
                double dy = y - cy, dz = z - cz;
                return dy * dy + dz * dz <= r2;
            },
            "cylinder_x(r=" + std::to_string(r) + ")"
        );
        return reg;
    }

    static Region3D cylinder_y(double cx, double cz, double r, double y0, double y1) {
        Region3D reg(cx - r, cx + r, y0, y1, cz - r, cz + r);
        reg.add_constraint(
            [cx, cz, r2 = r * r](double x, double, double z) {
                double dx = x - cx, dz = z - cz;
                return dx * dx + dz * dz <= r2;
            },
            "cylinder_y(r=" + std::to_string(r) + ")"
        );
        return reg;
    }

    static Region3D from_predicate(double x0, double x1, double y0, double y1, double z0, double z1, std::function<bool(double, double, double)> pred) {
        Region3D reg(x0, x1, y0, y1, z0, z1);
        reg.add_constraint(std::move(pred));
        return reg;
    }

    static Region3D all() { return { Interval::all(), Interval::all(), Interval::all() }; }
    static Region3D first_octant(double x_max = 1e300, double y_max = 1e300, double z_max = 1e300) { return box(0.0, x_max, 0.0, y_max, 0.0, z_max); }

private:
    std::vector<Constraint3D> m_constraints;

    void rebuild_legacy_constraint() {
        if (m_constraints.empty()) {
            constraint = nullptr;
            return;
        }

        auto cs = m_constraints;
        
        constraint = [cs](double x, double y, double z) -> bool {
            for (const auto& c : cs) if (!c(x, y, z)) return false;
            return true;
        };
    }

    static Interval make_iv(double lo, double hi) noexcept {
        if (lo > hi) std::swap(lo, hi);
        Endpoint ep_lo = std::isinf(lo) ? Endpoint::negative_infinity() : Endpoint::closed(lo);
        Endpoint ep_hi = std::isinf(hi) ? Endpoint::positive_infinity() : Endpoint::closed(hi);
        return { ep_lo, ep_hi };
    }
};

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTERVALS_HPP