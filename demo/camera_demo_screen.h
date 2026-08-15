#pragma once

#include <graphics.h>

#include <algorithm>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace ege
{

inline void cameraDemoAvailableScreenSize(int* width, int* height)
{
#ifdef _WIN32
    RECT workArea = {};
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0)) {
        *width = workArea.right - workArea.left;
        *height = workArea.bottom - workArea.top;
        return;
    }
#elif defined(__APPLE__)
    const CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    if (bounds.size.width > 0 && bounds.size.height > 0) {
        *width = static_cast<int>(bounds.size.width);
        *height = static_cast<int>(bounds.size.height);
        return;
    }
#endif

    // Portable fallback when the platform has no work-area query.  Keeping at
    // least the current EGE canvas prevents a resize from collapsing.
    *width = (std::max)(getwidth(), 640);
    *height = (std::max)(getheight(), 480);
}

} // namespace ege
