#include "backend/linux/LinuxWindow.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{

std::uint32_t windowsVirtualKey(KeySym key)
{
    if (key >= XK_a && key <= XK_z) return static_cast<std::uint32_t>('A' + key - XK_a);
    if (key >= XK_A && key <= XK_Z) return static_cast<std::uint32_t>(key);
    if (key >= XK_0 && key <= XK_9) return static_cast<std::uint32_t>(key);
    if (key >= XK_F1 && key <= XK_F24) return static_cast<std::uint32_t>(0x70 + key - XK_F1);
    if (key >= XK_KP_0 && key <= XK_KP_9) return static_cast<std::uint32_t>(0x60 + key - XK_KP_0);
    switch (key) {
    case XK_BackSpace: return 0x08;
    case XK_Tab: case XK_ISO_Left_Tab: return 0x09;
    case XK_Return: case XK_KP_Enter: return 0x0D;
    case XK_Shift_L: return 0xA0;
    case XK_Shift_R: return 0xA1;
    case XK_Control_L: return 0xA2;
    case XK_Control_R: return 0xA3;
    case XK_Alt_L: case XK_Meta_L: return 0xA4;
    case XK_Alt_R: case XK_Meta_R: return 0xA5;
    case XK_Super_L: return 0x5B;
    case XK_Super_R: return 0x5C;
    case XK_Caps_Lock: return 0x14;
    case XK_Escape: return 0x1B;
    case XK_space: return 0x20;
    case XK_Page_Up: return 0x21;
    case XK_Page_Down: return 0x22;
    case XK_End: return 0x23;
    case XK_Home: return 0x24;
    case XK_Left: return 0x25;
    case XK_Up: return 0x26;
    case XK_Right: return 0x27;
    case XK_Down: return 0x28;
    case XK_Print: return 0x2C;
    case XK_Insert: return 0x2D;
    case XK_Delete: return 0x2E;
    case XK_KP_Multiply: return 0x6A;
    case XK_KP_Add: return 0x6B;
    case XK_KP_Subtract: return 0x6D;
    case XK_KP_Decimal: return 0x6E;
    case XK_KP_Divide: return 0x6F;
    case XK_semicolon: return 0xBA;
    case XK_equal: return 0xBB;
    case XK_comma: return 0xBC;
    case XK_minus: return 0xBD;
    case XK_period: return 0xBE;
    case XK_slash: return 0xBF;
    case XK_grave: return 0xC0;
    case XK_bracketleft: return 0xDB;
    case XK_backslash: return 0xDC;
    case XK_bracketright: return 0xDD;
    case XK_apostrophe: return 0xDE;
    default: return 0;
    }
}

void emitUtf8(ege::WindowEventSink* sink, const char* text, int size)
{
    if (!sink || !text) return;
    for (int i = 0; i < size;) {
        const unsigned char first = static_cast<unsigned char>(text[i++]);
        std::uint32_t cp = first;
        int remaining = 0;
        if ((first & 0xE0u) == 0xC0u) { cp = first & 0x1Fu; remaining = 1; }
        else if ((first & 0xF0u) == 0xE0u) { cp = first & 0x0Fu; remaining = 2; }
        else if ((first & 0xF8u) == 0xF0u) { cp = first & 0x07u; remaining = 3; }
        else if (first >= 0x80u) continue;
        if (i + remaining > size) break;
        bool valid = true;
        for (int j = 0; j < remaining; ++j) {
            const unsigned char next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xC0u) != 0x80u) { valid = false; break; }
            cp = (cp << 6u) | (next & 0x3Fu);
        }
        if (valid && cp <= 0x10FFFFu && !(cp >= 0xD800u && cp <= 0xDFFFu)) sink->onText(cp);
    }
}

int egeButton(unsigned int button)
{
    switch (button) {
    case Button1: return 0;
    case Button3: return 1;
    case Button2: return 2;
    case 8: return 3;
    case 9: return 4;
    default: return -1;
    }
}

} // namespace

