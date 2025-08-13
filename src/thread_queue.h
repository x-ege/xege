#pragma once

#include <atomic>

#define QUEUE_LEN 1024

namespace ege
{

/** @note 拷贝/移动不安全 */
template <typename T> class thread_queue
{
public:
    thread_queue(void) : _begin{0}, _end{0}, _unoccupied(0), _head_locked{false}, _last{nullptr} {}

    ~thread_queue(void) {}

    void push(const T& d_)
    {
        int e = _end.load();
        int w;

        do {
            w = (e + 1) % QUEUE_LEN;

            int wi = (w + 1) % QUEUE_LEN;

            // 若队列满，强制从队头清理一个元素
            // 并发访问时，只要有一个线程清理就可以了，因此只需要CAS一次，失败直接跳过计科
            if (_unoccupied.compare_exchange_strong(wi, (_unoccupied.load() + 1) % QUEUE_LEN)) {
                int b = _begin.load();
                int ww;
                do {
                    ww = (b + 1) % QUEUE_LEN;
                } while (!_begin.compare_exchange_strong(b, ww));
            }

            // 占据当前元素
        } while (!_end.compare_exchange_weak(e, w));

        _queue[e] = d_;
    }

    int pop(T& d_)
    {
        // process 遍历 _begin 元素时自旋等待
        while (_head_locked.load()) {
        }

        // 无需判断 _begin ，因为只能从 _unoccupied 开始 pop 元素
        if (_end.load() == _unoccupied.load()) {
            return 0;
        }

        int u = _unoccupied.load();
        int w;

        do {
            w = (u + 1) % QUEUE_LEN;
        } while (!_unoccupied.compare_exchange_weak(u, w));

        _last = &_queue[u];
        d_    = *_last;

        int b = _begin.load();
        do {
            w = (b + 1) % QUEUE_LEN;
        } while (!_begin.compare_exchange_weak(b, w));
        return 1;
    }

    int unpop()
    {
        // process 遍历 _begin 元素时自旋等待
        while (_head_locked.load()) {
        }

        if (_begin.load() == (_end.load() + 1) % QUEUE_LEN) {
            return 0;
        }

        int u = _unoccupied.load();
        int w;
        do {
            w = (u + QUEUE_LEN - 1) % QUEUE_LEN;
        } while (!_unoccupied.compare_exchange_weak(u, w));

        u = _begin.load();
        do {
            w = (u + QUEUE_LEN - 1) % QUEUE_LEN;
        } while (!_begin.compare_exchange_weak(u, w));

        return 1;
    }

    // 获取上次 pop 时得到的元素
    T last() { return *_last; }

    void process(void (*process_func)(T&))
    {
        int r = _begin.load();
        int w = _end.load();
        if (r != w) {
            if (w < r) {
                w += QUEUE_LEN;
            }
            for (; r < w; r++) {
                bool l_head_locked{false};
                if (r == _begin.load()) {
                    for (bool b = false; !_head_locked.compare_exchange_weak(b, true); b = false) {
                    }
                    l_head_locked = true;
                }
                process_func(_queue[r % QUEUE_LEN]);
                if (l_head_locked) {
                    _head_locked.store(false);
                }
            }
        }
    }

    bool empty() { return _begin.load() == _end.load(); }

private:
    T               _queue[QUEUE_LEN];
    T*              _last; // 上次 pop 操作时获得的元素
    std::atomic_int _begin, _end;
    // 对于 _begin 端，pop 时先将这个值向后移动，将值 pop 出来，最后后移 _begin，
    // 这样并发进入 pop() 函数时也可以并发地将每个值 pop 出来而不用阻塞。
    std::atomic_int  _unoccupied;
    std::atomic_bool _head_locked;
};

} // namespace ege
