////////////////////////////////////////
/// @file ege_implicit_function_plot.cpp
/// @brief ege隐函数图像绘制 demo
///
/// 1. 实现表达式解析。
/// 2. 用零点判定的方法绘制函数。
///
/// @date 2025-07-07
///
////////////////////////////////////////

#include <graphics.h>
#include <iostream>
#include <string.h>
#include "arithmetic.h" // 这里用了 wysaid 提供的表达式计算头文件

char flag[860][640]; // 储存像素点函数值正负情况

int main()
{
    initgraph(860, 640);
    init_console();
    setcolor(GRAY);
    // 绘制坐标轴
    ege_line(0, 320, 860, 320);
    ege_line(430, 0, 430, 640);
    // 循环输入函数表达式
    for (int t = 1; is_run(); t++) {
        // 初始化
        memset(flag, 0, sizeof(flag));
        // 随机颜色
        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;
        // 输入表达式
        std::string expression;
        std::cout << "输入函数 F" << t << ": ";
        std::getline(std::cin, expression);
        // 化简表达式
        ArithmeticExpression e(expression);
        e.reduceNode();
        // 将等号右侧移到左侧
        int         ep   = expression.find('=');
        std::string temp = expression.substr(0, ep) + "-(" + expression.substr(ep + 1) + ')';
        std::cout << "移项 > " << temp << " = 0" << std::endl;
        // 计算像素点上的函数值
        for (int i = 0; i < 860; i++) {
            for (int j = 0; j < 640; j++) {
                double x = (i - 430) / 50.0;
                double y = -(j - 320) / 50.0;
                e.setX(x), e.setY(y);
                double value = e.value();
                if (value > 0) {
                    flag[i][j] = 1; // 正数
                }
                if (value == 0) {
                    flag[i][j] = 2; // 零点
                }
                if (value < 0) {
                    flag[i][j] = 3; // 负数
                }
                if (!e.isValid() || isnan(e.value()) || isinf(e.value())) {
                    flag[i][j] = 4; // 计算结果不合法
                }
            }
        }
        // 绘制图像
        setcolor(EGERGB(r, g, b));
        for (int i = 1; i < 860 - 1; i++) {
            for (int j = 1; j < 640 - 1; j++) {
                if (flag[i - 1][j - 1] == 4 || flag[i - 1][j] == 4 || flag[i - 1][j + 1] == 4 || flag[i][j - 1] == 4 ||
                    flag[i][j] == 4 || flag[i][j + 1] == 4 || flag[i + 1][j - 1] == 4 || flag[i + 1][j] == 4 ||
                    flag[i + 1][j + 1] == 4)
                {
                    // 计算结果不合法
                    continue;
                }
                if (flag[i][j] == 2) {
                    putpixel(i, j, EGERGB(r, g, b));
                    continue;
                }
                int s1 = 0, s2 = 0;
                // 计算周围8个像素中正数的像素点个数
                s1 += (flag[i - 1][j - 1] == 1) + (flag[i - 1][j] == 1) + (flag[i - 1][j + 1] == 1) +
                    (flag[i][j - 1] == 1) + (flag[i][j] == 1) + (flag[i][j + 1] == 1) + (flag[i + 1][j - 1] == 1) +
                    (flag[i + 1][j] == 1) + (flag[i + 1][j + 1] == 1);
                // 计算周围8个像素中负数的像素点个数
                s2 += (flag[i - 1][j - 1] == 3) + (flag[i - 1][j] == 3) + (flag[i - 1][j + 1] == 3) +
                    (flag[i][j - 1] == 3) + (flag[i][j] == 3) + (flag[i][j + 1] == 3) + (flag[i + 1][j - 1] == 3) +
                    (flag[i + 1][j] == 3) + (flag[i + 1][j + 1] == 3);
                // 如果有正有负就标记为零点
                if (s1 && s2) {
                    putpixel(i, j, EGERGB(r, g, b));
                }
            }
        }
    }
    close_console();
    closegraph();
}