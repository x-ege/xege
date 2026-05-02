#include "GLFWWindow.h"
#include "glad/gl.h"
#include <cstdio>

namespace ege {

GLFWWindow::GLFWWindow() : m_window(NULL), m_renderTarget(NULL), m_width(0), m_height(0) {
}

GLFWWindow::~GLFWWindow() {
    close();
}

bool GLFWWindow::create(int width, int height, const char* title) {
    if (!glfwInit()) {
        const char* desc = NULL;
        int err = glfwGetError(&desc);
        if (desc) {
            fprintf(stderr, "Failed to initialize GLFW (%d): %s\n", err, desc);
        } else {
            fprintf(stderr, "Failed to initialize GLFW\n");
        }
        return false;
    }

    // Request OpenGL 3.3 Core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!m_window) {
        const char* desc = NULL;
        int err = glfwGetError(&desc);
        if (desc) {
            fprintf(stderr, "Failed to create GLFW window (%d): %s\n", err, desc);
        } else {
            fprintf(stderr, "Failed to create GLFW window\n");
        }
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Load OpenGL functions with GLAD
    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    m_width = width;
    m_height = height;

    // Create screen render target
    m_renderTarget = new GlRenderTarget();
    if (!m_renderTarget->initOnScreen(width, height)) {
        fprintf(stderr, "Failed to initialize screen RenderTarget\n");
        delete m_renderTarget;
        m_renderTarget = NULL;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Setup basic GL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void GLFWWindow::show() {
    if (m_window) glfwShowWindow(m_window);
}

void GLFWWindow::hide() {
    if (m_window) glfwHideWindow(m_window);
}

void GLFWWindow::close() {
    if (m_renderTarget) {
        delete m_renderTarget;
        m_renderTarget = NULL;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = NULL;
        glfwTerminate();
    }
}

bool GLFWWindow::isClosed() {
    return !m_window || glfwWindowShouldClose(m_window);
}

void GLFWWindow::processEvents() {
    glfwPollEvents();
}

void GLFWWindow::swapBuffers() {
    if (m_window) {
        if (m_renderTarget) m_renderTarget->flush();
        glfwSwapBuffers(m_window);
    }
}

GraphicsContext* GLFWWindow::getGraphicsContext() {
    // Return the GlRenderTarget as a GraphicsContext for backward compatibility.
    // GlRenderTarget inherits from RenderTarget, not GraphicsContext.
    // This returns NULL — callers should use getRenderTarget() instead.
    // The drawing functions in egegapi.cpp are updated to use m_renderTarget.
    return NULL;
}

void* GLFWWindow::getNativeHandle() {
    return (void*)m_window;
}

int GLFWWindow::getWidth() const {
    return m_width;
}

int GLFWWindow::getHeight() const {
    return m_height;
}

} // namespace ege
