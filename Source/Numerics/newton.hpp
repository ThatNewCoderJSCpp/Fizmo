#ifndef FIZMO_NUMERICS_NEWTON_HPP
#define FIZMO_NUMERICS_NEWTON_HPP

#include "sparse.hpp"

#include <functional>
#include <string>
#include <vector>

namespace fizmo {
namespace numerics {

using SparseResidual = std::function<void(const std::vector<double>& x, std::vector<double>& residual)>;
using SparseJacobian = std::function<void(const std::vector<double>& x, CsrMatrix& jacobian)>;

struct SparseNewtonOptions {
    double tolerance = 1e-10;
    double step_tolerance = 1e-12;
    int max_iterations = 50;
    bool line_search = true;
    bool reuse_jacobian = false;
    SparseMethod method = SparseMethod::Automatic;
    std::function<bool()> cancelled;
};

struct SparseNewtonResult {
    bool converged = false;
    int iterations = 0;
    double residual = 0.0;
    long long linear_iterations = 0;
    std::string solver;
    std::string message;
};

SparseNewtonResult newton_sparse(const SparseResidual& residual, const SparseJacobian& jacobian, std::vector<double>& x, const SparseNewtonOptions& options = SparseNewtonOptions());

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_NEWTON_HPP
