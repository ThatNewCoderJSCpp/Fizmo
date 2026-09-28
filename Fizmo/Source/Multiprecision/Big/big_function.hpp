#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_FUNCTION_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_FUNCTION_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "big_float.hpp"

namespace fizmo {
namespace multiprecision {

class BigFloatFunction;

namespace bffdetail {

using fp1_t = BigFloat (*)(const BigFloat&);
using fp2_t = BigFloat (*)(const BigFloat&, const BigFloatContext&);

template <typename...> struct bff_void { using type = void; };

template <typename F, typename = void>
struct bff_ctx_aware : std::false_type {};

template <typename F>
struct bff_ctx_aware<F, typename bff_void<decltype(std::declval<F&>()(std::declval<const BigFloat&>(), std::declval<const BigFloatContext&>()))>::type>
    : std::integral_constant<bool, std::is_constructible<BigFloat,
        decltype(std::declval<F&>()(std::declval<const BigFloat&>(), std::declval<const BigFloatContext&>()))>::value> {};

template <typename F, typename = void>
struct bff_plain : std::false_type {};

template <typename F>
struct bff_plain<F, typename bff_void<decltype(std::declval<F&>()(std::declval<const BigFloat&>()))>::type>
    : std::integral_constant<bool, std::is_constructible<BigFloat, decltype(std::declval<F&>()(std::declval<const BigFloat&>()))>::value> {};

template <typename F>
struct bff_callable : std::integral_constant<bool, bff_ctx_aware<F>::value || bff_plain<F>::value> {};

template <typename T> struct bff_is_std_function : std::false_type {};
template <typename S> struct bff_is_std_function<std::function<S>> : std::true_type {};

template <typename D>
struct bff_kind : std::integral_constant<int, std::is_same<D, fp2_t>::value ? 2 : (std::is_same<D, fp1_t>::value ? 1 : 0)> {};

inline bool bff_same_as_current(const BigFloatContext& c) noexcept {
    const BigFloatContext& cur = BigFloatContext::current();
    return cur.precision == c.precision && cur.rounding_mode == c.rounding_mode;
}

template <typename F>
inline BigFloat bff_call(F& f, const BigFloat& x, const BigFloatContext* ctx, std::true_type) {
    return BigFloat(f(x, ctx ? *ctx : BigFloatContext::current()));
}

template <typename F>
inline BigFloat bff_call(F& f, const BigFloat& x, const BigFloatContext* ctx, std::false_type) {
    if (ctx == nullptr || bff_same_as_current(*ctx)) return BigFloat(f(x));
    ScopedContext scope(*ctx);
    return BigFloat(f(x));
}

struct bff_ops {
    BigFloat              (*invoke)(void*, const BigFloat&, const BigFloatContext*);
    void*                 (*clone)(const void* src, void* buf);    
    void                  (*relocate)(void* src, void* dst);       
    void                  (*destroy)(void* obj);
    const std::type_info& (*type)();
    bool                  ctx_aware;
};

template <typename D, bool Inline>
struct bff_model {
    static BigFloat invoke(void* obj, const BigFloat& x, const BigFloatContext* ctx) {
        return bff_call(*static_cast<D*>(obj), x, ctx, bff_ctx_aware<D>());
    }

    static void* clone(const void* src, void* buf) {
        const D& s = *static_cast<const D*>(src);
        if (Inline) { ::new (buf) D(s); return nullptr; }
        return new D(s);
    }

    static void relocate(void* src, void* dst) noexcept {
        D* s = static_cast<D*>(src);
        ::new (dst) D(std::move(*s));
        s->~D();
    }

    static void destroy(void* obj) noexcept {
        D* p = static_cast<D*>(obj);
        if (Inline) p->~D(); else delete p;
    }

    static const std::type_info& type() noexcept { return typeid(D); }

    static const bff_ops table;
};

template <typename D, bool Inline>
const bff_ops bff_model<D, Inline>::table = {
    &bff_model::invoke, &bff_model::clone, &bff_model::relocate, &bff_model::destroy, &bff_model::type, bff_ctx_aware<D>::value
};

} // namespace bffdetail

class BigFloatFunction {
public:
    static constexpr std::size_t buffer_size  = (2 * sizeof(BigFloat) > 48) ? 2 * sizeof(BigFloat) : 48;
    static constexpr std::size_t buffer_align = alignof(std::max_align_t);

private:
    using invoke_fn = BigFloat (*)(void*, const BigFloat&, const BigFloatContext*);

    alignas(std::max_align_t) unsigned char m_buf[buffer_size];
    const bffdetail::bff_ops* m_ops    = nullptr;
    invoke_fn                 m_invoke = nullptr;                 
    void*                     m_heap   = nullptr;
    bffdetail::fp1_t          m_fp1    = nullptr;
    bffdetail::fp2_t          m_fp2    = nullptr;
    const char*               m_name   = nullptr;
    mutable std::uint64_t     m_calls  = 0;

    void* obj() const noexcept { return m_heap ? m_heap : static_cast<void*>(const_cast<unsigned char*>(m_buf)); }

