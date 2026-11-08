#ifndef FIZMO_LIGHTING_IES_HPP
#define FIZMO_LIGHTING_IES_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fizmo {
namespace lighting {

class IesProfile {
public:
    static std::optional<IesProfile> parse(const std::string& text, std::string* error = nullptr);
    static std::optional<IesProfile> load_file(const std::string& path, std::string* error = nullptr);
    static IesProfile from_table(std::vector<float> vertical_degrees, std::vector<float> horizontal_degrees, std::vector<float> candela);

    float candela(float vertical_degrees, float horizontal_degrees) const noexcept;
    float normalized(float vertical_degrees, float horizontal_degrees) const noexcept;
    float max_candela() const noexcept { return m_max; }
    float lumens() const noexcept { return m_lumens; }
    float total_lumens() const noexcept;
    const std::vector<float>& vertical_angles() const noexcept { return m_vertical; }
    const std::vector<float>& horizontal_angles() const noexcept { return m_horizontal; }

private:
    void finish();
    float value(std::size_t h, std::size_t v) const noexcept { return m_values[h * m_vertical.size() + v]; }

    std::vector<float> m_vertical;
    std::vector<float> m_horizontal;
    std::vector<float> m_values;
    float m_max = 0.0f;
    float m_lumens = -1.0f;
};

} // namespace lighting
} // namespace fizmo

#endif // FIZMO_LIGHTING_IES_HPP
