#include <graphics.h>

#include <cmath>
#include <cstdio>

namespace
{

constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr float kPi = 3.14159265358979323846f;

void drawPanel(int left, int top, int right, int bottom, const char* title)
{
    setfillstyle(SOLID_FILL, EGERGB(28, 32, 42));
    bar(left, top, right, bottom);
    setcolor(EGERGB(82, 92, 112));
    rectangle(left, top, right, bottom);
    setfont(16, 0, "Arial");
    setcolor(EGERGB(220, 224, 235));
    outtextxy(left + 10, top + 7, title);
}

void drawPrimitivePanel()
{
    drawPanel(15, 15, 305, 155, "Primitives and line styles");

    setcolor(EGERGB(255, 96, 96));
    setlinestyle(SOLID_LINE, 0, 1);
    line(35, 55, 135, 55);

    setcolor(EGERGB(90, 210, 255));
    setlinestyle(DASHED_LINE, 0, 1);
    line(35, 75, 135, 75);

    setcolor(EGERGB(255, 215, 70));
    setlinestyle(DOTTED_LINE, 0, 1);
    line(35, 95, 135, 95);

    setlinestyle(SOLID_LINE, 0, 1);
    setcolor(EGERGB(160, 255, 130));
    rectangle(165, 48, 225, 105);
    circle(260, 77, 31);

    setfillstyle(SOLID_FILL, EGERGB(210, 90, 235));
    fillellipse(55, 115, 28, 18);
    setfillcolor(EGERGB(70, 200, 155));
    ege_fillellipse(105.0f, 111.0f, 55.0f, 28.0f);
    setcolor(EGERGB(255, 160, 60));
    arc(205, 122, 15, 275, 29);
}

void drawFillPanel()
{
    drawPanel(320, 15, 625, 155, "Fill patterns");
    setbkcolor(EGERGB(18, 22, 30));

    setfillstyle(LINE_FILL, EGERGB(255, 100, 105));
    bar(342, 50, 412, 130);
    setfillstyle(LTSLASH_FILL, EGERGB(100, 225, 150));
    bar(432, 50, 502, 130);
    setfillstyle(WIDE_DOT_FILL, EGERGB(100, 165, 255));
    bar(522, 50, 592, 130);
    setfillstyle(SOLID_FILL, WHITE);
}

void drawViewportPanel()
{
    setviewport(38, 208, 282, 292, true);
    setbkcolor(EGERGB(20, 25, 34));
    cleardevice();
    setfillstyle(SOLID_FILL, EGERGB(50, 110, 205));
    bar(-20, 18, 95, 72);
    setfillstyle(SOLID_FILL, EGERGB(225, 80, 115));
    fillellipse(118, 42, 75, 50);
    setcolor(EGERGB(255, 235, 90));
    setlinestyle(DASHED_LINE, 0, 1);
    line(-25, -10, 275, 100);
    setviewport(0, 0, kWidth, kHeight, false);

    setlinestyle(SOLID_LINE, 0, 1);
    setcolor(EGERGB(110, 125, 150));
    rectangle(38, 208, 282, 292);
    setfont(16, 0, "Arial");
    setcolor(EGERGB(220, 224, 235));
    outtextxy(25, 177, "Viewport and clipping");
}

void drawAlphaPanel()
{
    drawPanel(320, 170, 625, 315, "Alpha and image transfers");

    PIMAGE alphaImage = newimage(86, 86);
    color_t* alphaPixels = getbuffer(alphaImage);
    for (int y = 0; y < 86; ++y) {
        for (int x = 0; x < 86; ++x) {
            const int outerX = x - 43;
            const int outerY = y - 43;
            const int innerX = x - 52;
            const int innerY = y - 44;
            color_t pixel = 0;
            if (outerX * outerX + outerY * outerY <= 40 * 40) {
                pixel = EGEARGB(176, 176, 55, 55);
            }
            if (innerX * innerX + innerY * innerY <= 27 * 27) {
                pixel = EGEARGB(176, 48, 128, 176);
            }
            alphaPixels[y * 86 + x] = pixel;
        }
    }
    putimage_withalpha(nullptr, alphaImage, 343, 211);
    delimage(alphaImage);

    PIMAGE keyedImage = newimage(80, 70);
    setbkcolor(MAGENTA, keyedImage);
    cleardevice(keyedImage);
    setfillstyle(SOLID_FILL, EGERGB(100, 225, 140), keyedImage);
    bar(8, 8, 71, 61, keyedImage);
    setcolor(EGERGB(20, 80, 55), keyedImage);
    circle(40, 35, 22, keyedImage);
    putimage_transparent(nullptr, keyedImage, 452, 218, MAGENTA);
    putimage_alphablend(nullptr, keyedImage, 535, 218, 160, COLORTYPE_RGB32);
    delimage(keyedImage);
}

void drawTransformPanel()
{
    drawPanel(15, 330, 625, 465, "Rotation, scaling, text, and pixel updates");

    PIMAGE sprite = newimage(72, 72);
    color_t pixels[72 * 72];
    for (int y = 0; y < 72; ++y) {
        for (int x = 0; x < 72; ++x) {
            const int dx = x - 36;
            const int dy = y - 36;
            if (dx * dx + dy * dy > 31 * 31) {
                pixels[y * 72 + x] = 0;
            } else {
                pixels[y * 72 + x] = EGERGB(50 + x * 2, 80 + y * 2, 230 - x);
            }
        }
    }
    updatebuffer(sprite, 0, 0, 72, 72, pixels);

    putimage_rotatezoom(nullptr, sprite, 95, 405, 0.5f, 0.5f,
                        kPi / 7.0f, 1.15f, true, -1, true);
    putimage_rotatezoom(nullptr, sprite, 205, 405, 0.5f, 0.5f,
                        -kPi / 5.0f, 0.75f, true, -1, true);
    delimage(sprite);

    setfont(22, 0, "Arial");
    setcolor(EGERGB(235, 238, 245));
    outtextxy(270, 362, "GDI / OpenGL");
    setfont(18, 0, "Consolas");
    setcolor(EGERGB(115, 220, 255));
    outtextxy(270, 397, "fixed frame 10");

    setcolor(EGERGB(255, 205, 85));
    setlinewidth(4.0f);
    ege_line(455.0f, 365.0f, 590.0f, 430.0f);
    setlinewidth(1.0f);
    setcolor(EGERGB(235, 110, 210));
    ege_ellipse(485.0f, 355.0f, 105.0f, 85.0f);
}

} // namespace

int main()
{
    setinitmode(INIT_RENDERMANUAL);
    initgraph(kWidth, kHeight);
    setbkcolor(EGERGB(12, 15, 22));
    cleardevice();

    // Run the full-target clear/viewport compatibility case before the other
    // panels so its legacy cleardevice behavior does not erase their output.
    drawViewportPanel();
    drawPrimitivePanel();
    drawFillPanel();
    drawAlphaPanel();
    drawTransformPanel();

    for (int frame = 1; frame <= 10; ++frame) {
        delay_fps(10);
    }

    PIMAGE screenshot = newimage(getwidth(), getheight());
    getimage(screenshot, 0, 0, getwidth(), getheight());
    ege_setalpha(0xFF, screenshot);
    saveimage(screenshot, "graph_backend_validation_frame10.png");
    delimage(screenshot);
    std::printf("Screenshot saved: graph_backend_validation_frame10.png\n");

    closegraph();
    return 0;
}
