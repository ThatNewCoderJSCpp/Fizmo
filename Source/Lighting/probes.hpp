#ifndef FIZMO_LIGHTING_PROBES_HPP
#define FIZMO_LIGHTING_PROBES_HPP

#include "sh.hpp"
#include "math.hpp"

#include <cstdint>
#include <vector>

namespace fizmo {
namespace lighting {

class ProbeVolume {
public:
    void configure(const Vec3& lo, const Vec3& hi, float spacing, std::size_t max_probes = 32768);
    void clear();
    bool empty() const noexcept { return m_sh.empty(); }
    std::size_t count() const noexcept { return m_sh.size(); }
    int nx() const noexcept { return m_nx; }
    int ny() const noexcept { return m_ny; }
    int nz() const noexcept { return m_nz; }
    float spacing() const noexcept { return m_cell.x; }
    Vec3 origin() const noexcept { return m_lo.to_double(); }
    Vec3 position(std::size_t i) const noexcept { return position_f(i).to_double(); }
    Vec3f position_f(std::size_t i) const noexcept;
    std::size_t index(int x, int y, int z) const noexcept { return (static_cast<std::size_t>(z) * static_cast<std::size_t>(m_ny) + static_cast<std::size_t>(y)) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(x); }

    Rgb irradiance(const Vec3& p, const Vec3& n) const noexcept { return irradiance_f(Vec3f(p), normalize(Vec3f(n))); }
    Rgb irradiance_f(const Vec3f& p, const Vec3f& n) const noexcept;

    Sh9& probe(std::size_t i) { return m_sh[i]; }
    const Sh9& probe(std::size_t i) const { return m_sh[i]; }
    bool live(std::size_t i) const noexcept { return m_live[i] != 0; }
    void set_live(std::size_t i, bool on) noexcept { m_live[i] = on ? 1 : 0; }
    int updates() const noexcept { return m_updates; }
    void count_update() noexcept { ++m_updates; }
    bool matches(const Vec3f& lo, const Vec3f& hi, float spacing) const noexcept;
    Vec3 requested_lo() const noexcept { return m_req_lo.to_double(); }
    Vec3 requested_hi() const noexcept { return m_req_hi.to_double(); }
    float requested_spacing() const noexcept { return m_requested; }

private:
    std::vector<Sh9> m_sh;
    std::vector<std::uint8_t> m_live;
    Vec3f m_lo;
    Vec3f m_hi;
    Vec3f m_cell{ 1.0f, 1.0f, 1.0f };
    float m_requested = 0.0f;
    Vec3f m_req_lo;
    Vec3f m_req_hi;
    int m_nx = 0;
    int m_ny = 0;
    int m_nz = 0;
    int m_updates = 0;
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_PROBES_HPP
