#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace tensors {

SymbolicTensor::SymbolicTensor(std::vector<std::size_t> shape, std::vector<cas::Expression> data) : shape_(std::move(shape)), data_(std::move(data)) {
        if (data_.size() != total_size(shape_))
            throw std::invalid_argument("SymbolicTensor: data size ("
                + std::to_string(data_.size()) + ") != shape volume ("
                + std::to_string(total_size(shape_)) + ")");
    }

auto SymbolicTensor::zero(const std::vector<std::size_t>& shape) -> SymbolicTensor {
        std::size_t n = total_size(shape);
        std::vector<cas::Expression> d(n);
        for (auto& e : d) e = cas::Const(0.0);
        return { shape, std::move(d) };
    }

auto SymbolicTensor::constant(const std::vector<std::size_t>& shape, double val) -> SymbolicTensor {
        std::size_t n = total_size(shape);
        std::vector<cas::Expression> d(n);
        for (auto& e : d) e = cas::Const(val);
        return { shape, std::move(d) };
    }

auto SymbolicTensor::kronecker(const std::vector<std::size_t>& shape) -> SymbolicTensor {
        auto t = zero(shape);
        std::size_t rank = shape.size();
        if (rank == 0) return t;
        std::size_t dim = shape[0];

        for (std::size_t i = 0; i < dim; ++i) {
            std::vector<std::size_t> idx(rank, i);
            bool fits = true;
            for (std::size_t r = 1; r < rank; ++r) if (i >= shape[r]) { fits = false; break; }
            if (fits) t.at(idx) = cas::Const(1.0);
        }

        return t;
    }

auto SymbolicTensor::outer(const SymbolicTensor& a, const SymbolicTensor& b) -> SymbolicTensor {
        std::vector<std::size_t> sh;
        sh.reserve(a.rank() + b.rank());
        sh.insert(sh.end(), a.shape_.begin(), a.shape_.end());
        sh.insert(sh.end(), b.shape_.begin(), b.shape_.end());
        std::size_t na = a.size(), nb = b.size();
        std::vector<cas::Expression> d;
        d.reserve(na * nb);

        for (std::size_t i = 0; i < na; ++i)
            for (std::size_t j = 0; j < nb; ++j)
                d.push_back(a.data_[i] * b.data_[j]);

        return { std::move(sh), std::move(d) };
    }

auto SymbolicTensor::operator+(const SymbolicTensor& o) const -> SymbolicTensor {
        assert_same_shape(o);
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] + o.data_[i];
        return r;
    }

auto SymbolicTensor::operator-(const SymbolicTensor& o) const -> SymbolicTensor {
        assert_same_shape(o);
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] - o.data_[i];
        return r;
    }

auto SymbolicTensor::hadamard(const SymbolicTensor& o) const -> SymbolicTensor {
        assert_same_shape(o);
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = data_[i] * o.data_[i];
        return r;
    }

auto SymbolicTensor::contract(std::size_t axis_a, std::size_t axis_b) const -> SymbolicTensor {
        if (axis_a == axis_b) throw std::invalid_argument("SymbolicTensor::contract: axes must differ");
        if (axis_a >= rank() || axis_b >= rank()) throw std::out_of_range("SymbolicTensor::contract: axis out of range");
        if (shape_[axis_a] != shape_[axis_b]) throw std::invalid_argument("SymbolicTensor::contract: dimension mismatch on axes (" + std::to_string(shape_[axis_a]) + " vs " + std::to_string(shape_[axis_b]) + ")");
        std::size_t contract_dim = shape_[axis_a];
        std::vector<std::size_t> rshape;
        rshape.reserve(rank() - 2);
        for (std::size_t a = 0; a < rank(); ++a) if (a != axis_a && a != axis_b) rshape.push_back(shape_[a]);
        if (rshape.empty()) { rshape.push_back(1); }
        SymbolicTensor result = SymbolicTensor::zero(rshape);
        std::vector<std::size_t> idx(rank(), 0);

        for (std::size_t fi = 0; fi < size(); ++fi) {
            unflatten(fi, idx);
            if (idx[axis_a] != idx[axis_b]) continue; 
            std::vector<std::size_t> ridx;
            ridx.reserve(rshape.size());
            for (std::size_t a = 0; a < rank(); ++a)
            if (a != axis_a && a != axis_b) ridx.push_back(idx[a]); if (ridx.empty()) ridx.push_back(0);
            result.at(ridx) = result.at(ridx) + data_[fi];
        }

        return result;
    }

