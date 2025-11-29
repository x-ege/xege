/*
 * EGE (Easy Graphics Engine)
 * GDI Renderer - Windows GDI/GDI+ backend wrapper
 * 
 * This is a wrapper around the existing GDI-based rendering code.
 * It maintains backward compatibility while fitting into the new
 * renderer abstraction framework.
 */

#pragma once

#include "renderer_interface.h"

namespace ege {
namespace renderer {

/**
 * @class GDIRenderer
 * @brief GDI/GDI+ rendering backend (Windows only)
 * 
 * This renderer wraps the existing GDI-based implementation,
 * maintaining full backward compatibility with existing code.
 */
class GDIRenderer : public IRenderer {
public:
    GDIRenderer();
    virtual ~GDIRenderer();

    // IRenderer interface implementation
    virtual RendererType getType() const override { return RENDERER_GDI; }
    virtual bool initialize(int width, int height) override;
    virtual void shutdown() override;
    virtual void beginFrame() override;
    virtual void endFrame() override;
    virtual void clear(uint32_t color) override;
    virtual void setPixel(int x, int y, uint32_t color) override;
    virtual uint32_t getPixel(int x, int y) const override;
    virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color) override;
    virtual void drawRectangle(int x, int y, int w, int h, uint32_t color) override;
    virtual void fillRectangle(int x, int y, int w, int h, uint32_t color) override;
    virtual void drawCircle(int x, int y, int radius, uint32_t color) override;
    virtual void fillCircle(int x, int y, int radius, uint32_t color) override;
    virtual void drawEllipse(int x, int y, int xRadius, int yRadius, uint32_t color) override;
    virtual void fillEllipse(int x, int y, int xRadius, int yRadius, uint32_t color) override;
    virtual void drawArc(int x, int y, int startAngle, int endAngle, int radius, uint32_t color) override;
    virtual void drawPolyline(int numPoints, const int* points, uint32_t color) override;
    virtual void drawPolygon(int numPoints, const int* points, uint32_t color) override;
    virtual void fillPolygon(int numPoints, const int* points, uint32_t color) override;
    virtual void resize(int width, int height) override;
    virtual uint32_t* getPixelBuffer() override;
    virtual void syncPixelBuffer() override;

private:
    int m_width;
    int m_height;
    // Note: Actual GDI resources are managed by existing IMAGE class
    // This is just a thin wrapper for now
};

} // namespace renderer
} // namespace ege
