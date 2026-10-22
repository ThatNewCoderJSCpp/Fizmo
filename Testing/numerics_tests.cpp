#include "../Source/Numerics/numerics.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace fizmo::numerics;

namespace {

int g_passed = 0;
int g_failed = 0;
const double k_pi = 3.141592653589793;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

PdeField constant_field(double c) {
    return [c](const PdeInputs& in, double* out) {
        for (std::size_t i = 0; i < in.count; ++i) out[i] = c;
    };
}

PdeEquation linear_equation(const std::vector<std::pair<int, double>>& parts, const PdeField& source = nullptr) {
    PdeEquation e;
    e.linear = true;
    e.constant = true;
    for (const auto& p : parts) {
        e.terms.push_back(p.first);
        e.partials.push_back({ p.first, constant_field(p.second) });
    }
    e.residual = [parts, source](const PdeInputs& in, double* out) {
        for (std::size_t i = 0; i < in.count; ++i) {
            double s = 0.0;
            for (const auto& p : parts) s += p.second * in.term(static_cast<std::size_t>(p.first))[i];
            out[i] = s;
        }
        if (source) {
            std::vector<double> extra(in.count);
            source(in, extra.data());
            for (std::size_t i = 0; i < in.count; ++i) out[i] -= extra[i];
        }
    };
    return e;
}

PdeCondition value_condition(int term_u, double value) {
    PdeCondition c;
    c.function = 0;
    c.label = "u = " + num(value);
    c.equation = linear_equation({ { term_u, 1.0 } }, constant_field(value));
    return c;
}

void test_dense() {
    DenseMatrix<double> a(3, 3);
    const double m[3][3] = { { 2, 1, -1 }, { -3, -1, 2 }, { -2, 1, 2 } };
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) a(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = m[i][j];
    }
    std::vector<double> b = { 8, -11, -3 };
    DenseLU<double> lu;
    check(lu.factor(a), "dense LU factors");
    lu.solve(b);
    check(near(b[0], 2, 1e-12) && near(b[1], 3, 1e-12) && near(b[2], -1, 1e-12), "dense LU solves a 3x3 system");
    check(near(lu.determinant(), -1.0, 1e-12), "dense determinant: " + num(lu.determinant()));
    VectorFunction<double> f = [](const std::vector<double>& x, std::vector<double>& out) {
        out = { x[0] * x[0] + x[1] * x[1] - 4.0, x[0] - x[1] };
    };
    std::vector<double> x = { 1.0, 0.5 };
    const NewtonResult<double> r = newton_dense(f, x);
    check(r.converged && near(x[0], std::sqrt(2.0), 1e-10) && near(x[1], std::sqrt(2.0), 1e-10), "dense Newton finds (sqrt 2, sqrt 2)");
}