auto SymbolicTensor::contract(const SymbolicTensor& a, std::size_t a_axis, const SymbolicTensor& b, std::size_t b_axis) -> SymbolicTensor {
        if (a.shape_[a_axis] != b.shape_[b_axis]) throw std::invalid_argument("SymbolicTensor::contract: contraction dimension mismatch");
        std::size_t contract_dim = a.shape_[a_axis];
        std::vector<std::size_t> rshape;
        rshape.reserve(a.rank() + b.rank() - 2);
        for (std::size_t i = 0; i < a.rank(); ++i) if (i != a_axis) rshape.push_back(a.shape_[i]);
        for (std::size_t i = 0; i < b.rank(); ++i) if (i != b_axis) rshape.push_back(b.shape_[i]);
        if (rshape.empty()) rshape.push_back(1);
        SymbolicTensor result = SymbolicTensor::zero(rshape);
        auto a_strides = compute_strides(a.shape_);
        auto b_strides = compute_strides(b.shape_);
        auto r_strides = compute_strides(rshape);
        std::vector<std::size_t> a_idx(a.rank(), 0);
        std::vector<std::size_t> b_idx(b.rank(), 0);
        std::size_t a_free = a.size() / contract_dim;
        std::size_t b_free = b.size() / contract_dim;

        for (std::size_t ai = 0; ai < a.size(); ++ai) {
            a.unflatten(ai, a_idx);
            for (std::size_t k = 0; k < contract_dim; ++k) {
                if (a_idx[a_axis] != k) continue;
                for (std::size_t bi = 0; bi < b.size(); ++bi) {
                    b.unflatten(bi, b_idx);
                    if (b_idx[b_axis] != k) continue;
                    std::vector<std::size_t> ridx;
                    ridx.reserve(rshape.size());
                    for (std::size_t i = 0; i < a.rank(); ++i) if (i != a_axis) ridx.push_back(a_idx[i]);
                    for (std::size_t i = 0; i < b.rank(); ++i) if (i != b_axis) ridx.push_back(b_idx[i]);
                    if (ridx.empty()) ridx.push_back(0);
                    std::size_t ri = 0;
                    for (std::size_t d = 0; d < rshape.size(); ++d) ri += ridx[d] * r_strides[d];
                    result.data_[ri] = result.data_[ri] + a.data_[ai] * b.data_[bi];
                }
            }
        }

        return result;
    }

auto SymbolicTensor::transpose(const std::vector<std::size_t>& perm) const -> SymbolicTensor {
        if (perm.size() != rank()) throw std::invalid_argument("SymbolicTensor::transpose: permutation size mismatch");
        std::vector<std::size_t> rshape(rank());
        for (std::size_t i = 0; i < rank(); ++i) rshape[i] = shape_[perm[i]];
        SymbolicTensor result(rshape);
        auto r_strides = compute_strides(rshape);
        std::vector<std::size_t> idx(rank());

        for (std::size_t fi = 0; fi < size(); ++fi) {
            unflatten(fi, idx);
            std::size_t ri = 0;
            for (std::size_t d = 0; d < rank(); ++d) ri += idx[perm[d]] * r_strides[d];
            result.data_[ri] = data_[fi];
        }

        return result;
    }

auto SymbolicTensor::swap_axes(std::size_t a, std::size_t b) const -> SymbolicTensor {
        std::vector<std::size_t> perm(rank());
        std::iota(perm.begin(), perm.end(), 0);
        std::swap(perm[a], perm[b]);
        return transpose(perm);
    }

