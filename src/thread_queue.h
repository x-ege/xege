#pragma once

#include <mutex>

#define QUEUE_LEN 1024

namespace ege
{

template <typename T> class thread_queue
{
public:
    thread_queue(void) { _begin = _end = 0; }

    void push(const T& d_)
    {
        std::lock_guard g{_mutex};
        int  w       = (_end + 1) % QUEUE_LEN;
        _queue[_end] = d_;
        if (w == _begin) {
            _begin = (_begin + 1) % QUEUE_LEN;
        }
        _end = w;
    }

    int pop(T& d_)
    {
        std::lock_guard g{_mutex};
        if (_end == _begin) {
            return 0;
        }
        d_     = _queue[_begin];
        _last  = d_;
        _begin = (_begin + 1) % QUEUE_LEN;
        return 1;
    }

    int unpop()
    {
        std::lock_guard g{_mutex};
        if (_begin == (_end + 1) % QUEUE_LEN) {
            return 0;
        }
        _begin = (_begin + QUEUE_LEN - 1) % QUEUE_LEN;
        return 1;
    }

    T last() { return _last; }

    void process(void (*process_func)(T&))
    {
        std::lock_guard g{_mutex};
        int  r = _begin;
        int  w = _end;
        if (r != w) {
            if (w < r) {
                w += QUEUE_LEN;
            }
            for (; r < w; r++) {
                int pos = r % QUEUE_LEN;
                process_func(_queue[pos]);
            }
        }
    }

    bool empty()
    {
        std::lock_guard g{_mutex};
        return _begin == _end;
    }

private:
    std::mutex       _mutex;
    T                _queue[QUEUE_LEN];
    T                _last;
    int              _begin, _end;
};

} // namespace ege
