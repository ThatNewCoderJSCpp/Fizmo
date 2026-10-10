#ifndef FIZMO_MATH_SETS_HPP
#define FIZMO_MATH_SETS_HPP

#include "relation.hpp"

namespace fizmo {
namespace math {
namespace logic {

namespace detail {

inline std::string expr_key(const cas::Expression& e) {
    std::ostringstream os;
    os << e;                 
    return os.str();
}

} // namespace detail

class FiniteSet {
public:
    FiniteSet() = default;
    explicit FiniteSet(std::vector<cas::Expression> elems) : elems_(std::move(elems)) {}

    const std::vector<cas::Expression>& elements() const noexcept { return elems_; }
    std::size_t size()  const noexcept { return elems_.size(); }
    bool        empty() const noexcept { return elems_.empty(); }

    FiniteSet canonical() const {
        std::vector<cas::Expression> out;
        std::vector<std::string> seen;

        for (const auto& e : elems_) {
            std::string k = detail::expr_key(e);

            if (std::find(seen.begin(), seen.end(), k) == seen.end()) {
                seen.push_back(std::move(k));
                out.push_back(e);
            }
        }

        return FiniteSet(std::move(out));
    }

    std::string to_string() const {
        std::string s = "{";

        for (std::size_t i = 0; i < elems_.size(); ++i) {
            if (i) s += ", ";
            std::ostringstream os; os << elems_[i];
            s += os.str();
        }

        return s + "}";
    }