auto SymbolicTensor::slice(std::size_t axis, std::size_t index) const -> SymbolicTensor {
        if (axis >= rank()) throw std::out_of_range("SymbolicTensor::slice: axis out of range");
        if (index >= shape_[axis]) throw std::out_of_range("SymbolicTensor::slice: index out of range");
        std::vector<std::size_t> rshape;
        rshape.reserve(rank() - 1);
        for (std::size_t a = 0; a < rank(); ++a) if (a != axis) rshape.push_back(shape_[a]);
        if (rshape.empty()) rshape.push_back(1);
        SymbolicTensor result(rshape);
        std::vector<std::size_t> idx(rank(), 0);
        std::size_t ri = 0;

        for (std::size_t fi = 0; fi < size(); ++fi) {
            unflatten(fi, idx);
            if (idx[axis] != index) continue;
            result.data_[ri++] = data_[fi];
        }

        return result;
    }

auto SymbolicTensor::reshape(const std::vector<std::size_t>& new_shape) const -> SymbolicTensor {
        if (total_size(new_shape) != size()) throw std::invalid_argument("SymbolicTensor::reshape: total size mismatch");
        return { new_shape, data_ };
    }

auto SymbolicTensor::frobenius_norm_squared() const -> cas::Expression {
        cas::Expression s = data_[0] * data_[0];
        for (std::size_t i = 1; i < size(); ++i) s = s + data_[i] * data_[i];
        return s;
    }

auto SymbolicTensor::differentiate(const std::string& var) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = DIFFERENTIATE(data_[i], var);
        return r;
    }

auto SymbolicTensor::differentiate_iterative(const std::string& var, unsigned n) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = DIFFERENTIATE_ITERATIVE(data_[i], var, n);
        return r;
    }

auto SymbolicTensor::differentiate_recursive(const std::string& var, unsigned n) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = DIFFERENTIATE_RECURSIVE(data_[i], var, n);
        return r;
    }

auto SymbolicTensor::substitute(const std::string& var, double val) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, val);
        return r;
    }

auto SymbolicTensor::substitute(const std::string& var, const cas::Expression& repl) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = SUBSTITUTE(data_[i], var, repl);
        return r;
    }

auto SymbolicTensor::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicTensor {
        SymbolicTensor r(shape_, data_);
        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < size(); ++i)
                r.data_[i] = SUBSTITUTE(r.data_[i], nm, v);

        return r;
    }

auto SymbolicTensor::partial_evaluate(const std::string& var, double val) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, val);
        return r;
    }

auto SymbolicTensor::partial_evaluate(const std::string& var, const cas::Expression& repl) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = PARTIAL_EVALUATE(data_[i], var, repl);
        return r;
    }

auto SymbolicTensor::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> SymbolicTensor {
        SymbolicTensor r(shape_, data_);

        for (auto& [nm, v] : pairs)
            for (std::size_t i = 0; i < size(); ++i)
                r.data_[i] = PARTIAL_EVALUATE(r.data_[i], nm, v);

        return r;
    }

auto SymbolicTensor::simplify() const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = SIMPLIFY(data_[i]);
        return r;
    }

auto SymbolicTensor::full_simplify(const cas::RewriterConfig& cfg) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = FULL_SIMPLIFY(data_[i], cfg);
        return r;
    }

auto SymbolicTensor::rewrite(const cas::RewriterConfig& cfg) const -> SymbolicTensor {
        SymbolicTensor r(shape_);
        for (std::size_t i = 0; i < size(); ++i) r.data_[i] = REWRITE(data_[i], cfg);
        return r;
    }

