#include "GLFWWindow.h"
#include <stdio.h>

namespace ege {

GLFWWindow::GLFWWindow() : m_window(NULL), m_context(NULL), m_width(0), m_height(0) {
}

GLFWWindow::~GLFWWindow() {
    close();
}

bool GLFWWindow::create(int width, int height, const char* title) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    // Use legacy OpenGL for compatibility with immediate mode drawing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    
    m_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!m_window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    m_width = width;
    m_height = height;
    m_context = new OpenGLGraphicsContext();
    
    // Setup basic GL state
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1); // Top-left origin
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    return true;
}

void GLFWWindow::show() {
    if (m_window) glfwShowWindow(m_window);
}

void GLFWWindow::hide() {
    if (m_window) glfwHideWindow(m_window);
}

void GLFWWindow::close() {
    if (m_context) {
        delete m_context;
        m_context = NULL;
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
    if (m_window) glfwSwapBuffers(m_window);
}

GraphicsContext* GLFWWindow::getGraphicsContext() {
    return m_context;
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

}
