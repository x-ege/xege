#pragma once

#include <cstddef>
#include <cstdint>

namespace ege {

struct WindowOptions {
    bool borderless = false;
    bool topmost = false;
};

struct WindowEventSink {
    virtual ~WindowEventSink() = default;
    // Return true to accept the native close request. The internal interface
    // supports rejection for embedders/tests; EGE's public void
    // SetCloseHandler callback is a notification and accepts the close after
    // invoking the user handler, matching Win32 WM_CLOSE processing.
    virtual bool onCloseRequested() = 0;
    virtual void onResize(int width, int height) = 0;
    virtual void onKey(std::uint32_t key, bool pressed, bool repeat) = 0;
    virtual void onText(std::uint32_t codepoint) = 0;
    virtual void onMouseMove(int x, int y) = 0;
    // button uses the EGE order left/right/middle/X1/X2. clickCount is at
    // least one and lets the frontend preserve native double-click events.
    virtual void onMouseButton(int button, bool pressed, int x, int y,
                               int clickCount) = 0;
    virtual void onMouseWheel(float deltaX, float deltaY, int x, int y) = 0;
};

class Window {
public:
    virtual ~Window() = default;

    virtual bool create(int width, int height, const char* title,
                        const WindowOptions& options,
                        WindowEventSink* eventSink) = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void setTitle(const char* title) = 0;
    virtual void setSize(int width, int height) = 0;
    virtual void setPosition(int x, int y) = 0;
    virtual void setCursorVisible(bool visible) = 0;
    virtual void close() = 0;
    virtual bool isClosed() const = 0;
    virtual void processEvents() = 0;

    // Present a tightly packed, top-down BGRA/PARGB CPU surface. Implementations
    // must consume the pixels before returning and must not retain the pointer.
    virtual void present(const std::uint32_t* pixels, int width, int height,
                         std::size_t strideBytes) = 0;

    virtual void* getNativeHandle() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

} // namespace ege
