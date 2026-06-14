#ifndef FIZMO_CUSTOM_ARRAY_HPP
#define FIZMO_CUSTOM_ARRAY_HPP

#include <cstdint>
#include <algorithm>
#include <type_traits>
#include <utility>
#include "array_iterators.hpp"
#include "../Random/random_std_int.hpp"
#include "../Random/random_big_int.hpp"
#include "../Random/random_fixed_int.hpp"
#include <forward_list>
#include <deque>
#include <queue>
#include <stack>

namespace fizmo {

template<class T, class = void>
struct is_streamable : std::false_type {};

template<class T>
struct is_streamable<T,
    std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>
> : std::true_type {};

template<class T>
inline constexpr bool is_streamable_v = is_streamable<T>::value;

template <typename T> 
class DynamicArray;

template <typename T> 
struct is_custom_array : std::false_type {};

template <typename T> 
struct is_custom_array<DynamicArray<T>> : std::true_type{};

template <typename T> 
constexpr bool is_custom_array_v = is_custom_array<T>::value;

template <typename T, typename U>
static constexpr bool are_compatible_types = std::is_convertible<U, T>::value || std::is_same<typename std::decay<U>::type, typename std::decay<T>::type>::value;

namespace detail {
    template <typename T>
    struct is_pointer_like : std::is_pointer<T> {};

    template <typename T>
    struct is_pointer_like<std::unique_ptr<T>> : std::true_type {};

    template <typename T>
    struct is_pointer_like<std::shared_ptr<T>> : std::true_type {};

    template <typename T>
    constexpr bool is_pointer_like_v = is_pointer_like<T>::value;

    template <typename T>
    typename std::enable_if<std::is_pointer<T>::value>::type
    deep_reset_element(T& elem) {
        delete elem;
        elem = nullptr;
    }

    template <typename T>
    typename std::enable_if<
        !std::is_pointer<T>::value && is_pointer_like_v<T>
    >::type
    deep_reset_element(T& elem) { elem.reset(); }

    template <typename T>
    typename std::enable_if<is_custom_array_v<T>>::type
    deep_reset_element(T& elem) { elem.deep_reset(); }

    template <typename T>
    typename std::enable_if<
        !std::is_pointer<T>::value &&
        !is_pointer_like_v<T> &&
        !is_custom_array_v<T>
    >::type
    deep_reset_element(T& elem) { elem = T{}; }

} // namespace detail

static constexpr std::size_t npos = static_cast<std::size_t>(-1);

template <typename T>
struct ArrayView {
private:
    const T* m_data;
    std::size_t m_size;
    using value_type_array = DynamicArray<T>;

public:
    constexpr ArrayView(const T* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    constexpr std::size_t size() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }
    constexpr const T& operator[](std::size_t index) const noexcept { return index < m_size ? m_data[index] : m_data[m_size - 1]; }
    constexpr const T* at(std::size_t index) const noexcept { return index < m_size ? &m_data[index] : nullptr; }
    constexpr const T& front() const noexcept { return m_data[0]; }
    constexpr const T& back() const noexcept { return m_data[m_size - 1]; }
    constexpr const T* data() const noexcept { return m_data; }
    constexpr ConstArrayIterator<T> begin() const noexcept { return ConstArrayIterator<T>(m_data); }
    constexpr ConstArrayIterator<T> end() const noexcept { return ConstArrayIterator<T>(m_data + m_size); }
    constexpr ConstReverseArrayIterator<T> rbegin() const noexcept { return ConstReverseArrayIterator<T>(m_data + m_size); }
    constexpr ConstReverseArrayIterator<T> rend() const noexcept { return ConstReverseArrayIterator<T>(m_data); }
};

enum class Direction : std::uint8_t { All, Front, Back };

template <typename T>
class WhereProxy;

template <typename T>
class DynamicArray {
private:
    T* m_data;
    std::size_t m_size;
    std::size_t m_capacity;
    friend class WhereProxy<T>;

public:
    using value_type = T;
    using value_type_array = DynamicArray<T>;

    constexpr DynamicArray() noexcept : m_data(nullptr), m_size(0), m_capacity(0) {}
    
    DynamicArray(const DynamicArray& a) : m_data(new T[a.m_capacity]), m_size(a.m_size), m_capacity(a.m_capacity) {
        std::copy(a.m_data, a.m_data + a.m_size, m_data);
    }

    DynamicArray(DynamicArray&& a) noexcept : m_data(a.m_data), m_size(a.m_size), m_capacity(a.m_capacity) {
        a.m_data = nullptr;
        a.m_size = 0;
        a.m_capacity = 0;
    }

    DynamicArray(std::initializer_list<T> init) : m_data(new T[init.size()]), m_size(init.size()), m_capacity(init.size()) {
        std::copy(init.begin(), init.end(), m_data);
    }

    explicit DynamicArray(const std::vector<T>& vec) : m_data(new T[vec.size()]), m_size(vec.size()), m_capacity(vec.size()) {
        std::copy(vec.begin(), vec.end(), m_data);
    }

    template <std::size_t N>
    explicit DynamicArray(const std::array<T, N>& arr) : m_data(new T[N]), m_size(N), m_capacity(N) {
        std::copy(arr.begin(), arr.end(), m_data);
    }

    explicit DynamicArray(const std::deque<T>& deq) : m_data(new T[deq.size()]), m_size(deq.size()), m_capacity(deq.size()) {
        std::copy(deq.begin(), deq.end(), m_data);
    }

    explicit DynamicArray(const std::list<T>& lst) : m_data(new T[lst.size()]), m_size(lst.size()), m_capacity(lst.size()) {
        std::copy(lst.begin(), lst.end(), m_data);
    }

    explicit DynamicArray(const std::forward_list<T>& fwd) {
        m_size = m_capacity = static_cast<std::size_t>(std::distance(fwd.begin(), fwd.end()));
        m_data = new T[m_capacity];
        std::copy(fwd.begin(), fwd.end(), m_data);
    }

    explicit DynamicArray(std::queue<T> q) : m_data(nullptr), m_size(0), m_capacity(0) {
        m_capacity = m_size = q.size();
        m_data = new T[m_capacity];

        for (std::size_t i = 0; i < m_size; ++i) {
            m_data[i] = std::move(q.front());
            q.pop();
        }
    }

    explicit DynamicArray(std::stack<T> s) : m_data(nullptr), m_size(0), m_capacity(0) {
        m_capacity = m_size = s.size();
        m_data = new T[m_capacity];

        for (std::size_t i = m_size; i > 0; --i) {
            m_data[i - 1] = std::move(s.top());
            s.pop();
        }
    }

    explicit DynamicArray(std::priority_queue<T> pq) : m_data(nullptr), m_size(0), m_capacity(0) {
        m_capacity = m_size = pq.size();
        m_data = new T[m_capacity];

        for (std::size_t i = 0; i < m_size; ++i) {
            m_data[i] = std::move(const_cast<T&>(pq.top()));
            pq.pop();
        }
    }

    explicit DynamicArray(std::size_t size) : m_data(nullptr), m_size(size), m_capacity(size) {
        m_data = new T[size];
        for (std::size_t i = 0; i < size; ++i) { m_data[i] = T{}; }
    }

    template<typename U, typename = typename std::enable_if<are_compatible_types<U, T>>::type>
    DynamicArray(std::size_t size, U&& value) : m_data(nullptr), m_size(size), m_capacity(size) {
        m_data = new T[size];
        for (std::size_t i = 0; i < size; ++i) { m_data[i] = static_cast<T>(value); }
    }

    template<typename U, typename = typename std::enable_if<are_compatible_types<U, T>>::type>
    DynamicArray(std::size_t capacity, std::size_t size, U&& value) : m_data(nullptr), m_size(size), m_capacity(capacity) {
        m_data = new T[capacity];
        for (std::size_t i = 0; i < size; ++i) { m_data[i] = static_cast<T>(value); }
    }

    template <typename InputIt>
    DynamicArray(InputIt first, InputIt last) {
        m_size = m_capacity = std::distance(first, last);
        m_data = new T[m_capacity];
        std::copy(first, last, m_data);
    }

    template <typename Container>
    explicit DynamicArray(const Container& c) : DynamicArray(std::begin(c), std::end(c)) {}

