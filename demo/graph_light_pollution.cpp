/**
 * @file graph_light_pollution.cpp
 * @brief 光污染效果演示 - 展示眼花缭乱的炫目效果
 * 
 * 这个演示程序展示"光污染"效果，通过快速变化的颜色、闪烁的图形、
 * 重叠的半透明元素等方式，创造出令人眼花缭乱的视觉效果。
 * 
 * 按任意键退出
 */

#include <graphics.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_CIRCLES 30      // 圆形数量
#define MAX_RECTS 20        // 矩形数量
#define MAX_STARS 50        // 星星闪烁效果数量
#define MAX_LINES 40        // 线条数量

// 闪烁圆形结构
struct FlashCircle {
    float x, y;           // 位置
    float radius;         // 半径
    float vx, vy;         // 速度
    float pulse;          // 脉动相位
    float pulse_speed;    // 脉动速度
    float hue;            // 色相
    float hue_speed;      // 色相变化速度
    int alpha;            // 透明度
};

// 闪烁矩形结构
struct FlashRect {
    float x, y;           // 位置
    float w, h;           // 宽高
    float vx, vy;         // 速度
    float rotation;       // 旋转角度
    float rot_speed;      // 旋转速度
    float hue;            // 色相
    int alpha;            // 透明度
};

// 闪烁星星结构
struct FlashStar {
    int x, y;             // 位置
    float brightness;     // 亮度
    float blink_speed;    // 闪烁速度
    float phase;          // 相位
    int size;             // 大小
};

// 彩色线条结构
struct ColorLine {
    int x1, y1, x2, y2;   // 起止点
    float hue;            // 色相
    float hue_speed;      // 色相变化速度
    int alpha;            // 透明度
};

FlashCircle circles[MAX_CIRCLES];
FlashRect rects[MAX_RECTS];
FlashStar stars[MAX_STARS];
ColorLine lines[MAX_LINES];

int screen_w, screen_h;

// 初始化圆形
void InitCircles() {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        circles[i].x = (float)(random(screen_w));
        circles[i].y = (float)(random(screen_h));
        circles[i].radius = (float)(random(30) + 20);
        circles[i].vx = (float)(random(200) - 100) / 50.0f;
        circles[i].vy = (float)(random(200) - 100) / 50.0f;
        circles[i].pulse = (float)(random(628)) / 100.0f;
        circles[i].pulse_speed = (float)(random(50) + 10) / 100.0f;
        circles[i].hue = (float)(random(360));
        circles[i].hue_speed = (float)(random(100) + 50) / 10.0f;
        circles[i].alpha = random(100) + 100;
    }
}

// 初始化矩形
void InitRects() {
    for (int i = 0; i < MAX_RECTS; i++) {
        rects[i].x = (float)(random(screen_w));
        rects[i].y = (float)(random(screen_h));
        rects[i].w = (float)(random(80) + 40);
        rects[i].h = (float)(random(80) + 40);
        rects[i].vx = (float)(random(200) - 100) / 60.0f;
        rects[i].vy = (float)(random(200) - 100) / 60.0f;
        rects[i].rotation = (float)(random(360));
        rects[i].rot_speed = (float)(random(100) - 50) / 50.0f;
        rects[i].hue = (float)(random(360));
        rects[i].alpha = random(80) + 80;
    }
}

// 初始化星星
void InitStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(screen_w);
        stars[i].y = random(screen_h);
        stars[i].brightness = 0.0f;
        stars[i].blink_speed = (float)(random(50) + 20) / 100.0f;
        stars[i].phase = (float)(random(628)) / 100.0f;
        stars[i].size = random(3) + 2;
    }
}

// 初始化线条
void InitLines() {
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i].x1 = random(screen_w);
        lines[i].y1 = random(screen_h);
        lines[i].x2 = random(screen_w);
        lines[i].y2 = random(screen_h);
        lines[i].hue = (float)(random(360));
        lines[i].hue_speed = (float)(random(100) + 30) / 10.0f;
        lines[i].alpha = random(150) + 80;
    }
}

// 更新圆形
void UpdateCircles(float dt) {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        // 更新位置
        circles[i].x += circles[i].vx * dt * 60.0f;
        circles[i].y += circles[i].vy * dt * 60.0f;
        
        // 边界反弹
        if (circles[i].x < 0 || circles[i].x > screen_w) {
            circles[i].vx = -circles[i].vx;
            circles[i].x = circles[i].x < 0 ? 0.0f : (float)screen_w;
        }
        if (circles[i].y < 0 || circles[i].y > screen_h) {
            circles[i].vy = -circles[i].vy;
            circles[i].y = circles[i].y < 0 ? 0.0f : (float)screen_h;
        }
        
        // 更新脉动和色相
        circles[i].pulse += circles[i].pulse_speed * dt;
        circles[i].hue += circles[i].hue_speed * dt;
        if (circles[i].hue >= 360.0f) circles[i].hue -= 360.0f;
    }
}

// 更新矩形
void UpdateRects(float dt) {
    for (int i = 0; i < MAX_RECTS; i++) {
        // 更新位置
        rects[i].x += rects[i].vx * dt * 60.0f;
        rects[i].y += rects[i].vy * dt * 60.0f;
        
        // 边界反弹
        if (rects[i].x < 0 || rects[i].x > screen_w) {
            rects[i].vx = -rects[i].vx;
            rects[i].x = rects[i].x < 0 ? 0.0f : (float)screen_w;
        }
        if (rects[i].y < 0 || rects[i].y > screen_h) {
            rects[i].vy = -rects[i].vy;
            rects[i].y = rects[i].y < 0 ? 0.0f : (float)screen_h;
        }
        
        // 更新旋转
        rects[i].rotation += rects[i].rot_speed * dt * 60.0f;
        if (rects[i].rotation > 360.0f) rects[i].rotation -= 360.0f;
        if (rects[i].rotation < 0.0f) rects[i].rotation += 360.0f;
    }
}

