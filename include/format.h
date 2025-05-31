#pragma once
#include <stdint.h>

const size_t k_max_msg = 4096;

enum TypeTag : uint8_t {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};

enum ErrorCode : uint8_t {
    ERR_NOTFOUND,
    ERR_INVALID,
    ERR_OVERSIZED,
};

enum StatusCode : uint32_t {
    RES_OK,
    RES_NOTFOUND,
    RES_ERR,
};


