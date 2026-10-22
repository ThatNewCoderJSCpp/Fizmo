#ifndef FIZMO_NUMERICS_GEOMETRY_HPP
#define FIZMO_NUMERICS_GEOMETRY_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fizmo {
namespace numerics {

struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

struct Point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Rgba8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

Point2 operator+(Point2 a, Point2 b);
Point2 operator-(Point2 a, Point2 b);
Point2 operator*(Point2 a, double s);
Point3 operator+(Point3 a, Point3 b);
Point3 operator-(Point3 a, Point3 b);
Point3 operator*(Point3 a, double s);
double dot(Point3 a, Point3 b);
Point3 cross(Point3 a, Point3 b);
double length(Point3 a);
Point3 normalized(Point3 a);
bool finite(Point2 a);
bool finite(Point3 a);

const std::vector<std::string>& colormap_names();
Rgba8 colormap(const std::string& name, double t);
Rgba8 colormap_range(const std::string& name, double value, double lo, double hi);

struct ScalarGrid2 {
    int nx = 0;
    int ny = 0;
    double x0 = 0.0;
    double x1 = 1.0;
    double y0 = 0.0;
    double y1 = 1.0;
    std::vector<double> v;

    double x(int i) const { return x0 + (x1 - x0) * i / (nx - 1); }
    double y(int j) const { return y0 + (y1 - y0) * j / (ny - 1); }
    double at(int i, int j) const { return v[static_cast<std::size_t>(j) * nx + i]; }
};

struct ScalarGrid3 {
    int n[3] = { 0, 0, 0 };
    double lo[3] = { 0.0, 0.0, 0.0 };
    double hi[3] = { 1.0, 1.0, 1.0 };
    std::vector<double> v;

    Point3 position(int i, int j, int k) const;
    double at(int i, int j, int k) const { return v[(static_cast<std::size_t>(k) * n[1] + j) * n[0] + i]; }
};

ScalarGrid2 sample_grid(const std::function<double(double, double)>& f, double x0, double x1, double y0, double y1, int nx, int ny);
ScalarGrid3 sample_grid(const std::function<double(double, double, double)>& f, const double lo[3], const double hi[3], int nx, int ny, int nz);

std::vector<Point2> marching_squares(const ScalarGrid2& grid, double level, const std::function<double(double, double)>& refine = nullptr);
std::vector<std::vector<Point2>> join_segments(const std::vector<Point2>& segments, double tolerance = 1e-9);
std::vector<Point3> marching_tetrahedra(const ScalarGrid3& grid, double level = 0.0);

struct TriangleMesh {
    std::vector<Point3> positions;
    std::vector<Point3> normals;
    std::vector<Rgba8> colors;
    std::vector<std::uint32_t> indices;

    void compute_normals();
    void color_by(const std::vector<double>& values, const std::string& map, double lo, double hi);
    void color_by_height(const std::string& map);
    std::size_t triangles() const { return indices.size() / 3; }
};

TriangleMesh mesh_from_soup(const std::vector<Point3>& triangles);
TriangleMesh surface_mesh(const std::function<Point3(double u, double v)>& f, double u0, double u1, double v0, double v1, int nu, int nv);
TriangleMesh height_mesh(const ScalarGrid2& grid);
std::vector<std::vector<Point3>> mesh_grid_lines(const std::function<Point3(double u, double v)>& f, double u0, double u1, double v0, double v1, int lines_u, int lines_v, int samples);

std::vector<double> sample_parameters(const std::function<Point2(double)>& f, double t0, double t1, double tolerance = 1e-3, int initial = 200, int max_depth = 8);
std::vector<Point2> sample_curve(const std::function<Point2(double)>& f, double t0, double t1, double tolerance = 1e-3, int initial = 200, int max_depth = 8);
std::vector<Point3> sample_curve(const std::function<Point3(double)>& f, double t0, double t1, double tolerance = 1e-3, int initial = 200, int max_depth = 8);

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_GEOMETRY_HPP
