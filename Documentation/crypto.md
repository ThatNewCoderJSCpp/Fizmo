# Passwords and encryption

`#include <crypto.hpp>` adds BLAKE2b, Argon2 password hashing and XChaCha20-Poly1305 encryption to the hashes, HMAC and encodings already in `fizmo::crypto`. Everything here is checked against the RFC test vectors (RFC 7693, RFC 9106, RFC 8439) and against libsodium and the reference Argon2.

## BLAKE2b

```cpp
std::vector<std::uint8_t> d = fizmo::crypto::BLAKE2b::hash("abc");
std::vector<std::uint8_t> mac = fizmo::crypto::BLAKE2b::hash(message, 32, key);
```

`BLAKE2b(digest_size = 64, key = nullptr, key_size = 0)` hashes in pieces with `update` and `finish`; the digest is 1 to 64 bytes and the key at most 64. `blake2b_long(data, len, out_size)` is Argon2's variable-length hash H'.

## Password hashing

```cpp
std::string stored = fizmo::crypto::hash_password(password);
bool ok = fizmo::crypto::verify_password(attempt, stored);
```

`hash_password` uses Argon2id with a random 16-byte salt and gives the standard encoded form, `$argon2id$v=19$m=65536,t=2,p=1$salt$hash`, which other Argon2 libraries read too. `verify_password` compares in constant time and returns `false` for anything malformed.

`PasswordStrength` picks the cost; `Argon2Params::of(strength)` gives the settings:

| Strength | Memory | Passes |
|---|---|---|
| `minimum` | 19 MiB | 2 |
| `interactive` | 64 MiB | 2 |
| `moderate` | 256 MiB | 3 |
| `sensitive` | 1 GiB | 4 |

`Argon2Params` has `type` (`Argon2Type::d`, `i` or `id`), `memory_kib`, `iterations`, `parallelism` (lanes, run on that many threads), `hash_length` and `salt_length`. `password_needs_rehash(stored, params)` says whether a stored hash used weaker settings. `argon2(params, password, salt, secret, associated)` is the raw function, and `argon2_encode` and `argon2_decode` convert to and from the encoded form. Bad settings throw `std::invalid_argument`.

Every hash gets a fresh salt from the system's secure random source, so two users with the same password get different hashes. `salt_length` must be 8 to 1024 bytes (16 is the default); anything else throws instead of being quietly replaced. `generate_salt(length)` gives a random salt for `argon2` or `derive_key`. A pepper is a server-side secret kept outside the database: `hash_password(password, params or strength, pepper)` and `verify_password(password, encoded, pepper)` mix it in as Argon2's secret input, so a leaked database alone can't be cracked. The pepper must be at least 16 bytes and isn't stored in the hash, so the same pepper is needed to verify. `verify_password` also refuses stored hashes that ask for more than 16 GiB, 1024 passes or 255 lanes, so a tampered hash can't stall the server.

## Encryption

```cpp
std::string key = fizmo::crypto::generate_key();
std::string sealed = fizmo::crypto::encrypt(key, "attack at dawn");
std::string plain;
bool ok = fizmo::crypto::decrypt(key, sealed, plain);
```

`encrypt(key, plaintext, associated)` uses XChaCha20-Poly1305 with a random 24-byte nonce; the result is `FZE1`, the nonce, the ciphertext and the 16-byte tag. `decrypt` returns `false` if the key or the associated data is wrong or a byte changed. `encrypt_with_password(password, plaintext, strength or params, associated)` derives the key with Argon2id and stores the settings and salt in front (`FZP1`), so `decrypt_with_password(password, sealed, plain, associated)` needs only the password. `derive_key(password, salt, strength or params)` gives the 32-byte key on its own.

The building blocks are public too: `chacha20_block`, `chacha20_xor`, `hchacha20`, `Poly1305`, `ChaCha20Poly1305::seal` and `open` (RFC 8439, 12-byte nonce), and `XChaCha20Poly1305::seal` and `open` (24-byte nonce).
