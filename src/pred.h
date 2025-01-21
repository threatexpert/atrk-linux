#ifndef ATRK_common
#define ATRK_common

#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

//#define DEBUGON
#ifdef DEBUGON
 #define dbg   fprintf
#else
 #define dbg(...)
#endif



#endif /* !ATRK_common */
