#pragma once
#include <stdint.h>

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})
    
inline uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

inline uint32_t min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

inline size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}