auto SymbolicTensor::evaluate(const std::unordered_map<std::string, double>& vals) const -> std::vector<double> {
        std::vector<double> out(size());
        for (std::size_t i = 0; i < size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

auto SymbolicTensor::evaluate(std::initializer_list<std::pair<std::string, double>> vals) const -> std::vector<double> {
        std::vector<double> out(size());
        for (std::size_t i = 0; i < size(); ++i) out[i] = data_[i].evaluate(vals);
        return out;
    }

auto SymbolicTensor::from(const matrices::SymbolicMatrix3x3& m) -> SymbolicTensor {
        return {
            {3, 3},
            {
                m.at(0,0), m.at(0,1), m.at(0,2),
                m.at(1,0), m.at(1,1), m.at(1,2),
                m.at(2,0), m.at(2,1), m.at(2,2)
            }
        };
    }

auto SymbolicTensor::from(const matrices::SymbolicMatrix4x4& m) -> SymbolicTensor {
        return {
            {4, 4},
            {
                m.at(0,0), m.at(0,1), m.at(0,2), m.at(0,3),
                m.at(1,0), m.at(1,1), m.at(1,2), m.at(1,3),
                m.at(2,0), m.at(2,1), m.at(2,2), m.at(2,3),
                m.at(3,0), m.at(3,1), m.at(3,2), m.at(3,3)
            }
        };
    }

auto SymbolicTensor::from(const matrices::SymbolicMatrixNM& m) -> SymbolicTensor {
        std::vector<cas::Expression> d;
        d.reserve(m.num_rows() * m.num_cols());

        for (std::size_t r = 0; r < m.num_rows(); ++r)
            for (std::size_t c = 0; c < m.num_cols(); ++c)
                d.push_back(m.get_data()[r][c]);

        return { { m.num_rows(), m.num_cols() }, std::move(d) };
    }

auto SymbolicTensor::from(const matrices::SymbolicMatrixN& m) -> SymbolicTensor {
        std::size_t n = m.dimension();
        std::vector<cas::Expression> d;
        d.reserve(n * n);

        for (std::size_t r = 0; r < n; ++r)
            for (std::size_t c = 0; c < n; ++c)
                d.push_back(m.get_data()[r][c]);

        return { { n, n }, std::move(d) };
    }

auto SymbolicTensor::to_matrix() const -> matrices::SymbolicMatrixNM {
        if (rank() != 2) throw std::domain_error("SymbolicTensor::to_matrix requires rank 2");
        matrices::SymbolicMatrixNM m(shape_[0], shape_[1]);

        for (std::size_t r = 0; r < shape_[0]; ++r)
            for (std::size_t c = 0; c < shape_[1]; ++c)
                m.at(r, c) = data_[r * shape_[1] + c];

        return m;
    }

auto SymbolicTensor::to_vector() const -> vectors::SymbolicVectorN {
        if (rank() != 1) throw std::domain_error("SymbolicTensor::to_vector requires rank 1");
        return vectors::SymbolicVectorN(data_);
    }

auto SymbolicTensor::to_value() const -> cas::Expression {
        if (rank() != 0) throw std::domain_error("SymbolicTensor::to_value requires rank 0");
        return data_[0];
    }

auto SymbolicTensor::compute_strides(const std::vector<std::size_t>& shape) -> std::vector<std::size_t> {
        std::vector<std::size_t> strides(shape.size());
        if (shape.empty()) return strides;
        strides.back() = 1;
        for (std::size_t i = shape.size() - 1; i > 0; --i) strides[i - 1] = strides[i] * shape[i];
        return strides;
    }

auto SymbolicTensor::flat(const std::vector<std::size_t>& idx) const -> std::size_t {
        if (idx.size() != rank()) throw std::invalid_argument("SymbolicTensor: index rank mismatch");
        std::size_t fi = 0, stride = 1;

        for (std::size_t i = rank(); i-- > 0;) {
            if (idx[i] >= shape_[i])
                throw std::out_of_range("SymbolicTensor: index out of range on axis " + std::to_string(i));

            fi += idx[i] * stride;
            stride *= shape_[i];
        }
        return fi;
    }

auto SymbolicTensor::assert_same_shape(const SymbolicTensor& o) const -> void {
        if (shape_ != o.shape_) {
            std::string a, b;
            for (auto d : shape_)   { if (!a.empty()) a += "x"; a += std::to_string(d); }
            for (auto d : o.shape_) { if (!b.empty()) b += "x"; b += std::to_string(d); }
            throw std::invalid_argument("SymbolicTensor: shape mismatch (" + a + " vs " + b + ")");
        }
    }

auto SymbolicTensor::print_recursive(std::ostream& os, std::vector<std::size_t> prefix, std::size_t depth, std::size_t indent) const -> void {
        std::string pad(indent * 2, ' ');

        if (depth == rank()) {
            os << pad << data_[flat(prefix)].full_simplify();
            return;
        }

        for (std::size_t i = 0; i < shape_[depth]; ++i) {
            prefix.push_back(i);
            if (depth == rank() - 1) {
                if (i == 0) os << pad;
                os << data_[flat(prefix)].full_simplify();
                if (i + 1 < shape_[depth]) os << ",  ";
                else os << "\n";
            } else {
                if (i == 0) os << pad << "[\n";
                print_recursive(os, prefix, depth + 1, indent + 1);
                if (i + 1 == shape_[depth]) os << pad << "]\n";
            }
            prefix.pop_back();
        }
    }

auto SymbolicSquareTensor::kronecker(std::size_t dim, std::size_t rank) -> SymbolicSquareTensor {
        auto t = zero(dim, rank);

        for (std::size_t i = 0; i < dim; ++i) {
            std::vector<std::size_t> idx(rank, i);
            t.at(idx) = cas::Const(1.0);
        }

        return t;
    }

auto SymbolicSquareTensor::levi_civita(std::size_t n) -> SymbolicSquareTensor {
        auto t = zero(n, n);
        std::vector<std::size_t> perm(n);
        std::iota(perm.begin(), perm.end(), 0);

        do {
            int sign = permutation_sign(perm);
            t.at(perm) = cas::Const(static_cast<double>(sign));
        } while (std::next_permutation(perm.begin(), perm.end()));

        return t;
    }

auto SymbolicSquareTensor::outer(const SymbolicSquareTensor& a, const SymbolicSquareTensor& b) -> SymbolicSquareTensor {
        if (a.dim_ != b.dim_) throw std::invalid_argument("SymbolicSquareTensor::outer: dimension mismatch");
        auto t_inner = SymbolicTensor::outer(a.inner_, b.inner_);
        return { a.dim_, a.rank_ + b.rank_, std::move(t_inner.data()) };
    }

auto SymbolicSquareTensor::contract(std::size_t axis_a, std::size_t axis_b) const -> SymbolicSquareTensor {
        auto result = inner_.contract(axis_a, axis_b);
        std::size_t new_rank = rank_ - 2;
        if (new_rank == 0) new_rank = 1;  
        return { dim_, new_rank, std::move(result.data()) };
    }

auto SymbolicSquareTensor::trace() const -> SymbolicSquareTensor {
        if (rank_ < 2) throw std::domain_error("SymbolicSquareTensor::trace requires rank >= 2");
        return contract(0, 1);
    }

auto SymbolicSquareTensor::symmetrise() const -> SymbolicSquareTensor {
        auto result = SymbolicSquareTensor::zero(dim_, rank_);
        std::vector<std::size_t> perm(rank_);
        std::iota(perm.begin(), perm.end(), 0);
        std::size_t count = 0;

        do {
            auto permuted = transpose(perm);
            result += permuted;
            ++count;
        } while (std::next_permutation(perm.begin(), perm.end()));

        return result / static_cast<double>(count);
    }

auto SymbolicSquareTensor::antisymmetrise() const -> SymbolicSquareTensor {
        auto result = SymbolicSquareTensor::zero(dim_, rank_);
        std::vector<std::size_t> perm(rank_);
        std::iota(perm.begin(), perm.end(), 0);
        std::size_t count = 0;

        do {
            int sign = permutation_sign(perm);
            auto permuted = transpose(perm);
            result += permuted * static_cast<double>(sign);
            ++count;
        } while (std::next_permutation(perm.begin(), perm.end()));

        return result / static_cast<double>(count);
    }

auto SymbolicSquareTensor::slice(std::size_t axis, std::size_t index) const -> SymbolicSquareTensor {
        auto sliced = inner_.slice(axis, index);
        std::size_t new_rank = (rank_ > 1) ? rank_ - 1 : 1;
        return { dim_, new_rank, std::move(sliced.data()) };
    }

auto SymbolicSquareTensor::from(const vectors::SymbolicVector2& v) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(2);
        data.push_back(v.x);
        data.push_back(v.y);
        return { 2, 1, std::move(data) };
    }

auto SymbolicSquareTensor::from(const vectors::SymbolicVector3& v) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(3);
        data.push_back(v.x);
        data.push_back(v.y);
        data.push_back(v.z);
        return { 3, 1, std::move(data) };
    }

