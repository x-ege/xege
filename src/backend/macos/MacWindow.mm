#include "backend/macos/MacWindow.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <dispatch/dispatch.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include <unistd.h>

namespace
{

struct MacWindowState
{
    std::atomic<bool> closed{true};
    std::atomic<bool> closeNotified{false};
    std::atomic<int>  width{0};
    std::atomic<int>  height{0};
    ege::WindowEventSink* eventSink{nullptr};
    bool suppressCloseCallback{false}; // Main-thread only.
    bool cursorHidden{false};          // Main-thread only; balances NSCursor hide/unhide.
};

void performOnMainThreadSync(dispatch_block_t block)
{
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

NSString* stringFromUTF8(const char* value)
{
    if (value == nullptr) {
        return @"";
    }

    NSString* string = [[NSString alloc] initWithBytes:value
                                                length:std::strlen(value)
                                              encoding:NSUTF8StringEncoding];
    return string != nil ? string : @"";
}

bool shouldAvoidApplicationActivation()
{
    const char* value = std::getenv("EGE_MACOS_TEST_NO_ACTIVATE");
    return value != nullptr && std::strcmp(value, "1") == 0;
}

void notifyTestWindowReady()
{
    const char* value = std::getenv("EGE_MACOS_TEST_READY_FD");
    if (value == nullptr) {
        return;
    }

    char* end = nullptr;
    const long descriptor = std::strtol(value, &end, 10);
    unsetenv("EGE_MACOS_TEST_READY_FD");
    if (end == value || *end != '\0' || descriptor < 0
        || descriptor > std::numeric_limits<int>::max()) {
        return;
    }

    const int fd = static_cast<int>(descriptor);
    const char ready = '1';
    (void)::write(fd, &ready, sizeof(ready));
    (void)::close(fd);
}

std::uint32_t windowsVirtualKeyForEvent(NSEvent* event)
{
    // Modifier keys are reported through flagsChanged and need hardware-key
    // disambiguation because they have no printable characters.
    switch (event.keyCode) {
    case 0x38: return 0xA0; // left Shift
    case 0x3C: return 0xA1; // right Shift
    case 0x3B: return 0xA2; // left Control
    case 0x3E: return 0xA3; // right Control
    case 0x3A: return 0xA4; // left Option / Alt
    case 0x3D: return 0xA5; // right Option / Alt
    case 0x37: return 0x5B; // left Command
    case 0x36: return 0x5C; // right Command
    case 0x39: return 0x14; // Caps Lock
    default: break;
    }

    // flagsChanged events are valid for modifier keys above, but AppKit does
    // not guarantee character access for other non-key event subtypes.
    if (event.type != NSEventTypeKeyDown && event.type != NSEventTypeKeyUp) {
        return 0;
    }

    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length == 0) {
        return 0;
    }

    const unichar character = [characters characterAtIndex:0];
    const bool numericPad = (event.modifierFlags & NSEventModifierFlagNumericPad) != 0;
    if (character >= 'a' && character <= 'z') {
        return static_cast<std::uint32_t>(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return character;
    }
    if (character >= '0' && character <= '9') {
        return numericPad ? static_cast<std::uint32_t>(0x60 + character - '0') : character;
    }

    switch (character) {
    case NSBackspaceCharacter:
    case NSDeleteCharacter:       return 0x08;
    case NSTabCharacter:          return 0x09;
    case NSCarriageReturnCharacter:
    case NSEnterCharacter:
    case NSNewlineCharacter:      return 0x0D;
    case 0x1B:                    return 0x1B;
    case ' ':                     return 0x20;
    case NSPageUpFunctionKey:     return 0x21;
    case NSPageDownFunctionKey:   return 0x22;
    case NSEndFunctionKey:        return 0x23;
    case NSHomeFunctionKey:       return 0x24;
    case NSLeftArrowFunctionKey:  return 0x25;
    case NSUpArrowFunctionKey:    return 0x26;
    case NSRightArrowFunctionKey: return 0x27;
    case NSDownArrowFunctionKey:  return 0x28;
    case NSPrintScreenFunctionKey:return 0x2C;
    case NSInsertFunctionKey:     return 0x2D;
    case NSDeleteFunctionKey:     return 0x2E;
    case NSF1FunctionKey:         return 0x70;
    case NSF2FunctionKey:         return 0x71;
    case NSF3FunctionKey:         return 0x72;
    case NSF4FunctionKey:         return 0x73;
    case NSF5FunctionKey:         return 0x74;
    case NSF6FunctionKey:         return 0x75;
    case NSF7FunctionKey:         return 0x76;
    case NSF8FunctionKey:         return 0x77;
    case NSF9FunctionKey:         return 0x78;
    case NSF10FunctionKey:        return 0x79;
    case NSF11FunctionKey:        return 0x7A;
    case NSF12FunctionKey:        return 0x7B;
    case ';':                     return 0xBA;
    case '=':                     return numericPad ? 0x6B : 0xBB;
    case ',':                     return 0xBC;
    case '-':                     return numericPad ? 0x6D : 0xBD;
    case '.':                     return numericPad ? 0x6E : 0xBE;
    case '/':                     return numericPad ? 0x6F : 0xBF;
    case '`':                     return 0xC0;
    case '[':                     return 0xDB;
    case '\\':                    return 0xDC;
    case ']':                     return 0xDD;
    case '\'':                    return 0xDE;
    case '*':                     return numericPad ? 0x6A : 0;
    case '+':                     return numericPad ? 0x6B : 0xBB;
    default:                      return 0;
    }
}

bool modifierPressedForEvent(NSEvent* event, bool currentState)
{
    NSEventModifierFlags aggregateFlag = 0;
    switch (event.keyCode) {
    case 0x38:
    case 0x3C: aggregateFlag = NSEventModifierFlagShift; break;
    case 0x3B:
    case 0x3E: aggregateFlag = NSEventModifierFlagControl; break;
    case 0x3A:
    case 0x3D: aggregateFlag = NSEventModifierFlagOption; break;
    case 0x37:
    case 0x36: aggregateFlag = NSEventModifierFlagCommand; break;
    case 0x39: return (event.modifierFlags & NSEventModifierFlagCapsLock) != 0;
    default:   return false;
    }
    // AppKit's device-independent flags merge the left and right keys. A
    // flagsChanged event identifies the physical key that changed, so toggle
    // that side while the aggregate remains set. When the final side is
    // released the aggregate clears and both ordinary single-key and dual-key
    // sequences resolve to false without leaving a stuck modifier.
    return (event.modifierFlags & aggregateFlag) != 0
        ? !currentState : false;
}

void emitText(NSString* text, MacWindowState* state)
{
    if (state == nullptr || state->eventSink == nullptr) {
        return;
    }

    for (NSUInteger index = 0; index < text.length; ++index) {
        const unichar first = [text characterAtIndex:index];
        std::uint32_t codepoint = first;
        if (first >= 0xD800 && first <= 0xDBFF && index + 1 < text.length) {
            const unichar second = [text characterAtIndex:index + 1];
            if (second >= 0xDC00 && second <= 0xDFFF) {
                codepoint = 0x10000u + ((static_cast<std::uint32_t>(first) - 0xD800u) << 10u)
                    + (static_cast<std::uint32_t>(second) - 0xDC00u);
                ++index;
            }
        }
        if (codepoint < 0xD800u || codepoint > 0xDFFFu) {
            state->eventSink->onText(codepoint);
        }
    }
}

} // namespace

@interface EGEMacContentView : NSView <NSTextInputClient>
{
@private
    CGImageRef _presentedImage;
    NSInteger _pixelWidth;
    NSInteger _pixelHeight;
    NSMutableAttributedString* _markedText;
    MacWindowState* _state;
    bool _modifierKeyStates[256];
}
- (instancetype)initWithFrame:(NSRect)frame state:(MacWindowState*)state;
- (void)setPresentedImage:(CGImageRef)image pixelWidth:(NSInteger)width pixelHeight:(NSInteger)height;
- (void)resetModifierKeys;
- (void)detachState;
@end

@implementation EGEMacContentView

- (instancetype)initWithFrame:(NSRect)frame state:(MacWindowState*)state
{
    self = [super initWithFrame:frame];
    if (self != nil) {
        _presentedImage = nullptr;
        _pixelWidth     = 0;
        _pixelHeight    = 0;
        _markedText     = [[NSMutableAttributedString alloc] init];
        _state          = state;
        std::fill_n(_modifierKeyStates, 256, false);
    }
    return self;
}

- (void)dealloc
{
    if (_presentedImage != nullptr) {
        CGImageRelease(_presentedImage);
    }
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (void)detachState
{
    _state = nullptr;
}

- (void)setPresentedImage:(CGImageRef)image pixelWidth:(NSInteger)width pixelHeight:(NSInteger)height
{
    if (image != _presentedImage) {
        if (image != nullptr) {
            CGImageRetain(image);
        }
        if (_presentedImage != nullptr) {
            CGImageRelease(_presentedImage);
        }
        _presentedImage = image;
    }
    _pixelWidth  = width;
    _pixelHeight = height;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    if (_presentedImage == nullptr) {
        [[NSColor blackColor] setFill];
        NSRectFill(dirtyRect);
        return;
    }

    NSImage* image = [[NSImage alloc] initWithCGImage:_presentedImage
                                                size:NSMakeSize(_pixelWidth, _pixelHeight)];
    [image drawInRect:self.bounds
             fromRect:NSZeroRect
            operation:NSCompositingOperationCopy
             fraction:1.0
       respectFlipped:YES
                hints:@{NSImageHintInterpolation: @(NSImageInterpolationNone)}];
}

- (NSPoint)pixelPointForEvent:(NSEvent*)event
{
    const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    const NSRect bounds = self.bounds;
    const CGFloat scaleX = bounds.size.width > 0.0 && _pixelWidth > 0
        ? static_cast<CGFloat>(_pixelWidth) / bounds.size.width : 1.0;
    const CGFloat scaleY = bounds.size.height > 0.0 && _pixelHeight > 0
        ? static_cast<CGFloat>(_pixelHeight) / bounds.size.height : 1.0;

    // NSView operates in logical points on Retina screens. Mapping through
    // the current surface dimensions preserves EGE pixel coordinates for both
    // one-point-per-pixel and high-resolution presentation buffers.
    return NSMakePoint(std::floor(point.x * scaleX), std::floor(point.y * scaleY));
}

- (void)sendMouseMove:(NSEvent*)event
{
    if (_state == nullptr || _state->eventSink == nullptr) {
        return;
    }
    const NSPoint point = [self pixelPointForEvent:event];
    _state->eventSink->onMouseMove(static_cast<int>(point.x), static_cast<int>(point.y));
}

- (void)mouseMoved:(NSEvent*)event       { [self sendMouseMove:event]; }
- (void)mouseDragged:(NSEvent*)event     { [self sendMouseMove:event]; }
- (void)rightMouseDragged:(NSEvent*)event{ [self sendMouseMove:event]; }
- (void)otherMouseDragged:(NSEvent*)event{ [self sendMouseMove:event]; }

- (void)sendMouseButton:(NSEvent*)event pressed:(bool)pressed
{
    if (_state == nullptr || _state->eventSink == nullptr || event.buttonNumber > 4) {
        return;
    }
    const NSPoint point = [self pixelPointForEvent:event];
    _state->eventSink->onMouseButton(
        static_cast<int>(event.buttonNumber), pressed,
        static_cast<int>(point.x), static_cast<int>(point.y),
        static_cast<int>(std::max<NSInteger>(1, event.clickCount)));
}

- (void)mouseDown:(NSEvent*)event       { [self sendMouseButton:event pressed:true]; }
- (void)mouseUp:(NSEvent*)event         { [self sendMouseButton:event pressed:false]; }
- (void)rightMouseDown:(NSEvent*)event  { [self sendMouseButton:event pressed:true]; }
- (void)rightMouseUp:(NSEvent*)event    { [self sendMouseButton:event pressed:false]; }
- (void)otherMouseDown:(NSEvent*)event  { [self sendMouseButton:event pressed:true]; }
- (void)otherMouseUp:(NSEvent*)event    { [self sendMouseButton:event pressed:false]; }

- (void)scrollWheel:(NSEvent*)event
{
    if (_state == nullptr || _state->eventSink == nullptr) {
        return;
    }
    const NSPoint point = [self pixelPointForEvent:event];
    const CGFloat divisor = event.hasPreciseScrollingDeltas ? 10.0 : 1.0;
    _state->eventSink->onMouseWheel(
        static_cast<float>(event.scrollingDeltaX / divisor),
        static_cast<float>(event.scrollingDeltaY / divisor),
        static_cast<int>(point.x), static_cast<int>(point.y));
}

- (void)keyDown:(NSEvent*)event
{
    if (_state == nullptr) {
        return;
    }
    const std::uint32_t key = windowsVirtualKeyForEvent(event);
    if (key != 0 && _state->eventSink != nullptr) {
        _state->eventSink->onKey(key, true, event.isARepeat);
    }

    const NSEventModifierFlags shortcutModifiers =
        event.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl);
    if (shortcutModifiers == 0) {
        [self interpretKeyEvents:@[event]];
    }
}

- (void)keyUp:(NSEvent*)event
{
    if (_state == nullptr || _state->eventSink == nullptr) {
        return;
    }
    const std::uint32_t key = windowsVirtualKeyForEvent(event);
    if (key != 0) {
        _state->eventSink->onKey(key, false, false);
    }
}

- (void)flagsChanged:(NSEvent*)event
{
    if (_state == nullptr || _state->eventSink == nullptr) {
        return;
    }
    const std::uint32_t key = windowsVirtualKeyForEvent(event);
    if (key != 0) {
        const NSUInteger keyCode = event.keyCode;
        const bool currentState = keyCode < 256
            ? _modifierKeyStates[keyCode] : false;
        const bool pressed = modifierPressedForEvent(event, currentState);
        if (keyCode < 256) {
            _modifierKeyStates[keyCode] = pressed;
        }
        _state->eventSink->onKey(key, pressed, false);
    }
}

- (void)resetModifierKeys
{
    // Caps Lock is a toggle, not a held modifier. Do not synthesize a release
    // for it when the window loses focus; AppKit will report its actual toggle
    // state in a later flagsChanged event. The other modifier keys are physical
    // keys, and their release events must reach the sink so its generic Shift /
    // Control / Alt state cannot remain stuck after focus moves elsewhere.
    static const struct PhysicalModifier {
        unsigned short keyCode;
        std::uint32_t virtualKey;
    } modifiers[] = {
        {0x38, 0xA0}, // left Shift
        {0x3C, 0xA1}, // right Shift
        {0x3B, 0xA2}, // left Control
        {0x3E, 0xA3}, // right Control
        {0x3A, 0xA4}, // left Option / Alt
        {0x3D, 0xA5}, // right Option / Alt
        {0x37, 0x5B}, // left Command
        {0x36, 0x5C}, // right Command
    };

    for (const PhysicalModifier& modifier : modifiers) {
        if (!_modifierKeyStates[modifier.keyCode]) {
            continue;
        }
        // Clear before calling user code, making this operation idempotent even
        // if the callback causes another focus transition synchronously.
        _modifierKeyStates[modifier.keyCode] = false;
        if (_state != nullptr && _state->eventSink != nullptr) {
            _state->eventSink->onKey(modifier.virtualKey, false, false);
        }
    }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    (void)replacementRange;
    NSString* plainText = [string isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString*)string string] : (NSString*)string;
    if (plainText != nil) {
        emitText(plainText, _state);
    }
    [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    (void)selectedRange;
    (void)replacementRange;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        [_markedText setAttributedString:(NSAttributedString*)string];
    } else if ([string isKindOfClass:[NSString class]]) {
        [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:(NSString*)string]];
    }
}