    template <typename D, typename F>
    void store(F&& f, std::integral_constant<int, 1>) { m_fp1 = f; }

    template <typename D, typename F>
    void store(F&& f, std::integral_constant<int, 2>) { m_fp2 = f; }

    template <typename D, typename F>
    void store(F&& f, std::integral_constant<int, 0>) {
        constexpr bool fits = sizeof(D) <= buffer_size && alignof(D) <= buffer_align && std::is_nothrow_move_constructible<D>::value;
        store_object<D>(std::forward<F>(f), std::integral_constant<bool, fits>());
    }

    template <typename D, typename F>
    void store_object(F&& f, std::true_type) {
        ::new (static_cast<void*>(m_buf)) D(std::forward<F>(f));
        m_ops    = &bffdetail::bff_model<D, true>::table;
        m_invoke = m_ops->invoke;
    }

    template <typename D, typename F>
    void store_object(F&& f, std::false_type) {
        m_heap   = new D(std::forward<F>(f));
        m_ops    = &bffdetail::bff_model<D, false>::table;
        m_invoke = m_ops->invoke;
    }

    void clear() noexcept {
        if (m_ops) m_ops->destroy(obj());
        m_ops    = nullptr;
        m_invoke = nullptr;
        m_heap   = nullptr;
        m_fp1    = nullptr;
        m_fp2    = nullptr;
    }

    void move_from(BigFloatFunction& o) noexcept {
        m_ops    = o.m_ops;
        m_invoke = o.m_invoke;
        m_fp1    = o.m_fp1;
        m_fp2    = o.m_fp2;
        m_name   = o.m_name;
        m_calls  = o.m_calls;
        m_heap   = nullptr;

        if (m_ops) {
            if (o.m_heap) m_heap = o.m_heap;                                     
            else          m_ops->relocate(o.m_buf, m_buf);                       
        }

        o.m_ops    = nullptr;
        o.m_invoke = nullptr;
        o.m_heap   = nullptr;
        o.m_fp1    = nullptr;
        o.m_fp2    = nullptr;
        o.m_calls  = 0;
    }

    template <typename T> const T* target_impl(std::integral_constant<int, 1>) const noexcept { return m_fp1 ? &m_fp1 : nullptr; }
    template <typename T> const T* target_impl(std::integral_constant<int, 2>) const noexcept { return m_fp2 ? &m_fp2 : nullptr; }

    template <typename T> const T* target_impl(std::integral_constant<int, 0>) const noexcept {
        return (m_ops && m_ops->type() == typeid(T)) ? static_cast<const T*>(obj()) : nullptr;
    }

public:
    BigFloatFunction() noexcept {}
    BigFloatFunction(std::nullptr_t) noexcept {}

    BigFloatFunction(bffdetail::fp2_t fp, const char* name = nullptr) noexcept : m_fp2(fp), m_name(name) {}

    template <typename F, typename D = typename std::decay<F>::type,
              typename = typename std::enable_if<!std::is_same<D, BigFloatFunction>::value &&
                                                 !bffdetail::bff_is_std_function<D>::value &&
                                                 !std::is_same<D, std::nullptr_t>::value>::type>
    BigFloatFunction(F&& f, const char* name = nullptr) : m_name(name) {
        static_assert(bffdetail::bff_callable<D>::value,
                      "BigFloatFunction: callable must accept (const BigFloat&) or (const BigFloat&, const BigFloatContext&) "
                      "and return something BigFloat is constructible from");
        static_assert(std::is_copy_constructible<D>::value, "BigFloatFunction: callable must be copy constructible");
        store<D>(std::forward<F>(f), bffdetail::bff_kind<D>());
    }

    BigFloatFunction(std::function<BigFloat(const BigFloat&)> f, const char* name = nullptr) : m_name(name) {
        if (!f) return;
        if (const bffdetail::fp1_t* p = f.target<bffdetail::fp1_t>()) { m_fp1 = *p; return; }
        store<std::function<BigFloat(const BigFloat&)>>(std::move(f), std::integral_constant<int, 0>());
    }

    BigFloatFunction(std::function<BigFloat(BigFloat)> f, const char* name = nullptr) : m_name(name) {
        if (!f) return;
        store<std::function<BigFloat(BigFloat)>>(std::move(f), std::integral_constant<int, 0>());
    }

    BigFloatFunction(std::function<BigFloat(const BigFloat&, const BigFloatContext&)> f, const char* name = nullptr) : m_name(name) {
        if (!f) return;
        if (const bffdetail::fp2_t* p = f.target<bffdetail::fp2_t>()) { m_fp2 = *p; return; }
        store<std::function<BigFloat(const BigFloat&, const BigFloatContext&)>>(std::move(f), std::integral_constant<int, 0>());
    }

    BigFloatFunction(const BigFloatFunction& o) : m_ops(o.m_ops), m_invoke(o.m_invoke), m_heap(nullptr), m_fp1(o.m_fp1), m_fp2(o.m_fp2), m_name(o.m_name), m_calls(0) {
        if (m_ops) m_heap = m_ops->clone(o.obj(), m_buf);
    }

