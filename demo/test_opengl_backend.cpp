/*
 * EGE OpenGL Backend Test
 * 
 * This program demonstrates the new INIT_OPENGL flag for cross-platform rendering.
 * It creates a simple window and draws some basic graphics using the OpenGL backend.
 */

#include <graphics.h>

int main()
{
    // Initialize graphics with OpenGL backend
    // This will use GLFW and OpenGL 3.3 for cross-platform support
    initgraph(640, 480, INIT_OPENGL);
    
    // Clear screen with black
    cleardevice();
    
    // Draw circles
    setcolor(GREEN);
    circle(320, 240, 100);
    circle(320, 240, 80);
    circle(320, 240, 60);
    
    // Draw filled circles
    setfillcolor(RED);
    fillcircle(150, 120, 40);
    
    setfillcolor(BLUE);
    fillcircle(490, 120, 40);
    
    // Draw ellipses
    setcolor(YELLOW);
    ellipse(320, 350, 0, 360, 80, 40);
    
    setfillcolor(CYAN);
    fillellipse(320, 350, 60, 30);
    
    // Draw rectangles
    setcolor(RED);
    rectangle(50, 50, 590, 430);
    
    // Draw lines
    setcolor(YELLOW);
    line(0, 0, 640, 480);
    line(640, 0, 0, 480);
    
    // Draw some pixels for testing
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 50; j++) {
            putpixel(i + 20, j + 20, RGB(i * 5, j * 5, 128));
        }
    }
    
    outtextxy(220, 20, "EGE OpenGL Backend - Phase 3");
    
    // Wait for user input
    getch();
    
    closegraph();
    return 0;
}