auto SymbolicSquareTensor::from(const vectors::SymbolicVector4& v) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(4);
        data.push_back(v.x);
        data.push_back(v.y);
        data.push_back(v.z);
        data.push_back(v.w);
        return { 4, 1, std::move(data) };
    }

auto SymbolicSquareTensor::from(const matrices::SymbolicMatrix2x2& m) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(4);

        for (std::size_t r = 0; r < 2; ++r)
            for (std::size_t c = 0; c < 2; ++c)
                data.push_back(m.at(r, c));

        return { 2, 2, std::move(data) };
    }

auto SymbolicSquareTensor::from(const matrices::SymbolicMatrix3x3& m) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(9);

        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t c = 0; c < 3; ++c)
                data.push_back(m.at(r, c));

        return { 3, 2, std::move(data) };
    }

auto SymbolicSquareTensor::from(const matrices::SymbolicMatrix4x4& m) -> SymbolicSquareTensor {
        std::vector<cas::Expression> data;
        data.reserve(16);

        for (std::size_t r = 0; r < 4; ++r)
            for (std::size_t c = 0; c < 4; ++c)
                data.push_back(m.at(r, c));

        return { 4, 2, std::move(data) };
    }

auto SymbolicSquareTensor::to_matrix() const -> matrices::SymbolicMatrixN {
        if (rank_ != 2) throw std::domain_error("SymbolicSquareTensor::to_matrix requires rank 2");
        matrices::SymbolicMatrixN m(dim_);

        for (std::size_t r = 0; r < dim_; ++r)
            for (std::size_t c = 0; c < dim_; ++c)
                m.at(r, c) = inner_.flat_at(r * dim_ + c);
        return m;
    }

