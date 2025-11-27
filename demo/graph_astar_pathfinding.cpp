/**
 * @file graph_astar_pathfinding.cpp
 * @author wysaid (this@xege.org)
 * @brief A* 寻路算法可视化演示
 * @version 0.1
 * @date 2025-11-27
 *
 * A* 是一种经典的启发式搜索算法，广泛用于游戏开发和机器人路径规划。
 * 本程序通过动画演示 A* 算法的搜索过程，包括：
 * 1. 从起点开始，评估周围节点
 * 2. 选择 f(n) = g(n) + h(n) 最小的节点扩展
 * 3. 重复直到找到终点或确定无解
 * 4. 回溯找到最短路径
 *
 * 用户可以通过鼠标绘制障碍物，设置起点和终点
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
#include <queue>
#include <set>
#include <string>
#include <functional>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE        "A* 寻路算法可视化演示"
#define TEXT_CONTROLS_TITLE      "按键说明："
#define TEXT_CONTROLS_START      "S/空格/回车 - 开始/单步执行"
#define TEXT_CONTROLS_RESET      "R - 重置路径 (保留障碍物)"
#define TEXT_CONTROLS_CLEAR      "C - 清除所有障碍物"
#define TEXT_CONTROLS_GENERATE   "G - 生成随机迷宫"
#define TEXT_CONTROLS_AUTO       "A - 自动演示模式"
#define TEXT_CONTROLS_SPEED      "↑/↓ - 调整动画速度"
#define TEXT_CONTROLS_DIAGONAL   "D - 切换对角线移动"
#define TEXT_CONTROLS_EXIT       "ESC - 退出程序"
#define TEXT_MOUSE_HINT          "鼠标操作："
#define TEXT_MOUSE_LEFT          "左键 - 绘制/清除障碍物"
#define TEXT_MOUSE_RIGHT         "右键 - 设置起点(绿)/终点(红)"
#define TEXT_STATUS_READY        "状态: 准备就绪"
#define TEXT_STATUS_SEARCHING    "状态: 搜索中..."
#define TEXT_STATUS_FOUND        "状态: 找到路径!"
#define TEXT_STATUS_NO_PATH      "状态: 无法到达!"
#define TEXT_STATUS_AUTO         "状态: 自动演示中..."
#define TEXT_GRID_SIZE           "网格大小: %d x %d"
#define TEXT_ANIMATION_SPEED     "动画速度: %d ms"
#define TEXT_PATH_LENGTH         "路径长度: %d"
#define TEXT_NODES_EXPLORED      "已探索节点: %d"
#define TEXT_DIAGONAL_ON         "对角线移动: 开启"
#define TEXT_DIAGONAL_OFF        "对角线移动: 关闭"
#define TEXT_FONT_NAME           "宋体"
#define TEXT_LEGEND_TITLE        "图例："
#define TEXT_LEGEND_START        "起点"
#define TEXT_LEGEND_END          "终点"
#define TEXT_LEGEND_WALL         "障碍物"
#define TEXT_LEGEND_OPEN         "待探索"
#define TEXT_LEGEND_CLOSED       "已探索"
#define TEXT_LEGEND_PATH         "最短路径"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE        "A* Pathfinding Visualization"
#define TEXT_CONTROLS_TITLE      "Controls:"
#define TEXT_CONTROLS_START      "S/Space/Enter - Start/Step"
#define TEXT_CONTROLS_RESET      "R - Reset Path (Keep Walls)"
#define TEXT_CONTROLS_CLEAR      "C - Clear All Walls"
#define TEXT_CONTROLS_GENERATE   "G - Generate Random Maze"
#define TEXT_CONTROLS_AUTO       "A - Auto Demo Mode"
#define TEXT_CONTROLS_SPEED      "Up/Down - Adjust Speed"
#define TEXT_CONTROLS_DIAGONAL   "D - Toggle Diagonal Move"
#define TEXT_CONTROLS_EXIT       "ESC - Exit Program"
#define TEXT_MOUSE_HINT          "Mouse:"
#define TEXT_MOUSE_LEFT          "Left - Draw/Erase Walls"
#define TEXT_MOUSE_RIGHT         "Right - Set Start(G)/End(R)"
#define TEXT_STATUS_READY        "Status: Ready"
#define TEXT_STATUS_SEARCHING    "Status: Searching..."
#define TEXT_STATUS_FOUND        "Status: Path Found!"
#define TEXT_STATUS_NO_PATH      "Status: No Path!"
#define TEXT_STATUS_AUTO         "Status: Auto Demo..."
#define TEXT_GRID_SIZE           "Grid: %d x %d"
#define TEXT_ANIMATION_SPEED     "Speed: %d ms"
#define TEXT_PATH_LENGTH         "Path Length: %d"
#define TEXT_NODES_EXPLORED      "Nodes Explored: %d"
#define TEXT_DIAGONAL_ON         "Diagonal: ON"
#define TEXT_DIAGONAL_OFF        "Diagonal: OFF"
#define TEXT_FONT_NAME           "Arial"
#define TEXT_LEGEND_TITLE        "Legend:"
#define TEXT_LEGEND_START        "Start"
#define TEXT_LEGEND_END          "End"
#define TEXT_LEGEND_WALL         "Wall"
#define TEXT_LEGEND_OPEN         "Open"
#define TEXT_LEGEND_CLOSED       "Closed"
#define TEXT_LEGEND_PATH         "Path"
#endif

// 窗口和网格参数
const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;
const int PANEL_WIDTH   = 250;                           // 右侧控制面板宽度
const int CANVAS_WIDTH  = WINDOW_WIDTH - PANEL_WIDTH;    // 绘图区域宽度
const int CANVAS_HEIGHT = WINDOW_HEIGHT;                 // 绘图区域高度
const int CELL_SIZE     = 20;                            // 每个格子的大小
const int GRID_COLS     = CANVAS_WIDTH / CELL_SIZE;      // 网格列数
const int GRID_ROWS     = CANVAS_HEIGHT / CELL_SIZE;     // 网格行数

// 颜色定义 (使用 ASTAR_ 前缀避免与 Windows 宏冲突)
const color_t ASTAR_COLOR_BG      = EGERGB(30, 30, 40);
const color_t ASTAR_COLOR_GRID    = EGERGB(50, 50, 60);
const color_t ASTAR_COLOR_WALL    = EGERGB(60, 60, 80);
const color_t ASTAR_COLOR_START   = EGERGB(46, 204, 113);   // 绿色
const color_t ASTAR_COLOR_END     = EGERGB(231, 76, 60);    // 红色
const color_t ASTAR_COLOR_OPEN    = EGERGB(52, 152, 219);   // 蓝色 (待探索)
const color_t ASTAR_COLOR_CLOSED  = EGERGB(155, 89, 182);   // 紫色 (已探索)
const color_t ASTAR_COLOR_PATH    = EGERGB(241, 196, 15);   // 黄色 (路径)
const color_t ASTAR_COLOR_CURRENT = EGERGB(230, 126, 34);   // 橙色 (当前节点)

/**
 * @enum CellType
 * @brief 单元格类型
 */
