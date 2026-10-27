#include "emission.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace fizmo {
namespace chemistry {

namespace {

constexpr EmissionLine LINES[] = {
    { 1, 1, 656.279, 1000 }, { 1, 1, 486.135, 360 }, { 1, 1, 434.047, 180 }, { 1, 1, 410.174, 100 }, { 1, 1, 397.007, 60 }, { 1, 1, 388.905, 40 },
    { 2, 1, 587.562, 1000 }, { 2, 1, 447.148, 400 }, { 2, 1, 388.865, 900 }, { 2, 1, 501.568, 200 }, { 2, 1, 667.815, 200 }, { 2, 1, 706.519, 140 }, { 2, 1, 492.193, 100 }, { 2, 1, 471.315, 60 }, { 2, 1, 402.619, 100 }, { 2, 1, 728.135, 50 },
    { 3, 1, 670.776, 1000 }, { 3, 1, 610.354, 90 }, { 3, 1, 460.286, 20 }, { 3, 1, 413.256, 10 }, { 3, 1, 812.645, 40 },
    { 4, 1, 457.266, 300 }, { 4, 2, 313.042, 1000 }, { 4, 2, 467.342, 100 },
    { 5, 1, 249.773, 1000 }, { 5, 1, 249.677, 500 }, { 5, 1, 412.193, 30 }, { 5, 1, 518.0, 120 }, { 5, 1, 548.0, 100 }, { 5, 1, 580.0, 60 },
    { 6, 1, 247.856, 1000 }, { 6, 1, 909.483, 400 }, { 6, 1, 833.515, 300 }, { 6, 1, 538.034, 60 }, { 6, 2, 426.726, 200 }, { 6, 2, 658.288, 150 }, { 6, 2, 657.805, 120 },
    { 7, 1, 746.831, 1000 }, { 7, 1, 744.229, 700 }, { 7, 1, 742.364, 500 }, { 7, 1, 868.028, 800 }, { 7, 2, 500.515, 300 }, { 7, 2, 567.956, 300 }, { 7, 2, 463.054, 200 }, { 7, 2, 399.500, 200 },
    { 8, 1, 777.194, 1000 }, { 8, 1, 777.417, 800 }, { 8, 1, 777.539, 600 }, { 8, 1, 844.636, 600 }, { 8, 1, 615.818, 100 }, { 8, 1, 645.598, 60 }, { 8, 1, 533.074, 50 }, { 8, 2, 441.490, 150 }, { 8, 2, 464.913, 150 },
    { 9, 1, 685.603, 1000 }, { 9, 1, 690.248, 500 }, { 9, 1, 703.747, 600 }, { 9, 1, 712.789, 300 }, { 9, 1, 739.868, 300 },
    { 10, 1, 585.249, 1000 }, { 10, 1, 640.225, 900 }, { 10, 1, 703.241, 700 }, { 10, 1, 692.947, 600 }, { 10, 1, 659.895, 500 }, { 10, 1, 650.653, 600 }, { 10, 1, 633.443, 500 }, { 10, 1, 621.728, 300 }, { 10, 1, 614.306, 600 }, { 10, 1, 609.616, 300 }, { 10, 1, 594.483, 500 }, { 10, 1, 588.190, 300 }, { 10, 1, 540.056, 200 }, { 10, 1, 724.517, 400 },
    { 11, 1, 588.995, 1000 }, { 11, 1, 589.592, 500 }, { 11, 1, 568.820, 15 }, { 11, 1, 568.263, 8 }, { 11, 1, 498.281, 5 }, { 11, 1, 615.423, 5 }, { 11, 1, 616.075, 8 }, { 11, 1, 818.326, 40 }, { 11, 1, 819.482, 50 },
    { 12, 1, 285.213, 1000 }, { 12, 1, 518.360, 300 }, { 12, 1, 517.268, 200 }, { 12, 1, 516.732, 100 }, { 12, 1, 383.829, 200 }, { 12, 1, 470.299, 50 }, { 12, 1, 552.841, 60 }, { 12, 2, 279.553, 900 }, { 12, 2, 448.113, 100 },
    { 13, 1, 396.152, 1000 }, { 13, 1, 394.401, 600 }, { 13, 1, 308.215, 500 }, { 13, 1, 309.271, 800 }, { 13, 2, 466.306, 100 }, { 13, 2, 704.206, 60 },
    { 14, 1, 288.158, 1000 }, { 14, 1, 251.611, 800 }, { 14, 1, 390.552, 300 }, { 14, 2, 634.710, 150 }, { 14, 2, 637.136, 120 }, { 14, 2, 413.089, 80 },
    { 15, 1, 253.565, 1000 }, { 15, 1, 255.326, 600 }, { 15, 2, 602.418, 100 }, { 15, 2, 604.312, 80 },
    { 16, 1, 921.287, 1000 }, { 16, 1, 922.811, 700 }, { 16, 1, 469.412, 50 }, { 16, 2, 545.388, 200 }, { 16, 2, 564.000, 150 },
    { 17, 1, 837.594, 1000 }, { 17, 1, 858.597, 600 }, { 17, 1, 725.662, 300 }, { 17, 2, 479.454, 300 }, { 17, 2, 481.006, 250 },
    { 18, 1, 696.543, 900 }, { 18, 1, 706.722, 700 }, { 18, 1, 750.387, 1000 }, { 18, 1, 763.511, 1000 }, { 18, 1, 738.398, 600 }, { 18, 1, 772.376, 600 }, { 18, 1, 794.818, 500 }, { 18, 1, 811.531, 900 }, { 18, 1, 801.479, 500 }, { 18, 1, 842.465, 600 }, { 18, 2, 487.986, 200 }, { 18, 2, 476.487, 150 }, { 18, 2, 434.806, 150 }, { 18, 2, 480.602, 100 }, { 18, 1, 420.068, 120 }, { 18, 1, 415.859, 100 },
    { 19, 1, 766.490, 1000 }, { 19, 1, 769.896, 500 }, { 19, 1, 404.414, 30 }, { 19, 1, 404.721, 15 }, { 19, 1, 691.108, 10 }, { 19, 1, 693.877, 10 }, { 19, 1, 580.175, 5 },
    { 20, 1, 422.673, 1000 }, { 20, 2, 393.366, 900 }, { 20, 2, 396.847, 800 }, { 20, 1, 445.478, 100 }, { 20, 1, 443.496, 80 }, { 20, 1, 430.253, 80 }, { 20, 1, 558.876, 100 }, { 20, 1, 612.222, 120 }, { 20, 1, 616.217, 120 }, { 20, 1, 643.907, 150 }, { 20, 1, 646.257, 80 }, { 20, 1, 649.378, 80 }, { 20, 2, 849.802, 60 }, { 20, 2, 854.209, 100 }, { 20, 2, 866.214, 80 },
    { 21, 1, 391.181, 1000 }, { 21, 1, 402.369, 800 }, { 21, 2, 424.683, 500 }, { 21, 2, 361.384, 400 }, { 21, 1, 567.182, 80 }, { 21, 1, 604.020, 60 },
    { 22, 1, 498.173, 1000 }, { 22, 1, 499.107, 800 }, { 22, 1, 453.324, 700 }, { 22, 1, 365.350, 900 }, { 22, 1, 521.039, 300 }, { 22, 2, 334.941, 800 },
    { 23, 1, 437.924, 1000 }, { 23, 1, 411.178, 900 }, { 23, 1, 438.472, 700 }, { 23, 1, 318.540, 800 }, { 23, 2, 309.311, 600 },
    { 24, 1, 425.433, 1000 }, { 24, 1, 427.480, 800 }, { 24, 1, 428.972, 600 }, { 24, 1, 357.869, 900 }, { 24, 1, 520.844, 300 }, { 24, 1, 520.450, 200 }, { 24, 1, 529.829, 100 },
    { 25, 1, 403.076, 1000 }, { 25, 1, 403.307, 800 }, { 25, 1, 403.449, 600 }, { 25, 1, 279.482, 900 }, { 25, 1, 482.352, 100 }, { 25, 1, 475.404, 80 }, { 25, 1, 601.349, 40 },
    { 26, 1, 371.994, 1000 }, { 26, 1, 373.486, 800 }, { 26, 1, 385.991, 600 }, { 26, 1, 404.581, 500 }, { 26, 1, 438.355, 500 }, { 26, 1, 440.475, 400 }, { 26, 1, 432.576, 400 }, { 26, 1, 427.176, 300 }, { 26, 1, 495.760, 150 }, { 26, 1, 516.749, 150 }, { 26, 1, 527.036, 150 }, { 26, 1, 532.804, 200 }, { 26, 1, 537.149, 150 }, { 26, 1, 561.564, 100 }, { 26, 2, 259.940, 900 },
    { 27, 1, 345.350, 1000 }, { 27, 1, 340.512, 900 }, { 27, 1, 412.132, 200 }, { 27, 1, 389.408, 150 }, { 27, 2, 228.616, 600 },
    { 28, 1, 341.476, 1000 }, { 28, 1, 352.454, 900 }, { 28, 1, 361.939, 700 }, { 28, 1, 547.691, 60 }, { 28, 1, 508.051, 30 },
    { 29, 1, 324.754, 1000 }, { 29, 1, 327.396, 800 }, { 29, 1, 510.554, 200 }, { 29, 1, 515.324, 300 }, { 29, 1, 521.820, 350 }, { 29, 1, 578.213, 80 }, { 29, 1, 465.112, 40 },
    { 30, 1, 213.857, 1000 }, { 30, 1, 481.053, 400 }, { 30, 1, 472.216, 300 }, { 30, 1, 468.014, 200 }, { 30, 1, 636.234, 250 }, { 30, 1, 330.259, 200 },
    { 31, 1, 417.204, 1000 }, { 31, 1, 403.299, 700 }, { 31, 1, 294.364, 600 },
    { 32, 1, 265.118, 1000 }, { 32, 1, 303.906, 800 }, { 32, 1, 422.657, 100 },
    { 33, 1, 193.696, 1000 }, { 33, 1, 234.984, 500 }, { 33, 1, 286.044, 300 },
    { 34, 1, 196.026, 1000 }, { 34, 1, 473.084, 50 },
    { 35, 1, 827.244, 1000 }, { 35, 1, 863.866, 600 }, { 35, 2, 470.486, 200 }, { 35, 2, 478.550, 200 }, { 35, 2, 481.671, 150 },
    { 36, 1, 557.029, 600 }, { 36, 1, 587.092, 800 }, { 36, 1, 760.155, 1000 }, { 36, 1, 811.290, 1000 }, { 36, 1, 450.235, 300 }, { 36, 1, 445.392, 200 }, { 36, 1, 431.958, 200 }, { 36, 1, 427.397, 200 }, { 36, 2, 435.548, 200 }, { 36, 1, 769.454, 500 },
    { 37, 1, 780.027, 1000 }, { 37, 1, 794.760, 600 }, { 37, 1, 420.180, 40 }, { 37, 1, 421.553, 20 }, { 37, 1, 572.448, 10 },
    { 38, 1, 460.733, 1000 }, { 38, 2, 407.771, 900 }, { 38, 2, 421.552, 600 }, { 38, 1, 640.847, 100 }, { 38, 1, 650.402, 80 }, { 38, 1, 638.076, 60 }, { 38, 1, 707.010, 80 }, { 38, 1, 687.833, 60 }, { 38, 1, 483.207, 50 }, { 38, 1, 548.153, 40 },
    { 39, 1, 410.238, 1000 }, { 39, 1, 412.831, 700 }, { 39, 2, 371.030, 800 }, { 39, 2, 437.494, 500 }, { 39, 1, 643.500, 100 },
    { 40, 1, 360.119, 1000 }, { 40, 1, 468.780, 300 }, { 40, 2, 343.823, 800 }, { 40, 1, 473.946, 200 },
    { 41, 1, 405.894, 1000 }, { 41, 1, 407.973, 700 }, { 41, 1, 410.092, 500 },
    { 42, 1, 379.825, 1000 }, { 42, 1, 386.410, 900 }, { 42, 1, 390.296, 800 }, { 42, 1, 550.649, 300 }, { 42, 1, 553.305, 250 }, { 42, 1, 557.045, 200 },
    { 46, 1, 340.458, 1000 }, { 46, 1, 363.470, 800 }, { 46, 1, 360.955, 600 },
    { 47, 1, 328.068, 1000 }, { 47, 1, 338.289, 700 }, { 47, 1, 520.908, 150 }, { 47, 1, 546.550, 200 }, { 47, 1, 827.352, 60 },
    { 48, 1, 228.802, 1000 }, { 48, 1, 643.847, 500 }, { 48, 1, 508.582, 400 }, { 48, 1, 479.992, 300 }, { 48, 1, 467.815, 200 }, { 48, 1, 361.051, 300 },
    { 49, 1, 451.131, 1000 }, { 49, 1, 410.176, 800 }, { 49, 1, 325.609, 500 }, { 49, 1, 303.936, 400 },
    { 50, 1, 283.999, 1000 }, { 50, 1, 326.234, 800 }, { 50, 1, 452.474, 300 }, { 50, 1, 380.102, 300 }, { 50, 2, 645.351, 100 }, { 50, 2, 684.405, 80 },
    { 51, 1, 206.833, 1000 }, { 51, 1, 231.147, 600 }, { 51, 1, 259.805, 500 },
    { 52, 1, 214.281, 1000 }, { 52, 1, 238.578, 500 },
    { 53, 1, 804.374, 1000 }, { 53, 1, 905.833, 800 }, { 53, 2, 516.119, 300 }, { 53, 2, 546.462, 300 }, { 53, 2, 562.587, 150 },
    { 54, 1, 467.123, 600 }, { 54, 1, 462.427, 500 }, { 54, 1, 473.415, 300 }, { 54, 1, 480.702, 300 }, { 54, 1, 492.315, 300 }, { 54, 1, 823.163, 1000 }, { 54, 1, 828.012, 800 }, { 54, 1, 881.941, 900 }, { 54, 1, 764.202, 400 }, { 54, 2, 484.433, 300 }, { 54, 2, 529.222, 300 }, { 54, 2, 541.915, 300 },
    { 55, 1, 852.113, 1000 }, { 55, 1, 894.347, 600 }, { 55, 1, 455.528, 50 }, { 55, 1, 459.317, 25 }, { 55, 1, 672.328, 10 }, { 55, 1, 697.327, 10 },
    { 56, 1, 553.548, 1000 }, { 56, 2, 455.403, 900 }, { 56, 2, 493.409, 500 }, { 56, 2, 614.171, 300 }, { 56, 2, 649.690, 200 }, { 56, 1, 705.994, 150 }, { 56, 1, 577.762, 80 }, { 56, 1, 582.627, 40 },
    { 57, 2, 394.910, 800 }, { 57, 2, 408.672, 1000 }, { 57, 2, 433.374, 700 }, { 57, 1, 550.134, 100 },
    { 63, 2, 381.967, 1000 }, { 63, 2, 412.970, 900 }, { 63, 1, 459.403, 600 }, { 63, 1, 462.722, 500 }, { 63, 1, 466.188, 500 },
    { 72, 1, 307.288, 1000 }, { 72, 1, 368.224, 300 },
    { 74, 1, 400.875, 1000 }, { 74, 1, 429.461, 600 }, { 74, 1, 430.211, 500 }, { 74, 1, 407.436, 400 },
    { 78, 1, 265.945, 1000 }, { 78, 1, 306.471, 800 }, { 78, 1, 530.101, 100 },
    { 79, 1, 242.795, 1000 }, { 79, 1, 267.595, 900 }, { 79, 1, 312.278, 300 }, { 79, 1, 479.260, 60 }, { 79, 1, 627.817, 200 },
    { 80, 1, 253.652, 1000 }, { 80, 1, 404.656, 300 }, { 80, 1, 435.833, 600 }, { 80, 1, 546.074, 800 }, { 80, 1, 576.960, 250 }, { 80, 1, 579.066, 250 }, { 80, 1, 365.015, 400 }, { 80, 1, 407.783, 60 }, { 80, 1, 491.607, 30 }, { 80, 1, 690.746, 30 },
    { 81, 1, 535.046, 1000 }, { 81, 1, 377.572, 900 }, { 81, 1, 351.924, 500 }, { 81, 1, 276.787, 600 },
    { 82, 1, 405.781, 1000 }, { 82, 1, 368.346, 800 }, { 82, 1, 283.305, 900 }, { 82, 1, 363.957, 500 }, { 82, 1, 373.994, 200 }, { 82, 1, 722.896, 60 },
    { 83, 1, 306.772, 1000 }, { 83, 1, 289.798, 600 }, { 83, 1, 472.252, 200 },
    { 92, 2, 385.958, 1000 }, { 92, 2, 409.013, 700 }, { 92, 1, 591.539, 300 },
};

struct FlameEntry {
    int z;
    Srgb8 color;
    const char* name;
};

constexpr FlameEntry FLAMES[] = {
    { 3, { 230, 30, 60 }, "crimson red" },
    { 5, { 80, 220, 60 }, "bright green" },
    { 11, { 255, 175, 20 }, "intense yellow-orange" },
    { 12, { 245, 245, 255 }, "brilliant white (burning metal)" },
    { 15, { 140, 220, 200 }, "pale bluish green" },
    { 19, { 200, 140, 255 }, "lilac" },
    { 20, { 255, 100, 40 }, "orange-red (brick red)" },
    { 25, { 190, 230, 90 }, "yellowish green" },
    { 26, { 255, 190, 60 }, "gold" },
    { 29, { 40, 200, 170 }, "blue-green" },
    { 30, { 150, 230, 210 }, "pale bluish green" },
    { 33, { 110, 160, 255 }, "light blue" },
    { 34, { 120, 170, 255 }, "azure blue" },
    { 37, { 210, 60, 140 }, "red-violet" },
    { 38, { 255, 30, 40 }, "bright red" },
    { 42, { 180, 230, 110 }, "yellowish green" },
    { 49, { 90, 80, 220 }, "indigo blue" },
    { 51, { 170, 230, 170 }, "pale green" },
    { 52, { 160, 230, 160 }, "pale green" },
    { 55, { 120, 100, 255 }, "blue-violet" },
    { 56, { 170, 255, 110 }, "pale apple green" },
    { 81, { 60, 230, 60 }, "pure green" },
    { 82, { 170, 190, 240 }, "pale blue" },
};

double lobe(double x, double mu, double s1, double s2) {
    const double t = (x - mu) / (x < mu ? s1 : s2);
    return std::exp(-0.5 * t * t);
}

double srgb_gamma(double v) {
    v = std::max(0.0, std::min(1.0, v));
    return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

double srgb_linear(double v) {
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

Spectrum blank(double start, double end, double step) {
    Spectrum s;
    s.start_nm = start;
    s.step_nm = step > 0.0 ? step : 1.0;
    const std::size_t n = static_cast<std::size_t>(std::max(1.0, std::floor((end - start) / s.step_nm + 1e-9) + 1.0));
    s.intensity.assign(n, 0.0);
    return s;
}

void add_line(Spectrum& s, double nm, double strength, double fwhm) {
    const double sigma = std::max(1e-6, fwhm / 2.354820045);
    const double reach = 5.0 * sigma;
    const double lo = std::max(0.0, std::floor((nm - reach - s.start_nm) / s.step_nm));
    const double hi = std::min(static_cast<double>(s.size()) - 1.0, std::ceil((nm + reach - s.start_nm) / s.step_nm));
    if (hi < 0.0 || lo > static_cast<double>(s.size()) - 1.0) return;
    for (std::size_t i = static_cast<std::size_t>(lo); i <= static_cast<std::size_t>(hi); ++i) {
        const double d = (s.wavelength(i) - nm) / sigma;
        s.intensity[i] += strength * std::exp(-0.5 * d * d);
    }
}

double norm2(const std::vector<double>& v) {
    double t = 0.0;
    for (double x : v) t += x * x;
    return t;
}

} // namespace

double Spectrum::at(double nm) const noexcept {
    if (intensity.empty()) return 0.0;
    const double f = (nm - start_nm) / step_nm;
    if (f <= 0.0) return intensity.front();
    if (f >= static_cast<double>(intensity.size() - 1)) return intensity.back();
    const std::size_t i = static_cast<std::size_t>(f);
    const double w = f - static_cast<double>(i);
    return intensity[i] * (1.0 - w) + intensity[i + 1] * w;
}

double Spectrum::peak() const noexcept {
    double m = 0.0;
    for (double v : intensity) m = std::max(m, v);
    return m;
}

const std::vector<EmissionLine>& emission_lines() {
    static const std::vector<EmissionLine> all(std::begin(LINES), std::end(LINES));
    return all;
}

std::vector<EmissionLine> emission_lines_of(int z, double min_nm, double max_nm) {
    std::vector<EmissionLine> out;
    for (const EmissionLine& l : LINES) {
        if (l.atomic_number == z && l.wavelength_nm >= min_nm && l.wavelength_nm <= max_nm) out.push_back(l);
    }
    std::sort(out.begin(), out.end(), [](const EmissionLine& a, const EmissionLine& b) { return a.wavelength_nm < b.wavelength_nm; });
    return out;
}

std::vector<int> elements_with_emission_lines(double min_nm, double max_nm) {
    std::vector<int> out;
    for (const EmissionLine& l : LINES) {
        if (l.wavelength_nm >= min_nm && l.wavelength_nm <= max_nm && std::find(out.begin(), out.end(), l.atomic_number) == out.end()) out.push_back(l.atomic_number);
    }
    std::sort(out.begin(), out.end());
    return out;
}

EmissionLine strongest_emission_line(int z, double min_nm, double max_nm) {
    EmissionLine best;
    for (const EmissionLine& l : LINES) {
        if (l.atomic_number == z && l.wavelength_nm >= min_nm && l.wavelength_nm <= max_nm && l.intensity > best.intensity) best = l;
    }
    return best;
}

bool has_flame_color(int z) noexcept {
    for (const FlameEntry& f : FLAMES) {
        if (f.z == z) return true;
    }
    return false;
}

Srgb8 flame_color(int z) noexcept {
    for (const FlameEntry& f : FLAMES) {
        if (f.z == z) return f.color;
    }
    return Srgb8{};
}

const char* flame_color_name(int z) noexcept {
    for (const FlameEntry& f : FLAMES) {
        if (f.z == z) return f.name;
    }
    return "";
}

CieXyz cie_lobes(double nm) noexcept {
    CieXyz c;
    c.x = 1.056 * lobe(nm, 599.8, 37.9, 31.0) + 0.362 * lobe(nm, 442.0, 16.0, 26.7) - 0.065 * lobe(nm, 501.1, 20.4, 26.2);
    c.y = 0.821 * lobe(nm, 568.8, 46.9, 40.5) + 0.286 * lobe(nm, 530.9, 16.3, 31.1);
    c.z = 1.217 * lobe(nm, 437.0, 11.8, 36.0) + 0.681 * lobe(nm, 459.0, 26.0, 13.8);
    return c;
}

CieXyz cie_color_matching(double nm) noexcept {
    const double edge = nm > 650.0 ? 650.0 : nm < 430.0 ? 430.0 : nm;
    if (edge == nm) return cie_lobes(nm);
    const CieXyz base = cie_lobes(edge);
    const CieXyz here = cie_lobes(nm);
    const double k = (here.x + here.y + here.z) / std::max(1e-30, base.x + base.y + base.z);
    return CieXyz{ base.x * k, base.y * k, base.z * k };
}

CieXyz spectrum_xyz(const Spectrum& s) noexcept {
    CieXyz out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const CieXyz c = cie_color_matching(s.wavelength(i));
        out.x += c.x * s.intensity[i] * s.step_nm;
        out.y += c.y * s.intensity[i] * s.step_nm;
        out.z += c.z * s.intensity[i] * s.step_nm;
    }
    return out;
}

LinearRgb xyz_to_linear_srgb(const CieXyz& c) noexcept {
    LinearRgb o;
    o.r = 3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z;
    o.g = -0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z;
    o.b = 0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z;
    return o;
}

LinearRgb srgb8_to_linear(const Srgb8& c) noexcept {
    return LinearRgb{ srgb_linear(c.r / 255.0), srgb_linear(c.g / 255.0), srgb_linear(c.b / 255.0) };
}

Srgb8 linear_to_srgb8(const LinearRgb& in, bool normalize) noexcept {
    LinearRgb c = in;
    const double low = std::min({ c.r, c.g, c.b });
    if (low < 0.0) {
        c.r -= low;
        c.g -= low;
        c.b -= low;
    }
    if (normalize) {
        const double high = std::max({ c.r, c.g, c.b });
        if (high > 0.0) {
            c.r /= high;
            c.g /= high;
            c.b /= high;
        }
    }
    const auto to8 = [](double v) { return static_cast<std::uint8_t>(std::lround(255.0 * srgb_gamma(v))); };
    return Srgb8{ to8(c.r), to8(c.g), to8(c.b) };
}

Srgb8 wavelength_color(double nm) noexcept {
    return linear_to_srgb8(xyz_to_linear_srgb(cie_color_matching(nm)), true);
}

LinearRgb spectrum_linear_rgb(const Spectrum& s) noexcept {
    return xyz_to_linear_srgb(spectrum_xyz(s));
}

Srgb8 spectrum_color(const Spectrum& s) noexcept {
    return linear_to_srgb8(spectrum_linear_rgb(s), true);
}

std::string to_hex(const Srgb8& c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

bool parse_hex_color(const std::string& text, Srgb8& out) noexcept {
    std::string h;
    for (char ch : text) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) h += ch;
    }
    if (h.size() == 3) h = std::string{ h[0], h[0], h[1], h[1], h[2], h[2] };
    if (h.size() != 6) return false;
    const auto byte = [&](std::size_t i) { return static_cast<std::uint8_t>(std::stoi(h.substr(i, 2), nullptr, 16)); };
    out = Srgb8{ byte(0), byte(2), byte(4) };
    return true;
}

Spectrum element_emission_template(int z, double fwhm, double start, double end, double step) {
    Spectrum s = blank(start, end, step);
    for (const EmissionLine& l : LINES) {
        if (l.atomic_number == z) add_line(s, l.wavelength_nm, l.intensity, fwhm);
    }
    double area = 0.0;
    for (double v : s.intensity) area += v * s.step_nm;
    if (area > 0.0) {
        for (double& v : s.intensity) v /= area;
    }
    return s;
}

Spectrum emission_spectrum(const std::vector<std::pair<int, double>>& fractions, double fwhm, double start, double end, double step) {
    Spectrum s = blank(start, end, step);
    double total = 0.0;
    for (const auto& f : fractions) total += std::max(0.0, f.second);
    if (!(total > 0.0)) return s;
    for (const auto& f : fractions) {
        if (!(f.second > 0.0)) continue;
        const Spectrum t = element_emission_template(f.first, fwhm, start, end, step);
        for (std::size_t i = 0; i < s.size() && i < t.size(); ++i) s.intensity[i] += t.intensity[i] * f.second / total;
    }
    return s;
}

Spectrum absorption_spectrum(const std::vector<std::pair<int, double>>& fractions, double fwhm, double depth, double start, double end, double step) {
    Spectrum e = emission_spectrum(fractions, fwhm, start, end, step);
    const double peak = e.peak();
    Spectrum s = blank(start, end, step);
    for (std::size_t i = 0; i < s.size(); ++i) s.intensity[i] = 1.0 - (peak > 0.0 ? depth * e.intensity[i] / peak : 0.0);
    return s;
}

Spectrum resample_spectrum(const Spectrum& in, double start, double end, double step) {
    Spectrum s = blank(start, end, step);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nm = s.wavelength(i);
        s.intensity[i] = nm < in.start_nm || nm > in.end_nm() ? 0.0 : in.at(nm);
    }
    return s;
}

