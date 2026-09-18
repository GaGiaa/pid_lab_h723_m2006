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

static int test_encode_float_single_channel(void)
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

  return 0;
}

static int test_encode_multi_three_channels(void)
{
  static const float values[] = { 1.0F, 2.0F, 1000.0F };
  static const uint8_t expected_frame[] = {
    0x00U, 0x00U, 0x80U, 0x3FU,
    0x00U, 0x00U, 0x00U, 0x40U,
    0x00U, 0x00U, 0x7AU, 0x44U,
    0x00U, 0x00U, 0x80U, 0x7FU
  };
  uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(3U)];

  if (vofa_justfloat_encode_multi(values,
                                  3U,
                                  encoded_frame,
                                  sizeof(encoded_frame)) !=
      sizeof(expected_frame))
  {
    return 1;
  }

  if (!expect_bytes_equal(encoded_frame, expected_frame, sizeof(expected_frame)))
  {
    return 2;
  }

  return 0;
}

static int test_encode_multi_max_channels(void)
{
  float values[VOFA_JUSTFLOAT_MAX_CHANNELS];
  uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(
      VOFA_JUSTFLOAT_MAX_CHANNELS)];
  uint32_t channel_index;
  uint32_t expected_size;

  for (channel_index = 0U;
       channel_index < VOFA_JUSTFLOAT_MAX_CHANNELS;
       ++channel_index)
  {
    values[channel_index] = (float)channel_index;
  }

  expected_size = VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(
      VOFA_JUSTFLOAT_MAX_CHANNELS);
  if (vofa_justfloat_encode_multi(values,
                                  VOFA_JUSTFLOAT_MAX_CHANNELS,
                                  encoded_frame,
                                  sizeof(encoded_frame)) != expected_size)
  {
    return 1;
  }

  /* 帧尾必须位于最后一字节：00 00 80 7F */
  if ((encoded_frame[expected_size - 4U] != 0x00U)
      || (encoded_frame[expected_size - 3U] != 0x00U)
      || (encoded_frame[expected_size - 2U] != 0x80U)
      || (encoded_frame[expected_size - 1U] != 0x7FU))
  {
    return 2;
  }

  return 0;
}

static int test_encode_multi_invalid_args(void)
{
  static const float values[] = { 1.0F, 2.0F };
  uint8_t encoded_frame[VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(2U)];
  uint8_t small_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_FOR_CHANNELS(2U) - 1U];

  /* NULL 输入 */
  if (vofa_justfloat_encode_multi((const float *)0, 2U, encoded_frame,
                                  sizeof(encoded_frame)) != 0U)
  {
    return 1;
  }

  if (vofa_justfloat_encode_multi(values, 2U, (uint8_t *)0,
                                  sizeof(encoded_frame)) != 0U)
  {
    return 2;
  }

  /* 通道数非法 */
  if (vofa_justfloat_encode_multi(values, 0U, encoded_frame,
                                  sizeof(encoded_frame)) != 0U)
  {
    return 3;
  }

  if (vofa_justfloat_encode_multi(values, VOFA_JUSTFLOAT_MAX_CHANNELS + 1U,
                                  encoded_frame,
                                  sizeof(encoded_frame)) != 0U)
  {
    return 4;
  }

  /* 缓冲区过小 */
  if (vofa_justfloat_encode_multi(values, 2U, small_buffer,
                                  sizeof(small_buffer)) != 0U)
  {
    return 5;
  }

  return 0;
}

int main(void)
{
  int result;

  result = test_encode_float_single_channel();
  if (result != 0)
  {
    return result;
  }

  result = test_encode_multi_three_channels();
  if (result != 0)
  {
    return 10 + result;
  }

  result = test_encode_multi_max_channels();
  if (result != 0)
  {
    return 20 + result;
  }

  result = test_encode_multi_invalid_args();
  if (result != 0)
  {
    return 30 + result;
  }

  (void)printf("vofa_justfloat_test: PASS\n");
  return 0;
}
