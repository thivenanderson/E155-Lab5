#include "stm32l4xx.h"   // CMSIS device header (registers, IRQ numbers, NVIC functions)

static volatile int32_t encoder_count = 0;   // quarter-steps
static volatile uint8_t prev_state = 0;

// Index = (prev_state << 2) | new_state, where state = (A << 1) | B
static const int8_t qem_table[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

static inline uint8_t Encoder_ReadState(void)
{
    return (uint8_t)(GPIOA->IDR & 0x3);   // PA1 = bit 0 (B), PA0 = bit 1 (A)... see note below
}

void Encoder_Init(void)
{
    // 1. Clocks: GPIOA, and SYSCFG (needed to route pins to EXTI lines)
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;   // dummy read so the clock is active before first register access

    // 2. PA0, PA1 as inputs (MODER = 00). They reset to analog (11) on the L4, so clear explicitly.
    GPIOA->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);

    // 3. Pull-ups (PUPDR = 01)
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1);
    GPIOA->PUPDR |=  (GPIO_PUPDR_PUPD0_0 | GPIO_PUPDR_PUPD1_0);

    // 4. Route port A to EXTI0 and EXTI1 (EXTICR1 fields = 0000 selects port A)
    SYSCFG->EXTICR[0] &= ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI1);

    // 5. Trigger on both edges, unmask the lines
    EXTI->RTSR1 |= EXTI_RTSR1_RT0 | EXTI_RTSR1_RT1;
    EXTI->FTSR1 |= EXTI_FTSR1_FT0 | EXTI_FTSR1_FT1;
    EXTI->PR1    = EXTI_PR1_PIF0 | EXTI_PR1_PIF1;   // clear any stale pending flags
    EXTI->IMR1  |= EXTI_IMR1_IM0 | EXTI_IMR1_IM1;

    prev_state = Encoder_ReadState();

    // 6. NVIC (CMSIS core functions)
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_SetPriority(EXTI1_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

int32_t Encoder_GetCount(void)
{
    return encoder_count;   // aligned 32-bit read is atomic on Cortex-M4
}

static inline void Encoder_Update(void)
{
    uint8_t new_state = Encoder_ReadState();
    encoder_count += qem_table[(prev_state << 2) | new_state];
    prev_state = new_state;
}

void EXTI0_IRQHandler(void)
{
    EXTI->PR1 = EXTI_PR1_PIF0;   // write 1 to clear the pending flag
    Encoder_Update();
}

void EXTI1_IRQHandler(void)
{
    EXTI->PR1 = EXTI_PR1_PIF1;
    Encoder_Update();
}