void test_sparse() {
    const std::size_t n = 200;
    std::vector<Triplet> t;
    for (std::size_t i = 0; i < n; ++i) {
        t.push_back({ i, i, 2.0 });
        if (i > 0) t.push_back({ i, i - 1, -1.0 });
        if (i + 1 < n) t.push_back({ i, i + 1, -1.0 });
    }
    const CsrMatrix a = CsrMatrix::from_triplets(n, t);
    std::vector<double> exact(n), b;
    for (std::size_t i = 0; i < n; ++i) exact[i] = std::sin(0.03 * static_cast<double>(i)) + 0.5;
    a.multiply(exact, b);
    auto error = [&](const std::vector<double>& x) {
        double e = 0.0;
        for (std::size_t i = 0; i < n; ++i) e = std::max(e, std::fabs(x[i] - exact[i]));
        return e;
    };
    BandedLU lu;
    std::vector<double> x = b;
    check(lu.factor(a), "banded LU factors");
    lu.solve(x);
    check(error(x) < 1e-9, "banded LU solves the 1D Laplacian: " + num(error(x)));
    Ilu0 ilu;
    ilu.factor(a);
    const IterativeResult g = gmres(a, &ilu, b, x, 1e-12, 2000);
    check(g.converged && error(x) < 1e-6, "GMRES with ILU(0): " + num(error(x)) + " in " + std::to_string(g.iterations));
    x.assign(n, 0.0);
    const IterativeResult cg = conjugate_gradient(a, b, x, 1e-12, 5000);
    check(cg.converged && error(x) < 1e-6, "conjugate gradient: " + num(error(x)) + " in " + std::to_string(cg.iterations));
    x.assign(n, 0.0);
    const IterativeResult bi = bicgstab(a, &ilu, b, x, 1e-12, 2000);
    check(bi.converged && error(x) < 1e-6, "BiCGSTAB: " + num(error(x)));
    SparseSolver solver;
    solver.method = SparseMethod::Gmres;
    std::vector<double> y;
    check(solver.factor(a) && solver.solve(b, y) && error(y) < 1e-6, "SparseSolver with GMRES");
    SparseNewtonOptions opt;
    const std::size_t m = 99;
    const double h = 1.0 / (m + 1);
    SparseResidual res = [&](const std::vector<double>& u, std::vector<double>& r) {
        r.assign(m, 0.0);
        for (std::size_t i = 0; i < m; ++i) {
            const double l = i > 0 ? u[i - 1] : 0.0;
            const double rr = i + 1 < m ? u[i + 1] : 0.0;
            r[i] = (l - 2.0 * u[i] + rr) / (h * h) + std::exp(u[i]);
        }
    };
    SparseJacobian jac = [&](const std::vector<double>& u, CsrMatrix& J) {
        std::vector<Triplet> e;
        for (std::size_t i = 0; i < m; ++i) {
            e.push_back({ i, i, -2.0 / (h * h) + std::exp(u[i]) });
            if (i > 0) e.push_back({ i, i - 1, 1.0 / (h * h) });
            if (i + 1 < m) e.push_back({ i, i + 1, 1.0 / (h * h) });
        }
        J = CsrMatrix::from_triplets(m, e);
    };
    std::vector<double> u(m, 0.0);
    const SparseNewtonResult nr = newton_sparse(res, jac, u, opt);
    check(nr.converged && near(u[m / 2], 0.140539, 2e-4), "sparse Newton solves Bratu's equation: " + num(u[m / 2]));
}

