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

    const std::vector<Node2D*>& ordered() {
        if (m_order_dirty || m_draw_order.size() != m_children.size()) {
            m_draw_order.clear();
            for (auto& c : m_children) m_draw_order.push_back(c.get());
            std::stable_sort(m_draw_order.begin(), m_draw_order.end(), [](const Node2D* a, const Node2D* b) { return a->m_z < b->m_z; });
            m_order_dirty = false;
        }
        return m_draw_order;
    }

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

    const math::Matrix3d& local_transform() const noexcept {
        if (m_local_dirty) {
            const double rad = m_rotation * constants::pi_180();
            const double c = std::cos(rad), s = std::sin(rad);
            const double a = c * m_scale_x, b = -s * m_scale_y, d = s * m_scale_x, e = c * m_scale_y;
            m_local = math::Matrix3d{ a, b, m_x - (a * m_origin_x + b * m_origin_y), d, e, m_y - (d * m_origin_x + e * m_origin_y), 0.0, 0.0, 1.0 };
            m_local_dirty = false;
        }
        return m_local;
    }

    const math::Matrix3d& world_transform() const noexcept {
        if (m_world_dirty || m_local_dirty) {
            m_world = m_parent ? m_parent->world_transform() * local_transform() : local_transform();
            m_world_dirty = false;
        }
        return m_world;
    }

    vector2d local_to_world(double lx, double ly) const noexcept {
        const auto& m = world_transform().data;
        return { m[0] * lx + m[1] * ly + m[2], m[3] * lx + m[4] * ly + m[5] };
    }

    vector2d world_to_local(double wx, double wy) const noexcept {
        const auto& m = world_transform().data;
        const double det = m[0] * m[4] - m[1] * m[3];
        if (std::abs(det) < 1e-12) return { 0.0, 0.0 };
        const double dx = wx - m[2], dy = wy - m[5];
        return { (m[4] * dx - m[1] * dy) / det, (-m[3] * dx + m[0] * dy) / det };
    }

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

    Node2D& adopt(std::unique_ptr<Node2D> node) {
        Node2D& ref = *node;
        node->m_parent = this;
        node->touch();
        m_children.push_back(std::move(node));
        m_order_dirty = true;
        return ref;
    }

    std::unique_ptr<Node2D> detach(Node2D* child) {
        for (auto it = m_children.begin(); it != m_children.end(); ++it) {
            if (it->get() != child) continue;
            std::unique_ptr<Node2D> out = std::move(*it);
            m_children.erase(it);
            out->m_parent = nullptr;
            out->touch();
            m_order_dirty = true;
            return out;
        }
        return nullptr;
    }

    bool remove_child(Node2D* child) { return detach(child) != nullptr; }
    void clear_children() { m_children.clear(); m_draw_order.clear(); }

    bool reparent(Node2D& new_parent, bool keep_world = true) {
        if (!m_parent || &new_parent == this) return false;
        for (const Node2D* p = &new_parent; p; p = p->m_parent) if (p == this) return false;
        const math::Matrix3d world = world_transform();
        std::unique_ptr<Node2D> self = m_parent->detach(this);
        new_parent.adopt(std::move(self));

        if (keep_world) {
            const math::Matrix3d& pw = new_parent.world_transform();
            const auto& p = pw.data;
            const double det = p[0] * p[4] - p[1] * p[3];
            if (std::abs(det) > 1e-12) {
                const double ia = p[4] / det, ib = -p[1] / det, id = -p[3] / det, ie = p[0] / det;
                const double itx = -(ia * p[2] + ib * p[5]), ity = -(id * p[2] + ie * p[5]);
                const auto& w = world.data;
                const double a = ia * w[0] + ib * w[3], b = ia * w[1] + ib * w[4];
                const double d = id * w[0] + ie * w[3], e = id * w[1] + ie * w[4];
                const double tx = ia * w[2] + ib * w[5] + itx, ty = id * w[2] + ie * w[5] + ity;
                m_scale_x = std::hypot(a, d);
                const double rot = std::atan2(d, a);
                m_rotation = rot / constants::pi_180();
                const double c = std::cos(rot), s = std::sin(rot);
                m_scale_y = c * e - s * b;
                m_x = tx + (a * m_origin_x + b * m_origin_y);
                m_y = ty + (d * m_origin_x + e * m_origin_y);
                touch();
            }
        }
        return true;
    }

    Node2D* find(const std::string& name) noexcept {
        if (m_name == name) return this;
        for (auto& c : m_children) if (Node2D* f = c->find(name)) return f;
        return nullptr;
    }

    template <typename T>
    T* find_as(const std::string& name) noexcept { return dynamic_cast<T*>(find(name)); }

    template <typename Fn>
    void for_each(Fn&& fn) {
        fn(*this);
        for (auto& c : m_children) c->for_each(fn);
    }

    void update(double dt) {
        if (!m_active) return;
        update_self(dt);
        if (m_on_update) m_on_update(*this, dt);
        for (std::size_t i = 0; i < m_children.size(); ++i) m_children[i]->update(dt);
    }

    void draw(windows::Renderer& r, float parent_opacity = 1.0f) {
        if (!m_visible) return;
        const float opacity = parent_opacity * m_opacity;
        if (opacity <= 0.0f) return;
        r.push_transform();
        r.transform(local_transform());
        draw_self(r, opacity);
        if (m_on_draw) m_on_draw(*this, r);
        for (Node2D* c : ordered()) c->draw(r, opacity);
        r.pop_transform();
    }

    NodeBounds world_bounds() const {
        const NodeBounds b = local_bounds();
        if (!b.valid) return b;
        const vector2d c[4] = { local_to_world(b.x, b.y), local_to_world(b.x + b.w, b.y), local_to_world(b.x + b.w, b.y + b.h), local_to_world(b.x, b.y + b.h) };
        NodeBounds out;
        double x0 = c[0].x, x1 = c[0].x, y0 = c[0].y, y1 = c[0].y;
        for (int i = 1; i < 4; ++i) { x0 = std::min(x0, c[i].x); x1 = std::max(x1, c[i].x); y0 = std::min(y0, c[i].y); y1 = std::max(y1, c[i].y); }
        out.x = x0; out.y = y0; out.w = x1 - x0; out.h = y1 - y0; out.valid = true;
        return out;
    }

    bool hit(double wx, double wy) const {
        const NodeBounds b = local_bounds();
        if (!b.valid) return false;
        const vector2d l = world_to_local(wx, wy);
        return b.contains(l.x, l.y);
    }

    Node2D* pick(double wx, double wy) {
        if (!m_visible) return nullptr;
        const std::vector<Node2D*>& order = ordered();
        for (auto it = order.rbegin(); it != order.rend(); ++it) if (Node2D* p = (*it)->pick(wx, wy)) return p;
        return hit(wx, wy) ? this : nullptr;
    }
};

