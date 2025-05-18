// By wysaid
// blog: http://wysaid.org
// dependency: Easy Graphics Engine: https://github.com/misakamm/xege/ or http://xege.org
// tips: 本次demo兼容vc6.0, 直接复制下方代码运行即可

#define SHOW_CONSOLE
#define _CRT_SECURE_NO_WARNINGS
#include <graphics.h>
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdio>

#define PRINT_FPS 0

#if PRINT_FPS
#include <chrono>
#endif

using namespace std;

struct Point
{
    Point() : x(0), y(0), dx(0), dy(0) {}

    Point(float _x, float _y, float _u, float _v) : x(_x), y(_y), dx(0), dy(0), u(_u), v(_v) {}

    float x, y;
    float dx, dy;
    float u, v;
};

void my_line(int* data, int width, int height, int pnt1x, int pnt1y, int pnt2x, int pnt2y, int color)
{
    int   dx   = pnt2x - pnt1x;
    int   dy   = pnt2y - pnt1y;
    float x    = pnt1x;
    float y    = pnt1y;
    int   step = 0;
    if (abs(dx) > abs(dy)) {
        step = abs(dx);
    } else {
        step = abs(dy);
    }
    float xStep = dx / (float)step;
    float yStep = dy / (float)step;
    for (int i = 0; i < step; i++) {
        x += xStep;
        y += yStep;
        if (x < 0 || y < 0 || x >= width || y >= height) {
            continue;
        }
        data[(int)x + (int)y * width] = color;
    }
}

class Net
{
public:
    Net() : m_index(0), m_intensity(0.2f), m_lastIndex(-1) {}

    ~Net() {}

    void setTextureImage(PIMAGE texture)
    {
        m_texture   = texture;
        m_texWidth  = getwidth(texture);
        m_texHeight = getheight(texture);
    }

    void setOutputTarget(PIMAGE target)
    {
        m_outputTarget = target;
        m_outputWidth  = getwidth(target);
        m_outputHeight = getheight(target);
    }

    bool initNet(int w, int h, PIMAGE inputTexture, PIMAGE outputTarget)
    {
        if (w < 2 || h < 2) {
            return false;
        }

        m_texture     = inputTexture;
        m_texWidth    = getwidth(m_texture);
        m_texHeight   = getheight(m_texture);
        m_textureData = (color_t*)getbuffer(m_texture);

        m_outputTarget = outputTarget;
        m_outputWidth  = getwidth(m_outputTarget);
        m_outputHeight = getheight(m_outputTarget);

        m_width  = w;
        m_height = h;

        m_vec[0].resize(w * h);
        m_vec[1].resize(w * h);
        float widthStep  = 1.0f / (w - 1);
        float heightStep = 1.0f / (h - 1);

        for (int i = 0; i != h; ++i) {
            const float heightI = i * heightStep;
            int         index   = w * i;
            for (int j = 0; j != w; ++j) {
                const float widthJ = j * widthStep;
                m_vec[0][index]    = Point(widthJ, heightI, widthJ, heightI);
                m_vec[1][index]    = Point(widthJ, heightI, widthJ, heightI);

                ++index;
            }
        }
        return true;
    }

    void update()
    {
        const float widthStep  = 1.0f / (m_width - 1.0f);
        const float heightStep = 1.0f / (m_height - 1.0f);
        int         index      = (m_index + 1) % 2;

        for (int i = 1; i < m_height - 1; ++i) {
            const int k = m_width * i;
            for (int j = 1; j < m_width - 1; ++j) {
                const int h = k + j;
                float     dx, dy;
                dx = (m_vec[m_index][h - 1].x + m_vec[m_index][h + 1].x - m_vec[m_index][h].x * 2.0f);
                dy = (m_vec[m_index][h - 1].y + m_vec[m_index][h + 1].y - m_vec[m_index][h].y * 2.0f);

                dx += (m_vec[m_index][h - m_width].x + m_vec[m_index][h + m_width].x - m_vec[m_index][h].x * 2.0f);
                dy += (m_vec[m_index][h - m_width].y + m_vec[m_index][h + m_width].y - m_vec[m_index][h].y * 2.0f);

                // 模拟能量损失， 当加速度方向与速度方向相反时，加快减速
                if (((unsigned&)dx >> 31) != ((unsigned&)m_vec[m_index][h].dx >> 31)) {
                    dx *= 1.0f + m_intensity;
                }

                if (((unsigned&)dy >> 31) != ((unsigned&)m_vec[m_index][h].dy >> 31)) {
                    dy *= 1.0f + m_intensity;
                }

                m_vec[m_index][h].dx += dx * m_intensity;
                m_vec[m_index][h].dy += dy * m_intensity;
                m_vec[index][h].dx    = m_vec[m_index][h].dx;
                m_vec[index][h].dy    = m_vec[m_index][h].dy;

                m_vec[index][h].x = m_vec[m_index][h].x + m_vec[index][h].dx;
                m_vec[index][h].y = m_vec[m_index][h].y + m_vec[index][h].dy;
            }
        }
        m_index = index;
    }

