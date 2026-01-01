/*
 * EGE (Easy Graphics Engine)
 * FileName:    gdi_backend.h
 *
 * GDI rendering backend header - wraps existing Windows GDI/GDI+ functionality.
 * This backend maintains backward compatibility with the original EGE implementation.
 */

#ifndef EGE_GDI_BACKEND_H
#define EGE_GDI_BACKEND_H

#include "render_backend.h"

namespace ege
{

/**
 * @class GDIBackend
 * @brief GDI/GDI+ rendering backend for Windows
 *
 * This backend provides rendering using Windows GDI and GDI+.
 * It wraps the existing EGE implementation to maintain full backward compatibility.
 * This is the default backend on Windows.
 */
class GDIBackend : public RenderBackend
{
public:
    GDIBackend();
    virtual ~GDIBackend();

    // RenderBackend interface implementation
    virtual RenderBackendType getType() const override;
    virtual bool initialize(int width, int height, int mode) override;
    virtual void shutdown() override;
    virtual bool isInitialized() const override;
    virtual void swapBuffers() override;
    virtual void clear(color_t color) override;
    virtual void putPixel(int x, int y, color_t color) override;
    virtual color_t getPixel(int x, int y) override;
    virtual void drawLine(int x1, int y1, int x2, int y2, color_t color) override;
    virtual void drawRectangle(int left, int top, int right, int bottom, color_t color) override;
    virtual void fillRectangle(int left, int top, int right, int bottom, color_t color) override;
    virtual void drawCircle(int x, int y, int radius, color_t color) override;
    virtual void fillCircle(int x, int y, int radius, color_t color) override;
    virtual void drawEllipse(int x, int y, int xRadius, int yRadius, color_t color) override;
    virtual void fillEllipse(int x, int y, int xRadius, int yRadius, color_t color) override;
    virtual bool processEvents() override;
    virtual int getWidth() const override;
    virtual int getHeight() const override;

private:
    bool m_initialized;
    int m_width;
    int m_height;
};

} // namespace ege

#endif // EGE_GDI_BACKEND_H
