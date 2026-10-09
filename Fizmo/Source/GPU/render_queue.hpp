#ifndef FIZMO_GPU_RENDER_QUEUE_HPP
#define FIZMO_GPU_RENDER_QUEUE_HPP

#include "device.hpp"
#include "material.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace fizmo {
namespace gpu {

struct SortKey {
    static constexpr std::uint64_t make(std::uint8_t layer, std::uint16_t program, std::uint32_t depth, std::uint16_t sequence = 0) noexcept {
        return (static_cast<std::uint64_t>(layer) << 56) | (static_cast<std::uint64_t>(program) << 40) | (static_cast<std::uint64_t>(depth & 0xFFFFFFu) << 16) | sequence;
    }

    static constexpr std::uint64_t opaque(std::uint8_t layer, std::uint16_t program, float depth01) noexcept {
        return make(layer, program, quantize(depth01));
    }

    static constexpr std::uint64_t translucent(std::uint8_t layer, float depth01, std::uint16_t program = 0) noexcept {
        return (static_cast<std::uint64_t>(layer) << 56) | (static_cast<std::uint64_t>(0xFFFFFFu - quantize(depth01)) << 32) | (static_cast<std::uint64_t>(program) << 16);
    }

    static constexpr std::uint32_t quantize(float depth01) noexcept {
        return depth01 <= 0.0f ? 0u : depth01 >= 1.0f ? 0xFFFFFFu : static_cast<std::uint32_t>(depth01 * 16777215.0f);
    }
};

enum class ViewSort : std::uint8_t { Key = 0, Sequential, DepthAscending, DepthDescending };

struct ViewDesc {
    std::vector<ColorTarget> colors;
    DepthTarget              depth;
    bool                     has_depth = false;
    Rect2D                   area;
    Viewport                 viewport;
    bool                     has_viewport = false;
    ViewSort                 sort = ViewSort::Key;
    std::string              label;
};

struct DrawCall {
    std::uint64_t                             key            = 0;
    float                                     depth          = 0.0f;
    Material*                                 material       = nullptr;
    const RenderPipeline*                     pipeline       = nullptr;
    const ComputePipeline*                    compute        = nullptr;
    std::array<const BindGroup*, Material::kMaxGroups> groups{};
    std::array<const Buffer*, 4>              vertex_buffers{};
    std::array<std::uint64_t, 4>              vertex_offsets{};
    const Buffer*                             index_buffer   = nullptr;
    IndexType                                 index_type     = IndexType::UInt32;
    std::uint64_t                             index_offset   = 0;
    std::uint32_t                             count          = 0;
    std::uint32_t                             instances      = 1;
    std::uint32_t                             first          = 0;
    std::int32_t                              vertex_offset  = 0;
    std::uint32_t                             first_instance = 0;
    std::uint32_t                             dispatch[3]    = { 0, 0, 0 };
    std::array<std::uint8_t, 128>             push{};
    std::uint32_t                             push_size      = 0;
    Rect2D                                    scissor;
    bool                                      has_scissor    = false;

    DrawCall& with_material(Material& m) noexcept { material = &m; return *this; }
    DrawCall& with_pipeline(const RenderPipeline& p) noexcept { pipeline = &p; return *this; }
    DrawCall& with_group(std::uint32_t index, const BindGroup& g) noexcept { if (index < groups.size()) groups[index] = &g; return *this; }
    DrawCall& with_vertices(const Buffer& b, std::uint32_t slot = 0, std::uint64_t offset = 0) noexcept { if (slot < 4) { vertex_buffers[slot] = &b; vertex_offsets[slot] = offset; } return *this; }
    DrawCall& with_indices(const Buffer& b, IndexType type = IndexType::UInt32, std::uint64_t offset = 0) noexcept { index_buffer = &b; index_type = type; index_offset = offset; return *this; }
    DrawCall& with_count(std::uint32_t n, std::uint32_t inst = 1) noexcept { count = n; instances = inst; return *this; }
    DrawCall& with_key(std::uint64_t k) noexcept { key = k; return *this; }
    DrawCall& with_depth(float d) noexcept { depth = d; return *this; }
    DrawCall& with_scissor(const Rect2D& r) noexcept { scissor = r; has_scissor = true; return *this; }

    template <typename T>
    DrawCall& with_push(const T& value) noexcept {
        static_assert(std::is_trivially_copyable<T>::value && sizeof(T) <= 128, "push data must be trivially copyable and at most 128 bytes");
        std::memcpy(push.data(), &value, sizeof(T));
        push_size = static_cast<std::uint32_t>(sizeof(T));
        return *this;
    }
};

class RenderQueue;

class Encoder {
private:
    friend class RenderQueue;

    struct Item {
        std::uint16_t view = 0;
        std::uint32_t order = 0;
        DrawCall      call;
    };

    std::vector<Item> m_items;
    std::uint32_t     m_counter = 0;

public:
    void submit(std::uint16_t view, const DrawCall& call) { m_items.push_back({ view, m_counter++, call }); }

