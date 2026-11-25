#include "fizmo_library.hpp"
#include "argon2.hpp"
#include "blake2b.hpp"
#include "secure_memory.hpp"
#include "../Random/random_std_int.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

namespace fizmo {
namespace crypto {

namespace {

constexpr std::uint32_t version = 0x13;
constexpr std::size_t block_words = 128;
constexpr std::size_t sync_points = 4;

struct Block {
    std::uint64_t v[block_words];
};

inline std::uint64_t rotr64(std::uint64_t x, unsigned n) noexcept { return (x >> n) | (x << (64 - n)); }

inline std::uint64_t blamka(std::uint64_t x, std::uint64_t y) noexcept {
    const std::uint64_t m = 0xFFFFFFFFULL;
    return x + y + 2 * ((x & m) * (y & m));
}

inline void gb(std::uint64_t& a, std::uint64_t& b, std::uint64_t& c, std::uint64_t& d) noexcept {
    a = blamka(a, b); d = rotr64(d ^ a, 32);
    c = blamka(c, d); b = rotr64(b ^ c, 24);
    a = blamka(a, b); d = rotr64(d ^ a, 16);
    c = blamka(c, d); b = rotr64(b ^ c, 63);
}

inline void round_nomsg(std::uint64_t& v0, std::uint64_t& v1, std::uint64_t& v2, std::uint64_t& v3, std::uint64_t& v4, std::uint64_t& v5, std::uint64_t& v6, std::uint64_t& v7,
                        std::uint64_t& v8, std::uint64_t& v9, std::uint64_t& v10, std::uint64_t& v11, std::uint64_t& v12, std::uint64_t& v13, std::uint64_t& v14, std::uint64_t& v15) noexcept {
    gb(v0, v4, v8, v12);
    gb(v1, v5, v9, v13);
    gb(v2, v6, v10, v14);
    gb(v3, v7, v11, v15);
    gb(v0, v5, v10, v15);
    gb(v1, v6, v11, v12);
    gb(v2, v7, v8, v13);
    gb(v3, v4, v9, v14);
}

void fill_block(const Block& prev, const Block& ref, Block& next, bool with_xor) noexcept {
    Block r;
    Block tmp;
    for (std::size_t i = 0; i < block_words; ++i) r.v[i] = ref.v[i] ^ prev.v[i];
    tmp = r;
    if (with_xor) for (std::size_t i = 0; i < block_words; ++i) tmp.v[i] ^= next.v[i];
    std::uint64_t* v = r.v;
    for (std::size_t i = 0; i < 8; ++i) {
        std::uint64_t* q = v + 16 * i;
        round_nomsg(q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], q[8], q[9], q[10], q[11], q[12], q[13], q[14], q[15]);
    }
    for (std::size_t i = 0; i < 8; ++i) {
        std::uint64_t* q = v + 2 * i;
        round_nomsg(q[0], q[1], q[16], q[17], q[32], q[33], q[48], q[49], q[64], q[65], q[80], q[81], q[96], q[97], q[112], q[113]);
    }
    for (std::size_t i = 0; i < block_words; ++i) next.v[i] = tmp.v[i] ^ r.v[i];
    secure_zero(&r, sizeof r);
    secure_zero(&tmp, sizeof tmp);
}

inline void store_le32(std::uint8_t* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) p[i] = std::uint8_t(v >> (8 * i));
}

inline std::uint64_t load_le64(const std::uint8_t* p) noexcept {
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

inline void store_le64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) p[i] = std::uint8_t(v >> (8 * i));
}

void bytes_to_block(Block& b, const std::uint8_t* bytes) noexcept {
    for (std::size_t i = 0; i < block_words; ++i) b.v[i] = load_le64(bytes + 8 * i);
}

void block_to_bytes(std::uint8_t* bytes, const Block& b) noexcept {
    for (std::size_t i = 0; i < block_words; ++i) store_le64(bytes + 8 * i, b.v[i]);
}

struct Instance {
    std::vector<Block> memory;
    std::uint32_t passes = 0;
    std::uint32_t lanes = 0;
    std::uint32_t segment_length = 0;
    std::uint32_t lane_length = 0;
    std::uint32_t memory_blocks = 0;
    Argon2Type type = Argon2Type::id;
};

