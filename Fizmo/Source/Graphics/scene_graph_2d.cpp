#include "fizmo_library.hpp"
#include "scene_graph_2d.hpp"

namespace fizmo {
namespace graphics {

auto Node2D::ordered() -> const std::vector<Node2D*>& {
    if (m_order_dirty || m_draw_order.size() != m_children.size()) {
        m_draw_order.clear();
        for (auto& c : m_children) m_draw_order.push_back(c.get());
        std::stable_sort(m_draw_order.begin(), m_draw_order.end(), [](const Node2D* a, const Node2D* b) { return a->m_z < b->m_z; });
        m_order_dirty = false;
    }
    return m_draw_order;
}

auto Node2D::local_transform() const noexcept -> const math::Matrix3d& {
    if (m_local_dirty) {
        const double rad = m_rotation * constants::pi_180();
        const double c = std::cos(rad), s = std::sin(rad);
        const double a = c * m_scale_x, b = -s * m_scale_y, d = s * m_scale_x, e = c * m_scale_y;
        m_local = math::Matrix3d{ a, b, m_x - (a * m_origin_x + b * m_origin_y), d, e, m_y - (d * m_origin_x + e * m_origin_y), 0.0, 0.0, 1.0 };
        m_local_dirty = false;
    }
    return m_local;
}

auto Node2D::world_transform() const noexcept -> const math::Matrix3d& {
    if (m_world_dirty || m_local_dirty) {
        m_world = m_parent ? m_parent->world_transform() * local_transform() : local_transform();
        m_world_dirty = false;
    }
    return m_world;
}

auto Node2D::world_to_local(double wx, double wy) const noexcept -> vector2d {
    const auto& m = world_transform().data;
    const double det = m[0] * m[4] - m[1] * m[3];
    if (std::abs(det) < 1e-12) return { 0.0, 0.0 };
    const double dx = wx - m[2], dy = wy - m[5];
    return { (m[4] * dx - m[1] * dy) / det, (-m[3] * dx + m[0] * dy) / det };
}

auto Node2D::adopt(std::unique_ptr<Node2D> node) -> Node2D& {
    Node2D& ref = *node;
    node->m_parent = this;
    node->touch();
    m_children.push_back(std::move(node));
    m_order_dirty = true;
    return ref;
}

auto Node2D::detach(Node2D* child) -> std::unique_ptr<Node2D> {
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

bool Node2D::reparent(Node2D& new_parent, bool keep_world) {
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

auto Node2D::find(const std::string& name) noexcept -> Node2D* {
    if (m_name == name) return this;
    for (auto& c : m_children) if (Node2D* f = c->find(name)) return f;
    return nullptr;
}

void Node2D::update(double dt) {
    if (!m_active) return;
    update_self(dt);
    if (m_on_update) m_on_update(*this, dt);
    for (std::size_t i = 0; i < m_children.size(); ++i) m_children[i]->update(dt);
}

void Node2D::draw(windows::Renderer& r, float parent_opacity) {
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

auto Node2D::world_bounds() const -> NodeBounds {
    const NodeBounds b = local_bounds();
    if (!b.valid) return b;
    const vector2d c[4] = { local_to_world(b.x, b.y), local_to_world(b.x + b.w, b.y), local_to_world(b.x + b.w, b.y + b.h), local_to_world(b.x, b.y + b.h) };
    NodeBounds out;
    double x0 = c[0].x, x1 = c[0].x, y0 = c[0].y, y1 = c[0].y;
    for (int i = 1; i < 4; ++i) { x0 = std::min(x0, c[i].x); x1 = std::max(x1, c[i].x); y0 = std::min(y0, c[i].y); y1 = std::max(y1, c[i].y); }
    out.x = x0; out.y = y0; out.w = x1 - x0; out.h = y1 - y0; out.valid = true;
    return out;
}

bool Node2D::hit(double wx, double wy) const {
    const NodeBounds b = local_bounds();
    if (!b.valid) return false;
    const vector2d l = world_to_local(wx, wy);
    return b.contains(l.x, l.y);
}

auto Node2D::pick(double wx, double wy) -> Node2D* {
    if (!m_visible) return nullptr;
    const std::vector<Node2D*>& order = ordered();
    for (auto it = order.rbegin(); it != order.rend(); ++it) if (Node2D* p = (*it)->pick(wx, wy)) return p;
    return hit(wx, wy) ? this : nullptr;
}

void SpriteNode::draw_self(windows::Renderer& r, float opacity) {
    if (opacity >= 1.0f) { r.draw_sprite(m_sprite); return; }
    Sprite copy = m_sprite;
    copy.set_opacity(m_sprite.opacity() * opacity);
    r.draw_sprite(copy);
}

auto SpriteNode::local_bounds() const -> NodeBounds {
    if (!m_sprite.drawable()) return {};
    const auto b = m_sprite.rotated_bounds();
    return { b.x, b.y, b.w, b.h, true };
}

void ShapeNode::draw_self(windows::Renderer& r, float opacity) {
    Paint p = m_paint;
    p.set_opacity(m_paint.opacity() * opacity);
    switch (m_kind) {
        case Kind::Rect:        r.draw_path(Path2D::rounded_rect(0, 0, m_w, m_h, 0.0), p); break;
        case Kind::RoundedRect: r.draw_rounded_rect(0, 0, m_w, m_h, m_radius, p); break;
        case Kind::Ellipse:     r.draw_path(Path2D::ellipse(m_w * 0.5, m_h * 0.5, m_w * 0.5, m_h * 0.5), p); break;
        case Kind::Path:        r.draw_path(m_path, p); break;
    }
}

auto ShapeNode::rect(double w, double h, const Paint& p) -> std::unique_ptr<ShapeNode> { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Rect; n->m_w = w; n->m_h = h; n->m_paint = p; return n; }

auto ShapeNode::rounded(double w, double h, double r, const Paint& p) -> std::unique_ptr<ShapeNode> { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::RoundedRect; n->m_w = w; n->m_h = h; n->m_radius = r; n->m_paint = p; return n; }

auto ShapeNode::ellipse(double w, double h, const Paint& p) -> std::unique_ptr<ShapeNode> { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Ellipse; n->m_w = w; n->m_h = h; n->m_paint = p; return n; }

auto ShapeNode::path(Path2D path, const Paint& p) -> std::unique_ptr<ShapeNode> { auto n = std::make_unique<ShapeNode>(); n->m_kind = Kind::Path; n->m_path = std::move(path); n->m_paint = p; return n; }

void TextNode::draw_self(windows::Renderer& r, float opacity) {
    if (m_text.empty()) return;
    text::TextStyle st = m_style;
    if (opacity < 1.0f) st.set_opacity((m_style.has_opacity() ? m_style.opacity() : 1.0f) * opacity);
    r.draw_text(0, 0, m_text, st);
}

auto SceneGraph2D::pick_screen(double sx, double sy) -> Node2D* {
    if (!m_camera) return m_root.pick(sx, sy);
    const vector2d w = m_camera->screen_to_world(sx, sy);
    return m_root.pick(w.x, w.y);
}

} // namespace graphics
} // namespace fizmo