    void catchPoint(float x, float y)
    {
        int index;

        if (m_lastIndex < 0) {
            float mdis = 1e9f;
            for (int i = 1; i < m_height - 1; ++i) {
                const int k = m_width * i;
                for (int j = 1; j < m_width - 1; ++j) {
                    const int   h   = k + j;
                    const float dis = fabsf(x - m_vec[m_index][h].x) + fabsf(y - m_vec[m_index][h].y);
                    if (dis < mdis) {
                        index = h;
                        mdis  = dis;
                    }
                }
            }
            m_lastIndex = index;
        } else {
            index = m_lastIndex;
        }

        m_vec[0][index].x  = x;
        m_vec[0][index].y  = y;
        m_vec[1][index].x  = x;
        m_vec[1][index].y  = y;
        m_vec[0][index].dx = 0.0f;
        m_vec[0][index].dy = 0.0f;
        m_vec[1][index].dx = 0.0f;
        m_vec[1][index].dy = 0.0f;
    }

    void releasePoint() { m_lastIndex = -1; }

    template <class Type> inline void fillTriangle(const Type& v0, const Type& v1, const Type& v2)
    {
        if (v0.y == v2.y) {
            _fillSimpleTriangle(v0, v1, v2);
        } else if (v1.y == v2.y) {
            _fillSimpleTriangle(v1, v0, v2);
        } else if (v0.y == v1.y) {
            _fillSimpleTriangle(v0, v2, v1);
        } else {
            _fillNormalTriangle(v0, v1, v2);
        }
    }

    template <class Type> inline void _fillSimpleTriangle(const Type& vv0, const Type& v1, const Type& vv2)
    {
        assert(vv0.y == vv2.y);
        bool        isOK = (vv0.x < vv2.x);
        const Type& v0   = isOK ? vv0 : vv2;
        const Type& v2   = isOK ? vv2 : vv0;

        float h = v1.y - v0.y;

        float dL = (v1.x - v0.x) / h;
        float dR = (v1.x - v2.x) / h;

        float dUL = (v1.u - v0.u) / h;
        float dUR = (v1.u - v2.u) / h;

        float dVL = (v1.v - v0.v) / h;
        float dVR = (v1.v - v2.v) / h;

        float xL = v0.x, xR = v2.x;
        float uL = v0.u, uR = v2.u;
        float vL = v0.v, vR = v2.v;

        const color_t* data         = m_textureData;
        color_t*       outputBuffer = (color_t*)getbuffer(m_outputTarget);

        if (v0.y < v1.y) {
            for (int i = v0.y; i < v1.y; ++i) {
                float len  = xR - xL;
                float uLen = uR - uL;
                float vLen = vR - vL;
                for (int j = xL; j < xR; ++j) {
                    float percent = (j - xL) / len;
                    float u       = uL + uLen * percent;
                    float v       = vL + vLen * percent;
                    if (u < 0 || v < 0 || u > 1 || v > 1 || i < 0 || j < 0 || i >= m_outputHeight || j >= m_outputWidth)
                    {
                        continue;
                    }

                    int ww                    = u * (m_texWidth - 1);
                    int hh                    = v * (m_texHeight - 1);
                    int index                 = ww + hh * m_texWidth;
                    int outputIndex           = j + i * m_outputWidth;
                    outputBuffer[outputIndex] = data[index];
                }
                xL += dL;
                xR += dR;
                uL += dUL;
                uR += dUR;
                vL += dVL;
                vR += dVR;
            }
        } else {
            for (int i = v0.y; i > v1.y; --i) {
                float len  = xR - xL;
                float uLen = uR - uL;
                float vLen = vR - vL;
                for (int j = xL; j < xR; ++j) {
                    float percent = (j - xL) / len;
                    float u       = uL + uLen * percent;
                    float v       = vL + vLen * percent;
                    if (u < 0 || v < 0 || u > 1 || v > 1 || i < 0 || j < 0 || i >= m_outputHeight || j >= m_outputWidth)
                    {
                        continue;
                    }
                    int ww                              = u * (m_texWidth - 1);
                    int hh                              = v * (m_texHeight - 1);
                    int index                           = ww + hh * m_texWidth;
                    outputBuffer[j + i * m_outputWidth] = data[index];
                }
                xL -= dL;
                xR -= dR;
                uL -= dUL;
                uR -= dUR;
                vL -= dVL;
                vR -= dVR;
            }
        }
    }

