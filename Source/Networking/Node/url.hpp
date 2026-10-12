#ifndef FIZMO_NODE_URL_HPP
#define FIZMO_NODE_URL_HPP

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace networking {

class URLSearchParams {
public:
    using Entry = std::pair<std::string, std::string>;

    URLSearchParams() = default;
    explicit URLSearchParams(const std::string& query);
    URLSearchParams(std::initializer_list<Entry> entries) : m_entries(entries) {}

    void append(const std::string& name, const std::string& value);
    void remove(const std::string& name);
    void remove(const std::string& name, const std::string& value);
    void set(const std::string& name, const std::string& value);
    bool has(const std::string& name) const;
    std::string get(const std::string& name, const std::string& fallback = {}) const;
    std::vector<std::string> get_all(const std::string& name) const;
    void sort();
    void clear() { m_entries.clear(); }

    std::size_t size() const noexcept { return m_entries.size(); }
    bool empty() const noexcept { return m_entries.empty(); }
    const std::vector<Entry>& entries() const noexcept { return m_entries; }
    std::vector<Entry>::const_iterator begin() const noexcept { return m_entries.begin(); }
    std::vector<Entry>::const_iterator end() const noexcept { return m_entries.end(); }

    std::string to_string() const;

private:
    std::vector<Entry> m_entries;
};

class URL {
public:
    URL() = default;
    explicit URL(const std::string& input);
    URL(const std::string& input, const std::string& base);
    URL(const std::string& input, const URL& base);

    static bool parse(const std::string& input, URL& out, std::string* error = nullptr);
    static bool parse(const std::string& input, const URL& base, URL& out, std::string* error = nullptr);
    static bool can_parse(const std::string& input) { URL u; return parse(input, u); }
    static std::uint16_t default_port(const std::string& scheme) noexcept;

    bool valid() const noexcept { return m_valid; }
    explicit operator bool() const noexcept { return m_valid; }

    std::string href() const;
    std::string to_string() const { return href(); }
    std::string origin() const;
    std::string protocol() const { return m_scheme.empty() ? std::string() : m_scheme + ":"; }
    const std::string& scheme() const noexcept { return m_scheme; }
    const std::string& username() const noexcept { return m_username; }
    const std::string& password() const noexcept { return m_password; }
    std::string host() const;
    const std::string& hostname() const noexcept { return m_hostname; }
    std::string bare_hostname() const;
    const std::string& port() const noexcept { return m_port; }
    std::uint16_t port_number() const noexcept;
    const std::string& pathname() const noexcept { return m_path; }
    std::string search() const;
    const std::string& hash() const noexcept { return m_fragment; }
    std::string path_and_query() const;
    bool is_special() const noexcept;
    bool secure() const noexcept { return m_scheme == "https" || m_scheme == "wss"; }

    URLSearchParams& search_params();
    const URLSearchParams& search_params() const { return m_params; }

    URL& set_protocol(const std::string& value);
    URL& set_username(const std::string& value);
    URL& set_password(const std::string& value);
    URL& set_hostname(const std::string& value);
    URL& set_port(const std::string& value);
    URL& set_port(std::uint16_t value) { return set_port(value == 0 ? std::string() : std::to_string(value)); }
    URL& set_pathname(const std::string& value);
    URL& set_search(const std::string& value);
    URL& set_hash(const std::string& value);

    friend bool operator==(const URL& a, const URL& b) { return a.href() == b.href(); }
    friend bool operator!=(const URL& a, const URL& b) { return !(a == b); }

private:
    bool            m_valid = false;
    bool            m_opaque = false;
    bool            m_has_authority = false;
    bool            m_params_dirty = false;
    std::string     m_scheme;
    std::string     m_username;
    std::string     m_password;
    std::string     m_hostname;
    std::string     m_port;
    std::string     m_path;
    std::string     m_query;
    bool            m_has_query = false;
    std::string     m_fragment;
    URLSearchParams m_params;

};

namespace querystring {

std::string escape(const std::string& s);
std::string unescape(const std::string& s);
std::vector<std::pair<std::string, std::string>> parse(const std::string& query, char separator = '&', char equals = '=');
std::string stringify(const std::vector<std::pair<std::string, std::string>>& entries, char separator = '&', char equals = '=');

} // namespace querystring

std::string encode_uri_component(const std::string& s);
std::string decode_uri_component(const std::string& s);
std::string encode_uri(const std::string& s);

} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_URL_HPP
