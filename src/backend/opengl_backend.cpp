/*
 * EGE (Easy Graphics Engine)
 * FileName:    opengl_backend.cpp
 *
 * OpenGL rendering backend implementation - provides cross-platform rendering support.
 * This backend uses GLFW for window management and OpenGL for rendering.
 *
 * Note: This is an experimental feature for cross-platform support.
 * Enable with INIT_OPENGL flag in initgraph().
 */

#ifdef EGE_ENABLE_OPENGL

#include "opengl_backend.h"

// Include GLAD before GLFW (GLAD includes OpenGL headers)
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ege
{

// GLFW error callback
static void glfwErrorCallback(int error, const char* description)
{
    // Log error to stderr for debugging
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// GLFW framebuffer size callback
static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);

    // Update orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

OpenGLBackend::OpenGLBackend()
    : m_window(nullptr)
    , m_initialized(false)
    , m_width(0)
    , m_height(0)
    , m_clearColor(0)
    , m_drawColor(0xFFFFFFFF)
{
}

OpenGLBackend::~OpenGLBackend()
{
    shutdown();
}

RenderBackendType OpenGLBackend::getType() const
{
    return BACKEND_OPENGL;
}

bool OpenGLBackend::initGLFW()
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        return false;
    }

    // Request OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    return true;
}

bool OpenGLBackend::initGLAD()
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return false;
    }
    return true;
}

void OpenGLBackend::setupOpenGLState()
{
    // Set up orthographic projection for 2D rendering
    // Origin at top-left, Y increases downward (matching GDI behavior)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, m_width, m_height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Enable blending for alpha
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth testing for 2D
    glDisable(GL_DEPTH_TEST);

    // Set point size and line width
    glPointSize(1.0f);
    glLineWidth(1.0f);
}

bool OpenGLBackend::initialize(int width, int height, int mode)
{
    if (m_initialized) {
        return true;
    }

    m_width = width;
    m_height = height;

    // Initialize GLFW
    if (!initGLFW()) {
        return false;
    }

    // Create window
    // Check for borderless mode
    if (mode & INIT_NOBORDER) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    }

    // Check for hidden mode
    if (mode & INIT_HIDE) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    m_window = glfwCreateWindow(width, height, "EGE OpenGL", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }

    // Check for topmost mode
    if (mode & INIT_TOPMOST) {
        glfwSetWindowAttrib(m_window, GLFW_FLOATING, GLFW_TRUE);
    }

    glfwMakeContextCurrent(m_window);

    // Initialize GLAD
    if (!initGLAD()) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
        return false;
    }

    // Set up callbacks
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    // Enable vsync
    glfwSwapInterval(1);

    // Set up OpenGL state
    setupOpenGLState();

    // Allocate frame buffer for software rendering fallback (using vector for RAII)
    m_frameBuffer.resize(width * height, 0);

    m_initialized = true;
    return true;
}

void OpenGLBackend::shutdown()
{
    // Clear frame buffer (vector handles memory automatically)
    m_frameBuffer.clear();
    m_frameBuffer.shrink_to_fit();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    // Note: glfwTerminate() should only be called when no more GLFW windows exist
    // In a multi-window scenario, this should be handled differently
    if (m_initialized) {
        glfwTerminate();
    }

    m_initialized = false;
    m_width = 0;
    m_height = 0;
}

bool OpenGLBackend::isInitialized() const
{
    return m_initialized && m_window != nullptr;
}

void OpenGLBackend::swapBuffers()
{
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void OpenGLBackend::setGLColor(color_t color)
{
    float r = EGEGET_R(color) / 255.0f;
    float g = EGEGET_G(color) / 255.0f;
    float b = EGEGET_B(color) / 255.0f;
    float a = EGEGET_A(color) / 255.0f;
    glColor4f(r, g, b, a);
}

void OpenGLBackend::clear(color_t color)
{
    m_clearColor = color;
    float r = EGEGET_R(color) / 255.0f;
    float g = EGEGET_G(color) / 255.0f;
    float b = EGEGET_B(color) / 255.0f;
    float a = EGEGET_A(color) / 255.0f;

    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLBackend::putPixel(int x, int y, color_t color)
{
    setGLColor(color);
    glBegin(GL_POINTS);
    glVertex2i(x, y);
    glEnd();

    // Also update frame buffer
    if (!m_frameBuffer.empty() && x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_frameBuffer[y * m_width + x] = color;
    }
}

color_t OpenGLBackend::getPixel(int x, int y)
{
    // Read from frame buffer
    if (!m_frameBuffer.empty() && x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_frameBuffer[y * m_width + x];
    }
    return 0;
}

void OpenGLBackend::drawLine(int x1, int y1, int x2, int y2, color_t color)
{
    setGLColor(color);
    glBegin(GL_LINES);
    glVertex2i(x1, y1);
    glVertex2i(x2, y2);
    glEnd();
}

void OpenGLBackend::drawRectangle(int left, int top, int right, int bottom, color_t color)
{
    setGLColor(color);
    glBegin(GL_LINE_LOOP);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
    glEnd();
}

void OpenGLBackend::fillRectangle(int left, int top, int right, int bottom, color_t color)
{
    setGLColor(color);
    glBegin(GL_QUADS);
    glVertex2i(left, top);
    glVertex2i(right, top);
    glVertex2i(right, bottom);
    glVertex2i(left, bottom);
    glEnd();
}

void OpenGLBackend::drawCircleGL(int x, int y, int radius, bool fill)
{
    const int segments = 64;

    if (fill) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2i(x, y);
    } else {
        glBegin(GL_LINE_LOOP);
    }

    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * (float)M_PI * i / segments;
        float px = x + radius * std::cos(angle);
        float py = y + radius * std::sin(angle);
        glVertex2f(px, py);
    }

    glEnd();
}

void OpenGLBackend::drawCircle(int x, int y, int radius, color_t color)
{
    setGLColor(color);
    drawCircleGL(x, y, radius, false);
}

void OpenGLBackend::fillCircle(int x, int y, int radius, color_t color)
{
    setGLColor(color);
    drawCircleGL(x, y, radius, true);
}

void OpenGLBackend::drawEllipseGL(int x, int y, int xRadius, int yRadius, bool fill)
{
    const int segments = 64;

    if (fill) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2i(x, y);
    } else {
        glBegin(GL_LINE_LOOP);
    }

    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * (float)M_PI * i / segments;
        float px = x + xRadius * std::cos(angle);
        float py = y + yRadius * std::sin(angle);
        glVertex2f(px, py);
    }

    glEnd();
}

void OpenGLBackend::drawEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    setGLColor(color);
    drawEllipseGL(x, y, xRadius, yRadius, false);
}

void OpenGLBackend::fillEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    setGLColor(color);
    drawEllipseGL(x, y, xRadius, yRadius, true);
}

bool OpenGLBackend::processEvents()
{
    if (!m_window) {
        return false;
    }

    glfwPollEvents();
    return !glfwWindowShouldClose(m_window);
}

int OpenGLBackend::getWidth() const
{
    return m_width;
}

int OpenGLBackend::getHeight() const
{
    return m_height;
}

} // namespace ege

#endif // EGE_ENABLE_OPENGL
