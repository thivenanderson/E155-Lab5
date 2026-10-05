#include "STM32L432KC_TIMER.h"
#include "STM32L432KC_RCC.h"

void initTIM(TIMx_TypeDef *TIMx, uint16_t prescaler) {
     // Enable selected timer clock
     if (TIMx == TIM6)
          RCC->APB1ENR1 |= (1 << 4);
     else if (TIMx == TIM7)
          RCC->APB1ENR1 |= (1 << 5);

     TIMx->PSC = prescaler; // Set timer prescaler
     TIMx->EGR |= (1 << 0); // Force update event to load prescaler value
     TIMx->SR &= ~(1 << 0); // Clear update flag caused by update event
}

void startTIM(TIMx_TypeDef *TIMx) {
     TIMx->CR1 |= (1 << 0); // Start timer
}

void stopTIM(TIMx_TypeDef *TIMx) {
     TIMx->CR1 &= ~(1 << 0); // Stop timer
}

void resetTIM(TIMx_TypeDef *TIMx) {
     TIMx->CNT = 0; // Reset timer count
}

void setTIMARR(TIMx_TypeDef *TIMx, uint16_t arr) {
     TIMx->ARR = arr; // Set timer auto-reload value
}

int timerDone(TIMx_TypeDef *TIMx) {
     return TIMx->SR & (1 << 0); // Return timer update flag
}

void clearTIMFlag(TIMx_TypeDef *TIMx) {
     TIMx->SR &= ~(1 << 0); // Clear timer update flag
}