Spectrum spectrum_from_samples(const std::vector<std::pair<double, double>>& samples, double start, double end, double step) {
    Spectrum s = blank(start, end, step);
    if (samples.empty()) return s;
    std::vector<std::pair<double, double>> pts = samples;
    std::sort(pts.begin(), pts.end());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nm = s.wavelength(i);
        if (nm < pts.front().first || nm > pts.back().first) continue;
        auto it = std::lower_bound(pts.begin(), pts.end(), std::make_pair(nm, -1e300));
        if (it == pts.begin()) {
            s.intensity[i] = it->second;
            continue;
        }
        if (it == pts.end()) {
            s.intensity[i] = pts.back().second;
            continue;
        }
        const auto& b = *it;
        const auto& a = *(it - 1);
        const double w = b.first > a.first ? (nm - a.first) / (b.first - a.first) : 0.0;
        s.intensity[i] = a.second * (1.0 - w) + b.second * w;
    }
    return s;
}

Spectrum spectrum_from_lines(const std::vector<std::pair<double, double>>& lines, double fwhm, double start, double end, double step) {
    Spectrum s = blank(start, end, step);
    for (const auto& l : lines) add_line(s, l.first, l.second, fwhm);
    return s;
}

std::vector<double> nonnegative_least_squares(const std::vector<std::vector<double>>& a, const std::vector<double>& b) {
    const std::size_t n = a.size();
    std::vector<double> x(n, 0.0);
    if (n == 0) return x;
    const std::size_t m = b.size();
    const auto ata = [&](std::size_t i, std::size_t j) {
        double t = 0.0;
        for (std::size_t k = 0; k < m; ++k) t += a[i][k] * a[j][k];
        return t;
    };
    std::vector<std::vector<double>> g(n, std::vector<double>(n, 0.0));
    std::vector<double> h(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i; j < n; ++j) g[i][j] = g[j][i] = ata(i, j);
        for (std::size_t k = 0; k < m; ++k) h[i] += a[i][k] * b[k];
    }
    std::vector<bool> passive(n, false);
    const auto solve_passive = [&](std::vector<double>& z) {
        std::vector<std::size_t> idx;
        for (std::size_t i = 0; i < n; ++i) {
            if (passive[i]) idx.push_back(i);
        }
        z.assign(n, 0.0);
        const std::size_t p = idx.size();
        if (p == 0) return;
        std::vector<std::vector<double>> mtx(p, std::vector<double>(p + 1, 0.0));
        for (std::size_t r = 0; r < p; ++r) {
            for (std::size_t c = 0; c < p; ++c) mtx[r][c] = g[idx[r]][idx[c]];
            mtx[r][r] += 1e-14 * (1.0 + g[idx[r]][idx[r]]);
            mtx[r][p] = h[idx[r]];
        }
        for (std::size_t c = 0; c < p; ++c) {
            std::size_t piv = c;
            for (std::size_t r = c + 1; r < p; ++r) {
                if (std::fabs(mtx[r][c]) > std::fabs(mtx[piv][c])) piv = r;
            }
            std::swap(mtx[c], mtx[piv]);
            if (std::fabs(mtx[c][c]) < 1e-300) continue;
            for (std::size_t r = 0; r < p; ++r) {
                if (r == c) continue;
                const double f = mtx[r][c] / mtx[c][c];
                for (std::size_t k = c; k <= p; ++k) mtx[r][k] -= f * mtx[c][k];
            }
        }
        for (std::size_t r = 0; r < p; ++r) z[idx[r]] = std::fabs(mtx[r][r]) > 1e-300 ? mtx[r][p] / mtx[r][r] : 0.0;
    };
    const double scale = 1e-12 * (1.0 + norm2(h));
    for (int outer = 0; outer < static_cast<int>(3 * n + 10); ++outer) {
        std::vector<double> w(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            w[i] = h[i];
            for (std::size_t j = 0; j < n; ++j) w[i] -= g[i][j] * x[j];
        }
        int best = -1;
        double bw = scale;
        for (std::size_t i = 0; i < n; ++i) {
            if (!passive[i] && w[i] > bw) {
                bw = w[i];
                best = static_cast<int>(i);
            }
        }
        if (best < 0) break;
        passive[static_cast<std::size_t>(best)] = true;
        for (int inner = 0; inner < static_cast<int>(3 * n + 10); ++inner) {
            std::vector<double> z;
            solve_passive(z);
            bool all_positive = true;
            for (std::size_t i = 0; i < n; ++i) {
                if (passive[i] && z[i] <= 0.0) all_positive = false;
            }
            if (all_positive) {
                x = z;
                break;
            }
            double alpha = 1.0;
            for (std::size_t i = 0; i < n; ++i) {
                if (passive[i] && z[i] <= 0.0) {
                    const double d = x[i] - z[i];
                    if (d > 0.0) alpha = std::min(alpha, x[i] / d);
                }
            }
            for (std::size_t i = 0; i < n; ++i) {
                x[i] += alpha * (z[i] - x[i]);
                if (passive[i] && x[i] <= 1e-15) {
                    passive[i] = false;
                    x[i] = 0.0;
                }
            }
        }
    }
    for (double& v : x) v = std::max(0.0, v);
    return x;
}

