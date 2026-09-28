#ifndef FIZMO_VULKAN_PIPELINE_HPP
#define FIZMO_VULKAN_PIPELINE_HPP

#include "resources.hpp"
#include <fstream>

namespace fizmo {
namespace vulkan {

class ShaderModule {
private:
    FIZMO_VK_UNIQUE(VkShaderModule, vkDestroyShaderModule) m_module;

public:
    ShaderModule() noexcept = default;

    Result create(const Device& device, Span<std::uint32_t> spirv, const char* name = nullptr) noexcept {
        m_module.reset();
        if (!device.valid()) return Result::NotInitialized;
        if (spirv.empty() || spirv[0] != 0x07230203u) return Result::InvalidArgument;   
        const auto* d = device.state();
        auto info = detail::make<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        info.codeSize = spirv.size() * sizeof(std::uint32_t);
        info.pCode    = spirv.data();
        native::ShaderModule h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateShaderModule(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_module = { d, h };
        if (name) d->set_name(VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<std::uint64_t>(h), name);
        return Result::Success;
    }

    Result create_from_file(const Device& device, const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return Result::FileNotFound;
        const std::streamsize bytes = file.tellg();
        if (bytes <= 0 || bytes % 4 != 0) return Result::InvalidArgument;
        std::vector<std::uint32_t> words(static_cast<std::size_t>(bytes / 4));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(words.data()), bytes);
        return create(device, words, path.c_str());
    }

    void destroy() noexcept { m_module.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_module); }
    native::ShaderModule handle() const noexcept { return m_module.get(); }
};

struct DescriptorBinding {
    std::uint32_t  binding = 0;
    DescriptorType type    = DescriptorType::UniformBuffer;
    ShaderStage    stages  = ShaderStage::AllGraphics;
    std::uint32_t  count   = 1;
};

class DescriptorSetLayout {
private:
    FIZMO_VK_UNIQUE(VkDescriptorSetLayout, vkDestroyDescriptorSetLayout) m_layout;
    std::vector<DescriptorBinding> m_bindings;

public:
    DescriptorSetLayout() noexcept = default;

    Result create(const Device& device, Span<DescriptorBinding> bindings) {
        m_layout.reset();
        if (!device.valid()) return Result::NotInitialized;
        const auto* d = device.state();
        std::vector<VkDescriptorSetLayoutBinding> raw;
        raw.reserve(bindings.size());

        for (const auto& b : bindings) {
            VkDescriptorSetLayoutBinding vb{};
            vb.binding         = b.binding;
            vb.descriptorType  = to_vk(b.type);
            vb.descriptorCount = b.count;
            vb.stageFlags      = to_vk(b.stages);
            raw.push_back(vb);
        }

        auto info = detail::make<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        info.bindingCount = static_cast<std::uint32_t>(raw.size());
        info.pBindings    = raw.data();
        native::DescriptorSetLayout h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateDescriptorSetLayout(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_layout = { d, h };
        m_bindings.assign(bindings.begin(), bindings.end());
        return Result::Success;
    }

    void destroy() noexcept { m_layout.reset(); m_bindings.clear(); }
    bool valid() const noexcept { return static_cast<bool>(m_layout); }
    native::DescriptorSetLayout handle() const noexcept { return m_layout.get(); }
    const std::vector<DescriptorBinding>& bindings() const noexcept { return m_bindings; }
};

class DescriptorSet {
private:
    const detail::DeviceState* m_device = nullptr;
    native::DescriptorSet      m_set    = VK_NULL_HANDLE;

public:
    DescriptorSet() noexcept = default;
    DescriptorSet(const detail::DeviceState* d, native::DescriptorSet s) noexcept : m_device(d), m_set(s) {}

    bool valid() const noexcept { return m_set != VK_NULL_HANDLE; }
    native::DescriptorSet handle() const noexcept { return m_set; }
    const detail::DeviceState* device_state() const noexcept { return m_device; }
};

struct DescriptorPoolSize {
    DescriptorType type  = DescriptorType::UniformBuffer;
    std::uint32_t  count = 0;
};

struct DescriptorPoolDesc {
    std::uint32_t max_sets = 256;
    std::vector<DescriptorPoolSize> sizes;
    bool free_individual_sets = false;
};

class DescriptorPool {
private:
    FIZMO_VK_UNIQUE(VkDescriptorPool, vkDestroyDescriptorPool) m_pool;

public:
    DescriptorPool() noexcept = default;

