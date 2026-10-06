//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Quadrature encoder initialization and interrupt handling.

#include "encoder.h"





static volatile int encoderA_count = 0;
static volatile int encoderB_count = 0;

void encoderInit(void) {
    // Enable encoder pins as inputs
    gpioEnable(GPIO_PORT_A);
    pinMode(ENCODERA_PIN, GPIO_INPUT);
    gpioEnable(GPIO_PORT_B);
    pinMode(ENCODERB_PIN, GPIO_INPUT);

    // Enable SYSCFG clock domain in RCC
    RCC->APB2ENR |= (1 << 0); // SYSCFGEN

    // Configure EXTICR for encoder pins

    // PA8 -> EXTI8
    SYSCFG->EXTICR[2] &= ~(0b111 << 0);

    // PB5 -> EXTI5
    SYSCFG->EXTICR[1] &= ~(0b111 << 4);
    SYSCFG->EXTICR[1] |=  (1 << 4);

    // Configure encoder interrupts for rising and falling edges
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERA_PIN));
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERB_PIN));

    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN));
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN));

    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN));
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN));

    // Enable EXTI5-9 interrupt in NVIC
    NVIC->ISER[0] |= (1 << 23);

    // Enable interrupts globally
    __enable_irq();
}

int encoderGetACount(void) {
    return encoderA_count;
}

int encoderGetBCount(void) {
    return encoderB_count;
}

// EXTI lines 5-9 share this handler
void EXTI9_5_IRQHandler(void) {

    // Check encoder A interrupt
    if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERA_PIN))) {
        // Clear interrupt pending bit
        EXTI->PR1 = (1 << gpioPinOffset(ENCODERA_PIN));

        encoderA_count++;
    }

    // Check encoder B interrupt
    if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERB_PIN))) {
        // Clear interrupt pending bit
        EXTI->PR1 = (1 << gpioPinOffset(ENCODERB_PIN));

        encoderB_count++;
    }
}