    ~DynamicArray() {
        delete[] m_data;
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

public:
    DynamicArray& operator=(const DynamicArray& other) {
        if (this == &other) return *this;

        if (other.m_size <= m_capacity) {
            m_size = other.m_size;
            std::copy(other.m_data, other.m_data + other.m_size, m_data);
        } else {
            T* new_data = new T[other.m_capacity];
            std::copy(other.m_data, other.m_data + other.m_size, new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
        }

        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this == &other) return *this;
        delete[] m_data;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
        return *this;
    }

    DynamicArray& operator=(const std::vector<T>& vec) {
        std::size_t new_size = vec.size();

        if (new_size <= m_capacity) {
            m_size = new_size;
            std::copy(vec.begin(), vec.end(), m_data);
        } else {
            T* new_data = new T[new_size];
            std::copy(vec.begin(), vec.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = new_size;
            m_capacity = new_size;
        }

        return *this;
    }

    template <std::size_t N>
    DynamicArray& operator=(const std::array<T, N>& arr) {
        if (N <= m_capacity) {
            m_size = N;
            std::copy(arr.begin(), arr.end(), m_data);
        } else {
            T* new_data = new T[N];
            std::copy(arr.begin(), arr.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = N;
            m_capacity = N;
        }

        return *this;
    }

    DynamicArray& operator=(std::initializer_list<T> init) {
        std::size_t new_size = init.size();

        if (new_size <= m_capacity) {
            m_size = new_size;
            std::copy(init.begin(), init.end(), m_data);
        } else {
            T* new_data = new T[new_size];
            std::copy(init.begin(), init.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = new_size;
            m_capacity = new_size;
        }

        return *this;
    }

    DynamicArray& operator=(const std::deque<T>& deq) {
        std::size_t new_size = deq.size();

        if (new_size <= m_capacity) {
            m_size = new_size;
            std::copy(deq.begin(), deq.end(), m_data);
        } else {
            T* new_data = new T[new_size];
            std::copy(deq.begin(), deq.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = new_size;
            m_capacity = new_size;
        }

        return *this;
    }

    DynamicArray& operator=(const std::list<T>& lst) {
        std::size_t new_size = lst.size();

        if (new_size <= m_capacity) {
            m_size = new_size;
            std::copy(lst.begin(), lst.end(), m_data);
        } else {
            T* new_data = new T[new_size];
            std::copy(lst.begin(), lst.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = new_size;
            m_capacity = new_size;
        }

        return *this;
    }

    DynamicArray& operator=(const std::forward_list<T>& fwd) {
        std::size_t new_size = static_cast<std::size_t>(std::distance(fwd.begin(), fwd.end()));

        if (new_size <= m_capacity) {
            m_size = new_size;
            std::copy(fwd.begin(), fwd.end(), m_data);
        } else {
            T* new_data = new T[new_size];
            std::copy(fwd.begin(), fwd.end(), new_data);
            delete[] m_data;
            m_data = new_data;
            m_size = new_size;
            m_capacity = new_size;
        }

        return *this;
    }

    DynamicArray& operator=(std::queue<T> q) {
        std::size_t new_size = q.size();

        if (new_size > m_capacity) {
            delete[] m_data;
            m_data = new T[new_size];
            m_capacity = new_size;
        }

        m_size = new_size;
        
        for (std::size_t i = 0; i < m_size; ++i) {
            m_data[i] = std::move(q.front());
            q.pop();
        }

        return *this;
    }

    DynamicArray& operator=(std::stack<T> s) {
        std::size_t new_size = s.size();

        if (new_size > m_capacity) {
            delete[] m_data;
            m_data = new T[new_size];
            m_capacity = new_size;
        }

        m_size = new_size;

        for (std::size_t i = m_size; i > 0; --i) {
            m_data[i - 1] = std::move(s.top());
            s.pop();
        }

        return *this;
    }

    DynamicArray& operator=(std::priority_queue<T> pq) {
        std::size_t new_size = pq.size();

        if (new_size > m_capacity) {
            delete[] m_data;
            m_data = new T[new_size];
            m_capacity = new_size;
        }

        m_size = new_size;

        for (std::size_t i = 0; i < m_size; ++i) {
            m_data[i] = std::move(const_cast<T&>(pq.top()));
            pq.pop();
        }

        return *this;
    }

public:
    std::size_t size() const noexcept { return m_size; }
    std::size_t capacity() const noexcept { return m_capacity; }
    T* data() noexcept { return m_data; }
    const T* data() const noexcept { return m_data; }
    std::size_t max_index() const noexcept { return m_size - 1; }
    std::size_t middle_index() const noexcept { return m_size / 2; }
    bool empty() const noexcept { return m_size == 0; }

    inline WhereProxy<T> where(const T& value);

    template <typename Pred>
    inline WhereProxy<T> where(Pred pred);

public:
    T& operator[](std::size_t index) noexcept { return m_data[index]; }
    const T& operator[](std::size_t index) const noexcept { return m_data[index]; }
    T* at(std::size_t index) noexcept { return index < m_size ? &m_data[index] : nullptr; }
    const T* at(std::size_t index) const noexcept { return index < m_size ? &m_data[index] : nullptr; }

    T get(std::size_t index) const {
        if (index >= m_size) { throw std::out_of_range("DynamicArray::get — index out of range"); }
        return m_data[index];
    }

    T get_or(std::size_t index, const T& default_value = T{}) const noexcept { return index < m_size ? m_data[index] : default_value; }
    T& front() noexcept { return m_data[0]; }
    const T& front() const noexcept { return m_data[0]; }
    T& middle() noexcept { return m_data[m_size / 2]; }
    const T& middle() const noexcept { return m_data[m_size / 2]; }
    T& back() noexcept { return m_data[m_size - 1]; }
    const T& back() const noexcept { return m_data[m_size - 1]; }

public:
    ArrayIterator<T> begin() noexcept { return ArrayIterator<T>(m_data); }
    ArrayIterator<T> end() noexcept { return ArrayIterator<T>(m_data + m_size); }
    ConstArrayIterator<T> begin() const noexcept { return ConstArrayIterator<T>(m_data); }
    ConstArrayIterator<T> end() const noexcept { return ConstArrayIterator<T>(m_data + m_size); }
    ConstArrayIterator<T> cbegin() const noexcept { return ConstArrayIterator<T>(m_data); }
    ConstArrayIterator<T> cend() const noexcept { return ConstArrayIterator<T>(m_data + m_size); }
    ReverseArrayIterator<T> rbegin() noexcept { return ReverseArrayIterator<T>(m_data + m_size); }
    ReverseArrayIterator<T> rend() noexcept { return ReverseArrayIterator<T>(m_data); }
    ConstReverseArrayIterator<T> rbegin() const noexcept { return ConstReverseArrayIterator<T>(m_data + m_size); }
    ConstReverseArrayIterator<T> rend() const noexcept { return ConstReverseArrayIterator<T>(m_data); }
    ConstReverseArrayIterator<T> crbegin() const noexcept { return ConstReverseArrayIterator<T>(m_data + m_size); }
    ConstReverseArrayIterator<T> crend() const noexcept { return ConstReverseArrayIterator<T>(m_data); }

public:
    void reserve(std::size_t new_capacity) {
        if (new_capacity <= m_capacity) return;
        T* new_data = new T[new_capacity];

        if (m_data) {
            std::copy(m_data, m_data + m_size, new_data);
            delete[] m_data;
        }

        m_data = new_data;
        m_capacity = new_capacity;
    }

    void resize(std::size_t new_size) {
        if (new_size > m_capacity) { reserve(new_size); }
        for (std::size_t i = m_size; i < new_size; ++i) { m_data[i] = T{}; }
        m_size = new_size;
    }

    template <typename U>
    typename std::enable_if<are_compatible_types<T, U>>::type
    resize(std::size_t new_size, U&& value) {
        if (new_size > m_capacity) { reserve(new_size); }
        for (std::size_t i = m_size; i < new_size; ++i) { m_data[i] = static_cast<T>(value); }
        m_size = new_size;
    }

    void clear() noexcept { m_size = 0; }

    T* release() noexcept {
        T* released = m_data;
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
        return released;
    }

    void deallocate() noexcept {
        delete[] m_data;
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void shrink() {
        if (m_capacity == m_size) return;

        if (m_size == 0) {
            delete[] m_data;
            m_data = nullptr;
            m_capacity = 0;
            return;
        }

        T* new_data = new T[m_size];
        std::copy(m_data, m_data + m_size, new_data);
        delete[] m_data;
        m_data = new_data;
        m_capacity = m_size;
    }

    void ensure_capacity(std::size_t min_capacity) {
        if (min_capacity <= m_capacity) return;
        std::size_t new_capacity = m_capacity + (m_capacity / 2);
        if (new_capacity < min_capacity) { new_capacity = min_capacity; }
        reserve(new_capacity);
    }

    bool needs_reallocation(std::size_t count = 1) const noexcept { return (m_size + count) > m_capacity; }
    void reset() { for (std::size_t i = 0; i < m_size; ++i) { m_data[i] = T{}; }}
    void deep_reset() { for (std::size_t i = 0; i < m_size; ++i) { detail::deep_reset_element(m_data[i]); }}

public:
    void push_back(const T& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size++] = value;
    }

    void push_back(T&& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size++] = std::move(value);
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    push_back(U&& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size++] = static_cast<T>(std::forward<U>(value));
    }

    void push_back(const DynamicArray& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::copy(arr.m_data, arr.m_data + arr.m_size, m_data + m_size);
        m_size += arr.m_size;
    }

    void push_back(DynamicArray&& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::move(arr.m_data, arr.m_data + arr.m_size, m_data + m_size);
        m_size += arr.m_size;
    }

public:
    void push_front(const T& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + 1);
        m_data[0] = value;
        ++m_size;
    }

    void push_front(T&& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + 1);
        m_data[0] = std::move(value);
        ++m_size;
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    push_front(U&& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + 1);
        m_data[0] = static_cast<T>(std::forward<U>(value));
        ++m_size;
    }

    void push_front(const DynamicArray& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + arr.m_size);
        std::copy(arr.m_data, arr.m_data + arr.m_size, m_data);
        m_size += arr.m_size;
    }

    void push_front(DynamicArray&& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + arr.m_size);
        std::move(arr.m_data, arr.m_data + arr.m_size, m_data);
        m_size += arr.m_size;
    }

public:
    void insert(std::size_t index, const T& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + 1);
        m_data[index] = value;
        ++m_size;
    }

    void insert(std::size_t index, T&& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + 1);
        m_data[index] = std::move(value);
        ++m_size;
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    insert(std::size_t index, U&& value) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + 1);
        m_data[index] = static_cast<T>(std::forward<U>(value));
        ++m_size;
    }

    void insert(std::size_t index, const DynamicArray& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + arr.m_size);
        std::copy(arr.m_data, arr.m_data + arr.m_size, m_data + index);
        m_size += arr.m_size;
    }

    void insert(std::size_t index, DynamicArray&& arr) {
        if (arr.m_size == 0) return;
        ensure_capacity(m_size + arr.m_size);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + arr.m_size);
        std::move(arr.m_data, arr.m_data + arr.m_size, m_data + index);
        m_size += arr.m_size;
    }

    void insert(std::size_t index, const T* src, std::size_t count) {
        if (count == 0) return;
        ensure_capacity(m_size + count);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + count);
        std::copy(src, src + count, m_data + index);
        m_size += count;
    }

    void insert(std::size_t index, const DynamicArray& src, std::size_t src_start, std::size_t count) { range_insert(index, src.m_data + src_start, count); }

    void insert_unordered(std::size_t index, const T& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size] = std::move(m_data[index]);
        m_data[index] = value;
        ++m_size;
    }

    void insert_unordered(std::size_t index, T&& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size] = std::move(m_data[index]);
        m_data[index] = std::move(value);
        ++m_size;
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    insert_unordered(std::size_t index, U&& value) {
        ensure_capacity(m_size + 1);
        m_data[m_size] = std::move(m_data[index]);
        m_data[index] = static_cast<T>(std::forward<U>(value));
        ++m_size;
    }

public:
    T pop_back() {
        T value = std::move(m_data[--m_size]);
        return value;
    }

    DynamicArray pop_back(std::size_t from) {
        if (from >= m_size) return DynamicArray();
        std::size_t count = m_size - from;
        DynamicArray result(count);
        for (std::size_t i = 0; i < count; ++i) { result.m_data[i] = std::move(m_data[from + i]); }
        m_size = from;
        return result;
    }

    DynamicArray pop_back_count(std::size_t count) {
        if (count == 0) return DynamicArray();
        if (count > m_size) count = m_size;
        std::size_t start = m_size - count;
        return pop(start, m_size);
    }

    T pop_front() {
        T value = std::move(m_data[0]);
        std::move(m_data + 1, m_data + m_size, m_data);
        --m_size;
        return value;
    }

    DynamicArray pop_front(std::size_t end_idx) {
        if (end_idx == 0) return DynamicArray();
        if (end_idx > m_size) end_idx = m_size;
        DynamicArray result(end_idx);
        for (std::size_t i = 0; i < end_idx; ++i) { result.m_data[i] = std::move(m_data[i]); }
        std::move(m_data + end_idx, m_data + m_size, m_data);
        m_size -= end_idx;
        return result;
    }

    DynamicArray pop_front_count(std::size_t count) {
        if (count == 0) return DynamicArray();
        if (count > m_size) count = m_size;
        return pop(0, count);
    }

    T pop(std::size_t index) {
        T value = std::move(m_data[index]);
        std::move(m_data + index + 1, m_data + m_size, m_data + index);
        --m_size;
        return value;
    }

    DynamicArray pop(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx) return DynamicArray();
        if (end_idx > m_size) end_idx = m_size;
        std::size_t count = end_idx - start;
        DynamicArray result(count);
        for (std::size_t i = 0; i < count; ++i) { result.m_data[i] = std::move(m_data[start + i]); }
        std::move(m_data + end_idx, m_data + m_size, m_data + start);
        m_size -= count;
        return result;
    }

    T pop_unordered(std::size_t index) {
        T value = std::move(m_data[index]);
        m_data[index] = std::move(m_data[--m_size]);
        return value;
    }

    DynamicArray pop_unordered(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx) return DynamicArray();
        if (end_idx > m_size) end_idx = m_size;
        std::size_t count = end_idx - start;
        DynamicArray result(count);
        for (std::size_t i = 0; i < count; ++i) { result.m_data[i] = std::move(m_data[start + i]); }
        std::size_t remaining_after = m_size - end_idx;
        std::size_t backfill = count < remaining_after ? count : remaining_after;
        for (std::size_t i = 0; i < backfill; ++i) { m_data[start + i] = std::move(m_data[m_size - backfill + i]); }
        m_size -= count;
        return result;
    }

    DynamicArray pop_by_value(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices(value);
        apply_direction_filter(idx, count, dir);
        return pop_at_indices(idx.data(), idx.size());
    }

    template <typename Pred>
    DynamicArray pop_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices_if(pred);
        apply_direction_filter(idx, count, dir);
        return pop_at_indices(idx.data(), idx.size());
    }

    DynamicArray pop_by_value_unordered(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices(value);
        apply_direction_filter(idx, count, dir);
        return pop_at_indices_unordered(idx.data(), idx.size());
    }

    template <typename Pred>
    DynamicArray pop_if_unordered(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices_if(pred);
        apply_direction_filter(idx, count, dir);
        return pop_at_indices_unordered(idx.data(), idx.size());
    }

public:
    void remove_back() { --m_size; }

    void remove_front() {
        std::move(m_data + 1, m_data + m_size, m_data);
        --m_size;
    }

    void remove(std::size_t index) {
        std::move(m_data + index + 1, m_data + m_size, m_data + index);
        --m_size;
    }

    void remove_unordered(std::size_t index) { m_data[index] = std::move(m_data[--m_size]); }

    std::size_t remove_by_value(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices(value);
        apply_direction_filter(idx, count, dir);
        std::size_t n = idx.size();
        remove_at_indices(idx.data(), n);
        return n;
    }

    template <typename Pred>
    std::size_t remove_if(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices_if(pred);
        apply_direction_filter(idx, count, dir);
        std::size_t n = idx.size();
        remove_at_indices(idx.data(), n);
        return n;
    }

    std::size_t remove_by_value_unordered(const T& value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices(value);
        apply_direction_filter(idx, count, dir);
        std::size_t n = idx.size();
        remove_at_indices_unordered(idx.data(), n);
        return n;
    }

    template <typename Pred>
    std::size_t remove_if_unordered(Pred pred, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices_if(pred);
        apply_direction_filter(idx, count, dir);
        std::size_t n = idx.size();
        remove_at_indices_unordered(idx.data(), n);
        return n;
    }

public:
    template <typename... Args>
    T& emplace_back(Args&&... args) {
        ensure_capacity(m_size + 1);
        m_data[m_size] = T(std::forward<Args>(args)...);
        return m_data[m_size++];
    }

    template <typename... Args>
    T& emplace_front(Args&&... args) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data, m_data + m_size, m_data + m_size + 1);
        m_data[0] = T(std::forward<Args>(args)...);
        ++m_size;
        return m_data[0];
    }

    template <typename... Args>
    T& emplace(std::size_t index, Args&&... args) {
        ensure_capacity(m_size + 1);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + 1);
        m_data[index] = T(std::forward<Args>(args)...);
        ++m_size;
        return m_data[index];
    }

