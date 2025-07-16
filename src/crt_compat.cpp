#include <cstddef>
#include <cstdio>
#include <cstdlib>

/* Dev-C++ (v5.11, TDM-GCC 4.9.2)兼容 */
#ifdef EGE_CRT_COMPAT_DEVCPP
#ifdef __GNUC__
void operator delete(void* ptr, std::size_t size) noexcept
{
    operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
    operator delete[](ptr);
}


extern "C"
{

FILE* __cdecl __imp___acrt_iob_func(unsigned i)
{
    return &__iob_func()[i];
}

}

#endif // __GNUC__
#endif // EGE_CRT_COMPAT_DEVCPP

