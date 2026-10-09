#include "fizmo_library.hpp"
#include "renderer.hpp"

namespace fizmo {
namespace windows {

bool Renderer::bind() noexcept {
    if (!m_window) return false;
    void* handle = m_window->native_handle();
    if (!handle) return false;
    if (m_impl) release();

#if defined(OS_WINDOWS) || defined(OS_LINUX)
    const bool any_gpu = m_requested == RendererBackend::Auto || m_requested == RendererBackend::GPU;
    if (any_gpu || m_requested == RendererBackend::Vulkan) try_backend(handle, [] { return detail::make_gpu_renderer_impl(gpu::Backend::Vulkan); });
    if (!m_impl && (any_gpu || m_requested == RendererBackend::OpenGL)) try_backend(handle, [] { return detail::make_gpu_renderer_impl(gpu::Backend::OpenGL); });
    if (!m_impl && (m_requested == RendererBackend::Auto || m_requested == RendererBackend::Software)) try_backend(handle, [] { return detail::make_software_renderer_impl(); });
#endif

    if (!m_impl) return false;
    for (PassRecord& p : m_passes) { try { p.impl_id = m_impl->add_gpu_pass(p.stage, p.callback); } catch (...) { p.impl_id = 0; } }
    if (m_impl->is_gpu()) m_window->set_background_erase(false);
    m_window->set_paint_callback([this](void* dc) { if (m_impl) m_impl->paint(dc); });
    m_window->add_event_listener(WindowEventType::WindowResize, [this](const WindowEvent& e) { if (m_impl) m_impl->resize(e.x, e.y); });
    return true;
}

void Renderer::add_frame_observer(const std::shared_ptr<FrameObserver>& observer) {
    if (!observer) return;
    for (const auto& w : m_observers) if (w.lock() == observer) return;
    m_observers.push_back(observer);
}

void Renderer::remove_frame_observer(const FrameObserver* observer) noexcept {
    for (std::size_t i = 0; i < m_observers.size();) {
        const auto o = m_observers[i].lock();
        if (!o || o.get() == observer) m_observers.erase(m_observers.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
}

auto Renderer::backend() const noexcept -> RendererBackend {
    if (!m_impl) return m_requested;
    return m_impl->is_gpu() ? RendererBackend::GPU : RendererBackend::Software;
}

auto Renderer::active_backend() const noexcept -> RendererBackend {
    if (!m_impl) return m_requested;
    const std::string_view name = m_impl->backend_name();
    if (name == "vulkan") return RendererBackend::Vulkan;
    if (name == "opengl") return RendererBackend::OpenGL;
    return RendererBackend::Software;
}

void Renderer::begin_frame() noexcept {
    m_xform_stack.clear();
    reset_transform();
    m_frame_begin = FrameClock::now();
    m_frame_open  = true;
    if (m_impl) m_impl->begin_frame();
}

void Renderer::present() noexcept {
    if (!m_impl) return;
    const FrameClock::time_point start = FrameClock::now();
    while (!m_target_frames.empty()) end_target();
    if (m_impl->in_3d()) m_impl->end_3d();
    m_impl->present();
    finish_frame(start);
}

void Renderer::end_target() noexcept {
    if (m_target_frames.empty()) return;
    if (m_impl) {
        if (m_impl->in_3d()) m_impl->end_3d();
        m_impl->end_target();
    }
    TargetFrame& f = m_target_frames.back();
    m_xform = f.xform;
    m_xform_stack = std::move(f.stack);
    m_xform_kind = static_cast<XformKind>(f.kind);
    m_target_frames.pop_back();
}

void Renderer::draw_target(const graphics::RenderTarget& target, int dx, int dy, unsigned int dw, unsigned int dh, float opacity, const graphics::TextureRect& src) noexcept {
    if (!m_impl || !target.valid()) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_target(dx, dy, dw, dh, target, opacity, src); return; }

    if (axis_aligned_positive()) {
        const ScreenRect r = map_rect_bounds(dx, dy, dw, dh);
        m_impl->draw_target(r.x, r.y, r.w, r.h, target, opacity, src);
        return;
    }

    const double x0 = dx, y0 = dy, x1 = dx + static_cast<double>(dw), y1 = dy + static_cast<double>(dh);
    const RenderPoint quad[4] = { mapped(x0, y0), mapped(x1, y0), mapped(x1, y1), mapped(x0, y1) };
    m_impl->draw_target_quad(quad, target, opacity, src);
}

std::uint64_t Renderer::add_gpu_pass(GpuPassStage stage, GpuPassCallback callback) {
    PassRecord p;
    p.id = m_next_pass++;
    p.stage = stage;
    p.callback = std::move(callback);
    if (m_impl) p.impl_id = m_impl->add_gpu_pass(p.stage, p.callback);
    m_passes.push_back(std::move(p));
    return m_passes.back().id;
}

bool Renderer::remove_gpu_pass(std::uint64_t id) noexcept {
    for (auto it = m_passes.begin(); it != m_passes.end(); ++it) {
        if (it->id != id) continue;
        if (m_impl && it->impl_id) m_impl->remove_gpu_pass(it->impl_id);
        m_passes.erase(it);
        return true;
    }
    return false;
}

void Renderer::pop_transform() noexcept {
    if (m_xform_stack.empty()) return;
    m_xform = m_xform_stack.back();
    m_xform_stack.pop_back();
    classify_transform();
}

auto Renderer::inverse_map_point(double sx, double sy) const noexcept -> vector2d {
    const auto& m = m_xform.data;
    const double det = m[0] * m[4] - m[1] * m[3];
    if (std::abs(det) <= 1e-12) return { sx, sy };
    const double px = sx - m[2], py = sy - m[5], inv = 1.0 / det;
    return { (m[4] * px - m[1] * py) * inv, (-m[3] * px + m[0] * py) * inv };
}

void Renderer::draw_framebuffer(
    const graphics::Framebuffer& fb,
    int x, int y, unsigned int w, unsigned int h,
    bool smooth 
) noexcept {
    if (!m_impl || fb.width() == 0 || fb.height() == 0) return;
    const ScreenRect r = map_rect_bounds(x, y, w, h);
    if (r.w && r.h) m_impl->draw_pixel_buffer(r.x, r.y, r.w, r.h, fb.data(), fb.width(), fb.height(), smooth, fb.version());
}

void Renderer::set_clip_rect(int x, int y, unsigned int w, unsigned int h, RectOrigin origin) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->set_clip_rect(x, y, w, h, origin); return; }
    int rx = x, ry = y;
    resolve_rect_origin(rx, ry, x, y, w, h, origin);
    const ScreenRect r = map_rect_bounds(rx, ry, w, h);
    m_impl->set_clip_rect(r.x, r.y, r.w, r.h, RectOrigin::TopLeft);
}

void Renderer::draw_pixel(int x, int y, const fizmo::graphics::Color& color) noexcept {
    if (!m_impl) return;

    if (m_xform_kind <= XformKind::Translate) {
        const vector2d p = map_point(x, y);
        m_impl->draw_pixel(round_i(p.x), round_i(p.y), color);
        return;
    }

    draw_rect(x, y, 1, 1, graphics::Paint::fill(color));
}

void Renderer::draw_line(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_line(x1, y1, x2, y2, paint); return; }
    const vector2d a = map_point(x1, y1), b = map_point(x2, y2);
    m_impl->draw_line(round_i(a.x), round_i(a.y), round_i(b.x), round_i(b.y), paint);
}

