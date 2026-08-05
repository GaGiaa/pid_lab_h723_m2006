#include <stdint.h>
#include <stdio.h>

#include "vofa_justfloat.h"

static int expect_bytes(const uint8_t *actual, const uint8_t *expected, uint32_t length)
{
  uint32_t index;

  for (index = 0U; index < length; ++index)
  {
    if (actual[index] != expected[index])
    {
      return 0;
    }
  }

  return 1;
}

int main(void)
{
  static const uint8_t expected_timestamp[] = {
    0x00U, 0x00U, 0x7AU, 0x44U,
    0x00U, 0x00U, 0x80U, 0x7FU
  };
  static const uint8_t expected_zero[] = {
    0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x80U, 0x7FU
  };
  uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE];

  if (VofaJustFloatEncode1(1000.0F, frame) != VOFA_JUSTFLOAT_FRAME_SIZE)
  {
    return 1;
  }

  if (!expect_bytes(frame, expected_timestamp, VOFA_JUSTFLOAT_FRAME_SIZE))
  {
    return 2;
  }

  if (VofaJustFloatEncode1(0.0F, frame) != VOFA_JUSTFLOAT_FRAME_SIZE)
  {
    return 3;
  }

  if (!expect_bytes(frame, expected_zero, VOFA_JUSTFLOAT_FRAME_SIZE))
  {
    return 4;
  }

  if (VofaJustFloatEncode1(1.0F, (uint8_t *)0) != 0U)
  {
    return 5;
  }

  (void)printf("vofa_justfloat_test: PASS\n");
  return 0;
}