namespace ege
{
namespace backend
{

struct LinuxWindow::Impl
{
    Display* display = nullptr;
    ::Window window = 0;
    GC gc = nullptr;
    Atom wmDelete = None;
    XIM inputMethod = nullptr;
    XIC inputContext = nullptr;
    Cursor hiddenCursor = None;
    int width = 0;
    int height = 0;
    bool closed = true;
    WindowEventSink* sink = nullptr;
    std::array<Time, 5> clickTime{};
    std::array<int, 5> clickX{};
    std::array<int, 5> clickY{};
    std::array<bool, 256> keyStates{};
};

LinuxWindow::LinuxWindow() : impl_(new Impl) {}

LinuxWindow::~LinuxWindow()
{
    close();
}

bool LinuxWindow::primaryScreenSize(int* width, int* height)
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;
    const int screen = DefaultScreen(display);
    if (width) *width = DisplayWidth(display, screen);
    if (height) *height = DisplayHeight(display, screen);
    XCloseDisplay(display);
    return true;
}

bool LinuxWindow::create(int width, int height, const char* title,
    const WindowOptions& options, WindowEventSink* eventSink)
{
    close();
    if (width <= 0 || height <= 0) return false;
    impl_->display = XOpenDisplay(nullptr);
    if (!impl_->display) return false;
    const int screen = DefaultScreen(impl_->display);
    XSetWindowAttributes attributes{};
    attributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask
        | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
    attributes.override_redirect = False;
    impl_->window = XCreateWindow(impl_->display, RootWindow(impl_->display, screen),
        0, 0, static_cast<unsigned int>(width), static_cast<unsigned int>(height), 0,
        CopyFromParent, InputOutput, CopyFromParent, CWEventMask | CWOverrideRedirect, &attributes);
    if (!impl_->window) { close(); return false; }
    impl_->gc = XCreateGC(impl_->display, impl_->window, 0, nullptr);
    impl_->wmDelete = XInternAtom(impl_->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(impl_->display, impl_->window, &impl_->wmDelete, 1);
    impl_->closed = false;
    setTitle(title);
    if (options.borderless) {
        struct MotifHints { unsigned long flags, functions, decorations; long inputMode; unsigned long status; };
        const MotifHints hints{2, 0, 0, 0, 0};
        const Atom property = XInternAtom(impl_->display, "_MOTIF_WM_HINTS", False);
        XChangeProperty(impl_->display, impl_->window, property, property, 32,
            PropModeReplace, reinterpret_cast<const unsigned char*>(&hints), 5);
    }
    if (options.topmost) {
        const Atom state = XInternAtom(impl_->display, "_NET_WM_STATE", False);
        const Atom above = XInternAtom(impl_->display, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(impl_->display, impl_->window, state, XA_ATOM, 32,
            PropModeReplace, reinterpret_cast<const unsigned char*>(&above), 1);
    }
    XSetLocaleModifiers("");
    impl_->inputMethod = XOpenIM(impl_->display, nullptr, nullptr, nullptr);
    if (impl_->inputMethod) {
        impl_->inputContext = XCreateIC(impl_->inputMethod,
            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, impl_->window, XNFocusWindow, impl_->window, nullptr);
    }
    impl_->width = width;
    impl_->height = height;
    impl_->sink = eventSink;
    return true;
}

void LinuxWindow::show() { if (!impl_->closed) { XMapRaised(impl_->display, impl_->window); XFlush(impl_->display); } }
void LinuxWindow::hide() { if (!impl_->closed) { XUnmapWindow(impl_->display, impl_->window); XFlush(impl_->display); } }
void LinuxWindow::setTitle(const char* title)
{
    if (impl_->closed || !impl_->window) return;
    const char* value = title ? title : "";
    XStoreName(impl_->display, impl_->window, value);
    const Atom utf8 = XInternAtom(impl_->display, "UTF8_STRING", False);
    const Atom netName = XInternAtom(impl_->display, "_NET_WM_NAME", False);
    XChangeProperty(impl_->display, impl_->window, netName, utf8, 8,
        PropModeReplace, reinterpret_cast<const unsigned char*>(value),
        static_cast<int>(std::strlen(value)));
}
void LinuxWindow::setSize(int width, int height) { if (!impl_->closed && width > 0 && height > 0) XResizeWindow(impl_->display, impl_->window, width, height); }
void LinuxWindow::setPosition(int x, int y) { if (!impl_->closed) XMoveWindow(impl_->display, impl_->window, x, y); }

void LinuxWindow::setCursorVisible(bool visible)
{
    if (impl_->closed) return;
    if (visible) {
        XUndefineCursor(impl_->display, impl_->window);
    } else {
        if (impl_->hiddenCursor == None) {
            const char bits[1] = {0};
            Pixmap bitmap = XCreateBitmapFromData(impl_->display, impl_->window, bits, 1, 1);
            XColor black{};
            impl_->hiddenCursor = XCreatePixmapCursor(impl_->display, bitmap, bitmap, &black, &black, 0, 0);
            XFreePixmap(impl_->display, bitmap);
        }
        XDefineCursor(impl_->display, impl_->window, impl_->hiddenCursor);
    }
    XFlush(impl_->display);
}

void LinuxWindow::close()
{
    if (impl_->inputContext) { XDestroyIC(impl_->inputContext); impl_->inputContext = nullptr; }
    if (impl_->inputMethod) { XCloseIM(impl_->inputMethod); impl_->inputMethod = nullptr; }
    if (impl_->display && impl_->hiddenCursor != None) XFreeCursor(impl_->display, impl_->hiddenCursor);
    impl_->hiddenCursor = None;
    if (impl_->display && impl_->gc) XFreeGC(impl_->display, impl_->gc);
    impl_->gc = nullptr;
    if (impl_->display && impl_->window) XDestroyWindow(impl_->display, impl_->window);
    impl_->window = 0;
    if (impl_->display) XCloseDisplay(impl_->display);
    impl_->display = nullptr;
    impl_->width = 0;
    impl_->height = 0;
    impl_->sink = nullptr;
    impl_->keyStates.fill(false);
    impl_->closed = true;
}

bool LinuxWindow::isClosed() const { return impl_->closed; }

void LinuxWindow::processEvents()
{
    if (impl_->closed) return;
    while (XPending(impl_->display) > 0) {
        XEvent event{};
        XNextEvent(impl_->display, &event);
        if (XFilterEvent(&event, impl_->window)) continue;
        switch (event.type) {
        case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == impl_->wmDelete
                && (!impl_->sink || impl_->sink->onCloseRequested())) { close(); return; }
            break;
        case ConfigureNotify:
            if (event.xconfigure.width != impl_->width || event.xconfigure.height != impl_->height) {
                impl_->width = event.xconfigure.width;
                impl_->height = event.xconfigure.height;
                if (impl_->sink) impl_->sink->onResize(impl_->width, impl_->height);
            }
            break;
        case FocusIn: if (impl_->inputContext) XSetICFocus(impl_->inputContext); break;
        case FocusOut:
            if (impl_->inputContext) XUnsetICFocus(impl_->inputContext);
            if (impl_->sink) {
                for (std::size_t key = 0; key < impl_->keyStates.size(); ++key) {
                    if (impl_->keyStates[key]) impl_->sink->onKey(static_cast<std::uint32_t>(key), false, false);
                }
            }
            impl_->keyStates.fill(false);
            break;
        case MotionNotify: if (impl_->sink) impl_->sink->onMouseMove(event.xmotion.x, event.xmotion.y); break;
        case ButtonPress:
        case ButtonRelease: {
            if (!impl_->sink) break;
            const bool pressed = event.type == ButtonPress;
            if (pressed && (event.xbutton.button == Button4 || event.xbutton.button == Button5
                || event.xbutton.button == 6 || event.xbutton.button == 7)) {
                const float dx = event.xbutton.button == 6 ? -1.0f : (event.xbutton.button == 7 ? 1.0f : 0.0f);
                const float dy = event.xbutton.button == Button4 ? 1.0f : (event.xbutton.button == Button5 ? -1.0f : 0.0f);
                impl_->sink->onMouseWheel(dx, dy, event.xbutton.x, event.xbutton.y);
                break;
            }
            const int button = egeButton(event.xbutton.button);
            if (button < 0) break;
            int clicks = 1;
            if (pressed && event.xbutton.time - impl_->clickTime[button] <= 400
                && std::abs(event.xbutton.x - impl_->clickX[button]) <= 4
                && std::abs(event.xbutton.y - impl_->clickY[button]) <= 4) clicks = 2;
            if (pressed) { impl_->clickTime[button] = event.xbutton.time; impl_->clickX[button] = event.xbutton.x; impl_->clickY[button] = event.xbutton.y; }
            impl_->sink->onMouseButton(button, pressed, event.xbutton.x, event.xbutton.y, clicks);
            break;
        }
        case KeyPress:
        case KeyRelease: {
            bool pressed = event.type == KeyPress;
            bool repeat = false;
            if (!pressed && XPending(impl_->display) > 0) {
                XEvent next{};
                XPeekEvent(impl_->display, &next);
                if (next.type == KeyPress && next.xkey.keycode == event.xkey.keycode
                    && next.xkey.time == event.xkey.time) { XNextEvent(impl_->display, &event); repeat = pressed = true; }
            }
            KeySym symbol = NoSymbol;
            char buffer[64]{};
            int length = 0;
            if (pressed && impl_->inputContext) {
                Status status = 0;
                length = Xutf8LookupString(impl_->inputContext, &event.xkey, buffer,
                    static_cast<int>(sizeof(buffer)), &symbol, &status);
                if (status == XBufferOverflow) length = 0;
            } else {
                length = XLookupString(&event.xkey, buffer, static_cast<int>(sizeof(buffer)), &symbol, nullptr);
            }
            const std::uint32_t key = windowsVirtualKey(symbol);
            if (key < impl_->keyStates.size()) impl_->keyStates[key] = pressed;
            if (impl_->sink && key) impl_->sink->onKey(key, pressed, repeat);
            if (pressed && length > 0) emitUtf8(impl_->sink, buffer, length);
            break;
        }
        default: break;
        }
    }
}

void LinuxWindow::present(const std::uint32_t* pixels, int width, int height,
    std::size_t strideBytes)
{
    if (impl_->closed || !pixels || width <= 0 || height <= 0) return;
    const int screen = DefaultScreen(impl_->display);
    XImage* image = XCreateImage(impl_->display, DefaultVisual(impl_->display, screen),
        static_cast<unsigned int>(DefaultDepth(impl_->display, screen)), ZPixmap, 0, nullptr,
        static_cast<unsigned int>(width), static_cast<unsigned int>(height), 32, 0);
    if (!image) return;
    image->data = static_cast<char*>(std::malloc(static_cast<std::size_t>(image->bytes_per_line) * height));
    if (!image->data) { image->data = nullptr; XDestroyImage(image); return; }
    const int copyBytes = std::min(image->bytes_per_line, width * 4);
    for (int y = 0; y < height; ++y) {
        std::memcpy(image->data + static_cast<std::size_t>(y) * image->bytes_per_line,
            reinterpret_cast<const unsigned char*>(pixels) + static_cast<std::size_t>(y) * strideBytes,
            static_cast<std::size_t>(copyBytes));
    }
    XPutImage(impl_->display, impl_->window, impl_->gc, image, 0, 0, 0, 0,
        static_cast<unsigned int>(std::min(width, impl_->width)),
        static_cast<unsigned int>(std::min(height, impl_->height)));
    XDestroyImage(image);
    XFlush(impl_->display);
}

void* LinuxWindow::getNativeHandle() const { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(impl_->window)); }
int LinuxWindow::getWidth() const { return impl_->width; }
int LinuxWindow::getHeight() const { return impl_->height; }

bool LinuxWindow::inputBox(const char* title, const char* prompt, std::string* value)
{
    if (!value) return false;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;
    const int screen = DefaultScreen(display);
    ::Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 100, 100, 520, 90, 1,
        BlackPixel(display, screen), WhitePixel(display, screen));
    const std::string caption = std::string(title ? title : "Input") + " - " + (prompt ? prompt : "");
    XStoreName(display, window, caption.c_str());
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDelete, 1);
    XMapRaised(display, window);
    GC gc = XCreateGC(display, window, 0, nullptr);
    std::string input = *value;
    bool accepted = false;
    bool done = false;
    while (!done) {
        XEvent event{};
        XNextEvent(display, &event);
        if (event.type == ClientMessage) done = true;
        else if (event.type == Expose) {
            XClearWindow(display, window);
            XDrawString(display, window, gc, 12, 35, prompt ? prompt : "", static_cast<int>(std::strlen(prompt ? prompt : "")));
            XDrawString(display, window, gc, 12, 65, input.c_str(), static_cast<int>(input.size()));
        } else if (event.type == KeyPress) {
            char bytes[64]{};
            KeySym symbol = NoSymbol;
            const int length = XLookupString(&event.xkey, bytes, sizeof(bytes), &symbol, nullptr);
            if (symbol == XK_Return || symbol == XK_KP_Enter) { accepted = true; done = true; }
            else if (symbol == XK_Escape) done = true;
            else if (symbol == XK_BackSpace && !input.empty()) {
                std::size_t start = input.size() - 1;
                while (start > 0 && (static_cast<unsigned char>(input[start]) & 0xC0u) == 0x80u) --start;
                input.erase(start);
            }
            else if (length > 0) input.append(bytes, static_cast<std::size_t>(length));
            XClearArea(display, window, 0, 0, 0, 0, True);
        }
    }
    if (accepted) *value = input;
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return accepted;
}

} // namespace backend
} // namespace ege