    friend std::ostream& operator<<(std::ostream& os, const FiniteSet& fs) {
        return os << fs.to_string();
    }

private:
    std::vector<cas::Expression> elems_;
};

inline FiniteSet SET(std::vector<cas::Expression> elems) { return FiniteSet(std::move(elems)); }
inline FiniteSet EMPTY_SET() { return FiniteSet(); }

inline Predicate MEMBER(const cas::Expression& x, const FiniteSet& S) {
    const auto& es = S.elements();
    if (es.empty()) return Predicate(false);
    std::vector<Predicate> ors;
    ors.reserve(es.size());
    for (const auto& e : es) ors.push_back(Predicate(EQUALS(x, e)));
    if (ors.size() == 1) return std::move(ors[0]);
    return OR(std::move(ors));
}

inline Predicate NOT_MEMBER(const cas::Expression& x, const FiniteSet& S) { return NOT(MEMBER(x, S)); }

inline Predicate SUBSET(const FiniteSet& A, const FiniteSet& B) {
    const auto& es = A.elements();
    if (es.empty()) return Predicate(true);            
    std::vector<Predicate> ands;
    ands.reserve(es.size());
    for (const auto& a : es) ands.push_back(MEMBER(a, B));
    if (ands.size() == 1) return std::move(ands[0]);
    return AND(std::move(ands));
}

inline Predicate SUPERSET       (const FiniteSet& A, const FiniteSet& B) { return SUBSET(B, A); }
inline Predicate SET_EQUAL      (const FiniteSet& A, const FiniteSet& B) { return AND(SUBSET(A, B), SUBSET(B, A)); }
inline Predicate PROPER_SUBSET  (const FiniteSet& A, const FiniteSet& B) { return AND(SUBSET(A, B), NOT(SUBSET(B, A))); }
inline Predicate PROPER_SUPERSET(const FiniteSet& A, const FiniteSet& B) { return PROPER_SUBSET(B, A); }

inline Predicate DISJOINT(const FiniteSet& A, const FiniteSet& B) {
    const auto& es = A.elements();
    if (es.empty()) return Predicate(true);
    std::vector<Predicate> ands;
    ands.reserve(es.size());
    for (const auto& a : es) ands.push_back(NOT_MEMBER(a, B));
    if (ands.size() == 1) return std::move(ands[0]);
    return AND(std::move(ands));
}

inline FiniteSet SET_UNION(const FiniteSet& A, const FiniteSet& B) {
    std::vector<cas::Expression> all = A.elements();
    const auto& be = B.elements();
    all.insert(all.end(), be.begin(), be.end());
    return FiniteSet(std::move(all)).canonical();
}

inline FiniteSet SET_INTERSECTION(const FiniteSet& A, const FiniteSet& B) {
    std::vector<std::string> bkeys;
    for (const auto& b : B.elements()) bkeys.push_back(detail::expr_key(b));
    std::vector<cas::Expression> out;
    std::vector<std::string> seen;

    for (const auto& a : A.elements()) {
        std::string k = detail::expr_key(a);

        if (std::find(bkeys.begin(), bkeys.end(), k) != bkeys.end() &&
            std::find(seen.begin(),  seen.end(),  k) == seen.end()) {
            seen.push_back(k);
            out.push_back(a);
        }
    }

    return FiniteSet(std::move(out));
}

inline FiniteSet SET_DIFFERENCE(const FiniteSet& A, const FiniteSet& B) {
    std::vector<std::string> bkeys;
    for (const auto& b : B.elements()) bkeys.push_back(detail::expr_key(b));
    std::vector<cas::Expression> out;
    std::vector<std::string> seen;

    for (const auto& a : A.elements()) {
        std::string k = detail::expr_key(a);

        if (std::find(bkeys.begin(), bkeys.end(), k) == bkeys.end() && std::find(seen.begin(),  seen.end(),  k) == seen.end()) {
            seen.push_back(k);
            out.push_back(a);
        }
    }

    return FiniteSet(std::move(out));
}

inline FiniteSet SET_SYMDIFF(const FiniteSet& A, const FiniteSet& B) {
    return SET_UNION(SET_DIFFERENCE(A, B), SET_DIFFERENCE(B, A));
}

inline std::size_t CARDINALITY(const FiniteSet& S) { return S.canonical().size(); }

inline FiniteSet operator|(const FiniteSet& A, const FiniteSet& B) { return SET_UNION(A, B); }
inline FiniteSet operator&(const FiniteSet& A, const FiniteSet& B) { return SET_INTERSECTION(A, B); }
inline FiniteSet operator-(const FiniteSet& A, const FiniteSet& B) { return SET_DIFFERENCE(A, B); }
inline FiniteSet operator^(const FiniteSet& A, const FiniteSet& B) { return SET_SYMDIFF(A, B); }

enum class NumberSet : std::uint8_t {
    Naturals = 0,   
    Naturals0,      
    Integers,       
    Rationals,      
    Reals,          
    Primes,         
    Evens,          
    Odds            
};

inline const char* number_set_symbol(NumberSet s) noexcept {
    switch (s) {
        case NumberSet::Naturals:  return "\u2115";          
        case NumberSet::Naturals0: return "\u2115\u2080";    
        case NumberSet::Integers:  return "\u2124";          
        case NumberSet::Rationals: return "\u211A";          
        case NumberSet::Reals:     return "\u211D";         
        case NumberSet::Primes:    return "\u2119";          
        case NumberSet::Evens:     return "2\u2124";         
        case NumberSet::Odds:      return "2\u2124+1";       
    }
    return "?";
}

inline Predicate MEMBER(const cas::Expression& x, NumberSet s) {
    switch (s) {
        case NumberSet::Naturals:  return AND(IsInteger(x), IsPositive(x));
        case NumberSet::Naturals0: return AND(IsInteger(x), IsNonNegative(x));
        case NumberSet::Integers:  return IsInteger(x);
        case NumberSet::Rationals: return IsRational(x);
        case NumberSet::Reals:     return IsFinite(x);
        case NumberSet::Primes:    return IsPrime(x);
        case NumberSet::Evens:     return IsEven(x);
        case NumberSet::Odds:      return IsOdd(x);
    }
    return Predicate(false);
}

inline Predicate NOT_MEMBER(const cas::Expression& x, NumberSet s) { return NOT(MEMBER(x, s)); }

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SETS_HPP