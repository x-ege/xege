#include "backend/macos/MacWindow.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

class RecordingSink final : public ege::WindowEventSink
{
public:
    bool onCloseRequested() override
    {
        ++closeRequests;
        return allowClose;
    }
    void onResize(int width, int height) override
    {
        ++resizeEvents;
        lastWidth  = width;
        lastHeight = height;
    }
    void onKey(std::uint32_t key, bool pressed, bool) override
    {
        ++keyEvents;
        recordedKeys.emplace_back(key, pressed);
    }
    void onText(std::uint32_t) override { ++textEvents; }
    void onMouseMove(int, int) override { ++mouseMoveEvents; }
    void onMouseButton(int button, bool, int, int, int clickCount) override
    {
        ++mouseButtonEvents;
        lastMouseButton = button;
        lastClickCount = clickCount;
        maxClickCount = std::max(maxClickCount, clickCount);
    }
    void onMouseWheel(float, float, int, int) override { ++mouseWheelEvents; }

    int closeRequests{0};
    int resizeEvents{0};
    int keyEvents{0};
    int textEvents{0};
    int mouseMoveEvents{0};
    int mouseButtonEvents{0};
    int mouseWheelEvents{0};
    int lastWidth{0};
    int lastHeight{0};
    int lastMouseButton{-1};
    int lastClickCount{0};
    int maxClickCount{0};
    std::vector<std::pair<std::uint32_t, bool>> recordedKeys;
    bool allowClose{true};
};

bool hasWindowServerSession()
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == nullptr) {
        return false;
    }
    CFRelease(session);
    return true;
}

