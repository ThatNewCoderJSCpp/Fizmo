#ifndef FIZMO_REGION_HPP
#define FIZMO_REGION_HPP

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include "intervals.hpp"

namespace fizmo {

template <typename T>
struct Constraint2D {
    using predicate_type = std::function<bool(const T&, const T&)>;

    predicate_type predicate;
    std::string    label;

    Constraint2D() = default;
    Constraint2D(predicate_type p, std::string lbl = {}) : predicate(std::move(p)), label(std::move(lbl)) {}

    bool operator()(const T& x, const T& y) const { return predicate(x, y); }
    explicit operator bool() const { return static_cast<bool>(predicate); }
};

template <typename T>
struct Constraint3D {
    using predicate_type = std::function<bool(const T&, const T&, const T&)>;

    predicate_type predicate;
    std::string    label;

    Constraint3D() = default;
    Constraint3D(predicate_type p, std::string lbl = {}) : predicate(std::move(p)), label(std::move(lbl)) {}

    bool operator()(const T& x, const T& y, const T& z) const { return predicate(x, y, z); }
    explicit operator bool() const { return static_cast<bool>(predicate); }
};

template <typename T, typename = typename std::enable_if<ivdetail::iv_valid_value_v<T>>::type>
class Region {
public:
    using value_type      = T;
    using interval_type   = Interval<T>;
    using constraint_type = Constraint2D<T>;
    using predicate_type  = typename Constraint2D<T>::predicate_type;

    Interval<T> x_range;
    Interval<T> y_range;

private:
    std::vector<Constraint2D<T>> m_constraints;

public:
    Region() = default;                                                                 
    Region(Interval<T> xr, Interval<T> yr) : x_range(std::move(xr)), y_range(std::move(yr)) {}
    Region(Interval<T> xr, Interval<T> yr, predicate_type c, std::string label = {}) : Region(std::move(xr), std::move(yr)) {
        if (c) add_constraint(std::move(c), std::move(label));
    }
    Region(T x0, T x1, T y0, T y1) : x_range(Interval<T>::span(std::move(x0), std::move(x1))), y_range(Interval<T>::span(std::move(y0), std::move(y1))) {}
    Region(T x0, T x1, T y0, T y1, predicate_type c, std::string label = {}) : Region(std::move(x0), std::move(x1), std::move(y0), std::move(y1)) {
        if (c) add_constraint(std::move(c), std::move(label));
    }

    bool contains(const T& x, const T& y) const {
        if (!x_range.contains(x) || !y_range.contains(y)) return false;
        for (const auto& c : m_constraints) if (c && !c(x, y)) return false;
        return true;
    }

    bool        is_empty()         const { return x_range.is_empty() || y_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool        has_constraints()  const noexcept { return !m_constraints.empty(); }
    const std::vector<Constraint2D<T>>& constraints() const noexcept { return m_constraints; }

    Region& add_constraint(predicate_type pred, std::string label = {}) { m_constraints.emplace_back(std::move(pred), std::move(label)); return *this; }
    Region& add_constraint(Constraint2D<T> c)                           { m_constraints.push_back(std::move(c)); return *this; }
    Region& where(predicate_type pred, std::string label = {})          { return add_constraint(std::move(pred), std::move(label)); }
    Region& where_all(std::initializer_list<Constraint2D<T>> list)      { m_constraints.insert(m_constraints.end(), list.begin(), list.end()); return *this; }
    Region& clear_constraints()                                         { m_constraints.clear(); return *this; }

    Region intersect(const Region& other) const {
        Region r(x_range.intersect(other.x_range), y_range.intersect(other.y_range));
        r.m_constraints = m_constraints;
        r.m_constraints.insert(r.m_constraints.end(), other.m_constraints.begin(), other.m_constraints.end());
        return r;
    }

    Region operator&(const Region& other) const { return intersect(other); }

    predicate_type as_predicate() const {
        const Region copy(*this);
        return [copy](const T& x, const T& y) { return copy.contains(x, y); };
    }

    static Region rectangle(T x0, T x1, T y0, T y1) { return Region(std::move(x0), std::move(x1), std::move(y0), std::move(y1)); }
    static Region square(const T& cx, const T& cy, const T& half) { return Region(cx - half, cx + half, cy - half, cy + half); }

    static Region disk(const T& cx, const T& cy, const T& r) {
        Region reg = square(cx, cy, r);
        const T r2 = r * r;
        reg.add_constraint([cx, cy, r2](const T& x, const T& y) {
            const T dx = x - cx;
            const T dy = y - cy;
            return !(r2 < dx * dx + dy * dy);
        }, "disk(r=" + ivdetail::iv_str(r) + ")");
        return reg;
    }

    static Region annulus(const T& cx, const T& cy, const T& r_inner, const T& r_outer) {
        Region reg = square(cx, cy, r_outer);
        const T ri2 = r_inner * r_inner;
        const T ro2 = r_outer * r_outer;
        reg.add_constraint([cx, cy, ri2, ro2](const T& x, const T& y) {
            const T dx = x - cx;
            const T dy = y - cy;
            const T d2 = dx * dx + dy * dy;
            return !(d2 < ri2) && !(ro2 < d2);
        }, "annulus");
        return reg;
    }

    static Region half_plane_x_positive(T y0, T y1, T x_min = ivdetail::iv_zero<T>()) {
        return Region(Interval<T>::at_least(std::move(x_min)), Interval<T>::span(std::move(y0), std::move(y1)));
    }

    static Region half_plane_x_positive(T y0, T y1, T x_min, T x_max) {
        return Region(std::move(x_min), std::move(x_max), std::move(y0), std::move(y1));
    }

    static Region from_predicate(T x0, T x1, T y0, T y1, predicate_type pred) {
        return Region(std::move(x0), std::move(x1), std::move(y0), std::move(y1), std::move(pred));
    }

    static Region all()                          { return Region(Interval<T>::whole(), Interval<T>::whole()); }
    static Region first_quadrant()               { return Region(Interval<T>::at_least(ivdetail::iv_zero<T>()), Interval<T>::at_least(ivdetail::iv_zero<T>())); }
    static Region first_quadrant(T x_max, T y_max) { return Region(ivdetail::iv_zero<T>(), std::move(x_max), ivdetail::iv_zero<T>(), std::move(y_max)); }
};

template <typename T> using Region2D = Region<T>;

template <typename T, typename = typename std::enable_if<ivdetail::iv_valid_value_v<T>>::type>
class Region3D {
public:
    using value_type      = T;
    using interval_type   = Interval<T>;
    using constraint_type = Constraint3D<T>;
    using predicate_type  = typename Constraint3D<T>::predicate_type;