    void dispatch(std::uint16_t view, Material& material, std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1, std::uint64_t key = 0) {
        DrawCall c;
        c.material = &material;
        c.dispatch[0] = x; c.dispatch[1] = y; c.dispatch[2] = z;
        c.key = key;
        submit(view, c);
    }

    void dispatch(std::uint16_t view, const ComputePipeline& pipeline, std::initializer_list<const BindGroup*> groups, std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1, std::uint64_t key = 0) {
        DrawCall c;
        c.compute = &pipeline;
        std::size_t i = 0;
        for (const BindGroup* g : groups) if (i < c.groups.size()) c.groups[i++] = g;
        c.dispatch[0] = x; c.dispatch[1] = y; c.dispatch[2] = z;
        c.key = key;
        submit(view, c);
    }

    std::size_t size() const noexcept { return m_items.size(); }
    void clear() noexcept { m_items.clear(); m_counter = 0; }
};

class RenderQueue {
private:
    std::map<std::uint16_t, ViewDesc>     m_views;
    std::deque<Encoder>                   m_encoders;
    std::vector<Encoder*>                 m_free;
    std::vector<Encoder*>                 m_active;
    Encoder                               m_main;
    std::mutex                            m_lock;
    std::vector<const Encoder::Item*>     m_sorted;
    std::size_t                           m_last_draws = 0;
    std::size_t                           m_last_state_changes = 0;

    static bool is_compute(const DrawCall& c) noexcept { return c.dispatch[0] != 0 || c.compute; }

public:
    ViewDesc& view(std::uint16_t id) { return m_views[id]; }
    bool has_view(std::uint16_t id) const { return m_views.count(id) != 0; }
    void remove_view(std::uint16_t id) { m_views.erase(id); }

    void set_view(std::uint16_t id, const Texture& color, LoadOp load = LoadOp::Clear, ClearColor clear = {}, const Texture* depth = nullptr) {
        ViewDesc& v = m_views[id];
        v.colors.clear();
        ColorTarget c;
        c.texture = &color;
        c.load = load;
        c.clear = clear;
        v.colors.push_back(c);
        v.area = { { 0, 0 }, { color.width(), color.height() } };
        v.has_depth = depth != nullptr;
        if (depth) { v.depth = DepthTarget{}; v.depth.texture = depth; v.depth.load = load == LoadOp::Load ? LoadOp::Load : LoadOp::Clear; }
    }

    void set_view_sort(std::uint16_t id, ViewSort sort) { m_views[id].sort = sort; }

    Encoder& encoder() noexcept { return m_main; }

    Encoder& begin_encoder() {
        std::lock_guard<std::mutex> g(m_lock);
        Encoder* e = nullptr;
        if (!m_free.empty()) { e = m_free.back(); m_free.pop_back(); }
        else { m_encoders.emplace_back(); e = &m_encoders.back(); }
        e->clear();
        m_active.push_back(e);
        return *e;
    }

    void submit(std::uint16_t view, const DrawCall& call) { m_main.submit(view, call); }

    std::size_t last_draw_count() const noexcept { return m_last_draws; }
    std::size_t last_state_changes() const noexcept { return m_last_state_changes; }

