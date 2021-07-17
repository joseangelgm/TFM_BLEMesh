#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "source/data_format.h"

static char hex[] = "0123456789ABCDEF";

/**
 * @brief return uint8_t representation of a char.
 */
static uint8_t char_to_uint8_t(const char c)
{
    // A = 65 -> A = 1010 -> 10 in decimal. Thats why substract 55
    if(c == 'A' || (c > 'A' && c < 'F') || c == 'F')
    {
        return (uint8_t) c - 55;
    }
    // a = 97 -> a = 1010 -> 10 in decimal. Thats why substract 87
    else if(c == 'a' || (c > 'a' && c < 'f') || c == 'f')
    {
        return (uint8_t) c - 87;
    }
    // 0 = 65 -> A = 1010 -> 10 in decimal. Thats why substract 55
    else if(c == '0' || (c > '0' && c < '9') || c == '9')
    {
        return (uint8_t) c - 48;
    }
    return 0;
}

/**
 * @brief Return uint16_t representation of a char*.
 * char* has to be 4-sized
 */
uint16_t string_to_hex_uint16_t(const char *string)
{
    uint16_t cast = 0x0000;
    if (strlen(string) == 4)
    {
        cast = (uint16_t) ( (char_to_uint8_t(string[0]) << 12)
                          | (char_to_uint8_t(string[1]) << 8)
                          | (char_to_uint8_t(string[2]) << 4)
                          |  char_to_uint8_t(string[3]));
    }
    return cast;
}

/**
 * @brief Get char representation from a uint8_t.
 * uint8_t -> char[0] (ms), char[1] (ls)
 */
static void uint8_to_char(uint8_t value, char* ms, char* ls)
{

    uint8_t ms_value = value >> 4;

    *ms = hex[(int)ms_value];
    *ls = hex[(int)(value & 0xF)];
}

/**
 * @brief Return char[] representation of uint8_t[].
 * len is val[] size.
 */
char* uint8_array_to_string(uint8_t *val, uint16_t len)
{
    size_t size = ((sizeof(char) * len * 2) + 1);
    char* buff = (char *)malloc(size); // '\0'
    memset(buff, '\0', size);

    for (size_t i = 0; i < len ; i++)
	{
        uint8_to_char(val[i], &buff[i * 2], &buff[(i * 2) + 1]);
	}

    return buff;
}