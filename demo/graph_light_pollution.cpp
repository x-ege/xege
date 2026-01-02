/**
 * @file graph_light_pollution.cpp
 * @brief 光污染效果演示 - 展示眼花缭乱的炫目效果
 * 
 * 这个演示程序展示"光污染"效果，通过快速变化的颜色、闪烁的图形、
 * 重叠的半透明元素、频闪效果等方式，创造出令人眼花缭乱的视觉效果。
 * 参考KTV蹦迪灯光和鬼畜视频风格。
 * 
 * 警告：本程序包含强烈闪烁效果，可能引起不适，请谨慎观看！
 * 按任意键退出
 */

#include <graphics.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_CIRCLES 60      // 圆形数量（翻倍）
#define MAX_RECTS 40        // 矩形数量（翻倍）
#define MAX_STARS 100       // 星星闪烁效果数量（翻倍）
#define MAX_LINES 80        // 线条数量（翻倍）
#define MAX_PARTICLES 200   // 高速粒子数量
#define MAX_FLASHES 10      // 随机闪光数量
#define MAX_SPIRAL_CIRCLES 30 // 螺旋圆圈数量

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
    float hue;            // 色相（预计算）
};

// 彩色线条结构
struct ColorLine {
    int x1, y1, x2, y2;   // 起止点
    float hue;            // 色相
    float hue_speed;      // 色相变化速度
    int alpha;            // 透明度
    int width;            // 线宽
};

// 高速粒子结构
struct Particle {
    float x, y;           // 位置
    float vx, vy;         // 速度（更快）
    float hue;            // 色相
    int size;             // 大小
    float life;           // 生命值
};

// 随机闪光结构
struct Flash {
    int x, y;             // 位置
    float intensity;      // 强度
    float radius;         // 半径
    float phase;          // 相位
    float hue;            // 色相
};

// 螺旋圆圈结构
struct SpiralCircle {
    float angle;          // 角度
    float radius;         // 半径
    float hue;            // 色相
    float speed;          // 旋转速度
};

FlashCircle circles[MAX_CIRCLES];
FlashRect rects[MAX_RECTS];
FlashStar stars[MAX_STARS];
ColorLine lines[MAX_LINES];
Particle particles[MAX_PARTICLES];
Flash flashes[MAX_FLASHES];
SpiralCircle spiral_circles[MAX_SPIRAL_CIRCLES];

int screen_w, screen_h;
float strobe_phase = 0.0f;  // 频闪相位

// 初始化圆形
void InitCircles() {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        circles[i].x = (float)(random(screen_w));
        circles[i].y = (float)(random(screen_h));
        circles[i].radius = (float)(random(40) + 15);  // 更大范围
        circles[i].vx = (float)(random(400) - 200) / 30.0f;  // 速度翻倍
        circles[i].vy = (float)(random(400) - 200) / 30.0f;
        circles[i].pulse = (float)(random(628)) / 100.0f;
        circles[i].pulse_speed = (float)(random(150) + 50) / 100.0f;  // 更快脉动
        circles[i].hue = (float)(random(360));
        circles[i].hue_speed = (float)(random(300) + 100) / 10.0f;  // 更快变色
        circles[i].alpha = random(150) + 100;  // 更高透明度
    }
}

// 初始化矩形
void InitRects() {
    for (int i = 0; i < MAX_RECTS; i++) {
        rects[i].x = (float)(random(screen_w));
        rects[i].y = (float)(random(screen_h));
        rects[i].w = (float)(random(100) + 30);
        rects[i].h = (float)(random(100) + 30);
        rects[i].vx = (float)(random(400) - 200) / 40.0f;  // 更快速度
        rects[i].vy = (float)(random(400) - 200) / 40.0f;
        rects[i].rotation = (float)(random(360));
        rects[i].rot_speed = (float)(random(300) - 150) / 20.0f;  // 更快旋转
        rects[i].hue = (float)(random(360));
        rects[i].alpha = random(120) + 100;
    }
}

// 初始化星星
void InitStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(screen_w);
        stars[i].y = random(screen_h);
        stars[i].brightness = 0.0f;
        stars[i].blink_speed = (float)(random(100) + 40) / 100.0f;  // 更快闪烁
        stars[i].phase = (float)(random(628)) / 100.0f;
        stars[i].size = random(4) + 2;  // 更大的星星
        stars[i].hue = (float)(random(360));
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
        lines[i].hue_speed = (float)(random(200) + 60) / 10.0f;  // 更快变色
        lines[i].alpha = random(180) + 100;  // 更高透明度
        lines[i].width = random(5) + 2;  // 更宽的线条
    }
}