void Renderer::draw_rect(int x, int y, unsigned int w, unsigned int h, const graphics::Paint& paint) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_rect(x, y, w, h, paint); return; }
    int rx = x, ry = y;
    resolve_rect_origin(rx, ry, x, y, w, h, paint.origin());
    graphics::Paint p = paint;
    p.set_origin(RectOrigin::TopLeft);

    if (m_xform_kind != XformKind::General) {
        const ScreenRect r = map_rect_bounds(rx, ry, w, h);
        m_impl->draw_rect(r.x, r.y, r.w, r.h, p);
        return;
    }

    const double x0 = rx, y0 = ry, x1 = rx + static_cast<double>(w), y1 = ry + static_cast<double>(h);
    const RenderPoint pts[4] = { mapped(x0, y0), mapped(x1, y0), mapped(x1, y1), mapped(x0, y1) };
    m_impl->draw_polygon(pts, 4, p);
}

void Renderer::draw_rect_corners(int x1, int y1, int x2, int y2, const graphics::Paint& paint) noexcept {
    const ResolvedRect r = resolve_corners(x1, y1, x2, y2);
    graphics::Paint p = paint;
    p.set_origin(RectOrigin::TopLeft);
    draw_rect(r.x, r.y, r.w, r.h, p);
}

void Renderer::draw_ellipse(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_ellipse(cx, cy, rx, ry, paint); return; }
    const vector2d c = map_point(cx, cy);

    if (m_xform_kind != XformKind::General) {
        m_impl->draw_ellipse(round_i(c.x), round_i(c.y), scale_len(rx, m_xform.data[0]), scale_len(ry, m_xform.data[4]), paint);
        return;
    }

    build_ellipse(cx, cy, rx, ry);
    m_impl->draw_polygon(m_points.data(), m_points.size(), paint);
}

void Renderer::draw_arc(int cx, int cy, unsigned int rx, unsigned int ry, const graphics::Paint& paint) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_arc(cx, cy, rx, ry, paint); return; }
    const vector2d c = map_point(cx, cy);
    const auto& m = m_xform.data;
    const double sx = m_xform_kind == XformKind::General ? std::sqrt(std::abs(m[0] * m[4] - m[1] * m[3])) : m[0];
    const double sy = m_xform_kind == XformKind::General ? sx : m[4];
    m_impl->draw_arc(round_i(c.x), round_i(c.y), scale_len(rx, sx), scale_len(ry, sy), paint);
}

void Renderer::draw_polyline(const RenderPoint* points, std::size_t count, const graphics::Paint& paint, bool closed) noexcept {
    if (!m_impl || !points) return;
    m_impl->draw_polyline(map_points(points, count), count, closed, paint);
}

void Renderer::draw_path(const graphics::Path2D& path, const graphics::Paint& paint, double tolerance) noexcept {
    if (!m_impl) return;

    try {
        const auto& m = m_xform.data;
        const double s = std::sqrt(std::abs(m[0] * m[4] - m[1] * m[3]));
        const auto contours = path.flatten(s > 1e-12 ? tolerance / s : tolerance);

        for (const auto& contour : contours) {
            if (contour.points.size() < 2) continue;
            m_points.resize(contour.points.size());
            for (std::size_t i = 0; i < contour.points.size(); ++i) m_points[i] = mapped(contour.points[i].x, contour.points[i].y);
            if (paint.has_fill() && m_points.size() >= 3) m_impl->draw_polygon(m_points.data(), m_points.size(), fill_only(paint));
            if (paint.has_stroke()) m_impl->draw_polyline(m_points.data(), m_points.size(), contour.closed, paint);
        }
    } catch (...) {}
}

void Renderer::draw_image(
    const fizmo::images::BitmapImage& img,
    int dx, int dy,
    unsigned int dw, unsigned int dh,
    unsigned int sx, unsigned int sy,
    unsigned int sw, unsigned int sh
) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_image(dx, dy, dw, dh, img, sx, sy, sw, sh); return; }

    if (axis_aligned_positive()) {
        const ScreenRect r = map_rect_bounds(dx, dy, dw, dh);
        m_impl->draw_image(r.x, r.y, r.w, r.h, img, sx, sy, sw, sh);
        return;
    }

    const graphics::Texture tex(std::shared_ptr<const images::BitmapImage>(std::shared_ptr<const void>(), &img));
    emit_texture(tex, dx, dy, dw, dh, 1.0f, graphics::TextureRect(static_cast<int>(sx), static_cast<int>(sy), sw, sh));
}

void Renderer::draw_texture(const graphics::Texture& tex, int dx, int dy, float opacity) noexcept {
    if (tex.valid()) emit_texture(tex, dx, dy, tex.width(), tex.height(), opacity, tex.full_rect());
}

