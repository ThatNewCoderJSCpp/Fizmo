#ifndef FIZMO_GRAPHS_PROBABILITY_HPP
#define FIZMO_GRAPHS_PROBABILITY_HPP

#include "search.hpp"

#include <random>

namespace fizmo {
namespace graphs {

namespace gdetail {

template <class W>
W magnitude(const W& x) { return x < W(0) ? W(0) - x : x; }

template <class W>
bool close_to(const W& a, const W& b) { return !(W(1e-12) < magnitude(a - b)); }

template <class W>
double as_double(const W& x) { return static_cast<double>(x); }

template <class W>
std::vector<W> solve(std::vector<std::vector<W>> a, std::vector<W> b) {
    const std::size_t n = b.size();

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        for (std::size_t r = col + 1; r < n; ++r) {
            if (magnitude(a[pivot][col]) < magnitude(a[r][col])) pivot = r;
        }
        if (!(W(0) < magnitude(a[pivot][col]))) return std::vector<W>();
        std::swap(a[pivot], a[col]);
        std::swap(b[pivot], b[col]);
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const W f = a[r][col] / a[col][col];
            if (!(W(0) < magnitude(f))) continue;
            for (std::size_t c = col; c < n; ++c) a[r][c] = a[r][c] - f * a[col][c];
            b[r] = b[r] - f * b[col];
        }
    }

    for (std::size_t i = 0; i < n; ++i) b[i] = b[i] / a[i][i];
    return b;
}

} // namespace gdetail

template <class T = std::string, class W = double>
class MarkovChain : public Graph<T, W> {
public:
    MarkovChain() : Graph<T, W>(true) {}

    NodeId add_state(T value) { return this->add_node(std::move(value)); }

    void set_transition(NodeId from, NodeId to, W probability) { this->set_weight(from, to, std::move(probability)); }

    W transition(NodeId from, NodeId to) const {
        const Edge<W>* e = this->edge(from, to);
        return e ? e->weight : W(0);
    }

    W row_sum(NodeId from) const {
        W s = W(0);
        for (const Edge<W>& e : this->node(from).edges()) s = s + e.weight;
        return s;
    }

    bool valid() const {
        for (NodeId i = 0; i < this->size(); ++i) {
            if (this->node(i).degree() == 0) continue;
            if (!gdetail::close_to(row_sum(i), W(1))) return false;
            for (const Edge<W>& e : this->node(i).edges()) {
                if (e.weight < W(0)) return false;
            }
        }
        return true;
    }

    bool is_absorbing(NodeId id) const {
        const auto& es = this->node(id).edges();
        if (es.empty()) return true;
        for (const Edge<W>& e : es) {
            if (e.to != id && W(0) < e.weight) return false;
        }
        return true;
    }

    std::vector<NodeId> absorbing_states() const {
        std::vector<NodeId> out;
        for (NodeId i = 0; i < this->size(); ++i) {
            if (is_absorbing(i)) out.push_back(i);
        }
        return out;
    }

    std::vector<NodeId> transient_states() const {
        std::vector<NodeId> out;
        for (NodeId i = 0; i < this->size(); ++i) {
            if (!is_absorbing(i)) out.push_back(i);
        }
        return out;
    }

    std::vector<std::vector<W>> transition_matrix() const {
        const std::size_t           n = this->size();
        std::vector<std::vector<W>> m(n, std::vector<W>(n, W(0)));
        for (NodeId i = 0; i < n; ++i) {
            if (this->node(i).degree() == 0) m[i][i] = W(1);
            for (const Edge<W>& e : this->node(i).edges()) m[i][e.to] = m[i][e.to] + e.weight;
        }
        return m;
    }

    std::vector<W> point_distribution(NodeId state) const {
        std::vector<W> d(this->size(), W(0));
        d[state] = W(1);
        return d;
    }

    std::vector<W> step(const std::vector<W>& distribution) const {
        std::vector<W> next(this->size(), W(0));
        for (NodeId i = 0; i < this->size() && i < distribution.size(); ++i) {
            if (!(W(0) < distribution[i]) && !(distribution[i] < W(0))) continue;
            if (this->node(i).degree() == 0) {
                next[i] = next[i] + distribution[i];
                continue;
            }
            for (const Edge<W>& e : this->node(i).edges()) next[e.to] = next[e.to] + distribution[i] * e.weight;
        }
        return next;
    }

