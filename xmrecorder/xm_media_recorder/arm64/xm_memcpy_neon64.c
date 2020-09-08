#include "xm_memcpy_neon.h"

#ifdef SUPPORT_ARM_NEON
void xmmr_memcpy_neon(volatile unsigned char *dst, volatile unsigned char *src, int sz)
{
    if (sz & 31) {
        sz = (sz & -32) + 32;
    }

    __asm__ __volatile__ (
        "NEONCopy:            \n"
        "    ldp q0, q1, [%[src]], #32	 \n"  // load 32
        "    subs %[sz],%[sz], #32    \n"  // 32 processed per loop
        "    stp q0, q1, [%[dst]], #32	 \n"  // store 32
        "    b.gt NEONCopy \n"
        : [dst]"+r"(dst),   // %0
          [src]"+r"(src),   // %1
          [sz]"+r"(sz)   // %2  // Output registers
        :                             // Input registers
        : "v0", "v1", "cc", "memory"  // Clobber List
    );
}
#else
void xmmr_memcpy_neon(unsigned char *dst, const unsigned char *src, int sz)
{
    memcpy(dst, src, sz);
}
#endif

