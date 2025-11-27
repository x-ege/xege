/**
 * @file graph_boids.cpp
 * @author wysaid (this@xege.org)
 * @brief Boids 群集模拟可视化演示
 * @version 0.1
 * @date 2025-11-27
 *
 * Boids 是 Craig Reynolds 在 1986 年提出的模拟群体行为的算法。
 * 通过三条简单规则产生复杂的涌现行为：
 * 1. 分离 (Separation) - 避免与邻近个体碰撞
 * 2. 对齐 (Alignment) - 与邻近个体保持相同方向
 * 3. 凝聚 (Cohesion) - 向邻近个体的中心靠拢
 *
 * 本程序通过动画演示 Boids 算法的群集行为，可用于模拟鸟群、鱼群等。
 */

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <graphics.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <random>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE        "Boids 群集模拟"
#define TEXT_CONTROLS_TITLE      "按键说明："
#define TEXT_CONTROLS_ADD        "+/= - 增加 Boid 数量"
#define TEXT_CONTROLS_SUB        "-/_ - 减少 Boid 数量"
#define TEXT_CONTROLS_RESET      "R - 重置所有 Boids"
#define TEXT_CONTROLS_PREDATOR   "P - 切换捕食者模式"
#define TEXT_CONTROLS_TRAIL      "T - 切换轨迹显示"
#define TEXT_CONTROLS_SPEED      "↑/↓ - 调整速度"
#define TEXT_CONTROLS_EXIT       "ESC - 退出程序"
#define TEXT_MOUSE_HINT          "鼠标操作："
#define TEXT_MOUSE_LEFT          "左键 - 吸引 Boids"
#define TEXT_MOUSE_RIGHT         "右键 - 驱散 Boids"
#define TEXT_BOID_COUNT          "Boid 数量: %d"
#define TEXT_SPEED_MULT          "速度倍率: %.1fx"
#define TEXT_PREDATOR_ON         "捕食者模式: 开启"
#define TEXT_PREDATOR_OFF        "捕食者模式: 关闭"
#define TEXT_TRAIL_ON            "轨迹显示: 开启"
#define TEXT_TRAIL_OFF           "轨迹显示: 关闭"
#define TEXT_RULES_TITLE         "规则权重："
#define TEXT_RULE_SEPARATION     "分离: %.2f (1/2调整)"
#define TEXT_RULE_ALIGNMENT      "对齐: %.2f (3/4调整)"
#define TEXT_RULE_COHESION       "凝聚: %.2f (5/6调整)"
#define TEXT_FONT_NAME           "宋体"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE        "Boids Flocking Simulation"
#define TEXT_CONTROLS_TITLE      "Controls:"
#define TEXT_CONTROLS_ADD        "+/= - Add Boids"
#define TEXT_CONTROLS_SUB        "-/_ - Remove Boids"
#define TEXT_CONTROLS_RESET      "R - Reset All Boids"
#define TEXT_CONTROLS_PREDATOR   "P - Toggle Predator Mode"
#define TEXT_CONTROLS_TRAIL      "T - Toggle Trail"
#define TEXT_CONTROLS_SPEED      "Up/Down - Adjust Speed"
#define TEXT_CONTROLS_EXIT       "ESC - Exit"
#define TEXT_MOUSE_HINT          "Mouse:"
#define TEXT_MOUSE_LEFT          "Left - Attract Boids"
#define TEXT_MOUSE_RIGHT         "Right - Repel Boids"
#define TEXT_BOID_COUNT          "Boid Count: %d"
#define TEXT_SPEED_MULT          "Speed: %.1fx"
#define TEXT_PREDATOR_ON         "Predator Mode: ON"
#define TEXT_PREDATOR_OFF        "Predator Mode: OFF"
#define TEXT_TRAIL_ON            "Trail: ON"
#define TEXT_TRAIL_OFF           "Trail: OFF"
#define TEXT_RULES_TITLE         "Rule Weights:"
#define TEXT_RULE_SEPARATION     "Separation: %.2f (1/2)"
#define TEXT_RULE_ALIGNMENT      "Alignment: %.2f (3/4)"
#define TEXT_RULE_COHESION       "Cohesion: %.2f (5/6)"
#define TEXT_FONT_NAME           "Arial"
#endif