void Renderer::draw_texture_quad(const graphics::Texture& tex, const RenderPoint quad[4], float opacity, const graphics::TextureRect& src) noexcept {
    if (!m_impl || !tex.valid() || !quad) return;
    m_impl->draw_texture_quad(map_points(quad, 4), tex, opacity, src);
}

void Renderer::draw_sprite(const graphics::Sprite& sprite, double offset_x, double offset_y) noexcept {
    if (!m_impl || !sprite.drawable()) return;
    const graphics::SpriteQuad q = sprite.quad();
    RenderPoint quad[4];
    for (int i = 0; i < 4; ++i) quad[i] = mapped(q.x[i] + offset_x, q.y[i] + offset_y);
    const graphics::BlendMode saved = m_impl->blend_mode();
    if (sprite.blend_mode() != saved) m_impl->set_blend_mode(sprite.blend_mode());
    if (sprite.tinted()) m_impl->draw_texture_quad_tinted(quad, sprite.texture(), sprite.tint(), sprite.opacity(), sprite.source_rect());
    else m_impl->draw_texture_quad(quad, sprite.texture(), sprite.opacity(), sprite.source_rect());
    if (sprite.blend_mode() != saved) m_impl->set_blend_mode(saved);
}

void Renderer::draw_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, const graphics::TextureRect& src, const graphics::Color& tint, float opacity) noexcept {
    if (!m_impl || !tex.valid()) return;
    if (graphics::is_white(tint)) { emit_texture(tex, dx, dy, dw, dh, opacity, src); return; }
    const double x0 = dx, y0 = dy, x1 = dx + static_cast<double>(dw), y1 = dy + static_cast<double>(dh);
    const RenderPoint quad[4] = { mapped(x0, y0), mapped(x1, y0), mapped(x1, y1), mapped(x0, y1) };
    m_impl->draw_texture_quad_tinted(quad, tex, tint, opacity, src);
}

void Renderer::draw_texture_quad(const graphics::Texture& tex, const RenderPoint quad[4], const graphics::TextureRect& src, const graphics::Color& tint, float opacity) noexcept {
    if (!m_impl || !tex.valid() || !quad) return;
    RenderPoint q[4];
    for (int i = 0; i < 4; ++i) q[i] = mapped(quad[i].x, quad[i].y);
    m_impl->draw_texture_quad_tinted(q, tex, tint, opacity, src);
}

void Renderer::draw_vertices(const graphics::Vertex2D* vertices, std::size_t count, const graphics::Texture* texture) noexcept {
    if (!m_impl || !vertices || count < 3) return;

    try {
        if (m_xform_kind == XformKind::Identity) { m_impl->draw_vertices_2d(vertices, count, texture); return; }
        m_vertex_scratch.assign(vertices, vertices + count);
        for (graphics::Vertex2D& v : m_vertex_scratch) { const RenderPoint p = mapped(v.x, v.y); v.x = p.x; v.y = p.y; }
        m_impl->draw_vertices_2d(m_vertex_scratch.data(), m_vertex_scratch.size(), texture);
    } catch (...) {}
}

unsigned int Renderer::viewport_width() const noexcept { return !m_target_frames.empty() ? m_target_frames.back().width : (m_window ? m_window->width() : 0); }

unsigned int Renderer::viewport_height() const noexcept { return !m_target_frames.empty() ? m_target_frames.back().height : (m_window ? m_window->height() : 0); }

void Renderer::visible_world_rect(double& x0, double& y0, double& x1, double& y1) const noexcept {
    const double w = viewport_width(), h = viewport_height();
    const vector2d c[4] = { inverse_map_point(0.0, 0.0), inverse_map_point(w, 0.0), inverse_map_point(w, h), inverse_map_point(0.0, h) };
    x0 = x1 = c[0].x;
    y0 = y1 = c[0].y;
    for (int i = 1; i < 4; ++i) { x0 = std::min(x0, c[i].x); x1 = std::max(x1, c[i].x); y0 = std::min(y0, c[i].y); y1 = std::max(y1, c[i].y); }
}

void Renderer::draw_nine_slice(const graphics::NineSlice& slice, double x, double y, double w, double h, const graphics::Color& tint, float opacity) noexcept {
    if (!m_impl || !slice.valid()) return;

    try {
        slice.build(x, y, w, h, m_slice_scratch);
        const bool white = graphics::is_white(tint);

        for (const graphics::SliceQuad& q : m_slice_scratch) {
            const RenderPoint quad[4] = { mapped(q.x, q.y), mapped(q.x + q.w, q.y), mapped(q.x + q.w, q.y + q.h), mapped(q.x, q.y + q.h) };
            if (white) m_impl->draw_texture_quad(quad, slice.texture(), opacity, q.source);
            else m_impl->draw_texture_quad_tinted(quad, slice.texture(), tint, opacity, q.source);
        }
    } catch (...) {}
}

