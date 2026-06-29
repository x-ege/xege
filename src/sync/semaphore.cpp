#include "semaphore.h"

namespace ege
{

bool Semaphore::acquirable()
{
    std::lock_guard lock{mut};
    return counter > 0;
}

void Semaphore::acquire()
{
    std::unique_lock lock{mut};
    cv.wait(lock, [this] { return counter > 0; });
    counter--;
}

int Semaphore::add_permit()
{
    int old;
    {
        std::lock_guard lock{mut};
        old = counter++;
    }
    cv.notify_one();
    return old;
}

} // namespace ege
