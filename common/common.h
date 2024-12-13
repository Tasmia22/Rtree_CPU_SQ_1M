#ifndef __COMMON_H__
#define __COMMON_H__

#define XSTR(x) STR(x)
#define STR(x) #x


#define BLOCK_SIZE (256)
//#define NR_DPUS 3
static inline uint32_t ceil8(uint32_t a) {
    return (a + 0x07) & ~(0x07UL);
}
static inline uint32_t pceil8(uint32_t *a) {
    return (*a + 0x07) & ~(0x07UL);
}

#define BUNDLEFACTOR 30 // Number of points to form a leaf node
#define FANOUT 32   // Number of children per non-leaf node
#define MAX_POINTS 10000
#define MAX_NODES 10000

//#define ELEMENT_SIZE sizeof(uint32_t)

/* Structure used by both the host and the DPU to communicate results */
#include <stdint.h>
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_LIGHT_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#endif /* __COMMON_H__ */