    Result create(const Device& device, const DescriptorPoolDesc& desc = {}) {
        m_pool.reset();
        if (!device.valid() || desc.max_sets == 0) return Result::InvalidArgument;
        const auto* d = device.state();
        std::vector<VkDescriptorPoolSize> sizes;

        if (desc.sizes.empty()) {
            const std::uint32_t n = desc.max_sets;
            sizes = {
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         n * 2 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         n * 2 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, n * 4 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          n     },
                { VK_DESCRIPTOR_TYPE_SAMPLER,                n     },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          n     },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, n     },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, n     },
            };
        } else {
            for (const auto& s : desc.sizes) sizes.push_back({ to_vk(s.type), s.count });
        }

        auto info = detail::make<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        info.flags         = desc.free_individual_sets ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
        info.maxSets       = desc.max_sets;
        info.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        info.pPoolSizes    = sizes.data();
        native::DescriptorPool h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateDescriptorPool(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_pool = { d, h };
        return Result::Success;
    }

    Result allocate(const DescriptorSetLayout& layout, DescriptorSet& out) noexcept {
        auto* d = m_pool.device();
        if (!d || !layout.valid()) return Result::NotInitialized;
        native::DescriptorSetLayout l = layout.handle();
        auto info = detail::make<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        info.descriptorPool     = m_pool.get();
        info.descriptorSetCount = 1;
        info.pSetLayouts        = &l;
        native::DescriptorSet s = VK_NULL_HANDLE;
        VkResult r = d->fn.vkAllocateDescriptorSets(d->handle, &info, &s);
        if (r != VK_SUCCESS) return to_result(r);
        out = DescriptorSet(d, s);
        return Result::Success;
    }

    Result reset() noexcept {
        auto* d = m_pool.device();
        if (!d) return Result::NotInitialized;
        return to_result(d->fn.vkResetDescriptorPool(d->handle, m_pool.get(), 0));
    }

    void destroy() noexcept { m_pool.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_pool); }
    native::DescriptorPool handle() const noexcept { return m_pool.get(); }
};

class DescriptorWriter {
private:
    struct Entry {
        std::uint32_t          binding;
        std::uint32_t          array_element;
        DescriptorType         type;
        VkDescriptorBufferInfo buffer_info;
        VkDescriptorImageInfo  image_info;
        bool                   is_image;
    };

    std::vector<Entry> m_entries;

public:
    DescriptorWriter& buffer(
        std::uint32_t binding, const Buffer& buf,
        DescriptorType type = DescriptorType::UniformBuffer,
        std::uint64_t offset = 0, std::uint64_t range = kWholeSize,
        std::uint32_t array_element = 0
    ) {
        Entry e{};
        e.binding = binding; e.array_element = array_element; e.type = type; e.is_image = false;
        e.buffer_info = { buf.handle(), offset, range };
        m_entries.push_back(e);
        return *this;
    }

    DescriptorWriter& image(
        std::uint32_t binding, const ImageView& view, const Sampler& sampler,
        ImageLayout layout = ImageLayout::ShaderReadOnly,
        DescriptorType type = DescriptorType::CombinedImageSampler,
        std::uint32_t array_element = 0
    ) {
        Entry e{};
        e.binding = binding; e.array_element = array_element; e.type = type; e.is_image = true;
        e.image_info = { sampler.handle(), view.handle(), to_vk(layout) };
        m_entries.push_back(e);
        return *this;
    }

    DescriptorWriter& storage_image(
        std::uint32_t binding, 
        const ImageView& view,
        ImageLayout layout = ImageLayout::General, 
        std::uint32_t array_element = 0
    ) {
        Entry e{};
        e.binding = binding; e.array_element = array_element; e.type = DescriptorType::StorageImage; e.is_image = true;
        e.image_info = { VK_NULL_HANDLE, view.handle(), to_vk(layout) };
        m_entries.push_back(e);
        return *this;
    }