enum CellType
{
    CELL_EMPTY,    // 空地
    CELL_WALL,     // 障碍物
    CELL_START,    // 起点
    CELL_END       // 终点
};

/**
 * @enum CellState
 * @brief 单元格状态 (搜索过程中)
 */
enum CellState
{
    STATE_NONE,    // 未访问
    STATE_OPEN,    // 在开放列表中
    STATE_CLOSED,  // 在关闭列表中
    STATE_PATH     // 在最终路径上
};

/**
 * @enum AlgorithmState
 * @brief 算法状态
 */
enum AlgorithmState
{
    ALG_READY,      // 准备就绪
    ALG_SEARCHING,  // 搜索中
    ALG_FOUND,      // 找到路径
    ALG_NO_PATH,    // 无解
    ALG_AUTO        // 自动演示
};

/**
 * @struct Node
 * @brief A* 算法的节点结构
 */
struct Node
{
    int row, col;      // 位置
    float g;           // 从起点到当前节点的实际代价
    float h;           // 从当前节点到终点的启发式估计代价
    float f;           // f = g + h
    int parentRow;     // 父节点行
    int parentCol;     // 父节点列

    Node(int r = 0, int c = 0) : row(r), col(c), g(0), h(0), f(0), parentRow(-1), parentCol(-1) {}

    // 用于优先队列比较 (f 值小的优先)
    bool operator>(const Node& other) const { return f > other.f; }
};

