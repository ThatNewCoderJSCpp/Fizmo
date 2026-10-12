#ifndef FIZMO_ASYNC_COMPOSED_HPP
#define FIZMO_ASYNC_COMPOSED_HPP

#include "stream.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

namespace detail {

template <typename Stream>
struct ReadExactOp : std::enable_shared_from_this<ReadExactOp<Stream>> {
    Stream&       stream;
    MutableBuffer target;
    std::size_t   done = 0;
    IoHandler     handler;

    ReadExactOp(Stream& s, MutableBuffer b, IoHandler h) : stream(s), target(b), handler(std::move(h)) {}

    void step() {
        auto self = this->shared_from_this();
        stream.async_read_some(target + done, [self](std::error_code ec, std::size_t n) { self->on_read(ec, n); });
    }

    void on_read(std::error_code ec, std::size_t n) {
        done += n;
        if (!ec && done < target.size()) { step(); return; }
        auto h = std::move(handler);
        h(ec, done);
    }
};

template <typename Stream>
struct WriteAllOp : std::enable_shared_from_this<WriteAllOp<Stream>> {
    Stream&     stream;
    ConstBuffer source;
    std::size_t done = 0;
    IoHandler   handler;

    WriteAllOp(Stream& s, ConstBuffer b, IoHandler h) : stream(s), source(b), handler(std::move(h)) {}

    void step() {
        auto self = this->shared_from_this();
        stream.async_write_some(source + done, [self](std::error_code ec, std::size_t n) { self->on_write(ec, n); });
    }

    void on_write(std::error_code ec, std::size_t n) {
        done += n;
        if (!ec && n == 0 && done < source.size()) ec = error::closed;
        if (!ec && done < source.size()) { step(); return; }
        auto h = std::move(handler);
        h(ec, done);
    }
};

template <typename Stream, typename Storage>
struct OwnedWriteOp : std::enable_shared_from_this<OwnedWriteOp<Stream, Storage>> {
    Stream&     stream;
    Storage     data;
    std::size_t done = 0;
    IoHandler   handler;

    OwnedWriteOp(Stream& s, Storage d, IoHandler h) : stream(s), data(std::move(d)), handler(std::move(h)) {}

    std::size_t size() const noexcept { return data.size(); }

    void step() {
        auto self = this->shared_from_this();
        stream.async_write_some(ConstBuffer(data.data(), data.size()) + done, [self](std::error_code ec, std::size_t n) { self->on_write(ec, n); });
    }

    void on_write(std::error_code ec, std::size_t n) {
        done += n;
        if (!ec && n == 0 && done < size()) ec = error::closed;
        if (!ec && done < size()) { step(); return; }
        auto h = std::move(handler);
        h(ec, done);
    }
};

template <typename Stream, typename Dynamic>
struct ReadUntilOp : std::enable_shared_from_this<ReadUntilOp<Stream, Dynamic>> {
    Stream&     stream;
    Dynamic&    storage;
    std::string delimiter;
    std::size_t max_size;
    std::size_t search_from = 0;
    IoHandler   handler;
    char        chunk[4096];

    ReadUntilOp(Stream& s, Dynamic& d, std::string delim, std::size_t max, IoHandler h)
        : stream(s), storage(d), delimiter(std::move(delim)), max_size(max), handler(std::move(h)) {}

    bool find(std::size_t& end) {
        if (delimiter.empty()) { end = 0; return true; }
        if (storage.size() < delimiter.size()) return false;
        const auto* base = reinterpret_cast<const char*>(storage.data());
        const auto* last = base + storage.size();
        const auto* it = std::search(base + search_from, last, delimiter.begin(), delimiter.end());

        if (it != last) {
            end = static_cast<std::size_t>(it - base) + delimiter.size();
            return true;
        }

        search_from = storage.size() >= delimiter.size() ? storage.size() - delimiter.size() + 1 : 0;
        return false;
    }

    void start() {
        std::size_t end = 0;

        if (find(end)) {
            auto self = this->shared_from_this();
            stream.context().post([self, end]() { auto h = std::move(self->handler); h(std::error_code(), end); });
            return;
        }

        step();
    }

    void step() {
        if (storage.size() >= max_size) {
            auto h = std::move(handler);
            stream.context().post([h = std::move(h)]() mutable { h(make_error_code(error::buffer_full), 0); });
            return;
        }

        auto self = this->shared_from_this();
        const std::size_t room = std::min(sizeof(chunk), max_size - storage.size());
        stream.async_read_some(MutableBuffer(chunk, room), [self](std::error_code ec, std::size_t n) { self->on_read(ec, n); });
    }

    void on_read(std::error_code ec, std::size_t n) {
        if (n > 0) {
            const auto* p = reinterpret_cast<const typename Dynamic::value_type*>(chunk);
            storage.insert(storage.end(), p, p + n);
        }

        std::size_t end = 0;

        if (find(end)) {
            auto h = std::move(handler);
            h(std::error_code(), end);
            return;
        }

        if (ec) {
            auto h = std::move(handler);
            h(ec, 0);
            return;
        }

        step();
    }
};

} // namespace detail

