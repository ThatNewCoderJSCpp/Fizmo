#ifndef FIZMO_EIGEN_HPP
#define FIZMO_EIGEN_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../Common/main_convenience.hpp"
#include "vectors.hpp"
#include "matrices.hpp"
#include "../Calculus/equality_solver.hpp"
#include "../../Basic/constants.hpp"

#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <unordered_map>
#include <initializer_list>
#include <utility>
#include <stdexcept>

namespace fizmo {
namespace math {
namespace cas {

struct Eigenpair {
    double              eigenvalue;
    std::vector<double> eigenvector;   // unit-length
};

struct EigenDecomposition {
    std::vector<double>              eigenvalues;   // descending
    std::vector<std::vector<double>> eigenvectors;  // eigenvectors[i] corresponds to eigenvalues[i]
};

 Expression characteristic_polynomial(
    const matrices::SymbolicMatrix2x2& A,
    const std::string& lambda_var = "\u03BB"
);

 Expression characteristic_polynomial(
    const matrices::SymbolicMatrix3x3& A,
    const std::string& lambda_var = "\u03BB"
);

 Expression characteristic_polynomial(
    const matrices::SymbolicMatrixN& A,
    const std::string& lambda_var = "\u03BB"
);

 Expression characteristic_polynomial(
    const matrices::SymbolicMatrix4x4& A,
    const std::string& lambda_var = "\u03BB"
);

 double gershgorin_bound(const std::vector<std::vector<double>>& M);

 std::vector<std::vector<double>> eval_matrix(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals
);

 std::vector<std::vector<double>> eval_matrix_4x4(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals
);

 std::vector<std::vector<double>> eval_matrix_3x3(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals
);

 std::vector<std::vector<double>> eval_matrix_2x2(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals
);

inline std::vector<std::vector<double>> shifted_matrix(
    const std::vector<std::vector<double>>& M, double lambda
) {
    std::size_t n = M.size();
    auto B = M;
    for (std::size_t i = 0; i < n; ++i) B[i][i] -= lambda;
    return B;
}

 std::vector<std::vector<double>> null_space(
    std::vector<std::vector<double>> B,
    double tol = constants::middle_epsilon()
);

 std::vector<double> find_eigenvalues(
    const std::vector<std::vector<double>>& M,
    const solvers::SolverOptions& opts = {}
);

 EigenDecomposition decompose(
    const std::vector<std::vector<double>>& M,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) {
    return find_eigenvalues(eval_matrix_2x2(A, vals), opts);
}

 std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) {
    return find_eigenvalues(eval_matrix_3x3(A, vals), opts);
}

 std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) {
    return find_eigenvalues(eval_matrix_4x4(A, vals), opts);
}

 std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix4x4& A,
    const solvers::SolverOptions& opts = {}
) { return eigenvalues(A, std::unordered_map<std::string, double>{}, opts); }

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) {
    return find_eigenvalues(eval_matrix(A, vals), opts);
}

 std::vector<double> eigenvalues(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix2x2& A,
    const solvers::SolverOptions& opts = {}
) { return eigenvalues(A, std::unordered_map<std::string, double>{}, opts); }

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix3x3& A,
    const solvers::SolverOptions& opts = {}
) { return eigenvalues(A, std::unordered_map<std::string, double>{}, opts); }

inline std::vector<double> eigenvalues(
    const matrices::SymbolicMatrixN& A,
    const solvers::SolverOptions& opts = {}
) { return eigenvalues(A, std::unordered_map<std::string, double>{}, opts); }

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix4x4& A, const solvers::SolverOptions& opts = {}
) { return eigenpairs(A, std::unordered_map<std::string, double>{}, opts); }

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
);

 std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix2x2& A, const solvers::SolverOptions& opts = {}
) { return eigenpairs(A, std::unordered_map<std::string, double>{}, opts); }

inline std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix3x3& A, const solvers::SolverOptions& opts = {}
) { return eigenpairs(A, std::unordered_map<std::string, double>{}, opts); }

inline std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrixN& A, const solvers::SolverOptions& opts = {}
) { return eigenpairs(A, std::unordered_map<std::string, double>{}, opts); }

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) { return decompose(eval_matrix_2x2(A, vals), opts); }

 EigenDecomposition eigen(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix2x2& A, const solvers::SolverOptions& opts = {}
) { return eigen(A, std::unordered_map<std::string, double>{}, opts); }

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) { return decompose(eval_matrix_3x3(A, vals), opts); }

 EigenDecomposition eigen(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix3x3& A, const solvers::SolverOptions& opts = {}
) { return eigen(A, std::unordered_map<std::string, double>{}, opts); }

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) { return decompose(eval_matrix_4x4(A, vals), opts); }

 EigenDecomposition eigen(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrix4x4& A, const solvers::SolverOptions& opts = {}
) { return eigen(A, std::unordered_map<std::string, double>{}, opts); }

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts = {}
) { return decompose(eval_matrix(A, vals), opts); }

 EigenDecomposition eigen(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts = {}
);

inline EigenDecomposition eigen(
    const matrices::SymbolicMatrixN& A, const solvers::SolverOptions& opts = {}
) { return eigen(A, std::unordered_map<std::string, double>{}, opts); }

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_EIGEN_HPP