/**
 * @class AStarVisualizer
 * @brief A* 寻路算法可视化类
 */
class AStarVisualizer
{
public:
    AStarVisualizer()
        : m_startRow(GRID_ROWS / 2)
        , m_startCol(3)
        , m_endRow(GRID_ROWS / 2)
        , m_endCol(GRID_COLS - 4)
        , m_state(ALG_READY)
        , m_animationSpeed(30)
        , m_autoMode(false)
        , m_allowDiagonal(true)
        , m_pathLength(0)
        , m_nodesExplored(0)
        , m_settingStart(true)
    {
        // 初始化网格
        m_grid.resize(GRID_ROWS, std::vector<CellType>(GRID_COLS, CELL_EMPTY));
        m_cellState.resize(GRID_ROWS, std::vector<CellState>(GRID_COLS, STATE_NONE));
        m_nodeInfo.resize(GRID_ROWS, std::vector<Node>(GRID_COLS));

        // 设置起点和终点
        m_grid[m_startRow][m_startCol] = CELL_START;
        m_grid[m_endRow][m_endCol]     = CELL_END;

        // 初始化节点信息
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                m_nodeInfo[r][c] = Node(r, c);
            }
        }
    }

    // 重置搜索状态 (保留障碍物)
    void resetSearch()
    {
        m_state         = ALG_READY;
        m_pathLength    = 0;
        m_nodesExplored = 0;
        m_autoMode      = false;

        // 清空优先队列
        while (!m_openSet.empty()) m_openSet.pop();
        m_openSetLookup.clear();

        // 重置单元格状态
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                m_cellState[r][c]          = STATE_NONE;
                m_nodeInfo[r][c].g         = 0;
                m_nodeInfo[r][c].h         = 0;
                m_nodeInfo[r][c].f         = 0;
                m_nodeInfo[r][c].parentRow = -1;
                m_nodeInfo[r][c].parentCol = -1;
            }
        }
    }

    // 清除所有障碍物
    void clearWalls()
    {
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                if (m_grid[r][c] == CELL_WALL) {
                    m_grid[r][c] = CELL_EMPTY;
                }
            }
        }
        resetSearch();
    }

    // 生成随机迷宫 (使用递归分割法)
    void generateMaze()
    {
        clearWalls();

        // 简单随机障碍物生成
        srand(static_cast<unsigned>(time(nullptr)));

        // 生成一些随机墙壁
        int wallCount = (GRID_ROWS * GRID_COLS) / 4;
        for (int i = 0; i < wallCount; ++i) {
            int r = rand() % GRID_ROWS;
            int c = rand() % GRID_COLS;

            // 不覆盖起点和终点
            if (m_grid[r][c] == CELL_EMPTY) {
                m_grid[r][c] = CELL_WALL;
            }
        }

        // 添加一些连续的墙壁使其更像迷宫
        int lineCount = 15;
        for (int i = 0; i < lineCount; ++i) {
            int r      = rand() % GRID_ROWS;
            int c      = rand() % GRID_COLS;
            int length = rand() % 15 + 5;
            bool horizontal = rand() % 2;

            for (int j = 0; j < length; ++j) {
                int nr = horizontal ? r : r + j;
                int nc = horizontal ? c + j : c;

                if (nr >= 0 && nr < GRID_ROWS && nc >= 0 && nc < GRID_COLS) {
                    if (m_grid[nr][nc] == CELL_EMPTY) {
                        m_grid[nr][nc] = CELL_WALL;
                    }
                }
            }
        }

        // 确保起点和终点周围有一定空间
        clearAroundPoint(m_startRow, m_startCol, 2);
        clearAroundPoint(m_endRow, m_endCol, 2);

        resetSearch();
    }

    // 清除某点周围的障碍物
    void clearAroundPoint(int row, int col, int radius)
    {
        for (int dr = -radius; dr <= radius; ++dr) {
            for (int dc = -radius; dc <= radius; ++dc) {
                int nr = row + dr;
                int nc = col + dc;
                if (nr >= 0 && nr < GRID_ROWS && nc >= 0 && nc < GRID_COLS) {
                    if (m_grid[nr][nc] == CELL_WALL) {
                        m_grid[nr][nc] = CELL_EMPTY;
                    }
                }
            }
        }
    }

    // 初始化 A* 搜索
    void initSearch()
    {
        resetSearch();
        m_state = ALG_SEARCHING;

        // 将起点加入开放列表
        Node& startNode   = m_nodeInfo[m_startRow][m_startCol];
        startNode.g       = 0;
        startNode.h       = heuristic(m_startRow, m_startCol, m_endRow, m_endCol);
        startNode.f       = startNode.g + startNode.h;

        m_openSet.push(startNode);
        m_openSetLookup.insert(m_startRow * GRID_COLS + m_startCol);
        m_cellState[m_startRow][m_startCol] = STATE_OPEN;
    }

    // 执行一步 A* 搜索
    bool stepSearch()
    {
        if (m_state != ALG_SEARCHING && m_state != ALG_AUTO) {
            if (m_state == ALG_READY) {
                initSearch();
            }
            return false;
        }

        if (m_openSet.empty()) {
            m_state = ALG_NO_PATH;
            return true;
        }

        // 取出 f 值最小的节点
        Node current = m_openSet.top();
        m_openSet.pop();
        m_openSetLookup.erase(current.row * GRID_COLS + current.col);

        int row = current.row;
        int col = current.col;

        // 标记为已探索
        m_cellState[row][col] = STATE_CLOSED;
        ++m_nodesExplored;

        // 检查是否到达终点
        if (row == m_endRow && col == m_endCol) {
            m_state = ALG_FOUND;
            reconstructPath();
            return true;
        }

        // 探索相邻节点
        static const int dx4[] = {0, 0, 1, -1};
        static const int dy4[] = {1, -1, 0, 0};
        static const int dx8[] = {0, 0, 1, -1, 1, 1, -1, -1};
        static const int dy8[] = {1, -1, 0, 0, 1, -1, 1, -1};

        int numDirs = m_allowDiagonal ? 8 : 4;
        const int* dx = m_allowDiagonal ? dx8 : dx4;
        const int* dy = m_allowDiagonal ? dy8 : dy4;

        for (int i = 0; i < numDirs; ++i) {
            int newRow = row + dy[i];
            int newCol = col + dx[i];

            // 检查边界
            if (newRow < 0 || newRow >= GRID_ROWS || newCol < 0 || newCol >= GRID_COLS) {
                continue;
            }

            // 检查是否是墙壁或已关闭
            if (m_grid[newRow][newCol] == CELL_WALL || m_cellState[newRow][newCol] == STATE_CLOSED) {
                continue;
            }

            // 对角线移动时检查是否被墙壁阻挡
            if (m_allowDiagonal && i >= 4) {
                int checkRow1 = row + dy[i];
                int checkCol1 = col;
                int checkRow2 = row;
                int checkCol2 = col + dx[i];

                if (m_grid[checkRow1][checkCol1] == CELL_WALL && m_grid[checkRow2][checkCol2] == CELL_WALL) {
                    continue;
                }
            }

            // 计算新的 g 值
            float moveCost = (i >= 4) ? 1.414f : 1.0f; // 对角线移动代价为 √2
            float newG = m_nodeInfo[row][col].g + moveCost;

            // 检查是否已在开放列表中
            int nodeKey = newRow * GRID_COLS + newCol;
            bool inOpenSet = m_openSetLookup.find(nodeKey) != m_openSetLookup.end();

            if (!inOpenSet || newG < m_nodeInfo[newRow][newCol].g) {
                // 更新节点信息
                Node& neighbor   = m_nodeInfo[newRow][newCol];
                neighbor.g       = newG;
                neighbor.h       = heuristic(newRow, newCol, m_endRow, m_endCol);
                neighbor.f       = neighbor.g + neighbor.h;
                neighbor.parentRow = row;
                neighbor.parentCol = col;

                if (!inOpenSet) {
                    m_openSet.push(neighbor);
                    m_openSetLookup.insert(nodeKey);
                    m_cellState[newRow][newCol] = STATE_OPEN;
                }
            }
        }

        return false;
    }

    // 启发式函数 (使用欧几里得距离)
    float heuristic(int r1, int c1, int r2, int c2)
    {
        float dx = static_cast<float>(c2 - c1);
        float dy = static_cast<float>(r2 - r1);

        if (m_allowDiagonal) {
            // 对角线距离 (Chebyshev 距离的变体)
            return std::sqrt(dx * dx + dy * dy);
        } else {
            // 曼哈顿距离
            return std::abs(dx) + std::abs(dy);
        }
    }

    // 回溯重建路径
    void reconstructPath()
    {
        int row = m_endRow;
        int col = m_endCol;

        while (row != m_startRow || col != m_startCol) {
            if (m_grid[row][col] != CELL_START && m_grid[row][col] != CELL_END) {
                m_cellState[row][col] = STATE_PATH;
            }
            ++m_pathLength;

            int parentRow = m_nodeInfo[row][col].parentRow;
            int parentCol = m_nodeInfo[row][col].parentCol;

            if (parentRow < 0 || parentCol < 0) break;

            row = parentRow;
            col = parentCol;
        }
    }

    // 绘制所有内容
    void draw()
    {
        setbkcolor(ASTAR_COLOR_BG);
        cleardevice();

        ege_enable_aa(true);

        // 绘制网格
        drawGrid();

        // 绘制控制面板
        drawControlPanel();
    }

    // 绘制网格
    void drawGrid()
    {
        // 绘制单元格
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                int x = c * CELL_SIZE;
                int y = r * CELL_SIZE;

                color_t fillColor = ASTAR_COLOR_BG;

                // 根据单元格类型和状态确定颜色
                if (m_grid[r][c] == CELL_WALL) {
                    fillColor = ASTAR_COLOR_WALL;
                } else if (m_grid[r][c] == CELL_START) {
                    fillColor = ASTAR_COLOR_START;
                } else if (m_grid[r][c] == CELL_END) {
                    fillColor = ASTAR_COLOR_END;
                } else {
                    // 根据搜索状态
                    switch (m_cellState[r][c]) {
                        case STATE_PATH:
                            fillColor = ASTAR_COLOR_PATH;
                            break;
                        case STATE_CLOSED:
                            fillColor = ASTAR_COLOR_CLOSED;
                            break;
                        case STATE_OPEN:
                            fillColor = ASTAR_COLOR_OPEN;
                            break;
                        default:
                            fillColor = ASTAR_COLOR_BG;
                            break;
                    }
                }

                setfillcolor(fillColor);
                bar(x + 1, y + 1, x + CELL_SIZE - 1, y + CELL_SIZE - 1);
            }
        }

        // 绘制网格线
        setcolor(ASTAR_COLOR_GRID);
        for (int r = 0; r <= GRID_ROWS; ++r) {
            line(0, r * CELL_SIZE, CANVAS_WIDTH, r * CELL_SIZE);
        }
        for (int c = 0; c <= GRID_COLS; ++c) {
            line(c * CELL_SIZE, 0, c * CELL_SIZE, CANVAS_HEIGHT);
        }

        // 绘制起点和终点标记
        drawMarker(m_startRow, m_startCol, "S", ASTAR_COLOR_START);
        drawMarker(m_endRow, m_endCol, "E", ASTAR_COLOR_END);
    }

    // 绘制标记
    void drawMarker(int row, int col, const char* text, color_t color)
    {
        int x = col * CELL_SIZE + CELL_SIZE / 2;
        int y = row * CELL_SIZE + CELL_SIZE / 2;

        setfont(16, 0, TEXT_FONT_NAME);
        setcolor(ege::WHITE);
        settextjustify(CENTER_TEXT, CENTER_TEXT);
        outtextxy(x, y, text);
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
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 状态信息
        setfont(14, 0, TEXT_FONT_NAME);
        setcolor(EGERGB(150, 200, 255));

        char buf[128];

        // 显示网格大小
        sprintf(buf, TEXT_GRID_SIZE, GRID_COLS, GRID_ROWS);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示动画速度
        sprintf(buf, TEXT_ANIMATION_SPEED, m_animationSpeed);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示已探索节点数
        sprintf(buf, TEXT_NODES_EXPLORED, m_nodesExplored);
        outtextxy(textX, textY, buf);
        textY += lineHeight;

        // 显示路径长度
        if (m_state == ALG_FOUND) {
            sprintf(buf, TEXT_PATH_LENGTH, m_pathLength);
            outtextxy(textX, textY, buf);
        }
        textY += lineHeight;

        // 显示对角线移动状态
        if (m_allowDiagonal) {
            setcolor(EGERGB(100, 255, 100));
            outtextxy(textX, textY, TEXT_DIAGONAL_ON);
        } else {
            setcolor(EGERGB(255, 150, 150));
            outtextxy(textX, textY, TEXT_DIAGONAL_OFF);
        }
        textY += lineHeight + 5;

        // 显示状态
        switch (m_state) {
            case ALG_READY:
                setcolor(EGERGB(100, 255, 100));
                outtextxy(textX, textY, TEXT_STATUS_READY);
                break;
            case ALG_SEARCHING:
                setcolor(EGERGB(255, 200, 100));
                outtextxy(textX, textY, TEXT_STATUS_SEARCHING);
                break;
            case ALG_FOUND:
                setcolor(EGERGB(100, 255, 200));
                outtextxy(textX, textY, TEXT_STATUS_FOUND);
                break;
            case ALG_NO_PATH:
                setcolor(EGERGB(255, 100, 100));
                outtextxy(textX, textY, TEXT_STATUS_NO_PATH);
                break;
            case ALG_AUTO:
                setcolor(EGERGB(255, 150, 200));
                outtextxy(textX, textY, TEXT_STATUS_AUTO);
                break;
        }
        textY += lineHeight + 10;

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 图例
        setcolor(EGERGB(200, 200, 200));
        outtextxy(textX, textY, TEXT_LEGEND_TITLE);
        textY += lineHeight;

        drawLegendItem(textX, textY, ASTAR_COLOR_START, TEXT_LEGEND_START);
        textY += 20;
        drawLegendItem(textX, textY, ASTAR_COLOR_END, TEXT_LEGEND_END);
        textY += 20;
        drawLegendItem(textX, textY, ASTAR_COLOR_WALL, TEXT_LEGEND_WALL);
        textY += 20;
        drawLegendItem(textX, textY, ASTAR_COLOR_OPEN, TEXT_LEGEND_OPEN);
        textY += 20;
        drawLegendItem(textX, textY, ASTAR_COLOR_CLOSED, TEXT_LEGEND_CLOSED);
        textY += 20;
        drawLegendItem(textX, textY, ASTAR_COLOR_PATH, TEXT_LEGEND_PATH);
        textY += 25;

        // 分隔线
        setcolor(EGERGB(80, 80, 90));
        line(panelX + 10, textY, WINDOW_WIDTH - 10, textY);
        textY += 15;

        // 控制说明
        setcolor(EGERGB(200, 200, 200));
        outtextxy(textX, textY, TEXT_CONTROLS_TITLE);
        textY += lineHeight;

        setcolor(EGERGB(180, 180, 180));
        setfont(11, 0, TEXT_FONT_NAME);
        outtextxy(textX, textY, TEXT_CONTROLS_START);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_RESET);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_CLEAR);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_GENERATE);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_AUTO);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_SPEED);
        textY += 18;
        outtextxy(textX, textY, TEXT_CONTROLS_DIAGONAL);
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

    // 绘制图例项
    void drawLegendItem(int x, int y, color_t color, const char* text)
    {
        setfillcolor(color);
        bar(x, y + 2, x + 14, y + 16);
        setcolor(ege::WHITE);
        outtextxy(x + 20, y, text);
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
                    if (m_state == ALG_READY) {
                        initSearch();
                    }
                    if (m_state == ALG_SEARCHING) {
                        stepSearch();
                    }
                    break;

                case 'R':
                case 'r':
                    resetSearch();
                    break;

                case 'C':
                case 'c':
                    clearWalls();
                    break;

                case 'G':
                case 'g':
                    generateMaze();
                    break;

                case 'A':
                case 'a':
                    m_autoMode = !m_autoMode;
                    if (m_autoMode) {
                        if (m_state == ALG_READY) {
                            initSearch();
                        }
                        m_state = ALG_AUTO;
                    } else {
                        if (m_state == ALG_AUTO) {
                            m_state = ALG_SEARCHING;
                        }
                    }
                    break;

                case 'D':
                case 'd':
                    m_allowDiagonal = !m_allowDiagonal;
                    resetSearch();
                    break;

                case key_up:
                    m_animationSpeed = std::max(5, m_animationSpeed - 10);
                    break;

                case key_down:
                    m_animationSpeed = std::min(500, m_animationSpeed + 10);
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
        static bool leftButtonDown  = false;
        static bool rightButtonDown = false;
        static int  lastRow         = -1;
        static int  lastCol         = -1;

        while (mousemsg()) {
            mouse_msg msg = getmouse();

            int col = msg.x / CELL_SIZE;
            int row = msg.y / CELL_SIZE;

            // 确保在网格范围内
            if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) {
                continue;
            }

            if (msg.is_left()) {
                if (msg.is_down()) {
                    leftButtonDown = true;
                    lastRow        = row;
                    lastCol        = col;

                    // 切换墙壁状态
                    if (m_grid[row][col] == CELL_EMPTY) {
                        m_grid[row][col] = CELL_WALL;
                        resetSearch();
                    } else if (m_grid[row][col] == CELL_WALL) {
                        m_grid[row][col] = CELL_EMPTY;
                        resetSearch();
                    }
                } else if (msg.is_up()) {
                    leftButtonDown = false;
                } else if (msg.is_move() && leftButtonDown) {
                    // 拖动绘制墙壁
                    if (row != lastRow || col != lastCol) {
                        if (m_grid[row][col] == CELL_EMPTY) {
                            m_grid[row][col] = CELL_WALL;
                            resetSearch();
                        }
                        lastRow = row;
                        lastCol = col;
                    }
                }
            }

            if (msg.is_right()) {
                if (msg.is_down()) {
                    rightButtonDown = true;

                    // 设置起点或终点
                    if (m_grid[row][col] != CELL_WALL) {
                        if (m_settingStart) {
                            // 清除旧起点
                            m_grid[m_startRow][m_startCol] = CELL_EMPTY;
                            // 设置新起点
                            m_startRow                     = row;
                            m_startCol                     = col;
                            m_grid[row][col]               = CELL_START;
                        } else {
                            // 清除旧终点
                            m_grid[m_endRow][m_endCol] = CELL_EMPTY;
                            // 设置新终点
                            m_endRow                   = row;
                            m_endCol                   = col;
                            m_grid[row][col]           = CELL_END;
                        }
                        m_settingStart = !m_settingStart;
                        resetSearch();
                    }
                } else if (msg.is_up()) {
                    rightButtonDown = false;
                }
            }
        }
    }

    // 自动演示模式更新
    void autoUpdate()
    {
        if (m_autoMode && (m_state == ALG_SEARCHING || m_state == ALG_AUTO)) {
            m_state = ALG_AUTO;
            if (stepSearch()) {
                m_autoMode = false;
            }
        }
    }

    // 获取动画速度
    int getAnimationSpeed() const { return m_animationSpeed; }

    // 是否在自动模式
    bool isAutoMode() const { return m_autoMode; }

    // 启动自动演示 (生成迷宫并开始自动搜索)
    void startAutoDemo()
    {
        generateMaze();
        initSearch();
        m_autoMode = true;
        m_state    = ALG_AUTO;
    }

private:
    std::vector<std::vector<CellType>>  m_grid;          // 网格类型
    std::vector<std::vector<CellState>> m_cellState;     // 单元格状态
    std::vector<std::vector<Node>>      m_nodeInfo;      // 节点信息

    // A* 算法数据结构
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> m_openSet;
    std::set<int> m_openSetLookup; // 快速查找开放集合

    int m_startRow, m_startCol;    // 起点
    int m_endRow, m_endCol;        // 终点

    AlgorithmState m_state;        // 算法状态
    int            m_animationSpeed;
    bool           m_autoMode;
    bool           m_allowDiagonal;
    int            m_pathLength;
    int            m_nodesExplored;
    bool           m_settingStart; // 用于交替设置起点和终点
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
    AStarVisualizer visualizer;

    // 启动时自动生成迷宫并开始演示
    visualizer.startAutoDemo();

    // 主循环
    int frameCount = 0;
    for (; is_run(); delay_fps(60)) {
        // 处理输入
        visualizer.handleInput();
        visualizer.handleMouse();

        // 自动模式更新
        if (visualizer.isAutoMode()) {
            int framesPerStep = visualizer.getAnimationSpeed() / 16;
            if (framesPerStep < 1) framesPerStep = 1;
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
