#pragma once
#include "../interface/GraphicsContext.h"

namespace ege {

class OpenGLGraphicsContext : public GraphicsContext {
public:
    OpenGLGraphicsContext();
    ~OpenGLGraphicsContext();

    void setLineColor(color_t color) override;
    void setFillColor(color_t color) override;
    void setLineWidth(float width) override;

    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawRect(int x, int y, int w, int h) override;
    void fillRect(int x, int y, int w, int h) override;
    void drawEllipse(int x, int y, int w, int h) override;
    void fillEllipse(int x, int y, int w, int h) override;
    void drawCircle(int x, int y, int r) override;
    void fillCircle(int x, int y, int r) override;
    
    void putPixel(int x, int y, color_t color) override;
    color_t getPixel(int x, int y) override;

    void clear(color_t color) override;
    void flush() override;
    
    void setViewport(int x, int y, int w, int h) override;

private:
    color_t m_lineColor;
    color_t m_fillColor;
    float m_lineWidth;
};

}