SpectrumFit fit_emission_spectrum(const Spectrum& observed, std::vector<int> candidates, double fwhm, double min_percent) {
    SpectrumFit fit;
    if (observed.size() < 3) {
        fit.message = "the spectrum needs more points";
        return fit;
    }
    if (candidates.empty()) candidates = elements_with_emission_lines(observed.start_nm, observed.end_nm());
    std::vector<std::vector<double>> cols;
    std::vector<int> used;
    for (int z : candidates) {
        const Spectrum t = element_emission_template(z, fwhm, observed.start_nm, observed.end_nm(), observed.step_nm);
        if (!(t.peak() > 0.0)) continue;
        cols.push_back(t.intensity);
        used.push_back(z);
    }
    fit.candidates = used;
    if (cols.empty()) {
        fit.message = "none of the elements has lines in this wavelength range";
        return fit;
    }
    std::vector<double> b = observed.intensity;
    b.resize(cols.front().size(), 0.0);
    const std::vector<double> x = nonnegative_least_squares(cols, b);
    double total = 0.0;
    for (double v : x) total += v;
    if (!(total > 0.0)) {
        fit.message = "no element's lines match this spectrum";
        return fit;
    }
    fit.fitted.start_nm = observed.start_nm;
    fit.fitted.step_nm = observed.step_nm;
    fit.fitted.intensity.assign(b.size(), 0.0);
    for (std::size_t j = 0; j < cols.size(); ++j) {
        for (std::size_t i = 0; i < b.size(); ++i) fit.fitted.intensity[i] += x[j] * cols[j][i];
    }
    double ss_res = 0.0, ss_tot = 0.0, mean = 0.0;
    for (double v : b) mean += v;
    mean /= static_cast<double>(b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
        ss_res += (b[i] - fit.fitted.intensity[i]) * (b[i] - fit.fitted.intensity[i]);
        ss_tot += (b[i] - mean) * (b[i] - mean);
    }
    fit.residual = std::sqrt(ss_res / static_cast<double>(b.size()));
    fit.r_squared = ss_tot > 0.0 ? 1.0 - ss_res / ss_tot : NO_VALUE;
    double kept = 0.0;
    for (std::size_t j = 0; j < x.size(); ++j) {
        const double pct = 100.0 * x[j] / total;
        fit.weights.emplace_back(used[j], x[j]);
        if (pct >= min_percent) kept += x[j];
    }
    for (std::size_t j = 0; j < x.size(); ++j) {
        const double pct = 100.0 * x[j] / total;
        if (pct >= min_percent && kept > 0.0) fit.percentages.emplace_back(used[j], 100.0 * x[j] / kept);
    }
    std::sort(fit.percentages.begin(), fit.percentages.end(), [](const auto& a, const auto& b2) { return a.second > b2.second; });
    fit.ok = true;
    return fit;
}

