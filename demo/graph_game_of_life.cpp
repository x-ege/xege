/**
 * @file graph_game_of_life.cpp
 * @brief Conway's Game of Life Visualization Demo
 *
 * 康威生命游戏可视化演示
 * - 经典的元胞自动机模拟
 * - 简单规则产生复杂涌现行为
 * - 支持鼠标绘制初始状态
 * - 内置多种经典图案
 *
 * Controls / 控制:
 *   Space    - Pause/Resume 暂停/继续
 *   R        - Random reset 随机重置
 *   C        - Clear grid 清空网格
 *   G        - Toggle grid lines 开关网格线
 *   T        - Toggle trail effect 开关轨迹效果
 *   +/-      - Speed up/down 加速/减速
 *   1-9      - Load preset patterns 加载预设图案
 *   Mouse Left  - Draw/Erase cells 绘制/擦除细胞
 *   Mouse Right - Pan view 平移视图
 *   Mouse Wheel - Zoom in/out 缩放
 *   ESC      - Exit 退出
 *
 * Rules / 规则:
 *   1. 任何活细胞周围少于2个活邻居则死亡（孤独）
 *   2. 任何活细胞周围有2-3个活邻居则存活
 *   3. 任何活细胞周围超过3个活邻居则死亡（拥挤）
 *   4. 任何死细胞周围恰好3个活邻居则复活（繁殖）
 */

// 禁用 Windows.h 中的 min/max 宏，避免与 std::min/std::max 冲突
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <graphics.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _MSC_VER
#define DEMO_TITLE      "康威生命游戏"
#define STR_PAUSED      "已暂停"
#define STR_RUNNING     "运行中"
#define STR_GENERATION  "代数: %d"
#define STR_POPULATION  "存活: %d"
#define STR_SPEED       "速度: %d 代/秒"
#define STR_ZOOM        "缩放: %.1fx"
#define STR_GRID        "网格: %s"
#define STR_TRAIL       "轨迹: %s"
#define STR_ON          "开"
#define STR_OFF         "关"
#define STR_CONTROLS    "控制说明"
#define STR_SPACE       "空格 - 暂停/继续"
#define STR_KEY_R       "R - 随机重置"
#define STR_KEY_C       "C - 清空网格"
#define STR_KEY_G       "G - 开关网格"
#define STR_KEY_T       "T - 开关轨迹"
#define STR_PLUS_MINUS  "+/- - 调整速度"
#define STR_MOUSE_L     "左键 - 绘制细胞"
#define STR_MOUSE_R     "右键 - 平移视图"
#define STR_WHEEL       "滚轮 - 缩放"
#define STR_NUM_KEYS    "1-9 - 预设图案"
#define STR_ESC         "ESC - 退出"
#define STR_PATTERNS    "预设图案"
#define STR_PAT_GLIDER  "1 - 滑翔机"
#define STR_PAT_LWSS    "2 - 轻型飞船"
#define STR_PAT_PULSAR  "3 - 脉冲星"
#define STR_PAT_GOSPER  "4 - 高斯帕枪"
#define STR_PAT_PENTA   "5 - 五联体"
#define STR_PAT_DIEHARD "6 - 顽固"
#define STR_PAT_ACORN   "7 - 橡子"
#define STR_PAT_INF     "8 - 无限增长"
#define STR_PAT_RANDOM  "9 - 随机"
#else
#define DEMO_TITLE      "Conway's Game of Life"
#define STR_PAUSED      "Paused"
#define STR_RUNNING     "Running"
#define STR_GENERATION  "Gen: %d"
#define STR_POPULATION  "Pop: %d"
#define STR_SPEED       "Speed: %d gen/s"
#define STR_ZOOM        "Zoom: %.1fx"
#define STR_GRID        "Grid: %s"
#define STR_TRAIL       "Trail: %s"
#define STR_ON          "On"
#define STR_OFF         "Off"
#define STR_CONTROLS    "Controls"
#define STR_SPACE       "Space - Pause/Resume"
#define STR_KEY_R       "R - Random Reset"
#define STR_KEY_C       "C - Clear Grid"
#define STR_KEY_G       "G - Toggle Grid"
#define STR_KEY_T       "T - Toggle Trail"
#define STR_PLUS_MINUS  "+/- - Adjust Speed"
#define STR_MOUSE_L     "LMB - Draw Cells"
#define STR_MOUSE_R     "RMB - Pan View"
#define STR_WHEEL       "Wheel - Zoom"
#define STR_NUM_KEYS    "1-9 - Preset Patterns"
#define STR_ESC         "ESC - Exit"
#define STR_PATTERNS    "Patterns"
#define STR_PAT_GLIDER  "1 - Glider"
#define STR_PAT_LWSS    "2 - LWSS"
#define STR_PAT_PULSAR  "3 - Pulsar"
#define STR_PAT_GOSPER  "4 - Gosper Gun"
#define STR_PAT_PENTA   "5 - Pentadecathlon"
#define STR_PAT_DIEHARD "6 - Diehard"
#define STR_PAT_ACORN   "7 - Acorn"
#define STR_PAT_INF     "8 - Infinite Growth"
#define STR_PAT_RANDOM  "9 - Random"
#endif

