/**
 * @file graph_function_visualization.cpp
 * @author wysaid (this@xege.org)
 * @brief 基于蒙特卡洛法的 2D 函数图像绘制
 * @version 0.1
 * @date 2025-07-12
 *
 */

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <graphics.h>
#include <cmath>
#include <functional>
#include <random>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE             "EGE - 2D 函数图像绘制器 (蒙特卡洛法)"
#define TEXT_CONTROLS_TITLE           "控制说明:"
#define TEXT_CONTROLS_SPACE           "空格 - 切换到下一个函数"
#define TEXT_CONTROLS_SAMPLES         "+/= - 增加采样点数量 (+10000)"
#define TEXT_CONTROLS_SAMPLES_DOWN    "-/_ - 减少采样点数量 (-10000)"
#define TEXT_CONTROLS_TOLERANCE_UP    "W - 增加容差值 (+0.01) - 使函数线条更粗"
#define TEXT_CONTROLS_TOLERANCE_DOWN  "S - 减少容差值 (-0.01) - 使函数线条更细"
#define TEXT_CONTROLS_POINT_SIZE_DOWN "[ - 减少点大小"
#define TEXT_CONTROLS_POINT_SIZE_UP   "] - 增大点大小"
#define TEXT_CONTROLS_REDRAW          "R - 重新绘制当前函数"
#define TEXT_CONTROLS_EXIT            "ESC - 退出"
#define TEXT_CURRENT_FUNCTION         "当前函数: %s"
#define TEXT_COORD_RANGE              "坐标范围: X:[%.2f, %.2f]  Y:[%.2f, %.2f]"
#define TEXT_PARAMS_INFO              "采样点数: %d  容差: %.4f"
#define TEXT_POINT_SIZE_INFO          "点大小: %d"
#define TEXT_FUNCTION_CIRCLE          "圆形 (r=2)"
#define TEXT_FUNCTION_ELLIPSE         "椭圆 (a=3, b=2)"
#define TEXT_FUNCTION_PARABOLA        "抛物线 (y=x^2)"
#define TEXT_FUNCTION_HYPERBOLA       "双曲线"
#define TEXT_FUNCTION_SINE            "正弦波"
#define TEXT_FUNCTION_ROSE            "玫瑰花 (n=3)"
#define TEXT_FUNCTION_HEART           "心形"
#define TEXT_FUNCTION_LOTUS           "莲花"
#define TEXT_FONT_NAME                "宋体"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE             "EGE - 2D Function Graph Renderer (Monte Carlo Method)"
#define TEXT_CONTROLS_TITLE           "Controls:"
#define TEXT_CONTROLS_SPACE           "SPACE - Next function"
#define TEXT_CONTROLS_SAMPLES         "+/= - Increase samples (+10000)"
#define TEXT_CONTROLS_SAMPLES_DOWN    "-/_ - Decrease samples (-10000)"
#define TEXT_CONTROLS_TOLERANCE_UP    "W - Increase tolerance (+0.01)"
#define TEXT_CONTROLS_TOLERANCE_DOWN  "S - Decrease tolerance (-0.01)"
#define TEXT_CONTROLS_POINT_SIZE_DOWN "[ - Decrease point size"
#define TEXT_CONTROLS_POINT_SIZE_UP   "] - Increase point size"
#define TEXT_CONTROLS_REDRAW          "R - Redraw current function"
#define TEXT_CONTROLS_EXIT            "ESC - Exit"
#define TEXT_CURRENT_FUNCTION         "Current: %s"
#define TEXT_COORD_RANGE              "X: [%.2f, %.2f]  Y: [%.2f, %.2f]"
#define TEXT_PARAMS_INFO              "Samples: %d  Tolerance: %.4f"
#define TEXT_POINT_SIZE_INFO          "Point Size: %d"
#define TEXT_FUNCTION_CIRCLE          "Circle (r=2)"
#define TEXT_FUNCTION_ELLIPSE         "Ellipse (a=3, b=2)"
#define TEXT_FUNCTION_PARABOLA        "Parabola (y=x^2)"
#define TEXT_FUNCTION_HYPERBOLA       "Hyperbola"
#define TEXT_FUNCTION_SINE            "Sine Wave"
#define TEXT_FUNCTION_ROSE            "Rose (n=3)"
#define TEXT_FUNCTION_HEART           "Heart"
#define TEXT_FUNCTION_LOTUS           "Lotus"
#define TEXT_FONT_NAME                "Arial"
#endif

