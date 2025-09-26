#include "endian.h"

uint32_t
swap_uint32(uint32_t val)
{
  val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
  return (val << 16) | (val >> 16);
}

int
is_little_endian()
{
  uint16_t i = 1;
  return *(char *)&i;
}
