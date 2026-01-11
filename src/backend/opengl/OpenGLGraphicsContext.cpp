#include "OpenGLGraphicsContext.h"
#include <GLFW/glfw3.h>
#include <math.h>

namespace ege {

OpenGLGraphicsContext::OpenGLGraphicsContext() : m_lineColor(0xFFFFFFFF), m_fillColor(0xFFFFFFFF), m_lineWidth(1.0f) {
}

OpenGLGraphicsContext::~OpenGLGraphicsContext() {
}

void OpenGLGraphicsContext::setLineColor(color_t color) {
    m_lineColor = color;
}

void OpenGLGraphicsContext::setFillColor(color_t color) {
    m_fillColor = color;
}

void OpenGLGraphicsContext::setLineWidth(float width) {
    m_lineWidth = width;
    glLineWidth(width);
}

static void setGLColor(color_t color) {
    unsigned char r = (color >> 16) & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = color & 0xFF;
    unsigned char a = (color >> 24) & 0xFF;
    glColor4ub(r, g, b, a);
}

void OpenGLGraphicsContext::drawLine(int x1, int y1, int x2, int y2) {
    setGLColor(m_lineColor);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
}

void OpenGLGraphicsContext::drawRect(int x, int y, int w, int h) {
    setGLColor(m_lineColor);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

void OpenGLGraphicsContext::fillRect(int x, int y, int w, int h) {
    setGLColor(m_fillColor);
    glRecti(x, y, x + w, y + h);
}

void OpenGLGraphicsContext::drawEllipse(int x, int y, int w, int h) {
    setGLColor(m_lineColor);
    glBegin(GL_LINE_LOOP);
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    for (int i = 0; i < 360; i++) {
        float theta = i * 3.14159f / 180.0f;
        glVertex2f(cx + rx * cos(theta), cy + ry * sin(theta));
    }
    glEnd();
}

void OpenGLGraphicsContext::fillEllipse(int x, int y, int w, int h) {
    setGLColor(m_fillColor);
    glBegin(GL_POLYGON);
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;
    float rx = w / 2.0f;
    float ry = h / 2.0f;
    for (int i = 0; i < 360; i++) {
        float theta = i * 3.14159f / 180.0f;
        glVertex2f(cx + rx * cos(theta), cy + ry * sin(theta));
    }
    glEnd();
}

void OpenGLGraphicsContext::drawCircle(int x, int y, int r) {
    drawEllipse(x - r, y - r, 2 * r, 2 * r);
}

void OpenGLGraphicsContext::fillCircle(int x, int y, int r) {
    fillEllipse(x - r, y - r, 2 * r, 2 * r);
}

void OpenGLGraphicsContext::putPixel(int x, int y, color_t color) {
    setGLColor(color);
    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();
}

color_t OpenGLGraphicsContext::getPixel(int x, int y) {
    unsigned char pixel[4];
    // OpenGL reads from bottom-left, but we use top-left origin.
    // We need window height to flip Y.
    // But we don't have window height here easily.
    // Assuming current viewport is correct.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glReadPixels(x, viewport[3] - y - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    return (pixel[3] << 24) | (pixel[0] << 16) | (pixel[1] << 8) | pixel[2];
}

void OpenGLGraphicsContext::clear(color_t color) {
    unsigned char r = (color >> 16) & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = color & 0xFF;
    unsigned char a = (color >> 24) & 0xFF;
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLGraphicsContext::flush() {
    glFlush();
}

void OpenGLGraphicsContext::setViewport(int x, int y, int w, int h) {
    // In OpenGL, viewport is usually (0, 0, width, height) of the window.
    // EGE viewport is a logical viewport.
    // We can use glScissor for clipping.
    // And we need to adjust projection matrix for coordinate system.
    // For now, let's just set glViewport, but we need to flip Y.
    // We need window height to flip Y.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int windowHeight = viewport[3]; // Assuming current viewport covers full window
    
    // glViewport(x, windowHeight - y - h, w, h);
    // But this changes the mapping from NDC to window coordinates.
    // EGE expects (0,0) at top-left of viewport.
    
    // We should probably use glScissor for clipping and projection matrix for translation.
    // But for now, let's leave it empty and rely on EGE's logical coordinates if possible.
    // But EGE's logical coordinates are handled by GDI's SetViewportOrgEx.
    // We need to replicate that.
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1); // Top-left origin
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Translate to viewport origin?
    // If we use glOrtho(0, w, h, 0), then (0,0) is top-left of viewport.
    // But we need to position the viewport in the window.
    // This is getting complicated for legacy OpenGL.
    
    // Let's just set glViewport for now, assuming full window for simplicity in this step.
    // We will refine this later.
}

}
