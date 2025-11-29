/*
 * EGE OpenGL Backend - Polygon and Arc Demo
 * 
 * This demo showcases polygon, arc, and polyline drawing capabilities
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
    outtextxy(250, 10, "OpenGL Backend - Polygons & Arcs");
    
    // Draw triangle (polygon)
    setcolor(CYAN);
    int triangle[] = {100, 100, 150, 50, 200, 100};
    polygon(3, triangle);
    outtextxy(120, 120, "Triangle");
    
    // Draw filled pentagon
    setfillcolor(RED);
    const int sides = 5;
    int pentagon[sides * 2];
    int cx = 150, cy = 250;
    int radius = 60;
    for (int i = 0; i < sides; i++) {
        double angle = -M_PI / 2 + i * 2 * M_PI / sides;
        pentagon[i * 2] = cx + (int)(radius * cos(angle));
        pentagon[i * 2 + 1] = cy + (int)(radius * sin(angle));
    }
    fillpolygon(sides, pentagon);
    outtextxy(115, 330, "Pentagon");
    
    // Draw hexagon outline
    setcolor(GREEN);
    const int hex_sides = 6;
    int hexagon[hex_sides * 2];
    cx = 150; cy = 450;
    radius = 50;
    for (int i = 0; i < hex_sides; i++) {
        double angle = i * 2 * M_PI / hex_sides;
        hexagon[i * 2] = cx + (int)(radius * cos(angle));
        hexagon[i * 2 + 1] = cy + (int)(radius * sin(angle));
    }
    polygon(hex_sides, hexagon);
    outtextxy(118, 520, "Hexagon");
    
    // Draw polyline (wave pattern)
    setcolor(YELLOW);
    const int wave_points = 20;
    int wave[wave_points * 2];
    for (int i = 0; i < wave_points; i++) {
        wave[i * 2] = 300 + i * 15;
        wave[i * 2 + 1] = 150 + (int)(30 * sin(i * M_PI / 5));
    }
    polyline(wave_points, wave);
    outtextxy(400, 200, "Polyline");
    
    // Draw arcs
    setcolor(MAGENTA);
    arc(500, 300, 0, 90, 60);      // Quarter circle
    outtextxy(450, 380, "Quarter Arc");
    
    setcolor(CYAN);
    arc(500, 300, 90, 270, 60);    // Half circle
    
    setcolor(LIGHTBLUE);
    arc(500, 450, 0, 180, 50);     // Semicircle
    outtextxy(460, 520, "Semicircle");
    
    // Draw filled star polygon
    setfillcolor(YELLOW);
    const int star_points = 10;
    int star[star_points * 2];
    cx = 650; cy = 150;
    int outer_radius = 60;
    int inner_radius = 25;
    for (int i = 0; i < star_points; i++) {
        double angle = -M_PI / 2 + i * M_PI / 5;
        int r = (i % 2 == 0) ? outer_radius : inner_radius;
        star[i * 2] = cx + (int)(r * cos(angle));
        star[i * 2 + 1] = cy + (int)(r * sin(angle));
    }
    fillpolygon(star_points, star);
    outtextxy(630, 230, "Star");
    
    // Draw complex polygon (arrow)
    setcolor(LIGHTGREEN);
    int arrow[] = {
        600, 350,  // tip
        550, 400,  // left notch
        575, 400,  // left inner
        575, 450,  // left bottom
        625, 450,  // right bottom
        625, 400,  // right inner
        650, 400   // right notch
    };
    polygon(7, arrow);
    outtextxy(600, 470, "Arrow");
    
    // Draw decorative border
    setcolor(WHITE);
    rectangle(5, 5, 795, 595);
    
    // Instructions
    setcolor(LIGHTGRAY);
    outtextxy(270, 570, "Press any key to exit...");
    
    getch();
    closegraph();
    return 0;
}