std::uint32_t index_alpha(const Instance& in, std::uint32_t pass, std::uint32_t slice, std::uint32_t index, std::uint32_t pseudo_rand, bool same_lane) noexcept {
    std::uint32_t area;
    if (pass == 0) {
        if (slice == 0) area = index - 1;
        else if (same_lane) area = slice * in.segment_length + index - 1;
        else area = slice * in.segment_length + (index == 0 ? std::uint32_t(-1) : 0);
    } else {
        if (same_lane) area = in.lane_length - in.segment_length + index - 1;
        else area = in.lane_length - in.segment_length + (index == 0 ? std::uint32_t(-1) : 0);
    }
    std::uint64_t rel = pseudo_rand;
    rel = (rel * rel) >> 32;
    rel = area - 1 - ((std::uint64_t(area) * rel) >> 32);
    std::uint32_t start = 0;
    if (pass != 0) start = (slice == sync_points - 1) ? 0 : (slice + 1) * in.segment_length;
    return std::uint32_t((start + rel) % in.lane_length);
}

void next_addresses(Block& address, Block& input, const Block& zero) noexcept {
    ++input.v[6];
    fill_block(zero, input, address, false);
    fill_block(zero, address, address, false);
}

void fill_segment(Instance& in, std::uint32_t pass, std::uint32_t lane, std::uint32_t slice) {
    const bool independent = in.type == Argon2Type::i || (in.type == Argon2Type::id && pass == 0 && slice < sync_points / 2);
    Block address{};
    Block input{};
    Block zero{};
    if (independent) {
        input.v[0] = pass;
        input.v[1] = lane;
        input.v[2] = slice;
        input.v[3] = in.memory_blocks;
        input.v[4] = in.passes;
        input.v[5] = static_cast<std::uint64_t>(in.type);
    }
    std::uint32_t start = 0;
    if (pass == 0 && slice == 0) {
        start = 2;
        if (independent) next_addresses(address, input, zero);
    }
    std::uint32_t current = lane * in.lane_length + slice * in.segment_length + start;
    std::uint32_t previous = (current % in.lane_length == 0) ? current + in.lane_length - 1 : current - 1;

    for (std::uint32_t i = start; i < in.segment_length; ++i, ++current, ++previous) {
        if (current % in.lane_length == 1) previous = current - 1;
        std::uint64_t pseudo_rand;
        if (independent) {
            if (i % block_words == 0) next_addresses(address, input, zero);
            pseudo_rand = address.v[i % block_words];
        } else {
            pseudo_rand = in.memory[previous].v[0];
        }
        std::uint32_t ref_lane = std::uint32_t((pseudo_rand >> 32) % in.lanes);
        if (pass == 0 && slice == 0) ref_lane = lane;
        const std::uint32_t ref_index = index_alpha(in, pass, slice, i, std::uint32_t(pseudo_rand & 0xFFFFFFFFULL), ref_lane == lane);
        const Block& ref = in.memory[std::size_t(in.lane_length) * ref_lane + ref_index];
        fill_block(in.memory[previous], ref, in.memory[current], pass != 0);
    }
    secure_zero(&address, sizeof address);
    secure_zero(&input, sizeof input);
}

void check(const Argon2Params& p, std::size_t password_size, std::size_t salt_size, std::size_t secret_size, std::size_t associated_size) {
    if (p.type != Argon2Type::d && p.type != Argon2Type::i && p.type != Argon2Type::id) throw std::invalid_argument("argon2: unknown type");
    if (p.hash_length < 4) throw std::invalid_argument("argon2: the hash has to be at least 4 bytes");
    if (salt_size < 8) throw std::invalid_argument("argon2: the salt has to be at least 8 bytes");
    if (p.iterations < 1) throw std::invalid_argument("argon2: there has to be at least 1 iteration");
    if (p.parallelism < 1 || p.parallelism > 0xFFFFFFU) throw std::invalid_argument("argon2: the parallelism has to be from 1 to 16777215");
    if (p.memory_kib < 8ULL * p.parallelism) throw std::invalid_argument("argon2: the memory has to be at least 8 KiB for each lane of parallelism");
    const std::uint64_t limit = 0xFFFFFFFFULL;
    if (password_size > limit || salt_size > limit || secret_size > limit || associated_size > limit) throw std::invalid_argument("argon2: an input is longer than 4 GiB");
}

}

