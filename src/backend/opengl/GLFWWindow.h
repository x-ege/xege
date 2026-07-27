#pragma once
#include "../interface/Window.h"
#include "GlRenderTarget.h"
#include <GLFW/glfw3.h>

namespace ege {

class GLFWWindow : public Window {
public:
    GLFWWindow();
    ~GLFWWindow();
    bool create(int width, int height, const char* title) override;
    void show() override;
    void hide() override;
    void setTitle(const char* title) override;
    void setSize(int width, int height) override;
    void setPosition(int x, int y) override;
    void setCursorVisible(bool visible) override;
    void close() override;
    bool isClosed() override;
    void processEvents() override;
    void swapBuffers() override;
    GraphicsContext* getGraphicsContext() override;
    void* getNativeHandle() override;
    int getWidth() const override;
    int getHeight() const override;

    // OpenGL-specific
    GlRenderTarget* getRenderTarget() { return m_renderTarget; }

private:
    void resizeRenderTarget(int width, int height);

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void cursorPositionCallback(GLFWwindow* window, double x, double y);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void windowCloseCallback(GLFWwindow* window);
    static void windowSizeCallback(GLFWwindow* window, int width, int height);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window;
    GlRenderTarget* m_renderTarget;  // Screen render target
    int m_width;
    int m_height;
    int m_framebufferWidth;
    int m_framebufferHeight;
    double m_lastClickTime[5];
    int m_lastClickX[5];
    int m_lastClickY[5];
};

}
