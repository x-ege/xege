#ifndef EGE_TESTS_SUPPORT_DEMO_VALIDATION_INITGRAPH_H
#define EGE_TESTS_SUPPORT_DEMO_VALIDATION_INITGRAPH_H

#include "ege.h"

namespace ege
{

inline initmode_flag ege_demo_validation_mode(initmode_flag mode)
{
    int value = static_cast<int>(mode) & ~static_cast<int>(INIT_WITHLOGO);

#if defined(EGE_DEMO_VALIDATION_OPENGL)
    #if !defined(EGE_BUILD_OPENGL)
        #error "OpenGL demo validation requires an OpenGL-enabled EGE build"
    #endif
    value |= static_cast<int>(INIT_OPENGL);
#elif defined(EGE_DEMO_VALIDATION_GDI) && defined(EGE_BUILD_OPENGL)
    value &= ~static_cast<int>(INIT_OPENGL);
#endif

    return static_cast<initmode_flag>(value);
}

inline void ege_demo_validation_initgraph(int width, int height)
{
    initgraph(width, height, ege_demo_validation_mode(getinitmode()));
}

inline void ege_demo_validation_initgraph(int width, int height, initmode_flag mode)
{
    initgraph(width, height, ege_demo_validation_mode(mode));
}

inline void ege_demo_validation_initgraph(int width, int height, int mode)
{
    ege_demo_validation_initgraph(width, height, static_cast<initmode_flag>(mode));
}

} // namespace ege

// This header is force-included before each demo translation unit.  Replacing
// only the call-site token also handles qualified calls such as ege::initgraph.
#define initgraph ege_demo_validation_initgraph

#endif
