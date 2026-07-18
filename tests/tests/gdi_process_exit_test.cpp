#include "ege.h"

#include <cstdlib>

#ifdef _WIN32

int main()
{
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
    ege::initgraph(32, 24, mode);
    if (ege::getHWnd() == nullptr || !ege::is_run()) {
        return EXIT_FAILURE;
    }

    ege::delay_ms(1);
    ege::closegraph();

    // Deliberately return without a test-only shutdown helper. CTest's timeout
    // turns a regression in the Win32 static-destruction/UI-thread handshake
    // into a deterministic failure.
    return EXIT_SUCCESS;
}

#else

int main()
{
    return EXIT_SUCCESS;
}

#endif