    Interval<T> x_range;
    Interval<T> y_range;
    Interval<T> z_range;

private:
    std::vector<Constraint3D<T>> m_constraints;

public:
    Region3D() = default;                                                              
    Region3D(Interval<T> xr, Interval<T> yr, Interval<T> zr) : x_range(std::move(xr)), y_range(std::move(yr)), z_range(std::move(zr)) {}
    Region3D(Interval<T> xr, Interval<T> yr, Interval<T> zr, predicate_type c, std::string label = {})
        : Region3D(std::move(xr), std::move(yr), std::move(zr)) { if (c) add_constraint(std::move(c), std::move(label)); }
    Region3D(T x0, T x1, T y0, T y1, T z0, T z1)
        : x_range(Interval<T>::span(std::move(x0), std::move(x1))),
          y_range(Interval<T>::span(std::move(y0), std::move(y1))),
          z_range(Interval<T>::span(std::move(z0), std::move(z1))) {}
    Region3D(T x0, T x1, T y0, T y1, T z0, T z1, predicate_type c, std::string label = {})
        : Region3D(std::move(x0), std::move(x1), std::move(y0), std::move(y1), std::move(z0), std::move(z1)) {
        if (c) add_constraint(std::move(c), std::move(label));
    }

    bool contains(const T& x, const T& y, const T& z) const {
        if (!x_range.contains(x) || !y_range.contains(y) || !z_range.contains(z)) return false;
        for (const auto& c : m_constraints) if (c && !c(x, y, z)) return false;
        return true;
    }

    bool        is_empty()         const { return x_range.is_empty() || y_range.is_empty() || z_range.is_empty(); }
    std::size_t constraint_count() const noexcept { return m_constraints.size(); }
    bool        has_constraints()  const noexcept { return !m_constraints.empty(); }
    const std::vector<Constraint3D<T>>& constraints() const noexcept { return m_constraints; }

    Region3D& add_constraint(predicate_type pred, std::string label = {}) { m_constraints.emplace_back(std::move(pred), std::move(label)); return *this; }
    Region3D& add_constraint(Constraint3D<T> c)                           { m_constraints.push_back(std::move(c)); return *this; }
    Region3D& where(predicate_type pred, std::string label = {})          { return add_constraint(std::move(pred), std::move(label)); }
    Region3D& where_all(std::initializer_list<Constraint3D<T>> list)      { m_constraints.insert(m_constraints.end(), list.begin(), list.end()); return *this; }
    Region3D& clear_constraints()                                         { m_constraints.clear(); return *this; }

    Region3D intersect(const Region3D& other) const {
        Region3D r(x_range.intersect(other.x_range), y_range.intersect(other.y_range), z_range.intersect(other.z_range));
        r.m_constraints = m_constraints;
        r.m_constraints.insert(r.m_constraints.end(), other.m_constraints.begin(), other.m_constraints.end());
        return r;
    }

