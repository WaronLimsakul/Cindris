#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "buffer.h"

Buffer::Buffer() {
    uint8_t *begin = new uint8_t[buff_min_len];
    buff_begin = begin;
    buff_end = begin + buff_min_len;
    data_begin = begin;
    data_end = begin;
}

Buffer::~Buffer() {
    delete[] buff_begin;
}

uint8_t *Buffer::data() {
    return data_begin;
}

void Buffer::append(uint8_t src[], size_t len) {
    size_t back_space = buff_end - data_end;
    // normal append if back space is enough
    if (back_space >= len) {
        memcpy(data_end, src, len);
        data_end += len;
        return;
    }

    // move to front
    size_t front_space = data_begin - buff_begin;
    size_t data_len = data_end - data_begin;
    if (front_space > 0) {
        // don't use memcpy with overlap regions, it copy in optimal
        // order, and will make it not safe. Use memmove() instead.
        memmove(buff_begin, data_begin, data_len);
        data_begin -= front_space;
        data_end -= front_space;
    }

    // try normal append again
    back_space = buff_end - data_end;
    if (back_space >= len) {
        memcpy(data_end, src, len);
        data_end += len;
        return;
    }

    // ok, realloc then
    buff_begin = (uint8_t *)realloc(buff_begin, data_len + len);
    data_begin = buff_begin;
    data_end = data_begin + data_len;
    buff_end = buff_begin + data_len + len;

    // finally, we can append
    memcpy(data_end, src, len);
    data_end += len;
}

void Buffer::consume(size_t len) {
    assert(data_begin + len <= data_end);
    data_begin += len;
}

size_t Buffer::size() {
    return data_end - data_begin;
}

uint8_t Buffer::operator[](int at) {
    return *(data_begin + at);
}