- (void)unmarkText
{
    [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
}

- (BOOL)hasMarkedText                { return _markedText.length != 0; }
- (NSRange)markedRange               { return _markedText.length ? NSMakeRange(0, _markedText.length) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange             { return NSMakeRange(NSNotFound, 0); }
- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText { return @[]; }

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    (void)range;
    if (actualRange != nullptr) {
        *actualRange = NSMakeRange(NSNotFound, 0);
    }
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    (void)point;
    return NSNotFound;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    if (actualRange != nullptr) {
        *actualRange = range;
    }
    const NSRect caret = NSMakeRect(0, 0, 1, std::max<CGFloat>(1.0, self.bounds.size.height));
    const NSRect windowRect = [self convertRect:caret toView:nil];
    return self.window != nil ? [self.window convertRectToScreen:windowRect] : NSZeroRect;
}

- (void)doCommandBySelector:(SEL)selector
{
    (void)selector;
}

@end

@interface EGEMacWindowDelegate : NSObject <NSWindowDelegate>
{
@private
    MacWindowState* _state;
}
- (instancetype)initWithState:(MacWindowState*)state;
- (void)detachState;
@end

@implementation EGEMacWindowDelegate

- (instancetype)initWithState:(MacWindowState*)state
{
    self = [super init];
    if (self != nil) {
        _state = state;
    }
    return self;
}

- (void)detachState
{
    _state = nullptr;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    if (_state == nullptr) {
        return YES;
    }

    bool shouldClose = true;
    const bool firstNotification = !_state->closeNotified.exchange(true);
    if (!_state->suppressCloseCallback && firstNotification && _state->eventSink != nullptr) {
        shouldClose = _state->eventSink->onCloseRequested();
    }
    if (!shouldClose) {
        // A rejected request must be observable again on the next title-bar
        // click or Command+Q, just like a subsequent Win32 WM_CLOSE message.
        _state->closeNotified.store(false);
        return NO;
    }

    _state->closed.store(true);
    return YES;
}

- (void)windowWillClose:(NSNotification*)notification
{
    (void)notification;
    if (_state != nullptr) {
        _state->closed.store(true);
    }
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    NSWindow* window = notification.object;
    if ([window.contentView respondsToSelector:@selector(resetModifierKeys)]) {
        [(EGEMacContentView*)window.contentView resetModifierKeys];
    }
}

- (void)windowDidResize:(NSNotification*)notification
{
    if (_state == nullptr || _state->closed.load()) {
        return;
    }
    NSWindow* window = notification.object;
    const NSSize logicalSize = window.contentView.bounds.size;
    const int width  = std::max(1, static_cast<int>(std::lround(logicalSize.width)));
    const int height = std::max(1, static_cast<int>(std::lround(logicalSize.height)));
    const int previousWidth  = _state->width.exchange(width);
    const int previousHeight = _state->height.exchange(height);
    if ((width != previousWidth || height != previousHeight) && _state->eventSink != nullptr) {
        _state->eventSink->onResize(width, height);
    }
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    NSWindow* window = notification.object;
    [window.contentView setNeedsDisplay:YES];
}

@end

namespace ege
{
namespace backend
{

struct MacWindow::Impl
{
    std::unique_ptr<MacWindowState> state = std::make_unique<MacWindowState>();
    __strong NSWindow* window = nil;
    __strong EGEMacContentView* contentView = nil;
    __strong EGEMacWindowDelegate* delegate = nil;
};

MacWindow::MacWindow() : impl_(std::make_unique<Impl>()) {}

MacWindow::~MacWindow()
{
    close();
}

bool MacWindow::primaryScreenSize(int* width, int* height)
{
    if (width == nullptr || height == nullptr) {
        return false;
    }
    __block int screenWidth = 0;
    __block int screenHeight = 0;
    performOnMainThreadSync(^{
        @autoreleasepool {
            NSScreen* primaryScreen = NSScreen.screens.firstObject;
            if (primaryScreen != nil) {
                const NSRect frame = primaryScreen.frame;
                screenWidth = static_cast<int>(std::lround(NSWidth(frame)));
                screenHeight = static_cast<int>(std::lround(NSHeight(frame)));
            }
        }
    });
    if (screenWidth <= 0 || screenHeight <= 0) {
        return false;
    }
    *width = screenWidth;
    *height = screenHeight;
    return true;
}

bool MacWindow::inputBox(const char* title, const char* prompt,
                         std::string* value)
{
    if (value == nullptr) {
        return false;
    }

    __block bool accepted = false;
    __block std::string result;
    performOnMainThreadSync(^{
        @autoreleasepool {
            if ([NSScreen screens].count == 0) {
                return;
            }
            [NSApplication sharedApplication];
            NSAlert* alert = [[NSAlert alloc] init];
            alert.messageText = stringFromUTF8(title);
            alert.informativeText = stringFromUTF8(prompt);
            [alert addButtonWithTitle:@"OK"];
            [alert addButtonWithTitle:@"Cancel"];

            NSTextField* field = [[NSTextField alloc]
                initWithFrame:NSMakeRect(0, 0, 360, 24)];
            alert.accessoryView = field;
            if (!shouldAvoidApplicationActivation()) {
                [NSApp activateIgnoringOtherApps:YES];
            }
            const NSModalResponse response = [alert runModal];
            if (response == NSAlertFirstButtonReturn) {
                const char* utf8 = field.stringValue.UTF8String;
                result = utf8 != nullptr ? utf8 : "";
                accepted = true;
            }
        }
    });
    if (accepted) {
        *value = result;
    }
    return accepted;
}

bool MacWindow::create(int width, int height, const char* title,
    const WindowOptions& options, WindowEventSink* eventSink)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    close();
    __block bool created = false;
    performOnMainThreadSync(^{
        @autoreleasepool {
            static dispatch_once_t applicationOnce;
            dispatch_once(&applicationOnce, ^{
                [NSApplication sharedApplication];
                if (shouldAvoidApplicationActivation()) {
                    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
                } else if (NSApp.activationPolicy == NSApplicationActivationPolicyProhibited) {
                    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
                }
                if (!NSApp.running) {
                    [NSApp finishLaunching];
                }
            });

            MacWindowState* state = impl_->state.get();
            state->eventSink = eventSink;
            state->closed.store(false);
            state->closeNotified.store(false);
            state->width.store(width);
            state->height.store(height);
            state->suppressCloseCallback = false;

            const NSRect contentRect = NSMakeRect(0, 0, width, height);
            const NSWindowStyleMask style = options.borderless
                ? NSWindowStyleMaskBorderless
                : NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                    | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
            impl_->window = [[NSWindow alloc] initWithContentRect:contentRect
                                                        styleMask:style
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
            if (impl_->window == nil) {
                state->closed.store(true);
                state->eventSink = nullptr;
                return;
            }

            impl_->contentView = [[EGEMacContentView alloc] initWithFrame:contentRect state:state];
            impl_->delegate = [[EGEMacWindowDelegate alloc] initWithState:state];
            impl_->window.releasedWhenClosed = NO;
            impl_->window.delegate = impl_->delegate;
            impl_->window.contentView = impl_->contentView;
            impl_->window.title = stringFromUTF8(title);
            impl_->window.acceptsMouseMovedEvents = YES;
            impl_->window.level = options.topmost ? NSFloatingWindowLevel : NSNormalWindowLevel;
            [impl_->window center];
            [impl_->window makeFirstResponder:impl_->contentView];
            created = true;
        }
    });
    return created;
}

void MacWindow::show()
{
    performOnMainThreadSync(^{
        @autoreleasepool {
            if (impl_->window != nil && !impl_->state->closed.load()) {
                if (shouldAvoidApplicationActivation()) {
                    // Native GUI smoke tests still need a server-side window,
                    // but must not make their process active or steal focus.
                    // This API orders a window for an inactive application
                    // without making it key or changing application focus.
                    [impl_->window orderFrontRegardless];
                    notifyTestWindowReady();
                } else {
                    [impl_->window makeKeyAndOrderFront:nil];
                }
            }
        }
    });
}

void MacWindow::hide()
{
    performOnMainThreadSync(^{
        @autoreleasepool {
            [impl_->window orderOut:nil];
        }
    });
}

void MacWindow::setTitle(const char* title)
{
    const std::string copiedTitle = title != nullptr ? title : "";
    performOnMainThreadSync(^{
        @autoreleasepool {
            if (impl_->window != nil) {
                impl_->window.title = stringFromUTF8(copiedTitle.c_str());
            }
        }
    });
}

void MacWindow::setSize(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    performOnMainThreadSync(^{
        @autoreleasepool {
            if (impl_->window != nil && !impl_->state->closed.load()) {
                [impl_->window setContentSize:NSMakeSize(width, height)];
            }
        }
    });
}

void MacWindow::setPosition(int x, int y)
{
    performOnMainThreadSync(^{
        @autoreleasepool {
            if (impl_->window == nil || impl_->state->closed.load()) {
                return;
            }
            NSScreen* primaryScreen = NSScreen.screens.firstObject;
            if (primaryScreen == nil) {
                return;
            }
            const NSRect screenFrame = primaryScreen.frame;
            const NSPoint topLeft = NSMakePoint(
                NSMinX(screenFrame) + x, NSMaxY(screenFrame) - y);
            [impl_->window setFrameTopLeftPoint:topLeft];
        }
    });
}

void MacWindow::setCursorVisible(bool visible)
{
    performOnMainThreadSync(^{
        @autoreleasepool {
            if (!visible && !impl_->state->cursorHidden) {
                [NSCursor hide];
                impl_->state->cursorHidden = true;
            } else if (visible && impl_->state->cursorHidden) {
                [NSCursor unhide];
                impl_->state->cursorHidden = false;
            }
        }
    });
}

void MacWindow::close()
{
    if (impl_ == nullptr) {
        return;
    }
    performOnMainThreadSync(^{
        @autoreleasepool {
            MacWindowState* state = impl_->state.get();
            state->suppressCloseCallback = true;
            state->closed.store(true);
            state->eventSink = nullptr;
            if (state->cursorHidden) {
                [NSCursor unhide];
                state->cursorHidden = false;
            }
            [impl_->contentView detachState];
            [impl_->delegate detachState];
            impl_->window.delegate = nil;
            [impl_->window orderOut:nil];
            [impl_->window close];
            impl_->contentView = nil;
            impl_->delegate = nil;
            impl_->window = nil;
        }
    });
}

bool MacWindow::isClosed() const
{
    return impl_ == nullptr || impl_->state->closed.load();
}

void MacWindow::processEvents()
{
    performOnMainThreadSync(^{
        @autoreleasepool {
            NSEvent* event = nil;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:NSDate.distantPast
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES]) != nil) {
                bool commandQuit = false;
                if (event.type == NSEventTypeKeyDown
                    && (event.modifierFlags & NSEventModifierFlagCommand) != 0) {
                    NSString* characters = event.charactersIgnoringModifiers.lowercaseString;
                    commandQuit = [characters isEqualToString:@"q"];
                }
                if (commandQuit && impl_->window != nil
                    && (event.window == nil || event.window == impl_->window)) {
                    // Convert Command+Q into the same close request as the
                    // title-bar button. Never call terminate: or exit().
                    [impl_->window performClose:nil];
                    continue;
                }
                [NSApp sendEvent:event];
            }
            [NSApp updateWindows];
        }
    });
}

