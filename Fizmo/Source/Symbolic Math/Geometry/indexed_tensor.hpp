#ifndef FIZMO_INDEXED_TENSOR_HPP
#define FIZMO_INDEXED_TENSOR_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../CAS/expression_wrapper.hpp"
#include "../CAS/symbolic_context.hpp"
#include "../Common/main_convenience.hpp"
#include "../Linalg/tensors.hpp"

namespace fizmo {
namespace math {
namespace tensors {

enum class Variance { Up = 0, Down };

constexpr inline Variance flip(Variance v) noexcept { return v == Variance::Up ? Variance::Down : Variance::Up; }

inline const char* variance_label(Variance v) noexcept {
    return v == Variance::Up ? "Up" : "Down";
}

struct IndexSlot {
    std::string name;       
    Variance    variance;   // Up = contravariant, Down = covariant

    IndexSlot() = default;
    IndexSlot(std::string n, Variance v) : name(std::move(n)), variance(v) {}

    bool operator==(const IndexSlot& o) const { return name == o.name && variance == o.variance; }
    bool operator!=(const IndexSlot& o) const { return !(*this == o); }
    bool contracts_with(const IndexSlot& o) const { return name == o.name && variance != o.variance; }

    static IndexSlot up(std::string name)   { return { std::move(name), Variance::Up   }; }
    static IndexSlot down(std::string name) { return { std::move(name), Variance::Down }; }

    friend std::ostream& operator<<(std::ostream& os, const IndexSlot& s) { return os << s.name << (s.variance == Variance::Up ? "^" : "_"); }
};

class IndexedTensor {
public:
    IndexedTensor() = default;

    IndexedTensor(SymbolicSquareTensor data, std::vector<IndexSlot> indices) : data_(std::move(data)), indices_(std::move(indices)) {
        if (indices_.size() != data_.rank()) {
            throw std::invalid_argument(
                "IndexedTensor: slot count (" + std::to_string(indices_.size())
                + ") != tensor rank (" + std::to_string(data_.rank()) + ")"
            );
        }
    }

    IndexedTensor(SymbolicSquareTensor data, std::initializer_list<IndexSlot> indices) : IndexedTensor(std::move(data), std::vector<IndexSlot>(indices)) {}

public:
    static IndexedTensor zero(std::size_t dim, std::vector<IndexSlot> indices) { return { SymbolicSquareTensor::zero(dim, indices.size()), std::move(indices) }; }
    static IndexedTensor from_matrix(const matrices::SymbolicMatrixN& m, IndexSlot row, IndexSlot col) { return { SymbolicSquareTensor::from(m), { std::move(row), std::move(col) } }; }
    static IndexedTensor from_vector(const vectors::SymbolicVectorN& v, IndexSlot slot) { return { SymbolicSquareTensor::from(v), { std::move(slot) } }; }

    static IndexedTensor kronecker_delta(std::size_t dim, std::string up_name, std::string down_name) {
        return { SymbolicSquareTensor::kronecker(dim, 2), { IndexSlot::up(std::move(up_name)), IndexSlot::down(std::move(down_name)) } };
    }

public:
    const SymbolicSquareTensor& tensor()  const noexcept { return data_;    }
    SymbolicSquareTensor&       tensor()        noexcept { return data_;    }
    const std::vector<IndexSlot>& indices() const noexcept { return indices_; }
    std::size_t dim()   const noexcept { return data_.dim();  }
    std::size_t rank()  const noexcept { return data_.rank(); }
    std::size_t size()  const noexcept { return data_.size(); }

public:
    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_.at(idx); }
    cas::Expression&       flat_at(std::size_t i)       { return data_.flat_at(i); }
    const cas::Expression& flat_at(std::size_t i) const { return data_.flat_at(i); }

    const IndexSlot& slot(std::size_t axis) const { return indices_.at(axis); }

public:
    std::size_t axis_of(const std::string& name) const noexcept {
        for (std::size_t i = 0; i < indices_.size(); ++i) if (indices_[i].name == name) return i;
        return -1;
    }

    bool has_index(const std::string& name) const noexcept {
        for (auto& s : indices_) if (s.name == name) return true;
        return false;
    }
    
    IndexedTensor rename_index(const std::string& old_name, const std::string& new_name) const {
        auto slots = indices_;
        for (auto& s : slots) if (s.name == old_name) { s.name = new_name; break; }
        return { data_, std::move(slots) };
    }

