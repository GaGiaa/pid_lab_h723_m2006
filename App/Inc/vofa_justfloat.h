#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H

#include <stdint.h>

#define VOFA_JUSTFLOAT_FRAME_SIZE_BYTES (8U)

uint32_t vofa_justfloat_encode_float(
    float value,
    uint8_t frame_buffer[VOFA_JUSTFLOAT_FRAME_SIZE_BYTES]);

#endif /* VOFA_JUSTFLOAT_H */