void MacWindow::present(
    const std::uint32_t* pixels, int width, int height, std::size_t strideBytes)
{
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }

    const std::size_t unsignedWidth  = static_cast<std::size_t>(width);
    const std::size_t unsignedHeight = static_cast<std::size_t>(height);
    if (unsignedWidth > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t)) {
        return;
    }
    const std::size_t packedStride = unsignedWidth * sizeof(std::uint32_t);
    if (strideBytes < packedStride
        || unsignedHeight > std::numeric_limits<std::size_t>::max() / packedStride) {
        return;
    }
    const std::size_t byteCount = packedStride * unsignedHeight;
    if (byteCount > static_cast<std::size_t>(std::numeric_limits<CFIndex>::max())) {
        return;
    }

    // CFMutableData owns the staging allocation. Every source row is copied
    // before this method returns, and the CGImage retains the CFData rather
    // than the caller's pointer.
    CFMutableDataRef ownedPixels = CFDataCreateMutable(kCFAllocatorDefault, static_cast<CFIndex>(byteCount));
    if (ownedPixels == nullptr) {
        return;
    }
    CFDataSetLength(ownedPixels, static_cast<CFIndex>(byteCount));
    UInt8* destination = CFDataGetMutableBytePtr(ownedPixels);
    const UInt8* source = reinterpret_cast<const UInt8*>(pixels);
    for (std::size_t row = 0; row < unsignedHeight; ++row) {
        std::memcpy(destination + row * packedStride, source + row * strideBytes, packedStride);
    }

    performOnMainThreadSync(^{
        @autoreleasepool {
            if (impl_->contentView == nil || impl_->state->closed.load()) {
                return;
            }
            CGDataProviderRef provider = CGDataProviderCreateWithCFData(ownedPixels);
            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            if (provider == nullptr || colorSpace == nullptr) {
                if (provider != nullptr) CGDataProviderRelease(provider);
                if (colorSpace != nullptr) CGColorSpaceRelease(colorSpace);
                return;
            }

            const CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little
                | kCGImageAlphaPremultipliedFirst;
            CGImageRef image = CGImageCreate(unsignedWidth, unsignedHeight, 8, 32,
                packedStride, colorSpace, bitmapInfo, provider, nullptr, false,
                kCGRenderingIntentDefault);
            if (image != nullptr) {
                [impl_->contentView setPresentedImage:image pixelWidth:width pixelHeight:height];
                CGImageRelease(image);
            }
            CGColorSpaceRelease(colorSpace);
            CGDataProviderRelease(provider);
        }
    });
    CFRelease(ownedPixels);
}

void* MacWindow::getNativeHandle() const
{
    if (impl_ == nullptr || impl_->window == nil) {
        return nullptr;
    }
    return (__bridge void*)impl_->window;
}

int MacWindow::getWidth() const
{
    return impl_ != nullptr ? impl_->state->width.load() : 0;
}

int MacWindow::getHeight() const
{
    return impl_ != nullptr ? impl_->state->height.load() : 0;
}

} // namespace backend
} // namespace ege