// 更新星星
void UpdateStars(float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].phase += stars[i].blink_speed * dt;
        stars[i].brightness = (float)((sin(stars[i].phase) + 1.0) * 0.5);
    }
}

// 更新线条
void UpdateLines(float dt) {
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i].hue += lines[i].hue_speed * dt;
        if (lines[i].hue >= 360.0f) lines[i].hue -= 360.0f;
        
        // 偶尔重新生成线条位置
        if (random(1000) < 10) {
            lines[i].x1 = random(screen_w);
            lines[i].y1 = random(screen_h);
            lines[i].x2 = random(screen_w);
            lines[i].y2 = random(screen_h);
        }
    }
}

// 绘制圆形
void DrawCircles() {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        // 计算脉动半径
        float pulse_factor = (float)(sin(circles[i].pulse) * 0.3 + 1.0);
        float current_radius = circles[i].radius * pulse_factor;
        
        // 转换HSV到RGB
        color_t color = HSVtoRGB(circles[i].hue, 1.0f, 1.0f);
        
        // 设置透明色
        setfillcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), circles[i].alpha));
        setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), circles[i].alpha + 50));
        
        // 绘制圆形
        ege_fillellipse((int)(circles[i].x - current_radius), 
                       (int)(circles[i].y - current_radius),
                       (int)(current_radius * 2), 
                       (int)(current_radius * 2));
    }
}

// 绘制矩形
void DrawRects() {
    for (int i = 0; i < MAX_RECTS; i++) {
        // 转换HSV到RGB，使用高饱和度
        color_t color = HSVtoRGB((float)((int)(rects[i].rotation * 2) % 360), 1.0f, 1.0f);
        
        // 设置透明色
        setfillcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), rects[i].alpha));
        
        // 绘制旋转矩形（简化版，使用填充矩形）
        ege_fillrect((int)(rects[i].x - rects[i].w / 2), 
                    (int)(rects[i].y - rects[i].h / 2),
                    (int)(rects[i].w), 
                    (int)(rects[i].h));
    }
}

// 绘制星星
void DrawStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        // 根据亮度计算颜色
        int brightness = (int)(stars[i].brightness * 255);
        color_t color = HSVtoRGB((float)(random(360)), 1.0f, stars[i].brightness);
        
        setcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
        setfillcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
        
        // 绘制星星（小圆形或十字）
        if (stars[i].brightness > 0.5f) {
            ege_fillellipse(stars[i].x - stars[i].size / 2, 
                           stars[i].y - stars[i].size / 2,
                           stars[i].size, stars[i].size);
            // 添加十字闪光效果
            line(stars[i].x - stars[i].size * 2, stars[i].y, 
                 stars[i].x + stars[i].size * 2, stars[i].y);
            line(stars[i].x, stars[i].y - stars[i].size * 2, 
                 stars[i].x, stars[i].y + stars[i].size * 2);
        }
    }
}

// 绘制线条
void DrawLines() {
    for (int i = 0; i < MAX_LINES; i++) {
        color_t color = HSVtoRGB(lines[i].hue, 1.0f, 1.0f);
        setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), lines[i].alpha));
        
        setlinewidth(random(3) + 1);
        line(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2);
    }
}

// 绘制闪烁背景
void DrawFlashingBackground(float time) {
    // 使用正弦波产生闪烁效果
    float flash = (float)((sin(time * 3.0) + 1.0) * 0.5);
    int bg_brightness = (int)(flash * 30);
    
    // 随机改变背景色相
    float bg_hue = (float)((int)(time * 50) % 360);
    color_t bg_color = HSVtoRGB(bg_hue, 0.3f, (float)bg_brightness / 255.0f);
    
    setbkcolor(bg_color);
    cleardevice();
}

int main() {
    // 初始化图形窗口
    setinitmode(INIT_ANIMATION);
    initgraph(800, 600);
    randomize();
    
    screen_w = getwidth();
    screen_h = getheight();
    
    // 初始化所有元素
    InitCircles();
    InitRects();
    InitStars();
    InitLines();
    
    // 设置字体
    setfont(16, 0, "宋体");
    setcolor(WHITE);
    
    int fps = 60;
    float time = 0.0f;
    float dt = 1.0f / fps;
    
    // 启用抗锯齿
    ege_enable_aa(true);
    
    // 显示提示信息
    bool show_hint = true;
    float hint_time = 3.0f;
    
    // 主循环
    for (; is_run() && !kbhit(); delay_fps(fps)) {
        time += dt;
        
        // 绘制闪烁背景
        DrawFlashingBackground(time);
        
        // 更新所有元素
        UpdateCircles(dt);
        UpdateRects(dt);
        UpdateStars(dt);
        UpdateLines(dt);
        
        // 绘制所有元素
        DrawLines();
        DrawRects();
        DrawCircles();
        DrawStars();
        
        // 显示提示信息（前3秒）
        if (show_hint && time < hint_time) {
            float alpha = 1.0f;
            if (time > hint_time - 1.0f) {
                alpha = hint_time - time;
            }
            int text_alpha = (int)(alpha * 200);
            
            setcolor(EGERGBA(255, 255, 255, text_alpha));
            outtextxy(10, 10, "光污染效果演示 - 按任意键退出");
            
            char fps_str[64];
            sprintf(fps_str, "FPS: %.1f", getfps());
            outtextxy(10, 30, fps_str);
        } else {
            show_hint = false;
        }
    }
    
    // 清除键盘缓冲
    while (kbhit()) getch();
    
    closegraph();
    return 0;
}
