#include "fizmo_library.hpp"
#include "format_util.hpp"
#include "../Normal/png_codec.hpp"

namespace fizmo {
namespace images {

namespace {

struct IcoEntry {
    unsigned      width;
    unsigned      height;
    unsigned      bits;
    std::uint32_t size;
    std::uint32_t offset;
};

bool ico_entries(const std::uint8_t* data, std::size_t size, std::vector<IcoEntry>& entries) {
    if (!is_ico(data, size)) return false;
    fmt::Reader r(data, size);
    r.skip(4);
    const unsigned count = r.le16();

    for (unsigned i = 0; i < count; ++i) {
        IcoEntry e;
        e.width = r.u8();
        e.height = r.u8();
        r.u8();
        r.u8();
        r.le16();
        e.bits = r.le16();
        e.size = r.le32();
        e.offset = r.le32();
        if (!r.ok()) return false;
        if (!e.width) e.width = 256;
        if (!e.height) e.height = 256;
        if (e.offset >= size || e.size > size - e.offset) continue;
        entries.push_back(e);
    }

    return !entries.empty();
}

bool ico_decode_entry(const std::uint8_t* data, const IcoEntry& e, BitmapImage& out, std::string* error) {
    const std::uint8_t* p = data + e.offset;
    if (is_png(p, e.size)) return decode_png(p, e.size, out, error);
    fmt::Rgba img;
    if (!fmt::decode_dib(p, e.size, 0, 0, true, img, error)) return false;
    out = img.to_bitmap();
    return true;
}

} // namespace

bool is_ico(const std::uint8_t* data, std::size_t size) noexcept {
    if (size < 22) return false;
    if (data[0] != 0 || data[1] != 0 || (data[2] != 1 && data[2] != 2) || data[3] != 0) return false;
    const unsigned count = data[4] | (data[5] << 8);
    return count > 0 && 6 + count * 16u <= size && data[9] == 0;
}

bool decode_ico_all(const std::uint8_t* data, std::size_t size, std::vector<BitmapImage>& out, std::string* error) {
    out.clear();
    std::vector<IcoEntry> entries;
    if (!ico_entries(data, size, entries)) return fmt::fail(error, "not an ICO file");

    for (const auto& e : entries) {
        BitmapImage img;
        if (ico_decode_entry(data, e, img, error)) out.push_back(std::move(img));
    }

    return !out.empty() || fmt::fail(error, "no decodable images in ICO");
}

bool decode_ico(const std::uint8_t* data, std::size_t size, BitmapImage& out, std::string* error) {
    std::vector<IcoEntry> entries;
    if (!ico_entries(data, size, entries)) return fmt::fail(error, "not an ICO file");
    std::stable_sort(entries.begin(), entries.end(), [](const IcoEntry& a, const IcoEntry& b) {
        const std::uint64_t pa = static_cast<std::uint64_t>(a.width) * a.height, pb = static_cast<std::uint64_t>(b.width) * b.height;
        if (pa != pb) return pa > pb;
        return a.bits > b.bits;
    });

    for (const auto& e : entries)
        if (ico_decode_entry(data, e, out, error)) return true;

    return false;
}

bool encode_ico(const std::vector<BitmapImage>& images, std::vector<std::uint8_t>& out, std::string* error) {
    out.clear();
    if (images.empty() || images.size() > 65535) return fmt::fail(error, "ICO needs between 1 and 65535 images");
    std::vector<std::vector<std::uint8_t>> blobs(images.size());

    for (std::size_t i = 0; i < images.size(); ++i) {
        const BitmapImage& img = images[i];
        if (!img.is_valid_image() || img.width() > 256 || img.height() > 256) return fmt::fail(error, "ICO images must be at most 256x256");
        if (!encode_png(img, blobs[i], true, 9)) return fmt::fail(error, "PNG encode failed");
    }

    fmt::put_le16(out, 0);
    fmt::put_le16(out, 1);
    fmt::put_le16(out, static_cast<std::uint32_t>(images.size()));
    std::uint32_t offset = static_cast<std::uint32_t>(6 + 16 * images.size());

    for (std::size_t i = 0; i < images.size(); ++i) {
        out.push_back(static_cast<std::uint8_t>(images[i].width() & 255));
        out.push_back(static_cast<std::uint8_t>(images[i].height() & 255));
        out.push_back(0);
        out.push_back(0);
        fmt::put_le16(out, 1);
        fmt::put_le16(out, 32);
        fmt::put_le32(out, static_cast<std::uint32_t>(blobs[i].size()));
        fmt::put_le32(out, offset);
        offset += static_cast<std::uint32_t>(blobs[i].size());
    }

    for (const auto& b : blobs) out.insert(out.end(), b.begin(), b.end());
    return true;
}

} // namespace images
} // namespace fizmo