using namespace ege;

// Window dimensions
const int WINDOW_WIDTH    = 1280;
const int WINDOW_HEIGHT   = 800;
const int PANEL_WIDTH     = 200;
const int GRID_AREA_WIDTH = WINDOW_WIDTH - PANEL_WIDTH;

// Colors
const color_t LIFE_COLOR_BG         = EGERGB(20, 20, 30);
const color_t LIFE_COLOR_PANEL      = EGERGB(40, 40, 50);
const color_t LIFE_COLOR_TEXT       = EGERGB(220, 220, 220);
const color_t LIFE_COLOR_TITLE      = EGERGB(100, 200, 255);
const color_t LIFE_COLOR_GRID       = EGERGB(50, 50, 60);
const color_t LIFE_COLOR_CELL_ALIVE = EGERGB(50, 255, 100);
const color_t LIFE_COLOR_CELL_BORN  = EGERGB(100, 255, 150);
const color_t LIFE_COLOR_CELL_DYING = EGERGB(150, 100, 50);
const color_t LIFE_COLOR_TRAIL      = EGERGB(30, 60, 40);

// Grid settings
const int GRID_WIDTH  = 200;
const int GRID_HEIGHT = 150;

class GameOfLife
{
public:
    GameOfLife() :
        m_paused(true),
        m_showGrid(true),
        m_showTrail(false),
        m_generation(0),
        m_population(0),
        m_speed(10),
        m_cellSize(6.0f),
        m_offsetX(0.0f),
        m_offsetY(0.0f),
        m_isDragging(false),
        m_isDrawing(false),
        m_drawValue(true),
        m_lastMouseX(0),
        m_lastMouseY(0),
        m_frameCount(0)
    {
        m_grid.resize(GRID_WIDTH * GRID_HEIGHT, false);
        m_nextGrid.resize(GRID_WIDTH * GRID_HEIGHT, false);
        m_trailGrid.resize(GRID_WIDTH * GRID_HEIGHT, 0);
        m_prevGrid.resize(GRID_WIDTH * GRID_HEIGHT, false);

        // Center the view
        m_offsetX = (GRID_AREA_WIDTH - GRID_WIDTH * m_cellSize) / 2;
        m_offsetY = (WINDOW_HEIGHT - GRID_HEIGHT * m_cellSize) / 2;

        // Load default pattern (Gosper Glider Gun)
        loadPattern(4);
    }

