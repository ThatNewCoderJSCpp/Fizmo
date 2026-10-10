#ifndef FIZMO_OPEN_STACK_HPP
#define FIZMO_OPEN_STACK_HPP

#include <type_traits>
#include <memory>
#include <cstdint>
#include <array>
#include <stdexcept>

template<typename T>
class HeapStack {
private:
    struct Node {
        T data;
        std::unique_ptr<Node> next;
        std::unique_ptr<Node> prev;
        
        Node(const T& value) : data(value), next(nullptr), prev(nullptr) {}
        Node(T&& value) : data(std::move(value)), next(nullptr), prev(nullptr) {}
    };
    
    std::unique_ptr<Node> head;
    std::unique_ptr<Node> tail;
    std::size_t size_;

public:
    HeapStack() : head(nullptr), tail(nullptr), size_(0) {}
    
    void push_front(const T& value) {
        std::unique_ptr<Node> new_node = std::make_unique<Node>(value);

        if (!head) {
            tail = new_node.get();
        } else {
            new_node->next = std::move(head);
            new_node->next->prev = new_node.get();
        }
        head = std::move(new_node);
        ++size_;
    }
    
    void push_front(T&& value) {
        std::unique_ptr<Node> new_node = std::make_unique<Node>(std::move(value));
        if (!head) {
            tail = new_node.get();
        } else {
            new_node->next = std::move(head);
            new_node->next->prev = new_node.get();
        }
        head = std::move(new_node);
        ++size_;
    }
    
    void push_back(const T& value) {
        std::unique_ptr<Node> new_node = std::make_unique<Node>(value);
        Node* raw_ptr = new_node.get();
        
        if (!head) {
            head = std::move(new_node);
            tail = head.get();
        } else {
            new_node->prev = tail;
            tail->next = std::move(new_node);
            tail = raw_ptr;
        }
        ++size_;
    }
    
    void push_back(T&& value) {
        std::unique_ptr<Node> new_node = std::make_unique<Node>(std::move(value));
        Node* raw_ptr = new_node.get();
        
        if (!head) {
            head = std::move(new_node);
            tail = head.get();
        } else {
            new_node->prev = tail;
            tail->next = std::move(new_node);
            tail = raw_ptr;
        }
        ++size_;
    }
    
    T pop_front() {
        if (!head) { throw std::runtime_error("HeapStack is empty"); }
        T value = std::move(head->data);
        head = std::move(head->next);
        if (head) {
            head->prev = nullptr;
        } else {
            tail = nullptr;
        }
        --size_;
        return value;
    }
    
    T pop_back() {
        if (!tail) { throw std::runtime_error("HeapStack is empty"); }
        T value = std::move(tail->data);
        if (tail->prev) {
            tail = tail->prev;
            tail->next.reset();
        } else {
            head.reset();
            tail = nullptr;
        }
        --size_;
        return value;
    }
    
    T& front() {
        if (!head) throw std::runtime_error("HeapStack is empty");
        return head->data;
    }
    
    const T& front() const {
        if (!head) throw std::runtime_error("HeapStack is empty");
        return head->data;
    }
    
    T& back() {
        if (!tail) throw std::runtime_error("HeapStack is empty");
        return tail->data;
    }
    
    const T& back() const {
        if (!tail) throw std::runtime_error("HeapStack is empty");
        return tail->data;
    }

    T& operator[](std::size_t index) {
        if (index >= size_) { throw std::out_of_range("Index out of range"); }
        
        if (index < size_ / 2) {
            Node* current = head.get();
            for (std::size_t i = 0; i < index; ++i) { current = current->next.get(); }
            return current->data;
        } else {
            Node* current = tail;
            for (std::size_t i = size_ - 1; i > index; --i) { current = current->prev; }
            return current->data;
        }
    }
    
    const T& operator[](std::size_t index) const {
        if (index >= size_) { throw std::out_of_range("Index out of range"); }
        
        if (index < size_ / 2) {
            Node* current = head.get();
            for (std::size_t i = 0; i < index; ++i) { current = current->next.get(); }
            return current->data;
        } else {
            Node* current = tail;
            for (std::size_t i = size_ - 1; i > index; --i) { current = current->prev; }
            return current->data;
        }
    }
    
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr std::size_t size() const noexcept { return size_; }
};

template<typename T, size_t MaxSize = 1024>
class Stack {
private:
    std::array<T, MaxSize> data;
    std::size_t front_idx;
    std::size_t back_idx;
    std::size_t size_;
    
    constexpr std::size_t next_index(const std::size_t idx) const noexcept { return (idx + 1) % MaxSize; }
    constexpr std::size_t prev_index(const std::size_t idx) const noexcept { return (idx == 0) ? MaxSize - 1 : idx - 1; }

public:
    Stack() noexcept : front_idx(0), back_idx(0), size_(0) {}
    
    void push_front(const T& value) {
        if (size_ >= MaxSize) { throw std::runtime_error("Stack is full"); }
        if (size_ > 0) { front_idx = prev_index(front_idx); }
        data[front_idx] = value;
        ++size_;
    }
    
    void push_front(T&& value) {
        if (size_ >= MaxSize) { throw std::runtime_error("Stack is full"); }
        if (size_ > 0) { front_idx = prev_index(front_idx); }
        data[front_idx] = std::move(value);
        ++size_;
    }
    
    void push_back(const T& value) {
        if (size_ >= MaxSize) { throw std::runtime_error("Stack is full"); }
        if (size_ > 0) { back_idx = next_index(back_idx); }
        data[back_idx] = value;
        ++size_;
    }
    
    void push_back(T&& value) {
        if (size_ >= MaxSize) { throw std::runtime_error("Stack is full"); }
        if (size_ > 0) { back_idx = next_index(back_idx); }
        data[back_idx] = std::move(value);
        ++size_;
    }
    
    T pop_front() {
        if (size_ == 0) { throw std::runtime_error("Stack is empty"); }
        T value = std::move(data[front_idx]);
        if (size_ > 1) { front_idx = next_index(front_idx); }
        --size_;
        return value;
    }
    
    T pop_back() {
        if (size_ == 0) { throw std::runtime_error("Stack is empty"); }
        T value = std::move(data[back_idx]);
        if (size_ > 1) { back_idx = prev_index(back_idx); }
        --size_;
        return value;
    }
    
    T& front() {
        if (size_ == 0) throw std::runtime_error("Stack is empty");
        return data[front_idx];
    }
    
    const T& front() const {
        if (size_ == 0) throw std::runtime_error("Stack is empty");
        return data[front_idx];
    }
    
    T& back() {
        if (size_ == 0) throw std::runtime_error("Stack is empty");
        return data[back_idx];
    }
    
    const T& back() const {
        if (size_ == 0) throw std::runtime_error("Stack is empty");
        return data[back_idx];
    }

    T& operator[](std::size_t index) {
        if (index >= size_) { throw std::out_of_range("Index out of range"); }
        const std::size_t actual_index = (front_idx + index) % MaxSize;
        return data[actual_index];
    }
    
    const T& operator[](std::size_t index) const {
        if (index >= size_) { throw std::out_of_range("Index out of range"); }
        const std::size_t actual_index = (front_idx + index) % MaxSize;
        return data[actual_index];
    }
    
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr std::size_t size() const noexcept { return size_; }
    constexpr std::size_t capacity() const noexcept { return MaxSize; }
};

#endif // FIZMO_OPEN_STACK_HPP