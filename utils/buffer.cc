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

// consume data from the front
void Buffer::consume(size_t len) {
    assert(data_begin + len <= data_end);
    data_begin += len;
}

void Buffer::consume_back(size_t len) {
    assert(data_end - len >= data_begin);
    data_end -= len;
}

size_t Buffer::size() {
    return data_end - data_begin;
}

uint8_t& Buffer::at(size_t idx) {
    return *(data_begin + idx);
}

// return actual value at data_begin + at
uint8_t& Buffer::operator[](size_t at) {
    return *(data_begin + at);
}

void Buffer::out_nil() {
    uint8_t tag = TAG_NIL;
    append(&tag, 1);
}

void Buffer::out_err(ErrorCode code) {
    uint8_t tag = TAG_ERR;
    uint8_t code_u8 = code;
    append(&tag, 1);
    append(&code_u8, 1);
}

void Buffer::out_str(char *data, size_t len) {
    uint8_t tag = TAG_STR;
    append(&tag, 1);
    append((uint8_t *)&len, 4);
    append((uint8_t *)data, len);
}

void Buffer::out_int(int64_t data) {
    uint8_t tag = TAG_INT;
    append(&tag, 1);
    append((uint8_t *)&data, 8);
}

void Buffer::out_dbl(double data) {
    uint8_t tag = TAG_DBL;
    append(&tag, 1);
    append((uint8_t *)&data, sizeof(double));
}

void Buffer::out_array(uint32_t n) {
    uint8_t tag = TAG_ARR;
    append(&tag, 1);
    append((uint8_t *)&n, 4);
}

// begin array header, return the header idx for arr_end
size_t Buffer::arr_begin() {
    uint8_t tag = TAG_ARR;
    append(&tag, 1);
    uint32_t placeholder = 0;
    append((uint8_t *)&placeholder, 4);
    return size() - 4;
}

// complete the array header
void Buffer::arr_end(size_t arr_header_idx, uint32_t num_els) {
    assert(this->at(arr_header_idx - 1) == TAG_ARR); 
    memcpy(data() + arr_header_idx, &num_els, 4);
}

void Buffer::response_begin(size_t &header_pos) {
    header_pos = size(); // save current position index
    uint32_t place_holder = 0;
    append((uint8_t *)&place_holder, 4);
}

void Buffer::response_end(size_t &header_pos) {
    size_t msg_len = size() - header_pos - 4;
    if (msg_len > k_max_msg) {
        // clear everything and just put ERR_OVERSIZED
        consume_back(msg_len); 
        out_err(ERR_OVERSIZED);
        // add fill in the complete message size
        uint32_t err_size = 2; // 1 for tag, 1 for code
        memcpy(data_begin + header_pos, &err_size, 4); 
        return;
    }
    memcpy(data_begin + header_pos, &msg_len, 4);
}

