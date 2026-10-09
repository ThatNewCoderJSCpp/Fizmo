#ifndef FIZMO_MODEL_3D_HPP
#define FIZMO_MODEL_3D_HPP

#include "mesh.hpp"
#include "texture.hpp"
#include "../Util Hpp/json.hpp"
#include "../System/paths.hpp"
#include "../System/hot_reload.hpp"
#include "../System/jobs.hpp"
#include "../Windows/renderer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace graphics {

namespace model_math {

using Mat4 = std::array<double, 16>;

struct Quat {
    double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
};

inline Mat4 identity() noexcept { return { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }; }

Mat4 mul(const Mat4& a, const Mat4& b) noexcept;

Quat normalize(Quat q) noexcept;

Quat slerp(Quat a, Quat b, double t) noexcept;

Mat4 trs(const vector3d& t, const Quat& q0, const vector3d& s) noexcept;

inline Mat4 from_column_major(const float* m) noexcept {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) r[rr * 4 + c] = m[c * 4 + rr];
    return r;
}

Mat4 inverse(const Mat4& m) noexcept;

vector3d point(const Mat4& m, double x, double y, double z) noexcept;

inline vector3d direction(const Mat4& m, double x, double y, double z) noexcept {
    return { m[0] * x + m[1] * y + m[2] * z, m[4] * x + m[5] * y + m[6] * z, m[8] * x + m[9] * y + m[10] * z };
}

math::Matrix4d to_matrix(const Mat4& m) noexcept;

Mat4 from_matrix(const math::Matrix4d& m) noexcept;

} // namespace model_math

struct ModelImportOptions {
    bool         z_up             = true;
    double       scale            = 1.0;
    SampleFilter filter           = SampleFilter::Bilinear;
    bool         generate_normals = true;
    bool         smooth_normals   = true;
    bool         flip_v           = false;
};

struct ModelMaterial {
    std::string name;
    Color       base_color = Color(255, 255, 255, 255);
    int         texture    = -1;
    float       metallic   = 0.0f;
    float       roughness  = 0.8f;
    Color       emissive   = Color(0, 0, 0, 255);
    float       emissive_strength = 0.0f;
    bool        double_sided = false;
    bool        transparent  = false;
    float       alpha_cutoff = -1.0f;
    bool        unlit        = false;
};

struct ModelPrimitive {
    Mesh3D                                   mesh;
    int                                      material = -1;
    std::vector<std::array<std::uint16_t, 4>> joints;
    std::vector<std::array<float, 4>>         weights;
    bool skinned() const noexcept;
};

struct ModelMeshData {
    std::string                 name;
    std::vector<ModelPrimitive> primitives;
};

struct ModelNode {
    std::string            name;
    int                    parent = -1;
    std::vector<int>       children;
    vector3d               translation{ 0.0, 0.0, 0.0 };
    model_math::Quat       rotation;
    vector3d               scale{ 1.0, 1.0, 1.0 };
    bool                   has_matrix = false;
    model_math::Mat4       matrix = model_math::identity();
    int                    mesh = -1;
    int                    skin = -1;
};

struct ModelSkin {
    std::string                   name;
    std::vector<int>              joints;
    std::vector<model_math::Mat4> inverse_bind;
};

enum class AnimPath : std::uint8_t { Translation = 0, Rotation, Scale };
enum class AnimInterpolation : std::uint8_t { Linear = 0, Step, CubicSpline };

struct AnimChannel {
    int                node = -1;
    AnimPath           path = AnimPath::Translation;
    AnimInterpolation  interpolation = AnimInterpolation::Linear;
    std::vector<float> times;
    std::vector<float> values;
};

struct ModelAnimation {
    std::string              name;
    std::vector<AnimChannel> channels;
    float                    duration = 0.0f;
};

struct NodePose {
    vector3d         translation{ 0.0, 0.0, 0.0 };
    model_math::Quat rotation;
    vector3d         scale{ 1.0, 1.0, 1.0 };
};

