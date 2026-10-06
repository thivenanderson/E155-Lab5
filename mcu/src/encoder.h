//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Interface for quadrature encoder interrupts.

#ifndef ENCODER_H
#define ENCODER_H

#include "STM32L432KC.h"
#include <stm32l432xx.h>

void encoderInit(void);
int encoderGetACount(void);
int encoderGetBCount(void);

#endif // ENCODER_H