Argon2Params Argon2Params::of(PasswordStrength strength) noexcept {
    Argon2Params p;
    switch (strength) {
        case PasswordStrength::minimum:     p.memory_kib = 19456;   p.iterations = 2; break;
        case PasswordStrength::interactive: p.memory_kib = 65536;   p.iterations = 2; break;
        case PasswordStrength::moderate:    p.memory_kib = 262144;  p.iterations = 3; break;
        case PasswordStrength::sensitive:   p.memory_kib = 1048576; p.iterations = 4; break;
    }
    return p;
}

std::vector<std::uint8_t> argon2(const Argon2Params& params, const void* password, std::size_t password_size, const void* salt, std::size_t salt_size, const void* secret, std::size_t secret_size, const void* associated, std::size_t associated_size) {
    check(params, password_size, salt_size, secret_size, associated_size);

    std::uint8_t h0[72];
    {
        BLAKE2b h(64);
        std::uint8_t w[4];
        auto put = [&](std::uint32_t x) { store_le32(w, x); h.update(w, 4); };
        put(params.parallelism);
        put(params.hash_length);
        put(params.memory_kib);
        put(params.iterations);
        put(version);
        put(static_cast<std::uint32_t>(params.type));
        put(std::uint32_t(password_size));
        if (password_size) h.update(password, password_size);
        put(std::uint32_t(salt_size));
        if (salt_size) h.update(salt, salt_size);
        put(std::uint32_t(secret_size));
        if (secret_size) h.update(secret, secret_size);
        put(std::uint32_t(associated_size));
        if (associated_size) h.update(associated, associated_size);
        h.finish(h0);
    }

    Instance in;
    in.type = params.type;
    in.passes = params.iterations;
    in.lanes = params.parallelism;
    std::uint32_t blocks = params.memory_kib;
    if (blocks < 2 * sync_points * in.lanes) blocks = 2 * sync_points * in.lanes;
    in.segment_length = blocks / (in.lanes * sync_points);
    in.memory_blocks = in.segment_length * in.lanes * sync_points;
    in.lane_length = in.segment_length * sync_points;
    in.memory.assign(in.memory_blocks, Block{});

    std::uint8_t block_bytes[1024];
    for (std::uint32_t l = 0; l < in.lanes; ++l) {
        for (std::uint32_t c = 0; c < 2; ++c) {
            store_le32(h0 + 64, c);
            store_le32(h0 + 68, l);
            std::vector<std::uint8_t> b = blake2b_long(h0, sizeof h0, 1024);
            bytes_to_block(in.memory[std::size_t(l) * in.lane_length + c], b.data());
            secure_zero(b.data(), b.size());
        }
    }
    secure_zero(h0, sizeof h0);

    for (std::uint32_t pass = 0; pass < in.passes; ++pass) {
        for (std::uint32_t slice = 0; slice < sync_points; ++slice) {
            if (in.lanes == 1) {
                fill_segment(in, pass, 0, slice);
                continue;
            }
            std::vector<std::thread> workers;
            workers.reserve(in.lanes);
            for (std::uint32_t l = 0; l < in.lanes; ++l) workers.emplace_back([&in, pass, l, slice] { fill_segment(in, pass, l, slice); });
            for (std::thread& t : workers) t.join();
        }
    }

    Block final = in.memory[in.lane_length - 1];
    for (std::uint32_t l = 1; l < in.lanes; ++l) {
        const Block& last = in.memory[std::size_t(l) * in.lane_length + in.lane_length - 1];
        for (std::size_t i = 0; i < block_words; ++i) final.v[i] ^= last.v[i];
    }
    block_to_bytes(block_bytes, final);
    std::vector<std::uint8_t> tag = blake2b_long(block_bytes, sizeof block_bytes, params.hash_length);
    secure_zero(block_bytes, sizeof block_bytes);
    secure_zero(&final, sizeof final);
    secure_zero(in.memory.data(), in.memory.size() * sizeof(Block));
    return tag;
}

