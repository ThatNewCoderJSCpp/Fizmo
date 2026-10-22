#ifndef FIZMO_NUMERICS_PDE_HPP
#define FIZMO_NUMERICS_PDE_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace fizmo {
namespace numerics {

struct PdeTerm {
    int function = 0;
    int order[3] = { 0, 0, 0 };
    int time_order = 0;

    PdeTerm() = default;
    PdeTerm(int fun, int ox, int oy, int oz, int ot) : function(fun), order{ ox, oy, oz }, time_order(ot) {}
};

struct PdeInputs {
    std::size_t count = 0;
    std::vector<const double*> values;

    const double* x() const { return values[0]; }
    const double* y() const { return values[1]; }
    const double* z() const { return values[2]; }
    const double* t() const { return values[3]; }
    const double* term(std::size_t k) const { return values[4 + k]; }
};

using PdeField = std::function<void(const PdeInputs& in, double* out)>;

struct PdePartial {
    int term = 0;
    PdeField derivative;
};

struct PdeEquation {
    PdeField residual;
    std::vector<int> terms;
    std::vector<PdePartial> partials;
    bool linear = false;
    bool constant = false;
};

enum class PdeEdgeMode { Conditions, Periodic, Insulated, Free };

struct PdeCondition {
    PdeEquation equation;
    int function = 0;
    std::string label;
};

struct PdeEdge {
    PdeEdgeMode mode = PdeEdgeMode::Conditions;
    std::vector<PdeCondition> conditions;
};

enum class PdeMethod { Trapezoid, Bdf2, BackwardEuler };

enum class PdePoll { Continue, Abort, Finish };

struct PdeProblem {
    int dims = 1;
    int functions = 1;
    bool steady = false;
    std::vector<PdeTerm> terms;
    std::vector<PdeEquation> equations;
    double lo[3] = { 0.0, 0.0, 0.0 };
    double hi[3] = { 1.0, 1.0, 1.0 };
    int points[3] = { 101, 1, 1 };
    PdeEdge edges[3][2];
    std::vector<std::vector<PdeField>> initial;
    double t0 = 0.0;
    double t1 = 1.0;
    int steps = 100;
    int frames = 2;
    PdeMethod method = PdeMethod::Trapezoid;
    double band_budget_constant = 6e8;
    double band_budget_changing = 3e7;
    std::function<PdePoll()> poll;
};

struct PdeSolution {
    int dims = 1;
    int functions = 1;
    int n[3] = { 1, 1, 1 };
    double lo[3] = { 0.0, 0.0, 0.0 };
    double hi[3] = { 1.0, 1.0, 1.0 };
    bool periodic[3] = { false, false, false };
    std::vector<int> time_orders;
    std::vector<double> times;
    std::vector<std::vector<double>> frames;
    std::string problem;
    double stopped = -1.0;
    bool blew_up = false;
    bool finished_early = false;
    int steps = 0;
    int newton = 0;
    int newton_max = 0;
    long long linear_iterations = 0;
    bool linear = false;
    std::string solver;
    double residual = 0.0;
    double dt = 0.0;

    std::size_t points() const { return static_cast<std::size_t>(n[0]) * n[1] * n[2]; }
    double spacing(int axis) const;
    double coord(int axis, int i) const;
    double value(std::size_t frame, int function, int i, int j = 0, int k = 0) const;
    std::vector<double> at_time(double t, int function) const;
};

bool solve_pde(const PdeProblem& problem, PdeSolution& out, std::string& error);

int pde_time_order(const PdeProblem& problem, int function);

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_PDE_HPP