std::size_t Renderer::draw_tilemap(const graphics::TileMap& map, double x, double y) noexcept {
    if (!m_impl || map.tile_width() == 0 || map.tile_height() == 0) return 0;
    std::size_t drawn = 0;

    try {
        double vx0 = 0, vy0 = 0, vx1 = 0, vy1 = 0;
        visible_world_rect(vx0, vy0, vx1, vy1);
        const vector2d cam = inverse_map_point(viewport_width() * 0.5, viewport_height() * 0.5);
        const double tw = map.tile_width(), th = map.tile_height();

        for (const graphics::TileLayer& layer : map.layers()) {
            if (!layer.visible() || layer.opacity() <= 0.0f || layer.tint().alpha() == 0) continue;
            const double ox = x + layer.offset_x() + (1.0 - layer.parallax_x()) * cam.x;
            const double oy = y + layer.offset_y() + (1.0 - layer.parallax_y()) * cam.y;
            const int tx0 = std::max(0, static_cast<int>(std::floor((vx0 - ox) / tw)) - 1);
            const int ty0 = std::max(0, static_cast<int>(std::floor((vy0 - oy) / th)) - 1);
            const int tx1 = std::min(static_cast<int>(layer.width()) - 1, static_cast<int>(std::floor((vx1 - ox) / tw)) + 1);
            const int ty1 = std::min(static_cast<int>(layer.height()) - 1, static_cast<int>(std::floor((vy1 - oy) / th)) + 1);
            if (tx1 < tx0 || ty1 < ty0) continue;
            m_sprite_scratch.clear();
            const graphics::Color& tint = layer.tint();

            for (int ty = ty0; ty <= ty1; ++ty) {
                for (int tx = tx0; tx <= tx1; ++tx) {
                    const std::uint32_t raw = layer.get(tx, ty);
                    if ((raw & graphics::kTileIdMask) == 0) continue;
                    const std::uint32_t gid = map.resolve(raw);
                    const graphics::TileSet* set = map.tileset_for(gid);
                    if (!set || !set->texture().valid()) continue;
                    const graphics::TextureRect src = set->rect((gid & graphics::kTileIdMask) - set->first_gid());
                    if (src.is_empty()) continue;
                    const double px = ox + tx * tw, py = oy + ty * th + th - static_cast<double>(src.h);
                    double xs[4] = { px, px + src.w, px + src.w, px };
                    double ys[4] = { py, py, py + src.h, py + src.h };
                    const bool diag = (gid & graphics::kTileFlipDiag) != 0;
                    if (diag) { std::swap(xs[1], xs[3]); std::swap(ys[1], ys[3]); }
                    detail::SpriteInstance s;
                    if (!fill_instance(s, xs, ys, 0.0, 0.0, tint.red(), tint.green(), tint.blue(), tint.alpha(), layer.opacity())) continue;
                    const float iw = 1.0f / set->texture().width(), ih = 1.0f / set->texture().height();
                    s.u0 = src.x * iw;
                    s.v0 = src.y * ih;
                    s.u1 = (src.x + static_cast<float>(src.w)) * iw;
                    s.v1 = (src.y + static_cast<float>(src.h)) * ih;
                    const bool fh = (gid & graphics::kTileFlipH) != 0, fv = (gid & graphics::kTileFlipV) != 0;
                    if (diag ? fv : fh) std::swap(s.u0, s.u1);
                    if (diag ? fh : fv) std::swap(s.v0, s.v1);
                    s.source = src;
                    s.texture = &set->texture();
                    s.blend = layer.blend_mode();
                    m_sprite_scratch.push_back(s);
                }
            }

            drawn += m_sprite_scratch.size();
            if (!m_sprite_scratch.empty()) m_impl->draw_sprite_instances(m_sprite_scratch.data(), m_sprite_scratch.size());
        }
    } catch (...) {}

    return drawn;
}

std::size_t Renderer::draw_particles(const graphics::ParticleEmitter& emitter) noexcept {
    if (!m_impl || emitter.count() == 0) return 0;

    try {
        const graphics::ParticleConfig& cfg = emitter.config();
        const graphics::Texture& tex = cfg.texture.valid() ? cfg.texture : graphics::ParticleEmitter::default_texture();
        const graphics::TextureRect src = cfg.source.is_empty() ? tex.full_rect() : cfg.source;
        const float iw = 1.0f / tex.width(), ih = 1.0f / tex.height();
        const double aspect = src.w ? static_cast<double>(src.h) / src.w : 1.0;
        m_sprite_scratch.clear();

        for (const graphics::Particle& p : emitter.particles()) {
            const float t = p.age / p.life;
            const graphics::Color c = emitter.color_at(t);
            if (c.alpha() == 0) continue;
            const double size = emitter.size_at(p);
            if (size <= 0.0) continue;
            const vector2d at = emitter.world_position(p);
            double angle = p.rotation * constants::pi_180();
            double hw = size * 0.5, hh = size * 0.5 * aspect;

            if (cfg.align_to_velocity || cfg.stretch > 0.0f) {
                if (p.vx != 0.0f || p.vy != 0.0f) angle = std::atan2(p.vy, p.vx) + (cfg.align_to_velocity ? p.rotation * constants::pi_180() : 0.0);
                hw += cfg.stretch * std::hypot(p.vx, p.vy) * 0.5;
            }

            const double ca = std::cos(angle), sa = std::sin(angle);
            const double lx[4] = { -hw, hw, hw, -hw }, ly[4] = { -hh, -hh, hh, hh };
            double xs[4], ys[4];
            for (int k = 0; k < 4; ++k) { xs[k] = at.x + lx[k] * ca - ly[k] * sa; ys[k] = at.y + lx[k] * sa + ly[k] * ca; }
            detail::SpriteInstance s;
            if (!fill_instance(s, xs, ys, 0.0, 0.0, c.red(), c.green(), c.blue(), c.alpha(), 1.0f)) continue;
            s.u0 = src.x * iw;
            s.v0 = src.y * ih;
            s.u1 = (src.x + static_cast<float>(src.w)) * iw;
            s.v1 = (src.y + static_cast<float>(src.h)) * ih;
            s.source = src;
            s.texture = &tex;
            s.blend = cfg.blend;
            m_sprite_scratch.push_back(s);
        }

        if (!m_sprite_scratch.empty()) m_impl->draw_sprite_instances(m_sprite_scratch.data(), m_sprite_scratch.size());
        return m_sprite_scratch.size();
    } catch (...) {
        return 0;
    }
}

void Renderer::draw_trail(graphics::Trail& trail) noexcept {
    if (!m_impl) return;

    try {
        trail.build(m_trail_scratch);
        if (m_trail_scratch.empty()) return;
        BlendScope scope(*this, trail.blend_mode());
        draw_vertices(m_trail_scratch.data(), m_trail_scratch.size(), trail.texture().valid() ? &trail.texture() : nullptr);
    } catch (...) {}
}

void Renderer::draw_rounded_rect(double x, double y, double w, double h, double radius, const graphics::Paint& paint) noexcept {
    if (w <= 0.0 || h <= 0.0) return;
    const double r = std::max(0.0, std::min(radius, std::min(w, h) * 0.5));
    try { draw_path(graphics::Path2D::rounded_rect(x, y, w, h, r), paint); } catch (...) {}
}

