#ifndef _LEDDEFS_H
#define _LEDDEFS_H

#include <cstdint>

typedef uint16_t color_data_t;

struct color_t {
    color_data_t r;
    color_data_t g;
    color_data_t b;
    color_data_t w;
};

#endif //_LEDDEFS_H