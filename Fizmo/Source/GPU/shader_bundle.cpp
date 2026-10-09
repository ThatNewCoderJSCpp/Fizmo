#include "fizmo_library.hpp"
#include "shader_bundle.hpp"

namespace fizmo {
namespace gpu {

std::uint32_t ShaderMember::element_size() const noexcept {
    if (columns > 1) return (matrix_stride ? matrix_stride : rows * scalar_size()) * (columns - 1) + rows * scalar_size();
    return rows * scalar_size();
}

std::uint32_t ShaderBundle::Reader::u32() {
    if (end - p < 4) { ok = false; return 0; }
    std::uint32_t v = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    p += 4;
    return v;
}

std::string ShaderBundle::Reader::str() {
    const std::uint32_t n = u32();
    if (!ok || static_cast<std::size_t>(end - p) < n) { ok = false; return {}; }
    std::string s(reinterpret_cast<const char*>(p), n);
    p += n;
    return s;
}

auto ShaderBundle::find_block(const std::string& name) const noexcept -> const ShaderBlock* {
    if (!m_data) return nullptr;
    for (const ShaderBlock& b : m_data->blocks) if (b.name == name) return &b;
    return nullptr;
}

auto ShaderBundle::desc() const noexcept -> ShaderDesc {
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

bool ShaderBundle::parse(const std::uint8_t* bytes, std::size_t size, std::string* error) {
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

bool ShaderBundle::load(const std::filesystem::path& path, std::string* error) {
    const auto bytes = system::paths::read_bytes(path);
    if (!bytes) { if (error) *error = "cannot read " + system::paths::to_utf8(path); return false; }
    if (!parse(bytes->data(), bytes->size(), error)) return false;
    m_data->source = system::paths::to_utf8(path);
    return true;
}

std::string ShaderCompiler::default_tool() {
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

std::string ShaderCompiler::default_python() {
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

bool ShaderCompiler::compile(const std::filesystem::path& source, ShaderBundle& out, std::string* log) const {
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

bool load_shader(const std::filesystem::path& path, ShaderBundle& out, std::string* log) {
    const std::string ext = path.extension().string();
    if (ext == ".fzsh") return out.load(path, log);
    return ShaderCompiler().compile(path, out, log);
}

} // namespace gpu
} // namespace fizmo
