#ifndef FIZMO_TILEMAP_HPP
#define FIZMO_TILEMAP_HPP

#include "texture.hpp"
#include "draw_types_2d.hpp"
#include "../Util Hpp/json.hpp"
#include "../System/paths.hpp"
#include "../System/hot_reload.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

inline constexpr std::uint32_t kTileFlipH    = 0x80000000u;
inline constexpr std::uint32_t kTileFlipV    = 0x40000000u;
inline constexpr std::uint32_t kTileFlipDiag = 0x20000000u;
inline constexpr std::uint32_t kTileIdMask   = 0x1FFFFFFFu;

struct TileAnimation {
    std::vector<std::uint32_t> frames;
    std::vector<double>        durations;
};

class TileSet {
private:
    Texture      m_texture;
    unsigned int m_tile_w = 0, m_tile_h = 0, m_margin = 0, m_spacing = 0, m_columns = 0, m_count = 0;
    std::uint32_t m_first_gid = 1;
    std::string  m_name;
    std::vector<bool> m_solid;
    std::unordered_map<std::uint32_t, TileAnimation> m_animations;

    void recount() noexcept;

public:
    TileSet() = default;

    TileSet(const Texture& texture, unsigned int tile_w, unsigned int tile_h, unsigned int margin = 0, unsigned int spacing = 0, unsigned int columns = 0)
;

    const Texture& texture() const noexcept { return m_texture; }
    Texture& texture() noexcept { return m_texture; }
    unsigned int tile_width() const noexcept { return m_tile_w; }
    unsigned int tile_height() const noexcept { return m_tile_h; }
    unsigned int columns() const noexcept { return m_columns; }
    unsigned int count() const noexcept { return m_count; }
    std::uint32_t first_gid() const noexcept { return m_first_gid; }
    void set_first_gid(std::uint32_t g) noexcept { m_first_gid = g ? g : 1; }
    const std::string& name() const noexcept { return m_name; }
    void set_name(std::string n) { m_name = std::move(n); }
    void set_texture(const Texture& t) { m_texture = t; m_columns = 0; recount(); }

    bool contains(std::uint32_t gid) const noexcept {
        const std::uint32_t id = gid & kTileIdMask;
        return id >= m_first_gid && id < m_first_gid + m_count;
    }

    TextureRect rect(std::uint32_t local) const noexcept;

    void set_solid(std::uint32_t local, bool solid = true) { if (local < m_solid.size()) m_solid[local] = solid; }
    bool solid(std::uint32_t local) const noexcept { return local < m_solid.size() && m_solid[local]; }
    void set_animation(std::uint32_t local, TileAnimation anim) { m_animations[local] = std::move(anim); }
    const std::unordered_map<std::uint32_t, TileAnimation>& animations() const noexcept { return m_animations; }
};

class TileLayer {
private:
    std::string                m_name;
    unsigned int               m_width = 0, m_height = 0;
    std::vector<std::uint32_t> m_tiles;
    bool                       m_visible = true;
    bool                       m_collides = true;
    float                      m_opacity = 1.0f;
    Color                      m_tint = Color(255, 255, 255, 255);
    BlendMode                  m_blend = BlendMode::Normal;
    double                     m_offset_x = 0.0, m_offset_y = 0.0;
    double                     m_parallax_x = 1.0, m_parallax_y = 1.0;
    std::uint64_t              m_version = 1;

public:
    TileLayer() = default;
    TileLayer(std::string name, unsigned int w, unsigned int h);

    const std::string& name() const noexcept { return m_name; }
    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    std::uint64_t version() const noexcept { return m_version; }

    std::uint32_t get(int x, int y) const noexcept;

    void set(int x, int y, std::uint32_t gid) noexcept;

    void fill(std::uint32_t gid) { std::fill(m_tiles.begin(), m_tiles.end(), gid); ++m_version; }

