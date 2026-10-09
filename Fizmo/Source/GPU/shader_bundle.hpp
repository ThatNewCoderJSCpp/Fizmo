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

    std::uint32_t element_size() const noexcept {
        if (columns > 1) return (matrix_stride ? matrix_stride : rows * scalar_size()) * (columns - 1) + rows * scalar_size();
        return rows * scalar_size();
    }
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

        std::uint32_t u32() {
            if (end - p < 4) { ok = false; return 0; }
            std::uint32_t v = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
            p += 4;
            return v;
        }

        std::string str() {
            const std::uint32_t n = u32();
            if (!ok || static_cast<std::size_t>(end - p) < n) { ok = false; return {}; }
            std::string s(reinterpret_cast<const char*>(p), n);
            p += n;
            return s;
        }
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

    const ShaderBlock* find_block(const std::string& name) const noexcept {
        if (!m_data) return nullptr;
        for (const ShaderBlock& b : m_data->blocks) if (b.name == name) return &b;
        return nullptr;
    }

    ShaderDesc desc() const noexcept {
        ShaderDesc d;
        if (!m_data) return d;
        d.stage         = m_data->stage;
        d.spirv         = { m_data->words.data(), m_data->words.size() };
        d.glsl          = &m_data->piece;
        d.glsl_pieces   = 1;
        d.glsl_version  = m_data->version;
        d.glsl_bindings = { m_data->bindings.data(), m_data->bindings.size() };
        d.name          = m_data->name.c_str();
        return d;
    }

    bool parse(const std::uint8_t* bytes, std::size_t size, std::string* error = nullptr) {
        auto fail = [&](const char* why) { if (error) *error = why; return false; };
        if (size < 16 || std::memcmp(bytes, "FZSH", 4) != 0) return fail("not a fizmo shader bundle");
        Reader r{ bytes + 4, bytes + size };
        if (r.u32() != 1) return fail("unsupported shader bundle version");
        auto d = std::make_shared<Data>();
        d->stage   = static_cast<ShaderStage>(r.u32());
        d->version = static_cast<int>(r.u32());
        d->name    = r.str();
        d->glsl    = r.str();
        const std::uint32_t nb = r.u32();

        for (std::uint32_t i = 0; r.ok && i < nb; ++i) {
            d->binding_names.push_back(r.str());
            GlslBinding b;
            b.kind    = static_cast<GlslResource>(r.u32());
            b.set     = r.u32();
            b.binding = r.u32();
            b.count   = r.u32();
            d->bindings.push_back(b);
        }

        const std::uint32_t nblocks = r.u32();
        for (std::uint32_t i = 0; r.ok && i < nblocks; ++i) {
            ShaderBlock blk;
            blk.name    = r.str();
            blk.kind    = static_cast<BlockKind>(r.u32());
            blk.set     = r.u32();
            blk.binding = r.u32();
            blk.size    = r.u32();
            const std::uint32_t nm = r.u32();

            for (std::uint32_t k = 0; r.ok && k < nm; ++k) {
                ShaderMember m;
                m.name          = r.str();
                m.offset        = r.u32();
                m.type          = static_cast<ScalarType>(r.u32());
                m.rows          = r.u32();
                m.columns       = r.u32();
                m.array_count   = r.u32();
                m.array_stride  = r.u32();
                m.matrix_stride = r.u32();
                blk.members.push_back(std::move(m));
            }

            d->blocks.push_back(std::move(blk));
        }

        const std::uint32_t ni = r.u32();
        for (std::uint32_t i = 0; r.ok && i < ni; ++i) {
            ShaderInput in;
            in.name       = r.str();
            in.location   = r.u32();
            in.type       = static_cast<ScalarType>(r.u32());
            in.components = r.u32();
            d->inputs.push_back(std::move(in));
        }

        const std::uint32_t nw = r.u32();
        if (!r.ok || static_cast<std::size_t>(r.end - r.p) < static_cast<std::size_t>(nw) * 4) return fail("truncated shader bundle");
        d->words.resize(nw);
        for (std::uint32_t i = 0; i < nw; ++i) d->words[i] = r.u32();
        if (!r.ok || nw == 0) return fail("truncated shader bundle");
        if (d->stage != ShaderStage::Vertex && d->stage != ShaderStage::Fragment && d->stage != ShaderStage::Compute) return fail("bad shader stage in bundle");
        for (std::size_t i = 0; i < d->bindings.size(); ++i) d->bindings[i].name = d->binding_names[i].c_str();
        d->piece = d->glsl.c_str();
        m_data = std::move(d);
        return true;
    }

    bool load(const std::filesystem::path& path, std::string* error = nullptr) {
        const auto bytes = system::paths::read_bytes(path);
        if (!bytes) { if (error) *error = "cannot read " + system::paths::to_utf8(path); return false; }
        if (!parse(bytes->data(), bytes->size(), error)) return false;
        m_data->source = system::paths::to_utf8(path);
        return true;
    }

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

    static std::string default_tool() {
        const char* env = std::getenv("FIZMO_SHADER_TOOL");
        if (env && *env) return env;
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path here = fs::path(__FILE__).parent_path();
        const fs::path candidates[] = {
            here / ".." / ".." / "tools" / "gen_gl_shaders.py",
            system::paths::executable_dir() / "tools" / "gen_gl_shaders.py",
            system::paths::current_dir() / "tools" / "gen_gl_shaders.py",
        };
        for (const fs::path& c : candidates) if (fs::exists(c, ec)) return system::paths::to_utf8(fs::weakly_canonical(c, ec));
        return {};
    }

    static std::string default_python() {
        const char* env = std::getenv("FIZMO_PYTHON");
        if (env && *env) return env;
#if defined(OS_WINDOWS)
        const char* names[] = { "python", "py", "python3" };
#else
        const char* names[] = { "python3", "python" };
#endif
        for (const char* n : names) if (!system::find_program(n).empty()) return n;
        return {};
    }

