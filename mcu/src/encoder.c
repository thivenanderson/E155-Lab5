#include "encoder.h"
#include "stm32l4xx.h"

/* ---------------- Configuration ---------------- */
/* 408 = 12 pulses x 34:1 gearbox. The 25GA-370 datasheet you sent lists 120 for
 * the 1:10 version. Turn the output shaft exactly one revolution by hand:
 * the count should change by ENCODER_PPR x 4. */
#define ENCODER_PPR                 408u                    /* pulses per output rev, per channel */
#define ENCODER_COUNTS_PER_REV      (ENCODER_PPR * 4u)      /* x4 decoding */

#define ENCODER_USE_PULLUP          0       /* 1 = enable internal pull-ups on A and B */
#define ENCODER_DIRECTION_SIGN      (+1)    /* set to -1 to reverse count direction */
#define ENCODER_VELOCITY_WINDOW     8u      /* samples in the ring buffer (power of 2) */
#define ENCODER_TIMEOUT_MS          100u    /* no edge for this long => velocity = 0 */
#define ENCODER_IRQ_PRIORITY        1u

/* TIM2 free-running 32-bit timestamp timer */
#define ENCODER_TIMER               TIM2
#define ENCODER_TIMER_TICK_HZ       1000000u                /* 1 MHz tick = 1 us */
/* TIM2 input clock. Equals SystemCoreClock when the APB1 prescaler is 1.
 * Override this macro if you divide APB1 (timer clock is then 2x PCLK1). */
#define ENCODER_TIMER_CLOCK_HZ      (SystemCoreClock)

/* Pins: A = PB5 (EXTI5, Nucleo D11), B = PA8 (EXTI8, Nucleo D9) */
#define ENC_A_PORT                  GPIOB
#define ENC_A_PIN                   5u
#define ENC_B_PORT                  GPIOA
#define ENC_B_PIN                   8u

/* ---------------- State ---------------- */
typedef struct {
    int32_t  count;
    uint32_t timestamp;     /* TIM2 ticks */
} EncoderSample;

static volatile int32_t        s_count = 0;
static volatile uint8_t        s_prevState = 0;
static volatile uint32_t       s_head = 0;
static volatile EncoderSample  s_samples[ENCODER_VELOCITY_WINDOW];

/* Index = (prev << 2) | new, state = (A << 1) | B.
 * +1 for 00 -> 01 -> 11 -> 10 -> 00, -1 for the reverse, 0 for no change or an invalid jump. */
static const int8_t s_transition[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

/* ---------------- Helpers ---------------- */
static inline uint8_t readState(void)
{
    uint32_t a = (ENC_A_PORT->IDR >> ENC_A_PIN) & 0x1u;
    uint32_t b = (ENC_B_PORT->IDR >> ENC_B_PIN) & 0x1u;
    return (uint8_t)((a << 1) | b);             /* (A << 1) | B */
}

static inline uint32_t timestampNow(void)
{
    return ENCODER_TIMER->CNT;
}

static inline uint32_t enterCritical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static inline void exitCritical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static void initTimer(void)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    (void)RCC->APB1ENR1;                                /* let the clock settle */

    ENCODER_TIMER->CR1  = 0;                            /* stopped, up-counting */
    ENCODER_TIMER->PSC  = (ENCODER_TIMER_CLOCK_HZ / ENCODER_TIMER_TICK_HZ) - 1u;
    ENCODER_TIMER->ARR  = 0xFFFFFFFFu;                  /* full 32-bit range */
    ENCODER_TIMER->CNT  = 0;
    ENCODER_TIMER->EGR  = TIM_EGR_UG;                   /* load PSC into the shadow register */
    ENCODER_TIMER->SR   = 0;                            /* clear the update flag the UG set */
    ENCODER_TIMER->CR1 |= TIM_CR1_CEN;                  /* start free-running */
}

static void initGpio(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    /* PB5 and PA8 to input mode (00); PB5 resets to analog */
    ENC_A_PORT->MODER &= ~GPIO_MODER_MODE5;
    ENC_B_PORT->MODER &= ~GPIO_MODER_MODE8;

    ENC_A_PORT->PUPDR &= ~GPIO_PUPDR_PUPD5;
    ENC_B_PORT->PUPDR &= ~GPIO_PUPDR_PUPD8;
#if ENCODER_USE_PULLUP
    ENC_A_PORT->PUPDR |= GPIO_PUPDR_PUPD5_0;
    ENC_B_PORT->PUPDR |= GPIO_PUPDR_PUPD8_0;
#endif
}

