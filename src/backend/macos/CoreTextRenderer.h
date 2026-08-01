#ifndef EGE_BACKEND_MACOS_CORE_TEXT_RENDERER_H
#define EGE_BACKEND_MACOS_CORE_TEXT_RENDERER_H

#include "backend/interface/RenderTarget.h"

#include <CoreGraphics/CoreGraphics.h>

#include <string>

namespace ege
{
namespace backend
{

class CoreTextRenderer
{
public:
    CoreTextRenderer();

    void setFont(int height, int width, const char* face, int escapement, int orientation, int weight, bool italic,
        bool underline, bool strikeout);
    void getFont(int* height, int* width, char* face, int faceCapacity, int* escapement, int* orientation, int* weight,
        bool* italic, bool* underline, bool* strikeout) const;

    void measure(const char* text, float* width, float* height) const;
    void measure(const wchar_t* text, float* width, float* height) const;

    void draw(CGContextRef context, float x, float y, const char* text, color_t color, TextHAlign horizontal,
        TextVAlign vertical) const;
    void draw(CGContextRef context, float x, float y, const wchar_t* text, color_t color, TextHAlign horizontal,
        TextVAlign vertical) const;

private:
    void measureString(const void* string, bool wide, float* width, float* height) const;
    void drawString(CGContextRef context, float x, float y, const void* string, bool wide, color_t color,
        TextHAlign horizontal, TextVAlign vertical) const;

    int         height_;
    int         width_;
    std::string face_;
    int         escapement_;
    int         orientation_;
    int         weight_;
    bool        italic_;
    bool        underline_;
    bool        strikeout_;
};

} // namespace backend
} // namespace ege

#endif // EGE_BACKEND_MACOS_CORE_TEXT_RENDERER_H
