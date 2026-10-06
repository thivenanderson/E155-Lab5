//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Main program for lab 5 of E155 that enables the MCU to
// to determine the speed of a motor by reading from a quadrature encoder using interrupts.

#include "main.h"
volatile int encoderA_count = 0;
volatile int encoderB_count = 0;

int main(void) {
    //Enable encoders pins as inputs
    gpioEnable(GPIO_PORT_A);
    pinMode(ENCODERA_PIN, GPIO_INPUT);
    gpioEnable(GPIO_PORT_B);
    pinMode(ENCODERB_PIN, GPIO_INPUT);

    //Enable SYSCFG clock domain in RCC
    RCC->APB2ENR |= (1 << 0); // SYSCFGEN
    
    //Congigure EXITCR for encoder pins
    //PA8
    SYSCFG->EXTICR[2] &= ~(0b111 << 0); 
    //PB5
    SYSCFG->EXTICR[1] &= ~(0b111 << 4);
    SYSCFG->EXTICR[1] |= (1 << 4);

    //enable interrups
    __enable_irq();

    //Configure encoder interrupts for rising and falling edge
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERA_PIN)); 
    EXTI->IMR1 |= (1 << gpioPinOffset(ENCODERB_PIN));   
    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN)); 
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERA_PIN));  
    EXTI->RTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN)); 
    EXTI->FTSR1 |= (1 << gpioPinOffset(ENCODERB_PIN));
    NVIC->ISER[0] |= (1 << 23);                       // Turn on EXTI interrupt in NVIC_ISER (EXTI9_5 is IRQ 23)

    while (1) {}
  
}

// EXTI lines 5-9 share this handler
void EXTI9_5_IRQHandler(void){

    // Check that the encoder was what triggered our interrupt
    if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERA_PIN))){
        // If so, clear the interrupt (NB: Write 1 to reset.)
        EXTI->PR1 = (1 << gpioPinOffset(ENCODERA_PIN));

        // Then increase count
        encoderA_count++;

    }
    if (EXTI->PR1 & (1 << gpioPinOffset(ENCODERB_PIN))){
        // If so, clear the interrupt (NB: Write 1 to reset.)
        EXTI->PR1 = (1 << gpioPinOffset(ENCODERB_PIN));

        // Then toggle the LED
        encoderB_count++;

    }
  }