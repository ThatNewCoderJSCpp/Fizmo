#ifndef FIZMO_CHEMISTRY_EMISSION_HPP
#define FIZMO_CHEMISTRY_EMISSION_HPP

#include "common.hpp"
#include "constants.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace chemistry {

struct EmissionLine {
    std::uint8_t atomic_number = 0;
    std::uint8_t ion = 1;
    double wavelength_nm = 0.0;
    double intensity = 0.0;
};

struct CieXyz {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct LinearRgb {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

struct Srgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct Spectrum {
    double start_nm = 380.0;
    double step_nm = 1.0;
    std::vector<double> intensity;

    std::size_t size() const noexcept { return intensity.size(); }
    double wavelength(std::size_t i) const noexcept { return start_nm + step_nm * static_cast<double>(i); }
    double end_nm() const noexcept { return intensity.empty() ? start_nm : wavelength(intensity.size() - 1); }
    double at(double nm) const noexcept;
    double peak() const noexcept;
};

struct SpectrumFit {
    bool ok = false;
    std::string message;
    std::vector<std::pair<int, double>> percentages;
    std::vector<std::pair<int, double>> weights;
    double residual = NO_VALUE;
    double r_squared = NO_VALUE;
    Spectrum fitted;
    std::vector<int> candidates;
};

struct ColorFit {
    bool ok = false;
    std::string message;
    std::vector<std::pair<int, double>> percentages;
    Srgb8 matched;
    double distance = NO_VALUE;
    bool unique = true;
};

const std::vector<EmissionLine>& emission_lines();
std::vector<EmissionLine> emission_lines_of(int atomic_number, double min_nm = 0.0, double max_nm = 1e9);
std::vector<int> elements_with_emission_lines(double min_nm = 380.0, double max_nm = 780.0);
EmissionLine strongest_emission_line(int atomic_number, double min_nm = 380.0, double max_nm = 780.0);

bool has_flame_color(int atomic_number) noexcept;
Srgb8 flame_color(int atomic_number) noexcept;
const char* flame_color_name(int atomic_number) noexcept;

CieXyz cie_color_matching(double wavelength_nm) noexcept;
CieXyz spectrum_xyz(const Spectrum& spectrum) noexcept;
LinearRgb xyz_to_linear_srgb(const CieXyz& xyz) noexcept;
LinearRgb srgb8_to_linear(const Srgb8& c) noexcept;
Srgb8 linear_to_srgb8(const LinearRgb& c, bool normalize = true) noexcept;
Srgb8 wavelength_color(double wavelength_nm) noexcept;
Srgb8 spectrum_color(const Spectrum& spectrum) noexcept;
LinearRgb spectrum_linear_rgb(const Spectrum& spectrum) noexcept;
std::string to_hex(const Srgb8& c);
bool parse_hex_color(const std::string& text, Srgb8& out) noexcept;

Spectrum element_emission_template(int atomic_number, double fwhm_nm = 1.0, double start_nm = 380.0, double end_nm = 780.0, double step_nm = 0.5);
Spectrum emission_spectrum(const std::vector<std::pair<int, double>>& fractions, double fwhm_nm = 1.0, double start_nm = 380.0, double end_nm = 780.0, double step_nm = 0.5);
Spectrum absorption_spectrum(const std::vector<std::pair<int, double>>& fractions, double fwhm_nm = 1.0, double depth = 0.9, double start_nm = 380.0, double end_nm = 780.0, double step_nm = 0.5);
Spectrum resample_spectrum(const Spectrum& spectrum, double start_nm, double end_nm, double step_nm);
Spectrum spectrum_from_samples(const std::vector<std::pair<double, double>>& samples, double start_nm, double end_nm, double step_nm);
Spectrum spectrum_from_lines(const std::vector<std::pair<double, double>>& lines, double fwhm_nm, double start_nm, double end_nm, double step_nm);

std::vector<double> nonnegative_least_squares(const std::vector<std::vector<double>>& columns, const std::vector<double>& target);
SpectrumFit fit_emission_spectrum(const Spectrum& observed, std::vector<int> candidates = {}, double fwhm_nm = 1.0, double min_percent = 0.5);
ColorFit fit_color_mixture(const Srgb8& target, const std::vector<int>& candidates, bool flame = true);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_EMISSION_HPP