    BigFloatFunction(BigFloatFunction&& o) noexcept { move_from(o); }

    ~BigFloatFunction() { clear(); }

    BigFloatFunction& operator=(const BigFloatFunction& o) {
        if (this != &o) {
            BigFloatFunction t(o);
            clear();
            move_from(t);
        }

        return *this;
    }

    BigFloatFunction& operator=(BigFloatFunction&& o) noexcept {
        if (this != &o) {
            clear();
            move_from(o);
        }

        return *this;
    }

    BigFloatFunction& operator=(std::nullptr_t) noexcept { clear(); return *this; }

    template <typename F, typename D = typename std::decay<F>::type, typename = typename std::enable_if<!std::is_same<D, BigFloatFunction>::value && !std::is_same<D, std::nullptr_t>::value>::type>
    BigFloatFunction& operator=(F&& f) {
        BigFloatFunction t(std::forward<F>(f), m_name);
        clear();
        move_from(t);
        return *this;
    }

    void swap(BigFloatFunction& o) noexcept {
        BigFloatFunction t(std::move(o));
        o     = std::move(*this);
        *this = std::move(t);
    }

    BigFloat operator()(const BigFloat& x) const {                                  
        ++m_calls;
        if (m_fp1)     return m_fp1(x);
        if (m_fp2)     return m_fp2(x, BigFloatContext::current());
        if (!m_invoke) throw std::bad_function_call();
        return m_invoke(obj(), x, nullptr);
    }

    BigFloat operator()(const BigFloat& x, const BigFloatContext& ctx) const {       
        ++m_calls;
        if (m_fp2) return m_fp2(x, ctx);

        if (m_fp1) {
            if (bffdetail::bff_same_as_current(ctx)) return m_fp1(x);
            ScopedContext scope(ctx);
            return m_fp1(x);
        }

        if (!m_invoke) throw std::bad_function_call();
        return m_invoke(obj(), x, &ctx);
    }

    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    BigFloat operator()(T x) const { return (*this)(BigFloat(x)); }

    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
    BigFloat operator()(T x, const BigFloatContext& ctx) const { return (*this)(BigFloat(x), ctx); }

    void evaluate(const BigFloat* xs, BigFloat* out, std::size_t n, const BigFloatContext& ctx) const {
        if (!*this) throw std::bad_function_call();

        if (!is_context_aware() && !bffdetail::bff_same_as_current(ctx)) {
            ScopedContext scope(ctx);
            for (std::size_t i = 0; i < n; ++i) out[i] = (*this)(xs[i], ctx);
            return;
        }

        for (std::size_t i = 0; i < n; ++i) out[i] = (*this)(xs[i], ctx);
    }

    std::vector<BigFloat> evaluate(const std::vector<BigFloat>& xs, const BigFloatContext& ctx) const {
        std::vector<BigFloat> out(xs.size());
        evaluate(xs.data(), out.data(), xs.size(), ctx);
        return out;
    }

    std::vector<BigFloat> evaluate(const std::vector<BigFloat>& xs) const { return evaluate(xs, BigFloatContext::current()); }

    explicit operator bool()   const noexcept { return m_fp1 || m_fp2 || m_ops; }
    bool is_context_aware()    const noexcept { return m_fp2 != nullptr || (m_ops != nullptr && m_ops->ctx_aware); }
    bool is_function_pointer() const noexcept { return m_fp1 != nullptr || m_fp2 != nullptr; }
    bool is_inline()           const noexcept { return m_ops != nullptr && m_heap == nullptr; }  

    const char* name() const noexcept          { return m_name ? m_name : ""; }
    void        set_name(const char* n) noexcept { m_name = n; }                     

    std::uint64_t call_count() const noexcept       { return m_calls; }
    void          reset_call_count() const noexcept { m_calls = 0; }

    const std::type_info& target_type() const noexcept {
        if (m_fp1) return typeid(bffdetail::fp1_t);
        if (m_fp2) return typeid(bffdetail::fp2_t);
        return m_ops ? m_ops->type() : typeid(void);
    }

    template <typename T> const T* target() const noexcept { return target_impl<T>(bffdetail::bff_kind<T>()); }
    template <typename T> T*       target() noexcept       { return const_cast<T*>(static_cast<const BigFloatFunction&>(*this).target<T>()); }

    friend bool operator==(const BigFloatFunction& f, std::nullptr_t) noexcept { return !f; }
    friend bool operator==(std::nullptr_t, const BigFloatFunction& f) noexcept { return !f; }
    friend bool operator!=(const BigFloatFunction& f, std::nullptr_t) noexcept { return static_cast<bool>(f); }
    friend bool operator!=(std::nullptr_t, const BigFloatFunction& f) noexcept { return static_cast<bool>(f); }
};

inline void swap(BigFloatFunction& a, BigFloatFunction& b) noexcept { a.swap(b); }

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_FUNCTION_HPP