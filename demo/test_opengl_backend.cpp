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
    
    // Draw some basic shapes to test the renderer
    setcolor(GREEN);
    circle(320, 240, 100);
    
    setcolor(RED);
    rectangle(100, 100, 540, 380);
    
    setcolor(YELLOW);
    line(0, 0, 640, 480);
    line(640, 0, 0, 480);
    
    // Draw some pixels
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            putpixel(i + 50, j + 50, RGB(i * 2, j * 2, 128));
        }
    }
    
    outtextxy(250, 20, "EGE OpenGL Backend");
    
    // Wait for user input
    getch();
    
    closegraph();
    return 0;
}