    template <class Type> inline void _fillNormalTriangle(const Type& v0, const Type& v1, const Type& v2)
    {
        const Type* pnts[] = {&v0, &v1, &v2};

        if ((*pnts[0]).y > (*pnts[1]).y) {
            std::swap(pnts[0], pnts[1]);
        }

        if ((*pnts[0]).y > (*pnts[2]).y) {
            std::swap(pnts[0], pnts[2]);
        }

        if ((*pnts[1]).y > (*pnts[2]).y) {
            std::swap(pnts[1], pnts[2]);
        }

        const Type &vv0 = *pnts[0], &vv1 = *pnts[1], &vv2 = *pnts[2];

        Type newPoint;

        float percent = (vv1.y - vv0.y) / (vv2.y - vv0.y);

        newPoint.x = floorf(vv0.x + (vv2.x - vv0.x) * percent);
        newPoint.y = vv1.y;
        newPoint.u = vv0.u + (vv2.u - vv0.u) * percent;
        newPoint.v = vv0.v + (vv2.v - vv0.v) * percent;

        _fillSimpleTriangle(newPoint, vv0, vv1);
        _fillSimpleTriangle(newPoint, vv2, vv1);
    }

    void drawNet()
    {
        std::vector<Point>& vec = m_vec[m_index];
        int                 sz  = vec.size();
        int                 i; // i变量前置, 方便vc6.0 编译
        m_pointCache.resize(sz);
#if _MSC_VER < 1600 // 兼容vc6.0
        Point* v = &m_pointCache[0];
        memcpy(v, &vec[0], sz * sizeof(vec[0]));
#else
        Point* v = m_pointCache.data();
        memcpy(v, vec.data(), sz * sizeof(vec[0]));
#endif

        for (i = 0; i != sz; ++i) {
            v[i].x = floorf(v[i].x * m_outputWidth);
            v[i].y = floorf(v[i].y * m_outputHeight);
        }

        for (i = 1; i != m_height; ++i) {
            const int k1 = (i - 1) * m_width;
            const int k2 = i * m_width;

            for (int j = 1; j != m_width; ++j) {
                const int p1 = k1 + j - 1;
                const int p2 = k1 + j;
                const int p3 = k2 + j - 1;
                const int p4 = k2 + j;

                fillTriangle(v[p1], v[p2], v[p3]);
                fillTriangle(v[p3], v[p2], v[p4]);
            }
        }

        color_t* outputBuffer = (color_t*)getbuffer(m_outputTarget);

        if (m_lastIndex > 0) {
            for (i = 0; i != m_height; ++i) {
                const int k = i * m_width;
                for (int j = 1; j != m_width; ++j) {
                    const int h = k + j;
                    // line(v[h - 1].x, v[h - 1].y, v[h].x, v[h].y, m_outputTarget);
                    my_line((int*)outputBuffer, m_outputWidth, m_outputHeight, v[h - 1].x, v[h - 1].y, v[h].x, v[h].y,
                        0x00ffff00);
                }
            }

            for (i = 0; i != m_width; ++i) {
                for (int j = 1; j != m_height; ++j) {
                    const int h2 = j * m_width + i;
                    const int h1 = (j - 1) * m_width + i;
                    // line(v[h1].x, v[h1].y, v[h2].x, v[h2].y, m_outputTarget);
                    my_line((int*)outputBuffer, m_outputWidth, m_outputHeight, v[h1].x, v[h1].y, v[h2].x, v[h2].y,
                        0x00ffff00);
                }
            }
        }
    }

