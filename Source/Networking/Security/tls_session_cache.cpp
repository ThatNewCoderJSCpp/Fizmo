#include "fizmo_library.hpp"
#include "tls_session_cache.hpp"
#include "Backend/native_backend.hpp"
#include "../../Crypto/secure_memory.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace fizmo {
namespace networking {
namespace security {

namespace {

const char kMagic[8] = { 'F', 'Z', 'T', 'L', 'S', 'S', 'C', '1' };

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

bool get_u32(const std::vector<std::uint8_t>& in, std::size_t& pos, std::uint32_t& v) {
    if (pos + 4 > in.size()) return false;
    v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | in[pos++];
    return true;
}

bool get_u64(const std::vector<std::uint8_t>& in, std::size_t& pos, std::uint64_t& v) {
    if (pos + 8 > in.size()) return false;
    v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | in[pos++];
    return true;
}

std::string backend_tag() {
    std::string tag = detail::kBackendName;
#if defined(OPENSSL_VERSION_NUMBER)
    tag += "-" + std::to_string(OPENSSL_VERSION_NUMBER >> 28);
#endif
    return tag;
}

} // namespace

TLSSessionCache::TLSSessionCache(std::size_t max_entries, std::chrono::seconds ttl) : m_capacity(max_entries), m_ttl(ttl) {}

std::string TLSSessionCache::make_key(const std::string& host, std::uint16_t port) {
    return host + ":" + std::to_string(port);
}

void TLSSessionCache::store(const std::string& key, std::vector<std::uint8_t> session) {
    if (key.empty() || session.empty() || m_capacity == 0) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_index.find(key);

    if (it != m_index.end()) {
        crypto::secure_zero(it->second->session.data(), it->second->session.size());
        m_lru.erase(it->second);
        m_index.erase(it);
    }

    m_lru.push_front(Entry{ key, std::move(session), clock::now() + m_ttl });
    m_index[key] = m_lru.begin();
    evict_locked();
}

bool TLSSessionCache::lookup(const std::string& key, std::vector<std::uint8_t>& session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_index.find(key);

    if (it == m_index.end()) {
        ++m_misses;
        return false;
    }

    if (clock::now() >= it->second->expires) {
        crypto::secure_zero(it->second->session.data(), it->second->session.size());
        m_lru.erase(it->second);
        m_index.erase(it);
        ++m_misses;
        return false;
    }

    m_lru.splice(m_lru.begin(), m_lru, it->second);
    session = it->second->session;
    ++m_hits;
    return true;
}

void TLSSessionCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_index.find(key);
    if (it == m_index.end()) return;
    crypto::secure_zero(it->second->session.data(), it->second->session.size());
    m_lru.erase(it->second);
    m_index.erase(it);
}

void TLSSessionCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_lru) crypto::secure_zero(e.session.data(), e.session.size());
    m_lru.clear();
    m_index.clear();
}

std::size_t TLSSessionCache::prune() {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto now = clock::now();
    std::size_t removed = 0;

    for (auto it = m_lru.begin(); it != m_lru.end();) {
        if (now >= it->expires) {
            crypto::secure_zero(it->session.data(), it->session.size());
            m_index.erase(it->key);
            it = m_lru.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

void TLSSessionCache::set_capacity(std::size_t max_entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capacity = max_entries;
    evict_locked();
}

void TLSSessionCache::set_ttl(std::chrono::seconds ttl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ttl = ttl;
}

std::size_t TLSSessionCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lru.size();
}

std::size_t TLSSessionCache::capacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_capacity;
}

std::chrono::seconds TLSSessionCache::ttl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ttl;
}

std::uint64_t TLSSessionCache::hits() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hits;
}

std::uint64_t TLSSessionCache::misses() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_misses;
}

double TLSSessionCache::hit_rate() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t total = m_hits + m_misses;
    return total == 0 ? 0.0 : static_cast<double>(m_hits) / static_cast<double>(total);
}

void TLSSessionCache::evict_locked() {
    while (m_lru.size() > m_capacity) {
        auto& last = m_lru.back();
        crypto::secure_zero(last.session.data(), last.session.size());
        m_index.erase(last.key);
        m_lru.pop_back();
    }
}

std::vector<std::uint8_t> TLSSessionCache::serialize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::uint8_t> out(kMagic, kMagic + 8);
    const std::string tag = backend_tag();
    put_u32(out, static_cast<std::uint32_t>(tag.size()));
    out.insert(out.end(), tag.begin(), tag.end());
    const auto now = clock::now();
    std::uint32_t count = 0;
    for (const auto& e : m_lru) if (e.expires > now) ++count;
    put_u32(out, count);

    for (auto it = m_lru.rbegin(); it != m_lru.rend(); ++it) {
        if (it->expires <= now) continue;
        put_u32(out, static_cast<std::uint32_t>(it->key.size()));
        out.insert(out.end(), it->key.begin(), it->key.end());
        put_u64(out, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(it->expires.time_since_epoch()).count()));
        put_u32(out, static_cast<std::uint32_t>(it->session.size()));
        out.insert(out.end(), it->session.begin(), it->session.end());
    }

    return out;
}

bool TLSSessionCache::deserialize(const std::vector<std::uint8_t>& in) {
    if (in.size() < 16 || std::memcmp(in.data(), kMagic, 8) != 0) return false;
    std::size_t pos = 8;
    std::uint32_t tag_len = 0, count = 0;
    if (!get_u32(in, pos, tag_len) || pos + tag_len > in.size()) return false;
    const std::string tag(in.begin() + static_cast<std::ptrdiff_t>(pos), in.begin() + static_cast<std::ptrdiff_t>(pos + tag_len));
    pos += tag_len;
    if (tag != backend_tag()) return false;
    if (!get_u32(in, pos, count)) return false;
    std::vector<Entry> entries;
    const auto now = clock::now();

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t klen = 0, slen = 0;
        std::uint64_t expires = 0;
        if (!get_u32(in, pos, klen) || pos + klen > in.size()) return false;
        Entry e;
        e.key.assign(in.begin() + static_cast<std::ptrdiff_t>(pos), in.begin() + static_cast<std::ptrdiff_t>(pos + klen));
        pos += klen;
        if (!get_u64(in, pos, expires) || !get_u32(in, pos, slen) || pos + slen > in.size()) return false;
        e.session.assign(in.begin() + static_cast<std::ptrdiff_t>(pos), in.begin() + static_cast<std::ptrdiff_t>(pos + slen));
        pos += slen;
        e.expires = clock::time_point(std::chrono::seconds(static_cast<long long>(expires)));
        if (e.expires > now) entries.push_back(std::move(e));
    }

    for (auto& e : entries) {
        const auto expires = e.expires;
        const std::string key = e.key;
        store(key, std::move(e.session));
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_index.find(key);
        if (it != m_index.end()) it->second->expires = expires;
    }

    return true;
}

bool TLSSessionCache::save(const std::string& path) const {
    const auto data = serialize();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

bool TLSSessionCache::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return deserialize(data);
}

} // namespace security
} // namespace networking
} // namespace fizmo