void test_ode() {
    OdeSystem<double> decay = [](const double&, const std::vector<double>& y, std::vector<double>& d) { d = { -y[0] }; };
    const OdeSolution<double> r4 = solve_fixed<double>(decay, 0.0, { 1.0 }, 2.0, 100, OdeMethod::Rk4);
    check(near(r4.back()[0], std::exp(-2.0), 1e-9), "RK4 decay: " + num(r4.back()[0]));
    for (OdeMethod m : { OdeMethod::BackwardEuler, OdeMethod::Trapezoid, OdeMethod::Bdf2, OdeMethod::ImplicitMidpoint, OdeMethod::Heun, OdeMethod::Midpoint }) {
        const OdeSolution<double> s = solve_fixed<double>(decay, 0.0, { 1.0 }, 2.0, 2000, m);
        check(s.ok && near(s.back()[0], std::exp(-2.0), 2e-4), "fixed-step method " + std::to_string(static_cast<int>(m)) + ": " + num(s.back()[0]));
    }
    OdeSystem<double> spring = [](const double&, const std::vector<double>& y, std::vector<double>& d) { d = { y[1], -y[0] }; };
    OdeOptions<double> opt;
    opt.relative_tolerance = 1e-10;
    opt.absolute_tolerance = 1e-12;
    const OdeSolution<double> a = solve_adaptive<double>(spring, 0.0, { 1.0, 0.0 }, 10.0, opt);
    check(a.ok && near(a.back()[0], std::cos(10.0), 1e-8), "Dormand-Prince oscillator: " + num(a.back()[0]) + " in " + std::to_string(a.steps) + " steps");
    check(near(a.at(2.5)[0], std::cos(2.5), 1e-5), "dense output between steps: " + num(a.at(2.5)[0]));
    const OdeSolution<double> back = solve_adaptive<double>(spring, 0.0, { 1.0, 0.0 }, -3.0, opt);
    check(back.ok && near(back.back()[0], std::cos(-3.0), 1e-8), "integrating backward in time");
    OdeSystem<double> fall = [](const double&, const std::vector<double>& y, std::vector<double>& d) { d = { y[1], -9.81 }; };
    OdeOptions<double> ev;
    ev.events.push_back({ [](const double&, const std::vector<double>& y) { return y[0]; }, -1, true });
    const OdeSolution<double> drop = solve_adaptive<double>(fall, 0.0, { 10.0, 0.0 }, 100.0, ev);
    check(drop.stopped_by_event && near(drop.t.back(), std::sqrt(2.0 * 10.0 / 9.81), 1e-7), "event stops a falling ball at the floor: " + num(drop.t.back()));
    OdeSystem<double> stiff = [](const double& t, const std::vector<double>& y, std::vector<double>& d) { d = { -1000.0 * (y[0] - std::cos(t)) }; };
    OdeOptions<double> so;
    so.relative_tolerance = 1e-6;
    so.absolute_tolerance = 1e-9;
    const OdeSolution<double> st = solve_stiff<double>(stiff, 0.0, { 0.0 }, 2.0, so);
    const double expect = (1e6 * std::cos(2.0) + 1e3 * std::sin(2.0)) / (1e6 + 1.0);
    check(st.ok && near(st.back()[0], expect, 1e-4), "stiff solver: " + num(st.back()[0]) + " in " + std::to_string(st.steps) + " steps");
    AccelerationFunction<double> kepler = [](const double&, const std::vector<double>& q, const std::vector<double>&, std::vector<double>& acc) {
        const double r = std::sqrt(q[0] * q[0] + q[1] * q[1]);
        acc = { -q[0] / (r * r * r), -q[1] / (r * r * r) };
    };
    auto energy = [](const std::vector<double>& y) {
        return 0.5 * (y[2] * y[2] + y[3] * y[3]) - 1.0 / std::sqrt(y[0] * y[0] + y[1] * y[1]);
    };
    for (SymplecticMethod m : { SymplecticMethod::Verlet, SymplecticMethod::Yoshida4 }) {
        const OdeSolution<double> orbit = solve_symplectic<double>(kepler, 0.0, { 1.0, 0.0 }, { 0.0, 1.2 }, 100.0, 20000, m);
        double drift = 0.0;
        const double e0 = energy(orbit.y.front());
        for (const auto& y : orbit.y) drift = std::max(drift, std::fabs(energy(y) - e0));
        check(orbit.ok && drift < (m == SymplecticMethod::Verlet ? 1e-3 : 1e-6), "symplectic orbit keeps its energy: " + num(drift));
    }
    ImplicitOdeFunction<double> F = [](const double&, const std::vector<double>& d) { return d[2] * d[2] * d[2] + d[2] + d[0]; };
    const OdeSystem<double> sys = implicit_order_system<double>(F, 2);
    const OdeSolution<double> imp = solve_adaptive<double>(sys, 0.0, { 0.0, 0.0 }, 1.0, opt);
    check(imp.ok && near(imp.back()[0], 0.0, 1e-9), "implicit highest derivative with y = 0 stays 0");
    ImplicitOdeFunction<double> G = [](const double&, const std::vector<double>& d) { return d[2] + d[0]; };
    const OdeSolution<double> imp2 = solve_adaptive<double>(implicit_order_system<double>(G, 2), 0.0, { 0.0, 1.0 }, 3.0, opt);
    check(imp2.ok && near(imp2.back()[0], std::sin(3.0), 1e-7), "y'' + y = 0 written implicitly: " + num(imp2.back()[0]));
    const OdeSolution<float> fl = solve_fixed<float>([](const float&, const std::vector<float>& y, std::vector<float>& d) { d = { -y[0] }; }, 0.0f, { 1.0f }, 1.0f, 50);
    check(std::fabs(fl.back()[0] - std::exp(-1.0f)) < 1e-5f, "templates work with float");
}

