#ifndef FIZMO_VIDEO_FRAME_TEXTURE_HPP
#define FIZMO_VIDEO_FRAME_TEXTURE_HPP

#include "video_texture.hpp"
#include "../Video/frame.hpp"

namespace fizmo {
namespace graphics {

inline VideoMatrix video_matrix(video::ColorMatrix m) noexcept {
    switch (m) {
        case video::ColorMatrix::bt601:  return VideoMatrix::BT601;
        case video::ColorMatrix::bt709:  return VideoMatrix::BT709;
        case video::ColorMatrix::bt2020: return VideoMatrix::BT2020;
        case video::ColorMatrix::rgb:    return VideoMatrix::RGB;
        default:                         return VideoMatrix::Unspecified;
    }
}

inline VideoRange video_range(video::ColorRange r) noexcept {
    return r == video::ColorRange::full ? VideoRange::Full : VideoRange::Limited;
}

inline VideoFrameView video_frame_view(const video::VideoFrame& frame) noexcept {
    VideoFrameView v;
    if (frame.empty() || frame.width == 0 || frame.height == 0) {
        v.plane_count = 0;
        return v;
    }
    v.width            = frame.width;
    v.height           = frame.height;
    v.bit_depth        = video::bit_depth(frame.format);
    v.bytes_per_sample = video::bytes_per_sample(frame.format);
    v.plane_count      = video::plane_count(frame.format);
    v.chroma_shift_x   = video::chroma_shift_x(frame.format);
    v.chroma_shift_y   = video::chroma_shift_y(frame.format);
    v.matrix           = video_matrix(frame.color.matrix);
    v.range            = video_range(frame.color.range);
    v.chroma_location  = frame.color.chroma_location;
    const int planes = v.plane_count < 3 ? v.plane_count : 3;
    for (int i = 0; i < planes; ++i) {
        v.planes[i].data   = frame.plane(i);
        v.planes[i].stride = frame.stride[i];
    }
    return v;
}

inline bool update_video_texture(VideoTexture& texture, const video::VideoFrame& frame) {
    return texture.update(video_frame_view(frame));
}

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_VIDEO_FRAME_TEXTURE_HPP
