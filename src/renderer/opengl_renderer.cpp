/*
 * EGE (Easy Graphics Engine)
 * OpenGL Renderer Implementation
 */

#include "opengl_renderer.h"

#ifdef EGE_ENABLE_OPENGL
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace ege {
namespace renderer {

#ifdef EGE_ENABLE_OPENGL

// Simple vertex shader for 2D rendering
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Simple fragment shader for texture rendering
static const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D screenTexture;

void main()
{
    FragColor = texture(screenTexture, TexCoord);
}
)";

OpenGLRenderer::OpenGLRenderer()
    : m_window(nullptr)
    , m_width(0)
    , m_height(0)
    , m_fbo(0)
    , m_texture(0)
    , m_vao(0)
    , m_vbo(0)
    , m_shaderProgram(0)
    , m_bufferDirty(false)
{
}

OpenGLRenderer::~OpenGLRenderer()
{
    shutdown();
}

bool OpenGLRenderer::initialize(int width, int height)
{
    m_width = width;
    m_height = height;

    if (!initializeGLFW()) {
        return false;
    }

    if (!initializeOpenGL()) {
        return false;
    }

    // Allocate pixel buffer
    m_pixelBuffer.resize(width * height, 0xFF000000); // Black with full alpha
    
    createFramebuffer();
    
    return true;
}

void OpenGLRenderer::shutdown()
{
    if (m_window) {
        deleteFramebuffer();
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

bool OpenGLRenderer::initializeGLFW()
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    // Set OpenGL version to 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    m_window = glfwCreateWindow(m_width, m_height, "EGE - OpenGL", nullptr, nullptr);
    if (!m_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    return true;
}

bool OpenGLRenderer::initializeOpenGL()
{
    // Load OpenGL functions using GLAD
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return false;
    }

    printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        fprintf(stderr, "Vertex shader compilation failed: %s\n", infoLog);
        return false;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        fprintf(stderr, "Fragment shader compilation failed: %s\n", infoLog);
        return false;
    }

    // Link shaders
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
        fprintf(stderr, "Shader program linking failed: %s\n", infoLog);
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Set up vertex data for fullscreen quad
    float vertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void OpenGLRenderer::createFramebuffer()
{
    // Create texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::deleteFramebuffer()
{
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

void OpenGLRenderer::updateTexture()
{
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderer::beginFrame()
{
    glfwPollEvents();
}

void OpenGLRenderer::endFrame()
{
    if (m_bufferDirty) {
        updateTexture();
        m_bufferDirty = false;
    }

    // Render the texture to screen
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_shaderProgram);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(m_window);
}

void OpenGLRenderer::clear(uint32_t color)
{
    std::fill(m_pixelBuffer.begin(), m_pixelBuffer.end(), color);
    m_bufferDirty = true;
}

void OpenGLRenderer::setPixel(int x, int y, uint32_t color)
{
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_pixelBuffer[y * m_width + x] = color;
        m_bufferDirty = true;
    }
}

uint32_t OpenGLRenderer::getPixel(int x, int y) const
{
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_pixelBuffer[y * m_width + x];
    }
    return 0;
}

void OpenGLRenderer::drawLine(int x1, int y1, int x2, int y2, uint32_t color)
{
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        setPixel(x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void OpenGLRenderer::drawRectangle(int x, int y, int w, int h, uint32_t color)
{
    // Draw four lines
    drawLine(x, y, x + w - 1, y, color);           // Top
    drawLine(x, y, x, y + h - 1, color);           // Left
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // Right
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
}

void OpenGLRenderer::fillRectangle(int x, int y, int w, int h, uint32_t color)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            setPixel(i, j, color);
        }
    }
}

void OpenGLRenderer::drawCircle(int x, int y, int radius, uint32_t color)
{
    // Midpoint circle algorithm (Bresenham's circle)
    int cx = 0;
    int cy = radius;
    int d = 1 - radius;
    
    auto plotCirclePoints = [&](int cx, int cy) {
        setPixel(x + cx, y + cy, color);
        setPixel(x - cx, y + cy, color);
        setPixel(x + cx, y - cy, color);
        setPixel(x - cx, y - cy, color);
        setPixel(x + cy, y + cx, color);
        setPixel(x - cy, y + cx, color);
        setPixel(x + cy, y - cx, color);
        setPixel(x - cy, y - cx, color);
    };
    
    plotCirclePoints(cx, cy);
    
    while (cx < cy) {
        if (d < 0) {
            d += 2 * cx + 3;
        } else {
            d += 2 * (cx - cy) + 5;
            cy--;
        }
        cx++;
        plotCirclePoints(cx, cy);
    }
}

void OpenGLRenderer::fillCircle(int x, int y, int radius, uint32_t color)
{
    // Fill circle by drawing horizontal lines
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = static_cast<int>(std::sqrt(radius * radius - dy * dy));
        for (int i = x - dx; i <= x + dx; i++) {
            setPixel(i, y + dy, color);
        }
    }
}