static void initExti(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    /* EXTI5 -> port B (EXTICR2), EXTI8 -> port A (EXTICR3) */
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI5;
    SYSCFG->EXTICR[1] |=  SYSCFG_EXTICR2_EXTI5_PB;
    SYSCFG->EXTICR[2] &= ~SYSCFG_EXTICR3_EXTI8;         /* 0000 = port A */

    /* Both edges on both channels */
    EXTI->RTSR1 |= EXTI_RTSR1_RT5 | EXTI_RTSR1_RT8;
    EXTI->FTSR1 |= EXTI_FTSR1_FT5 | EXTI_FTSR1_FT8;

    /* Clear stale flags, then unmask */
    EXTI->PR1  = EXTI_PR1_PIF5 | EXTI_PR1_PIF8;
    EXTI->IMR1 |= EXTI_IMR1_IM5 | EXTI_IMR1_IM8;

    NVIC_SetPriority(EXTI9_5_IRQn, ENCODER_IRQ_PRIORITY);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* Called from the ISR. Kept short: timestamp, read pins, table lookup, store one sample. */
static inline void processEdge(void)
{
    uint32_t now      = timestampNow();
    uint8_t  newState = readState();
    int8_t   step     = s_transition[(s_prevState << 2) | newState];

    s_prevState = newState;
    if (step == 0) {
        return;                                 /* no movement or skipped state */
    }

    s_count += step * ENCODER_DIRECTION_SIGN;

    uint32_t head = (s_head + 1u) & (ENCODER_VELOCITY_WINDOW - 1u);
    s_samples[head].count     = s_count;
    s_samples[head].timestamp = now;
    s_head = head;
}

/* ---------------- Public API ---------------- */
void encoderInit(void)
{
    initTimer();
    initGpio();

    s_prevState = readState();
    s_count = 0;

    /* Fill the window with the current position so velocity starts at zero */
    uint32_t now = timestampNow();
    for (uint32_t i = 0; i < ENCODER_VELOCITY_WINDOW; i++) {
        s_samples[i].count     = 0;
        s_samples[i].timestamp = now;
    }
    s_head = 0;

    initExti();                                 /* enable interrupts last */
}

int32_t encoderGetCount(void)
{
    return s_count;                             /* aligned 32-bit read is atomic on Cortex-M4 */
}

float encoderGetVelocity(void)
{
    int32_t  newCount, oldCount;
    uint32_t newTime, oldTime, now;

    uint32_t primask = enterCritical();
    uint32_t head   = s_head;
    uint32_t oldest = (head + 1u) & (ENCODER_VELOCITY_WINDOW - 1u);
    newCount = s_samples[head].count;
    newTime  = s_samples[head].timestamp;
    oldCount = s_samples[oldest].count;
    oldTime  = s_samples[oldest].timestamp;
    now      = timestampNow();
    exitCritical(primask);

    /* Stopped: nothing has moved recently */
    const uint32_t timeoutTicks = (ENCODER_TIMER_TICK_HZ / 1000u) * ENCODER_TIMEOUT_MS;
    if ((uint32_t)(now - newTime) > timeoutTicks) {
        return 0.0f;
    }

    uint32_t dt = newTime - oldTime;            /* unsigned math handles timer wrap */
    int32_t  dc = newCount - oldCount;
    if (dt == 0u || dc == 0) {
        return 0.0f;
    }

    /* revs/sec = (dc / counts_per_rev) / (dt / tick_hz) */
    return ((float)dc * (float)ENCODER_TIMER_TICK_HZ) /
           ((float)ENCODER_COUNTS_PER_REV * (float)dt);
}

/* ---------------- Interrupt handler ---------------- */
/* EXTI lines 5 to 9 share this handler. */
void EXTI9_5_IRQHandler(void)
{
    uint32_t pending = EXTI->PR1 & (EXTI_PR1_PIF5 | EXTI_PR1_PIF8);
    EXTI->PR1 = pending;                        /* write 1 to clear only our flags */
    if (pending) {
        processEdge();
    }
}