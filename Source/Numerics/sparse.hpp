#ifndef FIZMO_NUMERICS_SPARSE_HPP
#define FIZMO_NUMERICS_SPARSE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace fizmo {
namespace numerics {

struct Triplet {
    std::size_t row = 0;
    std::size_t col = 0;
    double value = 0.0;
};

class CsrMatrix {
public:
    std::size_t n = 0;
    std::vector<std::size_t> ptr;
    std::vector<int> col;
    std::vector<double> val;

    CsrMatrix() = default;

    static CsrMatrix from_triplets(std::size_t size, const std::vector<Triplet>& entries, bool keep_diagonal = true);
    static CsrMatrix from_triplets(std::size_t size, const std::vector<std::size_t>& rows, const std::vector<int>& cols, const std::vector<double>& values, bool keep_diagonal = true);
    static CsrMatrix identity(std::size_t size);

    std::size_t size() const { return n; }
    std::size_t nonzeros() const { return val.size(); }
    double at(std::size_t row, std::size_t column) const;
    void multiply(const std::vector<double>& x, std::vector<double>& y) const;
    std::vector<double> operator*(const std::vector<double>& x) const;
    void bandwidth(std::size_t& lower, std::size_t& upper) const;
    std::vector<double> scale_rows();
};

class BandedLU {
public:
    bool factor(const CsrMatrix& a);
    void solve(std::vector<double>& b) const;
    bool ready() const { return m_ready; }

    static double cost(const CsrMatrix& a);

private:
    double& at(std::size_t i, std::size_t j) { return m_band[i * m_w + (j + m_kl - i)]; }
    double at(std::size_t i, std::size_t j) const { return m_band[i * m_w + (j + m_kl - i)]; }

    std::size_t m_n = 0;
    std::size_t m_kl = 0;
    std::size_t m_ku = 0;
    std::size_t m_w = 1;
    std::vector<double> m_band;
    std::vector<std::size_t> m_piv;
    bool m_ready = false;
};

class Ilu0 {
public:
    void factor(const CsrMatrix& a);
    void apply(const std::vector<double>& r, std::vector<double>& z) const;
    bool ready() const { return m_ready; }

private:
    CsrMatrix m_lu;
    std::vector<std::size_t> m_diag;
    bool m_ready = false;
};

struct IterativeResult {
    bool converged = false;
    int iterations = 0;
    double residual = 0.0;
};

IterativeResult gmres(const CsrMatrix& a, const Ilu0* preconditioner, const std::vector<double>& b, std::vector<double>& x, double tolerance = 1e-10, int max_iterations = 1000, int restart = 40);
IterativeResult conjugate_gradient(const CsrMatrix& a, const std::vector<double>& b, std::vector<double>& x, double tolerance = 1e-10, int max_iterations = 5000);
IterativeResult bicgstab(const CsrMatrix& a, const Ilu0* preconditioner, const std::vector<double>& b, std::vector<double>& x, double tolerance = 1e-10, int max_iterations = 2000);

enum class SparseMethod { Automatic, BandedLU, Gmres, ConjugateGradient, BiCgStab };

class SparseSolver {
public:
    SparseMethod method = SparseMethod::Automatic;
    double band_budget = 6e8;
    double tolerance = 1e-11;
    int max_iterations = 1500;

    bool factor(const CsrMatrix& a);
    bool solve(std::vector<double>& b);
    bool solve(const std::vector<double>& b, std::vector<double>& x);

    bool ready() const { return m_ready; }
    SparseMethod chosen() const { return m_chosen; }
    std::string name() const;
    long long iterations() const { return m_iterations; }

private:
    CsrMatrix m_a;
    BandedLU m_lu;
    Ilu0 m_ilu;
    SparseMethod m_chosen = SparseMethod::BandedLU;
    bool m_ready = false;
    long long m_iterations = 0;
};

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_SPARSE_HPP
