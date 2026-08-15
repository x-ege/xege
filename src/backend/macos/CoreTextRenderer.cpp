#include "backend/macos/CoreTextRenderer.h"

#include <CoreText/CoreText.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <utility>

namespace ege
{
namespace backend
{
namespace
{

template <typename T> class ScopedCF
{
public:
    explicit ScopedCF(T value = nullptr) : value_(value) {}

    ~ScopedCF()
    {
        if (value_ != nullptr) {
            CFRelease(value_);
        }
    }

    ScopedCF(const ScopedCF&)            = delete;
    ScopedCF& operator=(const ScopedCF&) = delete;

    ScopedCF(ScopedCF&& other) noexcept : value_(other.value_) { other.value_ = nullptr; }

    ScopedCF& operator=(ScopedCF&& other) noexcept
    {
        if (this != &other) {
            if (value_ != nullptr) {
                CFRelease(value_);
            }
            value_       = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }

    T get() const { return value_; }

    explicit operator bool() const { return value_ != nullptr; }

private:
    T value_;
};

CFStringRef createString(const void* string, bool wide)
{
    if (string == nullptr) {
        return CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    }
    if (!wide) {
        const char* utf8   = static_cast<const char*>(string);
        CFStringRef result = CFStringCreateWithCString(kCFAllocatorDefault, utf8, kCFStringEncodingUTF8);
        return result != nullptr ? result : CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
    }

    const wchar_t* wideString = static_cast<const wchar_t*>(string);
    const CFIndex  byteCount  = static_cast<CFIndex>(std::wcslen(wideString) * sizeof(wchar_t));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const CFStringEncoding encoding = sizeof(wchar_t) == 4 ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF16LE;
#else
    const CFStringEncoding encoding = sizeof(wchar_t) == 4 ? kCFStringEncodingUTF32BE : kCFStringEncodingUTF16BE;
#endif
    CFStringRef result = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(wideString), byteCount, encoding, false);
    return result != nullptr ? result : CFStringCreateWithCString(kCFAllocatorDefault, "", kCFStringEncodingUTF8);
}

CTFontRef createFont(const std::string& face, int height, int weight, bool italic)
{
    const CGFloat size = static_cast<CGFloat>(std::max(1, std::abs(height)));
    ScopedCF<CFStringRef> fontName(CFStringCreateWithCString(kCFAllocatorDefault, face.c_str(), kCFStringEncodingUTF8));
    CTFontRef base = fontName ? CTFontCreateWithName(fontName.get(), size, nullptr) : nullptr;
    if (base == nullptr) {
        base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
        if (base == nullptr) {
            return nullptr;
        }
    }

    CTFontSymbolicTraits traits = 0;
    if (italic) {
        traits |= kCTFontItalicTrait;
    }
    if (weight >= 600) {
        traits |= kCTFontBoldTrait;
    }
    if (traits == 0) {
        return base;
    }

    CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, 0.0, nullptr, traits, traits);
    if (styled != nullptr) {
        CFRelease(base);
        return styled;
    }
    return base;
}

CGColorRef createColor(color_t color)
{
    const CGFloat alpha = static_cast<CGFloat>((color >> 24U) & 0xFFU) / 255.0;
    const CGFloat red   = static_cast<CGFloat>((color >> 16U) & 0xFFU) / 255.0;
    const CGFloat green = static_cast<CGFloat>((color >> 8U) & 0xFFU) / 255.0;
    const CGFloat blue  = static_cast<CGFloat>(color & 0xFFU) / 255.0;
    const CGFloat             components[] = {red, green, blue, alpha};
    ScopedCF<CGColorSpaceRef> colorSpace(CGColorSpaceCreateDeviceRGB());
    return colorSpace ? CGColorCreate(colorSpace.get(), components) : nullptr;
}

CFAttributedStringRef createAttributedString(CFStringRef string, CTFontRef font, CGColorRef color, bool underline)
{
    const void* keys[3]   = {kCTFontAttributeName, kCTForegroundColorAttributeName, kCTUnderlineStyleAttributeName};
    const void* values[3] = {font, color, nullptr};
    CFIndex     count     = 2;
    ScopedCF<CFNumberRef> underlineStyle;
    int                   underlineValue = kCTUnderlineStyleSingle;
    if (underline) {
        underlineStyle  = ScopedCF<CFNumberRef>(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &underlineValue));
        values[count++] = underlineStyle.get();
    }
    ScopedCF<CFDictionaryRef> attributes(CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, count, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    return attributes ? CFAttributedStringCreate(kCFAllocatorDefault, string, attributes.get()) : nullptr;
}

struct LineInfo
{
    CGFloat width;
    CGFloat ascent;
    CGFloat descent;
    CGFloat leading;
};

LineInfo lineInfo(CTLineRef line)
{
    LineInfo info = {0.0, 0.0, 0.0, 0.0};
    if (line != nullptr) {
        info.width = static_cast<CGFloat>(CTLineGetTypographicBounds(line, &info.ascent, &info.descent, &info.leading));
    }
    return info;
}

} // namespace

CoreTextRenderer::CoreTextRenderer() :
    height_(16),
    width_(0),
    face_("Helvetica"),
    escapement_(0),
    orientation_(0),
    weight_(400),
    italic_(false),
    underline_(false),
    strikeout_(false)
{}

void CoreTextRenderer::setFont(int height, int width, const char* face, int escapement, int orientation, int weight,
    bool italic, bool underline, bool strikeout)
{
    height_      = height != 0 ? height : 16;
    width_       = width;
    face_        = face != nullptr && face[0] != '\0' ? face : "Helvetica";
    escapement_  = escapement;
    orientation_ = orientation;
    weight_      = weight;
    italic_      = italic;
    underline_   = underline;
    strikeout_   = strikeout;
}

void CoreTextRenderer::getFont(int* height, int* width, char* face, int faceCapacity, int* escapement, int* orientation,
    int* weight, bool* italic, bool* underline, bool* strikeout) const
{
    if (height != nullptr) {
        *height = height_;
    }
    if (width != nullptr) {
        *width = width_;
    }
    if (face != nullptr && faceCapacity > 0) {
        std::strncpy(face, face_.c_str(), static_cast<std::size_t>(faceCapacity - 1));
        face[faceCapacity - 1] = '\0';
    }
    if (escapement != nullptr) {
        *escapement = escapement_;
    }
    if (orientation != nullptr) {
        *orientation = orientation_;
    }
    if (weight != nullptr) {
        *weight = weight_;
    }
    if (italic != nullptr) {
        *italic = italic_;
    }
    if (underline != nullptr) {
        *underline = underline_;
    }
    if (strikeout != nullptr) {
        *strikeout = strikeout_;
    }
}

void CoreTextRenderer::measure(const char* text, float* width, float* height) const
{
    measureString(text, false, width, height);
}

void CoreTextRenderer::measure(const wchar_t* text, float* width, float* height) const
{
    measureString(text, true, width, height);
}

void CoreTextRenderer::draw(CGContextRef context, float x, float y, const char* text, color_t color,
    TextHAlign horizontal, TextVAlign vertical) const
{
    drawString(context, x, y, text, false, color, horizontal, vertical);
}

void CoreTextRenderer::draw(CGContextRef context, float x, float y, const wchar_t* text, color_t color,
    TextHAlign horizontal, TextVAlign vertical) const
{
    drawString(context, x, y, text, true, color, horizontal, vertical);
}

void CoreTextRenderer::measureString(const void* string, bool wide, float* width, float* height) const
{
    if (width != nullptr) {
        *width = 0.0f;
    }
    if (height != nullptr) {
        *height = 0.0f;
    }

    ScopedCF<CFStringRef> text(createString(string, wide));
    ScopedCF<CTFontRef>   font(createFont(face_, height_, weight_, italic_));
    ScopedCF<CGColorRef>  color(createColor(0xFFFFFFFFU));
    if (!text || !font || !color) {
        return;
    }
    ScopedCF<CFAttributedStringRef> attributed(createAttributedString(text.get(), font.get(), color.get(), underline_));
    ScopedCF<CTLineRef>             line(attributed ? CTLineCreateWithAttributedString(attributed.get()) : nullptr);
    const LineInfo                  info = lineInfo(line.get());
    const CGFloat widthScale = width_ > 0 ? static_cast<CGFloat>(width_) / std::max<CGFloat>(1.0, std::abs(height_)) :
                                            1.0;
    if (width != nullptr) {
        *width = static_cast<float>(info.width * widthScale);
    }
    if (height != nullptr) {
        *height = static_cast<float>(info.ascent + info.descent + info.leading);
    }
}

void CoreTextRenderer::drawString(CGContextRef context, float x, float y, const void* string, bool wide, color_t color,
    TextHAlign horizontal, TextVAlign vertical) const
{
    if (context == nullptr || string == nullptr) {
        return;
    }
    ScopedCF<CFStringRef> text(createString(string, wide));
    ScopedCF<CTFontRef>   font(createFont(face_, height_, weight_, italic_));
    ScopedCF<CGColorRef>  foreground(createColor(color));
    if (!text || !font || !foreground) {
        return;
    }
    ScopedCF<CFAttributedStringRef> attributed(
        createAttributedString(text.get(), font.get(), foreground.get(), underline_));
    ScopedCF<CTLineRef> line(attributed ? CTLineCreateWithAttributedString(attributed.get()) : nullptr);
    if (!line) {
        return;
    }

    const LineInfo info       = lineInfo(line.get());
    const CGFloat  textHeight = info.ascent + info.descent + info.leading;
    const CGFloat  widthScale = width_ > 0 ? static_cast<CGFloat>(width_) / std::max<CGFloat>(1.0, std::abs(height_)) :
                                             1.0;
    CGFloat        drawX      = x;
    CGFloat        drawY      = y;
    if (horizontal == TEXT_CENTER) {
        drawX -= info.width * widthScale * 0.5;
    } else if (horizontal == TEXT_RIGHT) {
        drawX -= info.width * widthScale;
    }
    if (vertical == TEXT_MIDDLE) {
        drawY -= textHeight * 0.5;
    } else if (vertical == TEXT_BOTTOM) {
        drawY -= textHeight;
    }

    CGContextSaveGState(context);
    CGContextTranslateCTM(context, drawX, drawY + info.ascent);
    const CGFloat radians = -static_cast<CGFloat>(escapement_) * 3.14159265358979323846 / 1800.0;
    CGContextRotateCTM(context, radians);
    CGContextScaleCTM(context, widthScale, -1.0);
    CGContextSetTextMatrix(context, CGAffineTransformIdentity);
    CGContextSetTextPosition(context, 0.0, 0.0);
    CTLineDraw(line.get(), context);
    if (strikeout_ && info.width > 0.0) {
        CGContextSetFillColorWithColor(context, foreground.get());
        const CGFloat thickness = std::max<CGFloat>(1.0, textHeight / 14.0);
        CGContextFillRect(context, CGRectMake(0.0, info.ascent * 0.45, info.width, thickness));
    }
    CGContextRestoreGState(context);
}

} // namespace backend
} // namespace ege