    void fill_rect(int x, int y, int w, int h, std::uint32_t gid) noexcept {
        for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i) set(i, j, gid);
    }

    void resize(unsigned int w, unsigned int h);

    const std::vector<std::uint32_t>& tiles() const noexcept { return m_tiles; }
    std::vector<std::uint32_t>& tiles() noexcept { ++m_version; return m_tiles; }

    bool visible() const noexcept { return m_visible; }
    void set_visible(bool v) noexcept { m_visible = v; }
    bool collides() const noexcept { return m_collides; }
    void set_collides(bool c) noexcept { m_collides = c; }
    float opacity() const noexcept { return m_opacity; }
    void set_opacity(float o) noexcept { m_opacity = std::max(0.0f, std::min(1.0f, o)); }
    const Color& tint() const noexcept { return m_tint; }
    void set_tint(const Color& c) noexcept { m_tint = c; }
    BlendMode blend_mode() const noexcept { return m_blend; }
    void set_blend_mode(BlendMode b) noexcept { m_blend = b; }
    double offset_x() const noexcept { return m_offset_x; }
    double offset_y() const noexcept { return m_offset_y; }
    void set_offset(double x, double y) noexcept { m_offset_x = x; m_offset_y = y; }
    double parallax_x() const noexcept { return m_parallax_x; }
    double parallax_y() const noexcept { return m_parallax_y; }
    void set_parallax(double x, double y) noexcept { m_parallax_x = x; m_parallax_y = y; }
};

struct TileHit {
    bool          hit = false;
    int           tile_x = 0, tile_y = 0;
    std::uint32_t gid = 0;
    double        distance = 0.0;
    double        x = 0.0, y = 0.0;
    double        normal_x = 0.0, normal_y = 0.0;
};

struct TileMove {
    double x = 0.0, y = 0.0;
    bool   hit_x = false, hit_y = false;
    bool   on_ground = false, on_ceiling = false, on_left = false, on_right = false;
};

class TileMap {
private:
    unsigned int           m_tile_w = 0, m_tile_h = 0;
    std::vector<TileSet>   m_sets;
    std::vector<TileLayer> m_layers;
    std::vector<bool>      m_solid_gids;
    double                 m_time = 0.0;
    std::unordered_map<std::uint32_t, std::uint32_t> m_anim_current;

public:
    TileMap() = default;
    TileMap(unsigned int tile_w, unsigned int tile_h) : m_tile_w(tile_w), m_tile_h(tile_h) {}

    unsigned int tile_width() const noexcept { return m_tile_w; }
    unsigned int tile_height() const noexcept { return m_tile_h; }
    void set_tile_size(unsigned int w, unsigned int h) noexcept { m_tile_w = w; m_tile_h = h; }

    TileSet& add_tileset(TileSet set);

    TileLayer& add_layer(const std::string& name, unsigned int w, unsigned int h) {
        m_layers.emplace_back(name, w, h);
        return m_layers.back();
    }

    std::vector<TileSet>& tilesets() noexcept { return m_sets; }
    const std::vector<TileSet>& tilesets() const noexcept { return m_sets; }
    std::vector<TileLayer>& layers() noexcept { return m_layers; }
    const std::vector<TileLayer>& layers() const noexcept { return m_layers; }

    TileLayer* layer(const std::string& name) noexcept {
        for (TileLayer& l : m_layers) if (l.name() == name) return &l;
        return nullptr;
    }

    const TileSet* tileset_for(std::uint32_t gid) const noexcept;

    std::uint32_t resolve(std::uint32_t gid) const noexcept;

    bool solid(std::uint32_t gid) const noexcept;

    void set_solid(std::uint32_t gid, bool value = true);

    void update(double dt);

    int world_to_tile_x(double wx) const noexcept { return m_tile_w ? static_cast<int>(std::floor(wx / m_tile_w)) : 0; }
    int world_to_tile_y(double wy) const noexcept { return m_tile_h ? static_cast<int>(std::floor(wy / m_tile_h)) : 0; }
    double tile_to_world_x(int tx) const noexcept { return static_cast<double>(tx) * m_tile_w; }
    double tile_to_world_y(int ty) const noexcept { return static_cast<double>(ty) * m_tile_h; }

    bool solid_at_tile(int tx, int ty) const noexcept;

    bool solid_at(double wx, double wy) const noexcept { return solid_at_tile(world_to_tile_x(wx), world_to_tile_y(wy)); }

    bool overlaps_solid(double x, double y, double w, double h) const noexcept;

    TileMove move(double x, double y, double w, double h, double dx, double dy) const noexcept;

    TileHit raycast(double ox, double oy, double dx, double dy, double max_distance) const noexcept;

    double pixel_width() const noexcept;

    double pixel_height() const noexcept;

    static bool parse_csv(const std::string& text, std::vector<std::uint32_t>& out, unsigned int& w, unsigned int& h);

    TileLayer* add_layer_csv(const std::string& name, const std::string& csv, std::int64_t id_offset = 0);

    static bool load_tiled_json(const std::filesystem::path& path, TileMap& out, std::string* error = nullptr);
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TILEMAP_HPP
