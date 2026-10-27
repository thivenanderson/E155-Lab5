//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Interface for quadrature encoder interrupts.

#ifndef ENCODER_H
#define ENCODER_H

#include "STM32L432KC.h"
#include <stm32l432xx.h>
#include <stdint.h>

#define ENCODERA_PIN PA8
#define ENCODERB_PIN PB5

// Motor encoder PPR from lab specs.
// Change to 120 for a -10 motor.
#define ENCODER_PPR 408.0f

// Using both edges of both channels gives 4 counts per encoder pulse.
#define ENCODER_COUNTS_PER_REV (4.0f * ENCODER_PPR)

void encoderInit(void);
int32_t encoderGetCount(void);
float encoderGetVelocity(void);
uint32_t encoderGetInterruptCount(void);

#endif // ENCODER_H