// 初始化高速粒子
void InitParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x = (float)(random(screen_w));
        particles[i].y = (float)(random(screen_h));
        particles[i].vx = (float)(random(1000) - 500) / 20.0f;  // 超快速度
        particles[i].vy = (float)(random(1000) - 500) / 20.0f;
        particles[i].hue = (float)(random(360));
        particles[i].size = random(4) + 2;
        particles[i].life = 1.0f;
    }
}

// 初始化随机闪光
void InitFlashes() {
    for (int i = 0; i < MAX_FLASHES; i++) {
        flashes[i].x = random(screen_w);
        flashes[i].y = random(screen_h);
        flashes[i].intensity = 0.0f;
        flashes[i].radius = (float)(random(100) + 50);
        flashes[i].phase = (float)(random(628)) / 100.0f;
        flashes[i].hue = (float)(random(360));
    }
}

// 初始化螺旋圆圈
void InitSpiralCircles() {
    for (int i = 0; i < MAX_SPIRAL_CIRCLES; i++) {
        spiral_circles[i].angle = (float)(i * 360.0f / MAX_SPIRAL_CIRCLES);
        spiral_circles[i].radius = (float)((i + 1) * 15);
        spiral_circles[i].hue = (float)(i * 360.0f / MAX_SPIRAL_CIRCLES);
        spiral_circles[i].speed = (float)(random(100) + 50) / 10.0f;
    }
}

// 更新圆形
void UpdateCircles(float dt) {
    for (int i = 0; i < MAX_CIRCLES; i++) {
        // 更新位置（速度已在初始化时增加）
        circles[i].x += circles[i].vx * dt * 120.0f;
        circles[i].y += circles[i].vy * dt * 120.0f;
        
        // 边界反弹（考虑脉动因子的最大半径）
        float max_radius = circles[i].radius * 1.3f;
        if (circles[i].x - max_radius < 0 || circles[i].x + max_radius > screen_w) {
            circles[i].vx = -circles[i].vx;
            circles[i].x = (circles[i].x < screen_w / 2) ? max_radius : (screen_w - max_radius);
        }
        if (circles[i].y - max_radius < 0 || circles[i].y + max_radius > screen_h) {
            circles[i].vy = -circles[i].vy;
            circles[i].y = (circles[i].y < screen_h / 2) ? max_radius : (screen_h - max_radius);
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
        // 更新位置（速度已增加）
        rects[i].x += rects[i].vx * dt * 120.0f;
        rects[i].y += rects[i].vy * dt * 120.0f;
        
        // 边界反弹（考虑矩形宽高）
        float half_w = rects[i].w / 2.0f;
        if (rects[i].x - half_w < 0 || rects[i].x + half_w > screen_w) {
            rects[i].vx = -rects[i].vx;
            rects[i].x = (rects[i].x < screen_w / 2) ? half_w : (screen_w - half_w);
        }
        float half_h = rects[i].h / 2.0f;
        if (rects[i].y - half_h < 0 || rects[i].y + half_h > screen_h) {
            rects[i].vy = -rects[i].vy;
            rects[i].y = (rects[i].y < screen_h / 2) ? half_h : (screen_h - half_h);
        }
        
        // 更新旋转（更快）
        rects[i].rotation += rects[i].rot_speed * dt * 120.0f;
        if (rects[i].rotation > 360.0f) rects[i].rotation -= 360.0f;
        if (rects[i].rotation < 0.0f) rects[i].rotation += 360.0f;
        
        // 更新色相（与旋转速度关联，更快）
        rects[i].hue += rects[i].rot_speed * 60.0f * dt;
        if (rects[i].hue >= 360.0f) rects[i].hue -= 360.0f;
        if (rects[i].hue < 0.0f) rects[i].hue += 360.0f;
    }
}

// 更新星星
void UpdateStars(float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].phase += stars[i].blink_speed * dt * 3.0f;  // 更快闪烁
        stars[i].brightness = (float)((sin(stars[i].phase * 5.0) + 1.0) * 0.5);  // 更快频率
        
        // 更频繁改变色相
        if (random(1000) < 20) {
            stars[i].hue = (float)(random(360));
        }
    }
}

