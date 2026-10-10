#ifndef FIZMO_POLYNOMIAL_SOLVER_HPP
#define FIZMO_POLYNOMIAL_SOLVER_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../Common/main_convenience.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <optional>

namespace fizmo {
namespace math {
namespace solvers {

struct SymbolicRoot {
    cas::Expression expr;
    std::uint32_t   multiplicity = 1;

    double evaluate(const std::string& var = {}) const {
        return expr.evaluate({ 
            {var, 0.0} 
        });
    }

    friend std::ostream& operator<<(std::ostream& os, const SymbolicRoot& r) {
        os << r.expr;
        if (r.multiplicity > 1) os << "  (multiplicity " << r.multiplicity << ")";
        return os;
    }
};

struct PolynomialSolution {
    std::vector<SymbolicRoot> roots;
    std::uint32_t             degree     = 0;
    bool                      degenerate = false;   // infinite or no solutions
    bool                      infinite   = false;

    std::size_t root_count()     const { return roots.size(); }
    bool        has_solutions()  const { return !roots.empty() || infinite; }

    friend std::ostream& operator<<(std::ostream& os, const PolynomialSolution& s) {
        if (s.infinite)   { os << "infinite solutions (identity)"; return os; }
        if (s.degenerate) { os << "no solutions (contradiction)";  return os; }
        os << "degree-" << s.degree << " polynomial, " << s.roots.size() << " root(s):\n";
        for (std::size_t i = 0; i < s.roots.size(); ++i) os << "  x" << (i + 1) << " = " << s.roots[i] << "\n";
        return os;
    }
};

class PolynomialSolver {
public:
    static PolynomialSolution solve_linear(
        cas::SymbolicContext& ctx,
        const cas::Expression& a,
        const cas::Expression& b
    ) {
        PolynomialSolution sol;
        sol.degree = 1;
        cas::Expression x = ctx.full_simplify(-b / a);
        sol.roots.push_back({x, 1});
        return sol;
    }

    static PolynomialSolution solve_quadratic(
        cas::SymbolicContext& ctx,
        const cas::Expression& a,
        const cas::Expression& b,
        const cas::Expression& c
    ) {
        PolynomialSolution sol;
        sol.degree = 2;
        using namespace cas;
        Expression disc = ctx.full_simplify(b * b - ctx.constant(4.0) * a * c);
        Expression sqrt_disc = SQRT(ctx, disc);
        Expression two_a = ctx.constant(2.0) * a;
        Expression x1 = ctx.full_simplify((-b + sqrt_disc) / two_a);
        Expression x2 = ctx.full_simplify((-b - sqrt_disc) / two_a);

        if (is_zero_constant(disc)) {
            sol.roots.push_back({x1, 2});
        } else {
            sol.roots.push_back({x1, 1});
            sol.roots.push_back({x2, 1});
        }

        return sol;
    }

    static PolynomialSolution solve_cubic(
        cas::SymbolicContext& ctx,
        const cas::Expression& a,
        const cas::Expression& b,
        const cas::Expression& c,
        const cas::Expression& d
    ) {
        PolynomialSolution sol;
        sol.degree = 3;
        using namespace cas;
        auto K = [&](double v) { return ctx.constant(v); };
        auto S = [&](const Expression& e) { return ctx.full_simplify(e); };
        Expression a2 = a * a;
        Expression b2 = b * b;
        Expression shift = S(b / (K(3.0) * a));
        Expression p = S((K(3.0) * a * c - b2) / (K(3.0) * a2));
        Expression q = S((K(2.0) * b2 * b - K(9.0) * a * b * c + K(27.0) * a2 * d) / (K(27.0) * a2 * a));
        Expression disc = S(K(-4.0) * p * p * p - K(27.0) * q * q);
        auto add_root = [&](const Expression& t, std::uint32_t mult) { sol.roots.push_back({S(t - shift), mult}); };

        auto solve_residual_quadratic = [&](const Expression& t1, bool filter_real) {
            Expression qb = t1;
            Expression qc = S(t1 * t1 + p);
            Expression qd = S(qb * qb - K(4.0) * qc);   
            if (filter_real && is_constant(qd) && is_negative_constant(qd)) return;                                 
            Expression sqrt_qd = SQRT(ctx, qd);
            Expression t2 = S((-qb + sqrt_qd) / K(2.0));
            Expression t3 = S((-qb - sqrt_qd) / K(2.0));

            if (is_zero_constant(qd)) {
                add_root(t2, 2);
            } else {
                add_root(t2, 1);
                add_root(t3, 1);
            }
        };

        if (is_constant(disc)) {
            if (is_zero_constant(disc)) {
                if (is_zero_constant(p) && is_zero_constant(q)) {
                    add_root(K(0.0), 3);            
                } else {
                    add_root(S(K(-3.0) * q / p), 1);
                    add_root(S(K(3.0) * q / (K(2.0) * p)), 2);
                }
            } else if (is_positive_constant(disc)) {
                Expression neg_p_3  = S(-p / K(3.0));            
                Expression two_r    = S(K(2.0) * SQRT(ctx, neg_p_3));
                Expression cos_arg  = S(K(3.0) * q / (K(2.0) * p) * SQRT(ctx, K(-3.0) / p));
                Expression theta    = S(ACOS(ctx, cos_arg) / K(3.0));
                Expression pi       = K(constants::pi());
                Expression two_pi_3 = S(K(2.0) * pi / K(3.0));
                add_root(S(two_r * COS(ctx, theta)),               1);
                add_root(S(two_r * COS(ctx, theta - two_pi_3)),    1);
                add_root(S(two_r * COS(ctx, theta + two_pi_3)),    1);
            } else {
                Expression h     = S(q * q / K(4.0) + p * p * p / K(27.0));
                Expression sq    = SQRT(ctx, h);
                Expression mq2   = S(-q / K(2.0));
                Expression u     = S(CBRT(ctx, mq2 + sq));
                Expression v     = S(CBRT(ctx, mq2 - sq));
                Expression t1    = S(u + v);
                add_root(t1, 1);
            }
        } else {
            Expression h     = S(q * q / K(4.0) + p * p * p / K(27.0));
            Expression sq    = SQRT(ctx, h);
            Expression mq2   = S(-q / K(2.0));
            Expression u     = S(CBRT(ctx, mq2 + sq));
            Expression v     = S(CBRT(ctx, mq2 - sq));
            Expression t1    = S(u + v);
            add_root(t1, 1);
            solve_residual_quadratic(t1, /*filter_real=*/false);
        }

        return sol;
    }

