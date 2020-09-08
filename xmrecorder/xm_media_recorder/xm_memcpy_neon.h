#ifndef XM_MEMCPY_NEON_H
#define XM_MEMCPY_NEON_H
#include<string.h>

#ifdef SUPPORT_ARM_NEON
void xmmr_memcpy_neon(volatile unsigned char *dst,
        volatile unsigned char *src, int sz);
#else
void xmmr_memcpy_neon(unsigned char *dst,
        const unsigned char *src, int sz);
#endif

#endif