    void flush(CommandList& cmd, std::uint32_t frame_index) {
        std::vector<Encoder*> sources{ &m_main };
        {
            std::lock_guard<std::mutex> g(m_lock);
            sources.insert(sources.end(), m_active.begin(), m_active.end());
        }

        m_sorted.clear();
        for (std::size_t e = 0; e < sources.size(); ++e) for (const Encoder::Item& it : sources[e]->m_items) m_sorted.push_back(&it);
        std::stable_sort(m_sorted.begin(), m_sorted.end(), [](const Encoder::Item* a, const Encoder::Item* b) { return a->view < b->view; });
        m_last_draws = 0;
        m_last_state_changes = 0;
        std::size_t i = 0;

        for (const auto& vit : m_views) {
            const std::uint16_t vid = vit.first;
            while (i < m_sorted.size() && m_sorted[i]->view < vid) ++i;
            std::size_t end = i;
            while (end < m_sorted.size() && m_sorted[end]->view == vid) ++end;
            const ViewDesc& v = vit.second;

            auto first = m_sorted.begin() + static_cast<std::ptrdiff_t>(i), last = m_sorted.begin() + static_cast<std::ptrdiff_t>(end);
            switch (v.sort) {
                case ViewSort::Key:             std::stable_sort(first, last, [](const Encoder::Item* a, const Encoder::Item* b) { return a->call.key < b->call.key; }); break;
                case ViewSort::DepthAscending:  std::stable_sort(first, last, [](const Encoder::Item* a, const Encoder::Item* b) { return a->call.depth < b->call.depth; }); break;
                case ViewSort::DepthDescending: std::stable_sort(first, last, [](const Encoder::Item* a, const Encoder::Item* b) { return a->call.depth > b->call.depth; }); break;
                case ViewSort::Sequential:      break;
            }

            for (std::size_t k = i; k < end; ++k) {
                const DrawCall& c = m_sorted[k]->call;
                if (!is_compute(c)) continue;
                if (c.material) { c.material->dispatch(cmd, frame_index, c.dispatch[0], c.dispatch[1], c.dispatch[2]); ++m_last_draws; continue; }
                cmd.set_pipeline(*c.compute);
                for (std::uint32_t g = 0; g < c.groups.size(); ++g) if (c.groups[g]) cmd.set_bind_group(g, *c.groups[g]);
                if (c.push_size) cmd.push_constants(c.push.data(), c.push_size);
                cmd.dispatch(c.dispatch[0], c.dispatch[1], c.dispatch[2]);
                ++m_last_draws;
            }

            bool any_draw = false;
            for (std::size_t k = i; k < end && !any_draw; ++k) any_draw = !is_compute(m_sorted[k]->call);

            if (any_draw && !v.colors.empty()) {
                RenderPassDesc pass;
                pass.area = v.area;
                pass.colors = { v.colors.data(), v.colors.size() };
                pass.depth = v.has_depth ? &v.depth : nullptr;
                pass.label = v.label.empty() ? nullptr : v.label.c_str();
                cmd.begin_render_pass(pass);
                const Viewport vp = v.has_viewport ? v.viewport : Viewport{ static_cast<float>(v.area.offset.x), static_cast<float>(v.area.offset.y), static_cast<float>(v.area.extent.width), static_cast<float>(v.area.extent.height), 0.0f, 1.0f };
                cmd.set_viewport(vp);
                Rect2D current_scissor = v.area;
                cmd.set_scissor(current_scissor);
                const void* bound_pipeline = nullptr;
                const Material* bound_material = nullptr;
                std::array<const void*, Material::kMaxGroups> bound_groups{};
                std::array<const void*, 4> bound_vb{};
                std::array<std::uint64_t, 4> bound_vb_offset{};
                const void* bound_ib = nullptr;

                for (std::size_t k = i; k < end; ++k) {
                    const DrawCall& c = m_sorted[k]->call;
                    if (is_compute(c)) continue;
                    const Rect2D sc = c.has_scissor ? c.scissor : v.area;
                    if (sc.offset.x != current_scissor.offset.x || sc.offset.y != current_scissor.offset.y || sc.extent.width != current_scissor.extent.width || sc.extent.height != current_scissor.extent.height) {
                        cmd.set_scissor(sc);
                        current_scissor = sc;
                    }

                    if (c.material) {
                        if (bound_material != c.material) {
                            if (!c.material->bind(cmd, frame_index)) continue;
                            bound_material = c.material;
                            bound_pipeline = &c.material->pipeline();
                            bound_groups.fill(nullptr);
                            ++m_last_state_changes;
                        } else if (c.push_size == 0 && c.material->push_size()) {
                            cmd.push_constants(c.material->push_data(), c.material->push_size());
                        }
                    } else if (c.pipeline && bound_pipeline != c.pipeline) {
                        cmd.set_pipeline(*c.pipeline);
                        bound_pipeline = c.pipeline;
                        bound_material = nullptr;
                        bound_groups.fill(nullptr);
                        ++m_last_state_changes;
                    }

                    for (std::uint32_t g = 0; g < c.groups.size(); ++g) {
                        if (!c.groups[g] || bound_groups[g] == c.groups[g]) continue;
                        cmd.set_bind_group(g, *c.groups[g]);
                        bound_groups[g] = c.groups[g];
                    }

                    if (c.push_size) cmd.push_constants(c.push.data(), c.push_size);

                    for (std::uint32_t s = 0; s < c.vertex_buffers.size(); ++s) {
                        if (!c.vertex_buffers[s]) continue;
                        if (bound_vb[s] == c.vertex_buffers[s] && bound_vb_offset[s] == c.vertex_offsets[s]) continue;
                        cmd.set_vertex_buffer(s, *c.vertex_buffers[s], c.vertex_offsets[s]);
                        bound_vb[s] = c.vertex_buffers[s];
                        bound_vb_offset[s] = c.vertex_offsets[s];
                    }

                    if (c.index_buffer) {
                        if (bound_ib != c.index_buffer) { cmd.set_index_buffer(*c.index_buffer, c.index_type, c.index_offset); bound_ib = c.index_buffer; }
                        cmd.draw_indexed(c.count, c.instances, c.first, c.vertex_offset, c.first_instance);
                    } else {
                        cmd.draw(c.count, c.instances, c.first, c.first_instance);
                    }

                    ++m_last_draws;
                }

                cmd.end_render_pass();
            } else if (!v.colors.empty() && v.colors[0].load == LoadOp::Clear) {
                RenderPassDesc pass;
                pass.area = v.area;
                pass.colors = { v.colors.data(), v.colors.size() };
                pass.depth = v.has_depth ? &v.depth : nullptr;
                cmd.begin_render_pass(pass);
                cmd.end_render_pass();
            }

            i = end;
        }

        m_main.clear();
        std::lock_guard<std::mutex> g(m_lock);
        for (Encoder* e : m_active) { e->clear(); m_free.push_back(e); }
        m_active.clear();
    }

    void discard() {
        m_main.clear();
        std::lock_guard<std::mutex> g(m_lock);
        for (Encoder* e : m_active) { e->clear(); m_free.push_back(e); }
        m_active.clear();
    }
};

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_RENDER_QUEUE_HPP