    void intensityInc(float f)
    {
        m_intensity += f;
        if (m_intensity > 0.3f) {
            m_intensity = 0.3f;
        }
    }

    void intensityDec(float f)
    {
        m_intensity -= f;
        if (m_intensity < 0.001f) {
            m_intensity = 0.001f;
        }
    }

    float getIntensity() { return m_intensity; }

private:
    vector<Point> m_vec[2];
    vector<Point> m_pointCache;
    int           m_index;
    int           m_width, m_height;
    float         m_intensity;
    int           m_lastIndex;

    PIMAGE   m_texture;
    int      m_texWidth, m_texHeight;
    color_t* m_textureData;

    PIMAGE m_outputTarget;
    int    m_outputWidth, m_outputHeight;
};

bool readFileNameDlg(LPSTR filename, LPCSTR title)
{
    OPENFILENAMEA ofna;
    *filename = 0;
    memset(&ofna, 0, sizeof(OPENFILENAMEA));
    ofna.lStructSize = sizeof(OPENFILENAMEA);
    ofna.hwndOwner   = getHWnd();
    ofna.hInstance   = getHInstance();
    ofna.lpstrFilter = "Image Files(*.jpg;*.png;*.bmp;*.gif)\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All Files(*.*)\0*.*\0\0";
    ofna.nMaxFile    = MAX_PATH;
    ofna.lpstrDefExt = "jpg";
    ofna.lpstrFile   = filename;
    ofna.lpstrTitle  = title;
    ofna.Flags       = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
    return !!GetOpenFileNameA(&ofna);
}

PIMAGE loadTexture(const char* filename)
{
    char   buffer[1024] = {0};
    PIMAGE pimg         = NULL;

    if (filename != NULL && *filename != '\0') {
        strcpy(buffer, filename);
    }

    setcolor(RED);
    outtextxy(100, 100, "按任意键选择一张图片才能进入下一步");

    do {
        getch();

        if (*buffer == '\0') {
            readFileNameDlg(buffer, "Please choose an image file!");
        }

        if (*buffer != '\0') {
            pimg    = newimage();
            int ret = getimage(pimg, buffer);
            if (ret != 0) {
                delimage(pimg);
                pimg = NULL;
            }
        }
    } while (pimg == NULL);

    return pimg;
}

int main(int argv, char** argc)
{
    const char* showMsgRule = "使用鼠标拖动可变换网格. 当前网格强度：%g";
    const char* infoMsg = "按'+'或者'-'可以增大或者减小网格弹力！这个版本由wysaid制作， 参见: http://blog.wysaid.org";
    const char* titleMsg = "EGE网格 By wysaid - 2024";

    initgraph(800, 600, INIT_RENDERMANUAL);
    setcaption(titleMsg);

    Net    net;
    char   buffer[1024];
    PIMAGE pimg   = loadTexture(argv > 1 ? argc[1] : NULL);
    PIMAGE target = newimage(getwidth(), getheight());

    setcolor(YELLOW, target);
    sprintf(buffer, showMsgRule, net.getIntensity());
    net.initNet(80, 60, pimg, target);

#if PRINT_FPS
    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();
    int                                            frames   = 0;
#endif

    for (; is_run(); delay_fps(60)) {
        cleardevice();

        if (keystate(key_mouse_l)) {
            int x, y;
            mousepos(&x, &y);
            net.catchPoint(x / 800.0f, y / 600.0f);
        } else {
            net.releasePoint();
            //flushmouse();
        }

        if (kbhit()) {
            switch (getch()) {
            case '+':
                net.intensityInc(0.005f);
                break;
            case '-':
                net.intensityDec(0.005f);
                break;
            case 27:
                exit(0);
            }
            flushkey();
            sprintf(buffer, showMsgRule, net.getIntensity());
        }

        net.drawNet();
        net.update();
        putimage(0, 0, target);
        setcolor(0x00ff0000);
        outtextxy(10, 10, infoMsg);
        outtextxy(10, 30, buffer);

#if PRINT_FPS

        ++frames;
        std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
        auto                                           dur         = (currentTime - lastTime).count() / 1.e6;
        if (dur >= 1000) {
            lastTime = currentTime;
            printf("FPS: %d\n", frames);
            frames = 0;
        }
#endif
    }

    delimage(pimg);
    delimage(target);
    closegraph();
    return 0;
}
