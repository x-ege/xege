#pragma once

#include "ege.h"

inline ege::initmode_flag with_opengl_test_mode(ege::initmode_flag mode)
{
#ifdef _WIN32
    return static_cast<ege::initmode_flag>(mode | ege::INIT_OPENGL);
#else
    return mode;
#endif
}
