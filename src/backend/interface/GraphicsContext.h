#pragma once
#include <stdint.h>

namespace ege {

typedef uint32_t color_t;

class GraphicsContext {
public:
    virtual ~GraphicsContext() {}

    virtual void setLineColor(color_t color) = 0;
    virtual void setFillColor(color_t color) = 0;
    virtual void setLineWidth(float width) = 0;

    virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
    virtual void drawRect(int x, int y, int w, int h) = 0;
    virtual void fillRect(int x, int y, int w, int h) = 0;
    virtual void drawEllipse(int x, int y, int w, int h) = 0;
    virtual void fillEllipse(int x, int y, int w, int h) = 0;
    virtual void drawCircle(int x, int y, int r) = 0;
    virtual void fillCircle(int x, int y, int r) = 0;
    
    virtual void putPixel(int x, int y, color_t color) = 0;
    virtual color_t getPixel(int x, int y) = 0;

    virtual void clear(color_t color) = 0;
    virtual void flush() = 0;
    
    virtual void setViewport(int x, int y, int w, int h) = 0;
};

}
