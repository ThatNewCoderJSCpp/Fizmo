#include "fizmo_library.hpp"
#include "types.hpp"

namespace fizmo {
namespace gpu {

const char* adapter_type_name(AdapterType type) noexcept {
    switch (type) {
        case AdapterType::Integrated: return "integrated";
        case AdapterType::Discrete:   return "discrete";
        case AdapterType::Virtual:    return "virtual";
        case AdapterType::Cpu:        return "cpu";
        default:                      return "unknown";
    }
}

std::uint32_t vendor_id_from_name(const std::string& vendor) noexcept {
    std::string v;
    v.reserve(vendor.size());
    for (char ch : vendor) v.push_back(static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch));
    if (v.find("nvidia") != std::string::npos) return 0x10DE;
    if (v.find("amd") != std::string::npos || v.find("ati ") != std::string::npos || v.find("advanced micro") != std::string::npos) return 0x1002;
    if (v.find("intel") != std::string::npos) return 0x8086;
    if (v.find("apple") != std::string::npos) return 0x106B;
    if (v.find("qualcomm") != std::string::npos) return 0x5143;
    if (v.find("arm") != std::string::npos) return 0x13B5;
    if (v.find("microsoft") != std::string::npos) return 0x1414;
    return 0;
}

auto BlendState::make(BlendFactor sc, BlendFactor dc, BlendFactor sa, BlendFactor da) noexcept -> BlendState {
    BlendState b;
    b.enable = true;
    b.src_color = sc; b.dst_color = dc;
    b.src_alpha = sa; b.dst_alpha = da;
    return b;
}

auto BlendState::alpha() noexcept -> BlendState { return make(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, BlendFactor::One, BlendFactor::OneMinusSrcAlpha); }

auto BlendState::premultiplied() noexcept -> BlendState { return make(BlendFactor::One, BlendFactor::OneMinusSrcAlpha, BlendFactor::One, BlendFactor::OneMinusSrcAlpha); }

} // namespace gpu
} // namespace fizmo