void OpenGLRenderer::drawEllipse(int x, int y, int xRadius, int yRadius, uint32_t color)
{
    // Midpoint ellipse algorithm
    int x1 = 0;
    int y1 = yRadius;
    
    // Region 1
    int dx = 2 * yRadius * yRadius * x1;
    int dy = 2 * xRadius * xRadius * y1;
    int d1 = yRadius * yRadius - xRadius * xRadius * yRadius + (xRadius * xRadius) / 4;
    
    auto plotEllipsePoints = [&](int px, int py) {
        setPixel(x + px, y + py, color);
        setPixel(x - px, y + py, color);
        setPixel(x + px, y - py, color);
        setPixel(x - px, y - py, color);
    };
    
    while (dx < dy) {
        plotEllipsePoints(x1, y1);
        
        if (d1 < 0) {
            x1++;
            dx += 2 * yRadius * yRadius;
            d1 += dx + yRadius * yRadius;
        } else {
            x1++;
            y1--;
            dx += 2 * yRadius * yRadius;
            dy -= 2 * xRadius * xRadius;
            d1 += dx - dy + yRadius * yRadius;
        }
    }
    
    // Region 2
    int d2 = yRadius * yRadius * (x1 + 1) * (x1 + 1) + 
             xRadius * xRadius * (y1 - 1) * (y1 - 1) - 
             xRadius * xRadius * yRadius * yRadius;
    
    while (y1 >= 0) {
        plotEllipsePoints(x1, y1);
        
        if (d2 > 0) {
            y1--;
            dy -= 2 * xRadius * xRadius;
            d2 += xRadius * xRadius - dy;
        } else {
            y1--;
            x1++;
            dx += 2 * yRadius * yRadius;
            dy -= 2 * xRadius * xRadius;
            d2 += dx - dy + xRadius * xRadius;
        }
    }
}

void OpenGLRenderer::fillEllipse(int x, int y, int xRadius, int yRadius, uint32_t color)
{
    // Fill ellipse by drawing horizontal lines
    for (int dy = -yRadius; dy <= yRadius; dy++) {
        int dx = static_cast<int>(xRadius * std::sqrt(1.0 - (dy * dy) / (double)(yRadius * yRadius)));
        for (int i = x - dx; i <= x + dx; i++) {
            setPixel(i, y + dy, color);
        }
    }
}

void OpenGLRenderer::drawArc(int x, int y, int startAngle, int endAngle, int radius, uint32_t color)
{
    // Convert angles from degrees to radians
    // EGE uses degrees, 0 degrees is at 3 o'clock, counter-clockwise
    const double PI = 3.14159265358979323846;
    double startRad = startAngle * PI / 180.0;
    double endRad = endAngle * PI / 180.0;
    
    // Normalize angles
    while (endRad < startRad) {
        endRad += 2 * PI;
    }
    
    // Calculate number of points based on arc length
    double arcLength = endRad - startRad;
    int numPoints = static_cast<int>(std::ceil(arcLength * radius / 2.0));
    if (numPoints < 2) numPoints = 2;
    
    // Draw arc as connected line segments
    double angleStep = arcLength / (numPoints - 1);
    int prevX = x + static_cast<int>(radius * std::cos(startRad));
    int prevY = y - static_cast<int>(radius * std::sin(startRad)); // Y is inverted
    
    for (int i = 1; i < numPoints; i++) {
        double angle = startRad + i * angleStep;
        int currX = x + static_cast<int>(radius * std::cos(angle));
        int currY = y - static_cast<int>(radius * std::sin(angle));
        drawLine(prevX, prevY, currX, currY, color);
        prevX = currX;
        prevY = currY;
    }
}

void OpenGLRenderer::drawPolyline(int numPoints, const int* points, uint32_t color)
{
    // Draw connected line segments
    if (numPoints < 2 || !points) return;
    
    for (int i = 0; i < numPoints - 1; i++) {
        int x1 = points[i * 2];
        int y1 = points[i * 2 + 1];
        int x2 = points[(i + 1) * 2];
        int y2 = points[(i + 1) * 2 + 1];
        drawLine(x1, y1, x2, y2, color);
    }
}

void OpenGLRenderer::drawPolygon(int numPoints, const int* points, uint32_t color)
{
    // Draw polygon outline (polyline + closing line)
    if (numPoints < 2 || !points) return;
    
    drawPolyline(numPoints, points, color);
    
    // Close the polygon
    int x1 = points[(numPoints - 1) * 2];
    int y1 = points[(numPoints - 1) * 2 + 1];
    int x2 = points[0];
    int y2 = points[1];
    drawLine(x1, y1, x2, y2, color);
}

void OpenGLRenderer::fillPolygon(int numPoints, const int* points, uint32_t color)
{
    // Scanline polygon fill algorithm
    if (numPoints < 3 || !points) return;
    
    // Find bounding box
    int minY = points[1], maxY = points[1];
    for (int i = 1; i < numPoints; i++) {
        int y = points[i * 2 + 1];
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    
    // Scanline fill
    for (int scanY = minY; scanY <= maxY; scanY++) {
        std::vector<int> intersections;
        
        // Find intersections with polygon edges
        for (int i = 0; i < numPoints; i++) {
            int x1 = points[i * 2];
            int y1 = points[i * 2 + 1];
            int x2 = points[((i + 1) % numPoints) * 2];
            int y2 = points[((i + 1) % numPoints) * 2 + 1];
            
            // Check if scanline intersects this edge
            if ((y1 <= scanY && y2 > scanY) || (y2 <= scanY && y1 > scanY)) {
                // Calculate intersection x coordinate
                int x = x1 + (scanY - y1) * (x2 - x1) / (y2 - y1);
                intersections.push_back(x);
            }
        }
        
        // Sort intersections
        std::sort(intersections.begin(), intersections.end());
        
        // Fill between pairs of intersections
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            for (int x = intersections[i]; x <= intersections[i + 1]; x++) {
                setPixel(x, scanY, color);
            }
        }
    }
}

void OpenGLRenderer::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    m_pixelBuffer.resize(width * height, 0xFF000000);
    
    deleteFramebuffer();
    createFramebuffer();
    
    if (m_window) {
        glfwSetWindowSize(m_window, width, height);
    }
}

uint32_t* OpenGLRenderer::getPixelBuffer()
{
    return m_pixelBuffer.data();
}

void OpenGLRenderer::syncPixelBuffer()
{
    m_bufferDirty = true;
}

#endif // EGE_ENABLE_OPENGL

} // namespace renderer
} // namespace ege