public:
    ShaderCompiler() : m_python(default_python()), m_tool(default_tool()) {}

    void set_python(const std::string& python) { m_python = python; }
    void set_tool(const std::string& tool) { m_tool = tool; }
    void set_versions(int gl_version, int compute_version) noexcept { m_gl_version = gl_version; m_compute_version = compute_version; }
    const std::string& tool() const noexcept { return m_tool; }
    bool available() const { return !m_python.empty() && !m_tool.empty(); }

    bool compile(const std::filesystem::path& source, ShaderBundle& out, std::string* log = nullptr) const {
        namespace fs = std::filesystem;
        if (!available()) { if (log) *log = "shader tool or python not found (set FIZMO_SHADER_TOOL / FIZMO_PYTHON)"; return false; }
        std::error_code ec;
        static std::atomic<std::uint64_t> counter{ 0 };
        const fs::path dir = system::paths::temp_dir() / ("fizmo_shaders_" + std::to_string(counter.fetch_add(1)) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(dir, ec);
        const std::vector<std::string> args = {
            m_tool, system::paths::to_utf8(source), "--bundle", system::paths::to_utf8(dir),
            "--gl-version", std::to_string(m_gl_version), "--compute-version", std::to_string(m_compute_version),
        };
        const system::ProcessResult r = system::run_process(m_python, args);
        bool ok = false;

        if (!r.started) {
            if (log) *log = "could not run " + m_python;
        } else if (r.exit_code != 0) {
            if (log) *log = r.output;
        } else {
            std::string err;
            ok = out.load(dir / (source.filename().string() + ".fzsh"), &err);
            if (ok) {
                out.m_data->source = system::paths::to_utf8(source);
                if (log) *log = r.output;
            } else if (log) {
                *log = err;
            }
        }

        fs::remove_all(dir, ec);
        return ok;
    }

    std::optional<ShaderBundle> compile(const std::filesystem::path& source, std::string* log = nullptr) const {
        ShaderBundle b;
        if (!compile(source, b, log)) return std::nullopt;
        return b;
    }
};

inline bool load_shader(const std::filesystem::path& path, ShaderBundle& out, std::string* log = nullptr) {
    const std::string ext = path.extension().string();
    if (ext == ".fzsh") return out.load(path, log);
    return ShaderCompiler().compile(path, out, log);
}

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_SHADER_BUNDLE_HPP