/**
 * @class Function2DRenderer
 * @brief 基于蒙特卡洛法的2D函数图像绘制类
 *
 * 该类可以绘制形如 f(x,y) = 0 的函数图像，通过蒙特卡洛方法
 * 在指定区域内随机采样点，计算函数值，当函数值接近0时绘制该点
 */
class Function2DRenderer
{
public:
    /**
     * @brief 函数类型定义
     * 函数应该返回 f(x,y) 的值，当返回值为0时表示该点在函数图像上
     */
    using FunctionType = std::function<double(double, double)>;

    /**
     * @brief 构造函数
     * @param width 绘图区域宽度（像素）
     * @param height 绘图区域高度（像素）
     * @param xMin 数学坐标系X轴最小值
     * @param xMax 数学坐标系X轴最大值
     * @param yMin 数学坐标系Y轴最小值
     * @param yMax 数学坐标系Y轴最大值
     */
    Function2DRenderer(int width, int height, double xMin, double xMax, double yMin, double yMax) :
        m_width(width),
        m_height(height),
        m_xMin(xMin),
        m_xMax(xMax),
        m_yMin(yMin),
        m_yMax(yMax),
        m_tolerance(0.01),
        m_sampleCount(100000),
        m_pointColor(RED),
        m_backgroundColor(BLACK),
        m_drawAxes(true),
        m_axisColor(WHITE),
        m_gridColor(DARKGRAY),
        m_showGrid(true),
        m_generator(std::random_device{}()),
        m_xDistribution(m_xMin, m_xMax),
        m_yDistribution(m_yMin, m_yMax)
    {}

    /**
     * @brief 设置函数容差
     * @param tolerance 容差值，当 |f(x,y)| < tolerance 时认为点在函数图像上
     */
    void setTolerance(double tolerance) { m_tolerance = std::abs(tolerance); }

    double tolerance() const { return m_tolerance; }

    /**
     * @brief 设置采样点数量
     * @param count 蒙特卡洛采样的点数量
     */
    void setSampleCount(int count) { m_sampleCount = std::max(1000, count); }

    int sampleCount() const { return m_sampleCount; }

    /**
     * @brief 设置绘图颜色
     * @param pointColor 函数图像点的颜色
     * @param backgroundColor 背景颜色
     */
    void setColors(color_t pointColor, color_t backgroundColor)
    {
        m_pointColor      = pointColor;
        m_backgroundColor = backgroundColor;
    }

    /**
     * @brief 设置坐标轴颜色
     * @param axisColor 坐标轴颜色
     * @param gridColor 网格颜色
     */
    void setAxisColors(color_t axisColor, color_t gridColor)
    {
        m_axisColor = axisColor;
        m_gridColor = gridColor;
    }

    /**
     * @brief 设置是否绘制坐标轴
     * @param drawAxes 是否绘制坐标轴
     */
    void setDrawAxes(bool drawAxes) { m_drawAxes = drawAxes; }

    /**
     * @brief 设置是否显示网格
     * @param showGrid 是否显示网格
     */
    void setShowGrid(bool showGrid) { m_showGrid = showGrid; }

    /**
     * @brief 设置点大小
     * @param size 点的大小（像素）
     */
    void setPointSize(int size) { m_pointSize = std::max(1, std::min(20, size)); }

    int pointSize() const { return m_pointSize; }

