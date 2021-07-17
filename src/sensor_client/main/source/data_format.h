#ifndef _DATA_FORMAT_H_
#define _DATA_FORMAT_H_

#include <stdint.h>

/**
 * @brief Return uint16_t representation of a char*.
 * char* has to be 4-sized
 */
uint16_t string_to_hex_uint16_t(const char *string);

/**
 * @brief Return char[] representation of uint8_t[].
 * len is val[] size.
 */
char* uint8_array_to_string(uint8_t *val, uint16_t len);

#endif