// 更新线条
void UpdateLines(float dt) {
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i].hue += lines[i].hue_speed * dt * 2.0f;  // 更快变色
        if (lines[i].hue >= 360.0f) lines[i].hue -= 360.0f;
        
        // 更频繁重新生成线条位置和线宽
        if (random(1000) < 30) {
            lines[i].x1 = random(screen_w);
            lines[i].y1 = random(screen_h);
            lines[i].x2 = random(screen_w);
            lines[i].y2 = random(screen_h);
            lines[i].width = random(5) + 2;
        }
    }
}

// 更新高速粒子
void UpdateParticles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x += particles[i].vx * dt * 200.0f;
        particles[i].y += particles[i].vy * dt * 200.0f;
        
        particles[i].life -= dt * 0.5f;
        
        // 重生粒子
        if (particles[i].life <= 0.0f || 
            particles[i].x < 0 || particles[i].x > screen_w ||
            particles[i].y < 0 || particles[i].y > screen_h) {
            particles[i].x = (float)(random(screen_w));
            particles[i].y = (float)(random(screen_h));
            particles[i].vx = (float)(random(1000) - 500) / 20.0f;
            particles[i].vy = (float)(random(1000) - 500) / 20.0f;
            particles[i].hue = (float)(random(360));
            particles[i].life = 1.0f;
        }
    }
}

// 更新随机闪光
void UpdateFlashes(float dt) {
    for (int i = 0; i < MAX_FLASHES; i++) {
        flashes[i].phase += dt * 20.0f;
        flashes[i].intensity = (float)(fabs(sin(flashes[i].phase)) * sin(flashes[i].phase * 3.0));
        
        // 随机改变位置和色相
        if (random(1000) < 30) {
            flashes[i].x = random(screen_w);
            flashes[i].y = random(screen_h);
            flashes[i].hue = (float)(random(360));
            flashes[i].radius = (float)(random(150) + 50);
        }
    }
}

// 更新螺旋圆圈
void UpdateSpiralCircles(float dt) {
    for (int i = 0; i < MAX_SPIRAL_CIRCLES; i++) {
        spiral_circles[i].angle += spiral_circles[i].speed * dt * 100.0f;
        if (spiral_circles[i].angle >= 360.0f) spiral_circles[i].angle -= 360.0f;
        spiral_circles[i].hue += dt * 200.0f;
        if (spiral_circles[i].hue >= 360.0f) spiral_circles[i].hue -= 360.0f;
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
        color_t color = HSVtoRGB(rects[i].hue, 1.0f, 1.0f);
        
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
        // 根据亮度和预计算的色相生成颜色
        color_t color = HSVtoRGB(stars[i].hue, 1.0f, stars[i].brightness);
        
        setcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
        setfillcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
        
        // 绘制星星（小圆形或十字）
        if (stars[i].brightness > 0.3f) {  // 降低阈值，更频繁显示
            int size = (int)(stars[i].size * (stars[i].brightness + 0.5f));
            ege_fillellipse(stars[i].x - size / 2, 
                           stars[i].y - size / 2,
                           size, size);
            // 添加十字闪光效果（更大）
            int cross_size = stars[i].size * 4;
            line(stars[i].x - cross_size, stars[i].y, 
                 stars[i].x + cross_size, stars[i].y);
            line(stars[i].x, stars[i].y - cross_size, 
                 stars[i].x, stars[i].y + cross_size);
        }
    }
}

// 绘制线条
void DrawLines() {
    for (int i = 0; i < MAX_LINES; i++) {
        color_t color = HSVtoRGB(lines[i].hue, 1.0f, 1.0f);
        setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), lines[i].alpha));
        
        setlinewidth(lines[i].width);
        line(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2);
    }
}

// 绘制高速粒子
void DrawParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0.0f) {
            color_t color = HSVtoRGB(particles[i].hue, 1.0f, particles[i].life);
            setcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
            setfillcolor(EGERGB(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color)));
            
            int size = (int)(particles[i].size * particles[i].life);
            ege_fillellipse((int)particles[i].x - size/2, (int)particles[i].y - size/2, size, size);
            
            // 添加拖尾效果
            int alpha = (int)(particles[i].life * 150);
            setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), alpha));
            line((int)particles[i].x, (int)particles[i].y, 
                 (int)(particles[i].x - particles[i].vx * 0.3f), 
                 (int)(particles[i].y - particles[i].vy * 0.3f));
        }
    }
}

