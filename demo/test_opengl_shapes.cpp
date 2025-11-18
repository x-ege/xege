/*
 * EGE OpenGL Backend - Advanced Shapes Demo
 * 
 * This demo showcases circle and ellipse drawing capabilities
 * in the OpenGL backend implementation.
 */

#include <graphics.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main()
{
    // Initialize with OpenGL backend
    initgraph(800, 600, INIT_OPENGL);
    
    setbkcolor(BLACK);
    cleardevice();
    
    // Title
    setcolor(WHITE);
    outtextxy(300, 10, "OpenGL Backend - Advanced Shapes");
    
    // Draw concentric circles
    setcolor(CYAN);
    for (int r = 20; r <= 100; r += 20) {
        circle(150, 150, r);
    }
    outtextxy(120, 260, "Circles");
    
    // Draw filled circles with different colors
    setfillcolor(RED);
    fillcircle(400, 150, 40);
    setfillcolor(GREEN);
    fillcircle(450, 150, 40);
    setfillcolor(BLUE);
    fillcircle(425, 120, 40);
    outtextxy(360, 260, "Filled Circles");
    
    // Draw concentric ellipses
    setcolor(YELLOW);
    for (int i = 1; i <= 3; i++) {
        ellipse(150, 420, 0, 360, 30 * i, 20 * i);
    }
    outtextxy(110, 520, "Ellipses");
    
    // Draw filled ellipses
    setfillcolor(MAGENTA);
    fillellipse(400, 420, 80, 40);
    setfillcolor(CYAN);
    fillellipse(500, 420, 40, 60);
    outtextxy(350, 520, "Filled Ellipses");
    
    // Draw decorative border
    setcolor(WHITE);
    rectangle(10, 10, 790, 590);
    rectangle(12, 12, 788, 588);
    
    // Draw diagonal lines
    setcolor(DARKGRAY);
    for (int i = 0; i < 800; i += 50) {
        line(i, 0, 800, 600 - i * 600 / 800);
        line(0, i * 600 / 800, 800 - i, 600);
    }
    
    // Instructions
    setcolor(LIGHTGRAY);
    outtextxy(250, 570, "Press any key to exit...");
    
    getch();
    closegraph();
    return 0;
}