    Region3D operator&(const Region3D& other) const { return intersect(other); }

    predicate_type as_predicate() const {
        const Region3D copy(*this);
        return [copy](const T& x, const T& y, const T& z) { return copy.contains(x, y, z); };
    }

    static Region3D box(T x0, T x1, T y0, T y1, T z0, T z1) {
        return Region3D(std::move(x0), std::move(x1), std::move(y0), std::move(y1), std::move(z0), std::move(z1));
    }

    static Region3D cube(const T& cx, const T& cy, const T& cz, const T& half) {
        return Region3D(cx - half, cx + half, cy - half, cy + half, cz - half, cz + half);
    }

    static Region3D ball(const T& cx, const T& cy, const T& cz, const T& r) {
        Region3D reg = cube(cx, cy, cz, r);
        const T r2 = r * r;
        reg.add_constraint([cx, cy, cz, r2](const T& x, const T& y, const T& z) {
            const T dx = x - cx;
            const T dy = y - cy;
            const T dz = z - cz;
            return !(r2 < dx * dx + dy * dy + dz * dz);
        }, "ball(r=" + ivdetail::iv_str(r) + ")");
        return reg;
    }

    static Region3D spherical_shell(const T& cx, const T& cy, const T& cz, const T& r_inner, const T& r_outer) {
        Region3D reg = cube(cx, cy, cz, r_outer);
        const T ri2 = r_inner * r_inner;
        const T ro2 = r_outer * r_outer;
        reg.add_constraint([cx, cy, cz, ri2, ro2](const T& x, const T& y, const T& z) {
            const T dx = x - cx;
            const T dy = y - cy;
            const T dz = z - cz;
            const T d2 = dx * dx + dy * dy + dz * dz;
            return !(d2 < ri2) && !(ro2 < d2);
        }, "spherical_shell");
        return reg;
    }

    static Region3D cylinder_z(const T& cx, const T& cy, const T& r, T z0, T z1) {
        Region3D reg(Interval<T>::span(cx - r, cx + r), Interval<T>::span(cy - r, cy + r), Interval<T>::span(std::move(z0), std::move(z1)));
        const T r2 = r * r;
        reg.add_constraint([cx, cy, r2](const T& x, const T& y, const T&) {
            const T dx = x - cx;
            const T dy = y - cy;
            return !(r2 < dx * dx + dy * dy);
        }, "cylinder_z(r=" + ivdetail::iv_str(r) + ")");
        return reg;
    }

    static Region3D cylinder_x(const T& cy, const T& cz, const T& r, T x0, T x1) {
        Region3D reg(Interval<T>::span(std::move(x0), std::move(x1)), Interval<T>::span(cy - r, cy + r), Interval<T>::span(cz - r, cz + r));
        const T r2 = r * r;
        reg.add_constraint([cy, cz, r2](const T&, const T& y, const T& z) {
            const T dy = y - cy;
            const T dz = z - cz;
            return !(r2 < dy * dy + dz * dz);
        }, "cylinder_x(r=" + ivdetail::iv_str(r) + ")");
        return reg;
    }

    static Region3D cylinder_y(const T& cx, const T& cz, const T& r, T y0, T y1) {
        Region3D reg(Interval<T>::span(cx - r, cx + r), Interval<T>::span(std::move(y0), std::move(y1)), Interval<T>::span(cz - r, cz + r));
        const T r2 = r * r;
        reg.add_constraint([cx, cz, r2](const T& x, const T&, const T& z) {
            const T dx = x - cx;
            const T dz = z - cz;
            return !(r2 < dx * dx + dz * dz);
        }, "cylinder_y(r=" + ivdetail::iv_str(r) + ")");
        return reg;
    }

    static Region3D from_predicate(T x0, T x1, T y0, T y1, T z0, T z1, predicate_type pred) {
        return Region3D(std::move(x0), std::move(x1), std::move(y0), std::move(y1), std::move(z0), std::move(z1), std::move(pred));
    }

    static Region3D all() { return Region3D(Interval<T>::whole(), Interval<T>::whole(), Interval<T>::whole()); }

    static Region3D first_octant() {
        const Interval<T> h = Interval<T>::at_least(ivdetail::iv_zero<T>());
        return Region3D(h, h, h);
    }

    static Region3D first_octant(T x_max, T y_max, T z_max) {
        return Region3D(ivdetail::iv_zero<T>(), std::move(x_max), ivdetail::iv_zero<T>(), std::move(y_max), ivdetail::iv_zero<T>(), std::move(z_max));
    }
};

} // namespace fizmo

#endif // FIZMO_REGION_HPP