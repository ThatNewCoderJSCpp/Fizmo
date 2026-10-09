#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

Expression characteristic_polynomial(
    const matrices::SymbolicMatrix2x2& A,
    const std::string& lambda_var 
) {
    Expression lam = VARIABLE(lambda_var);
    matrices::SymbolicMatrix2x2 shifted(
        A.at(0,0) - lam, A.at(0,1),
        A.at(1,0),       A.at(1,1) - lam
    );
    return SIMPLIFY(shifted.determinant());
}

Expression characteristic_polynomial(
    const matrices::SymbolicMatrix3x3& A,
    const std::string& lambda_var 
) {
    Expression lam = VARIABLE(lambda_var);
    matrices::SymbolicMatrix3x3 shifted;
    auto& d = shifted.get_data();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            d[r][c] = (r == c) ? A.at(r,c) - lam : A.at(r,c);
    return SIMPLIFY(shifted.determinant());
}

Expression characteristic_polynomial(
    const matrices::SymbolicMatrixN& A,
    const std::string& lambda_var 
) {
    std::size_t n = A.dimension();
    Expression lam = VARIABLE(lambda_var);
    matrices::SymbolicMatrixN shifted(n);
    for (std::size_t r = 0; r < n; ++r)
        for (std::size_t c = 0; c < n; ++c)
            shifted.at(r,c) = (r == c) ? A.at(r,c) - lam : A.at(r,c);
    return SIMPLIFY(shifted.determinant());
}

Expression characteristic_polynomial(
    const matrices::SymbolicMatrix4x4& A,
    const std::string& lambda_var 
) {
    Expression lam = VARIABLE(lambda_var);
    matrices::SymbolicMatrix4x4 shifted;
    auto& d = shifted.get_data();

    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            d[r][c] = (r == c) ? A.at(r,c) - lam : A.at(r,c);

    return SIMPLIFY(shifted.determinant());
}

double gershgorin_bound(const std::vector<std::vector<double>>& M) {
    std::size_t n = M.size();
    double bound = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        double center = std::abs(M[i][i]);
        double radius = 0.0;
        for (std::size_t j = 0; j < n; ++j) if (j != i) radius += std::abs(M[i][j]);
        bound = std::max(bound, center + radius);
    }
    return bound + 1.0;  
}

std::vector<std::vector<double>> eval_matrix(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals
) {
    std::size_t n = A.dimension();
    std::vector<std::vector<double>> M(n, std::vector<double>(n));
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            M[i][j] = A.at(i,j).evaluate(vals);
    return M;
}

std::vector<std::vector<double>> eval_matrix_4x4(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals
) {
    std::vector<std::vector<double>> M(4, std::vector<double>(4));
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            M[i][j] = A.at(i,j).evaluate(vals);
    return M;
}

std::vector<std::vector<double>> eval_matrix_3x3(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals
) {
    std::vector<std::vector<double>> M(3, std::vector<double>(3));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            M[i][j] = A.at(i,j).evaluate(vals);
    return M;
}

std::vector<std::vector<double>> eval_matrix_2x2(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals
) {
    std::vector<std::vector<double>> M(2, std::vector<double>(2));
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            M[i][j] = A.at(i,j).evaluate(vals);
    return M;
}

std::vector<std::vector<double>> null_space(
    std::vector<std::vector<double>> B,
    double tol 
) {
    std::size_t n = B.size();
    if (n == 0) return {};
    std::vector<std::size_t> col_perm(n);
    std::iota(col_perm.begin(), col_perm.end(), 0);
    std::size_t rank = 0;
    for (std::size_t col = 0; col < n && rank < n; ++col) {
        double best = 0.0;
        std::size_t best_r = rank, best_c = col;
        for (std::size_t r = rank; r < n; ++r) {
            for (std::size_t c = col; c < n; ++c) {
                double v = std::abs(B[r][c]);
                if (v > best) { best = v; best_r = r; best_c = c; }
            }
        }
        if (best <= tol) break;  
        if (best_r != rank) std::swap(B[rank], B[best_r]);
        if (best_c != col) {
            for (std::size_t r = 0; r < n; ++r) std::swap(B[r][col], B[r][best_c]);
            std::swap(col_perm[col], col_perm[best_c]);
        }
        double pivot = B[rank][col];
        for (std::size_t c = col; c < n; ++c) B[rank][c] /= pivot;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == rank) continue;
            double factor = B[r][col];
            if (std::abs(factor) < tol) continue;
            for (std::size_t c = col; c < n; ++c) B[r][c] -= factor * B[rank][c];
        }
        ++rank;
    }
    std::size_t nullity = n - rank;
    if (nullity == 0) {  return {}; }
    std::vector<std::vector<double>> basis;
    basis.reserve(nullity);
    for (std::size_t f = 0; f < nullity; ++f) {
        std::vector<double> v(n, 0.0);
        std::size_t free_col = rank + f;
        v[free_col] = 1.0;
        for (std::size_t r = rank; r-- > 0; ) {
            double s = 0.0;
            for (std::size_t c = r + 1; c < n; ++c) s += B[r][c] * v[c];
            v[r] = -s; 
        }
        std::vector<double> w(n);
        for (std::size_t i = 0; i < n; ++i) w[col_perm[i]] = v[i];
        double len = 0.0;
        for (double x : w) len += x * x;
        len = std::sqrt(len);
        if (len > tol) for (double& x : w) x /= len;
        basis.push_back(std::move(w));
    }

    return basis;
}

