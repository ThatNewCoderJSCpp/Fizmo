#ifndef FIZMO_GPU_MATERIAL_HPP
#define FIZMO_GPU_MATERIAL_HPP

#include "device.hpp"
#include "shader_bundle.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace gpu {

struct MaterialDesc {
    ShaderBundle                 vertex;
    ShaderBundle                 fragment;
    ShaderBundle                 compute;
    std::vector<VertexBinding>   vertex_bindings;
    std::vector<VertexAttribute> vertex_attributes;
    Topology                     topology     = Topology::TriangleList;
    CullMode                     cull_mode    = CullMode::None;
    FrontFace                    front_face   = FrontFace::CounterClockwise;
    DepthState                   depth;
    BlendState                   blend;
    std::vector<Format>          color_formats{ Format::RGBA8Unorm };
    Format                       depth_format = Format::Undefined;
    std::uint32_t                samples      = 1;
    std::string                  name;
};

class Material {
public:
    static constexpr std::uint32_t kMaxGroups = 4;

private:
    struct Slot {
        std::uint32_t set     = 0;
        std::uint32_t binding = 0;
        BindingType   type    = BindingType::SampledTexture;
        ShaderStage   stages  = ShaderStage::None;
        std::uint32_t count   = 1;
        std::string   name;
    };

    struct UniformBuffer {
        std::uint32_t                         set     = 0;
        std::uint32_t                         binding = 0;
        ShaderBlock                           block;
        std::vector<std::uint8_t>             data;
        std::vector<std::unique_ptr<Buffer>>  ring;
        std::vector<bool>                     dirty;
    };

    struct TextureBinding {
        const Texture* texture = nullptr;
        const Sampler* sampler = nullptr;
    };

    struct StorageBinding {
        const Buffer* buffer = nullptr;
        std::uint64_t offset = 0;
        std::uint64_t size   = 0;
        const Texture* image = nullptr;
    };

    struct Resources {
        ShaderModule                               vs, fs, cs;
        std::array<BindGroupLayout, kMaxGroups>    layouts;
        PipelineLayout                             layout;
        RenderPipeline                             pipeline;
        ComputePipeline                            compute;
        std::vector<UniformBuffer>                 uniforms;
        std::vector<std::array<BindGroup, kMaxGroups>> groups;
    };

    struct Retired {
        int       frames = 0;
        Resources res;
    };

    Device*                                   m_device = nullptr;
    MaterialDesc                              m_desc;
    Resources                                 m_res;
    std::vector<Slot>                         m_slots;
    std::uint32_t                             m_group_count = 0;
    std::vector<std::pair<std::uint64_t, TextureBinding>> m_textures;
    std::vector<std::pair<std::uint64_t, StorageBinding>> m_storage;
    ShaderBlock                               m_push_block;
    std::vector<std::uint8_t>                 m_push;
    ShaderStage                               m_push_stages = ShaderStage::None;
    std::vector<bool>                         m_groups_dirty;
    std::vector<Retired>                      m_retired;
    std::uint32_t                             m_last_frame = 0xFFFFFFFFu;
    std::uint32_t                             m_ring = 1;
    std::shared_ptr<Sampler>                  m_default_sampler;
    std::shared_ptr<Texture>                  m_white;
    std::shared_ptr<Buffer>                   m_dummy_storage;
    std::string                               m_error;

    static std::uint64_t key(std::uint32_t set, std::uint32_t binding) noexcept { return (static_cast<std::uint64_t>(set) << 32) | binding; }

    static BindingType binding_type(GlslResource kind) noexcept;

    static bool vertex_format(const ShaderInput& in, Format& out, std::uint32_t& size) noexcept;

    Result fail(Result r, const std::string& why) {
        m_error = why;
        return r;
    }

    bool is_compute() const noexcept { return m_desc.compute.valid(); }

    Result collect(const ShaderBundle& b, std::vector<Slot>& slots, ShaderBlock& push, ShaderStage& push_stages, std::vector<UniformBuffer>& uniforms);

    Result ensure_defaults();

    Result build(Resources& res, std::vector<Slot>& slots, ShaderBlock& push, ShaderStage& push_stages, std::uint32_t& group_count);

    void carry_values(const Resources& from);

    std::uint8_t* locate(const std::string& full, const ShaderMember*& member, std::uint32_t& index, std::vector<bool>** dirty);

