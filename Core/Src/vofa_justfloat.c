#include "vofa_justfloat.h"

#include <stddef.h>
#include <string.h>

uint32_t VofaJustFloatEncode1(float value, uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE])
{
  if (frame == NULL)
  {
    return 0U;
  }

  memcpy(frame, &value, sizeof(value));
  frame[4] = 0x00U;
  frame[5] = 0x00U;
  frame[6] = 0x80U;
  frame[7] = 0x7FU;

  return VOFA_JUSTFLOAT_FRAME_SIZE;
}