public:
    ArrayView<T> view(std::size_t start, std::size_t end_idx) const noexcept {
        if (start >= m_size) return ArrayView<T>(nullptr, 0);
        if (end_idx > m_size) end_idx = m_size;
        if (start >= end_idx) return ArrayView<T>(nullptr, 0);
        return ArrayView<T>(m_data + start, end_idx - start);
    }

    ArrayView<T> view() const noexcept { return ArrayView<T>(m_data, m_size); }

    DynamicArray slice(std::size_t start, std::size_t end_idx) const {
        if (start >= m_size) return DynamicArray();
        if (end_idx > m_size) end_idx = m_size;
        if (start >= end_idx) return DynamicArray();
        std::size_t count = end_idx - start;
        DynamicArray result;
        result.m_data = new T[count];
        result.m_size = result.m_capacity = count;
        std::copy(m_data + start, m_data + start + count, result.m_data);
        return result;
    }

    DynamicArray slice() const { return DynamicArray(*this); }
    
public:
    void take(DynamicArray& src) {
        if (src.m_size == 0) return;
        ensure_capacity(m_size + src.m_size);
        std::move(src.m_data, src.m_data + src.m_size, m_data + m_size);
        m_size += src.m_size;
        src.clear();
    }

    void take(std::size_t index, DynamicArray& src) {
        if (src.m_size == 0) return;
        ensure_capacity(m_size + src.m_size);
        std::move_backward(m_data + index, m_data + m_size, m_data + m_size + src.m_size);
        std::move(src.m_data, src.m_data + src.m_size, m_data + index);
        m_size += src.m_size;
        src.clear();
    }

    void take(DynamicArray& src, std::size_t src_index) {
        if (src_index >= src.m_size) return;
        ensure_capacity(m_size + 1);
        push_back(std::move(src.m_data[src_index]));
        src.remove(src_index);
    }

    void take(DynamicArray& src, std::size_t src_index, std::size_t dst_index) {
        if (src_index >= src.m_size) return;
        ensure_capacity(m_size + 1);
        T value = std::move(src.m_data[src_index]);
        src.remove(src_index);
        insert(dst_index, std::move(value));
    }

    void take_range(DynamicArray& src, std::size_t start, std::size_t end) {
        if (start >= end || start >= src.m_size) return;
        if (end > src.m_size) end = src.m_size;
        std::size_t count = end - start;
        DynamicArray<T> extracted = src.pop(start, end);
        ensure_capacity(m_size + count);
        push_back(std::move(extracted));
    }

    void take_range(DynamicArray& src, std::size_t start, std::size_t end, std::size_t dst_index) {
        if (start >= end || start >= src.m_size) return;
        if (end > src.m_size) end = src.m_size;
        std::size_t count = end - start;
        DynamicArray<T> extracted = src.pop(start, end);
        ensure_capacity(m_size + count);
        insert(dst_index, std::move(extracted));
    }

public:
    void swap(std::size_t i, std::size_t j) noexcept {
        if (i == j) return;
        T temp = std::move(m_data[i]);
        m_data[i] = std::move(m_data[j]);
        m_data[j] = std::move(temp);
    }

    void swap(std::size_t i, DynamicArray& other, std::size_t j) noexcept {
        T temp = std::move(m_data[i]);
        m_data[i] = std::move(other.m_data[j]);
        other.m_data[j] = std::move(temp);
    }

    void swap_ranges(std::size_t a_start, std::size_t b_start, std::size_t count) noexcept {
        if (a_start == b_start || count == 0) return;

        for (std::size_t i = 0; i < count; ++i) {
            T temp = std::move(m_data[a_start + i]);
            m_data[a_start + i] = std::move(m_data[b_start + i]);
            m_data[b_start + i] = std::move(temp);
        }
    }

    void swap_ranges(std::size_t a_start, DynamicArray& other, std::size_t b_start, std::size_t count) noexcept {
        if (this == &other && a_start == b_start) return;
        if (count == 0) return;

        for (std::size_t i = 0; i < count; ++i) {
            T temp = std::move(m_data[a_start + i]);
            m_data[a_start + i] = std::move(other.m_data[b_start + i]);
            other.m_data[b_start + i] = std::move(temp);
        }
    }

    void swap_ranges(
        std::size_t a_start, std::size_t a_end,
        DynamicArray& other,
        std::size_t b_start, std::size_t b_end
    ) {
        std::size_t n1 = a_end - a_start;
        std::size_t n2 = b_end - b_start;

        if (n1 != n2) { 
            throw std::invalid_argument(
                "DynamicArray<T>::swap_ranges unequal ranges. *this range = [" 
                    + std::to_string(a_start) + ", " + std::to_string(a_end) + "] = " + std::to_string(n1)
                    + "\nOther range = [" + std::to_string(b_start) + ", " + std::to_string(b_end) + "] = " + std::to_string(n2)
            ); 
        }

        for (std::size_t i = 0; i < n1; ++i) {
            std::swap(m_data[a_start + i], other.m_data[b_start + i]);
        }
    }

    void swap(const T& a, DynamicArray& other, const T& b) {
        DynamicArray<std::size_t> idxA = collect_indices(a);
        DynamicArray<std::size_t> idxB = other.collect_indices(b);
        std::size_t n = std::min(idxA.size(), idxB.size());
        for (std::size_t i = 0; i < n; ++i) { std::swap(m_data[idxA[i]], other.m_data[idxB[i]]); }
    }

    void swap(DynamicArray& other) noexcept {
        T* tmp_data = m_data;
        std::size_t tmp_size = m_size;
        std::size_t tmp_cap = m_capacity;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = tmp_data;
        other.m_size = tmp_size;
        other.m_capacity = tmp_cap;
    }

public:
    void copy_to(std::size_t src, DynamicArray& dest_arr, std::size_t dst) const { dest_arr.m_data[dst] = m_data[src]; }

    void copy_to(std::size_t src_start, std::size_t count, DynamicArray& dest_arr, std::size_t dst_start) const {
        if (count == 0) return;
        std::copy(m_data + src_start, m_data + src_start + count, dest_arr.m_data + dst_start);
    }

    void copy_from(DynamicArray& src_arr, std::size_t src, std::size_t dst) { m_data[dst] = src_arr.m_data[src]; }

    void copy_from(const DynamicArray& src_arr, std::size_t src_start, std::size_t count, std::size_t dst_start) {
        if (count == 0) return;
        std::copy(src_arr.m_data + src_start, src_arr.m_data + src_start + count, m_data + dst_start);
    }

    void copy_from(const DynamicArray& src_arr) { *this = src_arr; }

    void copy_within(std::size_t src_start, std::size_t count, std::size_t dst_start) {
        if (count == 0 || src_start == dst_start) return;

        if (dst_start > src_start) {
            std::copy_backward(m_data + src_start, m_data + src_start + count, m_data + dst_start + count);
        } else {
            std::copy(m_data + src_start, m_data + src_start + count, m_data + dst_start);
        }
    }

public:
    void move_to(std::size_t src, DynamicArray& dest_arr, std::size_t dst) noexcept { dest_arr.m_data[dst] = std::move(m_data[src]); }

    void move_to(std::size_t src_start, std::size_t count, DynamicArray& dest_arr, std::size_t dst_start) noexcept {
        if (count == 0) return;
        std::move(m_data + src_start, m_data + src_start + count, dest_arr.m_data + dst_start);
    }

    void move_from(DynamicArray& src_arr, std::size_t src, std::size_t dst) noexcept { m_data[dst] = std::move(src_arr.m_data[src]); }

    void move_from(DynamicArray& src_arr, std::size_t src_start, std::size_t count, std::size_t dst_start) noexcept {
        if (count == 0) return;
        std::move(src_arr.m_data + src_start, src_arr.m_data + src_start + count, m_data + dst_start);
    }

    void move_within(std::size_t src_start, std::size_t count, std::size_t dst_start) noexcept {
        if (count == 0 || src_start == dst_start) return;

        if (dst_start > src_start) {
            std::move_backward(m_data + src_start, m_data + src_start + count, m_data + dst_start + count);
        } else {
            std::move(m_data + src_start, m_data + src_start + count, m_data + dst_start);
        }
    }

public:
    void replace(const T& value) { for (std::size_t i = 0; i < m_size; ++i) { m_data[i] = value; }}

    void replace(T&& value) {
        for (std::size_t i = 0; i < m_size - 1; ++i) { m_data[i] = value; }
        m_data[m_size - 1] = std::move(value);
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    replace(U&& value) {
        T converted = static_cast<T>(std::forward<U>(value));
        for (std::size_t i = 0; i < m_size; ++i) { m_data[i] = converted; }
    }

    void replace(std::size_t index, const T& value) { m_data[index] = value; }
    void replace(std::size_t index, T&& value) { m_data[index] = std::move(value); }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    replace(std::size_t index, U&& value) { m_data[index] = static_cast<T>(std::forward<U>(value)); }

    void replace(std::size_t start, std::size_t end_idx, const T& value) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = value; }
    }

    void replace(std::size_t start, std::size_t end_idx, T&& value) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        for (std::size_t i = start; i < end_idx - 1; ++i) { m_data[i] = value; }
        m_data[end_idx - 1] = std::move(value);
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    replace(std::size_t start, std::size_t end_idx, U&& value) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        T converted = static_cast<T>(std::forward<U>(value));
        for (std::size_t i = start; i < end_idx; ++i) { m_data[i] = converted; }
    }

    void replace(std::size_t start, std::size_t end_idx, const DynamicArray& arr) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t count = end_idx - start;
        std::size_t copy_count = count < arr.m_size ? count : arr.m_size;
        std::copy(arr.m_data, arr.m_data + copy_count, m_data + start);
    }

    void replace(std::size_t start, std::size_t end_idx, DynamicArray&& arr) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t count = end_idx - start;
        std::size_t move_count = count < arr.m_size ? count : arr.m_size;
        std::move(arr.m_data, arr.m_data + move_count, m_data + start);
    }

    std::size_t replace_by_value(const T& old_value, const T& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices(old_value);
        apply_direction_filter(idx, count, dir);
        replace_at_indices(idx.data(), idx.size(), new_value);
        return idx.size();
    }

    template <typename Pred>
    std::size_t replace_if(Pred pred, const T& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        auto idx = collect_indices_if(pred);
        apply_direction_filter(idx, count, dir);
        replace_at_indices(idx.data(), idx.size(), new_value);
        return idx.size();
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value,
        std::size_t
    >::type
    replace_by_value(const T& old_value, U&& new_value, std::size_t count = 0, Direction dir = Direction::All) {
        T converted = static_cast<T>(std::forward<U>(new_value));
        return replace_by_value(old_value, converted, count, dir);
    }

public:
    DynamicArray<std::size_t> find_indices(const T& value) const { return collect_indices(value); }

    template <typename Pred>
    DynamicArray<std::size_t> find_indices_if(Pred pred) const { return collect_indices_if(pred); }

    std::size_t count_of(const T& value) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_size; ++i) { if (m_data[i] == value) ++n; }
        return n;
    }

    template <typename Pred>
    std::size_t count_if(Pred pred) const noexcept {
        std::size_t n = 0;
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) ++n; }
        return n;
    }

    bool contains(const T& value) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (m_data[i] == value) return true; }
        return false;
    }

    template <typename Pred>
    bool contains_if(Pred pred) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) return true; }
        return false;
    }

public:
    DynamicArray<std::pair<T, std::size_t>> frequency() const {
        DynamicArray<std::pair<T, std::size_t>> result;

        for (std::size_t i = 0; i < m_size; ++i) {
            bool found = false;

            for (std::size_t j = 0; j < result.size(); ++j) {
                if (result[j].first == m_data[i]) {
                    ++result[j].second;
                    found = true;
                    break;
                }
            }

            if (!found) { result.push_back(std::pair<T, std::size_t>(m_data[i], 1)); }
        }

        return result;
    }

public:
    template <typename Acc, typename Pred, typename Op>
    Acc accumulate_if(Acc init, Pred pred, Op op) const {
        for (std::size_t i = 0; i < m_size; ++i) {
            const T& v = m_data[i];
            if (pred(v)) { init = op(std::move(init), v); }
        }
        return init;
    }

    template <typename Acc, typename Op>
    Acc accumulate(Acc init, Op op) const {
        return accumulate_if(std::move(init),
            [](const T&) { return true; },
            std::move(op)
        );
    }

    template <typename Acc>
    Acc accumulate(Acc init) const {
        return accumulate_if(std::move(init),
            [](const T&) { return true; },
            [](Acc acc, const T& v) { return acc + v; }
        );
    }