    void run()
    {
        initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
        setbkmode(TRANSPARENT);
        setcaption(DEMO_TITLE);
        setbkcolor(LIFE_COLOR_BG);

        while (is_run()) {
            handleInput();

            if (!m_paused) {
                m_frameCount++;
                int updateInterval = 60 / m_speed;
                if (updateInterval < 1) {
                    updateInterval = 1;
                }

                if (m_frameCount >= updateInterval) {
                    update();
                    m_frameCount = 0;
                }
            }

            render();
            delay_fps(60);
        }

        closegraph();
    }

private:
    // Grid access helpers
    bool getCell(int x, int y) const
    {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) {
            return false;
        }
        return m_grid[y * GRID_WIDTH + x];
    }

    void setCell(int x, int y, bool alive)
    {
        if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
            m_grid[y * GRID_WIDTH + x] = alive;
        }
    }

    int countNeighbors(int x, int y) const
    {
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                if (getCell(x + dx, y + dy)) {
                    count++;
                }
            }
        }
        return count;
    }

    void update()
    {
        // Save previous state for visual effects
        m_prevGrid = m_grid;

        m_population = 0;

        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int  neighbors = countNeighbors(x, y);
                bool alive     = getCell(x, y);
                bool nextState = false;

                if (alive) {
                    // Rule 1 & 3: Die if < 2 or > 3 neighbors
                    // Rule 2: Survive if 2 or 3 neighbors
                    nextState = (neighbors == 2 || neighbors == 3);
                } else {
                    // Rule 4: Birth if exactly 3 neighbors
                    nextState = (neighbors == 3);
                }

                m_nextGrid[y * GRID_WIDTH + x] = nextState;

                if (nextState) {
                    m_population++;
                }

                // Update trail
                if (m_showTrail) {
                    if (alive) {
                        m_trailGrid[y * GRID_WIDTH + x] = 255;
                    } else if (m_trailGrid[y * GRID_WIDTH + x] > 0) {
                        m_trailGrid[y * GRID_WIDTH + x] = std::max(0, m_trailGrid[y * GRID_WIDTH + x] - 15);
                    }
                }
            }
        }

        std::swap(m_grid, m_nextGrid);
        m_generation++;
    }

    void render()
    {
        cleardevice();

        // Draw trail effect
        if (m_showTrail) {
            for (int y = 0; y < GRID_HEIGHT; y++) {
                for (int x = 0; x < GRID_WIDTH; x++) {
                    int trail = m_trailGrid[y * GRID_WIDTH + x];
                    if (trail > 0 && !m_grid[y * GRID_WIDTH + x]) {
                        float screenX = m_offsetX + x * m_cellSize;
                        float screenY = m_offsetY + y * m_cellSize;

                        if (screenX + m_cellSize >= 0 && screenX < GRID_AREA_WIDTH && screenY + m_cellSize >= 0 &&
                            screenY < WINDOW_HEIGHT)
                        {
                            int r = 30 * trail / 255;
                            int g = 60 * trail / 255;
                            int b = 40 * trail / 255;
                            setfillcolor(EGERGB(r, g, b));
                            bar((int)screenX, (int)screenY, (int)(screenX + m_cellSize - 1),
                                (int)(screenY + m_cellSize - 1));
                        }
                    }
                }
            }
        }

        // Draw grid lines
        if (m_showGrid && m_cellSize >= 4) {
            setcolor(LIFE_COLOR_GRID);
            for (int x = 0; x <= GRID_WIDTH; x++) {
                float screenX = m_offsetX + x * m_cellSize;
                if (screenX >= 0 && screenX < GRID_AREA_WIDTH) {
                    line((int)screenX, 0, (int)screenX, WINDOW_HEIGHT);
                }
            }
            for (int y = 0; y <= GRID_HEIGHT; y++) {
                float screenY = m_offsetY + y * m_cellSize;
                if (screenY >= 0 && screenY < WINDOW_HEIGHT) {
                    line(0, (int)screenY, GRID_AREA_WIDTH, (int)screenY);
                }
            }
        }

        // Draw cells
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                bool alive    = m_grid[y * GRID_WIDTH + x];
                bool wasAlive = m_prevGrid[y * GRID_WIDTH + x];

                if (alive || (wasAlive && !alive && !m_paused)) {
                    float screenX = m_offsetX + x * m_cellSize;
                    float screenY = m_offsetY + y * m_cellSize;

                    // Culling
                    if (screenX + m_cellSize < 0 || screenX > GRID_AREA_WIDTH || screenY + m_cellSize < 0 ||
                        screenY > WINDOW_HEIGHT)
                    {
                        continue;
                    }

                    color_t cellColor;
                    if (alive && !wasAlive) {
                        // Just born
                        cellColor = LIFE_COLOR_CELL_BORN;
                    } else if (!alive && wasAlive) {
                        // Just died
                        cellColor = LIFE_COLOR_CELL_DYING;
                    } else {
                        // Alive
                        cellColor = LIFE_COLOR_CELL_ALIVE;
                    }

                    setfillcolor(cellColor);

                    float margin = m_cellSize >= 6 ? 1.0f : 0.0f;
                    bar((int)(screenX + margin), (int)(screenY + margin), (int)(screenX + m_cellSize - margin - 1),
                        (int)(screenY + m_cellSize - margin - 1));
                }
            }
        }

        // Draw panel
        drawPanel();
    }

    void drawPanel()
    {
        // Panel background
        setfillcolor(LIFE_COLOR_PANEL);
        bar(GRID_AREA_WIDTH, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Border
        setcolor(EGERGB(80, 80, 90));
        line(GRID_AREA_WIDTH, 0, GRID_AREA_WIDTH, WINDOW_HEIGHT);

        int  x = GRID_AREA_WIDTH + 15;
        int  y = 20;
        char buf[64];

        // Title
        setfont(24, 0, "Consolas");
        setcolor(LIFE_COLOR_TITLE);
        outtextxy(x, y, DEMO_TITLE);
        y += 40;

        // Status
        setfont(16, 0, "Consolas");
        setcolor(m_paused ? EGERGB(255, 150, 100) : EGERGB(100, 255, 150));
        outtextxy(x, y, m_paused ? STR_PAUSED : STR_RUNNING);
        y += 30;

        // Stats
        setcolor(LIFE_COLOR_TEXT);
        sprintf(buf, STR_GENERATION, m_generation);
        outtextxy(x, y, buf);
        y += 22;

        sprintf(buf, STR_POPULATION, m_population);
        outtextxy(x, y, buf);
        y += 22;

        sprintf(buf, STR_SPEED, m_speed);
        outtextxy(x, y, buf);
        y += 22;

        sprintf(buf, STR_ZOOM, m_cellSize / 6.0f);
        outtextxy(x, y, buf);
        y += 22;

        sprintf(buf, STR_GRID, m_showGrid ? STR_ON : STR_OFF);
        outtextxy(x, y, buf);
        y += 22;

        sprintf(buf, STR_TRAIL, m_showTrail ? STR_ON : STR_OFF);
        outtextxy(x, y, buf);
        y += 40;

        // Controls
        setcolor(LIFE_COLOR_TITLE);
        outtextxy(x, y, STR_CONTROLS);
        y += 25;

        setfont(14, 0, "Consolas");
        setcolor(EGERGB(180, 180, 180));

        outtextxy(x, y, STR_SPACE);
        y += 18;
        outtextxy(x, y, STR_KEY_R);
        y += 18;
        outtextxy(x, y, STR_KEY_C);
        y += 18;
        outtextxy(x, y, STR_KEY_G);
        y += 18;
        outtextxy(x, y, STR_KEY_T);
        y += 18;
        outtextxy(x, y, STR_PLUS_MINUS);
        y += 18;
        outtextxy(x, y, STR_MOUSE_L);
        y += 18;
        outtextxy(x, y, STR_MOUSE_R);
        y += 18;
        outtextxy(x, y, STR_WHEEL);
        y += 18;
        outtextxy(x, y, STR_NUM_KEYS);
        y += 18;
        outtextxy(x, y, STR_ESC);
        y += 35;

        // Patterns
        setfont(16, 0, "Consolas");
        setcolor(LIFE_COLOR_TITLE);
        outtextxy(x, y, STR_PATTERNS);
        y += 25;

        setfont(14, 0, "Consolas");
        setcolor(EGERGB(180, 180, 180));

        outtextxy(x, y, STR_PAT_GLIDER);
        y += 18;
        outtextxy(x, y, STR_PAT_LWSS);
        y += 18;
        outtextxy(x, y, STR_PAT_PULSAR);
        y += 18;
        outtextxy(x, y, STR_PAT_GOSPER);
        y += 18;
        outtextxy(x, y, STR_PAT_PENTA);
        y += 18;
        outtextxy(x, y, STR_PAT_DIEHARD);
        y += 18;
        outtextxy(x, y, STR_PAT_ACORN);
        y += 18;
        outtextxy(x, y, STR_PAT_INF);
        y += 18;
        outtextxy(x, y, STR_PAT_RANDOM);
    }

    void handleInput()
    {
        // Keyboard input
        while (kbhit()) {
            int key = getch();
            switch (key) {
            case ' ':
                m_paused = !m_paused;
                break;

            case 'r':
            case 'R':
                randomize();
                break;

            case 'c':
            case 'C':
                clear();
                break;

            case 'g':
            case 'G':
                m_showGrid = !m_showGrid;
                break;

            case 't':
            case 'T':
                m_showTrail = !m_showTrail;
                if (!m_showTrail) {
                    std::fill(m_trailGrid.begin(), m_trailGrid.end(), 0);
                }
                break;

            case '+':
            case '=':
                m_speed = std::min(60, m_speed + 5);
                break;

            case '-':
            case '_':
                m_speed = std::max(1, m_speed - 5);
                break;

            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                loadPattern(key - '0');
                break;

            case key_esc:
                closegraph();
                exit(0);
                break;
            }
        }

        // Mouse input
        while (mousemsg()) {
            mouse_msg msg = getmouse();

            if (msg.x < GRID_AREA_WIDTH) {
                if (msg.is_left()) {
                    if (msg.is_down()) {
                        m_isDrawing = true;
                        int gridX   = (int)((msg.x - m_offsetX) / m_cellSize);
                        int gridY   = (int)((msg.y - m_offsetY) / m_cellSize);
                        if (gridX >= 0 && gridX < GRID_WIDTH && gridY >= 0 && gridY < GRID_HEIGHT) {
                            m_drawValue = !getCell(gridX, gridY);
                            setCell(gridX, gridY, m_drawValue);
                            updatePopulation();
                        }
                    } else if (msg.is_up()) {
                        m_isDrawing = false;
                    } else if (msg.is_move() && m_isDrawing) {
                        int gridX = (int)((msg.x - m_offsetX) / m_cellSize);
                        int gridY = (int)((msg.y - m_offsetY) / m_cellSize);
                        if (gridX >= 0 && gridX < GRID_WIDTH && gridY >= 0 && gridY < GRID_HEIGHT) {
                            setCell(gridX, gridY, m_drawValue);
                            updatePopulation();
                        }
                    }
                }

                if (msg.is_right()) {
                    if (msg.is_down()) {
                        m_isDragging = true;
                        m_lastMouseX = msg.x;
                        m_lastMouseY = msg.y;
                    } else if (msg.is_up()) {
                        m_isDragging = false;
                    } else if (msg.is_move() && m_isDragging) {
                        m_offsetX    += msg.x - m_lastMouseX;
                        m_offsetY    += msg.y - m_lastMouseY;
                        m_lastMouseX  = msg.x;
                        m_lastMouseY  = msg.y;
                    }
                }

                if (msg.is_wheel()) {
                    float oldCellSize  = m_cellSize;
                    float zoomFactor   = msg.wheel > 0 ? 1.2f : 0.8f;
                    m_cellSize        *= zoomFactor;
                    m_cellSize         = std::max(2.0f, std::min(30.0f, m_cellSize));

                    // Zoom towards mouse position
                    float mouseGridX = (msg.x - m_offsetX) / oldCellSize;
                    float mouseGridY = (msg.y - m_offsetY) / oldCellSize;
                    m_offsetX        = msg.x - mouseGridX * m_cellSize;
                    m_offsetY        = msg.y - mouseGridY * m_cellSize;
                }
            }
        }
    }

    void updatePopulation()
    {
        m_population = 0;
        for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
            if (m_grid[i]) {
                m_population++;
            }
        }
    }

    void clear()
    {
        std::fill(m_grid.begin(), m_grid.end(), false);
        std::fill(m_trailGrid.begin(), m_trailGrid.end(), 0);
        std::fill(m_prevGrid.begin(), m_prevGrid.end(), false);
        m_generation = 0;
        m_population = 0;
    }

    void randomize()
    {
        clear();
        for (int y = 0; y < GRID_HEIGHT; y++) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                if (rand() % 100 < 25) {
                    setCell(x, y, true);
                    m_population++;
                }
            }
        }
    }

    void loadPattern(int patternIndex)
    {
        clear();

        int centerX = GRID_WIDTH / 2;
        int centerY = GRID_HEIGHT / 2;

        // Pattern definitions (relative coordinates)
        std::vector<std::pair<int, int>> pattern;

        switch (patternIndex) {
        case 1: // Glider
            pattern = {{0, 0}, {1, 0}, {2, 0}, {2, -1}, {1, -2}};
            break;

        case 2: // Lightweight Spaceship (LWSS)
            pattern = {{0, 0}, {3, 0}, {4, 1}, {0, 2}, {4, 2}, {1, 3}, {2, 3}, {3, 3}, {4, 3}};
            break;

        case 3: // Pulsar
            pattern = {
                // Top left quadrant pattern (will be mirrored)
                {2, 0},
                {3, 0},
                {4, 0},
                {0, 2},
                {5, 2},
                {0, 3},
                {5, 3},
                {0, 4},
                {5, 4},
                {2, 5},
                {3, 5},
                {4, 5},
            };
            // Mirror the pattern
            {
                std::vector<std::pair<int, int>> fullPattern;
                for (auto& p : pattern) {
                    fullPattern.push_back({p.first, p.second});
                    fullPattern.push_back({-p.first - 1, p.second});
                    fullPattern.push_back({p.first, -p.second - 1});
                    fullPattern.push_back({-p.first - 1, -p.second - 1});
                }
                pattern = fullPattern;
            }
            break;

        case 4: // Gosper Glider Gun
            pattern = {{0, 4}, {0, 5}, {1, 4}, {1, 5}, {10, 4}, {10, 5}, {10, 6}, {11, 3}, {11, 7}, {12, 2}, {12, 8},
                {13, 2}, {13, 8}, {14, 5}, {15, 3}, {15, 7}, {16, 4}, {16, 5}, {16, 6}, {17, 5}, {20, 2}, {20, 3},
                {20, 4}, {21, 2}, {21, 3}, {21, 4}, {22, 1}, {22, 5}, {24, 0}, {24, 1}, {24, 5}, {24, 6}, {34, 2},
                {34, 3}, {35, 2}, {35, 3}};
            centerX = GRID_WIDTH / 4;
            centerY = GRID_HEIGHT / 2;
            break;

        case 5: // Pentadecathlon
            pattern = {
                {-4, 0}, {-3, 0}, {-2, -1}, {-2, 1}, {-1, 0}, {0, 0}, {1, 0}, {2, 0}, {3, -1}, {3, 1}, {4, 0}, {5, 0}};
            break;

        case 6: // Diehard
            pattern = {{0, 0}, {1, 0}, {1, 1}, {5, 1}, {6, -1}, {6, 1}, {7, 1}};
            break;

        case 7: // Acorn
            pattern = {{0, 0}, {1, -2}, {1, 0}, {3, -1}, {4, 0}, {5, 0}, {6, 0}};
            break;

        case 8: // Infinite growth (R-pentomino)
            pattern = {{0, 0}, {1, 0}, {0, 1}, {-1, 1}, {0, 2}};
            break;

        case 9: // Random
            randomize();
            return;
        }

        // Place pattern
        for (auto& p : pattern) {
            setCell(centerX + p.first, centerY + p.second, true);
        }

        updatePopulation();

        // Center view on pattern
        m_offsetX = (GRID_AREA_WIDTH - GRID_WIDTH * m_cellSize) / 2;
        m_offsetY = (WINDOW_HEIGHT - GRID_HEIGHT * m_cellSize) / 2;
    }

    std::vector<bool> m_grid;
    std::vector<bool> m_nextGrid;
    std::vector<bool> m_prevGrid;
    std::vector<int>  m_trailGrid;

    bool  m_paused;
    bool  m_showGrid;
    bool  m_showTrail;
    int   m_generation;
    int   m_population;
    int   m_speed;
    float m_cellSize;
    float m_offsetX;
    float m_offsetY;

    bool m_isDragging;
    bool m_isDrawing;
    bool m_drawValue;
    int  m_lastMouseX;
    int  m_lastMouseY;
    int  m_frameCount;
};

int main()
{
    srand((unsigned)time(NULL));
    setinitmode(INIT_ANIMATION);

    GameOfLife game;
    game.run();

    return 0;
}