    static PolynomialSolution solve_quartic(
        cas::SymbolicContext& ctx,
        const cas::Expression& a,
        const cas::Expression& b,
        const cas::Expression& c,
        const cas::Expression& d,
        const cas::Expression& e
    ) {
        PolynomialSolution sol;
        sol.degree = 4;
        using namespace cas;
        auto K = [&](double v) { return ctx.constant(v); };
        auto S = [&](const Expression& e_) { return ctx.full_simplify(e_); };
        Expression B = S(b / a), C = S(c / a), D = S(d / a), E = S(e / a);
        Expression B2 = B * B;
        Expression shift = S(B / K(4.0));
        Expression p = S(C - K(3.0) * B2 / K(8.0));
        Expression q = S(B2 * B / K(8.0) - B * C / K(2.0) + D);
        Expression r = S(K(-3.0) * B2 * B2 / K(256.0) + B2 * C / K(16.0) - B * D / K(4.0) + E);

        auto solve_sub_quadratic = [&](const Expression& qb, const Expression& qc) {
            Expression qd = S(qb * qb - K(4.0) * qc);
            if (is_constant(qd) && is_negative_constant(qd)) return;                                  
            Expression sqrt_qd = SQRT(ctx, qd);
            Expression x1 = S((-qb + sqrt_qd) / K(2.0));
            Expression x2 = S((-qb - sqrt_qd) / K(2.0));

            if (is_zero_constant(qd)) {
                sol.roots.push_back({S(x1 - shift), 2});
            } else {
                sol.roots.push_back({S(x1 - shift), 1});
                sol.roots.push_back({S(x2 - shift), 1});
            }
        };

        if (is_zero_constant(q)) {
            Expression bi_disc = S(p * p - K(4.0) * r);
            Expression sqrt_bd = SQRT(ctx, bi_disc);
            Expression u1 = S((-p + sqrt_bd) / K(2.0));
            Expression u2 = S((-p - sqrt_bd) / K(2.0));
            bool can_filter = is_constant(bi_disc);

            auto add_sqrt_pair = [&](const Expression& u) {
                if (can_filter && is_constant(u) && is_negative_constant(u)) return;                                

                if (is_zero_constant(u)) {
                    sol.roots.push_back({S(K(0.0) - shift), 2});
                } else {
                    Expression su = SQRT(ctx, u);
                    sol.roots.push_back({S(su - shift),  1});
                    sol.roots.push_back({S(-su - shift), 1});
                }
            };

            if (is_zero_constant(bi_disc)) {
                add_sqrt_pair(u1);                         
            } else {
                add_sqrt_pair(u1);
                add_sqrt_pair(u2);
            }

            return sol;
        }

        Expression rc_a = K(8.0);
        Expression rc_b = S(K(-4.0) * p);
        Expression rc_c = S(K(-8.0) * r);
        Expression rc_d = S(K(4.0) * p * r - q * q);
        PolynomialSolution cubic_sol = solve_cubic(ctx, rc_a, rc_b, rc_c, rc_d);
        Expression m;
        bool found_m = false;

        for (auto& root : cubic_sol.roots) {
            Expression candidate = S(K(2.0) * root.expr - p);
            if (is_zero_constant(candidate)) continue;     
            m = root.expr;
            found_m = true;
            break;
        }

        if (!found_m) {
            if (!cubic_sol.roots.empty()) {
                m = cubic_sol.roots[0].expr;
                found_m = true;
            }
        }

        if (!found_m) { return sol; }
        Expression s_sq = S(K(2.0) * m - p);
        Expression s    = S(SQRT(ctx, s_sq));
        Expression h    = S(q / (K(2.0) * s));           
        solve_sub_quadratic(s, S(m + h));
        solve_sub_quadratic(S(-s), S(m - h));
        return sol;
    }