public:
    void sort() { std::sort(m_data, m_data + m_size); }

    template <typename Compare>
    void sort(Compare comp) { std::sort(m_data, m_data + m_size, comp); }

    void sort(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::sort(m_data + start, m_data + end_idx);
    }

    template <typename Compare>
    void sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::sort(m_data + start, m_data + end_idx, comp);
    }

    void stable_sort() { std::stable_sort(m_data, m_data + m_size); }

    template <typename Compare>
    void stable_sort(Compare comp) { std::stable_sort(m_data, m_data + m_size, comp); }

    void stable_sort(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::stable_sort(m_data + start, m_data + end_idx);
    }

    template <typename Compare>
    void stable_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::stable_sort(m_data + start, m_data + end_idx, comp);
    }

    bool is_sorted() const noexcept { return std::is_sorted(m_data, m_data + m_size); }

    template <typename Compare>
    bool is_sorted(Compare comp) const noexcept { return std::is_sorted(m_data, m_data + m_size, comp); }

public:
    void bubble_sort() { bubble_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void bubble_sort(Compare comp) { bubble_sort(std::size_t(0), m_size, comp); }

    void bubble_sort(std::size_t start, std::size_t end_idx) { bubble_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void bubble_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        bool swapped = true;

        while (swapped) {
            swapped = false;

            for (std::size_t i = start + 1; i < end_idx; ++i) {
                if (comp(m_data[i], m_data[i - 1])) {
                    T t = std::move(m_data[i]); m_data[i] = std::move(m_data[i - 1]); m_data[i - 1] = std::move(t);
                    swapped = true;
                }
            }

            --end_idx;   
        }
    }

public:
    void insertion_sort() { insertion_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void insertion_sort(Compare comp) { insertion_sort(std::size_t(0), m_size, comp); }

    void insertion_sort(std::size_t start, std::size_t end_idx) { insertion_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void insertion_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;

        for (std::size_t i = start + 1; i < end_idx; ++i) {
            T key = std::move(m_data[i]);
            std::size_t j = i;

            while (j > start && comp(key, m_data[j - 1])) {
                m_data[j] = std::move(m_data[j - 1]);
                --j;
            }

            m_data[j] = std::move(key);
        }
    }

public:
    void selection_sort() { selection_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void selection_sort(Compare comp) { selection_sort(std::size_t(0), m_size, comp); }

    void selection_sort(std::size_t start, std::size_t end_idx) { selection_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void selection_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;

        for (std::size_t i = start; i + 1 < end_idx; ++i) {
            std::size_t best = i;
            for (std::size_t j = i + 1; j < end_idx; ++j) { if (comp(m_data[j], m_data[best])) best = j; }
            if (best != i) { T t = std::move(m_data[i]); m_data[i] = std::move(m_data[best]); m_data[best] = std::move(t); }
        }
    }

public:
    void cocktail_sort() { cocktail_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void cocktail_sort(Compare comp) { cocktail_sort(std::size_t(0), m_size, comp); }

    void cocktail_sort(std::size_t start, std::size_t end_idx) { cocktail_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void cocktail_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        if (end_idx - start <= 1) return;
        std::size_t lo = start, hi = end_idx - 1;
        bool swapped = true;

        while (swapped && lo < hi) {
            swapped = false;

            for (std::size_t i = lo; i < hi; ++i) {
                if (comp(m_data[i + 1], m_data[i])) {
                    T t = std::move(m_data[i]); m_data[i] = std::move(m_data[i + 1]); m_data[i + 1] = std::move(t);
                    swapped = true;
                }
            }

            --hi;
            if (!swapped) break;
            swapped = false;

            for (std::size_t i = hi; i > lo; --i) {
                if (comp(m_data[i], m_data[i - 1])) {
                    T t = std::move(m_data[i]); m_data[i] = std::move(m_data[i - 1]); m_data[i - 1] = std::move(t);
                    swapped = true;
                }
            }

            ++lo;
        }
    }

public:
    void merge_sort() { merge_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void merge_sort(Compare comp) { merge_sort(std::size_t(0), m_size, comp); }

    void merge_sort(std::size_t start, std::size_t end_idx) { merge_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void merge_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        if (end_idx - start <= 1) return;
        T* buf = new T[end_idx];
        merge_sort_impl(m_data, buf, start, end_idx, comp);
        delete[] buf;
    }

public:
    void quick_sort() { quick_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void quick_sort(Compare comp) { quick_sort(std::size_t(0), m_size, comp); }

    void quick_sort(std::size_t start, std::size_t end_idx) { quick_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void quick_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        if (end_idx - start <= 1) return;
        quick_sort_impl(start, end_idx - 1, comp);
    }

public:
    void heap_sort() { heap_sort_range(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void heap_sort(Compare comp) { heap_sort_range(std::size_t(0), m_size, comp); }

    void heap_sort(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        heap_sort_range(start, end_idx, std::less<T>{});
    }

    template <typename Compare>
    void heap_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        heap_sort_range(start, end_idx, comp);
    }

public:
    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    counting_sort() { counting_sort_range(std::size_t(0), m_size); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    counting_sort(std::size_t start, std::size_t end_idx) { counting_sort_range(start, end_idx); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    counting_sort_desc() { counting_sort_range_desc(std::size_t(0), m_size); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    counting_sort_desc(std::size_t start, std::size_t end_idx) { counting_sort_range_desc(start, end_idx); }

public:
    void shell_sort() { shell_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void shell_sort(Compare comp) { shell_sort(std::size_t(0), m_size, comp); }

    void shell_sort(std::size_t start, std::size_t end_idx) { shell_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void shell_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t n = end_idx - start;
        if (n <= 1) return;
        static const std::size_t ciura[] = { 1, 4, 10, 23, 57, 132, 301, 701 };
        static const std::size_t ciura_len = sizeof(ciura) / sizeof(ciura[0]);
        DynamicArray<std::size_t> gaps;
        for (std::size_t i = 0; i < ciura_len && ciura[i] < n; ++i) { gaps.push_back(ciura[i]); }
        if (gaps.empty()) { gaps.push_back(1); }

        while (gaps.back() < n) {
            std::size_t next = static_cast<std::size_t>(gaps.back() * 2.25);
            if (next >= n) break;
            gaps.push_back(next);
        }

        for (std::size_t gi = gaps.size(); gi > 0; --gi) {
            std::size_t gap = gaps[gi - 1];

            for (std::size_t i = start + gap; i < end_idx; ++i) {
                T key = std::move(m_data[i]);
                std::size_t j = i;

                while (j >= start + gap && comp(key, m_data[j - gap])) {
                    m_data[j] = std::move(m_data[j - gap]);
                    j -= gap;
                }

                m_data[j] = std::move(key);
            }
        }
    }

public:
    void gnome_sort() { gnome_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void gnome_sort(Compare comp) { gnome_sort(std::size_t(0), m_size, comp); }

    void gnome_sort(std::size_t start, std::size_t end_idx) { gnome_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void gnome_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        if (end_idx - start <= 1) return;
        std::size_t i = start + 1;

        while (i < end_idx) {
            if (i == start || !comp(m_data[i], m_data[i - 1])) {
                ++i;
            } else {
                T t = std::move(m_data[i]);
                m_data[i] = std::move(m_data[i - 1]);
                m_data[i - 1] = std::move(t);
                --i;
            }
        }
    }

public:
    void bitonic_sort() { bitonic_sort(std::size_t(0), m_size, std::less<T>{}); }

    template <typename Compare>
    void bitonic_sort(Compare comp) { bitonic_sort(std::size_t(0), m_size, comp); }

    void bitonic_sort(std::size_t start, std::size_t end_idx) { bitonic_sort(start, end_idx, std::less<T>{}); }

    template <typename Compare>
    void bitonic_sort(std::size_t start, std::size_t end_idx, Compare comp) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t n = end_idx - start;
        if (n <= 1) return;
        std::size_t padded = 1;
        while (padded < n) padded <<= 1;

        if (padded == n) {
            bitonic_sort_impl(start, padded, true, comp);
        } else {
            T* buf = new T[padded];
            for (std::size_t i = 0; i < n; ++i) { buf[i] = std::move(m_data[start + i]); }
            T max_val = buf[0];
            for (std::size_t i = 1; i < n; ++i) { if (comp(max_val, buf[i])) max_val = buf[i]; }
            for (std::size_t i = n; i < padded; ++i) { buf[i] = max_val; }
            T* saved = m_data;
            std::size_t saved_size = m_size;
            m_data = buf;
            m_size = padded;
            bitonic_sort_impl(0, padded, true, comp);
            m_data = saved;
            m_size = saved_size;
            for (std::size_t i = 0; i < n; ++i) { m_data[start + i] = std::move(buf[i]); }
            delete[] buf;
        }
    }

public:
    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_lsd() { radix_sort_lsd_range(std::size_t(0), m_size); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_lsd(std::size_t start, std::size_t end_idx) { radix_sort_lsd_range(start, end_idx); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_lsd_desc() { radix_sort_lsd_range(std::size_t(0), m_size); reverse(); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_lsd_desc(std::size_t start, std::size_t end_idx) {
        radix_sort_lsd_range(start, end_idx);
        reverse(start, end_idx);
    }

public:
    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_msd() { radix_sort_msd_range(std::size_t(0), m_size); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_msd(std::size_t start, std::size_t end_idx) { radix_sort_msd_range(start, end_idx); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_msd_desc() { radix_sort_msd_range(std::size_t(0), m_size); reverse(); }

    template <typename U = T>
    typename std::enable_if<std::is_integral<U>::value>::type
    radix_sort_msd_desc(std::size_t start, std::size_t end_idx) {
        radix_sort_msd_range(start, end_idx);
        reverse(start, end_idx);
    }

public:
    void unique() {
        if (m_size <= 1) return;
        std::size_t write = 1;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (!(m_data[i] == m_data[write - 1])) {
                if (write != i) m_data[write] = std::move(m_data[i]);
                ++write;
            }
        }

        m_size = write;
    }

    template <typename Pred>
    void unique(Pred pred) {
        if (m_size <= 1) return;
        std::size_t write = 1;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (!pred(m_data[write - 1], m_data[i])) {
                if (write != i) m_data[write] = std::move(m_data[i]);
                ++write;
            }
        }

        m_size = write;
    }

    DynamicArray distinct() const {
        DynamicArray result;
        result.reserve(m_size);

        for (std::size_t i = 0; i < m_size; ++i) {
            if (!result.contains(m_data[i])) { result.push_back(m_data[i]); }
        }

        return result;
    }

    template <typename Pred>
    DynamicArray distinct(Pred eq) const {
        DynamicArray result;
        result.reserve(m_size);

        for (std::size_t i = 0; i < m_size; ++i) {
            bool found = false;

            for (std::size_t j = 0; j < result.m_size; ++j) {
                if (eq(m_data[i], result.m_data[j])) { found = true; break; }
            }

            if (!found) { result.push_back(m_data[i]); }
        }

        return result;
    }

    void deduplicate() {
        for (std::size_t i = 0; i < m_size; ++i) {
            std::size_t j = i + 1;

            while (j < m_size) {
                if (m_data[j] == m_data[i]) {
                    m_data[j] = std::move(m_data[--m_size]);
                } else {
                    ++j;
                }
            }
        }
    }

    template <typename Pred>
    void deduplicate(Pred eq) {
        for (std::size_t i = 0; i < m_size; ++i) {
            std::size_t j = i + 1;

            while (j < m_size) {
                if (eq(m_data[i], m_data[j])) {
                    m_data[j] = std::move(m_data[--m_size]);
                } else {
                    ++j;
                }
            }
        }
    }

public:
    const T& min() const noexcept {
        const T* m = &m_data[0];
        for (std::size_t i = 1; i < m_size; ++i) { if (m_data[i] < *m) m = &m_data[i]; }
        return *m;
    }

    template <typename Compare>
    const T& min(Compare comp) const noexcept {
        const T* m = &m_data[0];
        for (std::size_t i = 1; i < m_size; ++i) { if (comp(m_data[i], *m)) m = &m_data[i]; }
        return *m;
    }

    const T& max() const noexcept {
        const T* m = &m_data[0];
        for (std::size_t i = 1; i < m_size; ++i) { if (*m < m_data[i]) m = &m_data[i]; }
        return *m;
    }

    template <typename Compare>
    const T& max(Compare comp) const noexcept {
        const T* m = &m_data[0];
        for (std::size_t i = 1; i < m_size; ++i) { if (comp(*m, m_data[i])) m = &m_data[i]; }
        return *m;
    }

    std::pair<const T&, const T&> min_max() const noexcept {
        std::size_t lo = 0, hi = 0;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (m_data[i] < m_data[lo]) lo = i;
            if (m_data[hi] < m_data[i]) hi = i;
        }

        return { m_data[lo], m_data[hi] };
    }

    template <typename Compare>
    std::pair<const T&, const T&> min_max(Compare comp) const noexcept {
        std::size_t lo = 0, hi = 0;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (comp(m_data[i], m_data[lo])) lo = i;
            if (comp(m_data[hi], m_data[i])) hi = i;
        }

        return { m_data[lo], m_data[hi] };
    }

    std::size_t min_element_index() const noexcept {
        std::size_t idx = 0;
        for (std::size_t i = 1; i < m_size; ++i) { if (m_data[i] < m_data[idx]) idx = i; }
        return idx;
    }

    template <typename Compare>
    std::size_t min_element_index(Compare comp) const noexcept {
        std::size_t idx = 0;
        for (std::size_t i = 1; i < m_size; ++i) { if (comp(m_data[i], m_data[idx])) idx = i; }
        return idx;
    }

    std::size_t max_element_index() const noexcept {
        std::size_t idx = 0;
        for (std::size_t i = 1; i < m_size; ++i) { if (m_data[idx] < m_data[i]) idx = i; }
        return idx;
    }

    template <typename Compare>
    std::size_t max_element_index(Compare comp) const noexcept {
        std::size_t idx = 0;
        for (std::size_t i = 1; i < m_size; ++i) { if (comp(m_data[idx], m_data[i])) idx = i; }
        return idx;
    }

    std::pair<std::size_t, std::size_t> min_max_index() const noexcept {
        std::size_t lo = 0, hi = 0;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (m_data[i] < m_data[lo]) lo = i;
            if (m_data[hi] < m_data[i]) hi = i;
        }

        return { lo, hi };
    }

    template <typename Compare>
    std::pair<std::size_t, std::size_t> min_max_index(Compare comp) const noexcept {
        std::size_t lo = 0, hi = 0;

        for (std::size_t i = 1; i < m_size; ++i) {
            if (comp(m_data[i], m_data[lo])) lo = i;
            if (comp(m_data[hi], m_data[i])) hi = i;
        }

        return { lo, hi };
    }

public:
    bool binary_search(const T& value) const { return std::binary_search(m_data, m_data + m_size, value); }

    template <typename Compare>
    bool binary_search(const T& value, Compare comp) const { return std::binary_search(m_data, m_data + m_size, value, comp); }

    std::size_t lower_bound(const T& value) const {
        auto it = std::lower_bound(m_data, m_data + m_size, value);
        return static_cast<std::size_t>(it - m_data);
    }

    template <typename Compare>
    std::size_t lower_bound(const T& value, Compare comp) const {
        auto it = std::lower_bound(m_data, m_data + m_size, value, comp);
        return static_cast<std::size_t>(it - m_data);
    }

    std::size_t upper_bound(const T& value) const {
        auto it = std::upper_bound(m_data, m_data + m_size, value);
        return static_cast<std::size_t>(it - m_data);
    }

    template <typename Compare>
    std::size_t upper_bound(const T& value, Compare comp) const {
        auto it = std::upper_bound(m_data, m_data + m_size, value, comp);
        return static_cast<std::size_t>(it - m_data);
    }

    std::pair<std::size_t, std::size_t> equal_range(const T& value) const {
        auto p = std::equal_range(m_data, m_data + m_size, value);
        return { static_cast<std::size_t>(p.first - m_data), static_cast<std::size_t>(p.second - m_data) };
    }

    template <typename Compare>
    std::pair<std::size_t, std::size_t> equal_range(const T& value, Compare comp) const {
        auto p = std::equal_range(m_data, m_data + m_size, value, comp);
        return { static_cast<std::size_t>(p.first - m_data), static_cast<std::size_t>(p.second - m_data) };
    }

public:
    template <typename Pred>
    bool all_of(Pred pred) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (!pred(m_data[i])) return false; }
        return true;
    }

    template <typename Pred>
    bool any_of(Pred pred) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) return true; }
        return false;
    }

    template <typename Pred>
    bool none_of(Pred pred) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) return false; }
        return true;
    }

public:
    void reverse() noexcept {
        if (m_size <= 1) return;
        std::size_t lo = 0, hi = m_size - 1;

        while (lo < hi) {
            T tmp = std::move(m_data[lo]);
            m_data[lo] = std::move(m_data[hi]);
            m_data[hi] = std::move(tmp);
            ++lo;
            --hi;
        }
    }

    void reverse(std::size_t start, std::size_t end_idx) noexcept {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t lo = start, hi = end_idx - 1;

        while (lo < hi) {
            T tmp = std::move(m_data[lo]);
            m_data[lo] = std::move(m_data[hi]);
            m_data[hi] = std::move(tmp);
            ++lo;
            --hi;
        }
    }

public:
    std::size_t find(const T& value) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (m_data[i] == value) return i; }
        return npos;
    }

    std::size_t find(const T& value, std::size_t start) const noexcept {
        for (std::size_t i = start; i < m_size; ++i) { if (m_data[i] == value) return i; }
        return npos;
    }

    template <typename Pred>
    std::size_t find_if(Pred pred) const noexcept {
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) return i; }
        return npos;
    }

    template <typename Pred>
    std::size_t find_if(Pred pred, std::size_t start) const noexcept {
        for (std::size_t i = start; i < m_size; ++i) { if (pred(m_data[i])) return i; }
        return npos;
    }

    std::size_t find_last(const T& value) const noexcept {
        for (std::size_t i = m_size; i > 0; --i) { if (m_data[i - 1] == value) return i - 1; }
        return npos;
    }

    std::size_t find_last(const T& value, std::size_t before) const noexcept {
        if (before > m_size) before = m_size;
        for (std::size_t i = before; i > 0; --i) { if (m_data[i - 1] == value) return i - 1; }
        return npos;
    }

    template <typename Pred>
    std::size_t find_last_if(Pred pred) const noexcept {
        for (std::size_t i = m_size; i > 0; --i) { if (pred(m_data[i - 1])) return i - 1; }
        return npos;
    }

    template <typename Pred>
    std::size_t find_last_if(Pred pred, std::size_t before) const noexcept {
        if (before > m_size) before = m_size;
        for (std::size_t i = before; i > 0; --i) { if (pred(m_data[i - 1])) return i - 1; }
        return npos;
    }

