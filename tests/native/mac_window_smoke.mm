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
    void onCloseRequested() override { ++closeRequests; }
    void onResize(int width, int height) override
    {
        ++resizeEvents;
        lastWidth  = width;
        lastHeight = height;
    }
    void onKey(std::uint32_t, bool, bool) override { ++keyEvents; }
    void onText(std::uint32_t) override { ++textEvents; }
    void onMouseMove(int, int) override { ++mouseMoveEvents; }
    void onMouseButton(int, bool, int, int) override { ++mouseButtonEvents; }
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
        [contentView mouseMoved:mouseMove];
        [contentView mouseDown:mouseDown];
        [contentView mouseUp:mouseUp];

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
            || sink.mouseButtonEvents - mouseButtonEventsBefore != 2
            || sink.mouseWheelEvents - mouseWheelEventsBefore != 1) {
            return fail("keyboard, text, mouse, or wheel event forwarding is incomplete");
        }

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
        // close button. It closes only this EGE window and never terminates the
        // test process.
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
        [NSApp postEvent:quitEvent atStart:NO];
        window.processEvents();
        if (!window.isClosed() || sink.closeRequests != 1) {
            return fail("Command+Q did not produce exactly one non-terminating close request");
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
