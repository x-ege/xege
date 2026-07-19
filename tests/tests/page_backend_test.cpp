#include "ege.h"
#include "../test_opengl_mode.h"

#include "../../src/ege_head.h"
#include "../../src/ege_graph.h"
#include "../../src/backend/opengl/GLFWWindow.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;
const int testWidth = 160;
const int testHeight = 120;

unsigned int rgb(ege::color_t color)
{
    return color & 0x00FFFFFFU;
}

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectDisplayedPixel(int x, int y, ege::color_t expected, const std::string& message)
{
    ege::GLFWWindow* window = dynamic_cast<ege::GLFWWindow*>(ege::graph_setting.window);
    if (!window || !window->getRenderTarget()) {
        expect(false, message + " (missing OpenGL presentation target)");
        return;
    }
    const ege::color_t actual = window->getRenderTarget()->getPixel(x, y);
    if (rgb(actual) != rgb(expected)) {
        ++failures;
        std::cerr << "FAIL: " << message << ", expected RGB=0x" << std::hex
                  << rgb(expected) << ", actual RGB=0x" << rgb(actual) << std::dec << '\n';
    }
}

void clearActivePage(ege::color_t color)
{
    ege::setviewport(0, 0, testWidth, testHeight, true);
    ege::setbkcolor(color);
    ege::cleardevice();
}

} // namespace

int main()
{
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
    ege::initgraph(testWidth, testHeight, with_opengl_test_mode(mode));
    if (!ege::getHWnd()) {
        std::cerr << "FAIL: unable to create hidden GLFW page test window\n";
        return EXIT_FAILURE;
    }

    ege::GLFWWindow* window = dynamic_cast<ege::GLFWWindow*>(ege::graph_setting.window);
    if (window && window->getRenderTarget()) {
        window->getRenderTarget()->clear(ege::MAGENTA);
        window->swapBuffers();
        expectDisplayedPixel(8, 7, ege::MAGENTA,
                             "the OpenGL window target presents a direct clear");
    }

    ege::setactivepage(0);
    clearActivePage(ege::RED);
    expect(rgb(ege::getpixel(8, 7)) == rgb(ege::RED),
           "active page 0 retains its red pixels before presentation");
    ege::setactivepage(1);
    clearActivePage(ege::BLUE);
    expect(rgb(ege::getpixel(8, 7)) == rgb(ege::BLUE),
           "active page 1 retains its blue pixels before presentation");

    ege::setvisualpage(0);
    ege::flushwindow();
    expectDisplayedPixel(8, 7, ege::RED, "visual page 0 is presented");

    ege::setvisualpage(1);
    ege::flushwindow();
    expectDisplayedPixel(8, 7, ege::BLUE, "visual page 1 is presented");

    ege::setactivepage(0);
    ege::putpixel(8, 7, ege::GREEN);
    expect(rgb(ege::getpixel(8, 7)) == rgb(ege::GREEN),
           "active page 0 accepts drawing while page 1 remains visual");
    ege::flushwindow();
    expectDisplayedPixel(8, 7, ege::BLUE,
                         "drawing to a non-visual page does not alter the displayed page");
    expect(rgb(ege::getpixel(8, 7)) == rgb(ege::GREEN),
           "non-visual page 0 retains drawing after page 1 presentation");

    ege::setvisualpage(0);
    ege::flushwindow();
    expectDisplayedPixel(8, 7, ege::GREEN,
                         "page contents are retained until that page becomes visual");

    ege::closegraph();
    if (failures != 0) {
        std::cerr << failures << " page backend assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All page backend assertions passed\n";
    return EXIT_SUCCESS;
}