    /**
     * @brief 绘制函数图像
     * @param func 要绘制的函数
     * @param pimg 目标图像，NULL表示绘制到当前窗口
     */
    void render(const FunctionType& func, PIMAGE pimg = NULL)
    {
        if (!func) {
            return;
        }

        // 绘制网格
        if (m_showGrid) {
            drawGrid(pimg);
        }

        // 绘制坐标轴
        if (m_drawAxes) {
            drawAxes(pimg);
        }

        // 蒙特卡洛采样绘制函数图像
        drawFunction(func, pimg);

        // 绘制标签
        drawLabels(pimg);
    }

    /**
     * @brief 添加预定义函数示例
     * @param name 函数名称
     * @param func 函数对象
     */
    void addFunction(const std::string& name, const FunctionType& func) { m_functions[name] = func; }

    /**
     * @brief 获取预定义函数列表
     * @return 函数名称列表
     */
    std::vector<std::string> getFunctionNames() const
    {
        std::vector<std::string> names;
        for (const auto& pair : m_functions) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * @brief 绘制预定义函数
     * @param name 函数名称
     * @param pimg 目标图像
     */
    void renderFunction(const std::string& name, PIMAGE pimg)
    {
        auto it = m_functions.find(name);
        if (it != m_functions.end()) {
            render(it->second, pimg);
        }
    }

private:
    /**
     * @brief 将数学坐标转换为屏幕坐标
     * @param x 数学坐标x
     * @param y 数学坐标y
     * @param screenX 输出屏幕坐标x
     * @param screenY 输出屏幕坐标y
     */
    void mathToScreen(double x, double y, int& screenX, int& screenY) const
    {
        screenX = static_cast<int>((x - m_xMin) / (m_xMax - m_xMin) * m_width);
        screenY = static_cast<int>((m_yMax - y) / (m_yMax - m_yMin) * m_height);
    }

    /**
     * @brief 绘制坐标轴
     * @param pimg 目标图像
     */
    void drawAxes(PIMAGE pimg)
    {
        setcolor(m_axisColor, pimg);
        setlinewidth(2, pimg);

        // 绘制X轴
        if (m_yMin <= 0 && m_yMax >= 0) {
            int screenX1, screenY1, screenX2, screenY2;
            mathToScreen(m_xMin, 0, screenX1, screenY1);
            mathToScreen(m_xMax, 0, screenX2, screenY2);
            line(screenX1, screenY1, screenX2, screenY2, pimg);
        }

        // 绘制Y轴
        if (m_xMin <= 0 && m_xMax >= 0) {
            int screenX1, screenY1, screenX2, screenY2;
            mathToScreen(0, m_yMin, screenX1, screenY1);
            mathToScreen(0, m_yMax, screenX2, screenY2);
            line(screenX1, screenY1, screenX2, screenY2, pimg);
        }
    }

    /**
     * @brief 绘制网格
     * @param pimg 目标图像
     */
    void drawGrid(PIMAGE pimg)
    {
        setcolor(m_gridColor, pimg);
        setlinewidth(1, pimg);

        // 计算网格间隔
        double xStep = (m_xMax - m_xMin) / 20.0;
        double yStep = (m_yMax - m_yMin) / 15.0;

        // 绘制垂直网格线
        for (double x = m_xMin; x <= m_xMax; x += xStep) {
            int screenX1, screenY1, screenX2, screenY2;
            mathToScreen(x, m_yMin, screenX1, screenY1);
            mathToScreen(x, m_yMax, screenX2, screenY2);
            line(screenX1, screenY1, screenX2, screenY2, pimg);
        }

        // 绘制水平网格线
        for (double y = m_yMin; y <= m_yMax; y += yStep) {
            int screenX1, screenY1, screenX2, screenY2;
            mathToScreen(m_xMin, y, screenX1, screenY1);
            mathToScreen(m_xMax, y, screenX2, screenY2);
            line(screenX1, screenY1, screenX2, screenY2, pimg);
        }
    }

    /**
     * @brief 使用蒙特卡洛法绘制函数图像
     * @param func 要绘制的函数
     * @param pimg 目标图像
     */
    void drawFunction(const FunctionType& func, PIMAGE pimg)
    {
        setcolor(m_pointColor, pimg);
        setfillcolor(m_pointColor, pimg);

        for (int i = 0; i < m_sampleCount; ++i) {
            // 生成随机点
            double x = m_xDistribution(m_generator);
            double y = m_yDistribution(m_generator);

            // 计算函数值
            double value = func(x, y);

            // 如果函数值在容差范围内，绘制该点
            if (std::abs(value) < m_tolerance) {
                int screenX, screenY;
                mathToScreen(x, y, screenX, screenY);

                // 检查是否在屏幕范围内
                if (screenX >= 0 && screenX < m_width && screenY >= 0 && screenY < m_height) {
                    if (m_pointSize == 1) {
                        putpixel(screenX, screenY, m_pointColor, pimg);
                    } else {
                        // 绘制圆形点
                        fillcircle(screenX, screenY, m_pointSize, pimg);
                    }
                }
            }
        }
    }

    /**
     * @brief 绘制标签和坐标
     * @param pimg 目标图像
     */
    void drawLabels(PIMAGE pimg)
    {
        setcolor(m_axisColor, pimg);
        setbkmode(TRANSPARENT, pimg);

        // 绘制坐标范围信息
        if (pimg) {
            // 如果指定了目标图像，需要临时设置为当前目标
            PIMAGE oldTarget = gettarget();
            settarget(pimg);
            xyprintf(10, 10, TEXT_COORD_RANGE, m_xMin, m_xMax, m_yMin, m_yMax);
            xyprintf(10, 30, TEXT_PARAMS_INFO, m_sampleCount, m_tolerance);
            xyprintf(10, 50, TEXT_POINT_SIZE_INFO, m_pointSize);
            settarget(oldTarget);
        } else {
            // 直接输出到当前窗口
            xyprintf(10, 10, TEXT_COORD_RANGE, m_xMin, m_xMax, m_yMin, m_yMax);
            xyprintf(10, 30, TEXT_PARAMS_INFO, m_sampleCount, m_tolerance);
            xyprintf(10, 50, TEXT_POINT_SIZE_INFO, m_pointSize);
        }
    }

private:
    int     m_width, m_height; // 绘图区域尺寸
    double  m_xMin, m_xMax;    // X轴数学坐标范围
    double  m_yMin, m_yMax;    // Y轴数学坐标范围
    double  m_tolerance;       // 函数值容差
    int     m_sampleCount;     // 蒙特卡洛采样点数
    color_t m_pointColor;      // 函数点颜色
    color_t m_backgroundColor; // 背景颜色
    color_t m_axisColor;       // 坐标轴颜色
    color_t m_gridColor;       // 网格颜色
    bool    m_drawAxes;        // 是否绘制坐标轴
    bool    m_showGrid;        // 是否显示网格
    int     m_pointSize = 3;   // 点大小

    // 随机数生成器
    mutable std::mt19937                           m_generator;
    mutable std::uniform_real_distribution<double> m_xDistribution;
    mutable std::uniform_real_distribution<double> m_yDistribution;

    // 预定义函数集合
    std::map<std::string, FunctionType> m_functions;
};

// 示例函数定义
namespace Examples
{

/**
 * @brief 圆形函数: x^2 + y^2 - r^2 = 0
 * @param radius 圆的半径
 * @return 圆形函数
 */
Function2DRenderer::FunctionType circle(double radius)
{
    return [radius](double x, double y) -> double { return x * x + y * y - radius * radius; };
}

/**
 * @brief 椭圆函数: (x/a)^2 + (y/b)^2 - 1 = 0
 * @param a 椭圆x轴半径
 * @param b 椭圆y轴半径
 * @return 椭圆函数
 */
Function2DRenderer::FunctionType ellipse(double a, double b)
{
    return [a, b](double x, double y) -> double { return (x * x) / (a * a) + (y * y) / (b * b) - 1.0; };
}

/**
 * @brief 抛物线函数: y - ax^2 - bx - c = 0
 * @param a 二次项系数
 * @param b 一次项系数
 * @param c 常数项
 * @return 抛物线函数
 */
Function2DRenderer::FunctionType parabola(double a, double b, double c)
{
    return [a, b, c](double x, double y) -> double { return y - a * x * x - b * x - c; };
}

/**
 * @brief 双曲线函数: (x/a)^2 - (y/b)^2 - 1 = 0
 * @param a x轴参数
 * @param b y轴参数
 * @return 双曲线函数
 */
Function2DRenderer::FunctionType hyperbola(double a, double b)
{
    return [a, b](double x, double y) -> double { return (x * x) / (a * a) - (y * y) / (b * b) - 1.0; };
}

/**
 * @brief 正弦波函数: y - A*sin(B*x + C) = 0
 * @param A 振幅
 * @param B 频率
 * @param C 相位
 * @return 正弦波函数
 */
Function2DRenderer::FunctionType sineWave(double A, double B, double C)
{
    return [A, B, C](double x, double y) -> double { return y - A * std::sin(B * x + C); };
}

/**
 * @brief 花瓣函数: r = A*sin(n*θ), 转换为直角坐标
 * @param A 振幅
 * @param n 花瓣数量参数
 * @return 花瓣函数
 */
Function2DRenderer::FunctionType rose(double A, int n)
{
    return [A, n](double x, double y) -> double {
        double r = std::sqrt(x * x + y * y);
        if (r < 1e-10) {
            return 0.0;
        }
        double theta      = std::atan2(y, x);
        double expected_r = A * std::sin(n * theta);
        return std::abs(r - std::abs(expected_r));
    };
}

/**
 * @brief 心形函数: (x^2 + y^2 - 1)^3 - x^2*y^3 = 0
 * @return 心形函数
 */
Function2DRenderer::FunctionType heart()
{
    return [](double x, double y) -> double {
        double temp = x * x + y * y - 1.0;
        return temp * temp * temp - x * x * y * y * y;
    };
}

/**
 * @brief 莲花函数: x^2 + y^2 - sin(4*atan2(y,x)) = 0
 * @return 莲花函数
 */
Function2DRenderer::FunctionType lotus()
{
    return [](double x, double y) -> double {
        double r     = std::sqrt(x * x + y * y);
        double theta = std::atan2(y, x);
        return r - 0.5 * (1 + std::sin(4 * theta));
    };
}
} // namespace Examples

// 演示程序
int main()
{
    // 初始化图形界面
    initgraph(800, 600, INIT_RENDERMANUAL);
    setbkcolor(BLACK);
    setbkmode(TRANSPARENT);
    setcaption(TEXT_WINDOW_TITLE);

    // 设置字体
    settextjustify(LEFT_TEXT, TOP_TEXT);
    setfont(16, 0, TEXT_FONT_NAME);

    // 创建函数图像渲染器
    Function2DRenderer renderer(800, 600, -5.0, 5.0, -5.0, 5.0);

    // 设置渲染参数
    renderer.setTolerance(0.05);
    renderer.setSampleCount(200000);
    renderer.setColors(YELLOW, BLACK);
    renderer.setAxisColors(WHITE, DARKGRAY);
    renderer.setDrawAxes(true);
    renderer.setShowGrid(true);

    // 添加预定义函数
    renderer.addFunction(TEXT_FUNCTION_CIRCLE, Examples::circle(2.0));
    renderer.addFunction(TEXT_FUNCTION_ELLIPSE, Examples::ellipse(3.0, 2.0));
    renderer.addFunction(TEXT_FUNCTION_PARABOLA, Examples::parabola(1.0, 0.0, 0.0));
    renderer.addFunction(TEXT_FUNCTION_HYPERBOLA, Examples::hyperbola(2.0, 1.5));
    renderer.addFunction(TEXT_FUNCTION_SINE, Examples::sineWave(2.0, 1.0, 0.0));
    renderer.addFunction(TEXT_FUNCTION_ROSE, Examples::rose(2.0, 3));
    renderer.addFunction(TEXT_FUNCTION_HEART, Examples::heart());
    renderer.addFunction(TEXT_FUNCTION_LOTUS, Examples::lotus());

    // 获取函数列表
    auto functionNames   = renderer.getFunctionNames();
    int  currentFunction = 0;

    PIMAGE imgCache = newimage(800, 600);

    // 渲染第一个函数
    if (!functionNames.empty()) {
        renderer.renderFunction(functionNames[currentFunction], imgCache);
    }

    // 主循环
    bool running      = true;
    bool drawFunction = true;

    while (running) {
        // 检查键盘输入

        while (kbhit()) {
            int key = getch();

            switch (key) {
            case 27: // ESC键 - 退出程序
                running = false;
                break;
            case '+': // 增加采样点数量 (+10000)
            case '=':
                renderer.setSampleCount(renderer.sampleCount() + 10000);
                drawFunction = true;
                break;
            case '-': // 减少采样点数量 (-10000)
            case '_':
                renderer.setSampleCount(std::max(1000, renderer.sampleCount() - 10000));
                drawFunction = true;
                break;
            case 'w': // 增加容差值 (+0.01) - 使函数线条更粗
            case 'W':
                renderer.setTolerance(std::min(1.0, renderer.tolerance() + 0.01));
                drawFunction = true;
                break;
            case 's': // 减少容差值 (-0.01) - 使函数线条更细
            case 'S':
                renderer.setTolerance(std::max(0.01, renderer.tolerance() - 0.01));
                drawFunction = true;
                break;
            case '[': // 减少点大小
                renderer.setPointSize(renderer.pointSize() - 1);
                drawFunction = true;
                break;
            case ']': // 增大点大小
                renderer.setPointSize(renderer.pointSize() + 1);
                drawFunction = true;
                break;
            case 32: // 空格键 - 切换到下一个函数
                if (!functionNames.empty()) {
                    currentFunction = (currentFunction + 1) % functionNames.size();
                    drawFunction    = true;
                }
                break;
            case 'r': // R键 - 重新绘制当前函数
            case 'R':
                drawFunction = true;
                break;
            }
        }

        if (drawFunction && !functionNames.empty()) {
            settarget(imgCache);
            setbkcolor(BLACK);
            cleardevice(); // 清除屏幕
            setcolor(WHITE);
            setbkmode(TRANSPARENT);
            outtextxy(10, 50, TEXT_CONTROLS_TITLE, NULL);
            outtextxy(10, 70, TEXT_CONTROLS_SPACE, NULL);
            outtextxy(10, 90, TEXT_CONTROLS_SAMPLES, NULL);
            outtextxy(10, 110, TEXT_CONTROLS_SAMPLES_DOWN, NULL);
            outtextxy(10, 130, TEXT_CONTROLS_TOLERANCE_UP, NULL);
            outtextxy(10, 150, TEXT_CONTROLS_TOLERANCE_DOWN, NULL);
            outtextxy(10, 170, TEXT_CONTROLS_POINT_SIZE_DOWN, NULL);
            outtextxy(10, 190, TEXT_CONTROLS_POINT_SIZE_UP, NULL);
            outtextxy(10, 210, TEXT_CONTROLS_REDRAW, NULL);
            outtextxy(10, 230, TEXT_CONTROLS_EXIT, NULL);

            // 显示当前函数名称
            xyprintf(10, 250, TEXT_CURRENT_FUNCTION, functionNames[currentFunction].c_str());

            renderer.renderFunction(functionNames[currentFunction], imgCache);
            settarget(nullptr);
            drawFunction = false; // 重置绘制标志
        }

        cleardevice();            // 清除屏幕
        putimage(0, 0, imgCache); // 绘制缓存图像

        delay_fps(60);
    }

    // 清理资源
    delimage(imgCache);
    closegraph();

    return 0;
}