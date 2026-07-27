#pragma once

#include "ege.h"

inline ege::initmode_flag with_opengl_test_mode(ege::initmode_flag mode)
{
#if defined(_WIN32) && defined(EGE_BUILD_OPENGL)
    return static_cast<ege::initmode_flag>(mode | ege::INIT_OPENGL);
#else
    return mode;
#endif
}