    static PolynomialSolution solve_polynomial(
        cas::SymbolicContext& ctx,
        std::vector<cas::Expression> coeffs
    ) {
        using namespace cas;
        PolynomialSolution sol;
        while (coeffs.size() > 1 && is_zero_constant(coeffs.front())) coeffs.erase(coeffs.begin());
        std::uint32_t deg = static_cast<std::uint32_t>(coeffs.size() - 1);
        sol.degree = deg;

        if (deg == 0) {
            if (is_zero_constant(coeffs[0])) { sol.infinite = true; }
            else                              { sol.degenerate = true; }
            return sol;
        }

        if (deg == 1) return solve_linear(ctx, coeffs[0], coeffs[1]);
        if (deg == 2) return solve_quadratic(ctx, coeffs[0], coeffs[1], coeffs[2]);
        if (deg == 3) return solve_cubic(ctx, coeffs[0], coeffs[1], coeffs[2], coeffs[3]);
        if (deg == 4) return solve_quartic(ctx, coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
        auto K = [&](double v) { return ctx.constant(v); };
        auto S = [&](const Expression& e) { return ctx.full_simplify(e); };
        bool all_constant = true;
        for (auto& c : coeffs) { if (!is_constant(c)) { all_constant = false; break; }}

        {
            std::uint32_t trailing_zeros = 0;

            for (std::size_t i = coeffs.size(); i-- > 0; ) {
                if (is_zero_constant(coeffs[i])) ++trailing_zeros;
                else break;
            }

            if (trailing_zeros > 0) {
                std::vector<Expression> reduced(coeffs.begin(), coeffs.end() - trailing_zeros);
                PolynomialSolution sub = solve_polynomial(ctx, std::move(reduced));
                sol.roots = std::move(sub.roots);
                sol.roots.push_back({K(0.0), trailing_zeros});
                sol.degree = deg;
                return sol;
            }
        }

        if (deg % 2 == 0) {
            bool only_even = true;

            for (std::size_t i = 0; i < coeffs.size(); ++i) {
                if ((deg - i) % 2 != 0 && !is_zero_constant(coeffs[i])) {
                    only_even = false;
                    break;
                }
            }

            if (only_even) {
                std::vector<Expression> half_coeffs;
                for (std::size_t i = 0; i < coeffs.size(); i += 2) half_coeffs.push_back(coeffs[i]);
                PolynomialSolution u_sol = solve_polynomial(ctx, std::move(half_coeffs));
                sol.degree = deg;

                for (auto& u_root : u_sol.roots) {
                    if (is_zero_constant(u_root.expr)) {
                        sol.roots.push_back({K(0.0), u_root.multiplicity * 2});
                    } else if (is_constant(u_root.expr) && is_negative_constant(u_root.expr)) {
                        // complex 
                    } else {
                        Expression su = S(SQRT(ctx, u_root.expr));
                        sol.roots.push_back({ su,       u_root.multiplicity});
                        sol.roots.push_back({S(-su),    u_root.multiplicity});
                    }
                }

                return sol;
            }
        }

        if (all_constant) {
            auto try_rational_roots = [&](std::vector<Expression>& c) -> bool {
                double a_n = c.front().node()->constant;
                double a_0 = c.back().node()->constant;
                if (a_n == 0.0 || a_0 == 0.0) return false;

                auto integer_divisors = [](double val) -> std::vector<int> {
                    int v = static_cast<int>(std::round(std::abs(val)));
                    if (v == 0 || std::abs(val - static_cast<double>(v)) >= constants::epsilon()) return {};
                    std::vector<int> divs;

                    for (int d = 1; d * d <= v; ++d) {
                        if (v % d == 0) {
                            divs.push_back(d);
                            if (d != v / d) divs.push_back(v / d);
                        }
                    }

                    return divs;
                };

                auto p_divs = integer_divisors(a_0);
                auto q_divs = integer_divisors(a_n);
                if (p_divs.empty() || q_divs.empty()) return false;

                auto eval_poly = [&](const std::vector<Expression>& co, double x) -> double {
                    double result = co[0].node()->constant;
                    for (std::size_t i = 1; i < co.size(); ++i) result = result * x + co[i].node()->constant;
                    return result;
                };

                auto synthetic_div = [&](const std::vector<Expression>& co, double r) -> std::vector<Expression> {
                    std::vector<Expression> q;
                    double carry = 0.0;

                    for (std::size_t i = 0; i < co.size() - 1; ++i) {
                        carry = co[i].node()->constant + carry * r;
                        q.push_back(K(carry));
                    }

                    return q;
                };

                bool found_any = false;
                constexpr std::size_t max_candidates = 200;
                std::size_t tested = 0;

                for (int p : p_divs) {
                    for (int q : q_divs) {
                        for (int sign : {+1, -1}) {
                            if (tested++ >= max_candidates) return found_any;
                            double candidate = sign * static_cast<double>(p) / static_cast<double>(q);
                            double val = eval_poly(c, candidate);

                            if (std::abs(val) <= constants::epsilon()) {
                                std::uint32_t mult = 0;
                                auto quotient = c;

                                while (true) {
                                    double v2 = eval_poly(quotient, candidate);
                                    if (std::abs(v2) > 1e-9) break;
                                    quotient = synthetic_div(quotient, candidate);
                                    ++mult;
                                    if (quotient.empty()) break;
                                }

                                sol.roots.push_back({K(candidate), mult});
                                c = quotient;
                                found_any = true;
                                if (c.size() <= 5) return found_any;
                            }
                        }
                    }
                }
                
                return found_any;
            };

            std::vector<Expression> working = coeffs;
            try_rational_roots(working);

            if (working.size() >= 2 && working.size() <= 5) {
                PolynomialSolution remainder = solve_polynomial(ctx, std::move(working));
                for (auto& r : remainder.roots) sol.roots.push_back(r);
            } else if (working.size() > 5) {
                // No closed-form available.
                // Leave partial roots 
            }
        }

        sol.degree = deg;
        return sol;
    }

    static PolynomialSolution solve_linear(
        const cas::Expression& a, const cas::Expression& b
    ) { return solve_linear(cas::global_context(), a, b); }

    static PolynomialSolution solve_quadratic(
        const cas::Expression& a, const cas::Expression& b, const cas::Expression& c
    ) { return solve_quadratic(cas::global_context(), a, b, c); }

    static PolynomialSolution solve_cubic(
        const cas::Expression& a, const cas::Expression& b,
        const cas::Expression& c, const cas::Expression& d
    ) { return solve_cubic(cas::global_context(), a, b, c, d); }

    static PolynomialSolution solve_quartic(
        const cas::Expression& a, const cas::Expression& b,
        const cas::Expression& c, const cas::Expression& d,
        const cas::Expression& e
    ) { return solve_quartic(cas::global_context(), a, b, c, d, e); }

    static PolynomialSolution solve_polynomial(
        std::vector<cas::Expression> coeffs
    ) { return solve_polynomial(cas::global_context(), std::move(coeffs)); }

    static PolynomialSolution solve_polynomial(
        cas::SymbolicContext& ctx,
        std::initializer_list<cas::Expression> coeffs
    ) { return solve_polynomial(ctx, std::vector<cas::Expression>(coeffs)); }

    static PolynomialSolution solve_polynomial(
        std::initializer_list<cas::Expression> coeffs
    ) { return solve_polynomial(cas::global_context(), std::vector<cas::Expression>(coeffs)); }

private:
    static bool is_constant(const cas::Expression& e) {
        if (e.is_invalid()) return false;
        auto* n = e.node();
        return n && n->type == cas::symbols::NodeType::Constant;
    }

    static bool is_zero_constant(const cas::Expression& e) {
        if (e.is_invalid()) return false;
        auto* n = e.node();
        return n && n->type == cas::symbols::NodeType::Constant && n->constant == 0.0;
    }

    static bool is_positive_constant(const cas::Expression& e) {
        if (e.is_invalid()) return false;
        auto* n = e.node();
        return n && n->type == cas::symbols::NodeType::Constant && n->constant > 0.0;
    }

    static bool is_negative_constant(const cas::Expression& e) {
        if (e.is_invalid()) return false;
        auto* n = e.node();
        return n && n->type == cas::symbols::NodeType::Constant && n->constant < 0.0;
    }
};

} // namespace solvers
} // namespace math
} // namespace fizmo

#endif // FIZMO_POLYNOMIAL_SOLVER_HPP