std::vector<double> find_eigenvalues(
    const std::vector<std::vector<double>>& M,
    const solvers::SolverOptions& opts 
) {
    std::size_t n = M.size();
    matrices::SymbolicMatrixN sym(n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
            sym.at(i,j) = Const(M[i][j]);

    std::string lvar = "__eigen_lambda__";
    Expression cp = characteristic_polynomial(sym, lvar);
    double R = gershgorin_bound(M);
    solvers::SolverOptions eopts = opts;
    if (eopts.coarse_scan_points < 2048) eopts.coarse_scan_points = 2048;
    auto roots = solvers::EquationSolver::find_roots(cp, lvar, -R, R, eopts);
    std::vector<double> eigenvalues;
    eigenvalues.reserve(roots.size());
    for (auto& r : roots) if (r.converged) eigenvalues.push_back(r.value);
    std::sort(eigenvalues.begin(), eigenvalues.end(), std::greater<double>());
    return eigenvalues;
}

EigenDecomposition decompose(
    const std::vector<std::vector<double>>& M,
    const solvers::SolverOptions& opts 
) {
    EigenDecomposition result;
    result.eigenvalues = find_eigenvalues(M, opts);
    result.eigenvectors.reserve(result.eigenvalues.size());
    for (double lam : result.eigenvalues) {
        auto B = shifted_matrix(M, lam);
        auto ns = null_space(B);
        if (!ns.empty()) {
            result.eigenvectors.push_back(std::move(ns[0]));
        } else {
            result.eigenvectors.push_back(std::vector<double>(M.size(), 0.0));
        }
    }
    return result;
}

std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenvalues(A, m, opts);
}

std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenvalues(A, m, opts);
}

std::vector<double> eigenvalues(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenvalues(A, m, opts);
}

std::vector<double> eigenvalues(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenvalues(A, m, opts);
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix2x2& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts 
) {
    auto M = eval_matrix_2x2(A, vals);
    auto dec = decompose(M, opts);
    std::vector<Eigenpair> out;
    out.reserve(dec.eigenvalues.size());
    for (std::size_t i = 0; i < dec.eigenvalues.size(); ++i) out.push_back({ dec.eigenvalues[i], std::move(dec.eigenvectors[i]) });
    return out;
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenpairs(A, m, opts);
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix3x3& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts 
) {
    auto M = eval_matrix_3x3(A, vals);
    auto dec = decompose(M, opts);
    std::vector<Eigenpair> out;
    out.reserve(dec.eigenvalues.size());
    for (std::size_t i = 0; i < dec.eigenvalues.size(); ++i) out.push_back({ dec.eigenvalues[i], std::move(dec.eigenvectors[i]) });
    return out;
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenpairs(A, m, opts);
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix4x4& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts 
) {
    auto M = eval_matrix_4x4(A, vals);
    auto dec = decompose(M, opts);
    std::vector<Eigenpair> out;
    out.reserve(dec.eigenvalues.size());
    for (std::size_t i = 0; i < dec.eigenvalues.size(); ++i)
        out.push_back({ dec.eigenvalues[i], std::move(dec.eigenvectors[i]) });
    return out;
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenpairs(A, m, opts);
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrixN& A,
    const std::unordered_map<std::string, double>& vals,
    const solvers::SolverOptions& opts 
) {
    auto M = eval_matrix(A, vals);
    auto dec = decompose(M, opts);
    std::vector<Eigenpair> out;
    out.reserve(dec.eigenvalues.size());
    for (std::size_t i = 0; i < dec.eigenvalues.size(); ++i) out.push_back({ dec.eigenvalues[i], std::move(dec.eigenvectors[i]) });
    return out;
}

std::vector<Eigenpair> eigenpairs(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigenpairs(A, m, opts);
}

EigenDecomposition eigen(
    const matrices::SymbolicMatrix2x2& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) { 
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigen(A, m, opts); 
}

EigenDecomposition eigen(
    const matrices::SymbolicMatrix3x3& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second); 
    return eigen(A, m, opts); 
}

EigenDecomposition eigen(
    const matrices::SymbolicMatrix4x4& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second);
    return eigen(A, m, opts);
}

EigenDecomposition eigen(
    const matrices::SymbolicMatrixN& A,
    std::initializer_list<std::pair<std::string, double>> vals,
    const solvers::SolverOptions& opts 
) {
    std::unordered_map<std::string, double> m;
    for (auto& p : vals) m.emplace(p.first, p.second); 
    return eigen(A, m, opts); 
}

} // namespace cas
} // namespace math
} // namespace fizmo
