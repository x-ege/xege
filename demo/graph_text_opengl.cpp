// OpenGL text rendering demo — tests Chinese/English text, font styles, and offscreen rendering
#include "graphics.h"
#include <cstdio>

int main() {
    initgraph(800, 600);
    setrendermode(RENDER_MANUAL);

    // Test 1: Basic English text
    setcolor(WHITE);
    setbkmode(TRANSPARENT);
    setfont(24, 0, "Arial");
    outtextxy(50, 30, "Hello, World! - OpenGL Text Rendering");

    // Test 2: Chinese text (PingFang on macOS)
    setfont(28, 0, "PingFang SC");
    setcolor(YELLOW);
    outtextxy(50, 80, "中文文字渲染测试");

    // Test 3: Different font sizes
    setcolor(GREEN);
    setfont(14, 0, "Arial");
    outtextxy(50, 140, "Small text (14px)");
    setfont(20, 0, "Arial");
    setcolor(LIGHTBLUE);
    outtextxy(50, 170, "Medium text (20px)");
    setfont(36, 0, "Arial");
    setcolor(RED);
    outtextxy(50, 210, "Large text (36px)");

    // Test 4: Bold text
    setfont(24, 0, "Arial", 0, 0, 700, false, false, false);
    setcolor(MAGENTA);
    outtextxy(50, 280, "Bold text (weight=700)");

    // Test 5: Mixed Chinese + English
    setfont(22, 0, "PingFang SC");
    setcolor(WHITE);
    outtextxy(50, 340, "Mixed: Hello World!");

    // Test 6: Offscreen image text rendering
    PIMAGE offImg = newimage(300, 100);
    settarget(offImg);
    setbkcolor(BLACK);
    cleardevice();
    setfont(20, 0, "Arial");
    setcolor(CYAN);
    outtextxy(10, 10, "Offscreen text test");
    setfont(18, 0, "PingFang SC");
    setcolor(LIGHTRED);
    outtextxy(10, 50, "Offscreen Chinese");
    settarget(NULL);

    // Draw the offscreen image onto the main window
    putimage(450, 30, offImg);

    // Test 7: Text measurement
    setfont(20, 0, "Arial");
    int w = textwidth("Hello");
    int h = textheight("Hello");
    char buf[128];
    snprintf(buf, sizeof(buf), "textwidth(Hello)=%d, textheight=%d", w, h);
    setcolor(WHITE);
    outtextxy(50, 420, buf);

    // Test 8: Rotated text (escapement = 900 = 90 degrees)
    setfont(24, 0, "Arial", 900, 0, 400, false, false, false);
    setcolor(LIGHTGREEN);
    outtextxy(600, 400, "Rotated 90deg");

    // Draw some primitives to show text + graphics coexist
    setcolor(LIGHTGRAY);
    line(0, 480, 800, 480);
    setfillcolor(EGERGB(30, 30, 50));
    bar(50, 500, 200, 570);
    setcolor(WHITE);
    setfont(16, 0, "Arial");
    outtextxy(60, 520, "Primitives + Text");

    delimage(offImg);

    // Wait for user to close
    for (; is_run(); delay_fps(30)) {
        if (kbhit()) break;
    }

    closegraph();
    return 0;
}
