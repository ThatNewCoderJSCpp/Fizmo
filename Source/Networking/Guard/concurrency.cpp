#include "fizmo_library.hpp"
#include "concurrency.hpp"

#include <algorithm>

namespace fizmo {
namespace networking {
namespace guard {

KeyedMutex::Lock& KeyedMutex::Lock::operator=(Lock&& other) noexcept {
    if (this != &other) {
        unlock();
        m_owner = other.m_owner;
        m_key = std::move(other.m_key);
        other.m_owner = nullptr;
    }

    return *this;
}

void KeyedMutex::Lock::unlock() {
    if (!m_owner) return;
    KeyedMutex* owner = m_owner;
    m_owner = nullptr;
    owner->release(m_key);
}

KeyedMutex::Lock KeyedMutex::lock(const std::string& key) {
    std::unique_lock<std::mutex> lk(m_mutex);
    Slot& slot = m_slots[key];
    ++slot.waiters;
    m_cv.wait(lk, [&] { return !m_slots[key].held; });
    Slot& s = m_slots[key];
    --s.waiters;
    s.held = true;
    return Lock(this, key);
}

KeyedMutex::Lock KeyedMutex::try_lock(const std::string& key) {
    std::lock_guard<std::mutex> lk(m_mutex);
    Slot& slot = m_slots[key];

    if (slot.held) return Lock();

    slot.held = true;
    return Lock(this, key);
}

KeyedMutex::Lock KeyedMutex::try_lock_for(const std::string& key, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lk(m_mutex);
    ++m_slots[key].waiters;
    const bool ok = m_cv.wait_for(lk, timeout, [&] { return !m_slots[key].held; });
    Slot& s = m_slots[key];
    --s.waiters;

    if (!ok) {
        if (!s.held && !s.waiters) m_slots.erase(key);
        return Lock();
    }

    s.held = true;
    return Lock(this, key);
}

std::vector<KeyedMutex::Lock> KeyedMutex::lock_all(std::vector<std::string> keys) {
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    std::vector<Lock> out;
    out.reserve(keys.size());

    for (const auto& k : keys) {
        out.push_back(lock(k));
    }

    return out;
}

bool KeyedMutex::locked(const std::string& key) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_slots.find(key);
    return it != m_slots.end() && it->second.held;
}

std::size_t KeyedMutex::size() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_slots.size();
}

void KeyedMutex::release(const std::string& key) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        const auto it = m_slots.find(key);
        if (it == m_slots.end()) return;
        it->second.held = false;
        if (!it->second.waiters) m_slots.erase(it);
    }

    m_cv.notify_all();
}

} // namespace guard
} // namespace networking
} // namespace fizmo
