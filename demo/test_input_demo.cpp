#include <ege.h>
#include <cstring>
#include <vector>
#include <string>
#include <cstdio>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_CAPTION_UTF8  "ege 输入演示 - UTF-8"
#define TEXT_CAPTION_UTF16 "ege 输入演示 - UTF-16"
#define TEXT_CAPTION_ANSI  "ege 输入演示 - ANSI"
#else
// 非MSVC编译器使用英文文案
#define TEXT_CAPTION_UTF8  "ege input demo - UTF-8"
#define TEXT_CAPTION_UTF16 "ege input demo - UTF-16"
#define TEXT_CAPTION_ANSI  "ege input demo - ANSI"
#endif

#define CHAR_MSG_MBCS   0
#define CHAR_MSG_UTF_16 1

#define FONT_HEIGHT 20
#define FONT_WIDTH  10
#define WIDTH       480
#define HEIGHT      360

int main(int argc, char** argv)
{
    int mode = CHAR_MSG_MBCS;
    if (argc > 1 && strcmp(argv[1], "--utf-8") == 0) {
        ege::setcaption(TEXT_CAPTION_UTF8);
        ege::setcodepage(EGE_CODEPAGE_UTF8);
        // ege::setunicodecharmessage(false);
    } else if (argc > 1 && strcmp(argv[1], "--utf-16") == 0) {
        ege::setcaption(TEXT_CAPTION_UTF16);
        // ege::setcodepage(EGE_CODEPAGE_ANSI);
        ege::setunicodecharmessage(true);
        mode = CHAR_MSG_UTF_16;
    } else {
        // ege::setcodepage(EGE_CODEPAGE_ANSI);
        ege::setcaption(TEXT_CAPTION_ANSI);
        // ege::setunicodecharmessage(false);
    }

    ege::initgraph(WIDTH, HEIGHT);
    ege::setfont(FONT_HEIGHT, FONT_WIDTH, "SimHei");

    std::vector<std::vector<char> >    input_strings;
    std::vector<std::vector<wchar_t> > input_strings_w;

    while (ege::is_run()) {
        std::vector<wchar_t> chars;

        while (ege::kbmsg()) {
            ege::key_msg kmsg = ege::getkey();
            if (kmsg.msg == ege::key_msg_char) {
                chars.push_back(kmsg.key);
            }
        }

        // print to console
        if (!chars.empty()) {
            for (int i = 0; i < chars.size(); ++i) {
                printf("%d", chars[i]);
                if (i != chars.size() - 1) {
                    printf(", ");
                }
            }
            printf("\n");

            if (mode == CHAR_MSG_UTF_16) {
                chars.push_back(0);
                input_strings_w.push_back(chars);
            } else {
                std::vector<char> str;
                for (int i = 0; i < chars.size(); ++i) {
                    str.push_back((char)chars[i]);
                }
                str.push_back(0);
                input_strings.push_back(str);
            }
        }

        // draw to screen
        ege::cleardevice();
        if (mode == CHAR_MSG_UTF_16) {
            if (input_strings_w.size() * FONT_HEIGHT > HEIGHT) {
                input_strings_w.erase(input_strings_w.begin());
            }
            for (int i = 0; i < input_strings_w.size(); ++i) {
                ege::outtextxy(5, i * FONT_HEIGHT, &input_strings_w[i][0]);
            }
        } else {
            if (input_strings.size() * FONT_HEIGHT > HEIGHT) {
                input_strings.erase(input_strings.begin());
            }
            for (int i = 0; i < input_strings.size(); ++i) {
                ege::outtextxy(5, i * FONT_HEIGHT, &input_strings[i][0]);
            }
        }

        ege::delay_fps(60);
    }
    return 0;
}