public:
    std::size_t fibonacci_search(const T& value) const {
        return fibonacci_search_range(value, 0, m_size, std::less<T>{});
    }

    template <typename Compare>
    std::size_t fibonacci_search(const T& value, Compare comp) const {
        return fibonacci_search_range(value, 0, m_size, comp);
    }

    std::size_t fibonacci_search(const T& value, std::size_t start, std::size_t end_idx) const {
        return fibonacci_search_range(value, start, end_idx, std::less<T>{});
    }

    template <typename Compare>
    std::size_t fibonacci_search(const T& value, std::size_t start, std::size_t end_idx, Compare comp) const {
        return fibonacci_search_range(value, start, end_idx, comp);
    }

public:
    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, std::size_t>::type
    interpolation_search(const T& value) const {
        return interpolation_search_range(value, 0, m_size);
    }

    template <typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, std::size_t>::type
    interpolation_search(const T& value, std::size_t start, std::size_t end_idx) const {
        return interpolation_search_range(value, start, end_idx);
    }

public:
    std::size_t exponential_search(const T& value) const {
        return exponential_search_impl(value, 0, m_size, std::less<T>{});
    }

    template <typename Compare>
    std::size_t exponential_search(const T& value, Compare comp) const {
        return exponential_search_impl(value, 0, m_size, comp);
    }

public:
    void rotate_left(std::size_t n = 1) {
        if (m_size <= 1) return;
        n %= m_size;
        if (n == 0) return;
        rotate_impl(n);
    }

    void rotate_right(std::size_t n = 1) {
        if (m_size <= 1) return;
        n %= m_size;
        if (n == 0) return;
        rotate_impl(m_size - n);
    }

public:
    template <typename A, typename B>
    static DynamicArray<std::pair<A, B>> zip(const DynamicArray<A>& a, const DynamicArray<B>& b) {
        std::size_t len = a.size() < b.size() ? a.size() : b.size();
        DynamicArray<std::pair<A, B>> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) { result.push_back(std::pair<A, B>(a[i], b[i])); }
        return result;
    }

    DynamicArray<std::pair<T, T>> zip(const DynamicArray& other) const {
        std::size_t len = m_size < other.m_size ? m_size : other.m_size;
        DynamicArray<std::pair<T, T>> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) { result.push_back(std::pair<T, T>(m_data[i], other.m_data[i])); }
        return result;
    }

    template <typename A, typename B, typename Func>
    static DynamicArray<decltype(std::declval<Func>()(std::declval<const A&>(), std::declval<const B&>()))> 
    zip_with(const DynamicArray<A>& a, const DynamicArray<B>& b, Func func) {
        using U = decltype(func(std::declval<const A&>(), std::declval<const B&>()));
        std::size_t len = a.size() < b.size() ? a.size() : b.size();
        DynamicArray<U> result;
        result.reserve(len);
        for (std::size_t i = 0; i < len; ++i) { result.push_back(func(a[i], b[i])); }
        return result;
    }

public:
    template <typename U = T>
    typename std::enable_if<
        is_custom_array_v<U>,
        typename U::value_type_array
    >::type
    flatten() const {
        using Inner = typename U::value_type_array;
        std::size_t total = 0;
        for (std::size_t i = 0; i < m_size; ++i) { total += m_data[i].size(); }
        Inner result;
        result.reserve(total);

        for (std::size_t i = 0; i < m_size; ++i) {
            for (std::size_t j = 0; j < m_data[i].size(); ++j) { result.push_back(m_data[i][j]); }
        }

        return result;
    }

    template <typename Func>
    auto flat_map(Func func) const -> DynamicArray<typename decltype(func(std::declval<const T&>()))::value_type> {
        using Inner = decltype(func(std::declval<const T&>()));
        using U = typename Inner::value_type;

        std::size_t total = 0;
        DynamicArray<Inner> mapped;
        mapped.reserve(m_size);

        for (std::size_t i = 0; i < m_size; ++i) {
            mapped.push_back(func(m_data[i]));
            total += mapped.back().size();
        }

        DynamicArray<U> result;
        result.reserve(total);

        for (std::size_t i = 0; i < mapped.size(); ++i) {
            for (std::size_t j = 0; j < mapped[i].size(); ++j) {
                result.push_back(std::move(mapped[i][j]));
            }
        }

        return result;
    }

public:
    template <typename Pred>
    DynamicArray take_while(Pred pred) const {
        std::size_t n = 0;
        while (n < m_size && pred(m_data[n])) ++n;
        DynamicArray result;
        if (n == 0) return result;
        result.reserve(n);
        for (std::size_t i = 0; i < n; ++i) { result.push_back(m_data[i]); }
        return result;
    }

    template <typename Pred>
    DynamicArray drop_while(Pred pred) const {
        std::size_t n = 0;
        while (n < m_size && pred(m_data[n])) ++n;
        std::size_t count = m_size - n;
        DynamicArray result;
        if (count == 0) return result;
        result.reserve(count);
        for (std::size_t i = n; i < m_size; ++i) { result.push_back(m_data[i]); }
        return result;
    }

public:
    template <typename U, typename Func>
    DynamicArray<U> scan(U init, Func func) const {
        DynamicArray<U> result;
        result.reserve(m_size);

        for (std::size_t i = 0; i < m_size; ++i) {
            init = func(std::move(init), m_data[i]);
            result.push_back(init);
        }

        return result;
    }

    template <typename Func>
    DynamicArray scan(Func func) const {
        DynamicArray result;
        if (m_size == 0) return result;
        result.reserve(m_size);
        T acc = m_data[0];
        result.push_back(acc);

        for (std::size_t i = 1; i < m_size; ++i) {
            acc = func(std::move(acc), m_data[i]);
            result.push_back(acc);
        }

        return result;
    }

    DynamicArray prefix_sum() const {
        DynamicArray result;
        if (m_size == 0) return result;
        result.reserve(m_size);
        T acc = m_data[0];
        result.push_back(acc);

        for (std::size_t i = 1; i < m_size; ++i) {
            acc = acc + m_data[i];
            result.push_back(acc);
        }

        return result;
    }

public:
    template <typename Pred>
    std::pair<DynamicArray, DynamicArray> partition(Pred pred) const {
        DynamicArray matching, non_matching;

        for (std::size_t i = 0; i < m_size; ++i) {
            if (pred(m_data[i])) { matching.push_back(m_data[i]); }
            else                 { non_matching.push_back(m_data[i]); }
        }

        return { std::move(matching), std::move(non_matching) };
    }

    template <typename Pred>
    std::size_t partition_in_place(Pred pred) {
        std::size_t write = 0;

        for (std::size_t read = 0; read < m_size; ++read) {
            if (pred(m_data[read])) {
                if (write != read) {
                    T tmp = std::move(m_data[write]);
                    m_data[write] = std::move(m_data[read]);
                    m_data[read] = std::move(tmp);
                }
                ++write;
            }
        }

        return write;
    }

    template <typename Pred>
    std::size_t stable_partition_in_place(Pred pred) {
        auto it = std::stable_partition(m_data, m_data + m_size, pred);
        return static_cast<std::size_t>(it - m_data);
    }

