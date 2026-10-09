#include "fizmo_library.hpp"

namespace fizmo {
namespace math {

std::string Interval::to_string() const {
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

std::string Interval::to_inequality_string(const std::string& var) const {
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

void IntervalSet::normalize() {
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

bool IntervalSet::is_all() const noexcept {
    return m_intervals.size() == 1 && 
           m_intervals[0].lower.is_negative_infinity() && 
           m_intervals[0].upper.is_positive_infinity();
}

bool IntervalSet::contains(double x) const {
    return std::any_of(m_intervals.begin(), m_intervals.end(), [x](const Interval& i) { return i.contains(x); });
}

auto IntervalSet::union_with(const IntervalSet& other) const -> IntervalSet {
    std::vector<Interval> combined;
    combined.reserve(m_intervals.size() + other.m_intervals.size());
    combined.insert(combined.end(), m_intervals.begin(), m_intervals.end());
    combined.insert(combined.end(), other.m_intervals.begin(), other.m_intervals.end());
    return IntervalSet(std::move(combined));
}

auto IntervalSet::intersect_with(const IntervalSet& other) const -> IntervalSet {
    std::vector<Interval> result;
        
    for (const auto& a : m_intervals) {
        for (const auto& b : other.m_intervals) {
            Interval intersection = a.intersect(b);
            if (!intersection.is_empty()) { result.push_back(intersection); }
        }
    }
        
    return IntervalSet(std::move(result));
}

auto IntervalSet::complement() const -> IntervalSet {
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

auto IntervalSet::symmetric_difference(const IntervalSet& other) const -> IntervalSet {
    return this->intersect_with(other.complement()).union_with(this->complement().intersect_with(other));
}

std::string IntervalSet::to_inequality_string(const std::string& var) const {
    std::stringstream ss;
    for (Interval i : m_intervals) { ss << i.to_inequality_string(var) << "\n"; }
    return ss.str();
}

bool Region::contains(double x, double y) const {
    if (!x_range.contains(x) || !y_range.contains(y)) return false;
    for (const auto& c : m_constraints) if (!c(x, y)) return false;
    return true;
}

auto Region::add_constraint(std::function<bool(double, double)> pred, std::string label) -> Region& {
    m_constraints.emplace_back(std::move(pred), std::move(label));
    rebuild_legacy_constraint();
    return *this;
}

auto Region::disk(double cx, double cy, double r) -> Region {
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

auto Region::annulus(double cx, double cy, double r_inner, double r_outer) -> Region {
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

void Region::rebuild_legacy_constraint() {
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

auto Region::make_iv(double lo, double hi) noexcept -> Interval {
    if (lo > hi) std::swap(lo, hi);
    Endpoint ep_lo = std::isinf(lo) ? Endpoint::negative_infinity() : Endpoint::closed(lo);
    Endpoint ep_hi = std::isinf(hi) ? Endpoint::positive_infinity() : Endpoint::closed(hi);
    return { ep_lo, ep_hi };
}

Region3D::Region3D(Interval xr, Interval yr, Interval zr, std::function<bool(double, double, double)> c) : x_range(std::move(xr)), y_range(std::move(yr)), z_range(std::move(zr)) { if (c) add_constraint(std::move(c)); }

Region3D::Region3D(double x0, double x1, double y0, double y1, double z0, double z1, std::function<bool(double, double, double)> c) : x_range(make_iv(x0, x1)), y_range(make_iv(y0, y1)), z_range(make_iv(z0, z1)) { if (c) add_constraint(std::move(c)); }

bool Region3D::contains(double x, double y, double z) const {
    if (!x_range.contains(x) || !y_range.contains(y) || !z_range.contains(z)) return false;
    for (const auto& c : m_constraints) if (!c(x, y, z)) return false;
    return true;
}

auto Region3D::add_constraint(std::function<bool(double, double, double)> pred, std::string label) -> Region3D& {
    m_constraints.emplace_back(std::move(pred), std::move(label));
    rebuild_legacy_constraint();
    return *this;
}

auto Region3D::ball(double cx, double cy, double cz, double r) -> Region3D {
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

auto Region3D::spherical_shell(double cx, double cy, double cz, double r_inner, double r_outer) -> Region3D {
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

auto Region3D::cylinder_z(double cx, double cy, double r, double z0, double z1) -> Region3D {
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

auto Region3D::cylinder_x(double cy, double cz, double r, double x0, double x1) -> Region3D {
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

auto Region3D::cylinder_y(double cx, double cz, double r, double y0, double y1) -> Region3D {
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

void Region3D::rebuild_legacy_constraint() {
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

auto Region3D::make_iv(double lo, double hi) noexcept -> Interval {
    if (lo > hi) std::swap(lo, hi);
    Endpoint ep_lo = std::isinf(lo) ? Endpoint::negative_infinity() : Endpoint::closed(lo);
    Endpoint ep_hi = std::isinf(hi) ? Endpoint::positive_infinity() : Endpoint::closed(hi);
    return { ep_lo, ep_hi };
}

} // namespace math
} // namespace fizmo
