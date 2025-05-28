#pragma once
#include <stdint.h>

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
    uint8_t operator[](int at); 
private:
    const size_t buff_min_len = 1024;
    uint8_t *buff_begin;
    uint8_t *buff_end;
    uint8_t *data_begin;
    uint8_t *data_end;
};
