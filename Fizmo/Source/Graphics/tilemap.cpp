#include "fizmo_library.hpp"
#include "tilemap.hpp"

namespace fizmo {
namespace graphics {

void TileSet::recount() noexcept {
    if (!m_texture.valid() || m_tile_w == 0 || m_tile_h == 0) { m_columns = m_count = 0; return; }
    const unsigned int tw = m_texture.width(), th = m_texture.height();
    if (m_columns == 0) m_columns = tw >= 2 * m_margin + m_tile_w ? (tw - 2 * m_margin + m_spacing) / (m_tile_w + m_spacing) : 0;
    const unsigned int rows = th >= 2 * m_margin + m_tile_h ? (th - 2 * m_margin + m_spacing) / (m_tile_h + m_spacing) : 0;
    m_count = m_columns * rows;
    m_solid.resize(m_count, false);
}

TileSet::TileSet(const Texture& texture, unsigned int tile_w, unsigned int tile_h, unsigned int margin, unsigned int spacing, unsigned int columns) : m_texture(texture), m_tile_w(tile_w), m_tile_h(tile_h), m_margin(margin), m_spacing(spacing), m_columns(columns) { recount(); }

auto TileSet::rect(std::uint32_t local) const noexcept -> TextureRect {
    if (m_columns == 0 || local >= m_count) return {};
    const unsigned int col = local % m_columns, row = local / m_columns;
    return TextureRect(static_cast<int>(m_margin + col * (m_tile_w + m_spacing)), static_cast<int>(m_margin + row * (m_tile_h + m_spacing)), m_tile_w, m_tile_h);
}

TileLayer::TileLayer(std::string name, unsigned int w, unsigned int h) : m_name(std::move(name)), m_width(w), m_height(h), m_tiles(static_cast<std::size_t>(w) * h, 0) {}

std::uint32_t TileLayer::get(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return 0;
    return m_tiles[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)];
}

void TileLayer::set(int x, int y, std::uint32_t gid) noexcept {
    if (x < 0 || y < 0 || x >= static_cast<int>(m_width) || y >= static_cast<int>(m_height)) return;
    m_tiles[static_cast<std::size_t>(y) * m_width + static_cast<std::size_t>(x)] = gid;
    ++m_version;
}

void TileLayer::resize(unsigned int w, unsigned int h) {
    std::vector<std::uint32_t> next(static_cast<std::size_t>(w) * h, 0);
    for (unsigned int y = 0; y < std::min(h, m_height); ++y)
        for (unsigned int x = 0; x < std::min(w, m_width); ++x) next[static_cast<std::size_t>(y) * w + x] = m_tiles[static_cast<std::size_t>(y) * m_width + x];
    m_tiles.swap(next);
    m_width = w;
    m_height = h;
    ++m_version;
}

auto TileMap::add_tileset(TileSet set) -> TileSet& {
    std::uint32_t next = 1;
    for (const TileSet& s : m_sets) next = std::max(next, s.first_gid() + s.count());
    if (set.first_gid() < next) set.set_first_gid(next);
    m_sets.push_back(std::move(set));
    return m_sets.back();
}

auto TileMap::tileset_for(std::uint32_t gid) const noexcept -> const TileSet* {
    const std::uint32_t id = gid & kTileIdMask;
    const TileSet* best = nullptr;
    for (const TileSet& s : m_sets) if (s.contains(id) && (!best || s.first_gid() > best->first_gid())) best = &s;
    return best;
}

std::uint32_t TileMap::resolve(std::uint32_t gid) const noexcept {
    const std::uint32_t id = gid & kTileIdMask;
    const auto it = m_anim_current.find(id);
    return it == m_anim_current.end() ? gid : ((gid & ~kTileIdMask) | it->second);
}

bool TileMap::solid(std::uint32_t gid) const noexcept {
    const std::uint32_t id = gid & kTileIdMask;
    if (id == 0) return false;
    if (id < m_solid_gids.size() && m_solid_gids[id]) return true;
    const TileSet* s = tileset_for(id);
    return s && s->solid(id - s->first_gid());
}

void TileMap::set_solid(std::uint32_t gid, bool value) {
    const std::uint32_t id = gid & kTileIdMask;
    if (id >= m_solid_gids.size()) m_solid_gids.resize(id + 1, false);
    m_solid_gids[id] = value;
}

void TileMap::update(double dt) {
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

bool TileMap::solid_at_tile(int tx, int ty) const noexcept {
    for (const TileLayer& l : m_layers) if (l.collides() && solid(l.get(tx, ty))) return true;
    return false;
}

bool TileMap::overlaps_solid(double x, double y, double w, double h) const noexcept {
    if (w <= 0.0 || h <= 0.0) return false;
    const int x0 = world_to_tile_x(x), y0 = world_to_tile_y(y);
    const int x1 = world_to_tile_x(x + w - 1e-9), y1 = world_to_tile_y(y + h - 1e-9);
    for (int ty = y0; ty <= y1; ++ty) for (int tx = x0; tx <= x1; ++tx) if (solid_at_tile(tx, ty)) return true;
    return false;
}

auto TileMap::move(double x, double y, double w, double h, double dx, double dy) const noexcept -> TileMove {
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

auto TileMap::raycast(double ox, double oy, double dx, double dy, double max_distance) const noexcept -> TileHit {
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

double TileMap::pixel_width() const noexcept {
    unsigned int w = 0;
    for (const TileLayer& l : m_layers) w = std::max(w, l.width());
    return static_cast<double>(w) * m_tile_w;
}

double TileMap::pixel_height() const noexcept {
    unsigned int h = 0;
    for (const TileLayer& l : m_layers) h = std::max(h, l.height());
    return static_cast<double>(h) * m_tile_h;
}

bool TileMap::parse_csv(const std::string& text, std::vector<std::uint32_t>& out, unsigned int& w, unsigned int& h) {
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

auto TileMap::add_layer_csv(const std::string& name, const std::string& csv, std::int64_t id_offset) -> TileLayer* {
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

bool TileMap::load_tiled_json(const std::filesystem::path& path, TileMap& out, std::string* error) {
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

} // namespace graphics
} // namespace fizmo