public:
    template <typename Func>
    DynamicArray<DynamicArray<T>> group_by(Func func) const {
        using K = decltype(func(std::declval<const T&>()));
        DynamicArray<K> keys;
        DynamicArray<DynamicArray<T>> groups;

        for (std::size_t i = 0; i < m_size; ++i) {
            K key = func(m_data[i]);
            std::size_t idx = keys.find(key);

            if (idx == npos) {
                keys.push_back(std::move(key));
                groups.push_back(DynamicArray<T>());
                groups.back().push_back(m_data[i]);
            } else {
                groups[idx].push_back(m_data[i]);
            }
        }

        return groups;
    }

public:
    DynamicArray union_with(const DynamicArray& other) const {
        DynamicArray result = distinct();

        for (std::size_t i = 0; i < other.m_size; ++i) {
            if (!result.contains(other.m_data[i])) { result.push_back(other.m_data[i]); }
        }

        return result;
    }

    DynamicArray intersect(const DynamicArray& other) const {
        DynamicArray result;

        for (std::size_t i = 0; i < m_size; ++i) {
            if (other.contains(m_data[i]) && !result.contains(m_data[i])) {
                result.push_back(m_data[i]);
            }
        }

        return result;
    }

    DynamicArray difference(const DynamicArray& other) const {
        DynamicArray result;

        for (std::size_t i = 0; i < m_size; ++i) {
            if (!other.contains(m_data[i]) && !result.contains(m_data[i])) {
                result.push_back(m_data[i]);
            }
        }

        return result;
    }

public:
    template <typename Func>
    DynamicArray& for_each(Func func) {
        for (std::size_t i = 0; i < m_size; ++i) { func(m_data[i]); }
        return *this;
    }

    template <typename Func>
    const DynamicArray& for_each(Func func) const {
        for (std::size_t i = 0; i < m_size; ++i) { func(m_data[i]); }
        return *this;
    }

    template <typename Func>
    DynamicArray& for_each_indexed(Func func) {
        for (std::size_t i = 0; i < m_size; ++i) { func(i, m_data[i]); }
        return *this;
    }

    template <typename Func>
    const DynamicArray& for_each_indexed(Func func) const {
        for (std::size_t i = 0; i < m_size; ++i) { func(i, m_data[i]); }
        return *this;
    }

public:
    template <typename U, typename Func>
    U reduce(U init, Func func) const {
        for (std::size_t i = 0; i < m_size; ++i) { init = func(std::move(init), m_data[i]); }
        return init;
    }

    template <typename Func>
    T reduce(Func func) const {
        T acc = m_data[0];
        for (std::size_t i = 1; i < m_size; ++i) { acc = func(std::move(acc), m_data[i]); }
        return acc;
    }

    template <typename U, typename Func>
    U fold_right(U init, Func func) const {
        for (std::size_t i = m_size; i > 0; --i) { init = func(m_data[i - 1], std::move(init)); }
        return init;
    }

    template <typename Func>
    T fold_right(Func func) const {
        T acc = m_data[m_size - 1];
        for (std::size_t i = m_size - 1; i > 0; --i) { acc = func(m_data[i - 1], std::move(acc)); }
        return acc;
    }

public:
    template <typename Pred>
    DynamicArray filter(Pred pred) const {
        DynamicArray result;
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) result.push_back(m_data[i]); }
        return result;
    }

    template <typename Pred>
    DynamicArray filter_indexed(Pred pred) const {
        DynamicArray result;
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(i, m_data[i])) result.push_back(m_data[i]); }
        return result;
    }

public:
    template <typename Func>
    auto map(Func func) const -> DynamicArray<decltype(func(std::declval<const T&>()))> {
        using U = decltype(func(std::declval<const T&>()));
        DynamicArray<U> result;
        result.reserve(m_size);
        for (std::size_t i = 0; i < m_size; ++i) { result.push_back(func(m_data[i])); }
        return result;
    }

    template <typename Func>
    auto map_indexed(Func func) const -> DynamicArray<decltype(func(std::size_t{}, std::declval<const T&>()))> {
        using U = decltype(func(std::size_t{}, std::declval<const T&>()));
        DynamicArray<U> result;
        result.reserve(m_size);
        for (std::size_t i = 0; i < m_size; ++i) { result.push_back(func(i, m_data[i])); }
        return result;
    }

public:
    void shuffle() {
        if (m_size <= 1) return;

        for (std::size_t i = m_size - 1; i > 0; --i) {
            std::size_t j = random_int<std::size_t>(0, i);
            T tmp = std::move(m_data[i]);
            m_data[i] = std::move(m_data[j]);
            m_data[j] = std::move(tmp);
        }
    }

    void shuffle(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t len = end_idx - start;
        if (len <= 1) return;

        for (std::size_t i = len - 1; i > 0; --i) {
            std::size_t j = random_int<std::size_t>(0, i);
            T tmp = std::move(m_data[start + i]);
            m_data[start + i] = std::move(m_data[start + j]);
            m_data[start + j] = std::move(tmp);
        }
    }

    void unsecure_shuffle() {
        if (m_size <= 1) return;

        for (std::size_t i = m_size - 1; i > 0; --i) {
            std::size_t j = unsecure_random_int<std::size_t>(0, i);
            T tmp = std::move(m_data[i]);
            m_data[i] = std::move(m_data[j]);
            m_data[j] = std::move(tmp);
        }
    }

    void unsecure_shuffle(std::size_t start, std::size_t end_idx) {
        if (start >= end_idx || start >= m_size) return;
        if (end_idx > m_size) end_idx = m_size;
        std::size_t len = end_idx - start;
        if (len <= 1) return;

        for (std::size_t i = len - 1; i > 0; --i) {
            std::size_t j = unsecure_random_int<std::size_t>(0, i);
            T tmp = std::move(m_data[start + i]);
            m_data[start + i] = std::move(m_data[start + j]);
            m_data[start + j] = std::move(tmp);
        }
    }

public:
    explicit operator std::vector<T>() const { return std::vector<T>(m_data, m_data + m_size); }
    explicit operator std::deque<T>() const { return std::deque<T>(m_data, m_data + m_size); }
    explicit operator std::list<T>() const { return std::list<T>(m_data, m_data + m_size); }
    explicit operator std::forward_list<T>() const { return std::forward_list<T>(m_data, m_data + m_size); }
    explicit operator std::queue<T>() const { return std::queue<T>(std::deque<T>(m_data, m_data + m_size)); }
    explicit operator std::stack<T>() const { return std::stack<T>(std::deque<T>(m_data, m_data + m_size)); }
    explicit operator std::priority_queue<T>() const { return std::priority_queue<T>(std::less<T>(), std::vector<T>(m_data, m_data + m_size)); }

    template <std::size_t N>
    explicit operator std::array<T, N>() const {
        std::array<T, N> result{};
        std::size_t count = N < m_size ? N : m_size;
        std::copy(m_data, m_data + count, result.begin());
        return result;
    }