    IndexedTensor relabel(std::vector<IndexSlot> new_indices) const {
        if (new_indices.size() != rank()) throw std::invalid_argument("IndexedTensor::relabel: slot count mismatch");
        return { data_, std::move(new_indices) };
    }

public:
    IndexedTensor operator+(const IndexedTensor& o) const {
        assert_same_indices(o);
        return { data_ + o.data_, indices_ };
    }

    IndexedTensor operator-(const IndexedTensor& o) const {
        assert_same_indices(o);
        return { data_ - o.data_, indices_ };
    }

    IndexedTensor operator-() const {
        return { -data_, indices_ };
    }

    IndexedTensor operator*(const cas::Expression& s) const { return { data_ * s, indices_ }; }
    IndexedTensor operator*(double s)            const { return { data_ * s, indices_ }; }
    IndexedTensor operator/(const cas::Expression& s) const { return { data_ / s, indices_ }; }
    IndexedTensor operator/(double s)            const { return { data_ / s, indices_ }; }

    IndexedTensor& operator+=(const IndexedTensor& o) { return *this = *this + o; }
    IndexedTensor& operator-=(const IndexedTensor& o) { return *this = *this - o; }
    IndexedTensor& operator*=(const cas::Expression& s)    { return *this = *this * s; }
    IndexedTensor& operator*=(double s)               { return *this = *this * s; }

    friend IndexedTensor operator*(const cas::Expression& s, const IndexedTensor& t) { return t * s; }
    friend IndexedTensor operator*(double s,            const IndexedTensor& t) { return t * s; }

public:
    friend IndexedTensor einstein(const IndexedTensor& a, const IndexedTensor& b) {
        if (a.dim() != b.dim()) throw std::invalid_argument("IndexedTensor::einstein: dimension mismatch (" + std::to_string(a.dim()) + " vs " + std::to_string(b.dim()) + ")");

        struct Pair { std::size_t a_axis; std::size_t b_axis; };
        std::vector<Pair> pairs;

        for (std::size_t i = 0; i < a.indices_.size(); ++i) {
            for (std::size_t j = 0; j < b.indices_.size(); ++j) {
                if (a.indices_[i].contracts_with(b.indices_[j])) {
                    pairs.push_back({ i, j });
                    break;
                }
            }
        }

        if (pairs.empty()) { return outer_product(a, b); }
        auto result = outer_product(a, b);
        struct AbsPair { std::size_t left; std::size_t right; };
        std::vector<AbsPair> abs_pairs;
        for (auto& p : pairs) abs_pairs.push_back({ p.a_axis, a.rank() + p.b_axis });

        std::sort(abs_pairs.begin(), abs_pairs.end(), [](const AbsPair& x, const AbsPair& y) {
            return std::max(x.left, x.right) > std::max(y.left, y.right);
        });

        for (auto& ap : abs_pairs) {
            std::size_t ax_lo = std::min(ap.left, ap.right);
            std::size_t ax_hi = std::max(ap.left, ap.right);
            auto new_data = result.data_.contract(ax_lo, ax_hi);
            std::vector<IndexSlot> new_slots;
            new_slots.reserve(result.indices_.size() - 2);
            for (std::size_t k = 0; k < result.indices_.size(); ++k) if (k != ax_lo && k != ax_hi) new_slots.push_back(result.indices_[k]);

            for (auto& other : abs_pairs) {
                if (&other == &ap) continue;
                if (other.left  > ax_hi) other.left  -= 2;
                else if (other.left  > ax_lo) other.left  -= 1;
                if (other.right > ax_hi) other.right -= 2;
                else if (other.right > ax_lo) other.right -= 1;
            }

            result.data_    = std::move(new_data);
            result.indices_ = std::move(new_slots);
        }

        return result;
    }

    friend IndexedTensor operator%(const IndexedTensor& a, const IndexedTensor& b) {
        return einstein(a, b);
    }

    static IndexedTensor outer_product(const IndexedTensor& a, const IndexedTensor& b) {
        auto combined = SymbolicSquareTensor::outer(a.data_, b.data_);
        std::vector<IndexSlot> slots;
        slots.reserve(a.indices_.size() + b.indices_.size());
        slots.insert(slots.end(), a.indices_.begin(), a.indices_.end());
        slots.insert(slots.end(), b.indices_.begin(), b.indices_.end());
        return { std::move(combined), std::move(slots) };
    }

    IndexedTensor outer_product(const IndexedTensor& b) const {
        return IndexedTensor::outer_product(*this, b);
    }