template <typename Stream>
void async_read(Stream& stream, MutableBuffer buffer, IoHandler handler) {
    auto op = std::make_shared<detail::ReadExactOp<Stream>>(stream, buffer, std::move(handler));

    if (buffer.size() == 0) {
        stream.context().post([op]() { auto h = std::move(op->handler); h(std::error_code(), 0); });
        return;
    }

    op->step();
}

template <typename Stream>
void async_write(Stream& stream, ConstBuffer buffer, IoHandler handler) {
    auto op = std::make_shared<detail::WriteAllOp<Stream>>(stream, buffer, std::move(handler));

    if (buffer.size() == 0) {
        stream.context().post([op]() { auto h = std::move(op->handler); h(std::error_code(), 0); });
        return;
    }

    op->step();
}

template <typename Stream>
void async_write(Stream& stream, std::string data, IoHandler handler) {
    auto op = std::make_shared<detail::OwnedWriteOp<Stream, std::string>>(stream, std::move(data), std::move(handler));

    if (op->size() == 0) {
        stream.context().post([op]() { auto h = std::move(op->handler); h(std::error_code(), 0); });
        return;
    }

    op->step();
}

template <typename Stream>
void async_write(Stream& stream, std::vector<std::uint8_t> data, IoHandler handler) {
    auto op = std::make_shared<detail::OwnedWriteOp<Stream, std::vector<std::uint8_t>>>(stream, std::move(data), std::move(handler));

    if (op->size() == 0) {
        stream.context().post([op]() { auto h = std::move(op->handler); h(std::error_code(), 0); });
        return;
    }

    op->step();
}

template <typename Stream>
void async_read_until(Stream& stream, std::string& storage, const std::string& delimiter, IoHandler handler, std::size_t max_size = 1u << 20) {
    auto op = std::make_shared<detail::ReadUntilOp<Stream, std::string>>(stream, storage, delimiter, max_size, std::move(handler));
    op->start();
}

template <typename Stream>
void async_read_until(Stream& stream, std::vector<std::uint8_t>& storage, const std::string& delimiter, IoHandler handler, std::size_t max_size = 1u << 20) {
    auto op = std::make_shared<detail::ReadUntilOp<Stream, std::vector<std::uint8_t>>>(stream, storage, delimiter, max_size, std::move(handler));
    op->start();
}

template <typename Stream>
void async_read_until(Stream& stream, std::string& storage, char delimiter, IoHandler handler, std::size_t max_size = 1u << 20) {
    async_read_until(stream, storage, std::string(1, delimiter), std::move(handler), max_size);
}

template <typename Stream>
void async_read_line(Stream& stream, std::string& storage, UniqueFunction<void(std::error_code, std::string)> handler, std::size_t max_size = 1u << 20) {
    async_read_until(stream, storage, "\n", [&storage, h = std::move(handler)](std::error_code ec, std::size_t n) mutable {
        if (ec) { h(ec, std::string()); return; }
        std::string line = storage.substr(0, n);
        storage.erase(0, n);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        h(ec, std::move(line));
    }, max_size);
}

template <typename Stream>
std::size_t read(Stream& stream, MutableBuffer buffer, std::error_code& ec, int timeout_ms = -1) {
    std::size_t done = 0;
    ec.clear();

    while (done < buffer.size()) {
        const std::size_t n = stream.read_some(buffer + done, ec, timeout_ms);
        done += n;
        if (ec) break;
    }

    return done;
}

template <typename Stream>
std::size_t write(Stream& stream, ConstBuffer buffer, std::error_code& ec, int timeout_ms = -1) {
    std::size_t done = 0;
    ec.clear();

    while (done < buffer.size()) {
        const std::size_t n = stream.write_some(buffer + done, ec, timeout_ms);
        done += n;
        if (ec) break;
        if (n == 0) { ec = error::closed; break; }
    }

    return done;
}

template <typename Stream>
std::size_t read_until(Stream& stream, std::string& storage, const std::string& delimiter, std::error_code& ec, int timeout_ms = -1, std::size_t max_size = 1u << 20) {
    ec.clear();
    std::size_t from = 0;

    for (;;) {
        const auto pos = storage.find(delimiter, from);
        if (pos != std::string::npos) return pos + delimiter.size();
        from = storage.size() >= delimiter.size() ? storage.size() - delimiter.size() + 1 : 0;
        if (storage.size() >= max_size) { ec = error::buffer_full; return 0; }
        char chunk[4096];
        const std::size_t n = stream.read_some(MutableBuffer(chunk, std::min(sizeof(chunk), max_size - storage.size())), ec, timeout_ms);
        storage.append(chunk, n);
        if (ec) return 0;
    }
}

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_COMPOSED_HPP