private:
    DynamicArray<std::size_t> collect_indices(const T& value) const {
        DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < m_size; ++i) { if (m_data[i] == value) idx.push_back(i); }
        return idx;
    }

    template <typename Pred>
    DynamicArray<std::size_t> collect_indices_if(Pred pred) const {
        DynamicArray<std::size_t> idx;
        for (std::size_t i = 0; i < m_size; ++i) { if (pred(m_data[i])) idx.push_back(i); }
        return idx;
    }

    static void apply_direction_filter(DynamicArray<std::size_t>& idx, std::size_t count, Direction dir) {
        if (dir == Direction::All || count == 0 || count >= idx.size()) return;

        if (dir == Direction::Front) {
            idx.resize(count);
        } else {
            std::size_t start = idx.size() - count;
            for (std::size_t i = 0; i < count; ++i) { idx[i] = idx[start + i]; }
            idx.resize(count);
        }
    }

    void remove_at_indices(const std::size_t* idx, std::size_t idx_count) {
        if (idx_count == 0) return;
        std::size_t j = 0;
        std::size_t write = idx[0];

        for (std::size_t read = idx[0]; read < m_size; ++read) {
            if (j < idx_count && read == idx[j]) { ++j; }
            else { m_data[write++] = std::move(m_data[read]); }
        }

        m_size = write;
    }

    void remove_at_indices_unordered(const std::size_t* idx, std::size_t idx_count) {
        for (std::size_t i = idx_count; i > 0; --i) {
            std::size_t index = idx[i - 1];
            if (index < m_size - 1) { m_data[index] = std::move(m_data[m_size - 1]); }
            --m_size;
        }
    }

    DynamicArray pop_at_indices(const std::size_t* idx, std::size_t idx_count) {
        DynamicArray result;
        result.reserve(idx_count);
        std::size_t j = 0;
        std::size_t write = 0;

        for (std::size_t read = 0; read < m_size; ++read) {
            if (j < idx_count && read == idx[j]) {
                result.m_data[result.m_size++] = std::move(m_data[read]);
                ++j;
            } else {
                if (write != read) m_data[write] = std::move(m_data[read]);
                ++write;
            }
        }

        m_size = write;
        return result;
    }

    DynamicArray pop_at_indices_unordered(const std::size_t* idx, std::size_t idx_count) {
        DynamicArray result;
        result.reserve(idx_count);

        for (std::size_t i = idx_count; i > 0; --i) {
            std::size_t index = idx[i - 1];
            result.push_back(std::move(m_data[index]));
            if (index < m_size - 1) { m_data[index] = std::move(m_data[m_size - 1]); }
            --m_size;
        }

        return result;
    }

    void replace_at_indices(const std::size_t* idx, std::size_t idx_count, const T& value) {
        for (std::size_t i = 0; i < idx_count; ++i) { m_data[idx[i]] = value; }
    }

    void rotate_impl(std::size_t pivot) {
        auto rev = [this](std::size_t lo, std::size_t hi) {
            while (lo < hi) {
                T tmp = std::move(m_data[lo]);
                m_data[lo] = std::move(m_data[hi]);
                m_data[hi] = std::move(tmp);
                ++lo;
                --hi;
            }
        };

        rev(0, pivot - 1);
        rev(pivot, m_size - 1);
        rev(0, m_size - 1);
    }

    template <typename Compare>
    void merge_sort_impl(T* arr, T* buf, std::size_t lo, std::size_t hi, Compare comp) {
        if (hi - lo <= 1) return;
        std::size_t mid = lo + (hi - lo) / 2;
        merge_sort_impl(arr, buf, lo, mid, comp);
        merge_sort_impl(arr, buf, mid, hi, comp);
        std::size_t i = lo, j = mid, k = lo;

        while (i < mid && j < hi) {
            if (comp(arr[j], arr[i])) { buf[k++] = std::move(arr[j++]); }
            else                      { buf[k++] = std::move(arr[i++]); }
        }

        while (i < mid) { buf[k++] = std::move(arr[i++]); }
        while (j < hi)  { buf[k++] = std::move(arr[j++]); }
        for (std::size_t x = lo; x < hi; ++x) { arr[x] = std::move(buf[x]); }
    }

    template <typename Compare>
    std::size_t qs_partition(std::size_t lo, std::size_t hi, Compare comp) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (comp(m_data[mid], m_data[lo])) { T t = std::move(m_data[lo]); m_data[lo] = std::move(m_data[mid]); m_data[mid] = std::move(t); }
        if (comp(m_data[hi],  m_data[lo])) { T t = std::move(m_data[lo]); m_data[lo] = std::move(m_data[hi]);  m_data[hi]  = std::move(t); }
        if (comp(m_data[mid], m_data[hi])) { T t = std::move(m_data[mid]); m_data[mid] = std::move(m_data[hi]); m_data[hi] = std::move(t); }
        T& pivot = m_data[hi];
        std::size_t i = lo;

        for (std::size_t j = lo; j < hi; ++j) {
            if (comp(m_data[j], pivot)) {
                T t = std::move(m_data[i]); m_data[i] = std::move(m_data[j]); m_data[j] = std::move(t);
                ++i;
            }
        }
        T t = std::move(m_data[i]); m_data[i] = std::move(m_data[hi]); m_data[hi] = std::move(t);
        return i;
    }

    template <typename Compare>
    void quick_sort_impl(std::size_t lo, std::size_t hi, Compare comp) {
        while (lo < hi) {
            if (hi - lo < 16) {
                for (std::size_t i = lo + 1; i <= hi; ++i) {
                    T key = std::move(m_data[i]);
                    std::size_t j = i;
                    while (j > lo && comp(key, m_data[j - 1])) {
                        m_data[j] = std::move(m_data[j - 1]);
                        --j;
                    }
                    m_data[j] = std::move(key);
                }
                return;
            }

            std::size_t p = qs_partition(lo, hi, comp);
            
            if (p - lo < hi - p) {
                if (p > lo) quick_sort_impl(lo, p - 1, comp);
                lo = p + 1;
            } else {
                if (p < hi) quick_sort_impl(p + 1, hi, comp);
                hi = (p == 0) ? 0 : p - 1;
                if (p == 0) return;
            }
        }
    }

    template <typename Compare>
    void sift_down(std::size_t start, std::size_t i, std::size_t n, Compare comp) {
        while (true) {
            std::size_t largest = i;
            std::size_t l = 2 * (i - start) + 1 + start;
            std::size_t r = l + 1;
            if (l < n && comp(m_data[largest], m_data[l])) largest = l;
            if (r < n && comp(m_data[largest], m_data[r])) largest = r;
            if (largest == i) return;
            T t = std::move(m_data[i]); m_data[i] = std::move(m_data[largest]); m_data[largest] = std::move(t);
            i = largest;
        }
    }

    template <typename Compare>
    void heap_sort_range(std::size_t lo, std::size_t hi, Compare comp) {
        std::size_t n = hi - lo;
        if (n <= 1) return;
        for (std::size_t i = lo + n / 2; i > lo; --i) { sift_down(lo, i - 1, hi, comp); }
     
        for (std::size_t end = hi - 1; end > lo; --end) {
            T t = std::move(m_data[lo]); m_data[lo] = std::move(m_data[end]); m_data[end] = std::move(t);
            sift_down(lo, lo, end, comp);
        }
    }

    void counting_sort_range(std::size_t lo, std::size_t hi) {
        if (lo >= hi || lo >= m_size) return;
        if (hi > m_size) hi = m_size;
        if (hi - lo <= 1) return;

        T min_val = m_data[lo], max_val = m_data[lo];
        for (std::size_t i = lo + 1; i < hi; ++i) {
            if (m_data[i] < min_val) min_val = m_data[i];
            if (max_val < m_data[i]) max_val = m_data[i];
        }

        using UT = typename std::make_unsigned<T>::type;
        std::size_t range = static_cast<std::size_t>(static_cast<UT>(max_val) - static_cast<UT>(min_val)) + 1;

        std::size_t* counts = new std::size_t[range]();
        for (std::size_t i = lo; i < hi; ++i) {
            ++counts[static_cast<std::size_t>(static_cast<UT>(m_data[i]) - static_cast<UT>(min_val))];
        }

        std::size_t idx = lo;
        for (std::size_t i = 0; i < range; ++i) {
            T val = static_cast<T>(static_cast<UT>(min_val) + static_cast<UT>(i));
            for (std::size_t c = 0; c < counts[i]; ++c) { m_data[idx++] = val; }
        }

        delete[] counts;
    }

    void counting_sort_range_desc(std::size_t lo, std::size_t hi) {
        if (lo >= hi || lo >= m_size) return;
        if (hi > m_size) hi = m_size;
        if (hi - lo <= 1) return;

        T min_val = m_data[lo], max_val = m_data[lo];
        for (std::size_t i = lo + 1; i < hi; ++i) {
            if (m_data[i] < min_val) min_val = m_data[i];
            if (max_val < m_data[i]) max_val = m_data[i];
        }

        using UT = typename std::make_unsigned<T>::type;
        std::size_t range = static_cast<std::size_t>(static_cast<UT>(max_val) - static_cast<UT>(min_val)) + 1;

        std::size_t* counts = new std::size_t[range]();
        for (std::size_t i = lo; i < hi; ++i) {
            ++counts[static_cast<std::size_t>(static_cast<UT>(m_data[i]) - static_cast<UT>(min_val))];
        }

        std::size_t idx = lo;
        for (std::size_t i = range; i > 0; --i) {
            T val = static_cast<T>(static_cast<UT>(min_val) + static_cast<UT>(i - 1));
            for (std::size_t c = 0; c < counts[i - 1]; ++c) { m_data[idx++] = val; }
        }

        delete[] counts;
    }

    template <typename Compare>
    void bitonic_sort_impl(std::size_t lo, std::size_t cnt, bool ascending, Compare comp) {
        if (cnt <= 1) return;
        std::size_t half = cnt / 2;
        bitonic_sort_impl(lo, half, true, comp);
        bitonic_sort_impl(lo + half, half, false, comp);
        bitonic_merge(lo, cnt, ascending, comp);
    }

    template <typename Compare>
    void bitonic_merge(std::size_t lo, std::size_t cnt, bool ascending, Compare comp) {
        if (cnt <= 1) return;
        std::size_t half = cnt / 2;

        for (std::size_t i = lo; i < lo + half; ++i) {
            bool should_swap = ascending ? comp(m_data[i + half], m_data[i]) : comp(m_data[i], m_data[i + half]);

            if (should_swap) {
                T t = std::move(m_data[i]);
                m_data[i] = std::move(m_data[i + half]);
                m_data[i + half] = std::move(t);
            }
        }

        bitonic_merge(lo, half, ascending, comp);
        bitonic_merge(lo + half, half, ascending, comp);
    }

    void radix_sort_lsd_range(std::size_t lo, std::size_t hi) {
        if (lo >= hi || lo >= m_size) return;
        if (hi > m_size) hi = m_size;
        std::size_t n = hi - lo;
        if (n <= 1) return;
        using UT = typename std::make_unsigned<T>::type;
        constexpr UT sign_bit = std::is_signed<T>::value ? (static_cast<UT>(1) << (sizeof(T) * 8 - 1)) : 0;
        constexpr int RADIX_BITS = 8;
        constexpr std::size_t BUCKETS = 1 << RADIX_BITS;
        constexpr UT MASK = static_cast<UT>(BUCKETS - 1);
        constexpr int PASSES = (sizeof(T) * 8 + RADIX_BITS - 1) / RADIX_BITS;
        T* buf = new T[n];
        T* src = m_data + lo;
        T* dst = buf;

        for (int pass = 0; pass < PASSES; ++pass) {
            int shift = pass * RADIX_BITS;
            std::size_t counts[BUCKETS] = {};

            for (std::size_t i = 0; i < n; ++i) {
                UT key = (static_cast<UT>(src[i]) ^ sign_bit) >> shift;
                ++counts[key & MASK];
            }

            std::size_t offsets[BUCKETS];
            offsets[0] = 0;
            for (std::size_t i = 1; i < BUCKETS; ++i) { offsets[i] = offsets[i - 1] + counts[i - 1]; }

            for (std::size_t i = 0; i < n; ++i) {
                UT key = (static_cast<UT>(src[i]) ^ sign_bit) >> shift;
                dst[offsets[key & MASK]++] = std::move(src[i]);
            }

            T* tmp = src; src = dst; dst = tmp;
        }

        if (src == buf) {
            for (std::size_t i = 0; i < n; ++i) { m_data[lo + i] = std::move(buf[i]); }
        }

        delete[] buf;
    }

    void radix_sort_msd_range(std::size_t lo, std::size_t hi) {
        if (lo >= hi || lo >= m_size) return;
        if (hi > m_size) hi = m_size;
        std::size_t n = hi - lo;
        if (n <= 1) return;
        using UT = typename std::make_unsigned<T>::type;
        constexpr UT sign_bit = std::is_signed<T>::value ? (static_cast<UT>(1) << (sizeof(T) * 8 - 1)) : 0;
        constexpr int RADIX_BITS = 8;
        int top_bit = static_cast<int>(sizeof(T) * 8 - RADIX_BITS);
        T* buf = new T[n];
        radix_sort_msd_impl(lo, hi, top_bit, sign_bit, buf, lo);
        delete[] buf;
    }

    template <typename U = T, typename = typename std::enable_if<std::is_integral<U>::value>::type>
    void radix_sort_msd_impl(
        std::size_t lo, std::size_t hi, int shift,
        typename std::make_unsigned<U>::type sign_bit,
        U* buf, std::size_t buf_offset
    ) {
        using UT = typename std::make_unsigned<U>::type;
        constexpr int RADIX_BITS = 8;
        constexpr std::size_t BUCKETS = 1 << RADIX_BITS;
        constexpr UT MASK = static_cast<UT>(BUCKETS - 1);
        std::size_t n = hi - lo;

        if (n <= 16 || shift < 0) {
            for (std::size_t i = lo + 1; i < hi; ++i) {
                T key = std::move(m_data[i]);
                std::size_t j = i;

                while (j > lo && m_data[j - 1] > key) {
                    m_data[j] = std::move(m_data[j - 1]);
                    --j;
                }

                m_data[j] = std::move(key);
            }

            return;
        }

        std::size_t counts[BUCKETS] = {};

        for (std::size_t i = lo; i < hi; ++i) {
            UT key = (static_cast<UT>(m_data[i]) ^ sign_bit) >> shift;
            ++counts[key & MASK];
        }

        std::size_t offsets[BUCKETS];
        offsets[0] = 0;
        for (std::size_t i = 1; i < BUCKETS; ++i) { offsets[i] = offsets[i - 1] + counts[i - 1]; }
        std::size_t saved_offsets[BUCKETS];
        for (std::size_t i = 0; i < BUCKETS; ++i) { saved_offsets[i] = offsets[i]; }

        for (std::size_t i = lo; i < hi; ++i) {
            UT key = (static_cast<UT>(m_data[i]) ^ sign_bit) >> shift;
            buf[offsets[key & MASK]++] = std::move(m_data[i]);
        }

        for (std::size_t i = 0; i < n; ++i) { m_data[lo + i] = std::move(buf[i]); }

        for (std::size_t b = 0; b < BUCKETS; ++b) {
            std::size_t bucket_lo = lo + saved_offsets[b];
            std::size_t bucket_hi = lo + (b + 1 < BUCKETS ? saved_offsets[b + 1] : n);
            
            if (bucket_hi - bucket_lo > 1) {
                radix_sort_msd_impl(bucket_lo, bucket_hi, shift - RADIX_BITS, sign_bit, buf, buf_offset);
            }
        }
    }

    template <typename Compare>
    std::size_t fibonacci_search_range(const T& value, std::size_t lo, std::size_t hi, Compare comp) const {
        if (lo >= hi || lo >= m_size) return npos;
        if (hi > m_size) hi = m_size;
        std::size_t n = hi - lo;

        std::size_t fib2 = 0;   
        std::size_t fib1 = 1;   
        std::size_t fib  = 1;  

        while (fib < n) {
            fib2 = fib1;
            fib1 = fib;
            fib  = fib1 + fib2;
        }

        std::size_t offset = lo;

        while (fib > 1) {
            std::size_t i = offset + fib2;
            if (i >= hi) i = hi - 1;

            if (comp(m_data[i], value)) {
                fib  = fib1;
                fib1 = fib2;
                fib2 = fib - fib1;
                offset = i + 1;
            } else if (comp(value, m_data[i])) {
                fib  = fib2;
                fib1 = fib1 - fib2;
                fib2 = fib - fib1;
            } else {
                return i;
            }
        }

        if (fib1 && offset < hi && !(comp(m_data[offset], value)) && !(comp(value, m_data[offset]))) {
            return offset;
        }

        return npos;
    }

    std::size_t interpolation_search_range(const T& value, std::size_t lo_init, std::size_t hi_init) const {
        if (lo_init >= hi_init || lo_init >= m_size) return npos;
        if (hi_init > m_size) hi_init = m_size;

        std::size_t lo = lo_init;
        std::size_t hi = hi_init - 1;

        while (lo <= hi && value >= m_data[lo] && value <= m_data[hi]) {
            if (lo == hi) {
                if (!(m_data[lo] < value) && !(value < m_data[lo])) return lo;
                return npos;
            }

            double denom = static_cast<double>(m_data[hi]) - static_cast<double>(m_data[lo]);
            double numer = static_cast<double>(value)       - static_cast<double>(m_data[lo]);
            std::size_t pos = lo + static_cast<std::size_t>((numer / denom) * static_cast<double>(hi - lo));
            if (pos > hi) pos = hi;

            if (m_data[pos] < value) {
                lo = pos + 1;
            } else if (value < m_data[pos]) {
                if (pos == 0) return npos;
                hi = pos - 1;
            } else {
                return pos;
            }
        }

        return npos;
    }

    template <typename Compare>
    std::size_t exponential_search_impl(const T& value, std::size_t lo, std::size_t hi, Compare comp) const {
        if (lo >= hi || lo >= m_size) return npos;
        if (hi > m_size) hi = m_size;
        if (!(comp(m_data[lo], value)) && !(comp(value, m_data[lo]))) return lo;
        std::size_t bound = 1;
        while (lo + bound < hi && comp(m_data[lo + bound], value)) { bound *= 2; }
        std::size_t search_lo = lo + bound / 2;
        std::size_t search_hi = (lo + bound < hi) ? lo + bound + 1 : hi;

        while (search_lo < search_hi) {
            std::size_t mid = search_lo + (search_hi - search_lo) / 2;

            if (comp(m_data[mid], value)) {
                search_lo = mid + 1;
            } else if (comp(value, m_data[mid])) {
                search_hi = mid;
            } else {
                return mid;
            }
        }

        return npos;
    }
};