    IndexedTensor contract_on(const std::string& name) const {
        std::size_t up_ax = rank(), down_ax = rank();

        for (std::size_t i = 0; i < indices_.size(); ++i) {
            if (indices_[i].name != name) continue;
            if (indices_[i].variance == Variance::Up   && up_ax   == rank()) up_ax   = i;
            else if (indices_[i].variance == Variance::Down && down_ax == rank()) down_ax = i;
        }

        if (up_ax == rank() || down_ax == rank()) throw std::invalid_argument("IndexedTensor::contract_on: need one Up and one Down index named '" + name + "'");
        std::size_t lo = std::min(up_ax, down_ax);
        std::size_t hi = std::max(up_ax, down_ax);
        auto new_data = data_.contract(lo, hi);
        std::vector<IndexSlot> new_slots;
        new_slots.reserve(indices_.size() - 2);
        for (std::size_t k = 0; k < indices_.size(); ++k) if (k != lo && k != hi) new_slots.push_back(indices_[k]);
        return { std::move(new_data), std::move(new_slots) };
    }

    IndexedTensor raise(const std::string& idx_name, const SymbolicSquareTensor& metric_inv) const {
        std::size_t ax = axis_of(idx_name);
        if (indices_[ax].variance != Variance::Down) throw std::invalid_argument("IndexedTensor::raise: index '" + idx_name + "' is already Up");
        std::string tmp = unique_name(idx_name);
        IndexedTensor g_inv(metric_inv, { IndexSlot::up(tmp), IndexSlot::up(idx_name) });
        auto result = einstein(*this, g_inv);
        return result.rename_index(tmp, idx_name);
    }

    IndexedTensor lower(const std::string& idx_name, const SymbolicSquareTensor& metric) const {
        std::size_t ax = axis_of(idx_name);
        if (indices_[ax].variance != Variance::Up) throw std::invalid_argument("IndexedTensor::lower: index '" + idx_name + "' is already Down");
        std::string tmp = unique_name(idx_name);
        IndexedTensor g(metric, { IndexSlot::down(tmp), IndexSlot::down(idx_name) });
        auto result = einstein(*this, g);
        return result.rename_index(tmp, idx_name);
    }
    
    IndexedTensor swap_indices(const std::string& a, const std::string& b) const {
        std::size_t ax_a = axis_of(a);
        std::size_t ax_b = axis_of(b);
        auto new_data = data_.swap_axes(ax_a, ax_b);
        auto new_slots = indices_;
        std::swap(new_slots[ax_a], new_slots[ax_b]);
        return { std::move(new_data), std::move(new_slots) };
    }

    IndexedTensor symmetrise_over(const std::vector<std::string>& names) const {
        std::vector<std::size_t> axes;
        for (auto& n : names) axes.push_back(axis_of(n));
        auto result = IndexedTensor::zero(dim(), indices_);
        std::vector<std::size_t> perm_order(axes.size());
        std::iota(perm_order.begin(), perm_order.end(), 0);
        std::size_t count = 0;

        do {
            std::vector<std::size_t> full_perm(rank());
            std::iota(full_perm.begin(), full_perm.end(), 0);
            for (std::size_t i = 0; i < axes.size(); ++i) full_perm[axes[i]] = axes[perm_order[i]];
            result.data_ += data_.transpose(full_perm);
            ++count;
        } while (std::next_permutation(perm_order.begin(), perm_order.end()));

        result.data_ /= static_cast<double>(count);
        return result;
    }

