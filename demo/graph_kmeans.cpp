/**
 * @file graph_kmeans.cpp
 * @author wysaid (this@xege.org)
 * @brief K-Means 聚类算法可视化演示
 * @version 0.1
 * @date 2025-11-27
 *
 * K-Means 是一种经典的无监督学习聚类算法，用于将数据集划分为 K 个不同的簇。
 * 本程序通过动画演示 K-Means 算法的迭代过程，包括：
 * 1. 初始化：随机选择 K 个点作为初始聚类中心
 * 2. 分配：将每个数据点分配到距离最近的聚类中心
 * 3. 更新：重新计算每个簇的中心点
 * 4. 迭代：重复分配和更新步骤直到收敛
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
#include <limits>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE        "K-Means 聚类算法演示"
#define TEXT_CONTROLS_TITLE      "按键说明："
#define TEXT_CONTROLS_START      "S/空格/回车 - 开始/继续迭代"
#define TEXT_CONTROLS_RESET      "R - 重置算法 (保留数据点)"
#define TEXT_CONTROLS_GENERATE   "G - 重新生成数据点"
#define TEXT_CONTROLS_ADD_K      "+/= - 增加簇数量 K"
#define TEXT_CONTROLS_SUB_K      "-/_ - 减少簇数量 K"
#define TEXT_CONTROLS_AUTO       "A - 自动演示模式"
#define TEXT_CONTROLS_SPEED      "↑/↓ - 调整动画速度"
#define TEXT_CONTROLS_EXIT       "ESC - 退出程序"
#define TEXT_CURRENT_K           "当前簇数量 K = %d"
#define TEXT_ITERATION           "迭代次数: %d"
#define TEXT_STATUS_READY        "状态: 准备就绪"
#define TEXT_STATUS_RUNNING      "状态: 迭代中..."
#define TEXT_STATUS_CONVERGED    "状态: 已收敛!"
#define TEXT_STATUS_AUTO         "状态: 自动演示中..."
#define TEXT_POINTS_COUNT        "数据点数量: %d"
#define TEXT_ANIMATION_SPEED     "动画速度: %d ms"
#define TEXT_FONT_NAME           "宋体"
#define TEXT_CLUSTER_INFO        "簇 %d: %d 个点"
#define TEXT_CENTROID_MOVED      "中心点移动距离: %.2f"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE        "K-Means Visualization"
#define TEXT_CONTROLS_TITLE      "Controls:"
#define TEXT_CONTROLS_START      "S/Space/Enter - Start/Continue Iteration"
#define TEXT_CONTROLS_RESET      "R - Reset Algorithm (Keep Data)"
#define TEXT_CONTROLS_GENERATE   "G - Generate New Data Points"
#define TEXT_CONTROLS_ADD_K      "+/= - Increase K"
#define TEXT_CONTROLS_SUB_K      "-/_ - Decrease K"
#define TEXT_CONTROLS_AUTO       "A - Auto Demo Mode"
#define TEXT_CONTROLS_SPEED      "Up/Down - Adjust Animation Speed"
#define TEXT_CONTROLS_EXIT       "ESC - Exit Program"
#define TEXT_CURRENT_K           "Current K = %d"
#define TEXT_ITERATION           "Iterations: %d"
#define TEXT_STATUS_READY        "Status: Ready"
#define TEXT_STATUS_RUNNING      "Status: Running..."
#define TEXT_STATUS_CONVERGED    "Status: Converged!"
#define TEXT_STATUS_AUTO         "Status: Auto Demo..."
#define TEXT_POINTS_COUNT        "Data Points: %d"
#define TEXT_ANIMATION_SPEED     "Animation Speed: %d ms"
#define TEXT_FONT_NAME           "Arial"
#define TEXT_CLUSTER_INFO        "Cluster %d: %d points"
#define TEXT_CENTROID_MOVED      "Centroid moved: %.2f"
#endif

// 窗口和可视化参数
const int WINDOW_WIDTH     = 1200;
const int WINDOW_HEIGHT    = 800;
const int PANEL_WIDTH      = 280;                            // 右侧控制面板宽度
const int CANVAS_WIDTH     = WINDOW_WIDTH - PANEL_WIDTH;     // 绘图区域宽度
const int CANVAS_HEIGHT    = WINDOW_HEIGHT;                  // 绘图区域高度
const int POINT_RADIUS     = 5;                              // 数据点半径
const int CENTROID_RADIUS  = 12;                             // 聚类中心半径
const int DEFAULT_K        = 5;                              // 默认簇数量
const int MIN_K            = 2;                              // 最小簇数量
const int MAX_K            = 10;                             // 最大簇数量
const int DEFAULT_POINTS   = 300;                            // 默认数据点数量
const int MIN_POINTS       = 50;                             // 最小数据点数量
const int MAX_POINTS       = 1000;                           // 最大数据点数量

// 预定义的簇颜色 (鲜艳的颜色便于区分)
const color_t CLUSTER_COLORS[] = {
    EGERGB(231, 76, 60),    // 红色
    EGERGB(46, 204, 113),   // 绿色
    EGERGB(52, 152, 219),   // 蓝色
    EGERGB(241, 196, 15),   // 黄色
    EGERGB(155, 89, 182),   // 紫色
    EGERGB(230, 126, 34),   // 橙色
    EGERGB(26, 188, 156),   // 青色
    EGERGB(236, 240, 241),  // 白色
    EGERGB(241, 148, 138),  // 粉色
    EGERGB(133, 193, 233),  // 浅蓝
};

/**
 * @struct Point2D
 * @brief 2D 数据点结构
 */
