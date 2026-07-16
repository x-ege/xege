#pragma once

#include <condition_variable>
#include <mutex>

namespace ege
{

class Semaphore
{
private:
    int                     counter;
    std::condition_variable cv;
    std::mutex              mut;

public:
    Semaphore(int c) : counter{c} {}

    bool acquirable();
    void acquire();

    int add_permit();
};

} // namespace ege
