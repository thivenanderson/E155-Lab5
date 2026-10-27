//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Main program for lab 5 of E155 that enables the MCU to
// determine the speed of a motor by reading from a quadrature encoder using interrupts.

#include "main.h"
#include "encoder.h"
#include <stdio.h>

volatile int32_t encoder_count = 0;
volatile float motor_velocity = 0.0f;

// Function used by printf to send characters to SEGGER
int _write(int file, char *ptr, int len) {
    int i;

    for (i = 0; i < len; i++) {
        ITM_SendChar(*ptr++);
    }

    return len;
}

int main(void) {

    encoderInit();

    // Enable TIM7 for display timing
    RCC->APB1ENR1 |= (1 << 5);
    initTIM(TIM7);

    while (1) {
        int32_t encoder_count = encoderGetCount();
        float motor_velocity = encoderGetVelocity();

        if (motor_velocity > 0.0f) {
            printf("Count: %ld, Direction: Clockwise, Velocity: %.3f rev/s\n",
                   encoder_count, motor_velocity);
        }
        else if (motor_velocity < 0.0f) {
            printf("Count: %ld, Direction: Counterclockwise, Velocity: %.3f rev/s\n",
                   encoder_count, -motor_velocity);
        }
        else {
            printf("Count: %ld, Direction: Stopped, Velocity: 0.000 rev/s\n",
                   encoder_count);
        }

        delay_millis(TIM7, 250);
    }
}
