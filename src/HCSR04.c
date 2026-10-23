/*
* HCSR04.c - CPEG222 Project 6
* TRIG pulse, ECHO pin setup, and the TIM7 microsecond timer for the HC-SR04.
*/
#include "HCSR04.h"

#define TRIG_PORT      GPIOE
#define TRIG_PIN       3U   // TRIG is connected to PE3
#define TRIG_PULSE_US  11U  // 10 us minimum, plus 1 because the first timer tick can be partial
#define TIM7_PSC       89U  // TIM7 clock is 90 MHz (2 x 45 MHz APB1), so 90 MHz / 90 = 1 MHz

// Sets TRIG as an output (idle low), ECHO as an input, and starts TIM7 counting once per microsecond
void HCSR04_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOGEN;

    TRIG_PORT->MODER &= ~(0x3U << (TRIG_PIN * 2U));
    TRIG_PORT->MODER |=  (0x1U << (TRIG_PIN * 2U));
    TRIG_PORT->BSRR = (1U << (TRIG_PIN + 16U));

    HCSR04_ECHO_PORT->MODER &= ~(0x3U << (HCSR04_ECHO_PIN * 2U));

    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7->PSC = TIM7_PSC;
    TIM7->ARR = 0xFFFFU; // free running: counts 0 to 65535 then wraps
    TIM7->EGR = TIM_EGR_UG;
    TIM7->CR1 |= TIM_CR1_CEN;
}

// Reads the free running TIM7 counter
uint16_t HCSR04_Timestamp(void) {
    return (uint16_t)TIM7->CNT;
}

// Busy waits using TIM7; the 16 bit subtraction stays correct even if the counter wraps
void delay_us(uint16_t us) {
    uint16_t start = (uint16_t)TIM7->CNT;
    while ((uint16_t)((uint16_t)TIM7->CNT - start) < us) { }
}

// Drives TRIG high for about 10 us, which makes the sensor send its ultrasonic burst
void HCSR04_Trigger(void) {
    TRIG_PORT->BSRR = (1U << TRIG_PIN);
    delay_us(TRIG_PULSE_US);
    TRIG_PORT->BSRR = (1U << (TRIG_PIN + 16U));
}