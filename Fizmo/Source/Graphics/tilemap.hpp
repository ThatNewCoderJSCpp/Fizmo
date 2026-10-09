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

    void recount() noexcept {
        if (!m_texture.valid() || m_tile_w == 0 || m_tile_h == 0) { m_columns = m_count = 0; return; }
        const unsigned int tw = m_texture.width(), th = m_texture.height();
        if (m_columns == 0) m_columns = tw >= 2 * m_margin + m_tile_w ? (tw - 2 * m_margin + m_spacing) / (m_tile_w + m_spacing) : 0;
        const unsigned int rows = th >= 2 * m_margin + m_tile_h ? (th - 2 * m_margin + m_spacing) / (m_tile_h + m_spacing) : 0;
        m_count = m_columns * rows;
        m_solid.resize(m_count, false);
    }

public:
    TileSet() = default;

    TileSet(const Texture& texture, unsigned int tile_w, unsigned int tile_h, unsigned int margin = 0, unsigned int spacing = 0, unsigned int columns = 0)
        : m_texture(texture), m_tile_w(tile_w), m_tile_h(tile_h), m_margin(margin), m_spacing(spacing), m_columns(columns) { recount(); }

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

    TextureRect rect(std::uint32_t local) const noexcept {
        if (m_columns == 0 || local >= m_count) return {};
        const unsigned int col = local % m_columns, row = local / m_columns;
        return TextureRect(static_cast<int>(m_margin + col * (m_tile_w + m_spacing)), static_cast<int>(m_margin + row * (m_tile_h + m_spacing)), m_tile_w, m_tile_h);
    }

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
    TileLayer(std::string name, unsigned int w, unsigned int h) : m_name(std::move(name)), m_width(w), m_height(h), m_tiles(static_cast<std::size_t>(w) * h, 0) {}

    const std::string& name() const noexcept { return m_name; }
    unsigned int width() const noexcept { return m_width; }
    unsigned int height() const noexcept { return m_height; }
    std::uint64_t version() const noexcept { return m_version; }

    std::uint32_t get(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return 0;
        return m_tiles[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)];
    }

    void set(int x, int y, std::uint32_t gid) noexcept {
        if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return;
        m_tiles[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)] = gid;
        ++m_version;
    }

    void fill(std::uint32_t gid) { std::fill(m_tiles.begin(), m_tiles.end(), gid); ++m_version; }

    void fill_rect(int x, int y, int w, int h, std::uint32_t gid) noexcept {
        for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i) set(i, j, gid);
    }

    void resize(unsigned int w, unsigned int h) {
        std::vector<std::uint32_t> next(static_cast<std::size_t>(w) * h, 0);
        for (unsigned int y = 0; y < std::min(h, m_height); ++y)
            for (unsigned int x = 0; x < std::min(w, m_width); ++x) next[static_cast<std::size_t>(y) * w + x] = m_tiles[static_cast<std::size_t>(y) * m_width + x];
        m_tiles.swap(next);
        m_width = w;
        m_height = h;
        ++m_version;
    }

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

    TileSet& add_tileset(TileSet set) {
        std::uint32_t next = 1;
        for (const TileSet& s : m_sets) next = std::max(next, s.first_gid() + s.count());
        if (set.first_gid() < next) set.set_first_gid(next);
        m_sets.push_back(std::move(set));
        return m_sets.back();
    }

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

    const TileSet* tileset_for(std::uint32_t gid) const noexcept {
        const std::uint32_t id = gid & kTileIdMask;
        const TileSet* best = nullptr;
        for (const TileSet& s : m_sets) if (s.contains(id) && (!best || s.first_gid() > best->first_gid())) best = &s;
        return best;
    }

    std::uint32_t resolve(std::uint32_t gid) const noexcept {
        const std::uint32_t id = gid & kTileIdMask;
        const auto it = m_anim_current.find(id);
        return it == m_anim_current.end() ? gid : ((gid & ~kTileIdMask) | it->second);
    }

    bool solid(std::uint32_t gid) const noexcept {
        const std::uint32_t id = gid & kTileIdMask;
        if (id == 0) return false;
        if (id < m_solid_gids.size() && m_solid_gids[id]) return true;
        const TileSet* s = tileset_for(id);
        return s && s->solid(id - s->first_gid());
    }

    void set_solid(std::uint32_t gid, bool value = true) {
        const std::uint32_t id = gid & kTileIdMask;
        if (id >= m_solid_gids.size()) m_solid_gids.resize(id + 1, false);
        m_solid_gids[id] = value;
    }

    void update(double dt) {
        m_time += dt;
        m_anim_current.clear();
        for (const TileSet& s : m_sets) {
            for (const auto& kv : s.animations()) {
                const TileAnimation& a = kv.second;
                if (a.frames.empty()) continue;
                double total = 0.0;
                for (std::size_t i = 0; i < a.frames.size(); ++i) total += i < a.durations.size() ? a.durations[i] : 0.1;
                if (total <= 0.0) continue;
                double t = std::fmod(m_time, total);
                std::size_t f = 0;
                for (; f < a.frames.size(); ++f) {
                    const double d = f < a.durations.size() ? a.durations[f] : 0.1;
                    if (t < d) break;
                    t -= d;
                }
                if (f >= a.frames.size()) f = a.frames.size() - 1;
                m_anim_current[s.first_gid() + kv.first] = s.first_gid() + a.frames[f];
            }
        }
    }

    int world_to_tile_x(double wx) const noexcept { return m_tile_w ? static_cast<int>(std::floor(wx / m_tile_w)) : 0; }
    int world_to_tile_y(double wy) const noexcept { return m_tile_h ? static_cast<int>(std::floor(wy / m_tile_h)) : 0; }
    double tile_to_world_x(int tx) const noexcept { return static_cast<double>(tx) * m_tile_w; }
    double tile_to_world_y(int ty) const noexcept { return static_cast<double>(ty) * m_tile_h; }

    bool solid_at_tile(int tx, int ty) const noexcept {
        for (const TileLayer& l : m_layers) if (l.collides() && solid(l.get(tx, ty))) return true;
        return false;
    }

    bool solid_at(double wx, double wy) const noexcept { return solid_at_tile(world_to_tile_x(wx), world_to_tile_y(wy)); }

    bool overlaps_solid(double x, double y, double w, double h) const noexcept {
        if (w <= 0.0 || h <= 0.0) return false;
        const int x0 = world_to_tile_x(x), y0 = world_to_tile_y(y);
        const int x1 = world_to_tile_x(x + w - 1e-9), y1 = world_to_tile_y(y + h - 1e-9);
        for (int ty = y0; ty <= y1; ++ty) for (int tx = x0; tx <= x1; ++tx) if (solid_at_tile(tx, ty)) return true;
        return false;
    }

    TileMove move(double x, double y, double w, double h, double dx, double dy) const noexcept {
        TileMove r;
        r.x = x;
        r.y = y;
        if (m_tile_w == 0 || m_tile_h == 0) { r.x += dx; r.y += dy; return r; }
        const double step_x = m_tile_w * 0.5, step_y = m_tile_h * 0.5;
        const int nx = std::max(1, static_cast<int>(std::ceil(std::abs(dx) / step_x)));
        const int ny = std::max(1, static_cast<int>(std::ceil(std::abs(dy) / step_y)));

        for (int i = 0; i < nx && !r.hit_x; ++i) {
            const double sx = dx / nx;
            const double nxp = r.x + sx;
            if (!overlaps_solid(nxp, r.y, w, h)) { r.x = nxp; continue; }
            r.hit_x = true;
            if (sx > 0.0) { r.x = tile_to_world_x(world_to_tile_x(nxp + w - 1e-9)) - w; r.on_right = true; }
            else { r.x = tile_to_world_x(world_to_tile_x(nxp) + 1); r.on_left = true; }
        }

        for (int i = 0; i < ny && !r.hit_y; ++i) {
            const double sy = dy / ny;
            const double nyp = r.y + sy;
            if (!overlaps_solid(r.x, nyp, w, h)) { r.y = nyp; continue; }
            r.hit_y = true;
            if (sy > 0.0) { r.y = tile_to_world_y(world_to_tile_y(nyp + h - 1e-9)) - h; r.on_ground = true; }
            else { r.y = tile_to_world_y(world_to_tile_y(nyp) + 1); r.on_ceiling = true; }
        }

        if (!r.on_ground && overlaps_solid(r.x, r.y + h, w, 0.5)) r.on_ground = dy >= 0.0;
        return r;
    }

    TileHit raycast(double ox, double oy, double dx, double dy, double max_distance) const noexcept {
        TileHit hit;
        const double len = std::hypot(dx, dy);
        if (len <= 0.0 || m_tile_w == 0 || m_tile_h == 0) return hit;
        dx /= len;
        dy /= len;
        int tx = world_to_tile_x(ox), ty = world_to_tile_y(oy);
        const int step_x = dx > 0.0 ? 1 : -1, step_y = dy > 0.0 ? 1 : -1;
        const double inf = 1e300;
        const double delta_x = dx != 0.0 ? std::abs(m_tile_w / dx) : inf;
        const double delta_y = dy != 0.0 ? std::abs(m_tile_h / dy) : inf;
        double side_x = dx != 0.0 ? ((dx > 0.0 ? tile_to_world_x(tx + 1) - ox : ox - tile_to_world_x(tx)) / std::abs(dx)) : inf;
        double side_y = dy != 0.0 ? ((dy > 0.0 ? tile_to_world_y(ty + 1) - oy : oy - tile_to_world_y(ty)) / std::abs(dy)) : inf;
        double t = 0.0;
        int axis = -1;

        while (t <= max_distance) {
            if (solid_at_tile(tx, ty)) {
                hit.hit = true;
                hit.tile_x = tx;
                hit.tile_y = ty;
                hit.distance = t;
                hit.x = ox + dx * t;
                hit.y = oy + dy * t;
                if (axis == 0) hit.normal_x = -step_x;
                else if (axis == 1) hit.normal_y = -step_y;
                for (const TileLayer& l : m_layers) if (l.collides() && solid(l.get(tx, ty))) { hit.gid = l.get(tx, ty); break; }
                return hit;
            }
            if (side_x < side_y) { t = side_x; side_x += delta_x; tx += step_x; axis = 0; }
            else { t = side_y; side_y += delta_y; ty += step_y; axis = 1; }
        }

        return hit;
    }

    double pixel_width() const noexcept {
        unsigned int w = 0;
        for (const TileLayer& l : m_layers) w = std::max(w, l.width());
        return static_cast<double>(w) * m_tile_w;
    }

    double pixel_height() const noexcept {
        unsigned int h = 0;
        for (const TileLayer& l : m_layers) h = std::max(h, l.height());
        return static_cast<double>(h) * m_tile_h;
    }

    static bool parse_csv(const std::string& text, std::vector<std::uint32_t>& out, unsigned int& w, unsigned int& h) {
        out.clear();
        w = h = 0;
        std::istringstream in(text);
        std::string line;

        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find_first_not_of(" \t,") == std::string::npos) continue;
            unsigned int count = 0;
            std::size_t start = 0;
            while (start <= line.size()) {
                std::size_t comma = line.find(',', start);
                if (comma == std::string::npos) comma = line.size();
                std::string cell = line.substr(start, comma - start);
                cell.erase(0, cell.find_first_not_of(" \t"));
                cell.erase(cell.find_last_not_of(" \t") + 1);
                if (!cell.empty()) {
                    const long long v = std::strtoll(cell.c_str(), nullptr, 10);
                    out.push_back(v < 0 ? 0u : static_cast<std::uint32_t>(v));
                    ++count;
                }
                start = comma + 1;
            }
            if (w == 0) w = count;
            else if (count != w) return false;
            ++h;
        }

        return w > 0;
    }

    TileLayer* add_layer_csv(const std::string& name, const std::string& csv, std::int64_t id_offset = 0) {
        std::vector<std::uint32_t> ids;
        unsigned int w = 0, h = 0;
        if (!parse_csv(csv, ids, w, h)) return nullptr;
        TileLayer& l = add_layer(name, w, h);
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const std::int64_t v = static_cast<std::int64_t>(ids[i]) + id_offset;
            l.tiles()[i] = v <= 0 ? 0u : static_cast<std::uint32_t>(v);
        }
        return &l;
    }

    static bool load_tiled_json(const std::filesystem::path& path, TileMap& out, std::string* error = nullptr) {
        auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
        const auto text = system::paths::read_text(path);
        if (!text) return fail("cannot read " + system::paths::to_utf8(path));
        json::Value doc;
        std::string err;
        if (!json::parse(*text, doc, &err)) return fail(err);
        if (doc["orientation"].as_string("orthogonal") != "orthogonal") return fail("only orthogonal Tiled maps are supported");
        if (doc["infinite"].as_bool()) return fail("infinite Tiled maps are not supported");
        TileMap map(doc["tilewidth"].as_uint(), doc["tileheight"].as_uint());
        const std::filesystem::path dir = path.parent_path();

        for (const json::Value& ts : doc["tilesets"].items()) {
            json::Value def = ts;
            std::filesystem::path base = dir;
            if (ts.has("source")) {
                const std::filesystem::path src = dir / system::paths::from_utf8(ts["source"].as_string());
                const auto ext = src.extension().string();
                if (ext != ".json" && ext != ".tsj") return fail("external tileset must be JSON: " + ts["source"].as_string());
                const auto tsx = system::paths::read_text(src);
                if (!tsx || !json::parse(*tsx, def, &err)) return fail("cannot read tileset " + ts["source"].as_string());
                base = src.parent_path();
            }
            const std::filesystem::path image = base / system::paths::from_utf8(def["image"].as_string());
            auto img = system::load_image(image, &err);
            if (!img) return fail(err);
            TileSet set(Texture(std::move(*img)), def["tilewidth"].as_uint(), def["tileheight"].as_uint(), def["margin"].as_uint(), def["spacing"].as_uint(), def["columns"].as_uint());
            set.set_first_gid(ts["firstgid"].as_uint(1));
            set.set_name(def["name"].as_string());

            for (const json::Value& tile : def["tiles"].items()) {
                const std::uint32_t id = tile["id"].as_uint();
                for (const json::Value& prop : tile["properties"].items()) {
                    if (prop["name"].as_string() == "solid" || prop["name"].as_string() == "collides") set.set_solid(id, prop["value"].as_bool());
                }
                if (tile["animation"].is_array()) {
                    TileAnimation a;
                    for (const json::Value& f : tile["animation"].items()) {
                        a.frames.push_back(f["tileid"].as_uint());
                        a.durations.push_back(f["duration"].as_number(100.0) / 1000.0);
                    }
                    set.set_animation(id, std::move(a));
                }
            }

            map.m_sets.push_back(std::move(set));
        }

        for (const json::Value& l : doc["layers"].items()) {
            if (l["type"].as_string() != "tilelayer") continue;
            if (l["encoding"].as_string("csv") != "csv" || l["data"].is_string()) return fail("only CSV-encoded (JSON array) Tiled layers are supported");
            TileLayer& layer = map.add_layer(l["name"].as_string(), l["width"].as_uint(), l["height"].as_uint());
            const auto& data = l["data"].items();
            auto& tiles = layer.tiles();
            for (std::size_t i = 0; i < data.size() && i < tiles.size(); ++i) tiles[i] = static_cast<std::uint32_t>(data[i].as_number());
            layer.set_visible(l["visible"].as_bool(true));
            layer.set_opacity(l["opacity"].as_float(1.0f));
            layer.set_offset(l["offsetx"].as_number(), l["offsety"].as_number());
            layer.set_parallax(l["parallaxx"].as_number(1.0), l["parallaxy"].as_number(1.0));
            for (const json::Value& prop : l["properties"].items()) if (prop["name"].as_string() == "collides") layer.set_collides(prop["value"].as_bool(true));
        }

        out = std::move(map);
        return true;
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TILEMAP_HPP
