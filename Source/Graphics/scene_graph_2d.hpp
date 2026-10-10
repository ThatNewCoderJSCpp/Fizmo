#ifndef FIZMO_SCENE_GRAPH_2D_HPP
#define FIZMO_SCENE_GRAPH_2D_HPP

#include "camera_2d.hpp"
#include "sprite.hpp"
#include "../Windows/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace graphics {

struct NodeBounds {
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    bool   valid = false;
    bool contains(double px, double py) const noexcept { return valid && px >= x && py >= y && px < x + w && py < y + h; }
};

class Node2D {
public:
    using UpdateFn = std::function<void(Node2D&, double)>;
    using DrawFn   = std::function<void(Node2D&, windows::Renderer&)>;

private:
    std::string                          m_name;
    Node2D*                              m_parent = nullptr;
    std::vector<std::unique_ptr<Node2D>> m_children;
    double                               m_x = 0.0, m_y = 0.0;
    double                               m_rotation = 0.0;
    double                               m_scale_x = 1.0, m_scale_y = 1.0;
    double                               m_origin_x = 0.0, m_origin_y = 0.0;
    int                                  m_z = 0;
    bool                                 m_visible = true;
    bool                                 m_active = true;
    float                                m_opacity = 1.0f;
    mutable math::Matrix3d               m_local;
    mutable math::Matrix3d               m_world;
    mutable bool                         m_local_dirty = true;
    mutable bool                         m_world_dirty = true;
    bool                                 m_order_dirty = false;
    std::vector<Node2D*>                 m_draw_order;
    UpdateFn                             m_on_update;
    DrawFn                               m_on_draw;

    void touch_world() noexcept {
        if (m_world_dirty) return;
        m_world_dirty = true;
        for (auto& c : m_children) c->touch_world();
    }

    void touch() noexcept {
        m_local_dirty = true;
        m_world_dirty = false;
        touch_world();
    }

    const std::vector<Node2D*>& ordered();

protected:
    virtual void update_self(double) {}
    virtual void draw_self(windows::Renderer&, float) {}
    virtual NodeBounds local_bounds() const { return {}; }

public:
    Node2D() = default;
    explicit Node2D(std::string name) : m_name(std::move(name)) {}
    virtual ~Node2D() = default;
    Node2D(const Node2D&) = delete;
    Node2D& operator=(const Node2D&) = delete;

    const std::string& name() const noexcept { return m_name; }
    void set_name(std::string n) { m_name = std::move(n); }
    Node2D* parent() const noexcept { return m_parent; }
    const std::vector<std::unique_ptr<Node2D>>& children() const noexcept { return m_children; }
    std::size_t child_count() const noexcept { return m_children.size(); }

    double x() const noexcept { return m_x; }
    double y() const noexcept { return m_y; }
    vector2d position() const noexcept { return { m_x, m_y }; }
    double rotation() const noexcept { return m_rotation; }
    double scale_x() const noexcept { return m_scale_x; }
    double scale_y() const noexcept { return m_scale_y; }
    int z_order() const noexcept { return m_z; }
    bool visible() const noexcept { return m_visible; }
    bool active() const noexcept { return m_active; }
    float opacity() const noexcept { return m_opacity; }

    Node2D& set_position(double x, double y) noexcept { m_x = x; m_y = y; touch(); return *this; }
    Node2D& translate(double dx, double dy) noexcept { m_x += dx; m_y += dy; touch(); return *this; }
    Node2D& set_rotation(double degrees) noexcept { m_rotation = degrees; touch(); return *this; }
    Node2D& rotate(double degrees) noexcept { m_rotation += degrees; touch(); return *this; }
    Node2D& set_scale(double s) noexcept { m_scale_x = m_scale_y = s; touch(); return *this; }
    Node2D& set_scale(double sx, double sy) noexcept { m_scale_x = sx; m_scale_y = sy; touch(); return *this; }
    Node2D& set_origin(double ox, double oy) noexcept { m_origin_x = ox; m_origin_y = oy; touch(); return *this; }
    Node2D& set_z_order(int z) noexcept { m_z = z; if (m_parent) m_parent->m_order_dirty = true; return *this; }
    Node2D& set_visible(bool v) noexcept { m_visible = v; return *this; }
    Node2D& set_active(bool a) noexcept { m_active = a; return *this; }
    Node2D& set_opacity(float o) noexcept { m_opacity = std::max(0.0f, std::min(1.0f, o)); return *this; }
    Node2D& on_update(UpdateFn fn) { m_on_update = std::move(fn); return *this; }
    Node2D& on_draw(DrawFn fn) { m_on_draw = std::move(fn); return *this; }