    IndexedTensor antisymmetrise_over(const std::vector<std::string>& names) const {
        std::vector<std::size_t> axes;
        for (auto& n : names) axes.push_back(axis_of(n));
        auto result = IndexedTensor::zero(dim(), indices_);
        std::vector<std::size_t> perm_order(axes.size());
        std::iota(perm_order.begin(), perm_order.end(), 0);
        std::size_t count = 0;

        do {
            std::vector<std::size_t> full_perm(rank());
            std::iota(full_perm.begin(), full_perm.end(), 0);
            for (std::size_t i = 0; i < axes.size(); ++i) full_perm[axes[i]] = axes[perm_order[i]];
            int sign = permutation_sign(perm_order);
            result.data_ += data_.transpose(full_perm) * static_cast<double>(sign);
            ++count;
        } while (std::next_permutation(perm_order.begin(), perm_order.end()));

        result.data_ /= static_cast<double>(count);
        return result;
    }

public:
    IndexedTensor differentiate(const std::string& var) const { return { data_.differentiate(var), indices_ }; }
    IndexedTensor simplify() const { return { data_.simplify(), indices_ }; }
    IndexedTensor full_simplify() const { return { data_.full_simplify(), indices_ }; }
    IndexedTensor substitute(const std::string& var, double val) const { return { data_.substitute(var, val), indices_ }; }
    IndexedTensor substitute(const std::string& var, const cas::Expression& r) const { return { data_.substitute(var, r), indices_ }; }
    IndexedTensor substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { return { data_.substitute(pairs), indices_ }; }

public:
    std::string index_string() const {
        std::string up, down;

        for (auto& s : indices_) {
            if (s.variance == Variance::Up) {
                if (!up.empty()) up += " ";
                up += s.name;
            } else {
                if (!down.empty()) down += " ";
                down += s.name;
            }
        }

        std::string result;
        if (!up.empty())   result += "^{" + up + "}";
        if (!down.empty()) result += "_{" + down + "}";
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const IndexedTensor& t) {
        os << "IndexedTensor" << t.index_string() << " ";
        os << t.data_;
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }

private:
    void assert_same_indices(const IndexedTensor& o) const {
        if (indices_.size() != o.indices_.size()) throw std::invalid_argument("IndexedTensor: rank mismatch in arithmetic");

        for (std::size_t i = 0; i < indices_.size(); ++i) {
            if (indices_[i] != o.indices_[i]) {
                throw std::invalid_argument(
                    "IndexedTensor: index mismatch at axis " + std::to_string(i)
                    + " (" + indices_[i].name + " vs " + o.indices_[i].name + ")"
                );
            }
        }
    }

    std::string unique_name(const std::string& base) const {
        std::string candidate = "__" + base;
        int suffix = 0;
        while (has_index(candidate)) { candidate = "__" + base + std::to_string(suffix++); }
        return candidate;
    }

    static int permutation_sign(const std::vector<std::size_t>& perm) {
        std::size_t n = perm.size();
        std::vector<bool> visited(n, false);
        int sign = 1;

        for (std::size_t i = 0; i < n; ++i) {
            if (visited[i]) continue;
            std::size_t cycle_len = 0, j = i;
            while (!visited[j]) { visited[j] = true; j = perm[j]; ++cycle_len; }
            if (cycle_len % 2 == 0) sign = -sign;
        }

        return sign;
    }

private:
    SymbolicSquareTensor   data_;
    std::vector<IndexSlot> indices_;
};

inline IndexedTensor CONTRACT(const IndexedTensor& a, const IndexedTensor& b) { return einstein(a, b); }
inline IndexedTensor CONTRACT(const IndexedTensor& t, const std::string& name) { return t.contract_on(name); }
inline IndexedTensor RAISE(const IndexedTensor& t, const std::string& idx, const SymbolicSquareTensor& g_inv) { return t.raise(idx, g_inv); }
inline IndexedTensor LOWER(const IndexedTensor& t, const std::string& idx, const SymbolicSquareTensor& g) { return t.lower(idx, g); }
inline IndexedTensor TRACE(const IndexedTensor& t, const std::string& name) { return t.contract_on(name); }

inline cas::Expression FULL_TRACE(const IndexedTensor& t) {
    if (t.rank() != 2) throw std::domain_error("FULL_TRACE: requires rank-2 tensor");
    if (t.indices()[0].name != t.indices()[1].name) throw std::invalid_argument("FULL_TRACE: indices must share a name");
    auto contracted = t.contract_on(t.indices()[0].name);
    return contracted.flat_at(0);
}

class ChristoffelSymbolsFirst {
public:
    ChristoffelSymbolsFirst() = default;
    explicit ChristoffelSymbolsFirst(std::size_t dim) : data_(dim, 3) {}

    explicit ChristoffelSymbolsFirst(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 3) throw std::invalid_argument("ChristoffelSymbolsFirst: rank must be 3");
    }

    static ChristoffelSymbolsFirst zero(std::size_t dim) {
        return ChristoffelSymbolsFirst(SymbolicSquareTensor::zero(dim, 3));
    }

    std::size_t dim()  const noexcept { return data_.dim(); }
    std::size_t rank() const noexcept { return data_.rank(); }

    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_.at(idx); }

    SymbolicSquareTensor&       raw()       noexcept { return data_; }
    const SymbolicSquareTensor& raw() const noexcept { return data_; }

    IndexedTensor as_indexed(
        const std::string& a = "alpha",
        const std::string& b = "beta",
        const std::string& c = "gamma"
    ) const {
        return IndexedTensor(
            data_,
            { IndexSlot::down(a), IndexSlot::down(b), IndexSlot::down(c) }
        );
    }

private:
    SymbolicSquareTensor data_;
};

