#include "fizmo_library.hpp"
#include "render_queue.hpp"

namespace fizmo {
namespace gpu {

void Encoder::dispatch(std::uint16_t view, Material& material, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t key) {
    DrawCall c;
    c.material = &material;
    c.dispatch[0] = x; c.dispatch[1] = y; c.dispatch[2] = z;
    c.key = key;
    submit(view, c);
}

void Encoder::dispatch(std::uint16_t view, const ComputePipeline& pipeline, std::initializer_list<const BindGroup*> groups, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t key) {
    DrawCall c;
    c.compute = &pipeline;
    std::size_t i = 0;
    for (const BindGroup* g : groups) if (i < c.groups.size()) c.groups[i++] = g;
    c.dispatch[0] = x; c.dispatch[1] = y; c.dispatch[2] = z;
    c.key = key;
    submit(view, c);
}

void RenderQueue::set_view(std::uint16_t id, const Texture& color, LoadOp load, ClearColor clear, const Texture* depth) {
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

auto RenderQueue::begin_encoder() -> Encoder& {
    std::lock_guard<std::mutex> g(m_lock);
    Encoder* e = nullptr;
    if (!m_free.empty()) { e = m_free.back(); m_free.pop_back(); }
    else { m_encoders.emplace_back(); e = &m_encoders.back(); }
    e->clear();
    m_active.push_back(e);
    return *e;
}

void RenderQueue::flush(CommandList& cmd, std::uint32_t frame_index) {
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

void RenderQueue::discard() {
    m_main.clear();
    std::lock_guard<std::mutex> g(m_lock);
    for (Encoder* e : m_active) { e->clear(); m_free.push_back(e); }
    m_active.clear();
}

} // namespace gpu
} // namespace fizmo
