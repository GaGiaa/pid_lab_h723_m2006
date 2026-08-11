#include "vofa_justfloat.h"

#include <stddef.h>
#include <string.h>

uint32_t vofa_justfloat_encode_float(
    float value,
    uint8_t frame_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES])
{
  if (frame_buffer == NULL)
  {
    return 0U;
  }

  memcpy(frame_buffer, &value, sizeof(value));
  frame_buffer[4] = 0x00U;
  frame_buffer[5] = 0x00U;
  frame_buffer[6] = 0x80U;
  frame_buffer[7] = 0x7FU;

  return VOFA_JUSTFLOAT_FRAME_SIZE_BYTES;
}
