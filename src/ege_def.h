#pragma once

#ifdef _MSC_VER
#define EGE_FORCEINLINE __forceinline
#else
#define EGE_FORCEINLINE __attribute__((always_inline)) inline
#endif





