#include <graphics.h>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_CLICK_LABEL        "       点击: %3d"
#define TEXT_DOUBLE_CLICK_LABEL "双击: %3d"
#else
// 非MSVC编译器使用英文文案
#define TEXT_CLICK_LABEL        "       click: %3d"
#define TEXT_DOUBLE_CLICK_LABEL "double click: %3d"
#endif

int mouseKeyToIndex(int keyMask)
{
    int index = -1;
    switch(keyMask) {
    case mouse_flag_left:  index = 0; break;
    case mouse_flag_mid:   index = 1; break;
    case mouse_flag_right: index = 2; break;
    case mouse_flag_x1:    index = 3; break;
    case mouse_flag_x2:    index = 4; break;
    }
    return index;
}

int main()
{
    const int winWidth = 1200, winHeight = 300;
    initgraph(winWidth, winHeight, INIT_RENDERMANUAL);
    ege_enable_aa(true);

    setlinecolor(EGEARGB(128, 81, 128, 222));
    settextcolor(BLACK);
    setbkcolor(EGEACOLOR(255, WHITE));
    setfillcolor(EGEARGB(128, 95, 96,221));

    setfont(24, 0, "Consolas");

    const int keyNum = 5;
    int clickCount[keyNum] = {0};
    int doubleClickCount[keyNum] = {0};
    bool keyPress[keyNum] = {false};
    bool update = true;

    const char* keyName[keyNum] = {"left", "mid", "right", "x1", "x2"};

    /* ------------------------------------------ */
    for (; is_run(); delay_fps(60)) {
        while (mousemsg()) {
            mouse_msg msg = getmouse();
            int index = mouseKeyToIndex(msg.flags & 0xFF);
            if (index != -1) {
                update = true;
                if (msg.is_doubleclick()) {
                    keyPress[index] = true;
                    doubleClickCount[index]++;
                } else if (msg.is_down()) {
                    keyPress[index] = true;
                    clickCount[index]++;
                } else if (msg.is_up()) {
                    keyPress[index] = false;
                }
            }
        }

        if (update) {
            update = false;
            cleardevice();

            settextjustify(CENTER_TEXT, TOP_TEXT);

            for (int i = 0; i < keyNum; i++) {
                const float padding = 40.0f;
                const float interval = (winWidth - 2.0f * padding) / keyNum;
                float       centerX = padding + (i + 0.5) * interval ;
                float       top     = keyPress[i] ? 80.0f : 60.0f;
                const float bottom  = 100.0f;
                const float width   = 40.0f;

                float x = centerX - width / 2.0f, y = top;
                float height = bottom - top;
                ege_fillroundrect(x, y, width, height, 8.0f, 8.0f, 0.0f, 0.0f);

                xyprintf(centerX, bottom + 10, keyName[i]);

                xyprintf(centerX, bottom + 60, TEXT_CLICK_LABEL, clickCount[i]);
                xyprintf(centerX, bottom + 80, TEXT_DOUBLE_CLICK_LABEL, doubleClickCount[i]);
            }
        }
    }

    getch();

    closegraph();

}