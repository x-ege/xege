#include "ege_head.h"
#include "ege_common.h"
#include "ege_extension.h"

#ifdef _WIN32
#include <mmsystem.h>
#include <digitalv.h>
#else
#include <unistd.h>
#endif
#include <algorithm>
#include <chrono>
#include <thread>

namespace ege
{

void api_sleep(long ms)
{
    if (ms >= 0) {
#ifdef _WIN32
        dll::timeBeginPeriod(1);
        ::Sleep(ms);
        dll::timeEndPeriod(1);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
    }
}

void ege_sleep(long ms)
{
    if (ms <= 0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{ms});
}

void delay(long ms)
{
    ege_sleep(ms);
}

/**
 * 刷新后延时。（刷新：处理事件以及交换双缓冲）
 * @param ms 延时时间，单位为毫秒
 */
void delay_ms(long ms)
{
    struct _graph_setting* pg = &graph_setting;
    egeControlBase* root = pg->egectrl_root;
    pg->skip_timer_mark = true;

    const double targetTime = get_highfeq_time_ls() * 1000.0 + ms;

    /* 处理 UI 事件，更新 UI 控件数据 */
    guiupdate(pg, root);

    /* 绘图后重绘UI并交换缓冲区 */
    const bool updateNeeded = needToUpdate(pg);
    if (root && updateNeeded) {
        root->draw(NULL);
    }

#ifdef _WIN32
    if (pg->window == NULL) {
        // The legacy GDI backend already owns a dedicated Win32 message
        // thread. Preserve its established refresh/sleep timing instead of
        // pumping the new drawing-thread Window abstraction.
        if (root && updateNeeded) {
            graphupdate(pg);
        }
        if (ms == 0) {
            std::this_thread::yield();
        } else if (ms > 0) {
            const double currentTime = get_highfeq_time_ls() * 1000.0;
            if (currentTime < targetTime) {
                ege_sleep(static_cast<long>(targetTime - currentTime));
            }
        }
        pg->skip_timer_mark = false;
        return;
    }
#endif

    dealmessage(pg, FORCE_UPDATE);

    /* 延时 */
    if (ms == 0) {
        /* 让出 CPU 时间片，处理后立即返回 */
        std::this_thread::yield();
    } else if (ms > 0) {
        // Native window backends run their event loop on the drawing thread.
        // Wait in short slices so close, keyboard and mouse events stay live.
        for (double currentTime = get_highfeq_time_ls() * 1000.0;
             currentTime < targetTime && is_run();
             currentTime = get_highfeq_time_ls() * 1000.0) {
            const long remaining = static_cast<long>(targetTime - currentTime);
            if (remaining > 0) {
                ege_sleep(std::min(remaining, 10L));
            } else {
                std::this_thread::yield();
            }
            dealmessage(pg, false);
            guiupdate(pg, root);
        }
    } else {
        /* ms < 0: Do nothing.*/
    }

    pg->skip_timer_mark = false;
}

/*
延迟1/fps的时间，调用间隔不大于200ms时能保证每秒能返回fps次
*/
void delay_fps(int fps)
{
    delay_fps((double)fps);
}

void delay_fps(long fps)
{
    delay_fps((double)fps);
}

void delay_fps(double fps)
{
    struct _graph_setting* pg = &graph_setting;
    egeControlBase* root = pg->egectrl_root;
    pg->skip_timer_mark = true;
    double delay_time = 1000.0 / fps;
    double avg_max_time = delay_time * 10.0; // 误差时间在这个数值以内做平衡
    double dw                   = get_highfeq_time_ls() * 1000.0;
    int nloop = 0;

    if (pg->delay_fps_dwLast == 0) {
        pg->delay_fps_dwLast = get_highfeq_time_ls() * 1000.0;
    }

    if (pg->delay_fps_dwLast + delay_time + avg_max_time > dw) {
        dw = pg->delay_fps_dwLast;
    }

    root->draw(NULL);

    for (; nloop >= 0; --nloop) {
        if ((dw + delay_time + (100.0) >= get_highfeq_time_ls() * 1000.0)) {
            do {
                ege_sleep((int)(dw + delay_time - get_highfeq_time_ls() * 1000.0));
            } while (dw + delay_time >= get_highfeq_time_ls() * 1000.0);
        }

        dealmessage(pg, FORCE_UPDATE);
        dw = get_highfeq_time_ls() * 1000.0;
        guiupdate(pg, root);

        if (pg->delay_fps_dwLast + delay_time + avg_max_time <= dw || pg->delay_fps_dwLast > dw) {
            pg->delay_fps_dwLast = dw;
        } else {
            pg->delay_fps_dwLast += delay_time;
        }
    }
    pg->skip_timer_mark = false;
}

/*
延迟1/fps的时间，调用间隔不大于200ms时能保证每秒能返回fps次
*/
void delay_jfps(int fps)
{
    delay_jfps((double)fps);
}

void delay_jfps(long fps)
{
    delay_jfps((double)fps);
}

void delay_jfps(double fps)
{
    struct _graph_setting* pg = &graph_setting;
    egeControlBase* root = pg->egectrl_root;
    pg->skip_timer_mark = true;
    double delay_time = 1000.0 / fps;
    double avg_max_time = delay_time * 10.0;
    double dw                   = get_highfeq_time_ls() * 1000.0;
    int nloop = 0;

    if (pg->delay_fps_dwLast == 0) {
        pg->delay_fps_dwLast = get_highfeq_time_ls() * 1000.0;
    }

    if (pg->delay_fps_dwLast + delay_time + avg_max_time > dw) {
        dw = pg->delay_fps_dwLast;
    }

    root->draw(NULL);

    for (; nloop >= 0; --nloop) {
        int bSleep = 0;

        while (dw + delay_time >= get_highfeq_time_ls() * 1000.0) {
            ege_sleep((int)(dw + delay_time - get_highfeq_time_ls() * 1000.0));
            bSleep = 1;
        }

        if (bSleep) {
            dealmessage(pg, FORCE_UPDATE);
        } else {
            updateFrameRate(false);
        }

        dw = get_highfeq_time_ls() * 1000.0;
        guiupdate(pg, root);

        if (pg->delay_fps_dwLast + delay_time + avg_max_time <= dw || pg->delay_fps_dwLast > dw) {
            pg->delay_fps_dwLast = dw;
        } else {
            pg->delay_fps_dwLast += delay_time;
        }
    }
    pg->skip_timer_mark = false;
}

double get_highfeq_time_ls()
{
    using namespace std::chrono;
    static auto start_time_point = steady_clock::now();

    auto durationTime = steady_clock::now() - start_time_point;
    return duration<double>(durationTime).count();
}


} //namespace ege