ColorFit fit_color_mixture(const Srgb8& target, const std::vector<int>& candidates, bool flame) {
    ColorFit fit;
    std::vector<std::vector<double>> cols;
    std::vector<int> used;
    for (int z : candidates) {
        LinearRgb c;
        if (flame && has_flame_color(z)) {
            c = srgb8_to_linear(flame_color(z));
        } else {
            const Spectrum t = element_emission_template(z, 1.0, 380.0, 780.0, 1.0);
            if (!(t.peak() > 0.0)) continue;
            c = spectrum_linear_rgb(t);
            const double low = std::min({ c.r, c.g, c.b });
            if (low < 0.0) {
                c.r -= low;
                c.g -= low;
                c.b -= low;
            }
        }
        const double s = c.r + c.g + c.b;
        if (!(s > 0.0)) continue;
        cols.push_back({ c.r / s, c.g / s, c.b / s });
        used.push_back(z);
    }
    if (cols.empty()) {
        fit.message = "pick elements that give off visible light";
        return fit;
    }
    const LinearRgb t = srgb8_to_linear(target);
    const double ts = t.r + t.g + t.b;
    if (!(ts > 0.0)) {
        fit.message = "black has no color to match";
        return fit;
    }
    const std::vector<double> b = { t.r / ts, t.g / ts, t.b / ts };
    const std::vector<double> x = nonnegative_least_squares(cols, b);
    double total = 0.0;
    for (double v : x) total += v;
    if (!(total > 0.0)) {
        fit.message = "none of these colors mix to that one";
        return fit;
    }
    LinearRgb mix;
    for (std::size_t j = 0; j < x.size(); ++j) {
        mix.r += x[j] * cols[j][0];
        mix.g += x[j] * cols[j][1];
        mix.b += x[j] * cols[j][2];
        if (x[j] > 1e-9) {
            fit.percentages.emplace_back(used[j], 100.0 * x[j] / total);
        }
    }
    std::sort(fit.percentages.begin(), fit.percentages.end(), [](const auto& a, const auto& b2) { return a.second > b2.second; });
    const double scale = ts / std::max(1e-12, mix.r + mix.g + mix.b);
    mix.r *= scale;
    mix.g *= scale;
    mix.b *= scale;
    fit.matched = linear_to_srgb8(mix, false);
    const double dr = fit.matched.r - target.r, dg = fit.matched.g - target.g, db = fit.matched.b - target.b;
    fit.distance = std::sqrt(dr * dr + dg * dg + db * db);
    fit.unique = used.size() <= 3;
    fit.ok = true;
    return fit;
}

} // namespace chemistry
} // namespace fizmo
