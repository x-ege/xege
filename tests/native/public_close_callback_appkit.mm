// Deliberately include EGE before AppKit. The inverse order is compiled by
// public_headers_appkit_first.mm; both are supported public-header contracts.
#include <graphics.h>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

extern "C" bool egeTestEGEFirstHeadersCompile()
{
    BOOL appKitValue = YES;
    return appKitValue && TRUE;
}

extern "C" bool egeTestHasWindowServerSession()
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == nullptr) {
        return false;
    }
    CFRelease(session);
    return true;
}

extern "C" void egeTestPerformNativeClose(void* nativeWindow)
{
    NSWindow* window = (__bridge NSWindow*)nativeWindow;
    [window performClose:nil];
}

extern "C" bool egeTestNativeWindowVisible(void* nativeWindow)
{
    NSWindow* window = (__bridge NSWindow*)nativeWindow;
    return window.visible;
}