void Renderer::draw_rounded_rect(double x, double y, double w, double h, double r_tl, double r_tr, double r_br, double r_bl, const graphics::Paint& paint) noexcept {
    if (w <= 0.0 || h <= 0.0) return;
    try { draw_path(graphics::Path2D::rounded_rect(x, y, w, h, r_tl, r_tr, r_br, r_bl), paint); } catch (...) {}
}

void Renderer::draw_sprites(const graphics::Sprite* const* sprites, std::size_t count) noexcept {
    if (!m_impl || !sprites || count == 0) return;

    try {
        m_sprite_scratch.resize(count);
        std::size_t n = 0;

        for (std::size_t i = 0; i < count; ++i) {
            const graphics::Sprite* sp = sprites[i];
            if (!sp || !sp->drawable()) continue;
            const graphics::SpriteQuad q = sp->quad();
            const graphics::TextureRect& src = sp->source_rect();
            const float tw = static_cast<float>(sp->texture().width()), th = static_cast<float>(sp->texture().height());
            detail::SpriteInstance& s = m_sprite_scratch[n];
            const graphics::Color& tint = sp->tint();
            if (!fill_instance(s, q.x, q.y, 0.0, 0.0, tint.red(), tint.green(), tint.blue(), tint.alpha(), sp->opacity())) continue;
            s.blend = sp->blend_mode();
            s.u0 = src.x / tw;
            s.v0 = src.y / th;
            s.u1 = (src.x + static_cast<float>(src.w)) / tw;
            s.v1 = (src.y + static_cast<float>(src.h)) / th;
            s.source  = src;
            s.texture = &sp->texture();
            ++n;
        }

        m_impl->draw_sprite_instances(m_sprite_scratch.data(), n);
    } catch (...) {}
}

void Renderer::draw_sprites(const std::vector<graphics::Sprite>& sprites) noexcept {
    try {
        m_sprite_pointers.clear();
        for (const graphics::Sprite& s : sprites) m_sprite_pointers.push_back(&s);
        draw_sprites(m_sprite_pointers.data(), m_sprite_pointers.size());
    } catch (...) {}
}

void Renderer::draw_sprite_batch(const graphics::SpriteBatch& batch, double offset_x, double offset_y) noexcept {
    if (!m_impl || batch.empty()) return;

    try {
        batch.draw_order(m_sprite_order);
        const std::vector<graphics::SpriteBatchItem>& items = batch.items();
        const std::vector<graphics::Texture>& textures = batch.textures();
        m_sprite_scratch.resize(m_sprite_order.size());
        std::size_t n = 0;

        for (const std::uint32_t idx : m_sprite_order) {
            const graphics::SpriteBatchItem& it = items[idx];
            if (it.texture >= textures.size()) continue;
            detail::SpriteInstance& s = m_sprite_scratch[n];
            const double xs[4] = { it.x[0], it.x[1], it.x[2], it.x[3] };
            const double ys[4] = { it.y[0], it.y[1], it.y[2], it.y[3] };
            if (!fill_instance(s, xs, ys, offset_x, offset_y, it.tint.red(), it.tint.green(), it.tint.blue(), it.tint.alpha(), it.opacity)) continue;
            s.u0 = it.u0;
            s.v0 = it.v0;
            s.u1 = it.u1;
            s.v1 = it.v1;
            s.source  = it.source;
            s.texture = &textures[it.texture];
            s.blend   = m_blend;
            ++n;
        }

        m_impl->draw_sprite_instances(m_sprite_scratch.data(), n);
    } catch (...) {}
}

bool Renderer::transformed_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept {
    if (!m_text_follow || m_xform_kind == XformKind::Identity || m_xform_kind == XformKind::Translate) return false;
    const auto& d = m_xform.data;
    const float m[6] = { static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2]), static_cast<float>(d[3]), static_cast<float>(d[4]), static_cast<float>(d[5]) };
    try { return m_impl->draw_rich_text_transformed(m, static_cast<float>(x), static_cast<float>(y), w, h, rt); } catch (...) { return false; }
}

std::string Renderer::wide_to_utf8(const wchar_t* s, int len) {
    std::string out;
    if (!s) return out;
    if (len < 0) { len = 0; while (s[len]) ++len; }
    for (int i = 0; i < len; ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(s[i]);
        if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < len) {
            const std::uint32_t lo = static_cast<std::uint32_t>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); ++i; }
        }
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }
    return out;
}

void Renderer::draw_text(int x, int y, const char* utf8, int len, const text::TextStyle& style) noexcept {
    if (!m_impl) return;
    if (utf8 && m_text_follow && m_xform_kind != XformKind::Identity && m_xform_kind != XformKind::Translate) {
        try { if (transformed_text(x, y, 0, 0, text::RichText(std::string(utf8, len < 0 ? std::strlen(utf8) : static_cast<std::size_t>(len)), style))) return; } catch (...) {}
    }
    const vector2d p = map_point(x, y);
    m_impl->draw_text(round_i(p.x), round_i(p.y), utf8, len, style);
}

void Renderer::draw_text(int x, int y, const wchar_t* str, int len, const text::TextStyle& style) noexcept {
    if (!m_impl) return;
    if (str && m_text_follow && m_xform_kind != XformKind::Identity && m_xform_kind != XformKind::Translate) {
        try { if (transformed_text(x, y, 0, 0, text::RichText(wide_to_utf8(str, len), style))) return; } catch (...) {}
    }
    const vector2d p = map_point(x, y);
    m_impl->draw_text(round_i(p.x), round_i(p.y), str, len, style);
}

void Renderer::draw_text(int x, int y, unsigned int w, unsigned int h, const std::string& text, const text::TextStyle& style) noexcept {
    if (!m_impl || text.empty()) return;
    try {
        const text::RichText rt(text, style);
        if (transformed_text(x, y, w, h, rt)) return;
        const vector2d p = map_point(x, y);
        m_impl->draw_rich_text(round_i(p.x), round_i(p.y), w, h, rt);
    } catch (...) {}
}