std::vector<std::uint8_t> argon2(const Argon2Params& params, const std::string& password, const std::string& salt, const std::string& secret, const std::string& associated) {
    return argon2(params, password.data(), password.size(), salt.data(), salt.size(), secret.data(), secret.size(), associated.data(), associated.size());
}

namespace {

const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64_unpadded(const std::vector<std::uint8_t>& data) {
    std::string out;
    std::size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const std::uint32_t n = (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8) | data[i + 2];
        out += b64_alphabet[(n >> 18) & 63];
        out += b64_alphabet[(n >> 12) & 63];
        out += b64_alphabet[(n >> 6) & 63];
        out += b64_alphabet[n & 63];
    }
    const std::size_t rest = data.size() - i;
    if (rest == 1) {
        const std::uint32_t n = std::uint32_t(data[i]) << 16;
        out += b64_alphabet[(n >> 18) & 63];
        out += b64_alphabet[(n >> 12) & 63];
    } else if (rest == 2) {
        const std::uint32_t n = (std::uint32_t(data[i]) << 16) | (std::uint32_t(data[i + 1]) << 8);
        out += b64_alphabet[(n >> 18) & 63];
        out += b64_alphabet[(n >> 12) & 63];
        out += b64_alphabet[(n >> 6) & 63];
    }
    return out;
}

bool b64_unpadded_decode(const std::string& text, std::vector<std::uint8_t>& out) {
    out.clear();
    std::uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        const char* at = std::strchr(b64_alphabet, c);
        if (!at || c == '\0') return false;
        acc = (acc << 6) | std::uint32_t(at - b64_alphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(std::uint8_t(acc >> bits));
            acc &= (1u << bits) - 1;
        }
    }
    if (bits >= 6) return false;
    return acc == 0;
}

bool read_number(const std::string& s, std::size_t& at, std::uint32_t& out) {
    const std::size_t begin = at;
    std::uint64_t v = 0;
    while (at < s.size() && s[at] >= '0' && s[at] <= '9') {
        v = v * 10 + std::uint64_t(s[at] - '0');
        if (v > 0xFFFFFFFFULL) return false;
        ++at;
    }
    if (at == begin || (at - begin > 1 && s[begin] == '0')) return false;
    out = std::uint32_t(v);
    return true;
}

bool expect(const std::string& s, std::size_t& at, const char* text) {
    const std::size_t n = std::strlen(text);
    if (s.compare(at, n, text) != 0) return false;
    at += n;
    return true;
}

const char* type_name(Argon2Type t) {
    switch (t) {
        case Argon2Type::d: return "argon2d";
        case Argon2Type::i: return "argon2i";
        case Argon2Type::id: return "argon2id";
    }
    return "argon2id";
}

}

std::string argon2_encode(const Argon2Params& params, const std::vector<std::uint8_t>& salt, const std::vector<std::uint8_t>& hash) {
    std::string out = "$";
    out += type_name(params.type);
    out += "$v=19$m=" + std::to_string(params.memory_kib) + ",t=" + std::to_string(params.iterations) + ",p=" + std::to_string(params.parallelism);
    out += "$" + b64_unpadded(salt) + "$" + b64_unpadded(hash);
    return out;
}

bool argon2_decode(const std::string& encoded, Argon2Params& params, std::vector<std::uint8_t>& salt, std::vector<std::uint8_t>& hash) {
    std::size_t at = 0;
    Argon2Params p;
    if (expect(encoded, at, "$argon2id$")) p.type = Argon2Type::id;
    else if (expect(encoded, at, "$argon2i$")) p.type = Argon2Type::i;
    else if (expect(encoded, at, "$argon2d$")) p.type = Argon2Type::d;
    else return false;
    std::uint32_t v = 0;
    if (!expect(encoded, at, "v=") || !read_number(encoded, at, v) || v != version || !expect(encoded, at, "$")) return false;
    if (!expect(encoded, at, "m=") || !read_number(encoded, at, p.memory_kib)) return false;
    if (!expect(encoded, at, ",t=") || !read_number(encoded, at, p.iterations)) return false;
    if (!expect(encoded, at, ",p=") || !read_number(encoded, at, p.parallelism)) return false;
    if (!expect(encoded, at, "$")) return false;
    const std::size_t dollar = encoded.find('$', at);
    if (dollar == std::string::npos) return false;
    if (!b64_unpadded_decode(encoded.substr(at, dollar - at), salt)) return false;
    if (!b64_unpadded_decode(encoded.substr(dollar + 1), hash)) return false;
    if (salt.size() < 8 || hash.size() < 4) return false;
    if (p.iterations < 1 || p.parallelism < 1 || p.parallelism > 0xFFFFFFU || p.memory_kib < 8ULL * p.parallelism) return false;
    p.salt_length = std::uint32_t(salt.size());
    p.hash_length = std::uint32_t(hash.size());
    params = p;
    return true;
}

