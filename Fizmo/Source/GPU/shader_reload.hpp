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

    bool load(const std::filesystem::path& path, ShaderBundle& out, std::string& error) const;

public:
    explicit ShaderHotReload(system::HotReloader& reloader) : m_reloader(&reloader) {}
    ~ShaderHotReload() { clear(); }

    ShaderHotReload(const ShaderHotReload&) = delete;
    ShaderHotReload& operator=(const ShaderHotReload&) = delete;

    ShaderCompiler& compiler() noexcept { return *m_compiler; }

    WatchId watch(Material& material, const std::filesystem::path& vertex, const std::filesystem::path& fragment);

    WatchId watch_compute(Material& material, const std::filesystem::path& compute);

    void clear() {
        for (WatchId id : m_ids) m_reloader->unwatch(id);
        m_ids.clear();
    }
};

Result create_material(Device& device, Material& out, const std::filesystem::path& vertex, const std::filesystem::path& fragment, MaterialDesc desc = {}, std::string* log = nullptr);

Result create_compute_material(Device& device, Material& out, const std::filesystem::path& compute, MaterialDesc desc = {}, std::string* log = nullptr);

} // namespace gpu
} // namespace fizmo

#endif // FIZMO_GPU_SHADER_RELOAD_HPP