// 窗口参数
const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;
const int PANEL_WIDTH   = 220;
const int CANVAS_WIDTH  = WINDOW_WIDTH - PANEL_WIDTH;
const int CANVAS_HEIGHT = WINDOW_HEIGHT;

// Boid 参数
const int   DEFAULT_BOID_COUNT = 150;
const int   MIN_BOID_COUNT     = 10;
const int   MAX_BOID_COUNT     = 500;
const float BOID_SIZE          = 8.0f;
const float MAX_SPEED          = 4.0f;
const float MAX_FORCE          = 0.15f;
const float PERCEPTION_RADIUS  = 50.0f;
const float SEPARATION_RADIUS  = 25.0f;

// 颜色定义
const ege::color_t BOIDS_COLOR_BG     = EGERGB(20, 25, 35);
const ege::color_t BOIDS_COLOR_PANEL  = EGERGB(35, 40, 50);
const ege::color_t BOIDS_COLOR_BOID   = EGERGB(100, 200, 255);
const ege::color_t BOIDS_COLOR_PRED   = EGERGB(255, 80, 80);
const ege::color_t BOIDS_COLOR_TRAIL  = EGERGB(60, 100, 140);

/**
 * @struct Vec2
 * @brief 2D 向量结构
 */
struct Vec2
{
    float x, y;

    Vec2(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
    Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }

    Vec2& operator+=(const Vec2& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2& operator-=(const Vec2& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    Vec2& operator*=(float s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    float magnitude() const { return std::sqrt(x * x + y * y); }

    float magnitudeSq() const { return x * x + y * y; }

    Vec2 normalized() const
    {
        float m = magnitude();
        if (m > 0) return Vec2(x / m, y / m);
        return Vec2(0, 0);
    }

    void limit(float max)
    {
        float m = magnitude();
        if (m > max) {
            x = x / m * max;
            y = y / m * max;
        }
    }

    void setMagnitude(float mag)
    {
        float m = magnitude();
        if (m > 0) {
            x = x / m * mag;
            y = y / m * mag;
        }
    }

    static float distance(const Vec2& a, const Vec2& b)
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static float distanceSq(const Vec2& a, const Vec2& b)
    {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }
};

/**
 * @struct Boid
 * @brief Boid 个体结构
 */
struct Boid
{
    Vec2  position;
    Vec2  velocity;
    Vec2  acceleration;
    float hue;        // 颜色色相 (用于个体差异)
    std::vector<Vec2> trail; // 轨迹历史

    Boid(float x, float y)
    {
        position = Vec2(x, y);
        // 随机初始速度
        float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float speed = static_cast<float>(rand()) / RAND_MAX * 2.0f + 1.0f;
        velocity    = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);
        acceleration = Vec2(0, 0);
        hue = static_cast<float>(rand()) / RAND_MAX * 60.0f + 180.0f; // 180-240 蓝绿色调
    }

    // 应用力
    void applyForce(const Vec2& force) { acceleration += force; }

    // 更新位置和速度
    void update(float speedMult)
    {
        velocity += acceleration * speedMult;
        velocity.limit(MAX_SPEED * speedMult);
        position += velocity;
        acceleration = Vec2(0, 0);

        // 更新轨迹
        trail.push_back(position);
        if (trail.size() > 20) {
            trail.erase(trail.begin());
        }
    }

    // 边界环绕
    void wrapEdges(int width, int height)
    {
        if (position.x < 0) position.x = static_cast<float>(width);
        if (position.x > width) position.x = 0;
        if (position.y < 0) position.y = static_cast<float>(height);
        if (position.y > height) position.y = 0;
    }
};

/**
 * @class BoidsSimulation
 * @brief Boids 群集模拟类
 */
class BoidsSimulation
{
public:
    BoidsSimulation()
        : m_separationWeight(1.5f)
        , m_alignmentWeight(1.0f)
        , m_cohesionWeight(1.0f)
        , m_speedMultiplier(1.0f)
        , m_predatorMode(false)
        , m_showTrail(false)
        , m_mouseAttract(false)
        , m_mouseRepel(false)
        , m_mouseX(0)
        , m_mouseY(0)
    {
        srand(static_cast<unsigned>(time(nullptr)));
        initBoids(DEFAULT_BOID_COUNT);
    }

    // 初始化 Boids
    void initBoids(int count)
    {
        m_boids.clear();
        for (int i = 0; i < count; ++i) {
            float x = static_cast<float>(rand() % CANVAS_WIDTH);
            float y = static_cast<float>(rand() % CANVAS_HEIGHT);
            m_boids.push_back(Boid(x, y));
        }
    }

    // 添加 Boids
    void addBoids(int count)
    {
        for (int i = 0; i < count && static_cast<int>(m_boids.size()) < MAX_BOID_COUNT; ++i) {
            float x = static_cast<float>(rand() % CANVAS_WIDTH);
            float y = static_cast<float>(rand() % CANVAS_HEIGHT);
            m_boids.push_back(Boid(x, y));
        }
    }

    // 移除 Boids
    void removeBoids(int count)
    {
        for (int i = 0; i < count && static_cast<int>(m_boids.size()) > MIN_BOID_COUNT; ++i) {
            m_boids.pop_back();
        }
    }

    // 分离行为 - 避免拥挤
    Vec2 separation(const Boid& boid)
    {
        Vec2  steering(0, 0);
        int   count = 0;
        float radiusSq = SEPARATION_RADIUS * SEPARATION_RADIUS;

        for (const auto& other : m_boids) {
            float d = Vec2::distanceSq(boid.position, other.position);
            if (d > 0 && d < radiusSq) {
                Vec2 diff = boid.position - other.position;
                diff = diff.normalized();
                diff = diff / std::sqrt(d); // 距离越近，斥力越大
                steering += diff;
                ++count;
            }
        }

        if (count > 0) {
            steering = steering / static_cast<float>(count);
            if (steering.magnitude() > 0) {
                steering.setMagnitude(MAX_SPEED);
                steering = steering - boid.velocity;
                steering.limit(MAX_FORCE);
            }
        }

        return steering;
    }

    // 对齐行为 - 与邻居保持相同方向
    Vec2 alignment(const Boid& boid)
    {
        Vec2  steering(0, 0);
        int   count = 0;
        float radiusSq = PERCEPTION_RADIUS * PERCEPTION_RADIUS;

        for (const auto& other : m_boids) {
            float d = Vec2::distanceSq(boid.position, other.position);
            if (d > 0 && d < radiusSq) {
                steering += other.velocity;
                ++count;
            }
        }

        if (count > 0) {
            steering = steering / static_cast<float>(count);
            steering.setMagnitude(MAX_SPEED);
            steering = steering - boid.velocity;
            steering.limit(MAX_FORCE);
        }

        return steering;
    }

    // 凝聚行为 - 向邻居中心靠拢
    Vec2 cohesion(const Boid& boid)
    {
        Vec2  steering(0, 0);
        int   count = 0;
        float radiusSq = PERCEPTION_RADIUS * PERCEPTION_RADIUS;

        for (const auto& other : m_boids) {
            float d = Vec2::distanceSq(boid.position, other.position);
            if (d > 0 && d < radiusSq) {
                steering += other.position;
                ++count;
            }
        }

        if (count > 0) {
            steering = steering / static_cast<float>(count);
            steering = steering - boid.position;
            steering.setMagnitude(MAX_SPEED);
            steering = steering - boid.velocity;
            steering.limit(MAX_FORCE);
        }

        return steering;
    }

    // 鼠标交互力
    Vec2 mouseForce(const Boid& boid)
    {
        Vec2 steering(0, 0);

        if (!m_mouseAttract && !m_mouseRepel) return steering;

        Vec2  mousePos(static_cast<float>(m_mouseX), static_cast<float>(m_mouseY));
        float d = Vec2::distance(boid.position, mousePos);

        if (d < 150.0f) {
            Vec2 diff = mousePos - boid.position;
            diff = diff.normalized();

            if (m_mouseRepel) {
                diff = diff * -1.0f; // 反向
            }

            float strength = (150.0f - d) / 150.0f; // 距离越近，力越大
            diff = diff * (MAX_FORCE * 3.0f * strength);
            steering = diff;
        }

        return steering;
    }

    // 更新所有 Boids
    void update()
    {
        for (auto& boid : m_boids) {
            Vec2 sep   = separation(boid) * m_separationWeight;
            Vec2 ali   = alignment(boid) * m_alignmentWeight;
            Vec2 coh   = cohesion(boid) * m_cohesionWeight;
            Vec2 mouse = mouseForce(boid);

            boid.applyForce(sep);
            boid.applyForce(ali);
            boid.applyForce(coh);
            boid.applyForce(mouse);

            boid.update(m_speedMultiplier);
            boid.wrapEdges(CANVAS_WIDTH, CANVAS_HEIGHT);
        }
    }

    // 绘制所有内容
    void draw()
    {
        setbkcolor(BOIDS_COLOR_BG);
        cleardevice();

        ege_enable_aa(true);

        // 绘制轨迹
        if (m_showTrail) {
            drawTrails();
        }

        // 绘制 Boids
        drawBoids();

        // 绘制鼠标影响范围
        if (m_mouseAttract || m_mouseRepel) {
            drawMouseInfluence();
        }

        // 绘制控制面板
        drawControlPanel();
    }

    // 绘制轨迹
    void drawTrails()
    {
        for (const auto& boid : m_boids) {
            if (boid.trail.size() < 2) continue;

            for (size_t i = 1; i < boid.trail.size(); ++i) {
                float alpha = static_cast<float>(i) / static_cast<float>(boid.trail.size());
                ege::color_t color = EGEACOLOR(static_cast<int>(alpha * 100), BOIDS_COLOR_TRAIL);
                setcolor(color);
                setlinestyle(PS_SOLID, 1);
                line(static_cast<int>(boid.trail[i - 1].x), static_cast<int>(boid.trail[i - 1].y),
                     static_cast<int>(boid.trail[i].x), static_cast<int>(boid.trail[i].y));
            }
        }
    }

    // 绘制 Boids
    void drawBoids()
    {
        for (const auto& boid : m_boids) {
            // 计算朝向角度
            float angle = std::atan2(boid.velocity.y, boid.velocity.x);

            // 三角形三个顶点
            float size = BOID_SIZE;
            float x1   = boid.position.x + std::cos(angle) * size * 1.5f;
            float y1   = boid.position.y + std::sin(angle) * size * 1.5f;
            float x2   = boid.position.x + std::cos(angle + 2.5f) * size;
            float y2   = boid.position.y + std::sin(angle + 2.5f) * size;
            float x3   = boid.position.x + std::cos(angle - 2.5f) * size;
            float y3   = boid.position.y + std::sin(angle - 2.5f) * size;

            // 根据色相生成颜色
            ege::color_t color = HSVtoRGB(boid.hue, 0.8f, 1.0f);
            if (m_predatorMode) {
                color = BOIDS_COLOR_PRED;
            }

            setfillcolor(color);
            setcolor(EGEACOLOR(200, color));

            // 绘制三角形
            ege_point points[3];
            points[0].x = x1;
            points[0].y = y1;
            points[1].x = x2;
            points[1].y = y2;
            points[2].x = x3;
            points[2].y = y3;

            ege_fillpoly(3, points);
        }
    }

    // 绘制鼠标影响范围
    void drawMouseInfluence()
    {
        ege::color_t color = m_mouseAttract ? EGERGB(100, 200, 100) : EGERGB(200, 100, 100);
        setcolor(EGEACOLOR(100, color));
        setlinestyle(PS_DOT, 2);
        circle(m_mouseX, m_mouseY, 150);
        setlinestyle(PS_SOLID, 1);
    }

    // 绘制控制面板
    void drawControlPanel()
    {
        int panelX = CANVAS_WIDTH;

        // 面板背景
        setfillcolor(BOIDS_COLOR_PANEL);
        bar(panelX, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

        // 分隔线
        setcolor(EGERGB(60, 65, 75));
        line(panelX, 0, panelX, WINDOW_HEIGHT);

        setfont(16, 0, TEXT_FONT_NAME);
        setcolor(ege::WHITE);
        settextjustify(LEFT_TEXT, TOP_TEXT);

        int textX      = panelX + 15;
        int textY      = 20;
        int lineHeight = 24;

        // 标题
        setfont(18, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_WINDOW_TITLE);
        textY += lineHeight + 10;

        // 分隔线
        setcolor(EGERGB(60, 65, 75));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 状态信息
        setfont(14, 0, TEXT_FONT_NAME);
        setcolor(EGERGB(150, 200, 255));

        char buf[128];

        // Boid 数量
        sprintf(buf, TEXT_BOID_COUNT, static_cast<int>(m_boids.size()));
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 速度倍率
        sprintf(buf, TEXT_SPEED_MULT, m_speedMultiplier);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 捕食者模式
        if (m_predatorMode) {
            setcolor(EGERGB(255, 150, 150));
            outtextxy(textX, textY, TEXT_PREDATOR_ON);
        } else {
            setcolor(EGERGB(150, 150, 150));
            outtextxy(textX, textY, TEXT_PREDATOR_OFF);
        }
        textY += lineHeight;

        // 轨迹显示
        if (m_showTrail) {
            setcolor(EGERGB(150, 255, 150));
            outtextxy(textX, textY, TEXT_TRAIL_ON);
        } else {
            setcolor(EGERGB(150, 150, 150));
            outtextxy(textX, textY, TEXT_TRAIL_OFF);
        }
        textY += lineHeight + 10;

        // 分隔线
        setcolor(EGERGB(60, 65, 75));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 规则权重
        setcolor(EGERGB(200, 200, 200));
        outtextxy(textX, textY, TEXT_RULES_TITLE);
        textY += lineHeight;

        setcolor(EGERGB(255, 200, 150));
        sprintf(buf, TEXT_RULE_SEPARATION, m_separationWeight);
        outtextxy(textX, textY, buf);
        textY += 20;

        setcolor(EGERGB(150, 255, 200));
        sprintf(buf, TEXT_RULE_ALIGNMENT, m_alignmentWeight);
        outtextxy(textX, textY, buf);
        textY += 20;

        setcolor(EGERGB(150, 200, 255));
        sprintf(buf, TEXT_RULE_COHESION, m_cohesionWeight);
        outtextxy(textX, textY, buf);
        textY += 25;

        // 分隔线
        setcolor(EGERGB(60, 65, 75));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 控制说明
        setcolor(EGERGB(200, 200, 200));
        outtextxy(textX, textY, TEXT_CONTROLS_TITLE);
        textY += lineHeight;

        setcolor(EGERGB(180, 180, 180));
        setfont(11, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_CONTROLS_ADD);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_SUB);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_RESET);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_PREDATOR);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_TRAIL);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_SPEED);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_EXIT);
        textY += 25;

        // 鼠标操作说明
        setcolor(EGERGB(200, 200, 200));
        setfont(14, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_MOUSE_HINT);
        textY += lineHeight;

        setcolor(EGERGB(180, 180, 180));
        setfont(11, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_MOUSE_LEFT);
        textY += 18;
        outtextxy(textX, textY, TEXT_MOUSE_RIGHT);
    }

    // 处理键盘输入
    void handleInput()
    {
        while (kbhit()) {
            int key = getch();

            switch (key) {
                case '+':
                case '=':
                    addBoids(10);
                    break;

                case '-':
                case '_':
                    removeBoids(10);
                    break;

                case 'R':
                case 'r':
                    initBoids(DEFAULT_BOID_COUNT);
                    break;

                case 'P':
                case 'p':
                    m_predatorMode = !m_predatorMode;
                    if (m_predatorMode) {
                        m_separationWeight = 3.0f;
                        m_alignmentWeight  = 0.5f;
                        m_cohesionWeight   = 0.3f;
                    } else {
                        m_separationWeight = 1.5f;
                        m_alignmentWeight  = 1.0f;
                        m_cohesionWeight   = 1.0f;
                    }
                    break;

                case 'T':
                case 't':
                    m_showTrail = !m_showTrail;
                    // 清除轨迹
                    if (!m_showTrail) {
                        for (auto& boid : m_boids) {
                            boid.trail.clear();
                        }
                    }
                    break;

                case key_up:
                    m_speedMultiplier = std::min(3.0f, m_speedMultiplier + 0.1f);
                    break;

                case key_down:
                    m_speedMultiplier = std::max(0.3f, m_speedMultiplier - 0.1f);
                    break;

                // 规则权重调整
                case '1':
                    m_separationWeight = std::max(0.0f, m_separationWeight - 0.1f);
                    break;
                case '2':
                    m_separationWeight = std::min(5.0f, m_separationWeight + 0.1f);
                    break;
                case '3':
                    m_alignmentWeight = std::max(0.0f, m_alignmentWeight - 0.1f);
                    break;
                case '4':
                    m_alignmentWeight = std::min(5.0f, m_alignmentWeight + 0.1f);
                    break;
                case '5':
                    m_cohesionWeight = std::max(0.0f, m_cohesionWeight - 0.1f);
                    break;
                case '6':
                    m_cohesionWeight = std::min(5.0f, m_cohesionWeight + 0.1f);
                    break;

                case key_esc:
                    closegraph();
                    exit(0);
                    break;
            }
        }
    }

    // 处理鼠标输入
    void handleMouse()
    {
        while (mousemsg()) {
            mouse_msg msg = getmouse();

            m_mouseX = msg.x;
            m_mouseY = msg.y;

            if (msg.is_left()) {
                m_mouseAttract = msg.is_down() || (msg.is_move() && m_mouseAttract);
                if (msg.is_up()) m_mouseAttract = false;
            }

            if (msg.is_right()) {
                m_mouseRepel = msg.is_down() || (msg.is_move() && m_mouseRepel);
                if (msg.is_up()) m_mouseRepel = false;
            }
        }
    }

private:
    std::vector<Boid> m_boids;

    float m_separationWeight;
    float m_alignmentWeight;
    float m_cohesionWeight;
    float m_speedMultiplier;

    bool m_predatorMode;
    bool m_showTrail;
    bool m_mouseAttract;
    bool m_mouseRepel;
    int  m_mouseX, m_mouseY;
};

/**
 * @brief 程序入口
 */
int main()
{
    // 初始化图形窗口
    setinitmode(INIT_ANIMATION);
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
    setcaption(TEXT_WINDOW_TITLE);
    setbkmode(TRANSPARENT);

    // 创建模拟器
    BoidsSimulation simulation;

    // 主循环
    for (; is_run(); delay_fps(60)) {
        // 处理输入
        simulation.handleInput();
        simulation.handleMouse();

        // 更新
        simulation.update();

        // 绘制
        simulation.draw();
    }

    closegraph();
    return 0;
}