    const math::Matrix3d& local_transform() const noexcept;

    const math::Matrix3d& world_transform() const noexcept;

    vector2d local_to_world(double lx, double ly) const noexcept {
        const auto& m = world_transform().data;
        return { m[0] * lx + m[1] * ly + m[2], m[3] * lx + m[4] * ly + m[5] };
    }

    vector2d world_to_local(double wx, double wy) const noexcept;

    vector2d world_position() const noexcept { return local_to_world(m_origin_x, m_origin_y); }

    double world_rotation() const noexcept {
        const auto& m = world_transform().data;
        return std::atan2(m[3], m[0]) / constants::pi_180();
    }

    float world_opacity() const noexcept { return m_parent ? m_parent->world_opacity() * m_opacity : m_opacity; }

    template <typename T, typename... Args>
    T& add_child(Args&&... args) {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *node;
        adopt(std::move(node));
        return ref;
    }

    Node2D& adopt(std::unique_ptr<Node2D> node);

    std::unique_ptr<Node2D> detach(Node2D* child);

    bool remove_child(Node2D* child) { return detach(child) != nullptr; }
    void clear_children() { m_children.clear(); m_draw_order.clear(); }

    bool reparent(Node2D& new_parent, bool keep_world = true);

    Node2D* find(const std::string& name) noexcept;

    template <typename T>
    T* find_as(const std::string& name) noexcept { return dynamic_cast<T*>(find(name)); }

    template <typename Fn>
    void for_each(Fn&& fn) {
        fn(*this);
        for (auto& c : m_children) c->for_each(fn);
    }

    void update(double dt);

    void draw(windows::Renderer& r, float parent_opacity = 1.0f);

    NodeBounds world_bounds() const;

    bool hit(double wx, double wy) const;

    Node2D* pick(double wx, double wy);
};

class SpriteNode : public Node2D {
private:
    Sprite m_sprite;

protected:
    void draw_self(windows::Renderer& r, float opacity) override;

    NodeBounds local_bounds() const override;

public:
    SpriteNode() = default;
    explicit SpriteNode(const Texture& tex, std::string name = std::string()) : Node2D(std::move(name)), m_sprite(tex) { m_sprite.set_position(0, 0); }
    Sprite& sprite() noexcept { return m_sprite; }
    const Sprite& sprite() const noexcept { return m_sprite; }
};

class ShapeNode : public Node2D {
public:
    enum class Kind : std::uint8_t { Rect = 0, RoundedRect, Ellipse, Path };

private:
    Kind   m_kind = Kind::Rect;
    double m_w = 0.0, m_h = 0.0, m_radius = 0.0;
    Paint  m_paint = Paint::fill(Color(255, 255, 255));
    Path2D m_path;

protected:
    void draw_self(windows::Renderer& r, float opacity) override;

    NodeBounds local_bounds() const override { return { 0.0, 0.0, m_w, m_h, m_kind != Kind::Path }; }

public:
    ShapeNode() = default;
    static std::unique_ptr<ShapeNode> rect(double w, double h, const Paint& p);
    static std::unique_ptr<ShapeNode> rounded(double w, double h, double r, const Paint& p);
    static std::unique_ptr<ShapeNode> ellipse(double w, double h, const Paint& p);
    static std::unique_ptr<ShapeNode> path(Path2D path, const Paint& p);

    Paint& paint() noexcept { return m_paint; }
    void set_size(double w, double h) noexcept { m_w = w; m_h = h; }
};

class TextNode : public Node2D {
private:
    std::string     m_text;
    text::TextStyle m_style;

protected:
    void draw_self(windows::Renderer& r, float opacity) override;

public:
    TextNode() = default;
    TextNode(std::string text, const text::TextStyle& style) : m_text(std::move(text)), m_style(style) {}
    void set_text(std::string t) { m_text = std::move(t); }
    const std::string& text() const noexcept { return m_text; }
    text::TextStyle& style() noexcept { return m_style; }
};

class SceneGraph2D {
private:
    Node2D    m_root{ "root" };
    Camera2D* m_camera = nullptr;

public:
    Node2D& root() noexcept { return m_root; }
    void set_camera(Camera2D* camera) noexcept { m_camera = camera; }
    Camera2D* camera() const noexcept { return m_camera; }

    void update(double dt) { m_root.update(dt); }

    void draw(windows::Renderer& r) {
        if (m_camera) r.begin_2d(*m_camera);
        m_root.draw(r);
        if (m_camera) r.end_2d();
    }

    Node2D* pick_screen(double sx, double sy);
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SCENE_GRAPH_2D_HPP