void test_pde() {
    {
        PdeProblem p;
        p.terms = { PdeTerm(0, 0, 0, 0, 1), PdeTerm(0, 2, 0, 0, 0), PdeTerm(0, 0, 0, 0, 0) };
        p.equations = { linear_equation({ { 0, 1.0 }, { 1, -0.5 } }) };
        p.lo[0] = 0.0;
        p.hi[0] = 1.0;
        p.points[0] = 161;
        p.edges[0][0].conditions = { value_condition(2, 0.0) };
        p.edges[0][1].conditions = { value_condition(2, 0.0) };
        p.initial = { { [](const PdeInputs& in, double* out) {
            for (std::size_t i = 0; i < in.count; ++i) out[i] = std::sin(k_pi * in.x()[i]);
        } } };
        p.t0 = 0.0;
        p.t1 = 1.0;
        p.steps = 400;
        p.frames = 101;
        p.method = PdeMethod::Bdf2;
        PdeSolution s;
        std::string err;
        const bool ok = solve_pde(p, s, err);
        const std::vector<double> u = s.at_time(0.5, 0);
        check(ok && s.linear && near(u[80], std::exp(-0.25 * k_pi * k_pi), 2e-3), "PDE heat equation: " + err + " " + num(ok ? u[80] : 0.0));
    }
    {
        PdeProblem p;
        p.terms = { PdeTerm(0, 0, 0, 0, 2), PdeTerm(0, 2, 0, 0, 0), PdeTerm(0, 0, 0, 0, 0) };
        p.equations = { linear_equation({ { 0, 1.0 }, { 1, -1.0 } }) };
        p.lo[0] = 0.0;
        p.hi[0] = 1.0;
        p.points[0] = 161;
        p.edges[0][0].conditions = { value_condition(2, 0.0) };
        p.edges[0][1].conditions = { value_condition(2, 0.0) };
        p.initial = { { [](const PdeInputs& in, double* out) {
            for (std::size_t i = 0; i < in.count; ++i) out[i] = std::sin(k_pi * in.x()[i]);
        }, constant_field(0.0) } };
        p.t1 = 1.0;
        p.steps = 400;
        p.frames = 101;
        PdeSolution s;
        std::string err;
        const bool ok = solve_pde(p, s, err);
        const std::vector<double> u = s.at_time(0.25, 0);
        check(ok && near(u[80], std::cos(0.25 * k_pi), 2e-3), "PDE wave equation: " + err + " " + num(ok ? u[80] : 0.0));
    }
    {
        PdeProblem p;
        p.steady = true;
        p.dims = 2;
        p.terms = { PdeTerm(0, 2, 0, 0, 0), PdeTerm(0, 0, 2, 0, 0), PdeTerm(0, 0, 0, 0, 0) };
        p.equations = { linear_equation({ { 0, 1.0 }, { 1, 1.0 } }) };
        p.points[0] = p.points[1] = 51;
        for (int a = 0; a < 2; ++a) {
            for (int s = 0; s < 2; ++s) p.edges[a][s].conditions = { value_condition(2, 0.0) };
        }
        PdeCondition top;
        top.function = 0;
        top.label = "u = sin(pi x)";
        top.equation = linear_equation({ { 2, 1.0 } }, [](const PdeInputs& in, double* out) {
            for (std::size_t i = 0; i < in.count; ++i) out[i] = std::sin(k_pi * in.x()[i]);
        });
        p.edges[1][1].conditions = { top };
        PdeSolution s;
        std::string err;
        const bool ok = solve_pde(p, s, err);
        const double centre = ok ? s.value(0, 0, 25, 25) : 0.0;
        check(ok && near(centre, std::sinh(k_pi / 2) / std::sinh(k_pi), 1e-3), "PDE Laplace on a square: " + err + " " + num(centre));
    }
    {
        PdeProblem p;
        p.terms = { PdeTerm(0, 0, 0, 0, 1), PdeTerm(0, 1, 0, 0, 0) };
        p.equations = { linear_equation({ { 0, 1.0 }, { 1, 1.0 } }) };
        p.lo[0] = -k_pi;
        p.hi[0] = k_pi;
        p.points[0] = 160;
        p.edges[0][0].mode = PdeEdgeMode::Periodic;
        p.initial = { { [](const PdeInputs& in, double* out) {
            for (std::size_t i = 0; i < in.count; ++i) out[i] = std::sin(in.x()[i]);
        } } };
        p.t1 = 2.0;
        p.steps = 400;
        p.frames = 41;
        PdeSolution s;
        std::string err;
        const bool ok = solve_pde(p, s, err);
        const std::vector<double> u = s.at_time(1.0, 0);
        check(ok && s.periodic[0] && near(u[80], std::sin(s.coord(0, 80) - 1.0), 2e-3), "PDE periodic transport: " + err + " " + num(ok ? u[80] : 0.0));
    }
    {
        PdeProblem p;
        p.steady = true;
        p.terms = { PdeTerm(0, 2, 0, 0, 0), PdeTerm(0, 0, 0, 0, 0) };
        PdeEquation e;
        e.terms = { 0, 1 };
        e.residual = [](const PdeInputs& in, double* out) {
            for (std::size_t i = 0; i < in.count; ++i) out[i] = in.term(0)[i] + std::exp(in.term(1)[i]);
        };
        p.equations = { e };
        p.points[0] = 161;
        p.edges[0][0].conditions = { value_condition(1, 0.0) };
        p.edges[0][1].conditions = { value_condition(1, 0.0) };
        PdeSolution s;
        std::string err;
        const bool ok = solve_pde(p, s, err);
        check(ok && !s.linear && near(s.value(0, 0, 80), 0.140539, 2e-4), "PDE nonlinear steady state with numeric partials: " + err + " " + num(ok ? s.value(0, 0, 80) : 0.0));
    }
    {
        PdeProblem p;
        p.terms = { PdeTerm(0, 0, 0, 0, 1) };
        p.equations = { linear_equation({ { 0, 1.0 } }) };
        p.poll = []() { return PdePoll::Abort; };
        PdeSolution s;
        std::string err;
        check(!solve_pde(p, s, err), "PDE solving can be aborted");
    }
}

