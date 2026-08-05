#ifndef __VOFA_JUSTFLOAT_H__
#define __VOFA_JUSTFLOAT_H__

#include <stdint.h>

#define VOFA_JUSTFLOAT_FRAME_SIZE (8U)

uint32_t VofaJustFloatEncode1(float value, uint8_t frame[VOFA_JUSTFLOAT_FRAME_SIZE]);

#endif /* __VOFA_JUSTFLOAT_H__ */