void Renderer::draw_rich_text(int x, int y, unsigned int w, unsigned int h, const text::RichText& rt) noexcept {
    if (!m_impl) return;
    if (transformed_text(x, y, w, h, rt)) return;
    const vector2d p = map_point(x, y);
    m_impl->draw_rich_text(round_i(p.x), round_i(p.y), w, h, rt);
}

bool Renderer::begin_3d(const graphics::Camera3D& camera, int x, int y, unsigned int w, unsigned int h) noexcept {
    if (!m_impl || w == 0 || h == 0) return false;
    if (m_impl->in_3d()) m_impl->end_3d();
    detail::Scene3D scene;
    m_origin_3d = camera.position();
    math::Matrix4d vp = camera.view_projection_matrix() * translation(m_origin_3d);

    if (camera.depth_range() == graphics::DepthRange::NegativeOneToOne) {
        for (std::size_t c = 0; c < 4; ++c) vp.data[8 + c] = 0.5 * vp.data[8 + c] + 0.5 * vp.data[12 + c];
    }

    for (std::size_t i = 0; i < 16; ++i) scene.view_proj[i] = static_cast<float>(vp.data[i]);
    scene.origin[0] = m_origin_3d.x; scene.origin[1] = m_origin_3d.y; scene.origin[2] = m_origin_3d.z;
    scene.x = x; scene.y = y; scene.width = w; scene.height = h;
    m_impl->begin_3d(scene);
    return m_impl->in_3d();
}

void Renderer::draw_mesh(const graphics::Mesh3D& mesh, const math::Matrix4d& model, const graphics::Material3D& material) noexcept {
    if (!m_impl) return;
    float m[16];
    to_origin_relative(model, m);
    m_impl->draw_mesh_3d(mesh, m, material);
}

void Renderer::draw_quads(const graphics::QuadMesh3D& quads, const math::Matrix4d& model, const graphics::Material3D& material) noexcept {
    if (!m_impl) return;
    float m[16];
    to_origin_relative(model, m);
    m_impl->draw_quads_3d(quads, m, nullptr, material);
}

void Renderer::draw_quads_at(const graphics::QuadMesh3D& quads, const vector3d& offset, const graphics::Material3D& material) noexcept {
    if (!m_impl) return;
    float m[16];
    std::int32_t cell[3];
    to_origin_relative(translation(offset), m);
    cell_origin(offset, cell);
    m_impl->draw_quads_3d(quads, m, cell, material);
}

auto Renderer::upload_mesh(graphics::Mesh3D mesh, bool keep_cpu_copy) -> graphics::MeshHandle3D {
    graphics::MeshHandle3D h = graphics::MeshHandle3D::from(std::move(mesh), keep_cpu_copy);
    if (m_impl && !h.empty()) m_impl->upload_handle_3d(h.slot());
    return h;
}

auto Renderer::upload_quads(graphics::QuadMesh3D quads, bool keep_cpu_copy) -> graphics::MeshHandle3D {
    graphics::MeshHandle3D h = graphics::MeshHandle3D::from(std::move(quads), keep_cpu_copy);
    if (m_impl && !h.empty()) m_impl->upload_handle_3d(h.slot());
    return h;
}

void Renderer::draw_mesh(const graphics::MeshHandle3D& mesh, const math::Matrix4d& model, const graphics::Material3D& material) noexcept {
    if (!m_impl || mesh.empty()) return;
    float m[16];
    to_origin_relative(model, m);
    m_impl->draw_handle_3d(mesh.slot(), m, nullptr, material);
}

void Renderer::draw_mesh_at(const graphics::MeshHandle3D& mesh, const vector3d& offset, const graphics::Material3D& material) noexcept {
    if (!m_impl || mesh.empty()) return;
    float m[16];
    std::int32_t cell[3];
    to_origin_relative(translation(offset), m);
    cell_origin(offset, cell);
    m_impl->draw_handle_3d(mesh.slot(), m, cell, material);
}

void Renderer::draw_quad_batch(const graphics::QuadBatch3D& batch, const graphics::Material3D& material) noexcept {
    if (!m_impl || batch.empty()) return;

    try {
        std::int64_t camera_cell[3];
        float camera_frac[3];

        for (int a = 0; a < 3; ++a) {
            const double c = axis(m_origin_3d, a), f = std::floor(c);
            camera_cell[a] = static_cast<std::int64_t>(f);
            camera_frac[a] = static_cast<float>(c - f);
        }

        m_batch_scratch.clear();
        m_batch_scratch.reserve(batch.size());

        for (const graphics::QuadBatch3D::Item& item : batch.items()) {
            detail::QuadBatchDraw d;
            d.slot  = item.slot;
            d.faces = item.faces;

            for (int a = 0; a < 3; ++a) {
                const double o = axis(item.offset, a), f = std::floor(o);
                const std::int64_t cell = static_cast<std::int64_t>(f);
                d.abs_cell[a] = clamp_i32(cell);
                d.rel_cell[a] = clamp_i32(cell - camera_cell[a]);
                d.frac[a]     = static_cast<float>(o - f);
            }

            m_batch_scratch.push_back(d);
        }

        m_impl->draw_quad_batch_3d(m_batch_scratch.data(), m_batch_scratch.size(), camera_frac, material);
    } catch (...) {}
}

void Renderer::draw_instances(const graphics::Mesh3D& mesh, const vector3d& origin, const graphics::Instance3D* instances,
                    std::size_t count, const graphics::Material3D& material) noexcept {
    if (!m_impl || !instances || count == 0) return;
    float m[16];
    to_origin_relative(translation(origin), m);
    m_impl->draw_instances_3d(mesh, m, instances, count, material);
}

void Renderer::draw_triangles_3d(const graphics::Vertex3D* vertices, std::size_t count, const graphics::Material3D& material) noexcept {
    if (!m_impl) return;
    float m[16];
    to_origin_relative(math::Matrix4d::identity(), m);
    m_impl->draw_triangles_3d(vertices, count, m, material);
}