    void update(const DescriptorSet& set) const {
        const auto* d = set.device_state();
        if (!d || m_entries.empty()) return;
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(m_entries.size());

        for (const auto& e : m_entries) {
            auto w = detail::make<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            w.dstSet          = set.handle();
            w.dstBinding      = e.binding;
            w.dstArrayElement = e.array_element;
            w.descriptorCount = 1;
            w.descriptorType  = to_vk(e.type);
            if (e.is_image) w.pImageInfo  = &e.image_info;
            else            w.pBufferInfo = &e.buffer_info;
            writes.push_back(w);
        }

        d->fn.vkUpdateDescriptorSets(d->handle, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void clear() noexcept { m_entries.clear(); }
};

struct PushConstantRange {
    ShaderStage   stages = ShaderStage::All;
    std::uint32_t offset = 0;
    std::uint32_t size   = 0;    
};

struct PipelineLayoutDesc {
    std::vector<const DescriptorSetLayout*> set_layouts;
    std::vector<PushConstantRange>          push_constants;
};

class PipelineLayout {
private:
    FIZMO_VK_UNIQUE(VkPipelineLayout, vkDestroyPipelineLayout) m_layout;

public:
    PipelineLayout() noexcept = default;

    Result create(const Device& device, const PipelineLayoutDesc& desc = {}) {
        m_layout.reset();
        if (!device.valid()) return Result::NotInitialized;
        const auto* d = device.state();
        std::vector<native::DescriptorSetLayout> sets;

        for (const auto* s : desc.set_layouts) {
            if (!s || !s->valid()) return Result::InvalidArgument;
            sets.push_back(s->handle());
        }

        std::vector<VkPushConstantRange> ranges;
        for (const auto& p : desc.push_constants) ranges.push_back({ to_vk(p.stages), p.offset, p.size });
        auto info = detail::make<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        info.setLayoutCount         = static_cast<std::uint32_t>(sets.size());
        info.pSetLayouts            = sets.data();
        info.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size());
        info.pPushConstantRanges    = ranges.data();
        native::PipelineLayout h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreatePipelineLayout(d->handle, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_layout = { d, h };
        return Result::Success;
    }

    void destroy() noexcept { m_layout.reset(); }
    bool valid() const noexcept { return static_cast<bool>(m_layout); }
    native::PipelineLayout handle() const noexcept { return m_layout.get(); }
};

struct VertexBinding {
    std::uint32_t binding = 0;
    std::uint32_t stride  = 0;
    VertexRate    rate    = VertexRate::PerVertex;
};

struct VertexAttribute {
    std::uint32_t location = 0;
    std::uint32_t binding  = 0;
    Format        format   = Format::RG32Float;
    std::uint32_t offset   = 0;
};

struct BlendState {
    bool        enable    = false;
    BlendFactor src_color = BlendFactor::One;
    BlendFactor dst_color = BlendFactor::Zero;
    BlendOp     color_op  = BlendOp::Add;
    BlendFactor src_alpha = BlendFactor::One;
    BlendFactor dst_alpha = BlendFactor::Zero;
    BlendOp     alpha_op  = BlendOp::Add;
    ColorWrite  write_mask = ColorWrite::All;

    static BlendState opaque() noexcept { return {}; }

    static BlendState alpha() noexcept {          
        BlendState b;
        b.enable = true;
        b.src_color = BlendFactor::SrcAlpha; b.dst_color = BlendFactor::OneMinusSrcAlpha;
        b.src_alpha = BlendFactor::One;      b.dst_alpha = BlendFactor::OneMinusSrcAlpha;
        return b;
    }

    static BlendState premultiplied() noexcept {
        BlendState b;
        b.enable = true;
        b.src_color = BlendFactor::One; b.dst_color = BlendFactor::OneMinusSrcAlpha;
        b.src_alpha = BlendFactor::One; b.dst_alpha = BlendFactor::OneMinusSrcAlpha;
        return b;
    }

    static BlendState additive() noexcept {
        BlendState b;
        b.enable = true;
        b.src_color = BlendFactor::SrcAlpha; b.dst_color = BlendFactor::One;
        b.src_alpha = BlendFactor::One;      b.dst_alpha = BlendFactor::One;
        return b;
    }
};

struct DepthState {
    bool      test    = false;
    bool      write   = false;
    CompareOp compare = CompareOp::Less;
};

struct GraphicsPipelineDesc {
    const ShaderModule*   vertex_shader   = nullptr;
    const ShaderModule*   fragment_shader = nullptr;   
    const char*           vertex_entry    = "main";
    const char*           fragment_entry  = "main";
    const PipelineLayout* layout          = nullptr;

    std::vector<VertexBinding>   vertex_bindings;
    std::vector<VertexAttribute> vertex_attributes;

    Topology    topology     = Topology::TriangleList;
    PolygonMode polygon_mode = PolygonMode::Fill;
    CullMode    cull_mode    = CullMode::None;
    FrontFace   front_face   = FrontFace::CounterClockwise;
    float       line_width   = 1.0f;
    SampleCount samples      = SampleCount::X1;
    DepthState  depth;

    std::vector<Format>     color_formats;
    Format                  depth_format = Format::Undefined;
    std::vector<BlendState> blend;

    const char* name = nullptr;
};

struct ComputePipelineDesc {
    const ShaderModule*   shader = nullptr;
    const char*           entry  = "main";
    const PipelineLayout* layout = nullptr;
    const char*           name   = nullptr;
};

class Pipeline {
private:
    FIZMO_VK_UNIQUE(VkPipeline, vkDestroyPipeline) m_pipeline;
    native::PipelineLayout m_layout = VK_NULL_HANDLE;
    BindPoint              m_bind_point = BindPoint::Graphics;

public:
    Pipeline() noexcept = default;

    Result create(const Device& device, const GraphicsPipelineDesc& desc) {
        m_pipeline.reset();
        if (!device.valid()) return Result::NotInitialized;
        if (!desc.vertex_shader || !desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
        if (desc.polygon_mode != PolygonMode::Fill && !device.features().fill_mode_non_solid) return Result::FeatureNotPresent;
        const auto* d = device.state();
        VkPipelineShaderStageCreateInfo stages[2]{};
        std::uint32_t stage_count = 0;
        stages[stage_count]        = detail::make<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        stages[stage_count].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[stage_count].module = desc.vertex_shader->handle();
        stages[stage_count].pName  = desc.vertex_entry;
        ++stage_count;

        if (desc.fragment_shader) {
            stages[stage_count]        = detail::make<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            stages[stage_count].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[stage_count].module = desc.fragment_shader->handle();
            stages[stage_count].pName  = desc.fragment_entry;
            ++stage_count;
        }

        std::vector<VkVertexInputBindingDescription> bindings;
        for (const auto& b : desc.vertex_bindings) bindings.push_back({ b.binding, b.stride, to_vk(b.rate) });
        std::vector<VkVertexInputAttributeDescription> attributes;
        for (const auto& a : desc.vertex_attributes) attributes.push_back({ a.location, a.binding, to_vk(a.format), a.offset });
        auto vertex_input = detail::make<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
        vertex_input.vertexBindingDescriptionCount   = static_cast<std::uint32_t>(bindings.size());
        vertex_input.pVertexBindingDescriptions      = bindings.data();
        vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertex_input.pVertexAttributeDescriptions    = attributes.data();
        auto input_assembly = detail::make<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
        input_assembly.topology = to_vk(desc.topology);
        auto viewport = detail::make<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
        viewport.viewportCount = 1;
        viewport.scissorCount  = 1;
        auto raster = detail::make<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        raster.polygonMode = to_vk(desc.polygon_mode);
        raster.cullMode    = to_vk(desc.cull_mode);
        raster.frontFace   = to_vk(desc.front_face);
        raster.lineWidth   = device.features().wide_lines ? desc.line_width : 1.0f;
        auto multisample = detail::make<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
        multisample.rasterizationSamples = to_vk(desc.samples);
        auto depth = detail::make<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
        depth.depthTestEnable  = desc.depth.test  ? VK_TRUE : VK_FALSE;
        depth.depthWriteEnable = desc.depth.write ? VK_TRUE : VK_FALSE;
        depth.depthCompareOp   = to_vk(desc.depth.compare);
        depth.maxDepthBounds   = 1.0f;
        std::vector<VkPipelineColorBlendAttachmentState> blend_states;

        for (std::size_t i = 0; i < desc.color_formats.size(); ++i) {
            const BlendState b = desc.blend.empty() ? BlendState::opaque() : desc.blend[std::min(i, desc.blend.size() - 1)];
            VkPipelineColorBlendAttachmentState s{};
            s.blendEnable         = b.enable ? VK_TRUE : VK_FALSE;
            s.srcColorBlendFactor = to_vk(b.src_color);
            s.dstColorBlendFactor = to_vk(b.dst_color);
            s.colorBlendOp        = to_vk(b.color_op);
            s.srcAlphaBlendFactor = to_vk(b.src_alpha);
            s.dstAlphaBlendFactor = to_vk(b.dst_alpha);
            s.alphaBlendOp        = to_vk(b.alpha_op);
            s.colorWriteMask      = to_vk(b.write_mask);
            blend_states.push_back(s);
        }

        auto blend = detail::make<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        blend.attachmentCount = static_cast<std::uint32_t>(blend_states.size());
        blend.pAttachments    = blend_states.data();
        const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        auto dynamic = detail::make<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates    = dynamic_states;
        std::vector<VkFormat> color_formats;
        for (Format f : desc.color_formats) color_formats.push_back(to_vk(f));
        auto rendering = detail::make<VkPipelineRenderingCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
        rendering.colorAttachmentCount    = static_cast<std::uint32_t>(color_formats.size());
        rendering.pColorAttachmentFormats = color_formats.data();
        rendering.depthAttachmentFormat   = to_vk(desc.depth_format);
        rendering.stencilAttachmentFormat = has_stencil(desc.depth_format) ? to_vk(desc.depth_format) : VK_FORMAT_UNDEFINED;
        auto info = detail::make<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
        info.pNext               = &rendering;
        info.stageCount          = stage_count;
        info.pStages             = stages;
        info.pVertexInputState   = &vertex_input;
        info.pInputAssemblyState = &input_assembly;
        info.pViewportState      = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState   = &multisample;
        info.pDepthStencilState  = &depth;
        info.pColorBlendState    = &blend;
        info.pDynamicState       = &dynamic;
        info.layout              = desc.layout->handle();
        native::Pipeline h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateGraphicsPipelines(d->handle, VK_NULL_HANDLE, 1, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_pipeline   = { d, h };
        m_layout     = desc.layout->handle();
        m_bind_point = BindPoint::Graphics;
        if (desc.name) d->set_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(h), desc.name);
        return Result::Success;
    }

    Result create(const Device& device, const ComputePipelineDesc& desc) noexcept {
        m_pipeline.reset();
        if (!device.valid()) return Result::NotInitialized;
        if (!desc.shader || !desc.layout || !desc.layout->valid()) return Result::InvalidArgument;
        const auto* d = device.state();
        auto info = detail::make<VkComputePipelineCreateInfo>(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
        info.stage        = detail::make<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        info.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        info.stage.module = desc.shader->handle();
        info.stage.pName  = desc.entry;
        info.layout       = desc.layout->handle();
        native::Pipeline h = VK_NULL_HANDLE;
        VkResult r = d->fn.vkCreateComputePipelines(d->handle, VK_NULL_HANDLE, 1, &info, nullptr, &h);
        if (r != VK_SUCCESS) return to_result(r);
        m_pipeline   = { d, h };
        m_layout     = desc.layout->handle();
        m_bind_point = BindPoint::Compute;
        if (desc.name) d->set_name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(h), desc.name);
        return Result::Success;
    }

    void destroy() noexcept { m_pipeline.reset(); m_layout = VK_NULL_HANDLE; }
    bool valid() const noexcept { return static_cast<bool>(m_pipeline); }
    native::Pipeline       handle()     const noexcept { return m_pipeline.get(); }
    native::PipelineLayout layout()     const noexcept { return m_layout; }
    BindPoint              bind_point() const noexcept { return m_bind_point; }
};

} // namespace vulkan
} // namespace fizmo

#endif // FIZMO_VULKAN_PIPELINE_HPP