class ChristoffelSymbolsSecond {
public:
    ChristoffelSymbolsSecond() = default;
    explicit ChristoffelSymbolsSecond(std::size_t dim) : data_(dim, 3) {}

    explicit ChristoffelSymbolsSecond(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 3) throw std::invalid_argument("ChristoffelSymbolsSecond: rank must be 3");
    }

    static ChristoffelSymbolsSecond zero(std::size_t dim) {
        return ChristoffelSymbolsSecond(SymbolicSquareTensor::zero(dim, 3));
    }

    std::size_t dim()  const noexcept { return data_.dim();  }
    std::size_t rank() const noexcept { return data_.rank(); }

    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_.at(idx); }

    SymbolicSquareTensor&       raw()       noexcept { return data_; }
    const SymbolicSquareTensor& raw() const noexcept { return data_; }

    IndexedTensor as_indexed(
        const std::string& a = "alpha",
        const std::string& b = "beta",
        const std::string& c = "gamma"
    ) const {
        return IndexedTensor(
            data_,
            { IndexSlot::up(a), IndexSlot::down(b), IndexSlot::down(c) }
        );
    }

private:
    SymbolicSquareTensor data_;
};

class RiemannTensor {
public:
    RiemannTensor() = default;
    explicit RiemannTensor(std::size_t dim) : data_(dim, 4) {}

    explicit RiemannTensor(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 4) throw std::invalid_argument("RiemannTensor: rank must be 4");
    }

    static RiemannTensor zero(std::size_t dim) {
        return RiemannTensor(SymbolicSquareTensor::zero(dim, 4));
    }

    std::size_t dim()  const noexcept { return data_.dim();  }
    std::size_t rank() const noexcept { return data_.rank(); }

    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_.at(idx); }

    SymbolicSquareTensor&       raw()       noexcept { return data_; }
    const SymbolicSquareTensor& raw() const noexcept { return data_; }

    IndexedTensor as_indexed_up_down_down_down(
        const std::string& a = "rho",
        const std::string& b = "sigma",
        const std::string& c = "mu",
        const std::string& d = "nu"
    ) const {
        return IndexedTensor(
            data_,
            { IndexSlot::up(a), IndexSlot::down(b), IndexSlot::down(c), IndexSlot::down(d) }
        );
    }

    IndexedTensor as_indexed_down_down_down_down(
        const std::string& a = "rho",
        const std::string& b = "sigma",
        const std::string& c = "mu",
        const std::string& d = "nu"
    ) const {
        return IndexedTensor(
            data_,
            { IndexSlot::down(a), IndexSlot::down(b), IndexSlot::down(c), IndexSlot::down(d) }
        );
    }

private:
    SymbolicSquareTensor data_;
};

class RicciTensor {
public:
    RicciTensor() = default;
    explicit RicciTensor(std::size_t dim) : data_(dim, 2) {}

    explicit RicciTensor(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 2) throw std::invalid_argument("RicciTensor: rank must be 2");
    }

    static RicciTensor zero(std::size_t dim) {
        return RicciTensor(SymbolicSquareTensor::zero(dim, 2));
    }

    std::size_t dim()  const noexcept { return data_.dim();  }
    std::size_t rank() const noexcept { return data_.rank(); }

    cas::Expression&       at(const std::vector<std::size_t>& idx)       { return data_.at(idx); }
    const cas::Expression& at(const std::vector<std::size_t>& idx) const { return data_.at(idx); }

    SymbolicSquareTensor&       raw()       noexcept { return data_; }
    const SymbolicSquareTensor& raw() const noexcept { return data_; }

    IndexedTensor as_indexed_down_down(
        const std::string& a = "mu",
        const std::string& b = "nu"
    ) const {
        return IndexedTensor(
            data_,
            { IndexSlot::down(a), IndexSlot::down(b) }
        );
    }

private:
    SymbolicSquareTensor data_;
};

} // namespace cas
} // namespace math

template <> struct is_symbolic_tensor<math::tensors::ChristoffelSymbolsFirst>  : std::true_type {};
template <> struct is_symbolic_tensor<math::tensors::ChristoffelSymbolsSecond> : std::true_type {};
template <> struct is_symbolic_tensor<math::tensors::RicciTensor>              : std::true_type {};
template <> struct is_symbolic_tensor<math::tensors::RiemannTensor>            : std::true_type {};

} // namespace fizmo

#endif // FIZMO_INDEXED_TENSOR_HPP