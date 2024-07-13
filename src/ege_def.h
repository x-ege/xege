#pragma once

#ifdef _MSC_VER
#define EGE_FORCEINLINE __forceinline
#else
#define EGE_FORCEINLINE __attribute__((always_inline)) inline
#endif

#if defined(_MSC_VER)
#   ifndef _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
#       define _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
#   endif
#   ifndef _ALLOW_RUNTIME_LIBRARY_MISMATCH
#       define _ALLOW_RUNTIME_LIBRARY_MISMATCH
#   endif
#endif



