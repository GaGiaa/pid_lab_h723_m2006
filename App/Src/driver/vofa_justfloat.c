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

uint32_t vofa_justfloat_encode_multi(
    const float *values,
    uint32_t channel_count,
    uint8_t *frame_buffer,
    uint32_t buffer_size)
{
  uint32_t frame_size;
  uint32_t channel_index;

  if ((values == NULL) || (frame_buffer == NULL))
  {
    return 0U;
  }

  if ((channel_count == 0U) || (channel_count > VOFA_JUSTFLOAT_MAX_CHANNELS))
  {
    return 0U;
  }

  frame_size = VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(channel_count);
  if (buffer_size < frame_size)
  {
    return 0U;
  }

  for (channel_index = 0U; channel_index < channel_count; ++channel_index)
  {
    memcpy(&frame_buffer[channel_index * 4U],
           &values[channel_index],
           sizeof(float));
  }

  frame_buffer[frame_size - 4U] = 0x00U;
  frame_buffer[frame_size - 3U] = 0x00U;
  frame_buffer[frame_size - 2U] = 0x80U;
  frame_buffer[frame_size - 1U] = 0x7FU;

  return frame_size;
}
