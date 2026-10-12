#include "fizmo_library.hpp"
#include "format_util.hpp"

#include <zlib.h>

namespace fizmo {
namespace images {
namespace fmt {

bool zlib_inflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out, std::size_t expected) {
    out.clear();
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 15 + 32) != Z_OK) return false;
    out.resize(expected ? expected : std::max<std::size_t>(size * 4, 1024));
    zs.next_in = const_cast<Bytef*>(data);
    zs.avail_in = static_cast<uInt>(size);
    int r = Z_OK;

    while (true) {
        if (zs.total_out >= out.size()) {
            if (expected) break;
            out.resize(out.size() * 2);
        }

        zs.next_out = out.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
        r = inflate(&zs, Z_NO_FLUSH);
        if (r == Z_STREAM_END) break;
        if (r != Z_OK) break;
        if (zs.avail_in == 0 && zs.avail_out != 0) break;
    }

    const std::size_t produced = zs.total_out;
    inflateEnd(&zs);
    out.resize(produced);
    if (expected) return produced >= expected;
    return r == Z_STREAM_END || r == Z_OK || r == Z_BUF_ERROR;
}

bool zlib_deflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out, int level) {
    uLongf bound = compressBound(static_cast<uLong>(size));
    out.resize(bound);
    if (compress2(out.data(), &bound, data, static_cast<uLong>(size), level) != Z_OK) return false;
    out.resize(bound);
    return true;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc) { return static_cast<std::uint32_t>(::crc32(crc, data, static_cast<uInt>(size))); }

} // namespace fmt
} // namespace images
} // namespace fizmo