namespace {

void check_pepper(const std::string& pepper) {
    if (pepper.size() < min_pepper_length) throw std::invalid_argument("the pepper has to be at least 16 bytes; generate_key() makes a 32-byte one");
}

std::string hash_with(const std::string& password, const Argon2Params& params, const std::string& pepper) {
    if (params.salt_length < min_salt_length || params.salt_length > max_salt_length) throw std::invalid_argument("the salt length has to be from 8 to 1024 bytes");
    std::vector<std::uint8_t> salt(params.salt_length);
    random_bytes(salt.data(), salt.size());
    std::vector<std::uint8_t> hash = argon2(params, password.data(), password.size(), salt.data(), salt.size(), pepper.data(), pepper.size());
    std::string out = argon2_encode(params, salt, hash);
    secure_zero(hash.data(), hash.size());
    secure_zero(salt.data(), salt.size());
    return out;
}

bool verify_with(const std::string& password, const std::string& encoded, const std::string& pepper) {
    Argon2Params p;
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> expected;
    if (!argon2_decode(encoded, p, salt, expected)) return false;
    if (salt.size() > max_salt_length || expected.size() > 1024) return false;
    if (p.memory_kib > verify_memory_limit_kib || p.iterations > verify_iteration_limit || p.parallelism > verify_parallelism_limit) return false;
    std::vector<std::uint8_t> got = argon2(p, password.data(), password.size(), salt.data(), salt.size(), pepper.data(), pepper.size());
    const bool same = got.size() == expected.size() && secure_compare(got.data(), expected.data(), got.size());
    secure_zero(got.data(), got.size());
    return same;
}

} // namespace

std::string hash_password(const std::string& password, const Argon2Params& params) {
    return hash_with(password, params, std::string());
}

std::string hash_password(const std::string& password, PasswordStrength strength) {
    return hash_password(password, Argon2Params::of(strength));
}

bool verify_password(const std::string& password, const std::string& encoded) {
    return verify_with(password, encoded, std::string());
}

std::string hash_password(const std::string& password, const Argon2Params& params, const std::string& pepper) {
    check_pepper(pepper);
    return hash_with(password, params, pepper);
}

std::string hash_password(const std::string& password, PasswordStrength strength, const std::string& pepper) {
    return hash_password(password, Argon2Params::of(strength), pepper);
}

bool verify_password(const std::string& password, const std::string& encoded, const std::string& pepper) {
    if (pepper.size() < min_pepper_length) return false;
    return verify_with(password, encoded, pepper);
}

std::string generate_salt(std::size_t length) {
    if (length < min_salt_length || length > max_salt_length) throw std::invalid_argument("the salt length has to be from 8 to 1024 bytes");
    std::string salt(length, '\0');
    random_bytes(&salt[0], salt.size());
    return salt;
}

bool password_needs_rehash(const std::string& encoded, const Argon2Params& params) {
    Argon2Params p;
    std::vector<std::uint8_t> salt;
    std::vector<std::uint8_t> hash;
    if (!argon2_decode(encoded, p, salt, hash)) return true;
    return p.type != params.type || p.memory_kib < params.memory_kib || p.iterations < params.iterations || p.parallelism != params.parallelism || p.hash_length < params.hash_length || p.salt_length < params.salt_length;
}

bool password_needs_rehash(const std::string& encoded, PasswordStrength strength) {
    return password_needs_rehash(encoded, Argon2Params::of(strength));
}

} // namespace crypto
} // namespace fizmo
