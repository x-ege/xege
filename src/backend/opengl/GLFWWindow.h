#pragma once
#include "../interface/Window.h"
#include "OpenGLGraphicsContext.h"
#include <GLFW/glfw3.h>

namespace ege {

class GLFWWindow : public Window {
public:
    GLFWWindow();
    ~GLFWWindow();
    bool create(int width, int height, const char* title) override;
    void show() override;
    void hide() override;
    void close() override;
    bool isClosed() override;
    void processEvents() override;
    void swapBuffers() override;
    GraphicsContext* getGraphicsContext() override;
    void* getNativeHandle() override;
    int getWidth() const override;
    int getHeight() const override;

private:
    GLFWwindow* m_window;
    OpenGLGraphicsContext* m_context;
    int m_width;
    int m_height;
};

}