    std::vector<W> distribution_after(std::vector<W> distribution, std::size_t steps) const {
        for (std::size_t s = 0; s < steps; ++s) distribution = step(distribution);
        return distribution;
    }

    std::vector<W> distribution_after(NodeId start, std::size_t steps) const { return distribution_after(point_distribution(start), steps); }

    W probability(NodeId from, NodeId to, std::size_t steps) const { return distribution_after(from, steps)[to]; }

    W path_probability(const Path<W>& path) const { return path.probability(*this); }

    W path_probability(const std::vector<NodeId>& states) const { return Path<W>(states, W(0)).probability(*this); }

    std::vector<W> stationary_distribution() const {
        const std::size_t           n = this->size();
        std::vector<std::vector<W>> P = transition_matrix();
        std::vector<std::vector<W>> A(n, std::vector<W>(n, W(0)));
        std::vector<W>              b(n, W(0));
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) A[i][j] = P[j][i] - (i == j ? W(1) : W(0));
        }
        for (std::size_t j = 0; j < n; ++j) A[n - 1][j] = W(1);
        if (n > 0) b[n - 1] = W(1);
        return gdetail::solve(std::move(A), std::move(b));
    }

    std::vector<std::vector<W>> absorption_probabilities() const {
        const std::vector<NodeId>   tr = transient_states();
        const std::vector<NodeId>   ab = absorbing_states();
        const std::size_t           t  = tr.size();
        std::vector<std::vector<W>> out(t, std::vector<W>(ab.size(), W(0)));
        if (t == 0) return out;
        std::vector<std::vector<W>> IQ = fundamental_system(tr);

        for (std::size_t k = 0; k < ab.size(); ++k) {
            std::vector<W> r(t, W(0));
            for (std::size_t i = 0; i < t; ++i) r[i] = transition(tr[i], ab[k]);
            const std::vector<W> col = gdetail::solve(IQ, r);
            if (col.empty()) continue;
            for (std::size_t i = 0; i < t; ++i) out[i][k] = col[i];
        }

        return out;
    }

    std::vector<W> expected_steps_to_absorption() const {
        const std::vector<NodeId> tr = transient_states();
        if (tr.empty()) return std::vector<W>();
        return gdetail::solve(fundamental_system(tr), std::vector<W>(tr.size(), W(1)));
    }

    template <class Rng>
    Path<W> simulate(NodeId start, std::size_t steps, Rng& rng) const {
        Path<W>                                path(start);
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        NodeId                                 at = start;

        for (std::size_t s = 0; s < steps; ++s) {
            const auto& es = this->node(at).edges();
            if (es.empty()) break;
            double       u    = uni(rng);
            NodeId       next = es.back().to;
            for (const Edge<W>& e : es) {
                u -= gdetail::as_double(e.weight);
                if (u < 0.0) {
                    next = e.to;
                    break;
                }
            }
            path.extend(next, W(1));
            at = next;
        }

        return path;
    }

private:
    std::vector<std::vector<W>> fundamental_system(const std::vector<NodeId>& tr) const {
        const std::size_t           t = tr.size();
        std::vector<std::vector<W>> IQ(t, std::vector<W>(t, W(0)));
        for (std::size_t i = 0; i < t; ++i) {
            for (std::size_t j = 0; j < t; ++j) IQ[i][j] = (i == j ? W(1) : W(0)) - transition(tr[i], tr[j]);
        }
        return IQ;
    }
};

template <class T = std::string, class W = double>
class ProbabilityTree : public Graph<T, W> {
public:
    ProbabilityTree() : Graph<T, W>(true) {
        m_root = this->add_node(T{});
        m_parent.push_back(invalid_node);
    }

    explicit ProbabilityTree(T root_label) : Graph<T, W>(true) {
        m_root = this->add_node(std::move(root_label));
        m_parent.push_back(invalid_node);
    }

    NodeId root() const { return m_root; }