struct Point2D
{
    float x;           // x 坐标
    float y;           // y 坐标
    int   clusterId;   // 所属簇的 ID (-1 表示未分配)

    Point2D(float _x = 0, float _y = 0) : x(_x), y(_y), clusterId(-1) {}

    // 计算到另一个点的欧几里得距离
    float distanceTo(const Point2D& other) const
    {
        float dx = x - other.x;
        float dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

/**
 * @struct Centroid
 * @brief 聚类中心结构
 */
struct Centroid
{
    float x;           // x 坐标
    float y;           // y 坐标
    float prevX;       // 上一次迭代的 x 坐标 (用于绘制移动轨迹)
    float prevY;       // 上一次迭代的 y 坐标

    Centroid(float _x = 0, float _y = 0) : x(_x), y(_y), prevX(_x), prevY(_y) {}

    // 计算移动距离
    float movedDistance() const
    {
        float dx = x - prevX;
        float dy = y - prevY;
        return std::sqrt(dx * dx + dy * dy);
    }

    // 保存当前位置
    void savePosition()
    {
        prevX = x;
        prevY = y;
    }
};

/**
 * @enum AlgorithmState
 * @brief 算法状态枚举
 */
enum AlgorithmState
{
    STATE_READY,       // 准备就绪
    STATE_RUNNING,     // 运行中
    STATE_CONVERGED,   // 已收敛
    STATE_AUTO         // 自动演示模式
};

/**
 * @class KMeansVisualizer
 * @brief K-Means 算法可视化类
 */
class KMeansVisualizer
{
public:
    KMeansVisualizer()
        : m_k(DEFAULT_K)
        , m_numPoints(DEFAULT_POINTS)
        , m_iteration(0)
        , m_state(STATE_READY)
        , m_animationSpeed(300)
        , m_convergenceThreshold(0.5f)
        , m_autoMode(false)
    {
        m_rd.seed(static_cast<unsigned int>(std::time(nullptr)));
        generateDataPoints();
        initializeCentroids();
    }

    // 生成随机数据点 (使用高斯分布生成多个簇状分布)
    void generateDataPoints()
    {
        m_points.clear();

        // 生成几个随机簇中心
        int numClusters      = m_rd() % 4 + 3; // 3-6 个自然簇
        float clusterSpread  = 60.0f;          // 簇内点的分散程度

        std::vector<Point2D> clusterCenters;
        for (int i = 0; i < numClusters; ++i) {
            float cx = static_cast<float>(m_rd() % (CANVAS_WIDTH - 100) + 50);
            float cy = static_cast<float>(m_rd() % (CANVAS_HEIGHT - 100) + 50);
            clusterCenters.push_back(Point2D(cx, cy));
        }

        // 围绕每个簇中心生成点
        std::normal_distribution<float> normalDist(0.0f, clusterSpread);
        int pointsPerCluster = m_numPoints / numClusters;

        for (int i = 0; i < numClusters; ++i) {
            int count = (i == numClusters - 1) ? (m_numPoints - static_cast<int>(m_points.size())) : pointsPerCluster;
            for (int j = 0; j < count; ++j) {
                float x = clusterCenters[i].x + normalDist(m_rd);
                float y = clusterCenters[i].y + normalDist(m_rd);

                // 确保点在画布范围内
                x = std::max(10.0f, std::min(static_cast<float>(CANVAS_WIDTH - 10), x));
                y = std::max(10.0f, std::min(static_cast<float>(CANVAS_HEIGHT - 10), y));

                m_points.push_back(Point2D(x, y));
            }
        }

        // 添加一些随机噪声点
        int noiseCount = m_numPoints / 10;
        for (int i = 0; i < noiseCount; ++i) {
            float x = static_cast<float>(m_rd() % (CANVAS_WIDTH - 20) + 10);
            float y = static_cast<float>(m_rd() % (CANVAS_HEIGHT - 20) + 10);
            m_points.push_back(Point2D(x, y));
        }

        m_numPoints = static_cast<int>(m_points.size());
    }

    // 初始化聚类中心 (使用 K-Means++ 初始化策略)
    void initializeCentroids()
    {
        m_centroids.clear();
        m_iteration = 0;
        m_state     = STATE_READY;

        if (m_points.empty() || m_k <= 0) return;

        // K-Means++ 初始化
        // 1. 随机选择第一个中心点
        int firstIdx = m_rd() % m_points.size();
        m_centroids.push_back(Centroid(m_points[firstIdx].x, m_points[firstIdx].y));

        // 2. 依次选择其余中心点，选择概率与到最近中心点的距离平方成正比
        std::vector<float> distances(m_points.size());

        for (int c = 1; c < m_k; ++c) {
            float totalDist = 0.0f;

            // 计算每个点到最近中心点的距离
            for (size_t i = 0; i < m_points.size(); ++i) {
                float minDist = std::numeric_limits<float>::max();
                for (const auto& centroid : m_centroids) {
                    float dx = m_points[i].x - centroid.x;
                    float dy = m_points[i].y - centroid.y;
                    float dist = dx * dx + dy * dy;
                    minDist    = std::min(minDist, dist);
                }
                distances[i] = minDist;
                totalDist += minDist;
            }

            // 按概率选择下一个中心点
            float r          = static_cast<float>(m_rd()) / static_cast<float>(m_rd.max()) * totalDist;
            float cumulative = 0.0f;
            int   selectedIdx = 0;

            for (size_t i = 0; i < m_points.size(); ++i) {
                cumulative += distances[i];
                if (cumulative >= r) {
                    selectedIdx = static_cast<int>(i);
                    break;
                }
            }

            m_centroids.push_back(Centroid(m_points[selectedIdx].x, m_points[selectedIdx].y));
        }

        // 重置所有点的簇分配
        for (auto& point : m_points) {
            point.clusterId = -1;
        }
    }

    // 执行一次 K-Means 迭代
    bool iterate()
    {
        if (m_centroids.empty() || m_points.empty()) return true;

        // 保存当前中心点位置
        for (auto& centroid : m_centroids) {
            centroid.savePosition();
        }

        // 步骤1：分配每个点到最近的中心点
        for (auto& point : m_points) {
            float minDist   = std::numeric_limits<float>::max();
            int   bestCluster = 0;

            for (int k = 0; k < m_k; ++k) {
                float dx = point.x - m_centroids[k].x;
                float dy = point.y - m_centroids[k].y;
                float dist = dx * dx + dy * dy;

                if (dist < minDist) {
                    minDist     = dist;
                    bestCluster = k;
                }
            }
            point.clusterId = bestCluster;
        }

        // 步骤2：更新中心点位置
        std::vector<float> sumX(m_k, 0.0f);
        std::vector<float> sumY(m_k, 0.0f);
        std::vector<int>   counts(m_k, 0);

        for (const auto& point : m_points) {
            if (point.clusterId >= 0 && point.clusterId < m_k) {
                sumX[point.clusterId] += point.x;
                sumY[point.clusterId] += point.y;
                counts[point.clusterId]++;
            }
        }

        for (int k = 0; k < m_k; ++k) {
            if (counts[k] > 0) {
                m_centroids[k].x = sumX[k] / counts[k];
                m_centroids[k].y = sumY[k] / counts[k];
            }
        }

        ++m_iteration;

        // 检查是否收敛
        float maxMoved = 0.0f;
        for (const auto& centroid : m_centroids) {
            maxMoved = std::max(maxMoved, centroid.movedDistance());
        }

        return maxMoved < m_convergenceThreshold;
    }

    // 绘制所有内容
    void draw()
    {
        // 清空画布
        setbkcolor(EGERGB(30, 30, 40));
        cleardevice();

        ege_enable_aa(true);

        // 绘制数据点
        drawPoints();

        // 绘制中心点移动轨迹
        drawCentroidTrails();

        // 绘制聚类中心
        drawCentroids();

        // 绘制控制面板
        drawControlPanel();
    }

    // 绘制数据点
    void drawPoints()
    {
        for (const auto& point : m_points) {
            color_t color;
            if (point.clusterId >= 0 && point.clusterId < m_k) {
                color = CLUSTER_COLORS[point.clusterId % (sizeof(CLUSTER_COLORS) / sizeof(CLUSTER_COLORS[0]))];
            } else {
                color = EGERGB(128, 128, 128); // 未分配的点为灰色
            }

            setfillcolor(color);
            setcolor(EGEACOLOR(200, color));
            ege_fillellipse(point.x - POINT_RADIUS, point.y - POINT_RADIUS, POINT_RADIUS * 2, POINT_RADIUS * 2);
        }
    }

    // 绘制中心点移动轨迹
    void drawCentroidTrails()
    {
        setlinestyle(PS_DASH, 2);
        for (int k = 0; k < static_cast<int>(m_centroids.size()); ++k) {
            const auto& centroid = m_centroids[k];
            if (centroid.movedDistance() > 0.1f) {
                color_t color = CLUSTER_COLORS[k % (sizeof(CLUSTER_COLORS) / sizeof(CLUSTER_COLORS[0]))];
                setcolor(EGEACOLOR(150, color));
                line(static_cast<int>(centroid.prevX), static_cast<int>(centroid.prevY),
                     static_cast<int>(centroid.x), static_cast<int>(centroid.y));
            }
        }
        setlinestyle(PS_SOLID, 1);
    }

    // 绘制聚类中心
    void drawCentroids()
    {
        for (int k = 0; k < static_cast<int>(m_centroids.size()); ++k) {
            const auto& centroid = m_centroids[k];
            color_t color = CLUSTER_COLORS[k % (sizeof(CLUSTER_COLORS) / sizeof(CLUSTER_COLORS[0]))];

            // 绘制十字星标记
            setcolor(ege::WHITE);
            setlinestyle(PS_SOLID, 3);

            int cx = static_cast<int>(centroid.x);
            int cy = static_cast<int>(centroid.y);

            // 外圈
            setfillcolor(color);
            ege_fillellipse(cx - CENTROID_RADIUS, cy - CENTROID_RADIUS, CENTROID_RADIUS * 2, CENTROID_RADIUS * 2);

            // 内圈 (白色)
            setfillcolor(ege::WHITE);
            ege_fillellipse(cx - CENTROID_RADIUS / 2, cy - CENTROID_RADIUS / 2, CENTROID_RADIUS, CENTROID_RADIUS);

            // 绘制十字
            setcolor(color);
            setlinestyle(PS_SOLID, 2);
            line(cx - CENTROID_RADIUS - 5, cy, cx + CENTROID_RADIUS + 5, cy);
            line(cx, cy - CENTROID_RADIUS - 5, cx, cy + CENTROID_RADIUS + 5);
        }
    }

    // 绘制控制面板
    void drawControlPanel()
    {
        int panelX = CANVAS_WIDTH;

        // 面板背景
        setfillcolor(EGERGB(45, 45, 55));
        bar(panelX, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX, 0, panelX, WINDOW_HEIGHT);

        // 设置字体
        setfont(18, 0, TEXT_FONT_NAME);
        setcolor(ege::WHITE);

        int textX = panelX + 15;
        int textY = 20;
        int lineHeight = 28;

        // 标题
        setfont(22, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_WINDOW_TITLE);
        textY += lineHeight + 10;

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 状态信息
        setfont(16, 0, TEXT_FONT_NAME);
        setcolor(EGERGB(150, 200, 255));

        char buf[128];

        // 显示 K 值
        sprintf(buf, TEXT_CURRENT_K, m_k);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示迭代次数
        sprintf(buf, TEXT_ITERATION, m_iteration);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示数据点数量
        sprintf(buf, TEXT_POINTS_COUNT, m_numPoints);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示动画速度
        sprintf(buf, TEXT_ANIMATION_SPEED, m_animationSpeed);
        outtextxy(textX, textY, buf);
        textY += lineHeight + 5;

        // 显示状态
        setcolor(EGERGB(100, 255, 100));
        switch (m_state) {
            case STATE_READY:
                outtextxy(textX, textY, TEXT_STATUS_READY);
                break;
            case STATE_RUNNING:
                setcolor(EGERGB(255, 200, 100));
                outtextxy(textX, textY, TEXT_STATUS_RUNNING);
                break;
            case STATE_CONVERGED:
                setcolor(EGERGB(100, 255, 200));
                outtextxy(textX, textY, TEXT_STATUS_CONVERGED);
                break;
            case STATE_AUTO:
                setcolor(EGERGB(255, 150, 200));
                outtextxy(textX, textY, TEXT_STATUS_AUTO);
                break;
        }
        textY += lineHeight + 10;

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 显示各簇信息
        setfont(14, 0, TEXT_FONT_NAME);
        std::vector<int> clusterCounts(m_k, 0);
        for (const auto& point : m_points) {
            if (point.clusterId >= 0 && point.clusterId < m_k) {
                clusterCounts[point.clusterId]++;
            }
        }

        for (int k = 0; k < m_k; ++k) {
            color_t color = CLUSTER_COLORS[k % (sizeof(CLUSTER_COLORS) / sizeof(CLUSTER_COLORS[0]))];
            setfillcolor(color);
            bar(textX, textY + 2, textX + 12, textY + 14);
            setcolor(ege::WHITE);
            sprintf(buf, TEXT_CLUSTER_INFO, k + 1, clusterCounts[k]);
            outtextxy(textX + 18, textY, buf);
            textY += 22;
        }

        textY += 10;

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 控制说明
        setcolor(EGERGB(200, 200, 200));
        setfont(14, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_CONTROLS_TITLE);
        textY += lineHeight;

        setcolor(EGERGB(180, 180, 180));
        setfont(12, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_CONTROLS_START);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_RESET);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_GENERATE);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_ADD_K);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_SUB_K);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_AUTO);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_SPEED);
        textY += 20;
        outtextxy(textX, textY, TEXT_CONTROLS_EXIT);
    }

    // 处理键盘输入
    void handleInput()
    {
        while (kbhit()) {
            int key = getch();

            switch (key) {
                case 'S':
                case 's':
                case ' ':
                case '\r':
                case '\n':
                    // 执行一次迭代或开始
                    if (m_state != STATE_CONVERGED) {
                        m_state = STATE_RUNNING;
                        if (iterate()) {
                            m_state = STATE_CONVERGED;
                        }
                    }
                    break;

                case 'R':
                case 'r':
                    // 重置算法
                    initializeCentroids();
                    m_autoMode = false;
                    break;

                case 'G':
                case 'g':
                    // 重新生成数据点
                    generateDataPoints();
                    initializeCentroids();
                    m_autoMode = false;
                    break;

                case 'A':
                case 'a':
                    // 切换自动模式
                    m_autoMode = !m_autoMode;
                    if (m_autoMode) {
                        m_state = STATE_AUTO;
                    } else {
                        if (m_state == STATE_AUTO) {
                            m_state = STATE_READY;
                        }
                    }
                    break;

                case '+':
                case '=':
                    // 增加 K
                    if (m_k < MAX_K) {
                        ++m_k;
                        initializeCentroids();
                    }
                    break;

                case '-':
                case '_':
                    // 减少 K
                    if (m_k > MIN_K) {
                        --m_k;
                        initializeCentroids();
                    }
                    break;

                case key_up:
                    // 加快动画速度
                    m_animationSpeed = std::max(50, m_animationSpeed - 50);
                    break;

                case key_down:
                    // 减慢动画速度
                    m_animationSpeed = std::min(1000, m_animationSpeed + 50);
                    break;

                case key_esc:
                    // 退出
                    closegraph();
                    exit(0);
                    break;
            }
        }
    }

    // 自动演示模式更新
    void autoUpdate()
    {
        if (m_autoMode && m_state != STATE_CONVERGED) {
            m_state = STATE_AUTO;
            if (iterate()) {
                m_state = STATE_CONVERGED;
                m_autoMode = false;
            }
        }
    }

    // 获取动画速度
    int getAnimationSpeed() const { return m_animationSpeed; }

    // 是否在自动模式
    bool isAutoMode() const { return m_autoMode; }

private:
    std::vector<Point2D>  m_points;              // 数据点
    std::vector<Centroid> m_centroids;           // 聚类中心
    int                   m_k;                   // 簇数量
    int                   m_numPoints;           // 数据点数量
    int                   m_iteration;           // 迭代次数
    AlgorithmState        m_state;               // 算法状态
    int                   m_animationSpeed;      // 动画速度 (毫秒)
    float                 m_convergenceThreshold;// 收敛阈值
    bool                  m_autoMode;            // 自动演示模式
    std::mt19937          m_rd;                  // 随机数生成器
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

    // 创建可视化器
    KMeansVisualizer visualizer;

    // 主循环
    for (; is_run(); delay_fps(60)) {
        // 处理输入
        visualizer.handleInput();

        // 自动模式更新
        if (visualizer.isAutoMode()) {
            static int frameCount = 0;
            int framesPerStep = visualizer.getAnimationSpeed() / 16; // 约 60fps
            if (++frameCount >= framesPerStep) {
                visualizer.autoUpdate();
                frameCount = 0;
            }
        }

        // 绘制
        visualizer.draw();
    }

    closegraph();
    return 0;
}
