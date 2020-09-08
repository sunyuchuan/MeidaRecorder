#include "xm_memcpy_neon.h"

#ifdef SUPPORT_ARM_NEON
void xmmr_memcpy_neon(volatile unsigned char *dst, volatile unsigned char *src, int sz)
{
    int d0, d1, d2;

    if (sz & 31) {
        sz = (sz & -32) + 32;
    }

    __asm__ __volatile__ (
        "rep ; movsl\n\t"
        "movl %4,%%ecx\n\t"
        "andl $3,%%ecx\n\t"
        "jz 1f\n\t"
        "rep ; movsb\n\t"
        "1:"
        : "=&c" (d0), "=&D" (d1), "=&S" (d2)
        : "0" (sz / 4), "g" (sz), "1" ((long)dst), "2" ((long)src)
        : "memory"
    );
}
#else
void xmmr_memcpy_neon(unsigned char *dst, const unsigned char *src, int sz)
{
    memcpy(dst, src, sz);
}
#endif

