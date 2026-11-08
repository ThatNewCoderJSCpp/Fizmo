#ifndef FIZMO_LIGHTING_PHOTONS_HPP
#define FIZMO_LIGHTING_PHOTONS_HPP

#include "integrator.hpp"

#include <unordered_map>
#include <vector>

namespace fizmo {
namespace lighting {
namespace detail {

struct Photon {
    Vec3f p;
    Vec3f wi;
    Vec3f n;
    Rgb power;
};

struct PhotonOptions {
    std::size_t count = 200000;
    int bundle = 1;
    int max_bounces = 12;
    float radius = 0.05f;
    std::uint64_t seed = 1;
    bool spectral = false;
    bool dispersion = true;
    unsigned threads = 0;
    LightFilter filter = LightFilter::all;
};

class PhotonMap {
public:
    void build(const Context& ctx, const PhotonOptions& o);
    Rgb gather(const Surface& s, const Bsdf& b, const Frame& frame, const Vec3f& wo) const;
    std::size_t size() const noexcept { return m_photons.size(); }
    float radius() const noexcept { return m_radius; }
    const std::vector<Photon>& photons() const noexcept { return m_photons; }
    void clear();

private:
    std::uint64_t key(int x, int y, int z) const noexcept;
    std::vector<Photon> m_photons;
    std::unordered_map<std::uint64_t, std::pair<std::uint32_t, std::uint32_t>> m_cells;
    float m_radius = 0.05f;
    float m_inv_cell = 20.0f;
};

} // namespace detail
} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_PHOTONS_HPP