auto SymbolicSquareTensor::to_vector() const -> vectors::SymbolicVectorN {
        if (rank_ != 1) throw std::domain_error("SymbolicSquareTensor::to_vector requires rank 1");
        return vectors::SymbolicVectorN(std::vector<cas::Expression>(inner_.data()));
    }

auto SymbolicSquareTensor::assert_same(const SymbolicSquareTensor& o) const -> void {
        if (dim_ != o.dim_ || rank_ != o.rank_)
            throw std::invalid_argument("SymbolicSquareTensor: mismatch (dim="
                + std::to_string(dim_) + ",rank=" + std::to_string(rank_)
                + " vs dim=" + std::to_string(o.dim_) + ",rank=" + std::to_string(o.rank_) + ")");
    }

auto SymbolicSquareTensor::permutation_sign(const std::vector<std::size_t>& perm) -> int {
        std::size_t n = perm.size();
        std::vector<bool> visited(n, false);
        int sign = 1;
        for (std::size_t i = 0; i < n; ++i) {
            if (visited[i]) continue;
            std::size_t cycle_len = 0;
            std::size_t j = i;
            while (!visited[j]) {
                visited[j] = true;
                j = perm[j];
                ++cycle_len;
            }
            if (cycle_len % 2 == 0) sign = -sign;
        }
        return sign;
    }

} // namespace tensors
} // namespace math
} // namespace fizmo
