#include "ege.h"
#include "../test_opengl_mode.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>

int main()
{
    // Keep the default force-exit policy. Legacy Win32 EGE terminates the
    // process when an unhandled close request reaches a default-mode window;
    // native OpenGL backends must preserve that behavior even when a demo's
    // own loop does not poll is_run().
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_HIDE);
    ege::initgraph(32, 24, with_opengl_test_mode(mode));
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        std::cerr << "FAIL: unable to create hidden GLFW exit test window\n";
        return EXIT_FAILURE;
    }

    GLFWkeyfun keyCallback = glfwSetKeyCallback(window, nullptr);
    if (!keyCallback) {
        std::cerr << "FAIL: GLFW key callback is not installed\n";
        return EXIT_FAILURE;
    }
    glfwSetKeyCallback(window, keyCallback);

    keyCallback(window, GLFW_KEY_Q, 0, GLFW_PRESS, GLFW_MOD_SUPER);
    ege::delay_ms(0);

    std::cerr << "FAIL: default-mode Command+Q returned instead of exiting\n";
    return EXIT_FAILURE;
}
