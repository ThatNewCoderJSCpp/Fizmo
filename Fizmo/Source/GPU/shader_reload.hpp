#ifndef FIZMO_GPU_SHADER_RELOAD_HPP
#define FIZMO_GPU_SHADER_RELOAD_HPP

#include "material.hpp"
#include "shader_bundle.hpp"
#include "../System/hot_reload.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace gpu {

class ShaderHotReload {
public:
    using WatchId = system::HotReloader::WatchId;

private:
    system::HotReloader*              m_reloader;
    std::shared_ptr<ShaderCompiler>   m_compiler = std::make_shared<ShaderCompiler>();
    std::vector<WatchId>              m_ids;

    bool load(const std::filesystem::path& path, ShaderBundle& out, std::string& error) const {
        if (path.extension() == ".fzsh") return out.load(path, &error);
        return m_compiler->compile(path, out, &error);
    }

public:
    explicit ShaderHotReload(system::HotReloader& reloader) : m_reloader(&reloader) {}
    ~ShaderHotReload() { clear(); }

    ShaderHotReload(const ShaderHotReload&) = delete;
    ShaderHotReload& operator=(const ShaderHotReload&) = delete;

    ShaderCompiler& compiler() noexcept { return *m_compiler; }

    WatchId watch(Material& material, const std::filesystem::path& vertex, const std::filesystem::path& fragment) {
        Material* m = &material;
        const WatchId a = m_reloader->watch(vertex, system::HotReloader::Reload([this, m](const std::filesystem::path& p, std::string& error) {
            ShaderBundle b;
            if (!load(p, b, error)) return false;
            if (failed(m->reload(&b, nullptr))) { error = m->error(); return false; }
            return true;
        }));
        const WatchId b = m_reloader->watch(fragment, system::HotReloader::Reload([this, m](const std::filesystem::path& p, std::string& error) {
            ShaderBundle sb;
            if (!load(p, sb, error)) return false;
            if (failed(m->reload(nullptr, &sb))) { error = m->error(); return false; }
            return true;
        }));
        if (a) m_ids.push_back(a);
        if (b) m_ids.push_back(b);
        return a && b ? a : 0;
    }

    WatchId watch_compute(Material& material, const std::filesystem::path& compute) {
        Material* m = &material;
        const WatchId id = m_reloader->watch(compute, system::HotReloader::Reload([this, m](const std::filesystem::path& p, std::string& error) {
            ShaderBundle b;
            if (!load(p, b, error)) return false;
            if (failed(m->reload(nullptr, nullptr, &b))) { error = m->error(); return false; }
            return true;
        }));
        if (id) m_ids.push_back(id);
        return id;
    }

    void clear() {
        for (WatchId id : m_ids) m_reloader->unwatch(id);
        m_ids.clear();
    }
};

inline Result create_material(Device& device, Material& out, const std::filesystem::path& vertex, const std::filesystem::path& fragment, MaterialDesc desc = {}, std::string* log = nullptr) {
    if (!load_shader(vertex, desc.vertex, log)) return Result::ShaderFailed;
    if (!load_shader(fragment, desc.fragment, log)) return Result::ShaderFailed;
    const Result r = out.create(device, std::move(desc));
    if (failed(r) && log) *log = out.error();
    return r;
}

inline Result create_compute_material(Device& device, Material& out, const std::filesystem::path& compute, MaterialDesc desc = {}, std::string* log = nullptr) {
    if (!load_shader(compute, desc.compute, log)) return Result::ShaderFailed;
    const Result r = out.create(device, std::move(desc));
    if (failed(r) && log) *log = out.error();
    return r;
}

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_SHADER_RELOAD_HPP
