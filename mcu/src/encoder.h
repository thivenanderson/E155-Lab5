#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configure PB5 (A) / PA8 (B), EXTI5/EXTI8, and TIM2 as the timestamp timer. */
void    encoderInit(void);

/* Signed position in quadrature counts (ENCODER_PPR x 4 per output-shaft revolution). */
int32_t encoderGetCount(void);

/* Signed velocity in output-shaft revolutions per second.
 * Returns 0.0f if the shaft has been stationary longer than the timeout. */
float   encoderGetVelocity(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */