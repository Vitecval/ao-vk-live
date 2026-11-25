#pragma once
#include <algorithm>
#include <cstdint>

namespace rvk::display {
struct TrackExtent {uint32_t width=0,height=0;};
// A configured client can legitimately need an outer window larger than the
// desktop (caption/frame included). Lift only the maximum tracking limit; do
// not change minimum size, maximized bounds, desktop mode or actual geometry.
// Required extents are bounded above the65535 client API limit to allow frame
// pixels without accepting wrapped/signed-invalid window coordinates.
inline TrackExtent ExpandTrackLimit(TrackExtent current,TrackExtent required) {
    if(!required.width || !required.height || required.width>131070 || required.height>131070)return current;
    return {std::max(current.width,required.width),std::max(current.height,required.height)};
}
} // namespace rvk::display
