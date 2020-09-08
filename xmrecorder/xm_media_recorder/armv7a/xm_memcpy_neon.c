#include "xm_memcpy_neon.h"

#ifdef SUPPORT_ARM_NEON
void xmmr_memcpy_neon(volatile unsigned char *dst, volatile unsigned char *src, int sz)
{
    if (sz & 63) {
        sz = (sz & -64) + 64;
    }

    __asm__ __volatile__ (
        "NEONCopyPLD:                          \n"
        "    VLDM %[src]!,{d0-d7}                 \n"
        "    VSTM %[dst]!,{d0-d7}                 \n"
        "    SUBS %[sz],%[sz],#0x40                 \n"
        "    BGT NEONCopyPLD                  \n"
        : [dst]"+r"(dst), [src]"+r"(src), [sz]"+r"(sz)
        :
        : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "cc", "memory"
    );
}
#else
void xmmr_memcpy_neon(unsigned char *dst, const unsigned char *src, int sz)
{
    memcpy(dst, src, sz);
}
#endif

