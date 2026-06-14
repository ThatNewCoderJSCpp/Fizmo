#ifndef FIZMO_SCENE_HPP
#define FIZMO_SCENE_HPP

#include "canvas.hpp"
#include "camera_2d.hpp"
#include "sprite.hpp"
#include <vector>
#include <algorithm>
#include <string>
#include <functional>
#include <memory>
#include <cassert>

namespace fizmo {
namespace graphics {

class IDrawable {
public:
    virtual ~IDrawable() noexcept = default;
    virtual void draw(Canvas& canvas) const noexcept = 0;
    virtual int  z_order()            const noexcept = 0;
    virtual bool visible()            const noexcept = 0;
};

class SpriteDrawable final : public IDrawable {
private:
    Sprite* m_sprite;

public:
    explicit SpriteDrawable(Sprite& s) noexcept : m_sprite(&s) {}

    void draw(Canvas& canvas) const noexcept override { m_sprite->draw(canvas); }
    int  z_order()            const noexcept override { return m_sprite->z_order(); }
    bool visible()            const noexcept override { return m_sprite->visible(); }

    Sprite&       sprite()       noexcept { return *m_sprite; }
    const Sprite& sprite() const noexcept { return *m_sprite; }
};

class Layer {
public:
    explicit Layer(const std::string& name = "", int z = 0) noexcept : m_name(name), m_z_order(z) {}
    Layer(const std::string& name, int z, unsigned int vp_w, unsigned int vp_h) noexcept : m_name(name), m_z_order(z), m_camera(0.0, 0.0, vp_w, vp_h) {}

    const std::string& name()    const noexcept { return m_name; }
    void set_name(const std::string& n) noexcept { m_name = n; }

    int  z_order()               const noexcept { return m_z_order; }
    void set_z_order(int z)            noexcept { m_z_order = z; }

    bool visible()               const noexcept { return m_visible; }
    void set_visible(bool v)           noexcept { m_visible = v; }

    Camera2D&       camera()       noexcept { return m_camera; }
    const Camera2D& camera() const noexcept { return m_camera; }

    bool camera_enabled() const noexcept { return m_use_camera; }
    void set_camera_enabled(bool e) noexcept { m_use_camera = e; }
    void add(IDrawable* d) noexcept { if (d) { m_drawables.push_back(d); m_dirty = true; }}

    void remove(IDrawable* d) noexcept {
        auto it = std::find(m_drawables.begin(), m_drawables.end(), d);
        if (it != m_drawables.end()) { m_drawables.erase(it); }
    }

    bool contains(const IDrawable* d) const noexcept {
        return std::find(m_drawables.begin(), m_drawables.end(), d) != m_drawables.end();
    }

    void clear() noexcept { m_drawables.clear(); }
    std::size_t size() const noexcept { return m_drawables.size(); }

    SpriteDrawable& add_sprite(Sprite& s) {
        m_owned.push_back(std::make_unique<SpriteDrawable>(s));
        auto* p = m_owned.back().get();
        add(p);
        return *p;
    }

    void mark_dirty() noexcept { m_dirty = true; }

    void sort() noexcept {

        std::stable_sort(
            m_drawables.begin(), m_drawables.end(),
            [](const IDrawable* a, const IDrawable* b) {
                return a->z_order() < b->z_order();
            }
        );

        m_dirty = false;
    }

    void render(Canvas& canvas) noexcept {
        if (!m_visible) return;
        if (m_dirty) sort();
        if (m_use_camera) m_camera.begin(canvas);
        for (const auto* d : m_drawables) { if (d->visible()) d->draw(canvas); }
        if (m_use_camera) m_camera.end(canvas);
    }

    void on_update(std::function<void(Layer&, double dt)> fn) noexcept { m_on_update = std::move(fn); }
    void update(double dt) { if (m_on_update) m_on_update(*this, dt); }

    template <typename Fn>
    void for_each(Fn&& fn) const { for (auto* d : m_drawables) fn(*d); }

    template <typename Fn>
    void for_each(Fn&& fn) { for (auto* d : m_drawables) fn(*d); }

private:
    std::string              m_name;
    int                      m_z_order    = 0;
    bool                     m_visible    = true;
    bool                     m_use_camera = true;
    bool                     m_dirty      = false;
    Camera2D                 m_camera;
    std::vector<IDrawable*>  m_drawables;
    std::function<void(Layer&, double dt)> m_on_update;
    std::vector<std::unique_ptr<SpriteDrawable>> m_owned;
};

class Scene {
public:
    Scene() noexcept = default;

    Layer& add_layer(const std::string& name, int z = 0) {
        m_layers.push_back(std::make_unique<Layer>(name, z));
        m_sorted = false;
        return *m_layers.back();
    }

    Layer& add_layer(const std::string& name, int z, unsigned int vp_w, unsigned int vp_h) {
        m_layers.push_back(std::make_unique<Layer>(name, z, vp_w, vp_h));
        m_sorted = false;
        return *m_layers.back();
    }

    Layer* find_layer(const std::string& name) noexcept {
        for (auto& l : m_layers) if (l->name() == name) return l.get();
        return nullptr;
    }

    const Layer* find_layer(const std::string& name) const noexcept {
        for (auto& l : m_layers) if (l->name() == name) return l.get();
        return nullptr;
    }

    Layer& operator[](const std::string& name) {
        auto* l = find_layer(name);
        assert(l && "Scene::operator[]: layer not found");
        return *l;
    }

    bool remove_layer(const std::string& name) noexcept {
        auto it = std::find_if(m_layers.begin(), m_layers.end(), [&](const std::unique_ptr<Layer>& l) { return l->name() == name; });
        if (it == m_layers.end()) return false;
        m_layers.erase(it);
        return true;
    }

    void clear() noexcept { m_layers.clear(); }
    std::size_t layer_count() const noexcept { return m_layers.size(); }
    void update(double dt) { for (auto& l : m_layers) l->update(dt); }

    void render(Canvas& canvas) noexcept {
        if (!m_sorted) sort_layers();
        for (auto* l : m_order) l->render(canvas);
    }

    void set_all_viewports(unsigned int w, unsigned int h) noexcept {
        for (auto& l : m_layers) l->camera().set_viewport_size(w, h);
    }

    template <typename Fn>
    void for_each_layer(Fn&& fn) { for (auto& l : m_layers) fn(*l); }

    template <typename Fn>
    void for_each_layer(Fn&& fn) const { for (auto& l : m_layers) fn(*l); }

private:
    void sort_layers() noexcept {
        m_order.clear();
        m_order.reserve(m_layers.size());
        for (auto& l : m_layers) m_order.push_back(l.get());

        std::stable_sort(
            m_order.begin(), m_order.end(),
            [](const Layer* a, const Layer* b) {
                return a->z_order() < b->z_order();
            }
        );

        m_sorted = true;
    }

    std::vector<std::unique_ptr<Layer>> m_layers;
    std::vector<Layer*>                 m_order;   
    bool                                m_sorted = true;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_SCENE_HPP