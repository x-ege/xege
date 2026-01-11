#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <mutex>
#endif

#define QUEUE_LEN 1024

namespace ege
{

#ifdef _WIN32
class Lock
{
public:
    Lock(LPCRITICAL_SECTION p_) : _psection(p_) { ::EnterCriticalSection(_psection); }

    ~Lock() { ::LeaveCriticalSection(_psection); }

private:
    LPCRITICAL_SECTION _psection;
};
#endif

template <typename T> class thread_queue
{
public:
    thread_queue(void)
    {
#ifdef _WIN32
        ::InitializeCriticalSection(&_section);
#endif
        _begin = _end = 0;
    }

    ~thread_queue(void) {
#ifdef _WIN32
        ::DeleteCriticalSection(&_section);
#endif
    }

    void push(const T& d_)
    {
#ifdef _WIN32
        Lock lock(&_section);
#else
        std::lock_guard<std::mutex> lock(_mutex);
#endif
        int  w       = (_end + 1) % QUEUE_LEN;
        _queue[_end] = d_;
        if (w == _begin) {
            _begin = (_begin + 1) % QUEUE_LEN;
        }
        _end = w;
    }

    int pop(T& d_)
    {
#ifdef _WIN32
        Lock lock(&_section);
#else
        std::lock_guard<std::mutex> lock(_mutex);
#endif
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
#ifdef _WIN32
        Lock lock(&_section);
#else
        std::lock_guard<std::mutex> lock(_mutex);
#endif
        if (_begin == (_end + 1) % QUEUE_LEN) {
            return 0;
        }
        _begin = (_begin + QUEUE_LEN - 1) % QUEUE_LEN;
        return 1;
    }

    T last() { return _last; }

    void process(void (*process_func)(T&))
    {
#ifdef _WIN32
        Lock lock(&_section);
#else
        std::lock_guard<std::mutex> lock(_mutex);
#endif
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
#ifdef _WIN32
        Lock lock(&_section);
#else
        std::lock_guard<std::mutex> lock(_mutex);
#endif
        return _begin == _end;
    }

private:
#ifdef _WIN32
    CRITICAL_SECTION _section;
#else
    std::mutex       _mutex;
#endif
    T                _queue[QUEUE_LEN];
    T                _last;
    int              _begin, _end;
};

} // namespace ege