    NodeId branch(NodeId parent, T outcome, W probability) {
        const NodeId id = this->add_node(std::move(outcome));
        m_parent.resize(this->size(), invalid_node);
        m_parent[id] = parent;
        this->add_edge(parent, id, std::move(probability));
        return id;
    }

    std::vector<NodeId> branches(NodeId parent, const std::vector<std::pair<T, W>>& outcomes) {
        std::vector<NodeId> ids;
        for (const auto& o : outcomes) ids.push_back(branch(parent, o.first, o.second));
        return ids;
    }

    NodeId parent(NodeId id) const { return id < m_parent.size() ? m_parent[id] : invalid_node; }
    std::vector<NodeId> children(NodeId id) const { return this->neighbors(id); }
    bool is_leaf(NodeId id) const { return this->node(id).degree() == 0; }

    std::vector<NodeId> leaves() const {
        std::vector<NodeId> out;
        for (NodeId i = 0; i < this->size(); ++i) {
            if (is_leaf(i)) out.push_back(i);
        }
        return out;
    }

    std::size_t depth(NodeId id) const {
        std::size_t d = 0;
        for (NodeId at = id; at != m_root && at != invalid_node; at = parent(at)) ++d;
        return d;
    }

    Path<W> path_to(NodeId id) const {
        std::vector<NodeId> nodes;
        for (NodeId at = id; at != invalid_node; at = parent(at)) nodes.push_back(at);
        std::reverse(nodes.begin(), nodes.end());
        return Path<W>(std::move(nodes), probability_of(id));
    }

    std::vector<T> outcomes_to(NodeId id) const {
        std::vector<T> out;
        for (NodeId at = id; at != m_root && at != invalid_node; at = parent(at)) out.push_back(this->value(at));
        std::reverse(out.begin(), out.end());
        return out;
    }

    W probability_of(NodeId id) const {
        W p = W(1);
        for (NodeId at = id; at != m_root && at != invalid_node; at = parent(at)) p = p * this->weight(parent(at), at);
        return p;
    }

    std::vector<std::pair<std::vector<T>, W>> outcomes() const {
        std::vector<std::pair<std::vector<T>, W>> out;
        for (NodeId leaf : leaves()) {
            if (leaf == m_root && this->size() > 1) continue;
            out.emplace_back(outcomes_to(leaf), probability_of(leaf));
        }
        return out;
    }

    template <class Event>
    W probability_that(Event&& event) const {
        W total = W(0);
        for (NodeId leaf : leaves()) {
            if (event(outcomes_to(leaf))) total = total + probability_of(leaf);
        }
        return total;
    }

    W probability_of_outcome(const T& outcome) const {
        return probability_that([&](const std::vector<T>& seq) { return std::find(seq.begin(), seq.end(), outcome) != seq.end(); });
    }

    W probability_of_sequence(const std::vector<T>& prefix) const {
        return probability_that([&](const std::vector<T>& seq) {
            return seq.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), seq.begin());
        });
    }

    template <class A, class B>
    W conditional(A&& event, B&& given) const {
        const W both = probability_that([&](const std::vector<T>& s) { return event(s) && given(s); });
        const W g    = probability_that(given);
        return both / g;
    }

    W conditional_outcome(const T& outcome, const T& given) const {
        auto has = [](const T& x) {
            return [x](const std::vector<T>& s) { return std::find(s.begin(), s.end(), x) != s.end(); };
        };
        return conditional(has(outcome), has(given));
    }

    template <class Value>
    W expected_value(Value&& value_of) const {
        W total = W(0);
        for (NodeId leaf : leaves()) total = total + probability_of(leaf) * W(value_of(outcomes_to(leaf)));
        return total;
    }

    bool valid() const {
        for (NodeId i = 0; i < this->size(); ++i) {
            if (is_leaf(i)) continue;
            W s = W(0);
            for (const Edge<W>& e : this->node(i).edges()) {
                if (e.weight < W(0)) return false;
                s = s + e.weight;
            }
            if (!gdetail::close_to(s, W(1))) return false;
        }
        return true;
    }

private:
    NodeId              m_root = invalid_node;
    std::vector<NodeId> m_parent;
};

} // namespace graphs
} // namespace fizmo

#endif // FIZMO_GRAPHS_PROBABILITY_HPP
