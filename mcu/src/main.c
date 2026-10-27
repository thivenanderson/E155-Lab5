//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Main program for lab 5 of E155 that enables the MCU to
// determine the speed of a motor by reading from a quadrature encoder using interrupts.

#include "main.h"
#include "encoder.h"

volatile int32_t encoder_count = 0;
volatile float motor_velocity = 0.0f;

int main(void) {

    encoderInit();

    // Enable TIM7 for display timing
    RCC->APB1ENR1 |= (1 << 5);
    initTIM(TIM7);

    while (1) {
        encoder_count = encoderGetCount();
        motor_velocity = encoderGetVelocity();

        delay_millis(TIM7, 100);
    }
}