#include <stdint.h>
#include <stdio.h>

#include "vofa_justfloat.h"

static int expect_bytes_equal(const uint8_t *actual_bytes,
                              const uint8_t *expected_bytes,
                              uint32_t byte_count)
{
  uint32_t byte_index;

  for (byte_index = 0U; byte_index < byte_count; ++byte_index)
  {
    if (actual_bytes[byte_index] != expected_bytes[byte_index])
    {
      return 0;
    }
  }

  return 1;
}

int main(void)
{
  static const uint8_t expected_timestamp_frame[] = {
    0x00U, 0x00U, 0x7AU, 0x44U,
    0x00U, 0x00U, 0x80U, 0x7FU
  };
  static const uint8_t expected_zero_frame[] = {
    0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x80U, 0x7FU
  };
  uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES];

  if (vofa_justfloat_encode_float(1000.0F, encoded_frame) !=
      VOFA_JUSTFLOAT_FRAME_SIZE_BYTES)
  {
    return 1;
  }

  if (!expect_bytes_equal(encoded_frame,
                          expected_timestamp_frame,
                          VOFA_JUSTFLOAT_FRAME_SIZE_BYTES))
  {
    return 2;
  }

  if (vofa_justfloat_encode_float(0.0F, encoded_frame) !=
      VOFA_JUSTFLOAT_FRAME_SIZE_BYTES)
  {
    return 3;
  }

  if (!expect_bytes_equal(encoded_frame,
                          expected_zero_frame,
                          VOFA_JUSTFLOAT_FRAME_SIZE_BYTES))
  {
    return 4;
  }

  if (vofa_justfloat_encode_float(1.0F, (uint8_t *)0) != 0U)
  {
    return 5;
  }

  (void)printf("vofa_justfloat_test: PASS\n");
  return 0;
}
