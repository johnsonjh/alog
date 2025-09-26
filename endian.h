#ifndef _ENDIAN_H
# define _ENDIAN_H

# include <stdint.h>

uint32_t swap_uint32(uint32_t val);
int is_little_endian();

#endif