int fail(const char* message)
{
    std::cerr << "MacWindow smoke failed: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    @autoreleasepool {
        // The smoke window must be observable by WindowServer without taking
        // keyboard focus away from the developer's current application.
        setenv("EGE_MACOS_TEST_NO_ACTIVATE", "1", 1);

        if (![NSThread isMainThread]) {
            return fail("AppKit smoke must run on the process main thread");
        }
        if (!hasWindowServerSession()) {
            std::cout << "MacWindow smoke skipped: no WindowServer session\n";
            return 77;
        }

        int screenWidth = 0;
        int screenHeight = 0;
        if (!ege::backend::MacWindow::primaryScreenSize(&screenWidth, &screenHeight)
            || screenWidth <= 0 || screenHeight <= 0) {
            return fail("primary screen size is unavailable");
        }

        RecordingSink sink;
        ege::backend::MacWindow window;
        const ege::WindowOptions defaultOptions;
        if (!window.create(96, 64, "EGE MacWindow smoke", defaultOptions, &sink)) {
            return fail("create returned false in an active GUI session");
        }
        if (window.isClosed() || window.getWidth() != 96 || window.getHeight() != 64) {
            return fail("initial logical content size is incorrect");
        }

        window.show();
        window.setTitle("EGE native AppKit smoke");
        window.setPosition(24, 24);
        window.setSize(80, 48);
        window.processEvents();
        if (window.getWidth() != 80 || window.getHeight() != 48) {
            return fail("resize was not reflected in logical EGE coordinates");
        }

        NSWindow* nativeWindow = (__bridge NSWindow*)window.getNativeHandle();
        if (nativeWindow == nil || ![nativeWindow.title isEqualToString:@"EGE native AppKit smoke"]) {
            return fail("native handle or UTF-8 title propagation is invalid");
        }
        if (NSApp.active || nativeWindow.keyWindow) {
            return fail("non-activating smoke window stole application focus");
        }

        // AppKit posts non-key events during ordinary window lifecycle work.
        // Processing one must not ask it for key-only character properties.
        NSEvent* applicationEvent = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                                       location:NSZeroPoint
                                                  modifierFlags:0
                                                      timestamp:0
                                                   windowNumber:nativeWindow.windowNumber
                                                        context:nil
                                                        subtype:0
                                                          data1:0
                                                          data2:0];
        [NSApp postEvent:applicationEvent atStart:YES];
        window.processEvents();

        const int keyEventsBefore = sink.keyEvents;
        const int textEventsBefore = sink.textEvents;
        const int mouseMoveEventsBefore = sink.mouseMoveEvents;
        const int mouseButtonEventsBefore = sink.mouseButtonEvents;
        const int mouseWheelEventsBefore = sink.mouseWheelEvents;
        NSView* contentView = nativeWindow.contentView;
        NSEvent* keyDown = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:nativeWindow.windowNumber
                                             context:nil
                                          characters:@"a"
                         charactersIgnoringModifiers:@"a"
                                           isARepeat:NO
                                             keyCode:0x00];
        NSEvent* keyUp = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                          location:NSZeroPoint
                                     modifierFlags:0
                                         timestamp:0
                                      windowNumber:nativeWindow.windowNumber
                                           context:nil
                                        characters:@"a"
                       charactersIgnoringModifiers:@"a"
                                         isARepeat:NO
                                           keyCode:0x00];
        [contentView keyDown:keyDown];
        [contentView keyUp:keyUp];

        NSEvent* mouseMove = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                                location:NSMakePoint(5, 5)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:nativeWindow.windowNumber
                                                 context:nil
                                             eventNumber:1
                                              clickCount:0
                                                pressure:0];
        NSEvent* mouseDown = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                                                location:NSMakePoint(5, 5)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:nativeWindow.windowNumber
                                                 context:nil
                                             eventNumber:2
                                              clickCount:1
                                                pressure:1];
        NSEvent* mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                              location:NSMakePoint(5, 5)
                                         modifierFlags:0
                                             timestamp:0
                                          windowNumber:nativeWindow.windowNumber
                                               context:nil
                                           eventNumber:3
                                            clickCount:1
                                              pressure:0];
        NSEvent* doubleClick = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                                                    location:NSMakePoint(6, 6)
                                               modifierFlags:0
                                                   timestamp:0
                                                windowNumber:nativeWindow.windowNumber
                                                     context:nil
                                                 eventNumber:4
                                                  clickCount:2
                                                    pressure:1];
        [contentView mouseMoved:mouseMove];
        [contentView mouseDown:mouseDown];
        [contentView mouseUp:mouseUp];
        [contentView mouseDown:doubleClick];

        CGEventRef xButtonCGEvent = CGEventCreateMouseEvent(
            nullptr, kCGEventOtherMouseDown, CGPointMake(7, 7), kCGMouseButtonCenter);
        NSEvent* xButtonEvent = nil;
        if (xButtonCGEvent != nullptr) {
            CGEventSetIntegerValueField(
                xButtonCGEvent, kCGMouseEventButtonNumber, 3);
            xButtonEvent = [NSEvent eventWithCGEvent:xButtonCGEvent];
            [contentView otherMouseDown:xButtonEvent];
            CFRelease(xButtonCGEvent);
        }

        CGEventRef scrollCGEvent = CGEventCreateScrollWheelEvent(
            nullptr, kCGScrollEventUnitLine, 1, 1);
        NSEvent* scrollEvent = scrollCGEvent != nullptr
            ? [NSEvent eventWithCGEvent:scrollCGEvent] : nil;
        if (scrollEvent != nil) {
            [contentView scrollWheel:scrollEvent];
        }
        if (scrollCGEvent != nullptr) {
            CFRelease(scrollCGEvent);
        }
        if (sink.keyEvents - keyEventsBefore != 2
            || sink.textEvents - textEventsBefore != 1
            || sink.mouseMoveEvents - mouseMoveEventsBefore != 1
            || sink.mouseButtonEvents - mouseButtonEventsBefore != 4
            || sink.mouseWheelEvents - mouseWheelEventsBefore != 1) {
            return fail("keyboard, text, mouse, or wheel event forwarding is incomplete");
        }
        if (sink.lastMouseButton != 3 || sink.lastClickCount != 1
            || sink.maxClickCount != 2) {
            return fail("double-click or extended mouse button forwarding is incomplete");
        }

        const auto sendModifier = ^(unsigned short keyCode,
                                    NSEventModifierFlags flags) {
            NSEvent* event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
                                              location:NSZeroPoint
                                         modifierFlags:flags
                                             timestamp:0
                                          windowNumber:nativeWindow.windowNumber
                                               context:nil
                                            characters:@""
                           charactersIgnoringModifiers:@""
                                             isARepeat:NO
                                               keyCode:keyCode];
            [contentView flagsChanged:event];
        };
        const std::size_t modifierStart = sink.recordedKeys.size();
        sendModifier(0x38, NSEventModifierFlagShift); // left down
        sendModifier(0x3C, NSEventModifierFlagShift); // right down
        sendModifier(0x38, NSEventModifierFlagShift); // left up
        sendModifier(0x3C, 0);                        // right up
        sendModifier(0x3B, NSEventModifierFlagControl); // left down
        sendModifier(0x3E, NSEventModifierFlagControl); // right down
        sendModifier(0x3B, NSEventModifierFlagControl); // left up
        sendModifier(0x3E, 0);                          // right up
        const std::vector<std::pair<std::uint32_t, bool>> expectedModifiers = {
            {0xA0, true}, {0xA1, true}, {0xA0, false}, {0xA1, false},
            {0xA2, true}, {0xA3, true}, {0xA2, false}, {0xA3, false},
        };
        if (sink.recordedKeys.size() - modifierStart != expectedModifiers.size()
            || !std::equal(expectedModifiers.begin(), expectedModifiers.end(),
                sink.recordedKeys.begin() + static_cast<std::ptrdiff_t>(modifierStart))) {
            return fail("left/right modifier state became stuck");
        }

        // Losing key status must release every held physical modifier through
        // the delegate, otherwise graphics.cpp can retain a generic modifier
        // after the user releases it in another application. The reset is
        // idempotent, and Caps Lock must not be treated as a held key.
        sendModifier(0x38, NSEventModifierFlagShift); // left Shift down
        sendModifier(0x3C, NSEventModifierFlagShift); // right Shift down
        sendModifier(0x3B, NSEventModifierFlagShift | NSEventModifierFlagControl);
        sendModifier(0x3E, NSEventModifierFlagShift | NSEventModifierFlagControl);
        sendModifier(0x3A, NSEventModifierFlagShift |
            NSEventModifierFlagControl | NSEventModifierFlagOption);
        sendModifier(0x3D, NSEventModifierFlagShift |
            NSEventModifierFlagControl | NSEventModifierFlagOption);
        sendModifier(0x37, NSEventModifierFlagShift |
            NSEventModifierFlagControl | NSEventModifierFlagOption |
            NSEventModifierFlagCommand);
        sendModifier(0x36, NSEventModifierFlagShift |
            NSEventModifierFlagControl | NSEventModifierFlagOption |
            NSEventModifierFlagCommand);
        const std::size_t focusResetStart = sink.recordedKeys.size();
        [nativeWindow.delegate windowDidResignKey:
            [NSNotification notificationWithName:NSWindowDidResignKeyNotification
                                          object:nativeWindow]];
        const std::vector<std::pair<std::uint32_t, bool>> expectedFocusReset = {
            {0xA0, false}, {0xA1, false}, {0xA2, false}, {0xA3, false},
            {0xA4, false}, {0xA5, false}, {0x5B, false}, {0x5C, false},
        };
        if (sink.recordedKeys.size() - focusResetStart != expectedFocusReset.size()
            || !std::equal(expectedFocusReset.begin(), expectedFocusReset.end(),
                sink.recordedKeys.begin() + static_cast<std::ptrdiff_t>(focusResetStart))) {
            return fail("window focus loss did not release held modifiers");
        }
        const std::size_t afterFocusReset = sink.recordedKeys.size();
        [nativeWindow.delegate windowDidResignKey:
            [NSNotification notificationWithName:NSWindowDidResignKeyNotification
                                          object:nativeWindow]];
        if (sink.recordedKeys.size() != afterFocusReset) {
            return fail("window focus loss emitted duplicate modifier releases");
        }

        const std::size_t capsStart = sink.recordedKeys.size();
        sendModifier(0x39, NSEventModifierFlagCapsLock);
        [nativeWindow.delegate windowDidResignKey:
            [NSNotification notificationWithName:NSWindowDidResignKeyNotification
                                          object:nativeWindow]];
        if (sink.recordedKeys.size() != capsStart + 1
            || sink.recordedKeys.back() != std::make_pair<std::uint32_t, bool>(0x14, true)) {
            return fail("Caps Lock focus-loss semantics changed unexpectedly");
        }
        sendModifier(0x39, 0);

        // Exercise padded input rows. MacWindow must copy only the visible
        // BGRA/PARGB pixels and must not retain this vector's data pointer.
        constexpr int pixelWidth  = 7;
        constexpr int pixelHeight = 5;
        constexpr std::size_t stridePixels = 9;
        std::vector<std::uint32_t> pixels(stridePixels * pixelHeight, 0xFF102030u);
        pixels[0] = 0xFFFF0000u;
        pixels[(pixelHeight - 1) * stridePixels + pixelWidth - 1] = 0xFF0000FFu;
        window.present(pixels.data(), pixelWidth, pixelHeight,
                       stridePixels * sizeof(std::uint32_t));
        std::fill(pixels.begin(), pixels.end(), 0u);
        pixels.clear();
        pixels.shrink_to_fit();
        window.processEvents();

        // A synthetic Command+Q takes the same delegate path as clicking the
        // close button. First reject it to model SetCloseHandler, then accept a
        // second request. Neither path terminates the test process.
        NSEvent* quitEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                             location:NSZeroPoint
                                        modifierFlags:NSEventModifierFlagCommand
                                            timestamp:0
                                         windowNumber:nativeWindow.windowNumber
                                              context:nil
                                           characters:@"q"
                          charactersIgnoringModifiers:@"q"
                                            isARepeat:NO
                                              keyCode:0x0C];
        sink.allowClose = false;
        [NSApp postEvent:quitEvent atStart:NO];
        window.processEvents();
        if (window.isClosed() || sink.closeRequests != 1) {
            return fail("rejected Command+Q closed the native window");
        }

        sink.allowClose = true;
        [NSApp postEvent:quitEvent atStart:NO];
        window.processEvents();
        if (!window.isClosed() || sink.closeRequests != 2) {
            return fail("accepted Command+Q did not close the native window");
        }

        window.close();

        ege::backend::MacWindow styledWindow;
        ege::WindowOptions styledOptions;
        styledOptions.borderless = true;
        styledOptions.topmost = true;
        if (!styledWindow.create(40, 30, "EGE styled window", styledOptions, &sink)) {
            return fail("borderless/topmost window creation failed");
        }
        NSWindow* styledNativeWindow = (__bridge NSWindow*)styledWindow.getNativeHandle();
        if (styledNativeWindow.styleMask != NSWindowStyleMaskBorderless
            || styledNativeWindow.level != NSFloatingWindowLevel) {
            return fail("INIT_NOBORDER/INIT_TOPMOST mapping is incorrect");
        }
        styledWindow.close();

        std::cout << "MacWindow smoke passed\n";
        return 0;
    }
}
