#include "ies.hpp"
#include "math.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace fizmo {
namespace lighting {

namespace {

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

float interpolate(const std::vector<float>& axis, float x, std::size_t& i0, std::size_t& i1) {
    if (axis.size() <= 1) {
        i0 = i1 = 0;
        return 0.0f;
    }
    if (x <= axis.front()) {
        i0 = i1 = 0;
        return 0.0f;
    }
    if (x >= axis.back()) {
        i0 = i1 = axis.size() - 1;
        return 0.0f;
    }
    const auto it = std::upper_bound(axis.begin(), axis.end(), x);
    i1 = static_cast<std::size_t>(it - axis.begin());
    i0 = i1 - 1;
    const float span = axis[i1] - axis[i0];
    return span > 0.0f ? (x - axis[i0]) / span : 0.0f;
}

} // namespace

std::optional<IesProfile> IesProfile::parse(const std::string& text, std::string* error) {
    std::istringstream in(text);
    std::string line;
    bool tilt_found = false;
    bool tilt_include = false;
    while (std::getline(in, line)) {
        const auto pos = line.find("TILT=");
        if (pos != std::string::npos) {
            tilt_found = true;
            tilt_include = line.find("INCLUDE", pos) != std::string::npos;
            break;
        }
    }
    if (!tilt_found) {
        fail(error, "IES file has no TILT= line");
        return std::nullopt;
    }
    std::vector<double> numbers;
    std::string token;
    while (in >> token) {
        std::replace(token.begin(), token.end(), ',', ' ');
        std::istringstream part(token);
        double v = 0.0;
        while (part >> v) numbers.push_back(v);
    }
    std::size_t at = 0;
    auto take = [&](double& out) {
        if (at >= numbers.size()) return false;
        out = numbers[at++];
        return true;
    };
    if (tilt_include) {
        double geometry = 0.0, pairs = 0.0;
        if (!take(geometry) || !take(pairs)) {
            fail(error, "IES tilt data is cut short");
            return std::nullopt;
        }
        at += static_cast<std::size_t>(pairs) * 2;
    }
    double lamps = 0, lumens = 0, multiplier = 1, nv = 0, nh = 0, type = 1, units = 1, w = 0, l = 0, h = 0, ballast = 1, future = 1, watts = 0;
    if (!take(lamps) || !take(lumens) || !take(multiplier) || !take(nv) || !take(nh) || !take(type) || !take(units) || !take(w) || !take(l) || !take(h) || !take(ballast) || !take(future) || !take(watts)) {
        fail(error, "IES header numbers are cut short");
        return std::nullopt;
    }
    const std::size_t v_count = static_cast<std::size_t>(nv), h_count = static_cast<std::size_t>(nh);
    if (v_count == 0 || h_count == 0 || numbers.size() < at + v_count + h_count + v_count * h_count) {
        fail(error, "IES angle or candela tables are cut short");
        return std::nullopt;
    }
    IesProfile p;
    p.m_vertical.assign(numbers.begin() + static_cast<std::ptrdiff_t>(at), numbers.begin() + static_cast<std::ptrdiff_t>(at + v_count));
    at += v_count;
    p.m_horizontal.assign(numbers.begin() + static_cast<std::ptrdiff_t>(at), numbers.begin() + static_cast<std::ptrdiff_t>(at + h_count));
    at += h_count;
    p.m_values.resize(v_count * h_count);
    const double scale = multiplier * ballast;
    for (std::size_t i = 0; i < v_count * h_count; ++i) p.m_values[i] = static_cast<float>(numbers[at + i] * scale);
    if (lumens > 0.0) p.m_lumens = static_cast<float>(lumens * lamps);
    p.finish();
    return p;
}

std::optional<IesProfile> IesProfile::load_file(const std::string& path, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error) *error = "could not open " + path;
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), error);
}

IesProfile IesProfile::from_table(std::vector<float> vertical_degrees, std::vector<float> horizontal_degrees, std::vector<float> candela) {
    IesProfile p;
    p.m_vertical = std::move(vertical_degrees);
    p.m_horizontal = std::move(horizontal_degrees);
    if (p.m_horizontal.empty()) p.m_horizontal.push_back(0.0f);
    p.m_values = std::move(candela);
    p.m_values.resize(p.m_vertical.size() * p.m_horizontal.size(), 0.0f);
    p.finish();
    return p;
}

void IesProfile::finish() {
    m_max = 0.0f;
    for (float v : m_values) m_max = std::max(m_max, v);
}

float IesProfile::candela(float vertical_degrees, float horizontal_degrees) const noexcept {
    if (m_vertical.empty() || m_values.empty()) return 0.0f;
    float hdeg = std::fmod(horizontal_degrees, 360.0f);
    if (hdeg < 0.0f) hdeg += 360.0f;
    const float last = m_horizontal.back();
    if (last <= 0.0f) hdeg = 0.0f;
    else if (last <= 90.0f) {
        hdeg = std::fmod(hdeg, 180.0f);
        if (hdeg > 90.0f) hdeg = 180.0f - hdeg;
    } else if (last <= 180.0f) {
        if (hdeg > 180.0f) hdeg = 360.0f - hdeg;
    }
    std::size_t v0, v1, h0, h1;
    const float tv = interpolate(m_vertical, vertical_degrees, v0, v1);
    const float th = interpolate(m_horizontal, hdeg, h0, h1);
    const float a = value(h0, v0) + (value(h0, v1) - value(h0, v0)) * tv;
    const float b = value(h1, v0) + (value(h1, v1) - value(h1, v0)) * tv;
    return a + (b - a) * th;
}

float IesProfile::normalized(float vertical_degrees, float horizontal_degrees) const noexcept {
    return m_max > 0.0f ? candela(vertical_degrees, horizontal_degrees) / m_max : 0.0f;
}

float IesProfile::total_lumens() const noexcept {
    if (m_lumens > 0.0f) return m_lumens;
    double sum = 0.0;
    const int nt = 90, np = 72;
    for (int i = 0; i < nt; ++i) {
        const double t0 = 180.0 * i / nt, t1 = 180.0 * (i + 1) / nt;
        const double band = 2.0 * kPi * (std::cos(t0 * kDegToRad) - std::cos(t1 * kDegToRad)) / np;
        for (int j = 0; j < np; ++j) sum += band * candela(static_cast<float>((t0 + t1) * 0.5), static_cast<float>(360.0 * (j + 0.5) / np));
    }
    return static_cast<float>(sum);
}

} // namespace lighting
} // namespace fizmo
