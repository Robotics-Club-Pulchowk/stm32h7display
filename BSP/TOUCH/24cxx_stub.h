#ifndef __24CXX_STUB_H
#define __24CXX_STUB_H
#include <stdint.h>
static inline void at24cxx_init(void) {}
static inline void at24cxx_write(uint16_t addr, uint8_t *buf, uint16_t len) {}
static inline void at24cxx_write_one_byte(uint16_t addr, uint8_t data) {}
static inline void at24cxx_read(uint16_t addr, uint8_t *buf, uint16_t len) {}
static inline uint8_t at24cxx_read_one_byte(uint16_t addr) { return 0; }
#endif
