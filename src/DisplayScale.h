#pragma once
#include <Rect.h>
#include <algorithm>
#include <cmath>

namespace DisplayScale {
// Air/OS exposes device pixels in the mode and logical pixels in BScreen's
// frame. On unscaled Haiku they agree. Reject inconsistent/invalid geometry.
inline float FromDimensions(float deviceWidth, float deviceHeight,
    float logicalWidth, float logicalHeight) {
    if (logicalWidth <= 0 || logicalHeight <= 0) return 1;
    float x = deviceWidth / logicalWidth, y = deviceHeight / logicalHeight;
    if (!std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1
        || x > 4 || y > 4 || std::abs(x - y) > .02f) return 1;
    return std::round(std::min(x, y) * 100) / 100;
}

inline BRect Pixels(BRect logical, float scale) {
    // BRect includes its last pixel: a 1001-pixel view needs 2002 pixels at 2x.
    return BRect(std::floor(logical.left * scale), std::floor(logical.top * scale),
        std::ceil((logical.right + 1) * scale) - 1,
        std::ceil((logical.bottom + 1) * scale) - 1);
}
}