template <typename T>
bool operator==(const DynamicArray<T>& lhs, const DynamicArray<T>& rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) if (!(lhs[i] == rhs[i])) return false;
    return true;
}

template <typename T>
bool operator!=(const DynamicArray<T>& lhs, const DynamicArray<T>& rhs) noexcept {
    return !(lhs == rhs);
}

template<class T>
std::ostream& print_element(std::ostream& os, const T& value) {
    if (is_streamable_v<T>) {
        os << value;
    } else {
        os << "OBJECT";
    }
    return os;
}

template<class T>
std::ostream& operator<<(std::ostream& os, const ArrayView<T>& view) {
    os << "ArrayView[\n";
    for (std::size_t i = 0; i < view.size(); ++i) {
        os << "    " << i << ": ";
        print_element(os, view[i]);
        if (i < view.size() - 1) os << ",";
        os << "\n";
    }
    return os << "]";
}

template<class T>
std::ostream& operator<<(std::ostream& os, const DynamicArray<T>& array) {
    os << "[\n";
    for (std::size_t i = 0; i < array.size(); ++i) {
        os << "    " << i << ": ";
        print_element(os, array[i]);
        if (i < array.size() - 1) os << ",";
        os << "\n";
    }
    return os << "]";
}

template <typename T>
class WhereProxy {
    friend class DynamicArray<T>;
    DynamicArray<T>& m_arr;
    DynamicArray<std::size_t> m_indices;
    WhereProxy(DynamicArray<T>& arr, DynamicArray<std::size_t>&& indices) : m_arr(arr), m_indices(std::move(indices)) {}

public:
    WhereProxy& first(std::size_t n = 1) {
        if (n < m_indices.size()) { m_indices.resize(n); }
        return *this;
    }

    WhereProxy& last(std::size_t n = 1) {
        if (n < m_indices.size()) {
            std::size_t start = m_indices.size() - n;
            for (std::size_t i = 0; i < n; ++i) { m_indices[i] = m_indices[start + i]; }
            m_indices.resize(n);
        }

        return *this;
    }

    WhereProxy& range(std::size_t start, std::size_t end_idx) {
        std::size_t write = 0;

        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (m_indices[i] >= start && m_indices[i] < end_idx) { m_indices[write++] = m_indices[i]; }
        }

        m_indices.resize(write);
        return *this;
    }

    WhereProxy& skip(std::size_t n = 1) {
        if (n >= m_indices.size()) {
            m_indices.clear();
        } else {
            std::size_t new_count = m_indices.size() - n;
            for (std::size_t i = 0; i < new_count; ++i) { m_indices[i] = m_indices[n + i]; }
            m_indices.resize(new_count);
        }

        return *this;
    }

public:
    std::size_t count() const noexcept { return m_indices.size(); }
    bool empty() const noexcept { return m_indices.empty(); }
    ArrayView<std::size_t> indices() const noexcept { return ArrayView<std::size_t>(m_indices.data(), m_indices.size()); }
    DynamicArray<std::size_t> indices_copy() const { return DynamicArray<std::size_t>(m_indices); }

    DynamicArray<std::size_t> release_indices() noexcept {
        DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.deallocate();       
        return out;
    }

    DynamicArray<std::size_t> extract_indices() noexcept {
        DynamicArray<std::size_t> out = std::move(m_indices);
        m_indices.clear();       
        return out;
    }

    DynamicArray<T> values() const {
        DynamicArray<T> result;
        result.reserve(m_indices.size());
        for (std::size_t i = 0; i < m_indices.size(); ++i) { result.push_back(m_arr.m_data[m_indices[i]]); }
        return result;
    }

    T* first_value() noexcept { return m_indices.empty() ? nullptr : &m_arr.m_data[m_indices.front()]; }
    const T* first_value() const noexcept { return m_indices.empty() ? nullptr : &m_arr.m_data[m_indices.front()]; }
    T* last_value() noexcept { return m_indices.empty() ? nullptr : &m_arr.m_data[m_indices.back()]; }
    const T* last_value() const noexcept { return m_indices.empty() ? nullptr : &m_arr.m_data[m_indices.back()]; }

    template <typename Func>
    WhereProxy& for_each(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) { func(m_arr.m_data[m_indices[i]]); }
        return *this;
    }

    template <typename Func>
    WhereProxy& transform(Func func) {
        for (std::size_t i = 0; i < m_indices.size(); ++i) { func(m_arr.m_data[m_indices[i]]); }
        return *this;
    }

public:
    template <typename Pred>
    WhereProxy& where(Pred pred) {
        std::size_t write = 0;

        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (pred(m_arr.m_data[m_indices[i]])) { m_indices[write++] = m_indices[i]; }
        }

        m_indices.resize(write);
        return *this;
    }

    WhereProxy& even() {
        std::size_t write = 0;

        for (std::size_t i = 0; i < m_indices.size(); i += 2) {
            m_indices[write++] = m_indices[i];
        }

        m_indices.resize(write);
        return *this;
    }

    WhereProxy& odd() {
        std::size_t write = 0;

        for (std::size_t i = 1; i < m_indices.size(); i += 2) {
            m_indices[write++] = m_indices[i];
        }

        m_indices.resize(write);
        return *this;
    }

    WhereProxy& step(std::size_t n) {
        if (n <= 1) return *this;
        std::size_t write = 0;

        for (std::size_t i = 0; i < m_indices.size(); i += n) {
            m_indices[write++] = m_indices[i];
        }

        m_indices.resize(write);
        return *this;
    }

    WhereProxy& except(const DynamicArray<std::size_t>& positions) {
        std::size_t write = 0;

        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            if (!positions.contains(i)) { m_indices[write++] = m_indices[i]; }
        }

        m_indices.resize(write);
        return *this;
    }

public:
    WhereProxy& at(std::size_t n) {
        if (n < m_indices.size()) {
            m_indices[0] = m_indices[n];
            m_indices.resize(1);
        } else {
            m_indices.clear();
        }

        return *this;
    }

    WhereProxy& slice(std::size_t start, std::size_t end_idx) {
        if (start >= m_indices.size()) { m_indices.clear(); return *this; }
        if (end_idx > m_indices.size()) end_idx = m_indices.size();
        if (start >= end_idx) { m_indices.clear(); return *this; }
        std::size_t count = end_idx - start;
        for (std::size_t i = 0; i < count; ++i) { m_indices[i] = m_indices[start + i]; }
        m_indices.resize(count);
        return *this;
    }

public:
    std::size_t remove() {
        std::size_t n = m_indices.size();
        m_arr.remove_at_indices(m_indices.data(), n);
        return n;
    }

    std::size_t remove_unordered() {
        std::size_t n = m_indices.size();
        m_arr.remove_at_indices_unordered(m_indices.data(), n);
        return n;
    }

    DynamicArray<T> pop() { return m_arr.pop_at_indices(m_indices.data(), m_indices.size()); }
    DynamicArray<T> pop_unordered() { return m_arr.pop_at_indices_unordered(m_indices.data(), m_indices.size()); }
    void replace(const T& value) { m_arr.replace_at_indices(m_indices.data(), m_indices.size(), value); }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    replace(U&& value) {
        T converted = static_cast<T>(std::forward<U>(value));
        m_arr.replace_at_indices(m_indices.data(), m_indices.size(), converted);
    }

    DynamicArray<T> swap_with(const T& value) {
        DynamicArray<T> old_values;
        old_values.reserve(m_indices.size());

        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            old_values.push_back(std::move(m_arr.m_data[m_indices[i]]));
            m_arr.m_data[m_indices[i]] = value;
        }

        return old_values;
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value,
        DynamicArray<T>
    >::type
    swap_with(U&& value) {
        T converted = static_cast<T>(std::forward<U>(value));
        return swap_with(converted);
    }

    WhereProxy& shuffle() {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;

        for (std::size_t i = n - 1; i > 0; --i) {
            std::size_t j = random_int<std::size_t>(0, i);
            T tmp = std::move(m_arr.m_data[m_indices[i]]);
            m_arr.m_data[m_indices[i]] = std::move(m_arr.m_data[m_indices[j]]);
            m_arr.m_data[m_indices[j]] = std::move(tmp);
        }

        return *this;
    }

    WhereProxy& sort() {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        DynamicArray<T> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) { tmp.push_back(std::move(m_arr.m_data[m_indices[i]])); }
        std::sort(tmp.data(), tmp.data() + n);
        for (std::size_t i = 0; i < n; ++i) { m_arr.m_data[m_indices[i]] = std::move(tmp[i]); }
        return *this;
    }

    template <typename Compare>
    WhereProxy& sort(Compare comp) {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        DynamicArray<T> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) { tmp.push_back(std::move(m_arr.m_data[m_indices[i]])); }
        std::sort(tmp.data(), tmp.data() + n, comp);
        for (std::size_t i = 0; i < n; ++i) { m_arr.m_data[m_indices[i]] = std::move(tmp[i]); }
        return *this;
    }

    WhereProxy& reverse() {
        std::size_t n = m_indices.size();
        if (n <= 1) return *this;
        std::size_t lo = 0, hi = n - 1;

        while (lo < hi) {
            T tmp = std::move(m_arr.m_data[m_indices[lo]]);
            m_arr.m_data[m_indices[lo]] = std::move(m_arr.m_data[m_indices[hi]]);
            m_arr.m_data[m_indices[hi]] = std::move(tmp);
            ++lo;
            --hi;
        }

        return *this;
    }

    void move_to(DynamicArray<T>& dest) {
        std::size_t n = m_indices.size();
        if (n == 0) return;
        dest.ensure_capacity(dest.m_size + n);
        std::size_t j = 0;
        std::size_t write = 0;

        for (std::size_t read = 0; read < m_arr.m_size; ++read) {
            if (j < n && read == m_indices[j]) {
                dest.m_data[dest.m_size++] = std::move(m_arr.m_data[read]);
                ++j;
            } else {
                if (write != read) m_arr.m_data[write] = std::move(m_arr.m_data[read]);
                ++write;
            }
        }

        m_arr.m_size = write;
    }

    void insert_before(const T& value) {
        if (m_indices.empty()) return;
        m_arr.ensure_capacity(m_arr.m_size + m_indices.size());

        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + (i - 1); 
            std::move_backward(m_arr.m_data + idx, m_arr.m_data + m_arr.m_size, m_arr.m_data + m_arr.m_size + 1);
            m_arr.m_data[idx] = value;
            ++m_arr.m_size;
        }
    }

    void insert_after(const T& value) {
        if (m_indices.empty()) return;
        m_arr.ensure_capacity(m_arr.m_size + m_indices.size());

        for (std::size_t i = m_indices.size(); i > 0; --i) {
            std::size_t idx = m_indices[i - 1] + i; 
            std::move_backward(m_arr.m_data + idx, m_arr.m_data + m_arr.m_size, m_arr.m_data + m_arr.m_size + 1);
            m_arr.m_data[idx] = value;
            ++m_arr.m_size;
        }
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    insert_before(U&& value) {
        T converted = static_cast<T>(std::forward<U>(value));
        insert_before(converted);
    }

    template <typename U>
    typename std::enable_if<
        are_compatible_types<T, U> &&
        !std::is_same<typename std::decay<U>::type, T>::value
    >::type
    insert_after(U&& value) {
        T converted = static_cast<T>(std::forward<U>(value));
        insert_after(converted);
    }

    void insert_unordered(const T& value) {
        if (m_indices.empty()) return;
        m_arr.ensure_capacity(m_arr.m_size + m_indices.size());

        for (std::size_t i = 0; i < m_indices.size(); ++i) {
            std::size_t idx = m_indices[i];
            m_arr.m_data[m_arr.m_size] = std::move(m_arr.m_data[idx]);
            m_arr.m_data[idx] = value;
            ++m_arr.m_size;
        }
    }
};

template <typename T>
WhereProxy<T> DynamicArray<T>::where(const T& value) { return WhereProxy<T>(*this, collect_indices(value)); }

template <typename T>
template <typename Pred>
WhereProxy<T> DynamicArray<T>::where(Pred pred) { return WhereProxy<T>(*this, collect_indices_if(pred)); }

} // namespace fizmo

#endif // FIZMO_CUSTOM_ARRAY_HPP