#ifndef FIZMO_GPU_SHADER_BUNDLE_HPP
#define FIZMO_GPU_SHADER_BUNDLE_HPP

#include "types.hpp"
#include "../System/paths.hpp"
#include "../System/process.hpp"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace gpu {

enum class ScalarType : std::uint8_t { Unknown = 0, Float, Int, UInt, Bool, Double };
enum class BlockKind : std::uint8_t { Uniform = 0, Push, Storage };

struct ShaderMember {
    std::string   name;
    std::uint32_t offset        = 0;
    ScalarType    type          = ScalarType::Unknown;
    std::uint32_t rows          = 1;
    std::uint32_t columns       = 1;
    std::uint32_t array_count   = 1;
    std::uint32_t array_stride  = 0;
    std::uint32_t matrix_stride = 0;

    std::uint32_t scalar_size() const noexcept { return type == ScalarType::Double ? 8u : 4u; }
    bool is_matrix() const noexcept { return columns > 1; }

    std::uint32_t element_size() const noexcept;
};

struct ShaderBlock {
    std::string               name;
    BlockKind                 kind    = BlockKind::Uniform;
    std::uint32_t             set     = 0;
    std::uint32_t             binding = 0;
    std::uint32_t             size    = 0;
    std::vector<ShaderMember> members;

    const ShaderMember* find(const std::string& member) const noexcept {
        for (const ShaderMember& m : members) if (m.name == member) return &m;
        return nullptr;
    }
};

struct ShaderInput {
    std::string   name;
    std::uint32_t location   = 0;
    ScalarType    type       = ScalarType::Float;
    std::uint32_t components = 1;
};

class ShaderCompiler;

class ShaderBundle {
private:
    friend class ShaderCompiler;

    struct Data {
        ShaderStage               stage   = ShaderStage::None;
        int                       version = 330;
        std::string               name;
        std::string               glsl;
        const char*               piece   = nullptr;
        std::vector<std::string>  binding_names;
        std::vector<GlslBinding>  bindings;
        std::vector<ShaderBlock>  blocks;
        std::vector<ShaderInput>  inputs;
        std::vector<std::uint32_t> words;
        std::string               source;
    };

    std::shared_ptr<Data> m_data;

    struct Reader {
        const std::uint8_t* p;
        const std::uint8_t* end;
        bool                ok = true;

        std::uint32_t u32();

        std::string str();
    };

public:
    ShaderBundle() = default;

    bool valid() const noexcept { return m_data && !m_data->words.empty(); }
    ShaderStage stage() const noexcept { return m_data ? m_data->stage : ShaderStage::None; }
    const std::string& name() const noexcept { static const std::string none; return m_data ? m_data->name : none; }
    const std::string& source() const noexcept { static const std::string none; return m_data ? m_data->source : none; }
    const std::string& glsl() const noexcept { static const std::string none; return m_data ? m_data->glsl : none; }
    const std::vector<GlslBinding>& bindings() const noexcept { static const std::vector<GlslBinding> none; return m_data ? m_data->bindings : none; }
    const std::vector<ShaderBlock>& blocks() const noexcept { static const std::vector<ShaderBlock> none; return m_data ? m_data->blocks : none; }
    const std::vector<ShaderInput>& inputs() const noexcept { static const std::vector<ShaderInput> none; return m_data ? m_data->inputs : none; }
    const std::vector<std::uint32_t>& spirv() const noexcept { static const std::vector<std::uint32_t> none; return m_data ? m_data->words : none; }

    const ShaderBlock* find_block(const std::string& name) const noexcept;

    ShaderDesc desc() const noexcept;

    bool parse(const std::uint8_t* bytes, std::size_t size, std::string* error = nullptr);

    bool load(const std::filesystem::path& path, std::string* error = nullptr);

    static std::optional<ShaderBundle> from_file(const std::filesystem::path& path, std::string* error = nullptr) {
        ShaderBundle b;
        if (!b.load(path, error)) return std::nullopt;
        return b;
    }
};

class ShaderCompiler {
private:
    std::string m_python;
    std::string m_tool;
    int         m_gl_version      = 410;
    int         m_compute_version = 430;

    static std::string default_tool();

    static std::string default_python();

public:
    ShaderCompiler() : m_python(default_python()), m_tool(default_tool()) {}

    void set_python(const std::string& python) { m_python = python; }
    void set_tool(const std::string& tool) { m_tool = tool; }
    void set_versions(int gl_version, int compute_version) noexcept { m_gl_version = gl_version; m_compute_version = compute_version; }
    const std::string& tool() const noexcept { return m_tool; }
    bool available() const { return !m_python.empty() && !m_tool.empty(); }

    bool compile(const std::filesystem::path& source, ShaderBundle& out, std::string* log = nullptr) const;

    std::optional<ShaderBundle> compile(const std::filesystem::path& source, std::string* log = nullptr) const {
        ShaderBundle b;
        if (!compile(source, b, log)) return std::nullopt;
        return b;
    }
};

bool load_shader(const std::filesystem::path& path, ShaderBundle& out, std::string* log = nullptr);

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_SHADER_BUNDLE_HPP