void Renderer::draw_triangles_3d(const graphics::Vertex3D* vertices, std::size_t count, const math::Matrix4d& model, const graphics::Material3D& material) noexcept {
    if (!m_impl) return;
    float m[16];
    to_origin_relative(model, m);
    m_impl->draw_triangles_3d(vertices, count, m, material);
}

void Renderer::draw_line_3d(const vector3d& a, const vector3d& b, const graphics::Color& color, float width, bool depth_test) noexcept {
    const vector3d pts[2] = { a - m_origin_3d, b - m_origin_3d };
    if (m_impl) m_impl->draw_lines_3d(pts, 2, color, width, depth_test);
}

void Renderer::draw_lines_3d(const std::vector<vector3d>& points, const graphics::Color& color, float width, bool depth_test) noexcept {
    if (!m_impl) return;

    try {
        m_line_scratch.resize(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) m_line_scratch[i] = points[i] - m_origin_3d;
        m_impl->draw_lines_3d(m_line_scratch.data(), m_line_scratch.size(), color, width, depth_test);
    } catch (...) {}
}

void Renderer::draw_box_3d(const vector3d& lo, const vector3d& hi, const graphics::Color& color, float width, bool depth_test) noexcept {
    if (!m_impl) return;
    vector3d c[8];
    for (int i = 0; i < 8; ++i) c[i] = vector3d{ (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
    const int e[24] = { 0,1, 2,3, 4,5, 6,7,  0,2, 1,3, 4,6, 5,7,  0,4, 1,5, 2,6, 3,7 };
    vector3d pts[24];
    for (int i = 0; i < 24; ++i) pts[i] = c[e[i]] - m_origin_3d;
    m_impl->draw_lines_3d(pts, 24, color, width, depth_test);
}

auto Renderer::measure_text(const std::string& text, const text::TextStyle& style, unsigned int max_width) noexcept -> text::TextMetrics {
    if (!m_impl || text.empty()) return {};
    try { return m_impl->measure_rich_text(text::RichText(text, style), max_width); } catch (...) { return {}; }
}

auto Renderer::measure_text(const std::string& text, const text::TextStyle& style) noexcept -> text::TextMetrics {
    if (m_impl) return m_impl->measure_text(text.c_str(), static_cast<int>(text.size()), style);
    return {};
}

auto Renderer::measure_text(const std::wstring& text, const text::TextStyle& style) noexcept -> text::TextMetrics {
    if (m_impl) return m_impl->measure_text(text.c_str(), static_cast<int>(text.size()), style);
    return {};
}

auto Renderer::translation(const vector3d& t) noexcept -> math::Matrix4d {
    math::Matrix4d m = math::Matrix4d::identity();
    m.data[3] = t.x; m.data[7] = t.y; m.data[11] = t.z;
    return m;
}

std::int32_t Renderer::clamp_i32(std::int64_t v) noexcept {
    constexpr std::int64_t lo = std::numeric_limits<std::int32_t>::min(), hi = std::numeric_limits<std::int32_t>::max();
    return static_cast<std::int32_t>(v < lo ? lo : (v > hi ? hi : v));
}

void Renderer::cell_origin(const vector3d& offset, std::int32_t* out) noexcept {
    out[0] = static_cast<std::int32_t>(std::floor(offset.x));
    out[1] = static_cast<std::int32_t>(std::floor(offset.y));
    out[2] = static_cast<std::int32_t>(std::floor(offset.z));
}

void Renderer::to_origin_relative(const math::Matrix4d& model, float* out) const noexcept {
    const math::Matrix4d rel = translation(-m_origin_3d) * model;
    for (std::size_t i = 0; i < 16; ++i) out[i] = static_cast<float>(rel.data[i]);
}

bool Renderer::open_target(const graphics::RenderTarget& target, const graphics::Color* clear_color) noexcept {
    if (!m_impl) return false;
    if (m_impl->in_3d()) m_impl->end_3d();

    try {
        TargetFrame f;
        f.xform = m_xform;
        f.stack = m_xform_stack;
        f.kind  = static_cast<int>(m_xform_kind);
        f.width = target.width();
        f.height = target.height();
        if (!m_impl->begin_target(target, clear_color)) return false;
        m_target_frames.push_back(std::move(f));
        m_xform_stack.clear();
        reset_transform();
        return true;
    } catch (...) {
        return false;
    }
}

void Renderer::release() noexcept {
    while (!m_target_frames.empty()) end_target();
    if (m_window) {
        m_window->set_paint_callback(nullptr);
        m_window->remove_event_listeners(WindowEventType::WindowResize);
        if (m_impl && m_impl->is_gpu()) m_window->set_background_erase(true);
    }

    if (m_impl) { m_impl->shutdown(); }
    m_impl.reset();
    m_frame_open  = false;
    m_has_present = false;
}

bool Renderer::fill_instance(
    detail::SpriteInstance& s, const double* xs, const double* ys, double offset_x, double offset_y,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a, float opacity
) const noexcept {
    if (opacity < 1.0f) a = opacity <= 0.0f ? 0 : static_cast<std::uint8_t>(a * opacity + 0.5f);
    if (a == 0) return false;

    for (int i = 0; i < 4; ++i) {
        const RenderPoint p = mapped(xs[i] + offset_x, ys[i] + offset_y);
        s.x[i] = p.x;
        s.y[i] = p.y;
    }

    s.r = r;
    s.g = g;
    s.b = b;
    s.a = a;
    return true;
}

void Renderer::finish_frame(FrameClock::time_point start) noexcept {
    const FrameClock::time_point end = FrameClock::now();
    FrameStats s;
    s.index       = m_frame_stats.index + 1;
    s.cpu_ms      = m_frame_open ? span_ms(start - m_frame_begin) : 0.0;
    s.present_ms  = span_ms(end - start);
    s.interval_ms = m_has_present ? span_ms(end - m_last_present) : 0.0;

    if (m_gpu_timing && m_impl) {
        const GpuTimings t = m_impl->gpu_timings();
        s.gpu_valid = t.valid;
        s.gpu_ms    = t.total_ms;
    }

    m_last_present = end;
    m_has_present  = true;
    m_frame_open   = false;
    m_frame_stats  = s;

    for (std::size_t i = 0; i < m_observers.size();) {
        if (const auto o = m_observers[i].lock()) { o->on_frame(*this, s); ++i; }
        else m_observers.erase(m_observers.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void Renderer::notify_unbind() noexcept {
    for (std::size_t i = 0; i < m_observers.size();) {
        if (const auto o = m_observers[i].lock()) { o->on_unbind(*this); ++i; }
        else m_observers.erase(m_observers.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void Renderer::classify_transform() noexcept {
    const auto& m = m_xform.data;
    if (m[1] != 0.0 || m[3] != 0.0) { m_xform_kind = XformKind::General; return; }
    if (m[0] != 1.0 || m[4] != 1.0) { m_xform_kind = XformKind::AxisAligned; return; }
    m_xform_kind = (m[2] == 0.0 && m[5] == 0.0) ? XformKind::Identity : XformKind::Translate;
}

bool Renderer::axis_aligned_positive() const noexcept {
    return m_xform_kind <= XformKind::Translate || (m_xform_kind == XformKind::AxisAligned && m_xform.data[0] > 0.0 && m_xform.data[4] > 0.0);
}

int Renderer::round_i(double v) noexcept {
    if (!(v > -2147483000.0)) return std::numeric_limits<int>::min() / 2;
    if (!(v <  2147483000.0)) return std::numeric_limits<int>::max() / 2;
    return static_cast<int>(std::lround(v));
}

unsigned int Renderer::scale_len(unsigned int len, double s) noexcept {
    const double v = std::abs(static_cast<double>(len) * s);
    return v >= 4.0e9 ? 4000000000u : static_cast<unsigned int>(std::lround(v));
}

auto Renderer::map_rect_bounds(int x, int y, unsigned int w, unsigned int h) const noexcept -> ScreenRect {
    if (m_xform_kind == XformKind::Identity) return { x, y, w, h };
    const double x0 = x, y0 = y, x1 = x + static_cast<double>(w), y1 = y + static_cast<double>(h);
    const vector2d c[4] = { map_point(x0, y0), map_point(x1, y0), map_point(x1, y1), map_point(x0, y1) };
    double lx = c[0].x, hx = c[0].x, ly = c[0].y, hy = c[0].y;

    for (int i = 1; i < 4; ++i) {
        lx = std::min(lx, c[i].x); hx = std::max(hx, c[i].x);
        ly = std::min(ly, c[i].y); hy = std::max(hy, c[i].y);
    }

    const int sx = round_i(lx), sy = round_i(ly), ex = round_i(hx), ey = round_i(hy);
    return { sx, sy, static_cast<unsigned int>(std::max(0, ex - sx)), static_cast<unsigned int>(std::max(0, ey - sy)) };
}

auto Renderer::map_points(const RenderPoint* pts, std::size_t count) noexcept -> const RenderPoint* {
    if (m_xform_kind == XformKind::Identity) return pts;

    try {
        m_points.resize(count);
        for (std::size_t i = 0; i < count; ++i) m_points[i] = mapped(pts[i].x, pts[i].y);
        return m_points.data();
    } catch (...) { return pts; }
}

void Renderer::build_ellipse(int cx, int cy, unsigned int rx, unsigned int ry) {
    const auto& m = m_xform.data;
    const double sx = std::hypot(m[0], m[3]), sy = std::hypot(m[1], m[4]);
    const double r = std::max(rx * sx, ry * sy);
    const int n = std::max(16, std::min(256, static_cast<int>(std::ceil(6.283185307179586 * r / 4.0))));
    m_points.resize(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        const double a = 6.283185307179586 * i / n;
        m_points[static_cast<std::size_t>(i)] = mapped(cx + rx * std::cos(a), cy + ry * std::sin(a));
    }
}

void Renderer::emit_texture(const graphics::Texture& tex, int dx, int dy, unsigned int dw, unsigned int dh, float opacity, const graphics::TextureRect& src) noexcept {
    if (!m_impl) return;
    if (m_xform_kind == XformKind::Identity) { m_impl->draw_texture(dx, dy, dw, dh, tex, opacity, src); return; }

    if (axis_aligned_positive()) {
        const ScreenRect r = map_rect_bounds(dx, dy, dw, dh);
        m_impl->draw_texture(r.x, r.y, r.w, r.h, tex, opacity, src);
        return;
    }

    const double x0 = dx, y0 = dy, x1 = dx + static_cast<double>(dw), y1 = dy + static_cast<double>(dh);
    const RenderPoint quad[4] = { mapped(x0, y0), mapped(x1, y0), mapped(x1, y1), mapped(x0, y1) };
    m_impl->draw_texture_quad(quad, tex, opacity, src);
}

auto Renderer::resolve_corners(int x1, int y1, int x2, int y2) noexcept -> ResolvedRect {
    int lx = (x1 < x2) ? x1 : x2;
    int ly = (y1 < y2) ? y1 : y2;
    unsigned int w = static_cast<unsigned int>((x1 < x2) ? (x2 - x1) : (x1 - x2));
    unsigned int h = static_cast<unsigned int>((y1 < y2) ? (y2 - y1) : (y1 - y2));
    return { lx, ly, w, h };
}

} // namespace windows
} // namespace fizmo

namespace fizmo {
namespace windows {

#if defined(OS_WINDOWS)
void blit_framebuffer(void* hdc_raw, const graphics::Framebuffer& fb) {
    HDC hdc = static_cast<HDC>(hdc_raw);
    unsigned int w = fb.width(), h = fb.height();
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(h);
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    std::vector<std::uint8_t> bits(w * h * 4);
    const graphics::Color* px = fb.data();

    for (unsigned int i = 0; i < w * h; ++i) {
        bits[i * 4 + 0] = px[i].blue();
        bits[i * 4 + 1] = px[i].green();
        bits[i * 4 + 2] = px[i].red();
        bits[i * 4 + 3] = px[i].alpha();
    }

    SetDIBitsToDevice(
        hdc, 0, 0, w, h,
        0, 0, 0, h,
        bits.data(), &bmi, DIB_RGB_COLORS
    );
}
#endif

} // namespace windows
} // namespace fizmo