void test_geometry() {
    const ScalarGrid2 g = sample_grid([](double x, double y) { return x * x + y * y; }, -2, 2, -2, 2, 201, 201);
    const std::vector<Point2> seg = marching_squares(g, 1.0);
    double len = 0.0;
    for (std::size_t i = 0; i + 1 < seg.size(); i += 2) len += std::hypot(seg[i + 1].x - seg[i].x, seg[i + 1].y - seg[i].y);
    check(near(len, 2 * k_pi, 1e-3), "marching squares traces a circle: " + num(len));
    const auto lines = join_segments(seg, 1e-9);
    check(lines.size() == 1, "joined into one loop: " + std::to_string(lines.size()));
    const double lo[3] = { -1.5, -1.5, -1.5 };
    const double hi[3] = { 1.5, 1.5, 1.5 };
    const ScalarGrid3 g3 = sample_grid([](double x, double y, double z) { return x * x + y * y + z * z; }, lo, hi, 61, 61, 61);
    const TriangleMesh sphere = mesh_from_soup(marching_tetrahedra(g3, 1.0));
    double area = 0.0;
    for (std::size_t t = 0; t + 2 < sphere.indices.size(); t += 3) {
        const Point3 a = sphere.positions[sphere.indices[t]];
        const Point3 b = sphere.positions[sphere.indices[t + 1]];
        const Point3 c = sphere.positions[sphere.indices[t + 2]];
        area += 0.5 * length(cross(b - a, c - a));
    }
    check(near(area, 4 * k_pi, 0.05), "marching tetrahedra makes a sphere: " + num(area));
    TriangleMesh m = surface_mesh([](double u, double v) { return Point3{ u, v, u * v }; }, 0, 1, 0, 1, 11, 11);
    m.color_by_height("viridis");
    check(m.triangles() == 200 && m.normals.size() == 121 && m.colors.size() == 121, "surface mesh has 200 triangles");
    const Rgba8 c0 = colormap("viridis", 0.0);
    const Rgba8 c1 = colormap("viridis", 1.0);
    check(c0.r == 68 && c1.g == 231, "viridis ends");
    const std::vector<Point2> curve = sample_curve([](double t) { return Point2{ std::cos(t), std::sin(t) }; }, 0, 2 * k_pi, 1e-4, 20, 10);
    double cl = 0.0;
    for (std::size_t i = 0; i + 1 < curve.size(); ++i) cl += std::hypot(curve[i + 1].x - curve[i].x, curve[i + 1].y - curve[i].y);
    check(near(cl, 2 * k_pi, 1e-3), "adaptive curve sampling: " + num(cl) + " with " + std::to_string(curve.size()) + " points");
}

} // namespace

int main() {
    test_dense();
    test_sparse();
    test_ode();
    test_pde();
    test_geometry();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
