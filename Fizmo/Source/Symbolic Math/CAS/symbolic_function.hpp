#ifndef FIZMO_MATH_SYMBOLIC_FUNCTION_HPP
#define FIZMO_MATH_SYMBOLIC_FUNCTION_HPP

#include "../Common/main_convenience.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <utility>

namespace fizmo {
namespace math {
namespace cas {

class SymbolicFunction {
public:
    SymbolicFunction(std::string name, std::vector<std::string> params) : name_(std::move(name)), params_(std::move(params)) {}
    const std::string& name() const noexcept { return name_; }
    std::size_t arity() const noexcept { return params_.size(); }
    const std::vector<std::string>& parameters() const noexcept { return params_; }

    Expression operator()(SymbolicContext& ctx) const {
        std::vector<Expression> args;
        args.reserve(params_.size());
        for (const auto& p : params_) args.push_back(VARIABLE(ctx, p));
        return build(ctx, args, std::vector<std::uint64_t>(params_.size(), 0));
    }

    template <typename... Es, typename = typename std::enable_if<(std::is_convertible<Es, Expression>::value && ...)>::type>
    Expression operator()(SymbolicContext& ctx, Es&&... es) const {
        std::vector<Expression> args{ Expression(std::forward<Es>(es))... };
        return build(ctx, args, std::vector<std::uint64_t>(args.size(), 0));
    }

    Expression operator()(SymbolicContext& ctx, const std::vector<Expression>& args) const {
        return build(ctx, args, std::vector<std::uint64_t>(args.size(), 0));
    }

    Expression operator()(const std::vector<Expression>& args) const {
        return (*this)(global_context(), args);
    }

    Expression partial(SymbolicContext& ctx, const std::string& wrt) const {
        return partial(ctx, { { wrt, std::uint64_t{1} } });
    }
    
    Expression partial(const std::string& wrt) const {
        return partial(global_context(), wrt);
    }

    Expression partial(SymbolicContext& ctx, std::initializer_list<std::pair<std::string, std::uint64_t>> wrt) const {
        std::vector<std::uint64_t> orders(params_.size(), 0);

        for (const auto& [pname, count] : wrt) {
            auto it = std::find(params_.begin(), params_.end(), pname);
            assert(it != params_.end() && "differentiation variable is not a parameter of this function");
            if (it != params_.end()) orders[static_cast<std::size_t>(it - params_.begin())] += count;
        }

        return build(ctx, declared_args(ctx), orders);
    }

    Expression partial(std::initializer_list<std::pair<std::string, std::uint64_t>> wrt) const {
        return partial(global_context(), wrt);
    }

    Expression partial(SymbolicContext& ctx, const std::vector<Expression>& args, const std::vector<std::uint64_t>& orders) const {
        return build(ctx, args, orders);
    }

    Expression partial(const std::vector<Expression>& args, const std::vector<std::uint64_t>& orders) const {
        return build(global_context(), args, orders);
    }

    Expression partial(SymbolicContext& ctx, const std::vector<Expression>& args, std::uint64_t slot, std::uint64_t order = 1) const {
        std::vector<std::uint64_t> o(args.size(), 0);
        if (slot < o.size()) o[slot] += order;
        return build(ctx, args, o);
    }

    Expression partial(const std::vector<Expression>& args, std::uint64_t slot, std::uint64_t order = 1) const {
        return partial(global_context(), args, slot, order);
    }

    bool operator==(const SymbolicFunction& o) const noexcept { return name_ == o.name_; }
    bool operator!=(const SymbolicFunction& o) const noexcept { return !(*this == o); }

private:
    std::vector<Expression> declared_args(SymbolicContext& ctx) const {
        std::vector<Expression> args;
        args.reserve(params_.size());
        for (const auto& p : params_) args.push_back(VARIABLE(ctx, p));
        return args;
    }

    std::vector<std::uint64_t> zero_orders() const {
        return std::vector<std::uint64_t>(params_.size(), 0);
    }

    Expression build(SymbolicContext& ctx, const std::vector<Expression>& args, std::vector<std::uint64_t> orders) const {
        orders.resize(args.size(), 0);  
        std::vector<symbols::MathExpression> inner;
        inner.reserve(args.size());
        for (const auto& a : args) inner.push_back(a.inner());
        const std::uint64_t fid = ctx.manager().function_id(name_);
        return Expression(ctx.manager().applied(fid, inner, orders), ctx.manager());
    }

    std::string name_;
    std::vector<std::string> params_;
};

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SYMBOLIC_FUNCTION_HPP