class Model3D {
public:
    std::vector<ModelNode>         nodes;
    std::vector<ModelMeshData>     meshes;
    std::vector<ModelMaterial>     materials;
    std::vector<Texture>           textures;
    std::vector<ModelSkin>         skins;
    std::vector<ModelAnimation>    animations;
    std::vector<int>               roots;
    model_math::Mat4               import_transform = model_math::identity();
    std::string                    source;

private:
    static std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }

    static void make_import(Model3D& m, const ModelImportOptions& o);

    static void generate_normals(Mesh3D& mesh, bool smooth);

    static std::vector<std::uint8_t> base64(const std::string& s);

    struct GltfContext {
        json::Value                            doc;
        std::vector<std::vector<std::uint8_t>> buffers;
        std::filesystem::path                  dir;
        std::string                            error;
    };

    static bool accessor_view(const GltfContext& g, int index, const std::uint8_t*& base, std::size_t& stride, std::size_t& count, int& comps, int& ctype, bool& normalized, std::size_t& avail);

    static double read_component(const std::uint8_t* p, int ctype, bool normalized) noexcept;

    static bool read_accessor(const GltfContext& g, int index, std::vector<double>& out, int& comps);

    static bool load_buffers(GltfContext& g, const std::vector<std::uint8_t>* glb_bin);

    static std::string uri_decode(const std::string& s);

    static bool load_gltf_doc(GltfContext& g, Model3D& m, const ModelImportOptions& o);

public:
    static bool load_gltf(const std::filesystem::path& path, Model3D& out, std::string* error = nullptr, const ModelImportOptions& options = {});

    static bool load_obj(const std::filesystem::path& path, Model3D& out, std::string* error = nullptr, const ModelImportOptions& options = {});

    static bool load(const std::filesystem::path& path, Model3D& out, std::string* error = nullptr, const ModelImportOptions& options = {});

    const Texture* texture_of(int material) const noexcept;

    Material3D material3d(int material, const Material3D* override_material = nullptr) const noexcept;

    std::vector<NodePose> rest_pose() const;

    void world_matrices(const std::vector<NodePose>& pose, std::vector<model_math::Mat4>& world) const;

    int find_node(const std::string& name) const noexcept;

    int find_animation(const std::string& name) const noexcept;

    std::size_t vertex_count() const noexcept;

    void bounds(vector3d& lo, vector3d& hi) const;

    void draw(windows::Renderer& r, const math::Matrix4d& transform = math::Matrix4d::identity(), const Material3D* override_material = nullptr) const;
};

class ModelAnimator {
private:
    struct Track {
        int    animation = -1;
        double time = 0.0;
        double speed = 1.0;
        bool   loop = true;
        double weight = 1.0;
    };

    const Model3D*                 m_model = nullptr;
    Track                          m_current;
    Track                          m_previous;
    double                         m_fade = 0.0;
    double                         m_fade_elapsed = 0.0;
    std::vector<NodePose>          m_pose;
    std::vector<NodePose>          m_scratch;
    std::vector<model_math::Mat4>  m_world;
    std::vector<std::vector<Mesh3D>> m_skinned;
    bool                           m_finished = false;

    static double sample_index(const std::vector<float>& times, double t, std::size_t& k) noexcept;

    static void sample_channel(const AnimChannel& c, double t, NodePose& out);

    void apply(const Track& tr, std::vector<NodePose>& pose) const;

    static void advance(Track& tr, double duration, double dt, bool& finished);

    void skin();

public:
    ModelAnimator() = default;
    explicit ModelAnimator(const Model3D& model) { attach(model); }

    void attach(const Model3D& model);

    const Model3D* model() const noexcept { return m_model; }

    bool play(int animation, bool loop = true, double fade_seconds = 0.0, double speed = 1.0);

    bool play(const std::string& name, bool loop = true, double fade_seconds = 0.0, double speed = 1.0) {
        return m_model && play(m_model->find_animation(name), loop, fade_seconds, speed);
    }

    void stop() { m_current = Track{}; m_previous = Track{}; if (m_model) m_pose = m_model->rest_pose(); refresh(); }
    void set_speed(double s) noexcept { m_current.speed = s; }
    void set_time(double t) noexcept { m_current.time = t; }
    double time() const noexcept { return m_current.time; }
    int current() const noexcept { return m_current.animation; }
    bool finished() const noexcept { return m_finished; }

    double duration() const noexcept;

    const std::vector<NodePose>& pose() const noexcept { return m_pose; }
    std::vector<NodePose>& pose() noexcept { return m_pose; }
    const std::vector<model_math::Mat4>& world() const noexcept { return m_world; }

    vector3d node_position(int node) const noexcept;

    void refresh() {
        if (!m_model) return;
        m_model->world_matrices(m_pose, m_world);
        skin();
    }

    void update(double dt);

    void draw(windows::Renderer& r, const math::Matrix4d& transform = math::Matrix4d::identity(), const Material3D* override_material = nullptr) const;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_MODEL_3D_HPP
