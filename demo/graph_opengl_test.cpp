/*
 * EGE (Easy Graphics Engine)
 * Demo: graph_opengl_test.cpp
 *
 * This demo demonstrates the usage of the OpenGL rendering backend.
 * It draws basic shapes using the same EGE API that works with both
 * GDI and OpenGL backends.
 *
 * To enable OpenGL backend:
 * 1. Build EGE with -DEGE_ENABLE_OPENGL=ON
 * 2. Use INIT_OPENGL flag: initgraph(640, 480, INIT_OPENGL);
 *
 * Note: This is an experimental feature for cross-platform support.
 * The OpenGL backend currently supports basic drawing operations.
 */

#include <ege.h>

int main()
{
    // Initialize with OpenGL backend (experimental)
    // If OpenGL backend is not available, falls back to GDI
#ifdef EGE_ENABLE_OPENGL
    ege::initgraph(640, 480, ege::INIT_OPENGL | ege::INIT_RENDERMANUAL);
    ege::setcaption("EGE OpenGL Backend Demo");
#else
    ege::initgraph(640, 480, ege::INIT_RENDERMANUAL);
    ege::setcaption("EGE Demo (GDI Backend)");
#endif

    // Set background color
    ege::setbkcolor(ege::EGERGB(32, 32, 48));
    ege::cleardevice();

    // Draw some shapes to demonstrate rendering
    // These functions work with both GDI and OpenGL backends

    // Draw a circle
    ege::setcolor(ege::EGERGB(255, 100, 100));
    ege::circle(160, 160, 80);

    // Draw a filled circle
    ege::setfillcolor(ege::EGERGB(100, 255, 100));
    ege::fillellipse(320, 160, 80, 80);

    // Draw a rectangle outline
    ege::setcolor(ege::EGERGB(100, 100, 255));
    ege::rectangle(420, 80, 580, 240);

    // Draw a filled rectangle
    ege::setfillcolor(ege::EGERGB(255, 255, 100));
    ege::bar(80, 280, 200, 400);

    // Draw some lines
    ege::setcolor(ege::EGERGB(255, 128, 0));
    for (int i = 0; i < 10; i++) {
        ege::line(240, 280 + i * 12, 560, 280 + i * 12);
    }

    // Draw an ellipse
    ege::setcolor(ege::EGERGB(255, 0, 255));
    ege::ellipse(400, 360, 0, 360, 120, 60);

    // Draw filled ellipse
    ege::setfillcolor(ege::EGERGB(0, 255, 255));
    ege::fillellipse(160, 360, 60, 40);

    // Draw some text
    ege::setcolor(ege::WHITE);
    ege::outtextxy(10, 10, "EGE Cross-Platform Backend Demo");
    ege::outtextxy(10, 30, "Press any key to exit...");

#ifdef EGE_ENABLE_OPENGL
    ege::outtextxy(10, 450, "Backend: OpenGL (experimental)");
#else
    ege::outtextxy(10, 450, "Backend: GDI (default)");
#endif

    // Update the display
    ege::flushwindow();

    // Wait for user input
    ege::getch();

    // Close graphics
    ege::closegraph();

    return 0;
}
