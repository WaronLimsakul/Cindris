#pragma once
#include <stdint.h>

#include "format.h"

// 	- `out_nil()` : doesn't need anything. Just append tag
// - `out_err(err, code)` : append Tag + error code
// - `out_str(char *, len)` : append Tag + String len + string
// - `out_int(int64_t)` : 
// - `out_double(double)`
// - `out_array()`

// The buff_end is NOT the last allocated byte, it's the byte after that.
// Same goes for data_end. The last byte of data is the one before it.
class Buffer {
public:
    Buffer();
    ~Buffer(); 
    uint8_t *data(); 
    void append(uint8_t src[], size_t len); 
    void consume(size_t len); 
    size_t size(); 
    uint8_t& at(size_t idx);
    uint8_t& operator[](size_t at); 

    void out_nil();
    void out_err(ErrorCode code);
    void out_str(char * data, size_t len);
    void out_int(int64_t data);
    void out_dbl(double data);

    // use this when you sure how many elements you want to send
    void out_array(uint32_t n);

    // use this when you not sure how many elements to sent
    size_t arr_begin();
    void arr_end(size_t arr_header_idx, uint32_t num_els);

    void response_begin(size_t &header_pos);
    void response_end(size_t &header_pos);

private:
    const size_t buff_min_len = 1024;
    uint8_t *buff_begin;
    uint8_t *buff_end;
    uint8_t *data_begin;
    uint8_t *data_end;

    void consume_back(size_t len);
};
