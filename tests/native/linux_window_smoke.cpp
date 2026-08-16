#include "backend/linux/LinuxWindow.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
class RecordingSink final : public ege::WindowEventSink
{
public:
    bool onCloseRequested() override { ++closes; return allowClose; }
    void onResize(int width, int height) override { ++resizes; lastWidth = width; lastHeight = height; }
    void onKey(std::uint32_t key, bool pressed, bool repeat) override
    { keys.push_back(key); keyPressed.push_back(pressed); repeated |= repeat; }
    void onText(std::uint32_t codepoint) override { text.push_back(codepoint); }
    void onMouseMove(int, int) override { ++moves; }
    void onMouseButton(int button, bool, int, int, int clicks) override
    { lastButton = button; maxClicks = clicks > maxClicks ? clicks : maxClicks; }
    void onMouseWheel(float, float dy, int, int) override { wheel += dy; }

    int closes = 0, resizes = 0, moves = 0, lastWidth = 0, lastHeight = 0;
    int lastButton = -1, maxClicks = 0;
    float wheel = 0;
    bool allowClose = false, repeated = false;
    std::vector<std::uint32_t> keys, text;
    std::vector<bool> keyPressed;
};

int fail(const char* message)
{
    std::cerr << "LinuxWindow smoke failed: " << message << '\n';
    return 1;
}

void send(Display* display, ::Window window, XEvent* event, long mask)
{
    event->xany.display = display;
    event->xany.window = window;
    XSendEvent(display, window, False, mask, event);
    XSync(display, False);
}
}

int main()
{
    int screenWidth = 0, screenHeight = 0;
    if (!ege::backend::LinuxWindow::primaryScreenSize(&screenWidth, &screenHeight)
        || screenWidth <= 0 || screenHeight <= 0) return fail("screen size unavailable");

    RecordingSink sink;
    ege::backend::LinuxWindow window;
    if (!window.create(96, 64, "EGE Linux smoke", {}, &sink)) return fail("create failed");
    if (window.isClosed() || window.getWidth() != 96 || window.getHeight() != 64) return fail("initial size invalid");
    window.show();
    window.setTitle("EGE native Xlib smoke");
    window.setPosition(12, 12);
    window.setSize(80, 48);

    Display* display = XOpenDisplay(nullptr);
    if (!display) return fail("second X connection failed");
    const ::Window native = static_cast<::Window>(reinterpret_cast<std::uintptr_t>(window.getNativeHandle()));
    XWindowAttributes attributes{};
    XGetWindowAttributes(display, native, &attributes);
    if (attributes.map_state == IsUnmapped) return fail("window was not mapped");

    XEvent motion{};
    motion.type = MotionNotify;
    motion.xmotion.x = 7;
    motion.xmotion.y = 8;
    send(display, native, &motion, PointerMotionMask);

    XEvent button{};
    button.type = ButtonPress;
    button.xbutton.button = Button1;
    button.xbutton.x = 7;
    button.xbutton.y = 8;
    button.xbutton.time = 100;
    send(display, native, &button, ButtonPressMask);
    button.xbutton.time = 200;
    send(display, native, &button, ButtonPressMask);
    button.xbutton.button = Button4;
    send(display, native, &button, ButtonPressMask);

    XEvent key{};
    key.type = KeyPress;
    key.xkey.keycode = XKeysymToKeycode(display, XK_a);
    key.xkey.state = 0;
    send(display, native, &key, KeyPressMask);
    key.type = KeyRelease;
    send(display, native, &key, KeyReleaseMask);

    std::vector<std::uint32_t> pixels(80 * 48, 0xFF336699U);
    window.present(pixels.data(), 80, 48, 80 * sizeof(std::uint32_t));
    window.processEvents();
    if (window.getWidth() != 80 || window.getHeight() != 48 || sink.resizes == 0) return fail("resize event missing");
    if (sink.moves == 0 || sink.lastButton != 0 || sink.maxClicks != 2 || sink.wheel <= 0) return fail("mouse mapping invalid");
    if (sink.keys.size() < 2 || sink.keys.front() != 'A' || sink.keyPressed.front() != true) return fail("key mapping invalid");

    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XEvent close{};
    close.type = ClientMessage;
    close.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
    close.xclient.format = 32;
    close.xclient.data.l[0] = static_cast<long>(wmDelete);
    send(display, native, &close, NoEventMask);
    window.processEvents();
    if (window.isClosed() || sink.closes != 1) return fail("rejected close was not preserved");
    sink.allowClose = true;
    send(display, native, &close, NoEventMask);
    window.processEvents();
    if (!window.isClosed() || sink.closes != 2) return fail("accepted close did not close");
    XCloseDisplay(display);
    std::cout << "LinuxWindow smoke passed\n";
    return 0;
}
