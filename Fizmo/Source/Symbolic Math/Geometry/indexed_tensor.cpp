#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace tensors {

IndexedTensor::IndexedTensor(SymbolicSquareTensor data, std::vector<IndexSlot> indices) : data_(std::move(data)), indices_(std::move(indices)) {
        if (indices_.size() != data_.rank()) {
            throw std::invalid_argument(
                "IndexedTensor: slot count (" + std::to_string(indices_.size())
                + ") != tensor rank (" + std::to_string(data_.rank()) + ")"
            );
        }
    }

auto IndexedTensor::kronecker_delta(std::size_t dim, std::string up_name, std::string down_name) -> IndexedTensor {
        return { SymbolicSquareTensor::kronecker(dim, 2), { IndexSlot::up(std::move(up_name)), IndexSlot::down(std::move(down_name)) } };
    }

auto IndexedTensor::rename_index(const std::string& old_name, const std::string& new_name) const -> IndexedTensor {
        auto slots = indices_;
        for (auto& s : slots) if (s.name == old_name) { s.name = new_name; break; }
        return { data_, std::move(slots) };
    }

auto IndexedTensor::relabel(std::vector<IndexSlot> new_indices) const -> IndexedTensor {
        if (new_indices.size() != rank()) throw std::invalid_argument("IndexedTensor::relabel: slot count mismatch");
        return { data_, std::move(new_indices) };
    }

auto IndexedTensor::outer_product(const IndexedTensor& a, const IndexedTensor& b) -> IndexedTensor {
        auto combined = SymbolicSquareTensor::outer(a.data_, b.data_);
        std::vector<IndexSlot> slots;
        slots.reserve(a.indices_.size() + b.indices_.size());
        slots.insert(slots.end(), a.indices_.begin(), a.indices_.end());
        slots.insert(slots.end(), b.indices_.begin(), b.indices_.end());
        return { std::move(combined), std::move(slots) };
    }

auto IndexedTensor::contract_on(const std::string& name) const -> IndexedTensor {
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

auto IndexedTensor::raise(const std::string& idx_name, const SymbolicSquareTensor& metric_inv) const -> IndexedTensor {
        std::size_t ax = axis_of(idx_name);
        if (indices_[ax].variance != Variance::Down) throw std::invalid_argument("IndexedTensor::raise: index '" + idx_name + "' is already Up");
        std::string tmp = unique_name(idx_name);
        IndexedTensor g_inv(metric_inv, { IndexSlot::up(tmp), IndexSlot::up(idx_name) });
        auto result = einstein(*this, g_inv);
        return result.rename_index(tmp, idx_name);
    }

auto IndexedTensor::lower(const std::string& idx_name, const SymbolicSquareTensor& metric) const -> IndexedTensor {
        std::size_t ax = axis_of(idx_name);
        if (indices_[ax].variance != Variance::Up) throw std::invalid_argument("IndexedTensor::lower: index '" + idx_name + "' is already Down");
        std::string tmp = unique_name(idx_name);
        IndexedTensor g(metric, { IndexSlot::down(tmp), IndexSlot::down(idx_name) });
        auto result = einstein(*this, g);
        return result.rename_index(tmp, idx_name);
    }

auto IndexedTensor::swap_indices(const std::string& a, const std::string& b) const -> IndexedTensor {
        std::size_t ax_a = axis_of(a);
        std::size_t ax_b = axis_of(b);
        auto new_data = data_.swap_axes(ax_a, ax_b);
        auto new_slots = indices_;
        std::swap(new_slots[ax_a], new_slots[ax_b]);
        return { std::move(new_data), std::move(new_slots) };
    }

auto IndexedTensor::symmetrise_over(const std::vector<std::string>& names) const -> IndexedTensor {
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

auto IndexedTensor::antisymmetrise_over(const std::vector<std::string>& names) const -> IndexedTensor {
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

auto IndexedTensor::index_string() const -> std::string {
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

auto IndexedTensor::assert_same_indices(const IndexedTensor& o) const -> void {
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

auto IndexedTensor::unique_name(const std::string& base) const -> std::string {
        std::string candidate = "__" + base;
        int suffix = 0;
        while (has_index(candidate)) { candidate = "__" + base + std::to_string(suffix++); }
        return candidate;
    }

auto IndexedTensor::permutation_sign(const std::vector<std::size_t>& perm) -> int {
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

cas::Expression FULL_TRACE(const IndexedTensor& t) {
    if (t.rank() != 2) throw std::domain_error("FULL_TRACE: requires rank-2 tensor");
    if (t.indices()[0].name != t.indices()[1].name) throw std::invalid_argument("FULL_TRACE: indices must share a name");
    auto contracted = t.contract_on(t.indices()[0].name);
    return contracted.flat_at(0);
}

ChristoffelSymbolsFirst::ChristoffelSymbolsFirst(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 3) throw std::invalid_argument("ChristoffelSymbolsFirst: rank must be 3");
    }

ChristoffelSymbolsSecond::ChristoffelSymbolsSecond(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 3) throw std::invalid_argument("ChristoffelSymbolsSecond: rank must be 3");
    }

RiemannTensor::RiemannTensor(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 4) throw std::invalid_argument("RiemannTensor: rank must be 4");
    }

auto RiemannTensor::as_indexed_up_down_down_down(
        const std::string& a,
        const std::string& b,
        const std::string& c,
        const std::string& d 
) const -> IndexedTensor {
        return IndexedTensor(
            data_,
            { IndexSlot::up(a), IndexSlot::down(b), IndexSlot::down(c), IndexSlot::down(d) }
        );
    }

auto RiemannTensor::as_indexed_down_down_down_down(
        const std::string& a,
        const std::string& b,
        const std::string& c,
        const std::string& d 
) const -> IndexedTensor {
        return IndexedTensor(
            data_,
            { IndexSlot::down(a), IndexSlot::down(b), IndexSlot::down(c), IndexSlot::down(d) }
        );
    }

RicciTensor::RicciTensor(SymbolicSquareTensor t) : data_(std::move(t)) {
        if (data_.rank() != 2) throw std::invalid_argument("RicciTensor: rank must be 2");
    }

} // namespace tensors
} // namespace math
} // namespace fizmo
