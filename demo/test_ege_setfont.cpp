/*
 * Test demo for ege_setfont function
 * This demo demonstrates the use of floating-point font sizes with GDI+
 */

#include <graphics.h>
#include <stdio.h>

int main()
{
    // Initialize graphics window
    initgraph(800, 600);
    setbkcolor(WHITE);
    cleardevice();

    // Test 1: Basic ege_setfont with floating-point size
    settextcolor(BLACK);
    ege_setfont(24.5f, L"Arial");
    ege_drawtext(L"24.5pt Arial font (floating-point size)", 50.0f, 50.0f);

    // Test 2: Different sizes to show precision
    ege_setfont(12.0f, L"Times New Roman");
    ege_drawtext(L"12.0pt Times New Roman", 50.0f, 100.0f);

    ege_setfont(12.5f, L"Times New Roman");
    ege_drawtext(L"12.5pt Times New Roman", 50.0f, 120.0f);

    ege_setfont(13.0f, L"Times New Roman");
    ege_drawtext(L"13.0pt Times New Roman", 50.0f, 140.0f);

    // Test 3: With font styles
    ege_setfont(20.0f, L"Arial", Gdiplus::FontStyleBold);
    ege_drawtext(L"20pt Arial Bold", 50.0f, 200.0f);

    ege_setfont(18.0f, L"Arial", Gdiplus::FontStyleItalic);
    ege_drawtext(L"18pt Arial Italic", 50.0f, 240.0f);

    ege_setfont(16.0f, L"Arial", Gdiplus::FontStyleBold | Gdiplus::FontStyleItalic);
    ege_drawtext(L"16pt Arial Bold Italic", 50.0f, 280.0f);

    ege_setfont(14.0f, L"Arial", Gdiplus::FontStyleUnderline);
    ege_drawtext(L"14pt Arial Underline", 50.0f, 320.0f);

    // Test 4: Very precise sizes
    ege_setfont(15.25f, L"Courier New");
    ege_drawtext(L"15.25pt Courier New (very precise)", 50.0f, 380.0f);

    // Test 5: Chinese characters with floating-point size
    ege_setfont(22.5f, L"SimSun");
    ege_drawtext(L"22.5点宋体 - 中文测试", 50.0f, 430.0f);

    // Test 6: Compare with measuretext
    float width, height;
    const wchar_t* testText = L"Test measuretext with GDI+ Font";
    ege_setfont(18.0f, L"Arial");
    measuretext(testText, &width, &height);
    
    wchar_t info[256];
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
    swprintf_s(info, 256, L"Width: %.2f, Height: %.2f", width, height);
#else
    swprintf(info, 256, L"Width: %.2f, Height: %.2f", width, height);
#endif
    ege_drawtext(testText, 50.0f, 480.0f);
    ege_setfont(12.0f, L"Arial");
    ege_drawtext(info, 50.0f, 510.0f);

    // Instructions
    ege_setfont(14.0f, L"Arial");
    settextcolor(BLUE);
    ege_drawtext(L"Press any key to exit...", 50.0f, 550.0f);

    // Wait for key press
    getch();
    closegraph();
    return 0;
}