    bool write_member(const std::string& name, const void* src, std::size_t bytes, bool floats);

    TextureBinding* texture_slot(std::uint32_t set, std::uint32_t binding) {
        for (auto& t : m_textures) if (t.first == key(set, binding)) return &t.second;
        return nullptr;
    }

    StorageBinding* storage_slot(std::uint32_t set, std::uint32_t binding) {
        for (auto& t : m_storage) if (t.first == key(set, binding)) return &t.second;
        return nullptr;
    }

    const Slot* find_slot(const std::string& name) const noexcept;

    bool build_groups(std::uint32_t ring);

    void advance(std::uint32_t frame_index);

    bool prepare(std::uint32_t frame_index);

    Result install(MaterialDesc desc, bool keep_values);

public:
    Material() = default;
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) = default;
    Material& operator=(Material&&) = default;

    Result create(Device& device, MaterialDesc desc);

    Result reload(const ShaderBundle* vertex, const ShaderBundle* fragment, const ShaderBundle* compute = nullptr);

    void destroy() noexcept;

    bool valid() const noexcept { return m_device && (m_res.pipeline.valid() || m_res.compute.valid()); }
    bool compute() const noexcept { return m_res.compute.valid(); }
    const std::string& error() const noexcept { return m_error; }
    const MaterialDesc& desc() const noexcept { return m_desc; }
    const RenderPipeline& pipeline() const noexcept { return m_res.pipeline; }
    const ComputePipeline& compute_pipeline() const noexcept { return m_res.compute; }
    const PipelineLayout& layout() const noexcept { return m_res.layout; }
    std::uint32_t push_size() const noexcept { return static_cast<std::uint32_t>(m_push.size()); }
    const std::uint8_t* push_data() const noexcept { return m_push.data(); }
    std::size_t retired_count() const noexcept { return m_retired.size(); }

    bool has(const std::string& name);

    std::vector<std::string> uniform_names() const;

    bool set(const std::string& name, float v) { return write_member(name, &v, sizeof(v), true); }
    bool set(const std::string& name, double v) { const float f = static_cast<float>(v); return write_member(name, &f, sizeof(f), true); }
    bool set(const std::string& name, std::int32_t v) { return write_member(name, &v, sizeof(v), false); }
    bool set(const std::string& name, std::uint32_t v) { return write_member(name, &v, sizeof(v), false); }
    bool set(const std::string& name, bool v) { const std::uint32_t b = v ? 1u : 0u; return write_member(name, &b, sizeof(b), false); }
    bool set(const std::string& name, std::initializer_list<float> v) { return write_member(name, v.begin(), v.size() * sizeof(float), true); }
    bool set(const std::string& name, const float* v, std::size_t count) { return write_member(name, v, count * sizeof(float), true); }

    template <std::size_t N>
    bool set(const std::string& name, const std::array<float, N>& v) { return write_member(name, v.data(), N * sizeof(float), true); }

    template <typename T, typename = std::enable_if_t<std::is_trivially_copyable<T>::value && !std::is_arithmetic<T>::value && !std::is_pointer<T>::value>>
    bool set(const std::string& name, const T& value) { return write_member(name, &value, sizeof(T), false); }

    bool set_bytes(const std::string& name, const void* data, std::size_t bytes) { return write_member(name, data, bytes, false); }

    bool set_push(const void* data, std::size_t bytes, std::size_t offset = 0);

    bool set_texture(const std::string& name, const Texture& texture, const Sampler* sampler = nullptr);

    bool set_storage(const std::string& name, const Buffer& buffer, std::uint64_t offset = 0, std::uint64_t size = 0);

    bool set_image(const std::string& name, const Texture& texture);

    bool bind(CommandList& cmd, std::uint32_t frame_index);

    bool bind_groups(CommandList& cmd, std::uint32_t frame_index);

    bool draw(CommandList& cmd, std::uint32_t frame_index, const Buffer& vertices, std::uint32_t count, std::uint32_t instances = 1);

    bool draw_fullscreen(CommandList& cmd, std::uint32_t frame_index, std::uint32_t vertices = 3) {
        if (!bind(cmd, frame_index)) return false;
        cmd.draw(vertices);
        return true;
    }

    bool dispatch(CommandList& cmd, std::uint32_t frame_index, std::uint32_t x, std::uint32_t y = 1, std::uint32_t z = 1);
};

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_MATERIAL_HPP