class SpriteNode : public Node2D {
private:
    Sprite m_sprite;

protected:
    void draw_self(windows::Renderer& r, float opacity) override {
        if (opacity >= 1.0f) { r.draw_sprite(m_sprite); return; }
        Sprite copy = m_sprite;
        copy.set_opacity(m_sprite.opacity() * opacity);
        r.draw_sprite(copy);
    }

    NodeBounds local_bounds() const override {
        if (!m_sprite.drawable()) return {};
        const auto b = m_sprite.rotated_bounds();
        return { b.x, b.y, b.w, b.h, true };
    }

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
    void draw_self(windows::Renderer& r, float opacity) override {
        Paint p = m_paint;
        p.set_opacity(m_paint.opacity() * opacity);
        switch (m_kind) {
            case Kind::Rect:        r.draw_path(Path2D::rounded_rect(0, 0, m_w, m_h, 0.0), p); break;
            case Kind::RoundedRect: r.draw_rounded_rect(0, 0, m_w, m_h, m_radius, p); break;
            case Kind::Ellipse:     r.draw_path(Path2D::ellipse(m_w * 0.5, m_h * 0.5, m_w * 0.5, m_h * 0.5), p); break;
            case Kind::Path:        r.draw_path(m_path, p); break;
        }
    }

    NodeBounds local_bounds() const override { return { 0.0, 0.0, m_w, m_h, m_kind != Kind::Path }; }

public:
    ShapeNode() = default;
    static std::unique_ptr<ShapeNode> rect(double w, double h, const Paint& p) { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Rect; n->m_w = w; n->m_h = h; n->m_paint = p; return n; }
    static std::unique_ptr<ShapeNode> rounded(double w, double h, double r, const Paint& p) { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::RoundedRect; n->m_w = w; n->m_h = h; n->m_radius = r; n->m_paint = p; return n; }
    static std::unique_ptr<ShapeNode> ellipse(double w, double h, const Paint& p) { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Ellipse; n->m_w = w; n->m_h = h; n->m_paint = p; return n; }
    static std::unique_ptr<ShapeNode> path(Path2D path, const Paint& p) { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Path; n->m_path = std::move(path); n->m_paint = p; return n; }

    Paint& paint() noexcept { return m_paint; }
    void set_size(double w, double h) noexcept { m_w = w; m_h = h; }
};

class TextNode : public Node2D {
private:
    std::string     m_text;
    text::TextStyle m_style;

protected:
    void draw_self(windows::Renderer& r, float opacity) override {
        if (m_text.empty()) return;
        text::TextStyle st = m_style;
        if (opacity < 1.0f) st.set_opacity((m_style.has_opacity() ? m_style.opacity() : 1.0f) * opacity);
        r.draw_text(0, 0, m_text, st);
    }

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

    Node2D* pick_screen(double sx, double sy) {
        if (!m_camera) return m_root.pick(sx, sy);
        const vector2d w = m_camera->screen_to_world(sx, sy);
        return m_root.pick(w.x, w.y);
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SCENE_GRAPH_2D_HPP
