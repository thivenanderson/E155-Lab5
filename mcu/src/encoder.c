//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Quadrature encoder initialization and interrupt handling.

#include "encoder.h"

//CONSTANTS
#define VELOCITY_SAMPLE_MS 100

//GLOBAL VARIABLES
static volatile int32_t encoder_count = 0;
static volatile uint8_t previous_state = 0;


//HELPER FUNCTIONS

// Initialize encoder GPIO pins
static void encoderGPIOInit(void) {
    gpioEnable(GPIO_PORT_A);
    pinMode(ENCODERA_PIN, GPIO_INPUT);

    gpioEnable(GPIO_PORT_B);
    pinMode(ENCODERB_PIN, GPIO_INPUT);
}



// Read encoder A and B as a 2-bit state
static uint8_t encoderReadState(void) {
    uint8_t encoderA = digitalRead(ENCODERA_PIN);
    uint8_t encoderB = digitalRead(ENCODERB_PIN);

    return (encoderA << 1) | encoderB;
}

// Initialize encoder interrupts
static void encoderInterruptInit(void) {
    RCC->APB2ENR |= (1 << 0);

    // PA8 -> EXTI8
    SYSCFG->EXTICR[2] &= ~(0b111 << 0);

    // PB5 -> EXTI5
    SYSCFG->EXTICR[1] &= ~(0b111 << 4);
    SYSCFG->EXTICR[1] |= (1 << 4);

    // Enable EXTI8 and EXTI5
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERA_PIN));
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERB_PIN));

    // Setup to trigger on rising and falling edges
    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN));
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN));

    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN));
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN));

    // EXTI5-9 uses IRQ 23
    NVIC->ISER[0] |= (1 << 23);
}

// Initialize TIM6 as free-running timer for velocity calculation
static void encoderTimerInit(void) {
    // Enable TIM6 clock
    RCC->APB1ENR1 |= (1 << 4);

    // Set maximum count before rollover
    TIM6->ARR = 0xFFFF;

    // Configure timer for 1 ms per count
    initTIM(TIM6);
}



// Update count based on previous and current quadrature states
static void encoderUpdateCount(uint8_t current_state) {

    // One direction
    if ((previous_state == 0b00 && current_state == 0b10) ||
        (previous_state == 0b10 && current_state == 0b11) ||
        (previous_state == 0b11 && current_state == 0b01) ||
        (previous_state == 0b01 && current_state == 0b00)) {

        encoder_count++;
    }

    // Opposite direction
    else if ((previous_state == 0b00 && current_state == 0b01) ||
             (previous_state == 0b01 && current_state == 0b11) ||
             (previous_state == 0b11 && current_state == 0b10) ||
             (previous_state == 0b10 && current_state == 0b00)) {

        encoder_count--;
    }

    // Save state for next encoder edge
    previous_state = current_state;
}

// Convert encoder counts over a time interval to rev/s
static float encoderCalculateVelocity(int32_t delta_count, uint16_t elapsed_ms) {
    float revolutions = (float) delta_count / ENCODER_COUNTS_PER_REV;
    float elapsed_seconds = (float) elapsed_ms / 1000.0f;

    return revolutions / elapsed_seconds;
}

//PUBLIC FUNCTIONS

// Initialize encoder
void encoderInit(void) {
    encoderGPIOInit();

    previous_state = encoderReadState();

    encoderInterruptInit();

    encoderTimerInit();

    __enable_irq();
}

// Return signed encoder count
int32_t encoderGetCount(void) {
    return encoder_count;
}

// Calculate and return signed motor velocity in rev/s
float encoderGetVelocity(void) {
    static int32_t previous_count = 0;
    static uint16_t previous_time = 0;
    static float velocity = 0.0f;

    uint16_t current_time = TIM6->CNT;
    uint16_t elapsed_time = current_time - previous_time;

    if (elapsed_time >= VELOCITY_SAMPLE_MS) {
        int32_t current_count = encoder_count;
        int32_t delta_count = current_count - previous_count;

        velocity = encoderCalculateVelocity(delta_count, elapsed_time);

        previous_count = current_count;
        previous_time = current_time;
    }

    return velocity;
}

//INTERUPT HANDLER
// EXTI lines 5-9 share this handler
void EXTI9_5_IRQHandler(void) {

    // Check if either encoder channel triggered an interrupt
    if ((EXTI->PR1 & (1 << gpioPinOffset(ENCODERA_PIN))) ||
        (EXTI->PR1 & (1 << gpioPinOffset(ENCODERB_PIN)))) {

        // Clear encoder A pending interrupt if set
        if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERA_PIN))) {
            EXTI->PR1 = (1 << gpioPinOffset(ENCODERA_PIN));
        }

        // Clear encoder B pending interrupt if set
        if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERB_PIN))) {
            EXTI->PR1 = (1 << gpioPinOffset(ENCODERB_PIN));
        }

        uint8_t current_state = encoderReadState();

        encoderUpdateCount(current_state);
    }
}