// 绘制随机闪光
void DrawFlashes() {
    for (int i = 0; i < MAX_FLASHES; i++) {
        if (flashes[i].intensity > 0.1f) {
            color_t color = HSVtoRGB(flashes[i].hue, 1.0f, 1.0f);
            int alpha = (int)(flashes[i].intensity * 200);
            
            // 绘制径向渐变光晕
            for (int r = (int)flashes[i].radius; r > 0; r -= 10) {
                int a = (int)(alpha * r / flashes[i].radius);
                setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), a));
                circle(flashes[i].x, flashes[i].y, r);
            }
            
            // 中心亮点
            setfillcolor(EGERGBA(255, 255, 255, alpha));
            ege_fillellipse(flashes[i].x - 5, flashes[i].y - 5, 10, 10);
        }
    }
}

// 绘制螺旋圆圈
void DrawSpiralCircles() {
    int center_x = screen_w / 2;
    int center_y = screen_h / 2;
    
    for (int i = 0; i < MAX_SPIRAL_CIRCLES; i++) {
        float rad = spiral_circles[i].angle * 3.14159f / 180.0f;
        int x = (int)(center_x + cos(rad) * spiral_circles[i].radius);
        int y = (int)(center_y + sin(rad) * spiral_circles[i].radius);
        
        color_t color = HSVtoRGB(spiral_circles[i].hue, 1.0f, 1.0f);
        setcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), 180));
        setfillcolor(EGERGBA(EGEGET_R(color), EGEGET_G(color), EGEGET_B(color), 120));
        
        int size = 15 + (int)(sin(spiral_circles[i].angle * 0.1f) * 5);
        ege_fillellipse(x - size/2, y - size/2, size, size);
    }
}

// 绘制频闪背景（增强版）
void DrawFlashingBackground(float time) {
    // 频闪效果（黑白快速交替）
    strobe_phase += 0.3f;
    int strobe = (int)strobe_phase % 10;
    
    if (strobe < 2) {  // 20%的时间全白
        setbkcolor(WHITE);
        cleardevice();
        return;
    }
    
    // 使用正弦波产生闪烁效果（增强）
    float flash = (float)((sin(time * 8.0) + 1.0) * 0.5);  // 更快频率
    int bg_brightness = (int)(flash * 60) + 10;  // 更高亮度
    
    // 随机改变背景色相（更快）
    float bg_hue = (float)((int)(time * 150) % 360);
    color_t bg_color = HSVtoRGB(bg_hue, 0.5f, (float)bg_brightness / 255.0f);  // 更高饱和度
    
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
    InitParticles();
    InitFlashes();
    InitSpiralCircles();
    
    // 设置字体
    setfont(18, 0, "SimHei");  // 更大更粗的字体
    setcolor(WHITE);
    
    int fps = 60;
    float time = 0.0f;
    float dt = 1.0f / fps;
    
    // 启用抗锯齿
    ege_enable_aa(true);
    
    // 显示警告信息
    bool show_hint = true;
    float hint_time = 4.0f;
    
    // 主循环
    for (; is_run() && !kbhit(); delay_fps(fps)) {
        time += dt;
        
        // 绘制频闪背景
        DrawFlashingBackground(time);
        
        // 更新所有元素
        UpdateCircles(dt);
        UpdateRects(dt);
        UpdateStars(dt);
        UpdateLines(dt);
        UpdateParticles(dt);
        UpdateFlashes(dt);
        UpdateSpiralCircles(dt);
        
        // 绘制所有元素（顺序很重要）
        DrawSpiralCircles();  // 先绘制螺旋
        DrawFlashes();        // 闪光效果
        DrawLines();
        DrawParticles();      // 高速粒子
        DrawRects();
        DrawCircles();
        DrawStars();
        
        // 显示警告信息（前4秒）
        if (show_hint && time < hint_time) {
            float alpha = 1.0f;
            if (time > hint_time - 1.0f) {
                alpha = hint_time - time;
            }
            int text_alpha = (int)(alpha * 255);
            
            // 黑色背景框
            setfillcolor(EGERGBA(0, 0, 0, text_alpha - 55));
            ege_fillrect(5, 5, 500, 90);
            
            // 红色警告文字
            setcolor(EGERGBA(255, 50, 50, text_alpha));
            outtextxy(10, 10, "[!] WARNING: Intense Flashing!");
            
            setcolor(EGERGBA(255, 255, 255, text_alpha));
            outtextxy(10, 35, "光污染效果演示 - 按任意键退出");
            
            char fps_str[64];
            snprintf(fps_str, sizeof(fps_str), "FPS: %.1f", getfps());
            outtextxy(10, 60, fps_str);
        } else {
            show_hint = false;
        }
    }
    
    // 清除键盘缓冲
    while (kbhit()) getch();
    
    closegraph();
    return 0;
}
