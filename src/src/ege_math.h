#pragma once

#include <math.h>

namespace ege
{


#if __cplusplus >= 201103L
inline int ege_round(float x)
{
    return round(x);
}
#else
int ege_round(float x);
#endif

}
