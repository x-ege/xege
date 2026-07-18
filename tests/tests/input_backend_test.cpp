#include "ege.h"
#include "../test_opengl_mode.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

ege::key_msg takeKeyMessage(const std::string& description)
{
    if (!ege::kbmsg()) {
        expect(false, description + " is queued");
        return {};
    }
    return ege::getkey();
}

ege::mouse_msg takeMouseMessage(const std::string& description)
{
    if (!ege::mousemsg()) {
        expect(false, description + " is queued");
        return {};
    }
    return ege::getmouse();
}

} // namespace

int main()
{
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
    ege::initgraph(80, 60, with_opengl_test_mode(mode));
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        std::cerr << "FAIL: unable to create hidden GLFW input test window\n";
        return EXIT_FAILURE;
    }

    GLFWkeyfun keyCallback = glfwSetKeyCallback(window, nullptr);
    expect(keyCallback != nullptr, "GLFW key callback is installed");
    if (keyCallback) {
        glfwSetKeyCallback(window, keyCallback);
        keyCallback(window, GLFW_KEY_A, 0, GLFW_PRESS, 0);
        expect(ege::keystate(ege::key_A), "key press updates keystate");
        expect(ege::keypress(ege::key_A) == 1, "key press updates the press counter");
        const ege::key_msg down = takeKeyMessage("key-down message");
        expect(down.msg == ege::key_msg_down && down.key == ege::key_A &&
               (down.flags & ege::key_flag_first_down),
               "GLFW key press maps to an EGE first-down message");

        keyCallback(window, GLFW_KEY_A, 0, GLFW_RELEASE, 0);
        expect(!ege::keystate(ege::key_A), "key release clears keystate");
        expect(ege::keyrelease(ege::key_A) == 1, "key release updates the release counter");
        const ege::key_msg up = takeKeyMessage("key-up message");
        expect(up.msg == ege::key_msg_up && up.key == ege::key_A,
               "GLFW key release maps to an EGE key-up message");

        keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
        const bool escapePending = ege::kbhit() != 0;
        expect(escapePending, "kbhit reports Escape instead of discarding it");
        if (escapePending) {
            const ege::key_msg escape = takeKeyMessage("Escape key-down message");
            expect(escape.msg == ege::key_msg_down && escape.key == ege::key_esc,
                   "GLFW Escape maps to the legacy EGE key_esc value");
        }
        keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_RELEASE, 0);
        ege::flushkey();
    }

    GLFWcharfun charCallback = glfwSetCharCallback(window, nullptr);
    expect(charCallback != nullptr, "GLFW character callback is installed");
    if (charCallback) {
        glfwSetCharCallback(window, charCallback);
        charCallback(window, 0x4E2D);
        const ege::key_msg character = takeKeyMessage("character message");
        expect(character.msg == ege::key_msg_char && character.key == 0x4E2D,
               "GLFW Unicode input maps to an EGE character message");
    }

    GLFWcursorposfun cursorCallback = glfwSetCursorPosCallback(window, nullptr);
    expect(cursorCallback != nullptr, "GLFW cursor callback is installed");
    if (cursorCallback) {
        glfwSetCursorPosCallback(window, cursorCallback);
        cursorCallback(window, 11.0, 13.0);
        const ege::mouse_msg move = takeMouseMessage("mouse-move message");
        expect(move.msg == ege::mouse_msg_move && move.x == 11 && move.y == 13,
               "GLFW cursor motion maps to EGE coordinates");
    }

    GLFWmousebuttonfun buttonCallback = glfwSetMouseButtonCallback(window, nullptr);
    expect(buttonCallback != nullptr, "GLFW mouse-button callback is installed");
    if (buttonCallback) {
        glfwSetMouseButtonCallback(window, buttonCallback);
        buttonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        expect(ege::keystate(ege::key_mouse_l), "mouse press updates keystate");
        const ege::mouse_msg down = takeMouseMessage("mouse-button-down message");
        expect(down.msg == ege::mouse_msg_down && down.is_left() &&
               down.x == 11 && down.y == 13,
               "GLFW left press maps to an EGE mouse-down message");

        buttonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        expect(!ege::keystate(ege::key_mouse_l), "mouse release clears keystate");
        const ege::mouse_msg up = takeMouseMessage("mouse-button-up message");
        expect(up.msg == ege::mouse_msg_up && up.is_left(),
               "GLFW left release maps to an EGE mouse-up message");
    }

    GLFWscrollfun scrollCallback = glfwSetScrollCallback(window, nullptr);
    expect(scrollCallback != nullptr, "GLFW scroll callback is installed");
    if (scrollCallback) {
        glfwSetScrollCallback(window, scrollCallback);
        scrollCallback(window, 0.0, 1.0);
        const ege::mouse_msg wheel = takeMouseMessage("mouse-wheel message");
        expect(wheel.msg == ege::mouse_msg_wheel && wheel.wheel == 120,
               "GLFW scroll maps to a Win32-compatible EGE wheel delta");
    }

    if (keyCallback) {
        keyCallback(window, GLFW_KEY_Q, 0, GLFW_PRESS, GLFW_MOD_SUPER);
        expect(!ege::is_run(), "Command+Q requests a clean EGE shutdown");
        expect(glfwWindowShouldClose(window) == GLFW_FALSE,
               "INIT_NOFORCEEXIT keeps the native window reusable after Command+Q");
    }

    ege::closegraph();
    if (failures != 0) {
        std::cerr << failures << " input backend assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All input backend assertions passed\n";
    return EXIT_SUCCESS;
}
